/** Edge detection render pass.
*/

#include "Scene/SceneDefines.slangh"
#include "Utils/Math/MathConstants.slangh"

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

#ifndef _IGNORE_BACK_FACE
#define _IGNORE_BACK_FACE 0
#endif

#define is_valid(name) (is_valid_##name != 0)

cbuffer PerFrameCB {
    uint    gSampleNumber;
    uint2   gResolution;
    float2  gFalloffRange;   // min/max occlusion distance values
    int     gRandomSeed;
    float   gRayBias = 0.f;
};

// Input edge detection data.
Texture2D<float>            gDepth;            // optional depth buffer
Texture2D<float3>           gNormal;           // optional normal buffer
Texture2D<PackedHitInfo>    gVBuffer;          // optional visibility buffer


// Ouput
RWTexture2D<float4> gOutput;

static const float kMinRayTracingBias = 0.0001f;

bool calcShadingData(const float3 viewDir, const HitInfo hit, out ShadingData sd) {
    if (hit.isValid() && hit.getType() == HitType::Triangle) {
        sd = {};
        ITextureSampler lod = ExplicitLodTextureSampler(0.f);
        TriangleHit triangleHit = hit.getTriangleHit();

        // Evaluate Falcor's material parameters at the hit point.
        // TODO: Implement texLOD to enable texture filtering in prepareShadingData().
        const VertexData v = gScene.getVertexData(triangleHit);
        const uint materialID = gScene.getMaterialID(triangleHit.instanceID);
        sd = gScene.materials.prepareShadingData(v, materialID, -viewDir, lod);
        return true;
    }

    return false;
}


[numthreads(16, 16, 1)]
void passRayQuery(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gResolution)) return;
    const uint2 pixelPos = dispatchThreadId.xy;

    #if is_valid(gVBuffer)
        const HitInfo hit = HitInfo(gVBuffer[pixelPos]);
        if(hit.isValid() && hit.getType() == HitType::Triangle) {
            SampleGenerator sg = SampleGenerator(pixelPos, gSampleNumber + gRandomSeed);
            const float3 viewDir = normalize(gScene.camera.computeRayPinhole(pixelPos, gResolution).dir);
            
            ITextureSampler lod = ExplicitLodTextureSampler(0.f);
            TriangleHit triangleHit = hit.getTriangleHit();

            const VertexData v = gScene.getVertexData(triangleHit);
            const uint materialID = gScene.getMaterialID(triangleHit.instanceID);
            const ShadingData sd = gScene.materials.prepareShadingData(v, materialID, -viewDir, lod);

            uint hints = (uint)MaterialInstanceHints::AdjustShadingNormal;
            const IMaterialInstance mi = gScene.materials.getMaterialInstance(sd, lod, hints);

#if (_IGNORE_BACK_FACE == 1)
            RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES> rayQuery;
            let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
#else 
            RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> rayQuery;
            let rayFlags = RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
#endif
            
            float pdf;
            
            RayDesc ray;
            ray.Origin = sd.posW;
            ray.TMax = gFalloffRange.y >= kMinRayTracingBias ? gFalloffRange.y : FLT_MAX;

            if(abs(gRayBias) > kMinRayTracingBias) {
                ray.TMin = gRayBias;
            }

            float occlusion = 0.f;

            float3 Ng = dot(sd.faceN, viewDir) >= 0.f ? -sd.faceN : sd.faceN;
            float3 Ns = faceforward(sd.frame.N, viewDir, sd.faceN);

            [[unroll]]
            for(uint i = 0; i < _SHADING_RATE; i ++) {
                ray.Direction = normalize(sample_cosine_hemisphere_oriented(sampleNext2D(sg), Ns, pdf));

                if(dot(ray.Direction, Ng) <= 0.0f) {
                    continue;
                }

                #if SCENE_HAS_GEOMETRY_TYPE(GEOMETRY_TYPE_TRIANGLE_MESH)

                    rayQuery.TraceRayInline(gScene.rtAccel, rayFlags, 0x02, ray);
                    rayQuery.Proceed();

                    if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
                        const float hitT = rayQuery.CommittedRayT();
                        float w = 1.f;
                        [[branch]] if(gFalloffRange.y >= kMinRayTracingBias) {
                            w = hitT > gFalloffRange.x ? hitT < gFalloffRange.y ? (1.f - smoothstep(gFalloffRange.x, gFalloffRange.y, hitT)) : 0.f : 1.f;
                        }
                        occlusion += w;
                    }
                #endif
            }

            gOutput[pixelPos] = float4(1.f - (occlusion /(float)_SHADING_RATE));
            return;
        }
    #endif

    gOutput[pixelPos] = float4(1.f);
}
