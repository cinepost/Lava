import Scene.Camera.Camera;
import CACAOData;

Texture2DArray<float>    g_ViewspaceDepthSource      : register(t0);
Texture1D<uint>          g_LoadCounter               : register(t2);
Texture2D<float>         g_ImportanceMap             : register(t3);
Texture2DArray           g_FinalSSAO                 : register(t4);
Texture1D<float>         g_ZeroTexture               : register(t5);
Texture2DArray           g_deinterlacedNormals       : register(t6);

RWTexture2DArray<float2> g_SSAOOutput                : register(u0);


// calculate effect radius and fit our screen sampling pattern inside it
void CalculateRadiusParameters(const float pixCenterLength, const float2 pixelDirRBViewspaceSizeAtCenterZ, out float pixLookupRadiusMod, out float effectRadius, out float falloffCalcMulSq)
{
    effectRadius = g_CACAOConsts.EffectRadius;

    // leaving this out for performance reasons: use something similar if radius needs to scale based on distance
    //effectRadius *= pow( pixCenterLength, g_CACAOConsts.RadiusDistanceScalingFunctionPow);

    // when too close, on-screen sampling disk will grow beyond screen size; limit this to avoid closeup temporal artifacts
    const float tooCloseLimitMod = saturate(pixCenterLength * g_CACAOConsts.EffectSamplingRadiusNearLimitRec) * 0.8 + 0.2;

    effectRadius *= tooCloseLimitMod;

    // 0.85 is to reduce the radius to allow for more samples on a slope to still stay within influence
    pixLookupRadiusMod = (0.85 * effectRadius) / pixelDirRBViewspaceSizeAtCenterZ.x;

    // used to calculate falloff (both for AO samples and per-sample weights)
    falloffCalcMulSq = -1.0f / (effectRadius*effectRadius);
}


float3 DecodeNormal(float3 encodedNormal)
{
    float3 normal = encodedNormal * g_CACAOConsts.NormalsUnpackMul.xxx + g_CACAOConsts.NormalsUnpackAdd.xxx;

#if SSAO_ENABLE_NORMAL_WORLD_TO_VIEW_CONVERSION
    normal = mul(normal, (float3x3)g_CACAOConsts.NormalsWorldToViewspaceMatrix).xyz;
#endif

    // normal = normalize( normal );    // normalize adds around 2.5% cost on High settings but makes little (PSNR 66.7) visual difference when normals are as in the sample (stored in R8G8B8A8_UNORM,
    //                                  // decoded in the shader), however it will likely be required if using different encoding/decoding or the inputs are not normalized, etc.

    return normal;
}

// all vectors in viewspace
float CalculatePixelObscurance(float3 pixelNormal, float3 hitDelta, float falloffCalcMulSq)
{
    float lengthSq = dot(hitDelta, hitDelta);
    float NdotD = dot(pixelNormal, hitDelta) / sqrt(lengthSq);

    float falloffMult = max(0.0, lengthSq * falloffCalcMulSq + 1.0);

    return max(0, NdotD - g_CACAOConsts.EffectHorizonAngleThreshold) * falloffMult;
}

void SSAOTapInner(const int qualityLevel, inout float obscuranceSum, inout float weightSum, const float2 samplingUV, const float mipLevel, const float3 pixCenterPos, const float3 negViewspaceDir, float3 pixelNormal, const float falloffCalcMulSq, const float weightMod, const int dbgTapIndex)
{
    // get depth at sample
    float viewspaceSampleZ = g_ViewspaceDepthSource.SampleLevel(g_ViewspaceDepthTapSampler, float3(samplingUV.xy, 0.0f), mipLevel).x; // * g_CACAOConsts.MaxViewspaceDepth;

    // convert to viewspace
    // float3 hitPos = NDCToViewspace(samplingUV.xy, viewspaceSampleZ).xyz;
    float3 hitPos = DepthBufferUVToViewspace(samplingUV.xy, viewspaceSampleZ).xyz;
    float3 hitDelta = hitPos - pixCenterPos;

    float obscurance = CalculatePixelObscurance(pixelNormal, hitDelta, falloffCalcMulSq);
    float weight = 1.0;

    if (qualityLevel >= SSAO_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET)
    {
        //float reduct = max( 0, dot( hitDelta, negViewspaceDir ) );
        float reduct = max(0, -hitDelta.z); // cheaper, less correct version
        reduct = saturate(reduct * g_CACAOConsts.NegRecEffectRadius + 2.0); // saturate( 2.0 - reduct / g_CACAOConsts.EffectRadius );
        weight = SSAO_HALOING_REDUCTION_AMOUNT * reduct + (1.0 - SSAO_HALOING_REDUCTION_AMOUNT);
    }
    weight *= weightMod;
    obscuranceSum += obscurance * weight;
    weightSum += weight;
}

void SSAOTap(const int qualityLevel, inout float obscuranceSum, inout float weightSum, const int tapIndex, const float2x2 rotScale, const float3 pixCenterPos, const float3 negViewspaceDir, float3 pixelNormal, const float2 normalizedScreenPos, const float2 depthBufferUV, const float mipOffset, const float falloffCalcMulSq, float weightMod, float2 normXY, float normXYLength)
{
    float2  sampleOffset;
    float   samplePow2Len;

    // patterns
    {
        float4 newSample = g_samplePatternMain[tapIndex];
        sampleOffset = mul(rotScale, newSample.xy);
        samplePow2Len = newSample.w;                      // precalculated, same as: samplePow2Len = log2( length( newSample.xy ) );
        weightMod *= newSample.z;
    }

    // snap to pixel center (more correct obscurance math, avoids artifacts)
    sampleOffset = round(sampleOffset);

    // calculate MIP based on the sample distance from the centre, similar to as described 
    // in http://graphics.cs.williams.edu/papers/SAOHPG12/.
    float mipLevel = (qualityLevel < SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (samplePow2Len + mipOffset);

    float2 samplingUV = sampleOffset * g_CACAOConsts.DeinterleavedDepthBufferInverseDimensions + depthBufferUV;

    SSAOTapInner(qualityLevel, obscuranceSum, weightSum, samplingUV, mipLevel, pixCenterPos, negViewspaceDir, pixelNormal, falloffCalcMulSq, weightMod, tapIndex * 2);

    // for the second tap, just use the mirrored offset
    float2 sampleOffsetMirroredUV = -sampleOffset;

    // tilt the second set of samples so that the disk is effectively rotated by the normal
    // effective at removing one set of artifacts, but too expensive for lower quality settings
    if (qualityLevel >= SSAO_TILT_SAMPLES_ENABLE_AT_QUALITY_PRESET)
    {
        float dotNorm = dot(sampleOffsetMirroredUV, normXY);
        sampleOffsetMirroredUV -= dotNorm * normXYLength * normXY;
        sampleOffsetMirroredUV = round(sampleOffsetMirroredUV);
    }

    // snap to pixel center (more correct obscurance math, avoids artifacts)
    float2 samplingMirroredUV = sampleOffsetMirroredUV * g_CACAOConsts.DeinterleavedDepthBufferInverseDimensions + depthBufferUV;

    SSAOTapInner(qualityLevel, obscuranceSum, weightSum, samplingMirroredUV, mipLevel, pixCenterPos, negViewspaceDir, pixelNormal, falloffCalcMulSq, weightMod, tapIndex * 2 + 1);
}

struct SSAOHits
{
    float3 hits[2];
    float weightMod;
};

SSAOHits SSAOGetHits(const int qualityLevel, const float2 depthBufferUV, const int tapIndex, const float mipOffset, const float2x2 rotScale, const float4 newSample)
{
    SSAOHits result;

    float2  sampleOffset;
    float   samplePow2Len;

    // patterns
    {
        // float4 newSample = g_samplePatternMain[tapIndex];
        sampleOffset = mul(rotScale, newSample.xy);
        samplePow2Len = newSample.w;                      // precalculated, same as: samplePow2Len = log2( length( newSample.xy ) );
        result.weightMod = newSample.z;
    }

    // snap to pixel center (more correct obscurance math, avoids artifacts)
    sampleOffset = round(sampleOffset) * g_CACAOConsts.DeinterleavedDepthBufferInverseDimensions;

    float mipLevel = (qualityLevel < SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (samplePow2Len + mipOffset);

    float2 sampleUV = depthBufferUV + sampleOffset;
    result.hits[0] = float3(sampleUV, g_ViewspaceDepthSource.SampleLevel(g_ViewspaceDepthTapSampler, float3(sampleUV, 0.0f), mipLevel).x);

    sampleUV = depthBufferUV - sampleOffset;
    result.hits[1] = float3(sampleUV, g_ViewspaceDepthSource.SampleLevel(g_ViewspaceDepthTapSampler, float3(sampleUV, 0.0f), mipLevel).x);

    return result;
}

struct SSAOSampleData
{
    float2 uvOffset;
    float mipLevel;
    float weightMod;
};

SSAOSampleData SSAOGetSampleData(const int qualityLevel, const float2x2 rotScale, const float4 newSample, const float mipOffset)
{
    float2  sampleOffset = mul(rotScale, newSample.xy);
    sampleOffset = round(sampleOffset) * g_CACAOConsts.DeinterleavedDepthBufferInverseDimensions;

    float samplePow2Len = newSample.w;
    float mipLevel = (qualityLevel < SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (samplePow2Len + mipOffset);

    SSAOSampleData result;

    result.uvOffset = sampleOffset;
    result.mipLevel = mipLevel;
    result.weightMod = newSample.z;

    return result;
}

SSAOHits SSAOGetHits2(SSAOSampleData data, const float2 depthBufferUV)
{
    SSAOHits result;
    result.weightMod = data.weightMod;
    float2 sampleUV = depthBufferUV + data.uvOffset;
    result.hits[0] = float3(sampleUV, g_ViewspaceDepthSource.SampleLevel(g_ViewspaceDepthTapSampler, float3(sampleUV, 0.0f), data.mipLevel).x);
    sampleUV = depthBufferUV - data.uvOffset;
    result.hits[1] = float3(sampleUV, g_ViewspaceDepthSource.SampleLevel(g_ViewspaceDepthTapSampler, float3(sampleUV, 0.0f), data.mipLevel).x);
    return result;
}

void SSAOAddHits(const int qualityLevel, const float3 pixCenterPos, const float3 pixelNormal, const float falloffCalcMulSq, inout float weightSum, inout float obscuranceSum, SSAOHits hits)
{
    float weight = hits.weightMod;
    [unroll]
    for (int hitIndex = 0; hitIndex < 2; ++hitIndex)
    {
        float3 hit = hits.hits[hitIndex];
        float3 hitPos = DepthBufferUVToViewspace(hit.xy, hit.z);
        float3 hitDelta = hitPos - pixCenterPos;

        float obscurance = CalculatePixelObscurance(pixelNormal, hitDelta, falloffCalcMulSq);

        if (qualityLevel >= SSAO_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET)
        {
            //float reduct = max( 0, dot( hitDelta, negViewspaceDir ) );
            float reduct = max(0, -hitDelta.z); // cheaper, less correct version
            reduct = saturate(reduct * g_CACAOConsts.NegRecEffectRadius + 2.0); // saturate( 2.0 - reduct / g_CACAOConsts.EffectRadius );
            weight = SSAO_HALOING_REDUCTION_AMOUNT * reduct + (1.0 - SSAO_HALOING_REDUCTION_AMOUNT);
        }
        obscuranceSum += obscurance * weight;
        weightSum += weight;
    }
}

void SSAOTap2(const int qualityLevel, inout float obscuranceSum, inout float weightSum, const int tapIndex, const float2x2 rotScale, const float3 pixCenterPos, const float3 negViewspaceDir, float3 pixelNormal, const float2 normalizedScreenPos, const float mipOffset, const float falloffCalcMulSq, float weightMod, float2 normXY, float normXYLength)
{
    float4 newSample = g_samplePatternMain[tapIndex];
    SSAOSampleData data = SSAOGetSampleData(qualityLevel, rotScale, newSample, mipOffset);
    SSAOHits hits = SSAOGetHits2(data, normalizedScreenPos);
    SSAOAddHits(qualityLevel, pixCenterPos, pixelNormal, falloffCalcMulSq, weightSum, obscuranceSum, hits);
}


void GenerateSSAOShadowsInternal(out float outShadowTerm, out float4 outEdges, out float outWeight, const float2 SVPos/*, const float2 normalizedScreenPos*/, uniform int qualityLevel, bool adaptiveBase)
{
    float2 SVPosRounded = trunc(SVPos);
    uint2 SVPosui = uint2(SVPosRounded); //same as uint2( SVPos )

    const int numberOfTaps = (adaptiveBase) ? (SSAO_ADAPTIVE_TAP_BASE_COUNT) : (g_numTaps[qualityLevel]);
    float pixZ, pixLZ, pixTZ, pixRZ, pixBZ;

    float2 depthBufferUV = (SVPos + 0.5f) * g_CACAOConsts.DeinterleavedDepthBufferInverseDimensions + g_CACAOConsts.DeinterleavedDepthBufferNormalisedOffset;
    float4 valuesUL = g_ViewspaceDepthSource.GatherRed(g_PointMirrorSampler, float3(depthBufferUV, 0.0f), int2(-1, -1));
    float4 valuesBR = g_ViewspaceDepthSource.GatherRed(g_PointMirrorSampler, float3(depthBufferUV, 0.0f));

    // get this pixel's viewspace depth
    pixZ = valuesUL.y; //float pixZ = g_ViewspaceDepthSource.SampleLevel( g_PointMirrorSampler, float3(normalizedScreenPos, 0.0f), 0.0 ).x; // * g_CACAOConsts.MaxViewspaceDepth;

    // get left right top bottom neighbouring pixels for edge detection (gets compiled out on qualityLevel == 0)
    pixLZ = valuesUL.x;
    pixTZ = valuesUL.z;
    pixRZ = valuesBR.z;
    pixBZ = valuesBR.x;

    // float2 normalizedScreenPos = SVPosRounded * g_CACAOConsts.Viewport2xPixelSize + g_CACAOConsts.Viewport2xPixelSize_x_025;
    float2 normalizedScreenPos = (SVPosRounded + 0.5f) * g_CACAOConsts.SSAOBufferInverseDimensions;
    float3 pixCenterPos = NDCToViewspace(normalizedScreenPos, pixZ); // g

    // Load this pixel's viewspace normal
    // uint2 fullResCoord = 2 * (SVPosui * 2 + g_CACAOConsts.PerPassFullResCoordOffset.xy);
    int3 normalCoord = int3(SVPosui, g_CACAOConsts.PassIndex);
    float3 pixelNormal = g_deinterlacedNormals[normalCoord].xyz;

    // optimized approximation of:  float2 pixelDirRBViewspaceSizeAtCenterZ = NDCToViewspace( normalizedScreenPos.xy + g_CACAOConsts._ViewportPixelSize.xy, pixCenterPos.z ).xy - pixCenterPos.xy;
    // const float2 pixelDirRBViewspaceSizeAtCenterZ = pixCenterPos.z * g_CACAOConsts.NDCToViewMul * g_CACAOConsts.Viewport2xPixelSize;
    const float2 pixelDirRBViewspaceSizeAtCenterZ = pixCenterPos.z * g_CACAOConsts.NDCToViewMul * g_CACAOConsts.SSAOBufferInverseDimensions;

    float pixLookupRadiusMod;
    float falloffCalcMulSq;

    // calculate effect radius and fit our screen sampling pattern inside it
    float effectViewspaceRadius;
    CalculateRadiusParameters(length(pixCenterPos), pixelDirRBViewspaceSizeAtCenterZ, pixLookupRadiusMod, effectViewspaceRadius, falloffCalcMulSq);

    // calculate samples rotation/scaling
    float2x2 rotScale;
    {
        // reduce effect radius near the screen edges slightly; ideally, one would render a larger depth buffer (5% on each side) instead
        if (!adaptiveBase && (qualityLevel >= SSAO_REDUCE_RADIUS_NEAR_SCREEN_BORDER_ENABLE_AT_QUALITY_PRESET))
        {
            float nearScreenBorder = min(min(depthBufferUV.x, 1.0 - depthBufferUV.x), min(depthBufferUV.y, 1.0 - depthBufferUV.y));
            nearScreenBorder = saturate(10.0 * nearScreenBorder + 0.6);
            pixLookupRadiusMod *= nearScreenBorder;
        }

        // load & update pseudo-random rotation matrix
        uint pseudoRandomIndex = uint(SVPosRounded.y * 2 + SVPosRounded.x) % 5;
        float4 rs = g_CACAOConsts.PatternRotScaleMatrices[pseudoRandomIndex];
        rotScale = float2x2(rs.x * pixLookupRadiusMod, rs.y * pixLookupRadiusMod, rs.z * pixLookupRadiusMod, rs.w * pixLookupRadiusMod);
    }

    // the main obscurance & sample weight storage
    float obscuranceSum = 0.0;
    float weightSum = 0.0;

    // edge mask for between this and left/right/top/bottom neighbour pixels - not used in quality level 0 so initialize to "no edge" (1 is no edge, 0 is edge)
    float4 edgesLRTB = float4(1.0, 1.0, 1.0, 1.0);

    // Move center pixel slightly towards camera to avoid imprecision artifacts due to using of 16bit depth buffer; a lot smaller offsets needed when using 32bit floats
    pixCenterPos *= g_CACAOConsts.DepthPrecisionOffsetMod;

    if (!adaptiveBase && (qualityLevel >= SSAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET))
    {
        edgesLRTB = CalculateEdges(pixZ, pixLZ, pixRZ, pixTZ, pixBZ);
    }

    // adds a more high definition sharp effect, which gets blurred out (reuses left/right/top/bottom samples that we used for edge detection)
    if (!adaptiveBase && (qualityLevel >= SSAO_DETAIL_AO_ENABLE_AT_QUALITY_PRESET))
    {
        // disable in case of quality level 4 (reference)
        if (qualityLevel != 4)
        {
            //approximate neighbouring pixels positions (actually just deltas or "positions - pixCenterPos" )
            float3 viewspaceDirZNormalized = float3(pixCenterPos.xy / pixCenterPos.zz, 1.0);

            // very close approximation of: float3 pixLPos  = NDCToViewspace( normalizedScreenPos + float2( -g_CACAOConsts.HalfViewportPixelSize.x, 0.0 ), pixLZ ).xyz - pixCenterPos.xyz;
            float3 pixLDelta = float3(-pixelDirRBViewspaceSizeAtCenterZ.x, 0.0, 0.0) + viewspaceDirZNormalized * (pixLZ - pixCenterPos.z);
            // very close approximation of: float3 pixRPos  = NDCToViewspace( normalizedScreenPos + float2( +g_CACAOConsts.HalfViewportPixelSize.x, 0.0 ), pixRZ ).xyz - pixCenterPos.xyz;
            float3 pixRDelta = float3(+pixelDirRBViewspaceSizeAtCenterZ.x, 0.0, 0.0) + viewspaceDirZNormalized * (pixRZ - pixCenterPos.z);
            // very close approximation of: float3 pixTPos  = NDCToViewspace( normalizedScreenPos + float2( 0.0, -g_CACAOConsts.HalfViewportPixelSize.y ), pixTZ ).xyz - pixCenterPos.xyz;
            float3 pixTDelta = float3(0.0, -pixelDirRBViewspaceSizeAtCenterZ.y, 0.0) + viewspaceDirZNormalized * (pixTZ - pixCenterPos.z);
            // very close approximation of: float3 pixBPos  = NDCToViewspace( normalizedScreenPos + float2( 0.0, +g_CACAOConsts.HalfViewportPixelSize.y ), pixBZ ).xyz - pixCenterPos.xyz;
            float3 pixBDelta = float3(0.0, +pixelDirRBViewspaceSizeAtCenterZ.y, 0.0) + viewspaceDirZNormalized * (pixBZ - pixCenterPos.z);

            const float rangeReductionConst = 4.0f;                         // this is to avoid various artifacts
            const float modifiedFalloffCalcMulSq = rangeReductionConst * falloffCalcMulSq;

            float4 additionalObscurance;
            additionalObscurance.x = CalculatePixelObscurance(pixelNormal, pixLDelta, modifiedFalloffCalcMulSq);
            additionalObscurance.y = CalculatePixelObscurance(pixelNormal, pixRDelta, modifiedFalloffCalcMulSq);
            additionalObscurance.z = CalculatePixelObscurance(pixelNormal, pixTDelta, modifiedFalloffCalcMulSq);
            additionalObscurance.w = CalculatePixelObscurance(pixelNormal, pixBDelta, modifiedFalloffCalcMulSq);

            obscuranceSum += g_CACAOConsts.DetailAOStrength * dot(additionalObscurance, edgesLRTB);
        }
    }

    // Sharp normals also create edges - but this adds to the cost as well
    if (!adaptiveBase && (qualityLevel >= SSAO_NORMAL_BASED_EDGES_ENABLE_AT_QUALITY_PRESET))
    {
        float3 neighbourNormalL = g_deinterlacedNormals[normalCoord + int3(-1, +0, 0)].xyz;
        float3 neighbourNormalR = g_deinterlacedNormals[normalCoord + int3(+1, +0, 0)].xyz;
        float3 neighbourNormalT = g_deinterlacedNormals[normalCoord + int3(+0, -1, 0)].xyz;
        float3 neighbourNormalB = g_deinterlacedNormals[normalCoord + int3(+0, +1, 0)].xyz;
        

        const float dotThreshold = SSAO_NORMAL_BASED_EDGES_DOT_THRESHOLD;

        float4 normalEdgesLRTB;
        normalEdgesLRTB.x = saturate((dot(pixelNormal, neighbourNormalL) + dotThreshold));
        normalEdgesLRTB.y = saturate((dot(pixelNormal, neighbourNormalR) + dotThreshold));
        normalEdgesLRTB.z = saturate((dot(pixelNormal, neighbourNormalT) + dotThreshold));
        normalEdgesLRTB.w = saturate((dot(pixelNormal, neighbourNormalB) + dotThreshold));

        //#define SSAO_SMOOTHEN_NORMALS // fixes some aliasing artifacts but kills a lot of high detail and adds to the cost - not worth it probably but feel free to play with it
#ifdef SSAO_SMOOTHEN_NORMALS
        //neighbourNormalL  = LoadNormal( fullResCoord, int2( -1,  0 ) );
        //neighbourNormalR  = LoadNormal( fullResCoord, int2(  1,  0 ) );
        //neighbourNormalT  = LoadNormal( fullResCoord, int2(  0, -1 ) );
        //neighbourNormalB  = LoadNormal( fullResCoord, int2(  0,  1 ) );
        pixelNormal += neighbourNormalL * edgesLRTB.x + neighbourNormalR * edgesLRTB.y + neighbourNormalT * edgesLRTB.z + neighbourNormalB * edgesLRTB.w;
        pixelNormal = normalize(pixelNormal);
#endif

        edgesLRTB *= normalEdgesLRTB;
    }



    const float globalMipOffset = SSAO_DEPTH_MIPS_GLOBAL_OFFSET;
    float mipOffset = (qualityLevel < SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (log2(pixLookupRadiusMod) + globalMipOffset);

    // Used to tilt the second set of samples so that the disk is effectively rotated by the normal
    // effective at removing one set of artifacts, but too expensive for lower quality settings
    float2 normXY = float2(pixelNormal.x, pixelNormal.y);
    float normXYLength = length(normXY);
    normXY /= float2(normXYLength, -normXYLength);
    normXYLength *= SSAO_TILT_SAMPLES_AMOUNT;

    const float3 negViewspaceDir = -normalize(pixCenterPos);

    // standard, non-adaptive approach
    if ((qualityLevel != 3) || adaptiveBase)
    {
        //SSAOHits prevHits = SSAOGetHits(qualityLevel, normalizedScreenPos, 0, mipOffset);

#if 0
        float4 newSample = g_samplePatternMain[0];
        // float zero = g_ZeroTexture.SampleLevel(g_PointClampSampler, float2(0.5f, 0.5f), 0);
        SSAOSampleData data = SSAOGetSampleData(qualityLevel, rotScale, newSample, mipOffset);
        SSAOHits hits = SSAOGetHits2(data, depthBufferUV);
        newSample = g_samplePatternMain[1];
        // newSample.x += zero;
        data = SSAOGetSampleData(qualityLevel, rotScale, newSample, mipOffset);

        [unroll]
        for (int i = 0; i < numberOfTaps - 1; ++i)
        {
            // zero = g_ZeroTexture.SampleLevel(g_PointClampSampler, float2(0.5f + zero, 0.5f), 0);
            SSAOHits nextHits = SSAOGetHits2(data, depthBufferUV);
            // hits.hits[0].x += zero;
            newSample = g_samplePatternMain[i + 2];

            SSAOAddHits(qualityLevel, pixCenterPos, pixelNormal, falloffCalcMulSq, weightSum, obscuranceSum, hits);
            SSAOSampleData nextData = SSAOGetSampleData(qualityLevel, rotScale, newSample, mipOffset);
            hits = nextHits;
            data = nextData;
        }

        // last loop iteration
        {
            SSAOAddHits(qualityLevel, pixCenterPos, pixelNormal, falloffCalcMulSq, weightSum, obscuranceSum, hits);
        }
#else

        [unroll]
        for (int i = 0; i < numberOfTaps; i++)
        {
            SSAOTap(qualityLevel, obscuranceSum, weightSum, i, rotScale, pixCenterPos, negViewspaceDir, pixelNormal, normalizedScreenPos, depthBufferUV, mipOffset, falloffCalcMulSq, 1.0, normXY, normXYLength);
            // SSAOHits hits = SSAOGetHits(qualityLevel, normalizedScreenPos, i, mipOffset, rotScale);
            // SSAOAddHits(qualityLevel, pixCenterPos, pixelNormal, 1.0f, falloffCalcMulSq, weightSum, obscuranceSum, hits);
        }

#endif
    }
    else // if( qualityLevel == 3 ) adaptive approach
    {
        // add new ones if needed
        float2 fullResUV = normalizedScreenPos + g_CACAOConsts.PerPassFullResUVOffset.xy;
        float importance = g_ImportanceMap.SampleLevel(g_LinearClampSampler, fullResUV, 0.0).x;

        // this is to normalize SSAO_DETAIL_AO_AMOUNT across all pixel regardless of importance
        obscuranceSum *= (SSAO_ADAPTIVE_TAP_BASE_COUNT / (float)SSAO_MAX_TAPS) + (importance * SSAO_ADAPTIVE_TAP_FLEXIBLE_COUNT / (float)SSAO_MAX_TAPS);

        // load existing base values
        float2 baseValues = g_FinalSSAO.Load(int4(SVPosui, g_CACAOConsts.PassIndex, 0)).xy;
        weightSum += baseValues.y * (float)(SSAO_ADAPTIVE_TAP_BASE_COUNT * 4.0);
        obscuranceSum += (baseValues.x) * weightSum;

        // increase importance around edges
        float edgeCount = dot(1.0 - edgesLRTB, float4(1.0, 1.0, 1.0, 1.0));

        float avgTotalImportance = (float)g_LoadCounter[0] * g_CACAOConsts.LoadCounterAvgDiv;

        float importanceLimiter = saturate(g_CACAOConsts.AdaptiveSampleCountLimit / avgTotalImportance);
        importance *= importanceLimiter;

        float additionalSampleCountFlt = SSAO_ADAPTIVE_TAP_FLEXIBLE_COUNT * importance;

        additionalSampleCountFlt += 1.5;
        uint additionalSamples = uint(additionalSampleCountFlt);
        uint additionalSamplesTo = min(SSAO_MAX_TAPS, additionalSamples + SSAO_ADAPTIVE_TAP_BASE_COUNT);

        // sample loop
        {
            float4 newSample = g_samplePatternMain[SSAO_ADAPTIVE_TAP_BASE_COUNT];
            SSAOSampleData data = SSAOGetSampleData(qualityLevel, rotScale, newSample, mipOffset);
            SSAOHits hits = SSAOGetHits2(data, depthBufferUV);
            newSample = g_samplePatternMain[SSAO_ADAPTIVE_TAP_BASE_COUNT + 1];

            for (uint i = SSAO_ADAPTIVE_TAP_BASE_COUNT; i < additionalSamplesTo - 1; i++)
            {
                data = SSAOGetSampleData(qualityLevel, rotScale, newSample, mipOffset);
                newSample = g_samplePatternMain[i + 2];
                SSAOHits nextHits = SSAOGetHits2(data, depthBufferUV);

                // float zero = g_ZeroTexture.SampleLevel(g_ZeroTextureSampler, (float)i, 0.0f);
                // hits.weightMod += zero;

                SSAOAddHits(qualityLevel, pixCenterPos, pixelNormal, falloffCalcMulSq, weightSum, obscuranceSum, hits);
                hits = nextHits;
            }

            // last loop iteration
            {
                SSAOAddHits(qualityLevel, pixCenterPos, pixelNormal, falloffCalcMulSq, weightSum, obscuranceSum, hits);
            }
        }
    }

    // early out for adaptive base - just output weight (used for the next pass)
    if (adaptiveBase)
    {
        float obscurance = obscuranceSum / weightSum;

        outShadowTerm = obscurance;
        outEdges = 0;
        outWeight = weightSum;
        return;
    }

    // calculate weighted average
    float obscurance = obscuranceSum / weightSum;

    // calculate fadeout (1 close, gradient, 0 far)
    float fadeOut = saturate(pixCenterPos.z * g_CACAOConsts.EffectFadeOutMul + g_CACAOConsts.EffectFadeOutAdd);

    // Reduce the SSAO shadowing if we're on the edge to remove artifacts on edges (we don't care for the lower quality one)
    if (!adaptiveBase && (qualityLevel >= SSAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET))
    {
        // float edgeCount = dot( 1.0-edgesLRTB, float4( 1.0, 1.0, 1.0, 1.0 ) );

        // when there's more than 2 opposite edges, start fading out the occlusion to reduce aliasing artifacts
        float edgeFadeoutFactor = saturate((1.0 - edgesLRTB.x - edgesLRTB.y) * 0.35) + saturate((1.0 - edgesLRTB.z - edgesLRTB.w) * 0.35);

        // (experimental) if you want to reduce the effect next to any edge
        // edgeFadeoutFactor += 0.1 * saturate( dot( 1 - edgesLRTB, float4( 1, 1, 1, 1 ) ) );

        fadeOut *= saturate(1.0 - edgeFadeoutFactor);
    }

    // same as a bove, but a lot more conservative version
    // fadeOut *= saturate( dot( edgesLRTB, float4( 0.9, 0.9, 0.9, 0.9 ) ) - 2.6 );

    // strength
    obscurance = g_CACAOConsts.EffectShadowStrength * obscurance;

    // clamp
    obscurance = min(obscurance, g_CACAOConsts.EffectShadowClamp);

    // fadeout
    obscurance *= fadeOut;

    // conceptually switch to occlusion with the meaning being visibility (grows with visibility, occlusion == 1 implies full visibility), 
    // to be in line with what is more commonly used.
    float occlusion = 1.0 - obscurance;

    // modify the gradient
    // note: this cannot be moved to a later pass because of loss of precision after storing in the render target
    occlusion = pow(saturate(occlusion), g_CACAOConsts.EffectShadowPow);

    // outputs!
    outShadowTerm = occlusion;    // Our final 'occlusion' term (0 means fully occluded, 1 means fully lit)
    outEdges = edgesLRTB;    // These are used to prevent blurring across edges, 1 means no edge, 0 means edge, 0.5 means half way there, etc.
    outWeight = weightSum;
}

[numthreads(GENERATE_WIDTH, GENERATE_HEIGHT, 1)]
void CSGenerateQ0(uint2 coord : SV_DispatchThreadID)
{
    float2 inPos = (float2)coord;
    float   outShadowTerm;
    float   outWeight;
    float4  outEdges;
    GenerateSSAOShadowsInternal(outShadowTerm, outEdges, outWeight, inPos.xy, 0, false);
    float2 out0;
    out0.x = outShadowTerm;
    out0.y = PackEdges(float4(1, 1, 1, 1)); // no edges in low quality
    g_SSAOOutput[uint3(coord, 0)] = out0;
}

[numthreads(GENERATE_WIDTH, GENERATE_HEIGHT, 1)]
void CSGenerateQ1(uint2 coord : SV_DispatchThreadID)
{
    float2 inPos = (float2)coord;
    float   outShadowTerm;
    float   outWeight;
    float4  outEdges;
    GenerateSSAOShadowsInternal(outShadowTerm, outEdges, outWeight, inPos.xy, 1, false);
    float2 out0;
    out0.x = outShadowTerm;
    out0.y = PackEdges(outEdges);
    g_SSAOOutput[uint3(coord, 0)] = out0;
}

[numthreads(GENERATE_WIDTH, GENERATE_HEIGHT, 1)]
void CSGenerateQ2(uint2 coord : SV_DispatchThreadID)
{
    float2 inPos = (float2)coord;
    float   outShadowTerm;
    float   outWeight;
    float4  outEdges;
    GenerateSSAOShadowsInternal(outShadowTerm, outEdges, outWeight, inPos.xy, 2, false);
    float2 out0;
    out0.x = outShadowTerm;
    out0.y = PackEdges(outEdges);
    g_SSAOOutput[uint3(coord, 0)] = out0;
}

[numthreads(GENERATE_WIDTH, GENERATE_HEIGHT, 1)]
void CSGenerateQ3Base(uint2 coord : SV_DispatchThreadID)
{
    float2 inPos = (float2)coord;
    float   outShadowTerm;
    float   outWeight;
    float4  outEdges;
    GenerateSSAOShadowsInternal(outShadowTerm, outEdges, outWeight, inPos.xy, 3, true);
    float2 out0;
    out0.x = outShadowTerm;
    out0.y = outWeight / ((float)SSAO_ADAPTIVE_TAP_BASE_COUNT * 4.0); //0.0; //frac(outWeight / 6.0);// / (float)(SSAO_MAX_TAPS * 4.0);
    g_SSAOOutput[uint3(coord, 0)] = out0;
}

[numthreads(GENERATE_WIDTH, GENERATE_HEIGHT, 1)]
void CSGenerateQ3(uint2 coord : SV_DispatchThreadID)
{

    float2 inPos = (float2)coord;
    float   outShadowTerm;
    float   outWeight;
    float4  outEdges;
    GenerateSSAOShadowsInternal(outShadowTerm, outEdges, outWeight, inPos.xy, 3, false);
    float2 out0;
    out0.x = outShadowTerm;
    out0.y = PackEdges(outEdges);
    g_SSAOOutput[uint3(coord, 0)] = out0;
}