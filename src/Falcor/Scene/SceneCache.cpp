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
#include "stdafx.h"
#include "SceneCache.h"
#include "SceneCacheStream.h"
#include "Scene.h"

#include "Falcor/Scene/Material/MaterialTextureLoader.h"

#include "lz4_stream/lz4_stream.h"

namespace Falcor {

namespace {

/** Specfies the current cache file version.
    This needs to be incremented every time the file format changes!
*/
const uint32_t kVersion = 14;

/** Scene cache directory (subdirectory in the application data directory).
*/
const std::string kDirectory = "/tmp/lava_scene_cache";

const size_t kBlockSize = 1 * 1024 * 1024;

const char* kMagic = "FalcorS$";

struct Header {
    uint8_t magic[8]{};
    uint32_t version{};

    bool isValid() const {
        return std::memcmp(magic, kMagic, sizeof(Header::magic)) == 0 && version == kVersion;
    }
};

}

bool SceneCache::hasValidCache(const Key& key) {
    auto cachePath = getCachePath(key);
    if (!fs::exists(cachePath)) return false;

    // Open file.
    std::ifstream fs(cachePath.c_str(), std::ios_base::binary);
    if (fs.bad()) return false;

    // Verify header.
    Header header;
    fs.read(reinterpret_cast<char*>(&header), sizeof(header));
    return !fs.eof() && header.isValid();
}

void SceneCache::writeCache(const Scene::SceneData& sceneData, const Key& key) {
    auto cachePath = getCachePath(key);

    LLOG_INF << "Writing scene cache to " << cachePath.string();

    // Create directories if not existing.
    fs::create_directories(cachePath.parent_path());

    // Open file.
    std::ofstream fs(cachePath.c_str(), std::ios_base::binary);
    if (fs.bad()) throw std::runtime_error("Failed to create scene cache file '" + cachePath.string() + "'!");

    // Write header (uncompressed).
    Header header;
    std::memcpy(header.magic, kMagic, sizeof(Header::magic));
    header.version = kVersion;
    fs.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write cache (compressed).
    lz4_stream::basic_ostream<kBlockSize> zs(fs);
    OutputStream stream(zs);
    writeSceneData(stream, sceneData);
    if (fs.bad()) throw std::runtime_error("Failed to write scene cache file to '" + cachePath.string() + "'!");
}

Scene::SceneData SceneCache::readCache(Device::SharedPtr pDevice, const Key& key) {
    auto cachePath = getCachePath(key);

    LLOG_INF << "Loading scene cache from " << cachePath.string();

    // Open file.
    std::ifstream fs(cachePath.c_str(), std::ios_base::binary);
    if (fs.bad()) throw std::runtime_error("Failed to open scene cache file '" + cachePath.string() + "'!");

    // Read header (uncompressed).
    Header header;
    fs.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!header.isValid()) throw std::runtime_error("Invalid header in scene cache file '" + cachePath.string() + "'!");

    // Read cache (compressed).
    lz4_stream::basic_istream<kBlockSize, kBlockSize> zs(fs);
    InputStream stream(zs);
    auto sceneData = readSceneData(pDevice, stream);
    if (fs.bad()) throw std::runtime_error("Failed to read scene cache file from '" + cachePath.string() + "'!");
    return sceneData;
}

fs::path SceneCache::getCachePath(const Key& key) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(2);
    for (auto c : key) ss << (int)c;
    return fs::path(getAppDataDirectory()) / kDirectory / ss.str();
}

// SceneData

void SceneCache::writeSceneData(OutputStream& stream, const Scene::SceneData& sceneData) {
    writeMarker(stream, "Filename");
    stream.write(sceneData.filename);

    writeMarker(stream, "RenderSettings");
    stream.write(sceneData.renderSettings);

    writeMarker(stream, "Cameras");
    stream.write((uint32_t)sceneData.cameras.size());
    for (const auto& pCamera : sceneData.cameras) writeCamera(stream, pCamera);
    stream.write(sceneData.selectedCamera);
    stream.write(sceneData.cameraSpeed);

    writeMarker(stream, "Lights");
    stream.write((uint32_t)sceneData.lights.size());
    for (const auto& pLight : sceneData.lights) writeLight(stream, pLight);

    writeMarker(stream, "Grids");
    stream.write((uint32_t)sceneData.grids.size());
    for (const auto& pGrid : sceneData.grids) writeGrid(stream, pGrid);

    writeMarker(stream, "GridVolumes");
    stream.write((uint32_t)sceneData.gridVolumes.size());
    for (const auto& pGridVolume : sceneData.gridVolumes) writeGridVolume(stream, pGridVolume, sceneData.grids);

    writeMarker(stream, "EnvMap");
    bool hasEnvMap = sceneData.pEnvMap != nullptr;
    stream.write(hasEnvMap);
    if (hasEnvMap) writeEnvMap(stream, sceneData.pEnvMap);

    writeMarker(stream, "Materials");
    writeMaterials(stream, sceneData.pMaterialSystem);

    writeMarker(stream, "SceneGraph");
    stream.write((uint32_t)sceneData.sceneGraph.size());
    for (const auto& node : sceneData.sceneGraph) {
        stream.write(node.name);
        stream.write(node.parent);
        stream.write(node.transform);
        stream.write(node.meshBind);
        stream.write(node.localToBindSpace);
    }

    writeMarker(stream, "Animations");
    stream.write((uint32_t)sceneData.animations.size());
    for (const auto& pAnimation : sceneData.animations) {
        writeAnimation(stream, pAnimation);
    }

    writeMarker(stream, "Metadata");
    writeMetadata(stream, sceneData.metadata);

    writeMarker(stream, "Meshes");
    stream.write(sceneData.meshDesc);
    stream.write(sceneData.meshNames);
    stream.write(sceneData.meshBBs);
    stream.write(sceneData.meshInstanceData);
    stream.write((uint32_t)sceneData.meshIdToInstanceIds.size());
    for (const auto& item : sceneData.meshIdToInstanceIds)
    {
        stream.write(item);
    }
    stream.write((uint32_t)sceneData.meshGroups.size());
    for (const auto& group : sceneData.meshGroups)
    {
        stream.write(group.meshList);
        stream.write(group.isStatic);
        stream.write(group.isDisplaced);
    }
    stream.write((uint32_t)sceneData.cachedMeshes.size());
    for (const auto& cachedMesh : sceneData.cachedMeshes)
    {
        stream.write(cachedMesh.meshID);
        stream.write(cachedMesh.timeSamples);
        stream.write((uint32_t)cachedMesh.vertexData.size());
        for (const auto& data : cachedMesh.vertexData) stream.write(data);
    }
    stream.write(sceneData.useCompressedHitInfo);
    stream.write(sceneData.has16BitIndices);
    stream.write(sceneData.has32BitIndices);
    stream.write(sceneData.meshDrawCount);
    stream.write(sceneData.meshIndexData);
    stream.write(sceneData.meshStaticData);
    stream.write(sceneData.meshSkinningData);

    writeMarker(stream, "Curves");
    stream.write(sceneData.curveDesc);
    stream.write(sceneData.curveBBs);
    stream.write(sceneData.curveInstanceData);
    stream.write(sceneData.curveIndexData);
    stream.write(sceneData.curveStaticData);

    stream.write((uint32_t)sceneData.cachedCurves.size());
    for (const auto& cachedCurve : sceneData.cachedCurves)
    {
        stream.write(cachedCurve.tessellationMode);
        stream.write(cachedCurve.geometryID);
        stream.write(cachedCurve.timeSamples);
        stream.write(cachedCurve.indexData);
        stream.write((uint32_t)cachedCurve.vertexData.size());
        for (const auto& data : cachedCurve.vertexData) stream.write(data);
    }


    writeMarker(stream, "CustomPrimitives");
    stream.write(sceneData.customPrimitiveDesc);
    stream.write(sceneData.customPrimitiveAABBs);

    writeMarker(stream, "End");
}

Scene::SceneData SceneCache::readSceneData(Device::SharedPtr pDevice, InputStream& stream) {
    Scene::SceneData sceneData;
    sceneData.pMaterialSystem = MaterialSystem::create(pDevice);

    readMarker(stream, "Filename");
    stream.read(sceneData.filename);

    readMarker(stream, "RenderSettings");
    stream.read(sceneData.renderSettings);

    readMarker(stream, "Cameras");
    sceneData.cameras.resize(stream.read<uint32_t>());
    for (auto& pCamera : sceneData.cameras) pCamera = readCamera(stream);
    stream.read(sceneData.selectedCamera);
    stream.read(sceneData.cameraSpeed);

    readMarker(stream, "Lights");
    sceneData.lights.resize(stream.read<uint32_t>());
    for (auto& pLight : sceneData.lights) pLight = readLight(stream);

    readMarker(stream, "Grids");
    sceneData.grids.resize(stream.read<uint32_t>());
    for (auto& pGrid : sceneData.grids) pGrid = readGrid(pDevice, stream);

    readMarker(stream, "GridVolumes");
    sceneData.gridVolumes.resize(stream.read<uint32_t>());
    for (auto& pGridVolume : sceneData.gridVolumes) pGridVolume = readGridVolume(pDevice, stream, sceneData.grids);

    readMarker(stream, "EnvMap");
    auto hasEnvMap = stream.read<bool>();
    if (hasEnvMap) sceneData.pEnvMap = readEnvMap(pDevice, stream);

    // Material textures are loaded asynchronously to allow loading other data
    // in parallel while loading textures from files and uploading them to the GPU.
    // Due to the current implementation, we need to make sure no other GPU operations (transfers)
    // are executed while loading material textures. Due to this, we load volume grids and the envmap
    // before material textures, as they upload buffers to the GPU when created.
    // Make sure no other GPU operations are executed until calling pMaterialTextureLoader.reset()
    // further down which blocks until all textures are loaded.
    auto pMaterialTextureLoader = std::make_unique<MaterialTextureLoader>(pDevice, true);

    readMarker(stream, "Materials");
    readMaterials(pDevice, stream, sceneData.pMaterialSystem, *pMaterialTextureLoader);

    readMarker(stream, "SceneGraph");
    sceneData.sceneGraph.resize(stream.read<uint32_t>());

    for (auto &node : sceneData.sceneGraph) {
        stream.read(node.name);
        stream.read(node.parent);
        stream.read(node.transform);
        stream.read(node.meshBind);
        stream.read(node.localToBindSpace);
    }

    readMarker(stream, "Animations");
    sceneData.animations.resize(stream.read<uint32_t>());
    for (auto& pAnimation : sceneData.animations) pAnimation = readAnimation(stream);

    readMarker(stream, "Metadata");
    sceneData.metadata = readMetadata(stream);

    readMarker(stream, "Meshes");
    stream.read(sceneData.meshDesc);
    stream.read(sceneData.meshNames);
    stream.read(sceneData.meshBBs);
    stream.read(sceneData.meshInstanceData);
    sceneData.meshIdToInstanceIds.resize(stream.read<uint32_t>());
    
    for (auto& item : sceneData.meshIdToInstanceIds) {
        stream.read(item);
    }

    sceneData.meshGroups.resize(stream.read<uint32_t>());
    for (auto& group : sceneData.meshGroups) {
        stream.read(group.meshList);
        stream.read(group.isStatic);
        stream.read(group.isDisplaced);
    }
    sceneData.cachedMeshes.resize(stream.read<uint32_t>());
    for (auto& cachedMesh : sceneData.cachedMeshes) {
        stream.read(cachedMesh.meshID);
        stream.read(cachedMesh.timeSamples);
        cachedMesh.vertexData.resize(stream.read<uint32_t>());
        for (auto& data : cachedMesh.vertexData) stream.read(data);
    }

    stream.read(sceneData.useCompressedHitInfo);
    stream.read(sceneData.has16BitIndices);
    stream.read(sceneData.has32BitIndices);
    stream.read(sceneData.meshDrawCount);
    stream.read(sceneData.meshIndexData);
    stream.read(sceneData.meshStaticData);
    stream.read(sceneData.meshSkinningData);

    readMarker(stream, "Curves");
    stream.read(sceneData.curveDesc);
    stream.read(sceneData.curveBBs);
    stream.read(sceneData.curveInstanceData);
    stream.read(sceneData.curveIndexData);
    stream.read(sceneData.curveStaticData);

    sceneData.cachedCurves.resize(stream.read<uint32_t>());

    for (auto& cachedCurve : sceneData.cachedCurves) {
        stream.read(cachedCurve.tessellationMode);
        stream.read(cachedCurve.geometryID);
        stream.read(cachedCurve.timeSamples);
        stream.read(cachedCurve.indexData);
        cachedCurve.vertexData.resize(stream.read<uint32_t>());
        for (auto& data : cachedCurve.vertexData) stream.read(data);
    }

    readMarker(stream, "CustomPrimitives");
    stream.read(sceneData.customPrimitiveDesc);
    stream.read(sceneData.customPrimitiveAABBs);

    readMarker(stream, "End");

    pMaterialTextureLoader.reset();

    return sceneData;
}

// Metadata

void SceneCache::writeMetadata(OutputStream& stream, const Scene::Metadata& metadata) {
    stream.write(metadata.fNumber);
    stream.write(metadata.filmISO);
    stream.write(metadata.shutterSpeed);
    stream.write(metadata.samplesPerPixel);
    stream.write(metadata.maxDiffuseBounces);
    stream.write(metadata.maxSpecularBounces);
    stream.write(metadata.maxTransmissionBounces);
    stream.write(metadata.maxVolumeBounces);
    std::optional<bool> dummy;
    stream.write(dummy);
}

Scene::Metadata SceneCache::readMetadata(InputStream& stream) {
    Scene::Metadata metadata;
    stream.read(metadata.fNumber);
    stream.read(metadata.filmISO);
    stream.read(metadata.shutterSpeed);
    stream.read(metadata.samplesPerPixel);
    stream.read(metadata.maxDiffuseBounces);
    stream.read(metadata.maxSpecularBounces);
    stream.read(metadata.maxTransmissionBounces);
    stream.read(metadata.maxVolumeBounces);
    std::optional<bool> dummy;
    stream.read(dummy);
    return metadata;
}

// Camera

void SceneCache::writeCamera(OutputStream& stream, const Camera::SharedPtr& pCamera) {
    stream.write(pCamera->mHasAnimation);
    stream.write(pCamera->mIsAnimated);
    stream.write(pCamera->mNodeID);

    stream.write(pCamera->mName);
    stream.write(pCamera->mPreserveHeight);
    stream.write(pCamera->mData);
}

Camera::SharedPtr SceneCache::readCamera(InputStream& stream) {
    auto pCamera = Camera::create();

    stream.read(pCamera->mHasAnimation);
    stream.read(pCamera->mIsAnimated);
    stream.read(pCamera->mNodeID);

    stream.read(pCamera->mName);
    stream.read(pCamera->mPreserveHeight);
    stream.read(pCamera->mData);

    return pCamera;
}

// Light

void SceneCache::writeLight(OutputStream& stream, const Light::SharedPtr& pLight) {
    LightType type = pLight->getType();
    stream.write(type);

    stream.write(pLight->mHasAnimation);
    stream.write(pLight->mIsAnimated);
    stream.write(pLight->mNodeID);

    stream.write(pLight->mName);
    stream.write(pLight->mActive);
    stream.write(pLight->mData);

    switch (type) {
        case LightType::Point:
        case LightType::Directional:
            break;
        case LightType::Distant:
            stream.write(std::static_pointer_cast<DistantLight>(pLight)->mAngle);
            break;
        case LightType::Rect:
        case LightType::Disc:
        case LightType::Sphere:
            stream.write(std::static_pointer_cast<AnalyticAreaLight>(pLight)->mScaling);
            stream.write(std::static_pointer_cast<AnalyticAreaLight>(pLight)->mTransformMatrix);
            break;
    }
}

Light::SharedPtr SceneCache::readLight(InputStream& stream) {
    Light::SharedPtr pLight;
    auto type = stream.read<LightType>();

    switch (type) {
        case LightType::Point:
            pLight = PointLight::create();
            break;
        case LightType::Directional:
            pLight = DirectionalLight::create();
            break;
        case LightType::Distant:
            pLight = DistantLight::create();
            break;
        case LightType::Rect:
            pLight = RectLight::create();
            break;
        case LightType::Disc:
            pLight = DiscLight::create();
            break;
        case LightType::Sphere:
            pLight = SphereLight::create();
            break;
    }

    stream.read(pLight->mHasAnimation);
    stream.read(pLight->mIsAnimated);
    stream.read(pLight->mNodeID);

    stream.read(pLight->mName);
    stream.read(pLight->mActive);
    stream.read(pLight->mData);

    switch (type) {
        case LightType::Point:
        case LightType::Directional:
            break;
        case LightType::Distant:
            stream.read(std::static_pointer_cast<DistantLight>(pLight)->mAngle);
            break;
        case LightType::Rect:
        case LightType::Disc:
        case LightType::Sphere:
            stream.read(std::static_pointer_cast<AnalyticAreaLight>(pLight)->mScaling);
            stream.read(std::static_pointer_cast<AnalyticAreaLight>(pLight)->mTransformMatrix);
            break;
    }

    return pLight;
}

// Material

void SceneCache::writeMaterials(OutputStream& stream, const MaterialSystem::SharedPtr& pMaterials) {
    uint32_t materialCount = pMaterials->getMaterialCount();
    stream.write(materialCount);

    for (uint32_t i = 0; i < materialCount; i++) {
        auto pMaterial = pMaterials->getMaterial(i);
        writeMaterial(stream, pMaterial);
    }
}

void SceneCache::writeMaterial(OutputStream& stream, const Material::SharedPtr& pMaterial) {
    // Write common fields.
    stream.write((uint32_t)pMaterial->getType());
    stream.write(pMaterial->mName);
    stream.write(pMaterial->mHeader);
    stream.write(pMaterial->mUpdates);
    writeTransform(stream, pMaterial->mTextureTransform);

    auto writeTextureSlot = [&stream, &pMaterial](Material::TextureSlot slot) {
        auto pTexture = pMaterial->getTexture(slot);
        bool hasTexture = pTexture != nullptr;
        stream.write(hasTexture);
        if (hasTexture) {
            stream.write(pTexture->getSourcePath());
        }
    };

    for (uint32_t slot = 0; slot < (uint32_t)Material::TextureSlot::Count; ++slot) {
        writeTextureSlot(Material::TextureSlot(slot));
    }

    // Write data in derived class.
    if (auto pBasicMaterial = pMaterial->toBasicMaterial()) writeBasicMaterial(stream, pBasicMaterial);
    else throw std::runtime_error("Unsupported material type");
}

void SceneCache::writeBasicMaterial(OutputStream& stream, const BasicMaterial::SharedPtr& pMaterial) {
    stream.write(pMaterial->mData);
    stream.write(pMaterial->mAlphaRange);
    stream.write(pMaterial->mIsTexturedBaseColorConstant);
    stream.write(pMaterial->mIsTexturedAlphaConstant);
    stream.write(pMaterial->mDisplacementMapChanged);

    writeSampler(stream, pMaterial->mpDefaultSampler);
    writeSampler(stream, pMaterial->mpDisplacementMinSampler);
    writeSampler(stream, pMaterial->mpDisplacementMaxSampler);
}

void SceneCache::readMaterials(Device::SharedPtr pDevice, InputStream& stream, const MaterialSystem::SharedPtr& pMaterials, MaterialTextureLoader& materialTextureLoader) {
    uint32_t materialCount = 0;
    stream.read(materialCount);

    for (uint32_t i = 0; i < materialCount; i++) {
        auto pMaterial = readMaterial(pDevice, stream, materialTextureLoader);
        pMaterials->addMaterial(pMaterial);
    }
}


Material::SharedPtr SceneCache::readMaterial(Device::SharedPtr pDevice, InputStream& stream, MaterialTextureLoader& materialTextureLoader) {
    // Create derived material class of the right type.
    Material::SharedPtr pMaterial; {
        uint32_t type;
        stream.read(type);
        switch ((MaterialType)type) {
            case MaterialType::Standard:
                pMaterial = StandardMaterial::create(pDevice);
                break;
            case MaterialType::Hair:
                pMaterial = HairMaterial::create(pDevice);
                break;
            case MaterialType::Cloth:
                pMaterial = ClothMaterial::create(pDevice);
                break;
            default:
                throw std::runtime_error("Unsupported material type");
        }
    }
    assert(pMaterial);

    // Read common fields.
    stream.read(pMaterial->mName);
    stream.read(pMaterial->mHeader);
    stream.read(pMaterial->mUpdates);
    pMaterial->mTextureTransform = readTransform(stream);

    auto readTextureSlot = [&](Material::TextureSlot slot) {
        auto hasTexture = stream.read<bool>();
        if (hasTexture) {
            auto path = stream.read<fs::path>();
            materialTextureLoader.loadTexture(pMaterial, slot, path);
        }
    };

    for (uint32_t slot = 0; slot < (uint32_t)Material::TextureSlot::Count; ++slot) {
        readTextureSlot(Material::TextureSlot(slot));
    }

    // Read data in derived class.
    if (auto pBasicMaterial = pMaterial->toBasicMaterial()) readBasicMaterial(stream, materialTextureLoader, pBasicMaterial);
    else throw std::runtime_error("Unsupported material type");

    return pMaterial;
}

void SceneCache::readBasicMaterial(InputStream& stream, MaterialTextureLoader& materialTextureLoader, const BasicMaterial::SharedPtr& pMaterial) {
    stream.read(pMaterial->mData);
    stream.read(pMaterial->mAlphaRange);
    stream.read(pMaterial->mIsTexturedBaseColorConstant);
    stream.read(pMaterial->mIsTexturedAlphaConstant);
    stream.read(pMaterial->mDisplacementMapChanged);

    pMaterial->mpDefaultSampler = readSampler(pMaterial->device(), stream);
    pMaterial->mpDisplacementMinSampler = readSampler(pMaterial->device(), stream);
    pMaterial->mpDisplacementMaxSampler = readSampler(pMaterial->device(), stream);
}

void SceneCache::writeSampler(OutputStream& stream, const Sampler::SharedPtr& pSampler) {
    bool valid = pSampler != nullptr;
    stream.write(valid);
    if (valid) {
        stream.write(pSampler->getDesc());
    }
}

Sampler::SharedPtr SceneCache::readSampler(Device::SharedPtr pDevice, InputStream& stream) {
    bool valid = stream.read<bool>();
    if (valid) {
        auto desc = stream.read<Sampler::Desc>();
        return Sampler::create(pDevice, desc);
    }
    return nullptr;
}

// GridVolume

void SceneCache::writeGridVolume(OutputStream& stream, const GridVolume::SharedPtr& pGridVolume, const std::vector<Grid::SharedPtr>& grids) {
    stream.write(pGridVolume->mHasAnimation);
    stream.write(pGridVolume->mIsAnimated);
    stream.write(pGridVolume->mNodeID);

    stream.write(pGridVolume->mName);
    for (const auto& gridSequence : pGridVolume->mGrids) {
        stream.write((uint32_t)gridSequence.size());
        for (const auto& pGrid : gridSequence) {
            uint32_t id = pGrid ? (uint32_t)std::distance(grids.begin(), std::find(grids.begin(), grids.end(), pGrid)) : uint32_t(-1);
            stream.write(id);
        }
    }
    stream.write(pGridVolume->mGridFrame);
    stream.write(pGridVolume->mGridFrameCount);
    stream.write(pGridVolume->mBounds);
    stream.write(pGridVolume->mData);
}

GridVolume::SharedPtr SceneCache::readGridVolume(Device::SharedPtr pDevice, InputStream& stream, const std::vector<Grid::SharedPtr>& grids) {
    GridVolume::SharedPtr pGridVolume = GridVolume::create(pDevice, "");

    stream.read(pGridVolume->mHasAnimation);
    stream.read(pGridVolume->mIsAnimated);
    stream.read(pGridVolume->mNodeID);

    stream.read(pGridVolume->mName);
    for (auto& gridSequence : pGridVolume->mGrids) {
        gridSequence.resize(stream.read<uint32_t>());
        for (auto& pGrid : gridSequence) {
            auto id = stream.read<uint32_t>();
            pGrid = id == uint32_t(-1) ? nullptr : grids[id];
        }
    }
    stream.read(pGridVolume->mGridFrame);
    stream.read(pGridVolume->mGridFrameCount);
    stream.read(pGridVolume->mBounds);
    stream.read(pGridVolume->mData);

    return pGridVolume;
}

// Grid 

void SceneCache::writeGrid(OutputStream& stream, const Grid::SharedPtr& pGrid) {
    const nanovdb::HostBuffer& buffer = pGrid->mGridHandle.buffer();
    stream.write((uint64_t)buffer.size());
    stream.write(buffer.data(), buffer.size());
}

Grid::SharedPtr SceneCache::readGrid(Device::SharedPtr pDevice, InputStream& stream) {
    uint64_t size = stream.read<uint64_t>();
    auto buffer = nanovdb::HostBuffer::create(size);
    stream.read(buffer.data(), buffer.size());
    return Grid::SharedPtr(new Grid(pDevice, nanovdb::GridHandle<nanovdb::HostBuffer>(std::move(buffer))));
}

// EnvMap 

void SceneCache::writeEnvMap(OutputStream& stream, const EnvMap::SharedPtr& pEnvMap) {
    auto filename = pEnvMap->getEnvMap()->getSourceFilename();
    stream.write(filename);
    stream.write(pEnvMap->mData);
    stream.write(pEnvMap->mRotation);
}

EnvMap::SharedPtr SceneCache::readEnvMap(Device::SharedPtr pDevice, InputStream& stream) {
    auto filename = stream.read<std::string>();
    auto pEnvMap = EnvMap::create(pDevice, filename);
    stream.read(pEnvMap->mData);
    stream.read(pEnvMap->mRotation);
    return pEnvMap;
}

// Transform 

void SceneCache::writeTransform(OutputStream& stream, const Transform& transform) {
    stream.write(transform.mTranslation);
    stream.write(transform.mScaling);
    stream.write(transform.mRotation);
}

Transform SceneCache::readTransform(InputStream& stream) {
    Transform transform;
    stream.read(transform.mTranslation);
    stream.read(transform.mScaling);
    stream.read(transform.mRotation);
    return transform;
}

// Animation 

void SceneCache::writeAnimation(OutputStream& stream, const Animation::SharedPtr& pAnimation) {
    stream.write(pAnimation->mName);
    stream.write(pAnimation->mNodeID);
    stream.write(pAnimation->mDuration);
    stream.write(pAnimation->mPreInfinityBehavior);
    stream.write(pAnimation->mPostInfinityBehavior);
    stream.write(pAnimation->mInterpolationMode);
    stream.write(pAnimation->mEnableWarping);
    stream.write(pAnimation->mKeyframes);
}

Animation::SharedPtr SceneCache::readAnimation(InputStream& stream) {
    Animation::SharedPtr pAnimation = Animation::create("", 0, 0.0);
    stream.read(pAnimation->mName);
    stream.read(pAnimation->mNodeID);
    stream.read(pAnimation->mDuration);
    stream.read(pAnimation->mPreInfinityBehavior);
    stream.read(pAnimation->mPostInfinityBehavior);
    stream.read(pAnimation->mInterpolationMode);
    stream.read(pAnimation->mEnableWarping);
    stream.read(pAnimation->mKeyframes);
    return pAnimation;
}

// Marker

void SceneCache::writeMarker(OutputStream& stream, const std::string& id) {
    stream.write(id);
}

void SceneCache::readMarker(InputStream& stream, const std::string& id) {
    auto str = stream.read<std::string>();
    if (id != str) throw std::runtime_error("Found invalid marker in cache!");
}

}  // namespace Falcor