/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
__exported import Experimental.Scene.Materials.IMaterial;
__exported import Experimental.Scene.Materials.StandardMaterialInstance;
import Scene.Material.BasicMaterialData;
import Scene.Material.ShadingUtils;
import Scene.Scene;

/** Implementation of Falcor's standard surface material.

    The material is a linear combination of six BSDF lobes handling
    delta/specular/diffuse reflection and transmission.

    By default, the material is single-sided and the back side black.
    Double-sided materials are supported by mirroring the BSDF around the
    shading normal. This is possible because the material is not layered.
*/
struct StandardMaterial : MaterialBase, IMaterial {
    BasicMaterialData data;

    // Specify that the associated `BSDF` type is `StandardMaterialInstance`.
    typedef StandardMaterialInstance MaterialInstance;

    StandardMaterialInstance setupMaterialInstance(const MaterialSystem ms, const ShadingData sd, const ITextureSampler lod, const uint hints) {
        SamplerState s = ms.getTextureSampler(header.getDefaultTextureSamplerID());

        StandardBSDFData d = {};

        d.eta = sd.frontFacing ? (sd.IoR / data.IoR) : (data.IoR / sd.IoR);
        d.diffuseTransmission = data.diffuseTransmission;
        d.specularTransmission = data.specularTransmission;
        if (d.diffuseTransmission > 0.f || d.specularTransmission > 0.f) {
            d.transmission = ms.sampleTexture(data.texTransmission, s, sd.uv, float4(1.f), lod).rgb * data.transmission;
        }

        // Calculate the specular reflectance for dielectrics from the IoR, as in the Disney BSDF [Burley 2015].
        // UE4 uses 0.08 multiplied by a default specular value of 0.5, hence F0=0.04 as default. The default IoR=1.5 gives the same result.
        float f = (data.IoR - 1.f) / (data.IoR + 1.f);
        float F0 = f * f;

        // Sample base color.
        const float3 baseColor = ms.sampleTexture(data.texBaseColor, s, sd.uv, float4(1.f), lod).rgb * data.baseColor.rgb;
        
#if MATERIAL_SYSTEM_HAS_SPEC_GLOSS_MATERIALS
        if (data.getShadingModel() == ShadingModel::MetalRough)
#endif
        {
            float metallic = ms.sampleTexture(data.texMetallic, s, sd.uv, float4(1.f), lod).r * data.metallic;

            d.diffuse = lerp(baseColor, float3(0), metallic);
            d.specular = lerp(float3(F0), baseColor, metallic);
            d.roughness = max(0.0f, ms.sampleTexture(data.texRoughness, s, sd.uv, float4(1.f), lod).r) * data.roughness;
            d.metallic = metallic;
        }
#if MATERIAL_SYSTEM_HAS_SPEC_GLOSS_MATERIALS
        else // ShadingModel::SpecGloss
        {
            float metallic = max(0.0f, ms.sampleTexture(data.texMetallic, s, sd.uv, float4(1.f), lod).r) * data.metallic;

            const float reflecttint = 0.0f;

            d.diffuse = baseColor;
            d.specular = lerp(float3(1), baseColor, reflecttint);
            d.roughness = 1 - max(0.0f, ms.sampleTexture(data.texRoughness, s, sd.uv, float4(1.f), lod).r) * data.roughness;
            d.metallic = metallic;
        }
#endif
        if (Scene::kDiffuseAlbedoMultiplier != 1.f) {
            d.diffuse = saturate(d.diffuse * Scene::kDiffuseAlbedoMultiplier);
        }

        // Compute final shading frame.
        ShadingFrame sf = sd.frame;
        if (isNormalMappingEnabled(hints)) {
            float3 encodedNormal = {0.f, 0.f, 1.f};
            const float  factor = data.bumpNormalFactor;
            
            [[ branch ]]
            switch(data.getNormalMapMode()) {
                case NormalMapMode::Bump:
                    float4 ddxy = .5f * ms.ddxy(sd.uv, lod);

                    float uH = ms.sampleTexture(data.texNormalMap, s, sd.uv + ddxy.xy, float4(0.f), lod).r;
                    float bH = ms.sampleTexture(data.texNormalMap, s, sd.uv - ddxy.xy, float4(0.f), lod).r;
                    float lH = ms.sampleTexture(data.texNormalMap, s, sd.uv + ddxy.zw, float4(0.f), lod).r;
                    float rH = ms.sampleTexture(data.texNormalMap, s, sd.uv - ddxy.zw, float4(0.f), lod).r;

                    float dy = (rH - lH) * factor;
                    float dx = (bH - uH) * factor;
                    float dz = 1.0f; //sqrt (1.0 - dx*dx - dy*dy);
                    encodedNormal = normalize(float3(dx, dy, dz));
                    sf = computeShadingFrameFromBumpMap(sd, encodedNormal);
                    break;
                case NormalMapMode::Normal:
                    encodedNormal = ms.sampleTexture(data.texNormalMap, s, sd.uv, float4(0.f), lod).rgb;
                    encodedNormal.x = data.isNormalMapXFlipped() ? (1.f - encodedNormal.x) : encodedNormal.x;
                    encodedNormal.y = data.isNormalMapYFlipped() ? (1.f - encodedNormal.y) : encodedNormal.y;

                    //encodedNormal = {0.f, 0.f, 1.0f};
                    //sf = ShadingFrame(sd.frame.fromLocal(normalize(encodedNormal)), sd.tangentW);
                    
                    sf = computeShadingFrameFromNormalMap(sd, data.getNormalMapType(), encodedNormal);
                    break;
                default:
                    break;
            }
        }
        
        flipShadingNormal(sd, sf);

        if (isAdjustShadingNormalEnabled(hints)) {
            adjustShadingNormal(sd, sf);
        }

        // Sample the emissive texture.
        // The standard material supports uniform emission over the hemisphere.
        // Note that the material is only emissive on the front-facing side.
        float3 emission = {};
        if (sd.frontFacing) {
            emission = ms.sampleTexture(data.texEmissive, s, sd.uv, float4(data.emissive, 1), lod).rgb * data.emissiveFactor;
        }

        return StandardMaterialInstance(sf, d, emission);
    }

    float evalOpacity(const MaterialSystem ms, const VertexData v, const ITextureSampler lod) {
        // TODO: handle dedicated opacity texture;
        return 1.0f;
        //SamplerState s = ms.getTextureSampler(header.getDefaultTextureSamplerID());
        //return ms.sampleTexture(data.texBaseColor, s, v.texC, data.baseColor, lod).r;
    }
};
