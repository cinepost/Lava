// ============================================================================================
// bilateral upscale

RWTexture2D<float>     g_BilateralUpscaleOutput            : register(u0);

Texture2DArray<float2> g_BilateralUpscaleInput             : register(t0);

Texture2D<float>       g_BilateralUpscaleDepth             : register(t1);
Texture2DArray<float>  g_BilateralUpscaleDownscaledDepth   : register(t3);


uint DoublePackFloat16(float v)
{
    uint2 p = f32tof16(float2(v, v));
    return p.x | (p.y << 16);
}

#define BILATERAL_UPSCALE_BUFFER_WIDTH  (BILATERAL_UPSCALE_WIDTH  + 4)
#define BILATERAL_UPSCALE_BUFFER_HEIGHT (BILATERAL_UPSCALE_HEIGHT + 4 + 4)

struct BilateralBufferVal
{
    // float depth;
    // float ssaoVal;
    uint packedDepths;
    uint packedSsaoVals;
};

groupshared BilateralBufferVal s_BilateralUpscaleBuffer[BILATERAL_UPSCALE_BUFFER_WIDTH][BILATERAL_UPSCALE_BUFFER_HEIGHT];

void BilateralUpscaleNxN(int2 tid, uint2 gtid, uint2 gid, const int width, const int height)
{
    // fill in group shared buffer
    {
        uint threadNum = (gtid.y * BILATERAL_UPSCALE_WIDTH + gtid.x) * 3;
        uint2 bufferCoord = uint2(threadNum % BILATERAL_UPSCALE_BUFFER_WIDTH, threadNum / BILATERAL_UPSCALE_BUFFER_WIDTH);
        uint2 imageCoord = (gid * uint2(BILATERAL_UPSCALE_WIDTH, BILATERAL_UPSCALE_HEIGHT)) + bufferCoord - 2;

        for (int i = 0; i < 3; ++i)
        {
            // uint2 depthBufferCoord = imageCoord + 2 * g_CACAOConsts.DeinterleavedDepthBufferOffset;
            // uint3 depthArrayBufferCoord = uint3(depthBufferCoord / 2, 2 * (depthBufferCoord.y % 2) + depthBufferCoord.x % 2);
            uint3 ssaoArrayBufferCoord = uint3(imageCoord / 2, 2 * (imageCoord.y % 2) + imageCoord.x % 2);
            uint3 depthArrayBufferCoord = ssaoArrayBufferCoord + uint3(g_CACAOConsts.DeinterleavedDepthBufferOffset, 0);
            ++imageCoord.x;

            BilateralBufferVal bufferVal;

            float depth = g_BilateralUpscaleDownscaledDepth[depthArrayBufferCoord];
            float ssaoVal = g_BilateralUpscaleInput.SampleLevel(g_PointClampSampler, float3((float2(ssaoArrayBufferCoord.xy) + 0.5f) * g_CACAOConsts.SSAOBufferInverseDimensions, ssaoArrayBufferCoord.z), 0).x;

            bufferVal.packedDepths = DoublePackFloat16(depth);
            bufferVal.packedSsaoVals = DoublePackFloat16(ssaoVal);

            s_BilateralUpscaleBuffer[bufferCoord.x + i][bufferCoord.y] = bufferVal;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    float depths[4];
    // load depths
    {
        int2 fullBufferCoord = 2 * tid;
        int2 fullDepthBufferCoord = fullBufferCoord + g_CACAOConsts.DepthBufferOffset;

        depths[0] = ScreenSpaceToViewSpaceDepth(g_BilateralUpscaleDepth[fullDepthBufferCoord + int2(0, 0)]);
        depths[1] = ScreenSpaceToViewSpaceDepth(g_BilateralUpscaleDepth[fullDepthBufferCoord + int2(1, 0)]);
        depths[2] = ScreenSpaceToViewSpaceDepth(g_BilateralUpscaleDepth[fullDepthBufferCoord + int2(0, 1)]);
        depths[3] = ScreenSpaceToViewSpaceDepth(g_BilateralUpscaleDepth[fullDepthBufferCoord + int2(1, 1)]);
    }
    min16float4 packedDepths = min16float4(depths[0], depths[1], depths[2], depths[3]);

    float totals[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float totalWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float2 pps[] = { float2(0.0f, 0.0f), float2(0.5f, 0.0f), float2(0.0f, 0.5f), float2(0.5f, 0.5f) };

    min16float4 packedTotals = min16float4(0.0f, 0.0f, 0.0f, 0.0f);
    min16float4 packedTotalWeights = min16float4(0.0f, 0.0f, 0.0f, 0.0f);

    int2 baseBufferCoord = gtid + int2(width, height);

    float distanceSigma = g_CACAOConsts.BilateralSimilarityDistanceSigma;
    min16float2 packedDistSigma = min16float2(1.0f / distanceSigma, 1.0f / distanceSigma);
    float sigma = g_CACAOConsts.BilateralSigmaSquared;
    min16float2 packedSigma = min16float2(1.0f / sigma, 1.0f / sigma);

    for (int x = -width; x <= width; ++x)
    {
        for (int y = -height; y <= height; ++y)
        {
            int2 bufferCoord = baseBufferCoord + int2(x, y);

            BilateralBufferVal bufferVal = s_BilateralUpscaleBuffer[bufferCoord.x][bufferCoord.y];

            min16float2 u = min16float2(x, x) + min16float2(0.0f, 0.5f);
            min16float2 v1 = min16float2(y, y) + min16float2(0.0f, 0.0f);
            min16float2 v2 = min16float2(y, y) + min16float2(0.5f, 0.5f);
            u = u * u;
            v1 = v1 * v1;
            v2 = v2 * v2;

            min16float2 dist1 = u + v1;
            min16float2 dist2 = u + v2;

            min16float2 wx1 = exp(-dist1 * packedSigma);
            min16float2 wx2 = exp(-dist2 * packedSigma);

            min16float2 bufferPackedDepths = UnpackFloat16(bufferVal.packedDepths);

#if 0
            min16float2 diff1 = abs(packedDepths.xy - bufferPackedDepths);
            min16float2 diff2 = abs(packedDepths.zw - bufferPackedDepths);
#else
            min16float2 diff1 = packedDepths.xy - bufferPackedDepths;
            min16float2 diff2 = packedDepths.zw - bufferPackedDepths;
            diff1 *= diff1;
            diff2 *= diff2;
#endif

            min16float2 wy1 = exp(-diff1 * packedDistSigma);
            min16float2 wy2 = exp(-diff2 * packedDistSigma);

            min16float2 weight1 = wx1 * wy1;
            min16float2 weight2 = wx2 * wy2;

            min16float2 packedSsaoVals = UnpackFloat16(bufferVal.packedSsaoVals);
            packedTotals.xy += packedSsaoVals * weight1;
            packedTotals.zw += packedSsaoVals * weight2;
            packedTotalWeights.xy += weight1;
            packedTotalWeights.zw += weight2;
        }
    }

    uint2 outputCoord = 2 * tid;
    min16float4 outputValues = packedTotals / packedTotalWeights;
    g_BilateralUpscaleOutput[outputCoord + int2(0, 0)] = outputValues.x; // totals[0] / totalWeights[0];
    g_BilateralUpscaleOutput[outputCoord + int2(1, 0)] = outputValues.y; // totals[1] / totalWeights[1];
    g_BilateralUpscaleOutput[outputCoord + int2(0, 1)] = outputValues.z; // totals[2] / totalWeights[2];
    g_BilateralUpscaleOutput[outputCoord + int2(1, 1)] = outputValues.w; // totals[3] / totalWeights[3];
}

[numthreads(BILATERAL_UPSCALE_WIDTH, BILATERAL_UPSCALE_HEIGHT, 1)]
void CSUpscaleBilateral5x5(int2 tid : SV_DispatchThreadID, uint2 gtid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    BilateralUpscaleNxN(tid, gtid, gid, 2, 2);
}

[numthreads(BILATERAL_UPSCALE_WIDTH, BILATERAL_UPSCALE_HEIGHT, 1)]
void CSUpscaleBilateral7x7(int2 tid : SV_DispatchThreadID, uint2 gtid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    BilateralUpscaleNxN(tid, gtid, gid, 3, 3);
}

[numthreads(BILATERAL_UPSCALE_WIDTH, BILATERAL_UPSCALE_HEIGHT, 1)]
void CSUpscaleBilateral5x5Half(int2 tid : SV_DispatchThreadID, uint2 gtid : SV_GroupThreadID, uint2 gid : SV_GroupID)
{
    const int width = 2, height = 2;

    // fill in group shared buffer
    {
        uint threadNum = (gtid.y * BILATERAL_UPSCALE_WIDTH + gtid.x) * 3;
        uint2 bufferCoord = uint2(threadNum % BILATERAL_UPSCALE_BUFFER_WIDTH, threadNum / BILATERAL_UPSCALE_BUFFER_WIDTH);
        uint2 imageCoord = (gid * uint2(BILATERAL_UPSCALE_WIDTH, BILATERAL_UPSCALE_HEIGHT)) + bufferCoord - 2;

        for (int i = 0; i < 3; ++i)
        {
            // uint2 depthBufferCoord = imageCoord + g_CACAOConsts.DeinterleavedDepthBufferOffset;
            // uint3 depthArrayBufferCoord = uint3(depthBufferCoord / 2, 2 * (depthBufferCoord.y % 2) + depthBufferCoord.x % 2);
            uint idx = (imageCoord.y % 2) * 3;
            uint3 ssaoArrayBufferCoord = uint3(imageCoord / 2, idx);
            uint3 depthArrayBufferCoord = ssaoArrayBufferCoord + uint3(g_CACAOConsts.DeinterleavedDepthBufferOffset, 0);
            ++imageCoord.x;

            BilateralBufferVal bufferVal;

            float depth = g_BilateralUpscaleDownscaledDepth[depthArrayBufferCoord];
            // float ssaoVal = g_BilateralUpscaleInput.SampleLevel(g_PointClampSampler, float3((float2(ssaoArrayBufferCoord.xy) + 0.5f) * g_CACAOConsts.HalfViewportPixelSize, ssaoArrayBufferCoord.z), 0);
            float ssaoVal = g_BilateralUpscaleInput.SampleLevel(g_PointClampSampler, float3((float2(ssaoArrayBufferCoord.xy) + 0.5f) * g_CACAOConsts.SSAOBufferInverseDimensions, ssaoArrayBufferCoord.z), 0).x;

            bufferVal.packedDepths = DoublePackFloat16(depth);
            bufferVal.packedSsaoVals = DoublePackFloat16(ssaoVal);

            s_BilateralUpscaleBuffer[bufferCoord.x + i][bufferCoord.y] = bufferVal;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    float depths[4];
    // load depths
    {
        int2 fullBufferCoord = 2 * tid;
        int2 fullDepthBufferCoord = fullBufferCoord + g_CACAOConsts.DepthBufferOffset;

        depths[0] = ScreenSpaceToViewSpaceDepth(g_BilateralUpscaleDepth[fullDepthBufferCoord + int2(0, 0)]);
        depths[1] = ScreenSpaceToViewSpaceDepth(g_BilateralUpscaleDepth[fullDepthBufferCoord + int2(1, 0)]);
        depths[2] = ScreenSpaceToViewSpaceDepth(g_BilateralUpscaleDepth[fullDepthBufferCoord + int2(0, 1)]);
        depths[3] = ScreenSpaceToViewSpaceDepth(g_BilateralUpscaleDepth[fullDepthBufferCoord + int2(1, 1)]);
    }
    min16float4 packedDepths = min16float4(depths[0], depths[1], depths[2], depths[3]);

    float totals[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float totalWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float2 pps[] = { float2(0.0f, 0.0f), float2(0.5f, 0.0f), float2(0.0f, 0.5f), float2(0.5f, 0.5f) };

    min16float4 packedTotals = min16float4(0.0f, 0.0f, 0.0f, 0.0f);
    min16float4 packedTotalWeights = min16float4(0.0f, 0.0f, 0.0f, 0.0f);

    int2 baseBufferCoord = gtid + int2(width, height);

    float distanceSigma = g_CACAOConsts.BilateralSimilarityDistanceSigma;
    min16float2 packedDistSigma = min16float2(1.0f / distanceSigma, 1.0f / distanceSigma);
    float sigma = g_CACAOConsts.BilateralSigmaSquared;
    min16float2 packedSigma = min16float2(1.0f / sigma, 1.0f / sigma);

    for (int x = -width; x <= width; ++x)
    {
        for (int y = -height; y <= height; ++y)
        {
            int2 bufferCoord = baseBufferCoord + int2(x, y);

            BilateralBufferVal bufferVal = s_BilateralUpscaleBuffer[bufferCoord.x][bufferCoord.y];

            min16float2 u = min16float2(x, x) + min16float2(0.0f, 0.5f);
            min16float2 v1 = min16float2(y, y) + min16float2(0.0f, 0.0f);
            min16float2 v2 = min16float2(y, y) + min16float2(0.5f, 0.5f);
            u = u * u;
            v1 = v1 * v1;
            v2 = v2 * v2;

            min16float2 dist1 = u + v1;
            min16float2 dist2 = u + v2;

            min16float2 wx1 = exp(-dist1 * packedSigma);
            min16float2 wx2 = exp(-dist2 * packedSigma);

            min16float2 bufferPackedDepths = UnpackFloat16(bufferVal.packedDepths);

#if 0
            min16float2 diff1 = abs(packedDepths.xy - bufferPackedDepths);
            min16float2 diff2 = abs(packedDepths.zw - bufferPackedDepths);
#else
            min16float2 diff1 = packedDepths.xy - bufferPackedDepths;
            min16float2 diff2 = packedDepths.zw - bufferPackedDepths;
            diff1 *= diff1;
            diff2 *= diff2;
#endif

            min16float2 wy1 = exp(-diff1 * packedDistSigma);
            min16float2 wy2 = exp(-diff2 * packedDistSigma);

            min16float2 weight1 = wx1 * wy1;
            min16float2 weight2 = wx2 * wy2;

            min16float2 packedSsaoVals = UnpackFloat16(bufferVal.packedSsaoVals);
            packedTotals.xy += packedSsaoVals * weight1;
            packedTotals.zw += packedSsaoVals * weight2;
            packedTotalWeights.xy += weight1;
            packedTotalWeights.zw += weight2;
        }
    }

    uint2 outputCoord = 2 * tid;
    min16float4 outputValues = packedTotals / packedTotalWeights;
    g_BilateralUpscaleOutput[outputCoord + int2(0, 0)] = outputValues.x; // totals[0] / totalWeights[0];
    g_BilateralUpscaleOutput[outputCoord + int2(1, 0)] = outputValues.y; // totals[1] / totalWeights[1];
    g_BilateralUpscaleOutput[outputCoord + int2(0, 1)] = outputValues.z; // totals[2] / totalWeights[2];
    g_BilateralUpscaleOutput[outputCoord + int2(1, 1)] = outputValues.w; // totals[3] / totalWeights[3];
}
