Texture2DArray<float2>    g_BlurInput  : register(t0);
RWTexture2DArray<float2>  g_BlurOutput : register(u0);

void AddSample(float ssaoValue, float edgeValue, inout float sum, inout float sumWeight)
{
    float weight = edgeValue;

    sum += (weight * ssaoValue);
    sumWeight += weight;
}

float2 SampleBlurredWide(float2 inPos, float2 coord)
{
    float3 fullCoord = float3(coord, 0.0f);
    float2 vC = g_BlurInput.SampleLevel(g_PointMirrorSampler, fullCoord, 0.0, int2(0, 0)).xy;
    float2 vL = g_BlurInput.SampleLevel(g_PointMirrorSampler, fullCoord, 0.0, int2(-2, 0)).xy;
    float2 vT = g_BlurInput.SampleLevel(g_PointMirrorSampler, fullCoord, 0.0, int2(0, -2)).xy;
    float2 vR = g_BlurInput.SampleLevel(g_PointMirrorSampler, fullCoord, 0.0, int2(2, 0)).xy;
    float2 vB = g_BlurInput.SampleLevel(g_PointMirrorSampler, fullCoord, 0.0, int2(0, 2)).xy;

    float packedEdges = vC.y;
    float4 edgesLRTB = UnpackEdges(packedEdges);
    edgesLRTB.x *= UnpackEdges(vL.y).y;
    edgesLRTB.z *= UnpackEdges(vT.y).w;
    edgesLRTB.y *= UnpackEdges(vR.y).x;
    edgesLRTB.w *= UnpackEdges(vB.y).z;

    float ssaoValue = vC.x;
    float ssaoValueL = vL.x;
    float ssaoValueT = vT.x;
    float ssaoValueR = vR.x;
    float ssaoValueB = vB.x;

    float sumWeight = 0.8f;
    float sum = ssaoValue * sumWeight;

    AddSample(ssaoValueL, edgesLRTB.x, sum, sumWeight);
    AddSample(ssaoValueR, edgesLRTB.y, sum, sumWeight);
    AddSample(ssaoValueT, edgesLRTB.z, sum, sumWeight);
    AddSample(ssaoValueB, edgesLRTB.w, sum, sumWeight);

    float ssaoAvg = sum / sumWeight;

    ssaoValue = ssaoAvg; //min( ssaoValue, ssaoAvg ) * 0.2 + ssaoAvg * 0.8;

    return float2(ssaoValue, packedEdges);
}

uint PackFloat16(min16float2 v)
{
    uint2 p = f32tof16(float2(v));
    return p.x | (p.y << 16);
}

min16float2 UnpackFloat16(uint a)
{
    float2 tmp = f16tof32(uint2(a & 0xFFFF, a >> 16));
    return min16float2(tmp);
}

// all in one, SIMD in yo SIMD dawg, shader
#define TILE_WIDTH  4
#define TILE_HEIGHT 3
#define HALF_TILE_WIDTH (TILE_WIDTH / 2)
#define QUARTER_TILE_WIDTH (TILE_WIDTH / 4)

#define ARRAY_WIDTH  (HALF_TILE_WIDTH  * BLUR_WIDTH  + 2)
#define ARRAY_HEIGHT (TILE_HEIGHT * BLUR_HEIGHT + 2)

#define ITERS 4

groupshared uint s_BlurF16Front_4[ARRAY_WIDTH][ARRAY_HEIGHT];
groupshared uint s_BlurF16Back_4[ARRAY_WIDTH][ARRAY_HEIGHT];

struct Edges_4
{
    min16float4 left;
    min16float4 right;
    min16float4 top;
    min16float4 bottom;
};

Edges_4 UnpackEdgesFloat16_4(min16float4 _packedVal)
{
    uint4 packedVal = (uint4)(_packedVal * 255.5);
    Edges_4 result;
    result.left   = min16float4(saturate(min16float4((packedVal >> 6) & 0x03) / 3.0 + g_CACAOConsts.InvSharpness));
    result.right  = min16float4(saturate(min16float4((packedVal >> 4) & 0x03) / 3.0 + g_CACAOConsts.InvSharpness));
    result.top    = min16float4(saturate(min16float4((packedVal >> 2) & 0x03) / 3.0 + g_CACAOConsts.InvSharpness));
    result.bottom = min16float4(saturate(min16float4((packedVal >> 0) & 0x03) / 3.0 + g_CACAOConsts.InvSharpness));

    return result;
}

min16float4 CalcBlurredSampleF16_4(min16float4 packedEdges, min16float4 centre, min16float4 left, min16float4 right, min16float4 top, min16float4 bottom)
{
    min16float4 sum = centre * min16float(0.5f);
    min16float4 weight = min16float4(0.5f, 0.5f, 0.5f, 0.5f);
    Edges_4 edges = UnpackEdgesFloat16_4(packedEdges);

    sum += left * edges.left;
    weight += edges.left;
    sum += right * edges.right;
    weight += edges.right;
    sum += top * edges.top;
    weight += edges.top;
    sum += bottom * edges.bottom;
    weight += edges.bottom;

    return sum / weight;
}

void LDSEdgeSensitiveBlur(const uint blurPasses, const uint2 tid, const uint2 gid)
{
    int2 imageCoord = gid * (int2(TILE_WIDTH * BLUR_WIDTH, TILE_HEIGHT * BLUR_HEIGHT) - (2*blurPasses)) + int2(TILE_WIDTH, TILE_HEIGHT) * tid - blurPasses;
    int2 bufferCoord = int2(HALF_TILE_WIDTH, TILE_HEIGHT) * tid + 1;

    // todo -- replace this with gathers.
    min16float4 packedEdges[QUARTER_TILE_WIDTH][TILE_HEIGHT];
    {
        float2 input[TILE_WIDTH][TILE_HEIGHT];
        int y;
        [unroll]
        for (y = 0; y < TILE_HEIGHT; ++y)
        {
            [unroll]
            for (int x = 0; x < TILE_WIDTH; ++x)
            {
                input[x][y] = g_BlurInput.SampleLevel(g_PointMirrorSampler, float3((imageCoord + int2(x, y) + 0.5f) * g_CACAOConsts.SSAOBufferInverseDimensions, 0.0f), 0).xy;
            }
        }
        [unroll]
        for (y = 0; y < TILE_HEIGHT; ++y)
        {
            [unroll]
            for (int x = 0; x < QUARTER_TILE_WIDTH; ++x)
            {
                min16float2 ssaoVals = min16float2(input[4 * x + 0][y].x, input[4 * x + 1][y].x);
                s_BlurF16Front_4[bufferCoord.x + 2*x + 0][bufferCoord.y + y] = PackFloat16(ssaoVals);
                ssaoVals = min16float2(input[4 * x + 2][y].x, input[4 * x + 3][y].x);
                s_BlurF16Front_4[bufferCoord.x + 2*x + 1][bufferCoord.y + y] = PackFloat16(ssaoVals);
                // min16float2 ssaoVals = min16float2(1, 1);
                packedEdges[x][y] = min16float4(input[4 * x + 0][y].y, input[4 * x + 1][y].y, input[4 * x + 2][y].y, input[4 * x + 3][y].y);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint i = 0; i < (blurPasses + 1) / 2; ++i)
    {
        [unroll]
        for (int y = 0; y < TILE_HEIGHT; ++y)
        {
            [unroll]
            for (int x = 0; x < QUARTER_TILE_WIDTH; ++x)
            {
                int2 c = bufferCoord + int2(2*x, y);
                min16float4 centre = min16float4(UnpackFloat16(s_BlurF16Front_4[c.x + 0][c.y + 0]), UnpackFloat16(s_BlurF16Front_4[c.x + 1][c.y + 0]));
                min16float4 top    = min16float4(UnpackFloat16(s_BlurF16Front_4[c.x + 0][c.y - 1]), UnpackFloat16(s_BlurF16Front_4[c.x + 1][c.y - 1]));
                min16float4 bottom = min16float4(UnpackFloat16(s_BlurF16Front_4[c.x + 0][c.y + 1]), UnpackFloat16(s_BlurF16Front_4[c.x + 1][c.y + 1]));

                min16float2 tmp = UnpackFloat16(s_BlurF16Front_4[c.x - 1][c.y + 0]);
                min16float4 left = min16float4(tmp.y, centre.xyz);
                tmp = UnpackFloat16(s_BlurF16Front_4[c.x + 2][c.y + 0]);
                min16float4 right = min16float4(centre.yzw, tmp.x);

                min16float4 tmp_4 = CalcBlurredSampleF16_4(packedEdges[x][y], centre, left, right, top, bottom);
                s_BlurF16Back_4[c.x + 0][c.y] = PackFloat16(tmp_4.xy);
                s_BlurF16Back_4[c.x + 1][c.y] = PackFloat16(tmp_4.zw);
            }
        }
        GroupMemoryBarrierWithGroupSync();

        if (2 * i + 1 < blurPasses)
        {
            [unroll]
            for (int y = 0; y < TILE_HEIGHT; ++y)
            {
                [unroll]
                for (int x = 0; x < QUARTER_TILE_WIDTH; ++x)
                {
                    int2 c = bufferCoord + int2(2 * x, y);
                    min16float4 centre = min16float4(UnpackFloat16(s_BlurF16Back_4[c.x + 0][c.y + 0]), UnpackFloat16(s_BlurF16Back_4[c.x + 1][c.y + 0]));
                    min16float4 top    = min16float4(UnpackFloat16(s_BlurF16Back_4[c.x + 0][c.y - 1]), UnpackFloat16(s_BlurF16Back_4[c.x + 1][c.y - 1]));
                    min16float4 bottom = min16float4(UnpackFloat16(s_BlurF16Back_4[c.x + 0][c.y + 1]), UnpackFloat16(s_BlurF16Back_4[c.x + 1][c.y + 1]));

                    min16float2 tmp = UnpackFloat16(s_BlurF16Back_4[c.x - 1][c.y + 0]);
                    min16float4 left = min16float4(tmp.y, centre.xyz);
                    tmp = UnpackFloat16(s_BlurF16Back_4[c.x + 2][c.y + 0]);
                    min16float4 right = min16float4(centre.yzw, tmp.x);

                    min16float4 tmp_4 = CalcBlurredSampleF16_4(packedEdges[x][y], centre, left, right, top, bottom);
                    s_BlurF16Front_4[c.x + 0][c.y] = PackFloat16(tmp_4.xy);
                    s_BlurF16Front_4[c.x + 1][c.y] = PackFloat16(tmp_4.zw);
                }
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }

    [unroll]
    for (uint y = 0; y < TILE_HEIGHT; ++y)
    {
        uint outputY = TILE_HEIGHT * tid.y + y;
        if (blurPasses <= outputY && outputY < TILE_HEIGHT * BLUR_HEIGHT - blurPasses)
        {
            [unroll]
            for (int x = 0; x < QUARTER_TILE_WIDTH; ++x)
            {
                uint outputX = TILE_WIDTH * tid.x + 4 * x;

                min16float4 ssaoVal;
                if (blurPasses % 2 == 0)
                {
                    ssaoVal = min16float4(UnpackFloat16(s_BlurF16Front_4[bufferCoord.x + x][bufferCoord.y + y]), UnpackFloat16(s_BlurF16Front_4[bufferCoord.x + x + 1][bufferCoord.y + y]));
                }
                else
                {
                    ssaoVal = min16float4(UnpackFloat16(s_BlurF16Back_4[bufferCoord.x + x][bufferCoord.y + y]), UnpackFloat16(s_BlurF16Back_4[bufferCoord.x + x + 1][bufferCoord.y + y]));
                }

                if (blurPasses <= outputX && outputX < TILE_WIDTH * BLUR_WIDTH - blurPasses)
                {
                    g_BlurOutput[int3(imageCoord + int2(4 * x, y), 0)] = float2(ssaoVal.x, packedEdges[x][y].x);
                }
                outputX += 1;
                if (blurPasses <= outputX && outputX < TILE_WIDTH * BLUR_WIDTH - blurPasses)
                {
                    g_BlurOutput[int3(imageCoord + int2(4 * x + 1, y), 0)] = float2(ssaoVal.y, packedEdges[x][y].y);
                }
                outputX += 1;
                if (blurPasses <= outputX && outputX < TILE_WIDTH * BLUR_WIDTH - blurPasses)
                {
                    g_BlurOutput[int3(imageCoord + int2(4 * x + 2, y), 0)] = float2(ssaoVal.z, packedEdges[x][y].z);
                }
                outputX += 1;
                if (blurPasses <= outputX && outputX < TILE_WIDTH * BLUR_WIDTH - blurPasses)
                {
                    g_BlurOutput[int3(imageCoord + int2(4 * x + 3, y), 0)] = float2(ssaoVal.w, packedEdges[x][y].w);
                }
            }
        }
    }
}

[numthreads(BLUR_WIDTH, BLUR_HEIGHT, 1)]
void CSEdgeSensitiveBlur1(uint2 tid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    LDSEdgeSensitiveBlur(1, tid, gid);
}

[numthreads(BLUR_WIDTH, BLUR_HEIGHT, 1)]
void CSEdgeSensitiveBlur2(uint2 tid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    LDSEdgeSensitiveBlur(2, tid, gid);
}

[numthreads(BLUR_WIDTH, BLUR_HEIGHT, 1)]
void CSEdgeSensitiveBlur3(uint2 tid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    LDSEdgeSensitiveBlur(3, tid, gid);
}

[numthreads(BLUR_WIDTH, BLUR_HEIGHT, 1)]
void CSEdgeSensitiveBlur4(uint2 tid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    LDSEdgeSensitiveBlur(4, tid, gid);
}

[numthreads(BLUR_WIDTH, BLUR_HEIGHT, 1)]
void CSEdgeSensitiveBlur5(uint2 tid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    LDSEdgeSensitiveBlur(5, tid, gid);
}

[numthreads(BLUR_WIDTH, BLUR_HEIGHT, 1)]
void CSEdgeSensitiveBlur6(uint2 tid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    LDSEdgeSensitiveBlur(6, tid, gid);
}

[numthreads(BLUR_WIDTH, BLUR_HEIGHT, 1)]
void CSEdgeSensitiveBlur7(uint2 tid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    LDSEdgeSensitiveBlur(7, tid, gid);
}


[numthreads(BLUR_WIDTH, BLUR_HEIGHT, 1)]
void CSEdgeSensitiveBlur8(uint2 tid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    LDSEdgeSensitiveBlur(8, tid, gid);
}