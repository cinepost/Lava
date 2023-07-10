/** Edge detection render pass.
*/

#include "Scene/SceneDefines.slangh"

import Scene.Raster;
import Scene.RaytracingInline;
import Scene.Shading;

__exported import Scene.Scene;
__exported import Scene.Shading;
__exported import Scene.HitInfo;

import Experimental.Scene.Materials.StandardMaterial;
import Scene.Material.ShadingUtils;

import Utils.Math.MathHelpers;
import Utils.Sampling.SampleGenerator;

#ifndef _SHADING_RATE
#define _SHADING_RATE 1
#endif

#define is_valid(name) (is_valid_##name != 0)

cbuffer PerFrameCB
{
    uint  gSampleNumber;
    uint2 gResolution;
    float2 gDistanceRange;   // min/max occlusion distance values
}

// Input edge detection data.
Texture2D<float>            gDepth;            // optional depth buffer
Texture2D<float3>           gNormal;           // optional normal buffer
Texture2D<PackedHitInfo>    gVBuffer;          // optional visibility buffer


// Ouput
RWTexture2D<float4> gOutput;

bool calcShadingData(const float3 rayDir, const HitInfo hit, out ShadingData sd) {
    if (hit.isValid() && hit.getType() == HitType::Triangle) {
        sd = {};
        ITextureSampler lod = ExplicitLodTextureSampler(0.f);
        TriangleHit triangleHit = hit.getTriangleHit();

        // Evaluate Falcor's material parameters at the hit point.
        // TODO: Implement texLOD to enable texture filtering in prepareShadingData().
        const VertexData v = gScene.getVertexData(triangleHit);
        const uint materialID = gScene.getMaterialID(triangleHit.instanceID);
        sd = gScene.materials.prepareShadingData(v, materialID, -rayDir, lod);
        return true;
    }

    return false;
}

/** U pass.
*/
[numthreads(16, 16, 1)]
void passRayQuery(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gResolution)) return;
    const uint2 pixelPos = dispatchThreadId.xy;
    
    #if is_valid(gVBuffer)
        const HitInfo hit = HitInfo(gVBuffer[pixelPos]);
        if(hit.isValid() && hit.getType() == HitType::Triangle) {
            SampleGenerator sg = SampleGenerator(pixelPos, gSampleNumber);
            const float3 rayDir = normalize(gScene.camera.computeRayPinhole(pixelPos, gResolution).dir);
            
            ITextureSampler lod = ExplicitLodTextureSampler(0.f);
            TriangleHit triangleHit = hit.getTriangleHit();

            const VertexData v = gScene.getVertexData(triangleHit);
            const uint materialID = gScene.getMaterialID(triangleHit.instanceID);
            const ShadingData sd = gScene.materials.prepareShadingData(v, materialID, -rayDir, lod);

            float3 rayOrigin = sd.posW;
            if (gScene.doFixShadowTerminator(triangleHit.instanceID)) {
                rayOrigin = gScene.computeShadingPosition(triangleHit.instanceID, triangleHit.primitiveIndex, rayOrigin, 
                    sd.N, triangleHit.getBarycentricWeights());
            }

            RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayQuery;
            let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

            float pdf;
            
            RayDesc ray;
            ray.Origin = rayOrigin;
            ray.TMin = .001f;
            ray.TMax = gDistanceRange.y;

            float occlusion = 0.f;

            [[unroll]]
            for(uint i = 0; i < _SHADING_RATE; i ++) {

                ray.Direction = normalize(sample_cosine_hemisphere_oriented(sampleNext2D(sg), sd.N, pdf));

                rayQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
                rayQuery.Proceed();

                #if SCENE_HAS_GEOMETRY_TYPE(GEOMETRY_TYPE_TRIANGLE_MESH)
                    if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
                        const float hitT = rayQuery.CommittedRayT();
                        const float w = hitT > gDistanceRange.x ? hitT < gDistanceRange.y ? (1.f - smoothstep(gDistanceRange.x, gDistanceRange.y, hitT)) : 0.f : 1.f;
                        occlusion += (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ? w : 0.f;
                    }
                #endif
            }

            gOutput[pixelPos] = float4(1.f - (occlusion /(float)_SHADING_RATE));
            return;
        }
    #endif

    gOutput[pixelPos] = float4(1.f);
}
