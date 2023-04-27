/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
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
#include "stdafx.h"

#include <fstream>
#include "Falcor/Experimental/Scene/Material/BSDFIntegrator.h"
#include "Falcor/Utils/Image/ImageIO.h"


#include "MERLMaterial.h"

namespace Falcor
{
    namespace
    {
        static_assert((sizeof(MaterialHeader) + sizeof(MERLMaterialData)) <= sizeof(MaterialDataBlob), "MERLMaterialData is too large");

        // Angular sampling resolution of the measured data.
        const size_t kBRDFSamplingResThetaH = 90;
        const size_t kBRDFSamplingResThetaD = 90;
        const size_t kBRDFSamplingResPhiD = 360;

        // Scale factors for the RGB channels of the measured data.
        const double kRedScale = 1.0 / 1500.0;
        const double kGreenScale = 1.15 / 1500.0;
        const double kBlueScale = 1.66 / 1500.0;

        const uint32_t kAlbedoLUTSize = MERLMaterialData::kAlbedoLUTSize;
        const ResourceFormat kAlbedoLUTFormat = ResourceFormat::RGBA32Float;
    }

    MERLMaterial::SharedPtr MERLMaterial::create(Device::SharedPtr pDevice, const std::string& name, const fs::path& path)
    {
        return SharedPtr(new MERLMaterial(pDevice, name, path));
    }

    MERLMaterial::MERLMaterial(Device::SharedPtr pDevice, const std::string& name, const fs::path& path)
        : Material(pDevice, name, MaterialType::MERL)
    {
        if (!loadBRDF(path))
        {
            std::string err = "MERLMaterial() - Failed to load BRDF from " + path.string();
            throw std::runtime_error(err);
        }

        // Create resources for albedo lookup table.
        Sampler::Desc desc;
        desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Point, Sampler::Filter::Point);
        desc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
        desc.setMaxAnisotropy(1);
        mpLUTSampler = Sampler::create(mpDevice, desc);

        prepareAlbedoLUT(mpDevice->getRenderContext());
    }

    Material::UpdateFlags MERLMaterial::update(MaterialSystem* pOwner)
    {
        assert(pOwner);

        auto flags = Material::UpdateFlags::None;
        if (mUpdates != Material::UpdateFlags::None)
        {
            uint32_t bufferID = pOwner->addBuffer(mpBRDFData);
            uint32_t samplerID = pOwner->addTextureSampler(mpLUTSampler);

            if (mData.bufferID != bufferID || mData.samplerID != samplerID)
            {
                mUpdates |= Material::UpdateFlags::DataChanged;
            }
            mData.bufferID = bufferID;
            mData.samplerID = samplerID;

            updateTextureHandle(pOwner, mpAlbedoLUT, mData.texAlbedoLUT);

            flags |= mUpdates;
            mUpdates = Material::UpdateFlags::None;
        }

        return flags;
    }

    bool MERLMaterial::isEqual(const Material::SharedPtr& pOther) const
    {
        auto other = std::dynamic_pointer_cast<MERLMaterial>(pOther);
        if (!other) return false;

        if (!isBaseEqual(*other)) return false;
        if (mPath != other->mPath) return false;

        return true;
    }

    bool MERLMaterial::loadBRDF(const fs::path& path) {
        fs::path fullPath;
        if (!findFileInDataDirectories(path, fullPath)) {
            LLOG_WRN << "MERLMaterial::loadBRDF() - Can't find file " << path.string();
            return false;
        }

        std::ifstream ifs(fullPath.string(), std::ios_base::in | std::ios_base::binary);
        if (!ifs.good()) {
            LLOG_WRN << "MERLMaterial::loadBRDF() - Failed to open file " << path.string();
            return false;
        }

        // Load header.
        int dims[3] = {};
        ifs.read(reinterpret_cast<char*>(dims), sizeof(int) * 3);

        size_t n = (size_t)dims[0] * dims[1] * dims[2];
        if (n != kBRDFSamplingResThetaH * kBRDFSamplingResThetaD * kBRDFSamplingResPhiD / 2) {
            LLOG_WRN << "MERLMaterial::loadBRDF() - Dimensions don't match in file " << path.string();
            return false;
        }

        // Load BRDF data.
        std::vector<double> data(3 * n);
        ifs.read(reinterpret_cast<char*>(data.data()), sizeof(double) * 3 * n);
        if (!ifs.good()) {
            LLOG_WRN << "MERLMaterial::loadBRDF() - Failed to load BRDF data from file " << path.string();
            return false;
        }

        mPath = fullPath;
        mBRDFName = fullPath.stem().string();
        prepareData(dims, data);
        markUpdates(Material::UpdateFlags::ResourcesChanged);

        LLOG_INF << "Loaded MERL BRDF " <<  mBRDFName;

        return true;
    }

    void MERLMaterial::prepareData(const int dims[3], const std::vector<double>& data)
    {
        // Convert BRDF samples to fp32 precision and interleave RGB channels.
        const size_t n = (size_t)dims[0] * dims[1] * dims[2];

        assert(data.size() == 3 * n);
        std::vector<float3> brdf(n);

        size_t negCount = 0;
        size_t infCount = 0;
        size_t nanCount = 0;

        for (size_t i = 0; i < n; i++)
        {
            float3& v = brdf[i];

            // Extract RGB and apply scaling.
            v.x = static_cast<float>(data[i] * kRedScale);
            v.y = static_cast<float>(data[i + n] * kGreenScale);
            v.z = static_cast<float>(data[i + 2 * n] * kBlueScale);

            // Validate data point and set to zero if invalid.
            bool isNeg = v.x < 0.f || v.y < 0.f || v.z < 0.f;
            bool isInf = std::isinf(v.x) || std::isinf(v.y) || std::isinf(v.z);
            bool isNaN = std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z);

            if (isNeg) negCount++;
            if (isInf) infCount++;
            if (isNaN) nanCount++;

            if (isInf || isNaN) v = float3(0.f);
            else if (isNeg) v = max(v, float3(0.f));
        }

        if (negCount > 0) LLOG_WRN << "MERL BRDF " << mBRDFName << " has " << std::to_string(negCount) << " samples with negative values. Clamped to zero.";
        if (infCount > 0) LLOG_WRN << "MERL BRDF " << mBRDFName << " has " << std::to_string(infCount) << " samples with inf values. Sample set to zero.";
        if (nanCount > 0) LLOG_WRN << "MERL BRDF " << mBRDFName << " has " << std::to_string(nanCount) << " samples with NaN values. Sample set to zero.";

        // Create GPU buffer.
        assert(sizeof(brdf[0]) == sizeof(float3));
        mpBRDFData = Buffer::create(mpDevice, brdf.size() * sizeof(brdf[0]), ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, brdf.data());
    }

    void MERLMaterial::prepareAlbedoLUT(RenderContext* pRenderContext)
    {
        const auto texPath = mPath.replace_extension("dds");

        // Try loading albedo lookup table.
        if (fs::is_regular_file(texPath))
        {
            // Load 1D texture in non-SRGB format, no mips.
            // If successful, verify dimensions/format/etc. match the expectations.
            mpAlbedoLUT = Texture::createFromFile(mpDevice, texPath, false, false, ResourceBindFlags::ShaderResource);

            if (mpAlbedoLUT)
            {
                if (mpAlbedoLUT->getFormat() == kAlbedoLUTFormat &&
                    mpAlbedoLUT->getWidth() == kAlbedoLUTSize &&
                    mpAlbedoLUT->getHeight() == 1 && mpAlbedoLUT->getDepth() == 1 &&
                    mpAlbedoLUT->getMipCount() == 1 && mpAlbedoLUT->getArraySize() == 1)
                {
                    LLOG_INF << "Loaded albedo LUT from " <<  texPath.string();
                    return;
                }
            }
        }

        // Failed to load a valid lookup table. We'll recompute it.
        computeAlbedoLUT(pRenderContext);

        assert(mpAlbedoLUT);
#ifdef WIN32
        // ImageIO::saveToDDS(mpDevice->getRenderContext(), texPath, mpAlbedoLUT, ImageIO::CompressionMode::None, false);
#else
        // Cache lookup table in texture on disk.
        // TODO: Capture texture to DDS is not yet supported. Calling ImageIO directly for now.
        //mpAlbedoLUT->captureToFile(0, 0, texPath, Bitmap::FileFormat::DdsFile, Bitmap::ExportFlags::Uncompressed);
#endif

        LLOG_INF << "Saved albedo LUT to " << texPath;
    }

    void MERLMaterial::computeAlbedoLUT(RenderContext* pRenderContext)
    {
        LLOG_INF << "Computing albedo LUT for MERL BRDF " <<  mBRDFName;

        std::vector<float> cosThetas(kAlbedoLUTSize);
        for (uint32_t i = 0; i < kAlbedoLUTSize; i++) cosThetas[i] = (float)(i + 1) / kAlbedoLUTSize;

        // Create copy of material to avoid changing our local state.
        auto pMaterial = SharedPtr(new MERLMaterial(*this));

        // Create and update scene containing the material.
        Scene::SceneData sceneData;
        sceneData.pMaterialSystem = MaterialSystem::create(mpDevice);
        uint materialID = sceneData.pMaterialSystem->addMaterial(pMaterial);

        Scene::SharedPtr pScene = Scene::create(mpDevice, std::move(sceneData));
        pScene->update(pRenderContext, 0.0);

        // Create BSDF integrator utility.
        auto pIntegrator = BSDFIntegrator::create(pRenderContext, pScene);

        // Integreate BSDF.
        auto albedos = pIntegrator->integrateIsotropic(pRenderContext, materialID, cosThetas);

        // Copy result into format needed for texture creation.
        static_assert(kAlbedoLUTFormat == ResourceFormat::RGBA32Float);
        std::vector<float4> initData(kAlbedoLUTSize, float4(0.f));
        for (uint32_t i = 0; i < kAlbedoLUTSize; i++) initData[i] = float4(albedos[i], 1.f);

        // Create albedo LUT texture.
        mpAlbedoLUT = Texture::create2D(mpDevice, kAlbedoLUTSize, 1, kAlbedoLUTFormat, 1, 1, initData.data(), ResourceBindFlags::ShaderResource);
    }

#ifdef SCRIPTING
    SCRIPT_BINDING(MERLMaterial)
    {
        SCRIPT_BINDING_DEPENDENCY(Material)

        pybind11::class_<MERLMaterial, Material, MERLMaterial::SharedPtr> material(m, "MERLMaterial");
        material.def(pybind11::init(&MERLMaterial::create), "name"_a, "path"_a);
    }
#endif
}
