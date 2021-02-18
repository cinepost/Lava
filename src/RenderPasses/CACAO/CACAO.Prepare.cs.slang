// =============================================================================
// Prepare

Texture2D<float>          g_DepthIn    : register(t0);

groupshared uint s_PrepareMem[10][18];


min16float ScreenSpaceToViewSpaceDepth(min16float screenDepth)
{
    min16float depthLinearizeMul = min16float(g_CACAOConsts.DepthUnpackConsts.x);
    min16float depthLinearizeAdd = min16float(g_CACAOConsts.DepthUnpackConsts.y);

    // Optimised version of "-cameraClipNear / (cameraClipFar - projDepth * (cameraClipFar - cameraClipNear)) * cameraClipFar"

    // Set your depthLinearizeMul and depthLinearizeAdd to:
    // depthLinearizeMul = ( cameraClipFar * cameraClipNear) / ( cameraClipFar - cameraClipNear );
    // depthLinearizeAdd = cameraClipFar / ( cameraClipFar - cameraClipNear );

    return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

min16float4 ScreenSpaceToViewSpaceDepth4x(min16float4 screenDepths)
{
    min16float depthLinearizeMul = min16float(g_CACAOConsts.DepthUnpackConsts.x);
    min16float depthLinearizeAdd = min16float(g_CACAOConsts.DepthUnpackConsts.y);

    // Optimised version of "-cameraClipNear / (cameraClipFar - projDepth * (cameraClipFar - cameraClipNear)) * cameraClipFar"

    // Set your depthLinearizeMul and depthLinearizeAdd to:
    // depthLinearizeMul = ( cameraClipFar * cameraClipNear) / ( cameraClipFar - cameraClipNear );
    // depthLinearizeAdd = cameraClipFar / ( cameraClipFar - cameraClipNear );

    return depthLinearizeMul / (depthLinearizeAdd - screenDepths);
}

RWTexture2DArray<float> g_PrepareDepthsAndMips_OutMip0 : register(u0);
RWTexture2DArray<float> g_PrepareDepthsAndMips_OutMip1 : register(u1);
RWTexture2DArray<float> g_PrepareDepthsAndMips_OutMip2 : register(u2);
RWTexture2DArray<float> g_PrepareDepthsAndMips_OutMip3 : register(u3);

groupshared float s_PrepareDepthsAndMipsBuffer[4][8][8];

float MipSmartAverage(float4 depths)
{
    float closest = min(min(depths.x, depths.y), min(depths.z, depths.w));
    float falloffCalcMulSq = -1.0f / g_CACAOConsts.EffectRadius * g_CACAOConsts.EffectRadius;
    float4 dists = depths - closest.xxxx;
    float4 weights = saturate(dists * dists * falloffCalcMulSq + 1.0);
    return dot(weights, depths) / dot(weights, float4(1.0, 1.0, 1.0, 1.0));
}

min16float MipSmartAverage_16(min16float4 depths)
{
    min16float closest = min(min(depths.x, depths.y), min(depths.z, depths.w));
    min16float falloffCalcMulSq = min16float(-1.0f / g_CACAOConsts.EffectRadius * g_CACAOConsts.EffectRadius);
    min16float4 dists = depths - closest.xxxx;
    min16float4 weights = saturate(dists * dists * falloffCalcMulSq + 1.0);
    return dot(weights, depths) / dot(weights, min16float4(1.0, 1.0, 1.0, 1.0));
}

void PrepareDepthsAndMips(float4 samples, uint2 outputCoord, uint2 gtid)
{
    samples = ScreenSpaceToViewSpaceDepth(samples);

    s_PrepareDepthsAndMipsBuffer[0][gtid.x][gtid.y] = samples.w;
    s_PrepareDepthsAndMipsBuffer[1][gtid.x][gtid.y] = samples.z;
    s_PrepareDepthsAndMipsBuffer[2][gtid.x][gtid.y] = samples.x;
    s_PrepareDepthsAndMipsBuffer[3][gtid.x][gtid.y] = samples.y;

    g_PrepareDepthsAndMips_OutMip0[int3(outputCoord.x, outputCoord.y, 0)] = samples.w;
    g_PrepareDepthsAndMips_OutMip0[int3(outputCoord.x, outputCoord.y, 1)] = samples.z;
    g_PrepareDepthsAndMips_OutMip0[int3(outputCoord.x, outputCoord.y, 2)] = samples.x;
    g_PrepareDepthsAndMips_OutMip0[int3(outputCoord.x, outputCoord.y, 3)] = samples.y;

    uint depthArrayIndex = 2 * (gtid.y % 2) + (gtid.x % 2);
    uint2 depthArrayOffset = int2(gtid.x % 2, gtid.y % 2);
    int2 bufferCoord = int2(gtid) - int2(depthArrayOffset);

    outputCoord /= 2;
    GroupMemoryBarrierWithGroupSync();

    // if (stillAlive) <-- all threads alive here
    {
        float sample_00 = s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 0][bufferCoord.y + 0];
        float sample_01 = s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 0][bufferCoord.y + 1];
        float sample_10 = s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 1][bufferCoord.y + 0];
        float sample_11 = s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 1][bufferCoord.y + 1];

        float avg = MipSmartAverage(float4(sample_00, sample_01, sample_10, sample_11));
        g_PrepareDepthsAndMips_OutMip1[int3(outputCoord.x, outputCoord.y, depthArrayIndex)] = avg;
        s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x][bufferCoord.y] = avg;
    }

    bool stillAlive = gtid.x % 4 == depthArrayOffset.x && gtid.y % 4 == depthArrayOffset.y;

    outputCoord /= 2;
    GroupMemoryBarrierWithGroupSync();

    if (stillAlive)
    {
        float sample_00 = s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 0][bufferCoord.y + 0];
        float sample_01 = s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 0][bufferCoord.y + 2];
        float sample_10 = s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 2][bufferCoord.y + 0];
        float sample_11 = s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 2][bufferCoord.y + 2];

        float avg = MipSmartAverage(float4(sample_00, sample_01, sample_10, sample_11));
        g_PrepareDepthsAndMips_OutMip2[int3(outputCoord.x, outputCoord.y, depthArrayIndex)] = avg;
        s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x][bufferCoord.y] = avg;
    }

    stillAlive = gtid.x % 8 == depthArrayOffset.x && depthArrayOffset.y % 8 == depthArrayOffset.y;

    outputCoord /= 2;
    GroupMemoryBarrierWithGroupSync();

    if (stillAlive)
    {
        float sample_00 = s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 0][bufferCoord.y + 0];
        float sample_01 = s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 0][bufferCoord.y + 4];
        float sample_10 = s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 4][bufferCoord.y + 0];
        float sample_11 = s_PrepareDepthsAndMipsBuffer[depthArrayIndex][bufferCoord.x + 4][bufferCoord.y + 4];

        float avg = MipSmartAverage(float4(sample_00, sample_01, sample_10, sample_11));
        g_PrepareDepthsAndMips_OutMip3[int3(outputCoord.x, outputCoord.y, depthArrayIndex)] = avg;
    }
}

[numthreads(PREPARE_DEPTHS_AND_MIPS_WIDTH, PREPARE_DEPTHS_AND_MIPS_HEIGHT, 1)]
void CSPrepareDownsampledDepthsAndMips(uint2 tid : SV_DispatchThreadID, uint2 gtid : SV_GroupThreadID)
{
    int2 depthBufferCoord = 4 * tid.xy;
    int2 outputCoord = tid;

    float2 uv = (float2(depthBufferCoord)+0.5f) * g_CACAOConsts.DepthBufferInverseDimensions;
    float4 samples;
#if 1
    samples.x = g_DepthIn.SampleLevel(g_PointClampSampler, uv, 0, int2(0, 2));
    samples.y = g_DepthIn.SampleLevel(g_PointClampSampler, uv, 0, int2(2, 2));
    samples.z = g_DepthIn.SampleLevel(g_PointClampSampler, uv, 0, int2(2, 0));
    samples.w = g_DepthIn.SampleLevel(g_PointClampSampler, uv, 0, int2(0, 0));
#else
    samples.x = g_DepthIn[depthBufferCoord + uint2(0, 2)];
    samples.y = g_DepthIn[depthBufferCoord + uint2(2, 2)];
    samples.z = g_DepthIn[depthBufferCoord + uint2(2, 0)];
    samples.w = g_DepthIn[depthBufferCoord + uint2(0, 0)];
#endif

    PrepareDepthsAndMips(samples, outputCoord, gtid);
}

[numthreads(PREPARE_DEPTHS_AND_MIPS_WIDTH, PREPARE_DEPTHS_AND_MIPS_HEIGHT, 1)]
void CSPrepareNativeDepthsAndMips(uint2 tid : SV_DispatchThreadID, uint2 gtid : SV_GroupThreadID)
{
    int2 depthBufferCoord = 2 * tid.xy;
    int2 outputCoord = tid;

    float2 uv = (float2(depthBufferCoord)+0.5f) * g_CACAOConsts.DepthBufferInverseDimensions;
    float4 samples = g_DepthIn.GatherRed(g_PointClampSampler, uv);

    PrepareDepthsAndMips(samples, outputCoord, gtid);
}

RWTexture2DArray<float> g_PrepareDepthsOut : register(u0);

void PrepareDepths(float4 samples, uint2 tid)
{
    samples = ScreenSpaceToViewSpaceDepth(samples);
    g_PrepareDepthsOut[int3(tid.x, tid.y, 0)] = samples.w;
    g_PrepareDepthsOut[int3(tid.x, tid.y, 1)] = samples.z;
    g_PrepareDepthsOut[int3(tid.x, tid.y, 2)] = samples.x;
    g_PrepareDepthsOut[int3(tid.x, tid.y, 3)] = samples.y;
}

[numthreads(PREPARE_DEPTHS_WIDTH, PREPARE_DEPTHS_HEIGHT, 1)]
void CSPrepareDownsampledDepths(uint2 tid : SV_DispatchThreadID)
{
    int2 depthBufferCoord = 4 * tid.xy;

    float2 uv = (float2(depthBufferCoord)+0.5f) * g_CACAOConsts.DepthBufferInverseDimensions;
    float4 samples;
    samples.x = g_DepthIn.SampleLevel(g_PointClampSampler, uv, 0, int2(0, 2));
    samples.y = g_DepthIn.SampleLevel(g_PointClampSampler, uv, 0, int2(2, 2));
    samples.z = g_DepthIn.SampleLevel(g_PointClampSampler, uv, 0, int2(2, 0));
    samples.w = g_DepthIn.SampleLevel(g_PointClampSampler, uv, 0, int2(0, 0));
    
    PrepareDepths(samples, tid);
}

[numthreads(PREPARE_DEPTHS_WIDTH, PREPARE_DEPTHS_HEIGHT, 1)]
void CSPrepareNativeDepths(uint2 tid : SV_DispatchThreadID)
{
    int2 depthBufferCoord = 2 * tid.xy;

    float2 uv = (float2(depthBufferCoord)+0.5f) * g_CACAOConsts.DepthBufferInverseDimensions;
    float4 samples = g_DepthIn.GatherRed(g_PointClampSampler, uv);

    PrepareDepths(samples, tid);
}

[numthreads(PREPARE_DEPTHS_HALF_WIDTH, PREPARE_DEPTHS_HALF_HEIGHT, 1)]
void CSPrepareDownsampledDepthsHalf(uint2 tid : SV_DispatchThreadID)
{
    float sample_00 = g_DepthIn.Load(int3(4 * tid.x + 0, 4 * tid.y + 0, 0));
    float sample_11 = g_DepthIn.Load(int3(4 * tid.x + 2, 4 * tid.y + 2, 0));
    sample_00 = ScreenSpaceToViewSpaceDepth(sample_00);
    sample_11 = ScreenSpaceToViewSpaceDepth(sample_11);
    g_PrepareDepthsOut[int3(tid.x, tid.y, 0)] = sample_00;
    g_PrepareDepthsOut[int3(tid.x, tid.y, 3)] = sample_11;
}

[numthreads(PREPARE_DEPTHS_HALF_WIDTH, PREPARE_DEPTHS_HALF_HEIGHT, 1)]
void CSPrepareNativeDepthsHalf(uint2 tid : SV_DispatchThreadID)
{
    float sample_00 = g_DepthIn.Load(int3(2 * tid.x + 0, 2 * tid.y + 0, 0));
    float sample_11 = g_DepthIn.Load(int3(2 * tid.x + 1, 2 * tid.y + 1, 0));
    sample_00 = ScreenSpaceToViewSpaceDepth(sample_00);
    sample_11 = ScreenSpaceToViewSpaceDepth(sample_11);
    g_PrepareDepthsOut[int3(tid.x, tid.y, 0)] = sample_00;
    g_PrepareDepthsOut[int3(tid.x, tid.y, 3)] = sample_11;
}

groupshared float s_PrepareDepthsNormalsAndMipsBuffer[18][18];

RWTexture2DArray<float4> g_PrepareNormals_NormalOut : register(u0);

struct PrepareNormalsInputDepths
{
    float depth_10;
    float depth_20;

    float depth_01;
    float depth_11;
    float depth_21;
    float depth_31;

    float depth_02;
    float depth_12;
    float depth_22;
    float depth_32;

    float depth_13;
    float depth_23;
};

void PrepareNormals(PrepareNormalsInputDepths depths, float2 uv, float2 pixelSize, int2 normalCoord)
{
    float3 p_10 = NDCToViewspace(uv + float2(+0.0f, -1.0f) * pixelSize, depths.depth_10);
    float3 p_20 = NDCToViewspace(uv + float2(+1.0f, -1.0f) * pixelSize, depths.depth_20);

    float3 p_01 = NDCToViewspace(uv + float2(-1.0f, +0.0f) * pixelSize, depths.depth_01);
    float3 p_11 = NDCToViewspace(uv + float2(+0.0f, +0.0f) * pixelSize, depths.depth_11);
    float3 p_21 = NDCToViewspace(uv + float2(+1.0f, +0.0f) * pixelSize, depths.depth_21);
    float3 p_31 = NDCToViewspace(uv + float2(+2.0f, +0.0f) * pixelSize, depths.depth_31);

    float3 p_02 = NDCToViewspace(uv + float2(-1.0f, +1.0f) * pixelSize, depths.depth_02);
    float3 p_12 = NDCToViewspace(uv + float2(+0.0f, +1.0f) * pixelSize, depths.depth_12);
    float3 p_22 = NDCToViewspace(uv + float2(+1.0f, +1.0f) * pixelSize, depths.depth_22);
    float3 p_32 = NDCToViewspace(uv + float2(+2.0f, +1.0f) * pixelSize, depths.depth_32);

    float3 p_13 = NDCToViewspace(uv + float2(+0.0f, +2.0f) * pixelSize, depths.depth_13);
    float3 p_23 = NDCToViewspace(uv + float2(+1.0f, +2.0f) * pixelSize, depths.depth_23);

    float4 edges_11 = CalculateEdges(p_11.z, p_01.z, p_21.z, p_10.z, p_12.z);
    float4 edges_21 = CalculateEdges(p_21.z, p_11.z, p_31.z, p_20.z, p_22.z);
    float4 edges_12 = CalculateEdges(p_12.z, p_02.z, p_22.z, p_11.z, p_13.z);
    float4 edges_22 = CalculateEdges(p_22.z, p_12.z, p_32.z, p_21.z, p_23.z);

    float3 norm_11 = CalculateNormal(edges_11, p_11, p_01, p_21, p_10, p_12);
    float3 norm_21 = CalculateNormal(edges_21, p_21, p_11, p_31, p_20, p_22);
    float3 norm_12 = CalculateNormal(edges_12, p_12, p_02, p_22, p_11, p_13);
    float3 norm_22 = CalculateNormal(edges_22, p_22, p_12, p_32, p_21, p_23);

    g_PrepareNormals_NormalOut[int3(normalCoord, 0)] = float4(norm_11, 1.0f);
    g_PrepareNormals_NormalOut[int3(normalCoord, 1)] = float4(norm_21, 1.0f);
    g_PrepareNormals_NormalOut[int3(normalCoord, 2)] = float4(norm_12, 1.0f);
    g_PrepareNormals_NormalOut[int3(normalCoord, 3)] = float4(norm_22, 1.0f);
}

[numthreads(PREPARE_NORMALS_WIDTH, PREPARE_NORMALS_HEIGHT, 1)]
void CSPrepareDownsampledNormals(int2 tid : SV_DispatchThreadID)
{
    int2 depthCoord = 4 * tid + g_CACAOConsts.DepthBufferOffset;

    PrepareNormalsInputDepths depths;

    depths.depth_10 = ScreenSpaceToViewSpaceDepth(g_DepthIn.Load(int3(depthCoord, 0), int2(+0, -2)));
    depths.depth_20 = ScreenSpaceToViewSpaceDepth(g_DepthIn.Load(int3(depthCoord, 0), int2(+2, -2)));

    depths.depth_01 = ScreenSpaceToViewSpaceDepth(g_DepthIn.Load(int3(depthCoord, 0), int2(-2, +0)));
    depths.depth_11 = ScreenSpaceToViewSpaceDepth(g_DepthIn.Load(int3(depthCoord, 0), int2(+0, +0)));
    depths.depth_21 = ScreenSpaceToViewSpaceDepth(g_DepthIn.Load(int3(depthCoord, 0), int2(+2, +0)));
    depths.depth_31 = ScreenSpaceToViewSpaceDepth(g_DepthIn.Load(int3(depthCoord, 0), int2(+4, +0)));

    depths.depth_02 = ScreenSpaceToViewSpaceDepth(g_DepthIn.Load(int3(depthCoord, 0), int2(-2, +2)));
    depths.depth_12 = ScreenSpaceToViewSpaceDepth(g_DepthIn.Load(int3(depthCoord, 0), int2(+0, +2)));
    depths.depth_22 = ScreenSpaceToViewSpaceDepth(g_DepthIn.Load(int3(depthCoord, 0), int2(+2, +2)));
    depths.depth_32 = ScreenSpaceToViewSpaceDepth(g_DepthIn.Load(int3(depthCoord, 0), int2(+4, +2)));

    depths.depth_13 = ScreenSpaceToViewSpaceDepth(g_DepthIn.Load(int3(depthCoord, 0), int2(+0, +4)));
    depths.depth_23 = ScreenSpaceToViewSpaceDepth(g_DepthIn.Load(int3(depthCoord, 0), int2(+2, +4)));

    float2 pixelSize = 2.0f * g_CACAOConsts.OutputBufferInverseDimensions; // 2.0f * g_CACAOConsts.DepthBufferInverseDimensions;
    float2 uv = (float2(4 * tid) + 0.5f) * g_CACAOConsts.OutputBufferInverseDimensions; // * g_CACAOConsts.SSAOBufferInverseDimensions;

    PrepareNormals(depths, uv, pixelSize, tid);
}

[numthreads(PREPARE_NORMALS_WIDTH, PREPARE_NORMALS_HEIGHT, 1)]
void CSPrepareNativeNormals(int2 tid : SV_DispatchThreadID)
{
    int2 depthCoord = 2 * tid + g_CACAOConsts.DepthBufferOffset;
    float2 depthBufferUV = (float2(depthCoord)-0.5f) * g_CACAOConsts.DepthBufferInverseDimensions;
    float4 samples_00 = g_DepthIn.GatherRed(g_PointClampSampler, depthBufferUV, int2(0, 0));
    float4 samples_10 = g_DepthIn.GatherRed(g_PointClampSampler, depthBufferUV, int2(2, 0));
    float4 samples_01 = g_DepthIn.GatherRed(g_PointClampSampler, depthBufferUV, int2(0, 2));
    float4 samples_11 = g_DepthIn.GatherRed(g_PointClampSampler, depthBufferUV, int2(2, 2));

    PrepareNormalsInputDepths depths;

    depths.depth_10 = ScreenSpaceToViewSpaceDepth(samples_00.z);
    depths.depth_20 = ScreenSpaceToViewSpaceDepth(samples_10.w);

    depths.depth_01 = ScreenSpaceToViewSpaceDepth(samples_00.x);
    depths.depth_11 = ScreenSpaceToViewSpaceDepth(samples_00.y);
    depths.depth_21 = ScreenSpaceToViewSpaceDepth(samples_10.x);
    depths.depth_31 = ScreenSpaceToViewSpaceDepth(samples_10.y);

    depths.depth_02 = ScreenSpaceToViewSpaceDepth(samples_01.w);
    depths.depth_12 = ScreenSpaceToViewSpaceDepth(samples_01.z);
    depths.depth_22 = ScreenSpaceToViewSpaceDepth(samples_11.w);
    depths.depth_32 = ScreenSpaceToViewSpaceDepth(samples_11.z);

    depths.depth_13 = ScreenSpaceToViewSpaceDepth(samples_01.y);
    depths.depth_23 = ScreenSpaceToViewSpaceDepth(samples_11.x);

    // use unused samples to make sure compiler doesn't overlap memory and put a sync
    // between loads
    float epsilon = (samples_00.w + samples_10.z + samples_01.x + samples_11.y) * 1e-20f;

    float2 pixelSize = g_CACAOConsts.OutputBufferInverseDimensions;
    float2 uv = (float2(2 * tid) + 0.5f + epsilon) * g_CACAOConsts.OutputBufferInverseDimensions;

    PrepareNormals(depths, uv, pixelSize, tid);
}

Texture2D<float4>        g_PrepareNormalsFromNormalsInput  : register(t0);
RWTexture2DArray<float4> g_PrepareNormalsFromNormalsOutput : register(u0);

float3 PrepareNormalsFromInputNormalsLoadNormal(int2 pos)
{
    float3 encodedNormal = g_PrepareNormalsFromNormalsInput.SampleLevel(g_PointClampSampler, (float2(pos)+0.5f) * g_CACAOConsts.OutputBufferInverseDimensions, 0).xyz;
    return DecodeNormal(encodedNormal);
}

[numthreads(PREPARE_NORMALS_FROM_INPUT_NORMALS_WIDTH, PREPARE_NORMALS_FROM_INPUT_NORMALS_HEIGHT, 1)]
void CSPrepareDownsampledNormalsFromInputNormals(int2 tid : SV_DispatchThreadID)
{
    int2 baseCoord = 4 * tid;
    g_PrepareNormalsFromNormalsOutput[uint3(tid, 0)] = float4(PrepareNormalsFromInputNormalsLoadNormal(baseCoord + int2(0, 0)), 1.0f);
    g_PrepareNormalsFromNormalsOutput[uint3(tid, 1)] = float4(PrepareNormalsFromInputNormalsLoadNormal(baseCoord + int2(2, 0)), 1.0f);
    g_PrepareNormalsFromNormalsOutput[uint3(tid, 2)] = float4(PrepareNormalsFromInputNormalsLoadNormal(baseCoord + int2(0, 2)), 1.0f);
    g_PrepareNormalsFromNormalsOutput[uint3(tid, 3)] = float4(PrepareNormalsFromInputNormalsLoadNormal(baseCoord + int2(2, 2)), 1.0f);
}

[numthreads(PREPARE_NORMALS_FROM_INPUT_NORMALS_WIDTH, PREPARE_NORMALS_FROM_INPUT_NORMALS_HEIGHT, 1)]
void CSPrepareNativeNormalsFromInputNormals(int2 tid : SV_DispatchThreadID)
{
    int2 baseCoord = 2 * tid;
    g_PrepareNormalsFromNormalsOutput[uint3(tid, 0)] = float4(PrepareNormalsFromInputNormalsLoadNormal(baseCoord + int2(0, 0)), 1.0f);
    g_PrepareNormalsFromNormalsOutput[uint3(tid, 1)] = float4(PrepareNormalsFromInputNormalsLoadNormal(baseCoord + int2(1, 0)), 1.0f);
    g_PrepareNormalsFromNormalsOutput[uint3(tid, 2)] = float4(PrepareNormalsFromInputNormalsLoadNormal(baseCoord + int2(0, 1)), 1.0f);
    g_PrepareNormalsFromNormalsOutput[uint3(tid, 3)] = float4(PrepareNormalsFromInputNormalsLoadNormal(baseCoord + int2(1, 1)), 1.0f);
}