/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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

#include <atomic>
#include <sstream>
#include <numeric>

#include "Scene.h"
#include "SceneDefines.slangh"
#include "Scene/Curves/CurveConfig.h"

#include "HitInfo.h"

//#include "Falcor/Core/API/Vulkan/VKDevice.h"

#include "Falcor/Core/API/IndirectCommands.h"
#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Core/Program/RtProgram.h"
#include "Falcor/Core/Program/ProgramVars.h"

#include "Falcor/Scene/Lights/LightLinker.h"

#include "Falcor/Utils/StringUtils.h"
#include "Falcor/Utils/Math/MathHelpers.h"
#include "Falcor/Utils/Timing/Profiler.h"
#include "Falcor/Utils/Debug/debug.h"
#include "SceneBuilder.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

//#include "nvvk/buffers_vk.hpp"

static std::atomic<uint32_t> _cnt = 0;

namespace Falcor {

static_assert(sizeof(MeshDesc) % 16 == 0, "MeshDesc size should be a multiple of 16");
static_assert(sizeof(GeometryInstanceData) % 16 == 0, "GeometryInstanceData size should be a multiple of 16");
static_assert(sizeof(PackedStaticVertexData) % 16 == 0, "PackedStaticVertexData size should be a multiple of 16");
static_assert(sizeof(PackedMeshletData) % 16 == 0, "Meshlet size should be a multiple of 16");

namespace {
    // Large scenes are split into multiple BLAS groups in order to reduce build memory usage.
    // The target is max 0.5GB intermediate memory per BLAS group. Note that this is not a strict limit.
    //const size_t kMaxBLASBuildMemory = 1ull << 29; // 512mb
    //const size_t kMaxBLASBuildMemory = 1ull << 28; // 256mb
    const size_t kMaxBLASBuildMemory = 1ull << 27; // 128mb

    const std::string kParameterBlockName = "gScene";
    const std::string kGeometryInstanceBufferName = "geometryInstances";
    const std::string kMeshBufferName = "meshes";
    const std::string kMeshletGroupsBufferName = "meshletGroups";
    const std::string kMeshletIndicesBufferName = "meshletLocalIndexData";
    const std::string kMeshletVerticesBufferName = "meshletVertices";
    const std::string kMeshletPrimIndicesBufferName = "meshletPrimIndices";
    const std::string kMeshletsBufferName = "meshlets";
    const std::string kIndexBufferName = "indexData";
    const std::string kVertexBufferName = "vertices";
    const std::string kPrevVertexBufferName = "prevVertices";
    const std::string kProceduralPrimAABBBufferName = "proceduralPrimitiveAABBs";
    const std::string kCurveBufferName = "curves";
    const std::string kCurveIndexBufferName = "curveIndices";
    const std::string kCurveVertexBufferName = "curveVertices";
    const std::string kPrevCurveVertexBufferName = "prevCurveVertices";
    const std::string kSDFGridsArrayName = "sdfGrids";
    const std::string kCustomPrimitiveBufferName = "customPrimitives";
    const std::string kMaterialsBlockName = "materials";
    const std::string kPerPrimMaterialIDsBufferName = "perPrimMaterialIDs";
    const std::string kLightsBufferName = "lights";
    const std::string kGridVolumesBufferName = "gridVolumes";

    const std::string kStats = "stats";
    const std::string kBounds = "bounds";
    const std::string kAnimations = "animations";
    const std::string kLoopAnimations = "loopAnimations";
    const std::string kCamera = "camera";
    const std::string kCameras = "cameras";
    const std::string kCameraSpeed = "cameraSpeed";
    const std::string kLights = "lights";
    const std::string kLightProfile = "lightProfile";
    const std::string kLightLinker = "lightLinker";
    const std::string kAnimated = "animated";
    const std::string kRenderSettings = "renderSettings";
    const std::string kUpdateCallback = "updateCallback";
    const std::string kEnvMap = "envMap";
    const std::string kMaterials = "materials";
    const std::string kGridVolumes = "gridVolumes";
    const std::string kGetLight = "getLight";
    const std::string kGetMaterial = "getMaterial";
    const std::string kGetGridVolume = "getGridVolume";
    const std::string kSetEnvMap = "setEnvMap";
    const std::string kAddViewpoint = "addViewpoint";
    const std::string kRemoveViewpoint = "kRemoveViewpoint";
    const std::string kSelectViewpoint = "selectViewpoint";

    // Checks if the transform flips the coordinate system handedness (its determinant is negative).
    inline bool doesTransformFlip(const glm::mat4& m) {
        return glm::determinant((glm::mat3)m) < 0.f;
    }
}

static inline VkTransformMatrixKHR toTransformMatrixKHR(const glm::mat4& m) {
    VkTransformMatrixKHR out_matrix;
    
    auto temp = glm::transpose(m);
    memcpy(&out_matrix, glm::value_ptr(temp), sizeof(VkTransformMatrixKHR));
    return out_matrix;
}

Scene::Scene(std::shared_ptr<Device> pDevice, SceneData&& sceneData): mpDevice(pDevice) {
    mRayTraceInitialized = false;
    //mDebug.setup(mpDevice->getApiHandle()); 

    // Copy/move scene data to member variables.
    mFilename = sceneData.filename;
    mRenderSettings = sceneData.renderSettings;
    mCameras = std::move(sceneData.cameras);
    mSelectedCamera = sceneData.selectedCamera;
    mCameraSpeed = sceneData.cameraSpeed;
    
    mLights = std::move(sceneData.lights);
    mpLightLinker = std::move(sceneData.pLightLinker);

    mpMaterialSystem = std::move(sceneData.pMaterialSystem);
    mMaterialXs = std::move(sceneData.materialxs);
    mGridVolumes = std::move(sceneData.gridVolumes);
    mGrids = std::move(sceneData.grids);
    mpEnvMap = sceneData.pEnvMap;
    mpLightProfile = sceneData.pLightProfile;
    mSceneGraph = std::move(sceneData.sceneGraph);
    mMetadata = std::move(sceneData.metadata);

    // Merge all geometry instance lists into one.
    assert(sceneData.meshInstanceData.size() == sceneData.meshInstanceNamesData.size());
    assert(sceneData.curveInstanceData.size() == sceneData.curveInstanceNamesData.size());
    assert(sceneData.sdfGridInstancesData.size() == sceneData.sdfGridInstanceNamesData.size());
    mGeometryInstanceData.reserve(sceneData.meshInstanceData.size() + sceneData.curveInstanceData.size() + sceneData.sdfGridInstancesData.size());
    mGeometryInstanceData.insert(std::end(mGeometryInstanceData), std::begin(sceneData.meshInstanceData), std::end(sceneData.meshInstanceData));
    mGeometryInstanceData.insert(std::end(mGeometryInstanceData), std::begin(sceneData.curveInstanceData), std::end(sceneData.curveInstanceData));
    mGeometryInstanceData.insert(std::end(mGeometryInstanceData), std::begin(sceneData.sdfGridInstancesData), std::end(sceneData.sdfGridInstancesData));

    mGeometryInstanceNamesData.reserve(sceneData.meshInstanceNamesData.size() + sceneData.curveInstanceNamesData.size() + sceneData.sdfGridInstanceNamesData.size());
    mGeometryInstanceNamesData.insert(std::end(mGeometryInstanceNamesData), std::begin(sceneData.meshInstanceNamesData), std::end(sceneData.meshInstanceNamesData));
    mGeometryInstanceNamesData.insert(std::end(mGeometryInstanceNamesData), std::begin(sceneData.curveInstanceNamesData), std::end(sceneData.curveInstanceNamesData));
    mGeometryInstanceNamesData.insert(std::end(mGeometryInstanceNamesData), std::begin(sceneData.sdfGridInstanceNamesData), std::end(sceneData.sdfGridInstanceNamesData));

    {
        uint32_t internalID = 0;
        for(auto& instance: mGeometryInstanceData) instance.internalID = internalID++;
    }

    mMeshDesc = std::move(sceneData.meshDesc);
    mMeshNames = std::move(sceneData.meshNames);
    mMeshBBs = std::move(sceneData.meshBBs);
    mMeshIdToInstanceIds = std::move(sceneData.meshIdToInstanceIds);
    mMeshGroups = std::move(sceneData.meshGroups);

    mPerPrimMaterialIDs = std::move(sceneData.perPrimitiveMaterialIDsData);

    mUseCompressedHitInfo = sceneData.useCompressedHitInfo;
    mHas16BitIndices = sceneData.has16BitIndices;
    mHas32BitIndices = sceneData.has32BitIndices;

    mMeshletGroups = std::move(sceneData.meshletGroups);
    mMeshletsData = std::move(sceneData.meshletsData);
    mMeshletVertices = std::move(sceneData.meshletVertices);
    mMeshletIndices = std::move(sceneData.meshletIndices);
    mMeshletPrimIndices = std::move(sceneData.meshletPrimIndices);

    mCurveDesc = std::move(sceneData.curveDesc);
    mCurveBBs = std::move(sceneData.curveBBs);
    mCurveIndexData = std::move(sceneData.curveIndexData);
    mCurveStaticData = std::move(sceneData.curveStaticData);

    mCustomPrimitiveDesc = std::move(sceneData.customPrimitiveDesc);
    mCustomPrimitiveAABBs = std::move(sceneData.customPrimitiveAABBs);

    // Setup additional resources.
    mFrontClockwiseRS[RasterizerState::CullMode::None] = RasterizerState::create(RasterizerState::Desc().setFrontCounterCW(false).setCullMode(RasterizerState::CullMode::None));
    mFrontClockwiseRS[RasterizerState::CullMode::Back] = RasterizerState::create(RasterizerState::Desc().setFrontCounterCW(false).setCullMode(RasterizerState::CullMode::Back));
    mFrontClockwiseRS[RasterizerState::CullMode::Front] = RasterizerState::create(RasterizerState::Desc().setFrontCounterCW(false).setCullMode(RasterizerState::CullMode::Front));
    mFrontCounterClockwiseRS[RasterizerState::CullMode::None] = RasterizerState::create(RasterizerState::Desc().setFrontCounterCW(true).setCullMode(RasterizerState::CullMode::None));
    mFrontCounterClockwiseRS[RasterizerState::CullMode::Back] = RasterizerState::create(RasterizerState::Desc().setFrontCounterCW(true).setCullMode(RasterizerState::CullMode::Back));
    mFrontCounterClockwiseRS[RasterizerState::CullMode::Front] = RasterizerState::create(RasterizerState::Desc().setFrontCounterCW(true).setCullMode(RasterizerState::CullMode::Front));

    // Setup volume grid -> id map.
    for (size_t i = 0; i < mGrids.size(); ++i) mGridIDs.emplace(mGrids[i], (uint32_t)i);

    // Set default SDF grid config.
    setSDFGridConfig();

    // Create vertex array objects for meshes and curves.
    createMeshVao(sceneData.meshDrawCount, sceneData.meshIndexData, sceneData.meshStaticData, sceneData.meshSkinningData);
    createCurveVao(mCurveIndexData, mCurveStaticData);

    // Create animation controller.
    mpAnimationController = AnimationController::create(this, sceneData.meshStaticData, sceneData.meshSkinningData, sceneData.prevVertexCount, sceneData.animations);

    // Some runtime mesh data validation. These are essentially asserts, but large scenes are mostly opened in Release
    for (const auto& mesh : mMeshDesc) {
        if (mesh.isDynamic()) {
            if (mesh.prevVbOffset + mesh.vertexCount > sceneData.prevVertexCount) throw std::runtime_error("Cached Mesh Animation: Invalid prevVbOffset");
        }
    }

    for (const auto &mesh : sceneData.cachedMeshes) {
        if (!mMeshDesc[mesh.meshID].isAnimated()) throw std::runtime_error("Cached Mesh Animation: Referenced mesh ID is not dynamic");
        if (mesh.timeSamples.size() != mesh.vertexData.size()) throw std::runtime_error("Cached Mesh Animation: Time sample count mismatch.");
        for (const auto &vertices : mesh.vertexData)
        {
            if (vertices.size() != mMeshDesc[mesh.meshID].vertexCount) throw std::runtime_error("Cached Mesh Animation: Vertex count mismatch.");
        }
    }
    
    for (const auto& cache : sceneData.cachedCurves) {
        if (cache.tessellationMode != CurveTessellationMode::LinearSweptSphere)
        {
            if (!mMeshDesc[cache.geometryID].isAnimated()) throw std::runtime_error("Cached Curve Animation: Referenced mesh ID is not dynamic");
        }
    }

    // Must be placed after curve data/AABB creation.
    mpAnimationController->addAnimatedVertexCaches(std::move(sceneData.cachedCurves), std::move(sceneData.cachedMeshes), sceneData.meshStaticData);


    // Finalize scene.
    finalize();

    // Init ray tracing. 
    // TODO: Init only if needed...
    initRayTracing();

    LLOG_WRN << "Scenes count " << (uint32_t)(++_cnt);
}

Scene::~Scene() {
    mpDevice->getRenderContext()->flush(true);

    //for(auto & pBlas: mBlasObjects) {
    //    mpDevice->getApiHandle()->destroyAccelerationStructure(pBlas->getApiHandle());
    //    pBlas.reset();
    //}

    _cnt--;
    printMeshletsStats();
    LLOG_WRN << "Scene destroyed!";
}

Scene::SharedPtr Scene::create(std::shared_ptr<Device> pDevice, const std::string& filename) {
    assert(pDevice);
    auto pBuilder = SceneBuilder::create(pDevice, filename);
    return pBuilder ? pBuilder->getScene() : nullptr;
}

Scene::SharedPtr Scene::create(std::shared_ptr<Device> pDevice, SceneData&& sceneData) {
    assert(pDevice);
    return Scene::SharedPtr(new Scene(pDevice, std::move(sceneData)));
}

Shader::DefineList Scene::getDefaultSceneDefines() {
    Shader::DefineList defines;
    defines.add("SCENE_RAYTRACING_ENABLED", "0");

    defines.add("SCENE_GEOMETRY_TYPES", "0");
    defines.add("SCENE_GRID_COUNT", "0");
    defines.add("SCENE_SDF_GRID_COUNT", "0");
    defines.add("SCENE_ENVMAP_SAMPLERS_COUNT", "0");
    defines.add("SCENE_HAS_INDEXED_VERTICES", "0");
    defines.add("SCENE_HAS_16BIT_INDICES", "0");
    defines.add("SCENE_HAS_32BIT_INDICES", "0");
    defines.add("SCENE_USE_LIGHT_PROFILE", "0");
    defines.add("SCENE_HAS_PERPRIM_MATERIALS", "0");
    defines.add("SCENE_HAS_LIGHT_LINKER", "0");

    defines.add("SCENE_DIFFUSE_ALBEDO_MULTIPLIER", "1.f");

    defines.add(LightLinker::getDefaultDefines());
    defines.add(MaterialSystem::getDefaultDefines());

    return defines;
}

Shader::DefineList Scene::getSceneDefines() const {
    Shader::DefineList defines;
    defines.add("SCENE_RAYTRACING_ENABLED", mRenderSettings.useRayTracing ? "1" : "0");

    defines.add("SCENE_GEOMETRY_TYPES", std::to_string((uint32_t)mGeometryTypes));
    defines.add("SCENE_GRID_COUNT", std::to_string(mGrids.size()));
    defines.add("SCENE_HAS_INDEXED_VERTICES", hasIndexBuffer() ? "1" : "0");
    defines.add("SCENE_HAS_16BIT_INDICES", mHas16BitIndices ? "1" : "0");
    defines.add("SCENE_HAS_32BIT_INDICES", mHas32BitIndices ? "1" : "0");
    defines.add("SCENE_USE_LIGHT_PROFILE", mpLightProfile != nullptr ? "1" : "0");
    defines.add("SCENE_HAS_PERPRIM_MATERIALS", !mPerPrimMaterialIDs.empty() ? "1" : "0");
    defines.add("SCENE_DIFFUSE_ALBEDO_MULTIPLIER", std::to_string(mRenderSettings.diffuseAlbedoMultiplier));

    defines.add(mHitInfo.getDefines());
    defines.add(mpLightLinker ? mpLightLinker->getDefines() : LightLinker::getDefaultDefines());
    defines.add(mpMaterialSystem->getDefines());

    defines.add(getSceneLightSamplersDefines());
    defines.add(getSceneSDFGridDefines());

    return defines;
}

Shader::DefineList Scene::getSceneLightSamplersDefines() const {
    Shader::DefineList defines;

    size_t envmapLightsCount = 0;
    size_t phySkyLightsCount = 0;
    for(const auto& light: mLights) {
        switch(light->getType()) {
            case LightType::Env:
                envmapLightsCount++;
                break;
            case LightType::PhysSunSky:
                phySkyLightsCount++;
                break;
            default:
                break;
        }
    }
    defines.add("SCENE_ENVMAP_SAMPLERS_COUNT", std::to_string(envmapLightsCount));
    defines.add("SCENE_PHYSKY_SAMPLERS_COUNT", std::to_string(phySkyLightsCount));

    return defines;
}

Shader::DefineList Scene::getSceneSDFGridDefines() const {
    Shader::DefineList defines;
    defines.add("SCENE_SDF_GRID_COUNT", std::to_string(mSDFGrids.size()));
    defines.add("SCENE_SDF_GRID_MAX_LOD_COUNT", std::to_string(mSDFGridMaxLODCount));

    defines.add("SCENE_SDF_GRID_IMPLEMENTATION_NDSDF", std::to_string((uint32_t)SDFGrid::Type::NormalizedDenseGrid));
    defines.add("SCENE_SDF_GRID_IMPLEMENTATION_SVS", std::to_string((uint32_t)SDFGrid::Type::SparseVoxelSet));
    defines.add("SCENE_SDF_GRID_IMPLEMENTATION_SBS", std::to_string((uint32_t)SDFGrid::Type::SparseBrickSet));
    defines.add("SCENE_SDF_GRID_IMPLEMENTATION_SVO", std::to_string((uint32_t)SDFGrid::Type::SparseVoxelOctree));

    defines.add("SCENE_SDF_NO_INTERSECTION_METHOD", std::to_string((uint32_t)SDFGridIntersectionMethod::None));
    defines.add("SCENE_SDF_NO_VOXEL_SOLVER", std::to_string((uint32_t)SDFGridIntersectionMethod::GridSphereTracing));
    defines.add("SCENE_SDF_VOXEL_SPHERE_TRACING", std::to_string((uint32_t)SDFGridIntersectionMethod::VoxelSphereTracing));

    defines.add("SCENE_SDF_NO_GRADIENT_EVALUATION_METHOD", std::to_string((uint32_t)SDFGridGradientEvaluationMethod::None));
    defines.add("SCENE_SDF_GRADIENT_NUMERIC_DISCONTINUOUS", std::to_string((uint32_t)SDFGridGradientEvaluationMethod::NumericDiscontinuous));
    defines.add("SCENE_SDF_GRADIENT_NUMERIC_CONTINUOUS", std::to_string((uint32_t)SDFGridGradientEvaluationMethod::NumericContinuous));

    defines.add("SCENE_SDF_GRID_IMPLEMENTATION", std::to_string((uint32_t)mSDFGridConfig.implementation));
    defines.add("SCENE_SDF_VOXEL_INTERSECTION_METHOD", std::to_string((uint32_t)mSDFGridConfig.intersectionMethod));
    defines.add("SCENE_SDF_GRADIENT_EVALUATION_METHOD", std::to_string((uint32_t)mSDFGridConfig.gradientEvaluationMethod));
    defines.add("SCENE_SDF_SOLVER_MAX_ITERATION_COUNT", std::to_string(mSDFGridConfig.solverMaxIterations));
    defines.add("SCENE_SDF_OPTIMIZE_VISIBILITY_RAYS", mSDFGridConfig.optimizeVisibilityRays ? "1" : "0");

    return defines;
}

Program::TypeConformanceList Scene::getTypeConformances() const {
        return mpMaterialSystem->getTypeConformances();
}

const LightCollection::SharedPtr& Scene::getLightCollection(RenderContext* pContext) {
    if (!mpLightCollection) {
        mpLightCollection = LightCollection::create(pContext, shared_from_this());
        mpLightCollection->setShaderData(mpSceneBlock["lightCollection"]);

        mSceneStats.emissiveMemoryInBytes = mpLightCollection->getMemoryUsageInBytes();
    }
    return mpLightCollection;
}

void Scene::rasterize(RenderContext* pContext, GraphicsState* pState, GraphicsVars* pVars, RasterizerState::CullMode cullMode) {
    rasterize(pContext, pState, pVars, mFrontClockwiseRS[cullMode], mFrontCounterClockwiseRS[cullMode]);
}

void Scene::rasterizeX(RenderContext* pContext, GraphicsState* pState, GraphicsVars* pVars, RasterizerState::CullMode cullMode) {
    rasterizeX(pContext, pState, pVars, mFrontClockwiseRS[cullMode], mFrontCounterClockwiseRS[cullMode]);
}


void Scene::rasterize(RenderContext* pContext, GraphicsState* pState, GraphicsVars* pVars, const RasterizerState::SharedPtr& pRasterizerStateCW, const RasterizerState::SharedPtr& pRasterizerStateCCW) {
    PROFILE(mpDevice, "rasterizeScene");

    auto start = std::chrono::high_resolution_clock::now();

    // On first execution or if BLASes need to be rebuilt, create BLASes for all geometries.
    if (!mBlasDataValid) {
        initGeomDesc(pContext);
        buildBlas(pContext);
    }

    // On first execution, when meshes have moved, when there's a new ray type count, or when a BLAS has changed, create/update the TLAS
    //
    // The raytracing shader table has one hit record per ray type and geometry. We need to know the ray type count in order to setup the indexing properly.
    // Note that for DXR 1.1 ray queries, the shader table is not used and the ray type count doesn't matter and can be set to zero.
    //
    int rayTypeCount = 0;
    auto tlasIt = mTlasCache.find(rayTypeCount);
    if (tlasIt == mTlasCache.end() || !tlasIt->second.pTlasObject) {
        // We need a hit entry per mesh right now to pass GeometryIndex()
        buildTlas(pContext, rayTypeCount, true);

        // If new TLAS was just created, get it so the iterator is valid
        if (tlasIt == mTlasCache.end()) tlasIt = mTlasCache.find(rayTypeCount);
    }
    assert(mpSceneBlock);

    // Bind TLAS.
    assert(tlasIt != mTlasCache.end() && tlasIt->second.pTlasObject);
    mpSceneBlock["rtAccel"].setAccelerationStructure(tlasIt->second.pTlasObject);

    pVars->setParameterBlock("gScene", mpSceneBlock);

    auto pCurrentRS = pState->getRasterizerState();
    bool isIndexed = hasIndexBuffer();

    LLOG_TRC << "mDrawArgs size " << std::to_string(mDrawArgs.size());
    for (const auto& draw : mDrawArgs) {
        //auto loop_start = std::chrono::high_resolution_clock::now();

        pState->setVao(draw.ibFormat == ResourceFormat::R16Uint ? mpMeshVao16Bit : mpMeshVao);
        
        if (draw.ccw) {
            if(draw.cullBackface) pState->setRasterizerState(mFrontCounterClockwiseRS[RasterizerState::CullMode::Back]);
            else pState->setRasterizerState(pRasterizerStateCCW);
        } else {
            if(draw.cullBackface) pState->setRasterizerState(mFrontClockwiseRS[RasterizerState::CullMode::Front]);
            else pState->setRasterizerState(pRasterizerStateCW);
        }
        
        // Draw the primitives.
        if (isIndexed) pContext->drawIndexedIndirect(pState, pVars, draw.count, draw.pBuffer.get(), 0);
        else pContext->drawIndirect(pState, pVars, draw.count, draw.pBuffer.get(), 0, nullptr, 0);
    
        //auto loop_stop = std::chrono::high_resolution_clock::now();
        //LLOG_TRC << "Scene::rasterize() loop time " << std::chrono::duration_cast<std::chrono::milliseconds>(loop_stop - loop_start).count() << " ms.";
    }

    pState->setRasterizerState(pCurrentRS);

    auto stop = std::chrono::high_resolution_clock::now();
    LLOG_TRC << "Scene::rasterize() time " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " ms.";
}

void Scene::rasterizeX(RenderContext* pContext, GraphicsState* pState, GraphicsVars* pVars, const RasterizerState::SharedPtr& pRasterizerStateCW, const RasterizerState::SharedPtr& pRasterizerStateCCW) {
    PROFILE(mpDevice, "rasterizeXScene");

    pVars->setParameterBlock("gScene", mpSceneBlock);

    auto pCurrentRS = pState->getRasterizerState();
    bool isIndexed = hasIndexBuffer();

    for (size_t materialID = 0; materialID < mMaterialDrawArgs.size(); materialID++) {

        auto pMaterial = mpMaterialSystem->getMaterial(materialID);
        
        for (size_t draw_id : mMaterialDrawArgs[materialID]) {
            size_t i = 0;
            
            const auto& draw = mDrawArgs[draw_id];

            if (draw.count > 0) {

                // Set state.
                pState->setVao(draw.ibFormat == ResourceFormat::R16Uint ? mpMeshVao16Bit : mpMeshVao);

                if (draw.ccw) pState->setRasterizerState(pRasterizerStateCCW);
                else pState->setRasterizerState(pRasterizerStateCW);

                // Draw the primitives.
                if (isIndexed) {
                    //pContext->drawIndexedIndirectCount(pState, pVars, draw.count, draw.pBuffer.get(), 0, draw.pCountBuffer.get(), 0);
                    pContext->drawIndexedIndirect(pState, pVars, draw.count, draw.pBuffer.get(), 0);
                } else {
                    pContext->drawIndirect(pState, pVars, draw.count, draw.pBuffer.get(), 0, nullptr, 0);
                }
                pContext->flush();
                i+=1;
            }
        }
    }

    pState->setRasterizerState(pCurrentRS);
}

uint32_t Scene::getRaytracingMaxAttributeSize() const {
    bool hasDisplacedMesh = hasGeometryType(Scene::GeometryType::DisplacedTriangleMesh);
    return hasDisplacedMesh ? 12 : 8;
}

void Scene::raytrace(RenderContext* pContext, RtProgram* pProgram, const std::shared_ptr<RtProgramVars>& pVars, uint3 dispatchDims) {
    PROFILE(mpDevice, "raytraceScene");

    assert(pContext && pProgram && pVars);
    if (pVars->getRayTypeCount() > 0 && pVars->getGeometryCount() != getGeometryCount()) {
        throw std::runtime_error("RtProgramVars geometry count mismatch");
    }

    uint32_t rayTypeCount = pVars->getRayTypeCount();
    setRaytracingShaderData(pContext, pVars->getRootVar(), rayTypeCount);

    // Set ray type constant.
    pVars->getRootVar()["DxrPerFrame"]["rayTypeCount"] = rayTypeCount;

    pContext->raytrace(pProgram, pVars.get(), dispatchDims.x, dispatchDims.y, dispatchDims.z);
}

void Scene::createMeshVao(uint32_t drawCount, const std::vector<uint32_t>& indexData, const std::vector<PackedStaticVertexData>& staticData, const std::vector<SkinningVertexData>& skinningData)
{
    if (drawCount == 0) return;

    // Create the index buffer.
    size_t ibSize = sizeof(uint32_t) * indexData.size();
    if (ibSize > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Index buffer size exceeds 4GB");
    }

    Buffer::SharedPtr pIB = nullptr;
    if (ibSize > 0) {
        ResourceBindFlags ibBindFlags = Resource::BindFlags::Index | ResourceBindFlags::ShaderResource;
        pIB = Buffer::create(mpDevice, ibSize, ibBindFlags, Buffer::CpuAccess::None, indexData.data());
        LLOG_TRC << "pIB buffer size " << pIB->getSize();
    }

    // Create the vertex data structured buffer.
    const size_t vertexCount = (uint32_t)staticData.size();
    size_t staticVbSize = sizeof(PackedStaticVertexData) * vertexCount;
    if (staticVbSize > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Vertex buffer size exceeds 4GB");
    }

    ResourceBindFlags vbBindFlags = ResourceBindFlags::Vertex | ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess;
    Buffer::SharedPtr pStaticBuffer = Buffer::createStructured(mpDevice, sizeof(PackedStaticVertexData), (uint32_t)vertexCount, vbBindFlags, Buffer::CpuAccess::None, nullptr, false);
    LLOG_TRC << "pStaticBuffer buffer size " << pStaticBuffer->getSize();


    Vao::BufferVec pVBs(kVertexBufferCount);
    pVBs[kStaticDataBufferIndex] = pStaticBuffer;

    // Create the draw ID buffer.
    // This is only needed when rasterizing the scene.
    ResourceFormat drawIDFormat = drawCount <= (1 << 16) ? ResourceFormat::R16Uint : ResourceFormat::R32Uint;

    Buffer::SharedPtr pDrawIDBuffer = nullptr;
    if (drawIDFormat == ResourceFormat::R16Uint)
    {
        assert(drawCount <= (1 << 16));
        std::vector<uint16_t> drawIDs(drawCount);
        for (uint32_t i = 0; i < drawCount; i++) drawIDs[i] = i;
        pDrawIDBuffer = Buffer::create(mpDevice, drawCount * sizeof(uint16_t), ResourceBindFlags::Vertex, Buffer::CpuAccess::None, drawIDs.data());
    }
    else if (drawIDFormat == ResourceFormat::R32Uint)
    {
        std::vector<uint32_t> drawIDs(drawCount);
        for (uint32_t i = 0; i < drawCount; i++) drawIDs[i] = i;
        pDrawIDBuffer = Buffer::create(mpDevice, drawCount * sizeof(uint32_t), ResourceBindFlags::Vertex, Buffer::CpuAccess::None, drawIDs.data());
    }
    else should_not_get_here();

    LLOG_TRC << "pDrawIDBuffer buffer size " << pDrawIDBuffer->getSize();

    assert(pDrawIDBuffer);
    pVBs[kDrawIdBufferIndex] = pDrawIDBuffer;

    // Create vertex layout.
    // The layout only initializes the vertex data and draw ID layout. The skinning data doesn't get passed into the vertex shader.
    VertexLayout::SharedPtr pLayout = VertexLayout::create();

    // Add the packed static vertex data layout.
    VertexBufferLayout::SharedPtr pStaticLayout = VertexBufferLayout::create();
    pStaticLayout->addElement(VERTEX_POSITION_NAME, offsetof(PackedStaticVertexData, position), ResourceFormat::RGB32Float, 1, VERTEX_POSITION_LOC);
    pStaticLayout->addElement(VERTEX_TEXCOORD_U_NAME, offsetof(PackedStaticVertexData, texU), ResourceFormat::R32Float, 1, VERTEX_TEXCOORD_U_LOC);
    pStaticLayout->addElement(VERTEX_PACKED_NORMAL_TANGENT_CURVE_RADIUS_NAME, offsetof(PackedStaticVertexData, packedNormalTangentCurveRadius), ResourceFormat::RGB32Float, 1, VERTEX_PACKED_NORMAL_TANGENT_CURVE_RADIUS_LOC);
    pStaticLayout->addElement(VERTEX_TEXCOORD_V_NAME, offsetof(PackedStaticVertexData, texV), ResourceFormat::R32Float, 1, VERTEX_TEXCOORD_V_LOC);
    pLayout->addBufferLayout(kStaticDataBufferIndex, pStaticLayout);

    // Add the draw ID layout.
    VertexBufferLayout::SharedPtr pInstLayout = VertexBufferLayout::create();
    pInstLayout->addElement(INSTANCE_DRAW_ID_NAME, 0, drawIDFormat, 1, INSTANCE_DRAW_ID_LOC);
    pInstLayout->setInputClass(VertexBufferLayout::InputClass::PerInstanceData, 1);
    pLayout->addBufferLayout(kDrawIdBufferIndex, pInstLayout);

    // Create the VAO objects.
    // Note that the global index buffer can be mixed 16/32-bit format.
    // For drawing the meshes we need separate VAOs for these cases.
    mpMeshVao = Vao::create(Vao::Topology::TriangleList, pLayout, pVBs, pIB, ResourceFormat::R32Uint);
    mpMeshVao16Bit = Vao::create(Vao::Topology::TriangleList, pLayout, pVBs, pIB, ResourceFormat::R16Uint);
}

void Scene::createCurveVao(const std::vector<uint32_t>& indexData, const std::vector<StaticCurveVertexData>& staticData) {
    if (indexData.empty() || staticData.empty()) return;

    // Create the index buffer.
    size_t ibSize = sizeof(uint32_t) * indexData.size();
    if (ibSize > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Curve index buffer size exceeds 4GB");
    }

    Buffer::SharedPtr pIB = nullptr;
    if (ibSize > 0) {
        ResourceBindFlags ibBindFlags = Resource::BindFlags::Index | ResourceBindFlags::ShaderResource;
        pIB = Buffer::create(mpDevice, ibSize, ibBindFlags, Buffer::CpuAccess::None, indexData.data());
    }

    // Create the vertex data as structured buffers.
    const size_t vertexCount = (uint32_t)staticData.size();
    size_t staticVbSize = sizeof(StaticCurveVertexData) * vertexCount;
    if (staticVbSize > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Curve vertex buffer exceeds 4GB");
    }

    ResourceBindFlags vbBindFlags = ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::Vertex;
    // Also upload the curve vertex data.
    Buffer::SharedPtr pStaticBuffer = Buffer::createStructured(mpDevice, sizeof(StaticCurveVertexData), (uint32_t)vertexCount, vbBindFlags, Buffer::CpuAccess::None, staticData.data(), false);

    // Curves do not need DrawIDBuffer.
    Vao::BufferVec pVBs(kVertexBufferCount - 1);
    pVBs[kStaticDataBufferIndex] = pStaticBuffer;

    // Create vertex layout.
    // The layout only initializes the vertex data layout. The skinning data doesn't get passed into the vertex shader.
    VertexLayout::SharedPtr pLayout = VertexLayout::create();

    // Add the packed static vertex data layout.
    VertexBufferLayout::SharedPtr pStaticLayout = VertexBufferLayout::create();
    pStaticLayout->addElement(CURVE_VERTEX_POSITION_NAME, offsetof(StaticCurveVertexData, position), ResourceFormat::RGB32Float, 1, CURVE_VERTEX_POSITION_LOC);
    pStaticLayout->addElement(CURVE_VERTEX_RADIUS_NAME, offsetof(StaticCurveVertexData, radius), ResourceFormat::R32Float, 1, CURVE_VERTEX_RADIUS_LOC);
    pStaticLayout->addElement(CURVE_VERTEX_TEXCOORD_NAME, offsetof(StaticCurveVertexData, texCrd), ResourceFormat::RG32Float, 1, CURVE_VERTEX_TEXCOORD_LOC);
    pLayout->addBufferLayout(kStaticDataBufferIndex, pStaticLayout);

    // Create the VAO objects.
    mpCurveVao = Vao::create(Vao::Topology::LineStrip, pLayout, pVBs, pIB, ResourceFormat::R32Uint);
}
    

void Scene::setSDFGridConfig() {
    if (mSDFGrids.empty()) return;

    for (const SDFGrid::SharedPtr& pSDFGrid : mSDFGrids) {
        if (mSDFGridConfig.implementation == SDFGrid::Type::None)
        {
            mSDFGridConfig.implementation = pSDFGrid->getType();
        }
        else if (mSDFGridConfig.implementation != pSDFGrid->getType())
        {
            throw std::runtime_error("All SDF grids in the same scene must currently be of the same type.");
        }
    }

    // Set default SDF grid config and compute allowed SDF grid UI settings list.

    switch (mSDFGridConfig.implementation) {
        case SDFGrid::Type::NormalizedDenseGrid:
        {
            mSDFGridConfig.intersectionMethod = SDFGridIntersectionMethod::VoxelSphereTracing;
            mSDFGridConfig.gradientEvaluationMethod = SDFGridGradientEvaluationMethod::NumericDiscontinuous;
            mSDFGridConfig.solverMaxIterations = 256;
            mSDFGridConfig.optimizeVisibilityRays = true;

            break;
        }
        case SDFGrid::Type::SparseVoxelSet:
        case SDFGrid::Type::SparseBrickSet:
        {
            mSDFGridConfig.intersectionMethod = SDFGridIntersectionMethod::VoxelSphereTracing;
            mSDFGridConfig.gradientEvaluationMethod = SDFGridGradientEvaluationMethod::NumericDiscontinuous;
            mSDFGridConfig.solverMaxIterations = 256;
            mSDFGridConfig.optimizeVisibilityRays = true;

            break;
        }
        case SDFGrid::Type::SparseVoxelOctree:
        {
            mSDFGridConfig.intersectionMethod = SDFGridIntersectionMethod::VoxelSphereTracing;
            mSDFGridConfig.gradientEvaluationMethod = SDFGridGradientEvaluationMethod::NumericDiscontinuous;
            mSDFGridConfig.solverMaxIterations = 256;
            mSDFGridConfig.optimizeVisibilityRays = true;
        }
    }
}

void Scene::initSDFGrids() {
    if (mSDFGridConfig.implementation == SDFGrid::Type::SparseBrickSet) {
        mSDFGridConfig.implementationData.SBS.virtualBrickCoordsBitCount = 0;
        mSDFGridConfig.implementationData.SBS.brickLocalVoxelCoordsBitCount = 0;
    }
    else if (mSDFGridConfig.implementation == SDFGrid::Type::SparseVoxelOctree)
    {
        mSDFGridConfig.implementationData.SVO.svoIndexBitCount = 0;
    }

    for (const SDFGrid::SharedPtr& pSDFGrid : mSDFGrids) {
        pSDFGrid->createResources(mpDevice->getRenderContext());
        #ifndef _WIN32
        if (mSDFGridConfig.implementation == SDFGrid::Type::SparseBrickSet) {
            const SDFSBS* pSBS = reinterpret_cast<const SDFSBS*>(pSDFGrid.get());
            mSDFGridConfig.implementationData.SBS.virtualBrickCoordsBitCount = std::max(mSDFGridConfig.implementationData.SBS.virtualBrickCoordsBitCount, pSBS->getVirtualBrickCoordsBitCount());
            mSDFGridConfig.implementationData.SBS.brickLocalVoxelCoordsBitCount = std::max(mSDFGridConfig.implementationData.SBS.brickLocalVoxelCoordsBitCount, pSBS->getBrickLocalVoxelCoordsBrickCount());
        }
        else if (mSDFGridConfig.implementation == SDFGrid::Type::SparseVoxelOctree)
        {
            const SDFSVO* pSVO = reinterpret_cast<const SDFSVO*>(pSDFGrid.get());
            mSDFGridConfig.implementationData.SVO.svoIndexBitCount = std::max(mSDFGridConfig.implementationData.SVO.svoIndexBitCount, pSVO->getSVOIndexBitCount());
        }
        #endif
    }
}


void Scene::initResources() {
    ComputeProgram::SharedPtr pProgram = ComputeProgram::createFromFile(mpDevice, "Scene/SceneBlock.slang", "main", getSceneDefines());
    ParameterBlockReflection::SharedConstPtr pReflection = pProgram->getReflector()->getParameterBlock(kParameterBlockName);
    assert(pReflection);

    mpSceneBlock = ParameterBlock::create(mpDevice, pReflection);
    
    if (!mGeometryInstanceData.empty()) {
        mpGeometryInstancesBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kGeometryInstanceBufferName], (uint32_t)mGeometryInstanceData.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpGeometryInstancesBuffer->setName("Scene::mpGeometryInstancesBuffer");
    }

    if (!mMeshDesc.empty()) {
        mpMeshesBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kMeshBufferName], (uint32_t)mMeshDesc.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpMeshesBuffer->setName("Scene::mpMeshesBuffer");
    }
    
    if (!mCurveDesc.empty()) {
        mpCurvesBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kCurveBufferName], (uint32_t)mCurveDesc.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpCurvesBuffer->setName("Scene::mpCurvesBuffer");
    }

    if (!mLights.empty()) {
        mpLightsBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kLightsBufferName], (uint32_t)mLights.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpLightsBuffer->setName("Scene::mpLightsBuffer");
    }

    if (!mGridVolumes.empty())
    {
        mpGridVolumesBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kGridVolumesBufferName], (uint32_t)mGridVolumes.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpGridVolumesBuffer->setName("Scene::mpGridVolumesBuffer");
    }

    if (!mMeshletGroups.empty()) {
        mpMeshletGroupsBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kMeshletGroupsBufferName], (uint32_t)mMeshletGroups.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpMeshletGroupsBuffer->setName("Scene::mpMeshletGroupsBuffer");
    }
    
    if (!mMeshletsData.empty()) {
        mpMeshletsBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kMeshletsBufferName], (uint32_t)mMeshletsData.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpMeshletsBuffer->setName("Scene::mpMeshletsBuffer");
    }

    if (!mMeshletIndices.empty()) {
        mpMeshletIndicesBuffer = Buffer::create(mpDevice, mMeshletIndices.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr);
        mpMeshletIndicesBuffer->setName("Scene::mpMeshletIndicesBuffer");
    }

    if (!mMeshletVertices.empty()) {
        mpMeshletVerticesBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kMeshletVerticesBufferName], (uint32_t)mMeshletVertices.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpMeshletVerticesBuffer->setName("Scene::mpMeshletVeticesBuffer");
    }

    if (!mMeshletPrimIndices.empty()) {
        mpMeshletPrimIndicesBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kMeshletPrimIndicesBufferName], (uint32_t)mMeshletPrimIndices.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpMeshletPrimIndicesBuffer->setName("Scene::mpMeshletPrimIndicesBuffer");
    }

    if (!mPerPrimMaterialIDs.empty()) {
        mpPerPrimMaterialIDsBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kPerPrimMaterialIDsBufferName], (int32_t)mPerPrimMaterialIDs.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpPerPrimMaterialIDsBuffer->setName("Scene::mpPerPrimMaterialIDsBuffer");
    }
}

void Scene::uploadResources() {
    assert(mpAnimationController);

    // Upload geometry.

    // Meshes.
    if (!mMeshDesc.empty()) mpMeshesBuffer->setBlob(mMeshDesc.data(), 0, sizeof(MeshDesc) * mMeshDesc.size());

    // Meshlets.
    if (!mMeshletGroups.empty()) mpMeshletGroupsBuffer->setBlob(mMeshletGroups.data(), 0, sizeof(MeshletGroup) * mMeshletGroups.size());
    if (!mMeshletsData.empty()) mpMeshletsBuffer->setBlob(mMeshletsData.data(), 0, sizeof(PackedMeshletData) * mMeshletsData.size());
    if (!mMeshletVertices.empty()) mpMeshletVerticesBuffer->setBlob(mMeshletVertices.data(), 0, sizeof(uint32_t) * mMeshletVertices.size());
    if (!mMeshletIndices.empty()) mpMeshletIndicesBuffer->setBlob(mMeshletIndices.data(), 0, sizeof(uint8_t) * mMeshletIndices.size());
    if (!mMeshletPrimIndices.empty()) mpMeshletPrimIndicesBuffer->setBlob(mMeshletPrimIndices.data(), 0, sizeof(uint32_t) * mMeshletPrimIndices.size());
    
    // Per prim material ids.
    if (!mPerPrimMaterialIDs.empty()) mpPerPrimMaterialIDsBuffer->setBlob(mPerPrimMaterialIDs.data(), 0, sizeof(int32_t) * mPerPrimMaterialIDs.size());

    // Curves
    if (!mCurveDesc.empty()) mpCurvesBuffer->setBlob(mCurveDesc.data(), 0, sizeof(CurveDesc) * mCurveDesc.size());

    mpSceneBlock->setBuffer(kGeometryInstanceBufferName, mpGeometryInstancesBuffer);
    mpSceneBlock->setBuffer(kMeshBufferName, mpMeshesBuffer);

    mpSceneBlock->setBuffer(kMeshletGroupsBufferName, mpMeshletGroupsBuffer);
    mpSceneBlock->setBuffer(kMeshletsBufferName, mpMeshletsBuffer);
    mpSceneBlock->setBuffer(kMeshletIndicesBufferName, mpMeshletIndicesBuffer);
    mpSceneBlock->setBuffer(kMeshletVerticesBufferName, mpMeshletVerticesBuffer);
    mpSceneBlock->setBuffer(kMeshletPrimIndicesBufferName, mpMeshletPrimIndicesBuffer);

    mpSceneBlock->setBuffer(kPerPrimMaterialIDsBufferName, mpPerPrimMaterialIDsBuffer);

    mpSceneBlock->setBuffer(kCurveBufferName, mpCurvesBuffer);

    auto sdfGridsVar = mpSceneBlock[kSDFGridsArrayName];

    for (uint32_t i = 0; i < mSDFGrids.size(); i++) {
        const SDFGrid::SharedPtr& pGrid = mSDFGrids[i];
        pGrid->setShaderData(sdfGridsVar[i]);
    }

    mpSceneBlock->setBuffer(kLightsBufferName, mpLightsBuffer);
    mpSceneBlock->setBuffer(kGridVolumesBufferName, mpGridVolumesBuffer);

    if (mpMeshVao != nullptr) {
        if (hasIndexBuffer()) mpSceneBlock->setBuffer(kIndexBufferName, mpMeshVao->getIndexBuffer());
        mpSceneBlock->setBuffer(kVertexBufferName, mpMeshVao->getVertexBuffer(Scene::kStaticDataBufferIndex));
        mpSceneBlock->setBuffer(kPrevVertexBufferName, mpAnimationController->getPrevVertexData()); // Can be nullptr
    }

    if (mpCurveVao != nullptr) {
        mpSceneBlock->setBuffer(kCurveIndexBufferName, mpCurveVao->getIndexBuffer());
        mpSceneBlock->setBuffer(kCurveVertexBufferName, mpCurveVao->getVertexBuffer(Scene::kStaticDataBufferIndex));
        mpSceneBlock->setBuffer(kPrevCurveVertexBufferName, mpAnimationController->getPrevCurveVertexData());
    }
}

Scene::UpdateFlags Scene::updateMaterials(bool forceUpdate) {
    // Update material system.
    Material::UpdateFlags materialUpdates = mpMaterialSystem->update(forceUpdate);

    UpdateFlags flags = UpdateFlags::None;
    if (forceUpdate || materialUpdates != Material::UpdateFlags::None) {
        flags |= UpdateFlags::MaterialsChanged;

        // Bind materials parameter block to scene.
        mpSceneBlock->setParameterBlock(kMaterialsBlockName, mpMaterialSystem->getParameterBlock());

        // If displacement parameters have changed, we need to trigger displacement update.
        if (is_set(materialUpdates, Material::UpdateFlags::DisplacementChanged)) {
            mDisplacement.needsUpdate = true;
        }

        updateMaterialStats();
    }

    return flags;
}

Scene::UpdateFlags Scene::updateLightLinker(bool forceUpdate) {
    if(!mpLightLinker) return UpdateFlags::None;

    // Update light linker.
    LightLinker::UpdateFlags lightLinkerUpdates = mpLightLinker->update(forceUpdate);

    UpdateFlags flags = UpdateFlags::None;
    if (forceUpdate || (lightLinkerUpdates != LightLinker::UpdateFlags::None)) {
        flags |= UpdateFlags::LightLinkerChanged;
        mpLightLinker->setShaderData(mpSceneBlock[kLightLinker]);
    }

    return flags;
}

void Scene::uploadSelectedCamera() {
    getCamera()->setShaderData(mpSceneBlock[kCamera]);
}

void Scene::updateBounds() {
    //const auto& globalMatrices = mpAnimationController->getGlobalMatrices();
    const auto& globalMatrixLists = mpAnimationController->getGlobalMatrixLists();

    mSceneBB = AABB();

    for (const auto& inst : mGeometryInstanceData) {
        const auto& matrixList = globalMatrixLists[inst.nodeID];
        for(const glm::mat4& transform: matrixList) {
            switch (inst.getType()) {
                case GeometryType::TriangleMesh:
                case GeometryType::DisplacedTriangleMesh: 
                {
                    const AABB& meshBB = mMeshBBs[inst.geometryID];
                    mSceneBB |= meshBB.transform(transform);
                    break;
                }
                case GeometryType::Curve:
                {
                    const AABB& curveBB = mCurveBBs[inst.geometryID];
                    mSceneBB |= curveBB.transform(transform);
                    break;
                }
                case GeometryType::SDFGrid:
                {
                    float3x3 transform3x3 = glm::mat3(transform);
                    transform3x3[0] = glm::abs(transform3x3[0]);
                    transform3x3[1] = glm::abs(transform3x3[1]);
                    transform3x3[2] = glm::abs(transform3x3[2]);
                    float3 center = transform[3];
                    float3 halfExtent = transform3x3 * float3(0.5f);
                    mSceneBB |= AABB(center - halfExtent, center + halfExtent);
                    break;
                }
            }
        }
    }

    for (const auto& aabb : mCustomPrimitiveAABBs) {
        mSceneBB |= aabb;
    }

    for (const auto& pGridVolume : mGridVolumes) {
        mSceneBB |= pGridVolume->getBounds();
    }
}

Scene::UpdateFlags Scene::updateSDFGrids(RenderContext* pRenderContext) {
    UpdateFlags updateFlags = UpdateFlags::None;
    if (!is_set(mGeometryTypes, GeometryTypeFlags::SDFGrid)) return updateFlags;

    for (uint32_t sdfGridID = 0; sdfGridID < mSDFGrids.size(); ++sdfGridID) {
        SDFGrid::SharedPtr& pSDFGrid = mSDFGrids[sdfGridID];
        SDFGrid::UpdateFlags sdfGridUpdateFlags = pSDFGrid->update(pRenderContext);

        if (is_set(sdfGridUpdateFlags, SDFGrid::UpdateFlags::AABBsChanged)) {
            updateGeometryStats();

            // Clear any previous BLAS data. This will trigger a full BLAS/TLAS rebuild.
            // TODO: Support partial rebuild of just the procedural primitives.
            mBlasDataValid = false;
            updateFlags |= Scene::UpdateFlags::SDFGeometryChanged;
        }

        if (is_set(sdfGridUpdateFlags, SDFGrid::UpdateFlags::BuffersReallocated)) {
            updateGeometryStats();
            pSDFGrid->setShaderData(mpSceneBlock[kSDFGridsArrayName][sdfGridID]);
            updateFlags |= Scene::UpdateFlags::SDFGeometryChanged;
        }
    }

    return updateFlags;
}

void Scene::updateGeometryInstances(bool forceUpdate) {
    if (mGeometryInstanceData.empty()) return;

    bool dataChanged = false;
    const auto& globalMatrixLists = mpAnimationController->getGlobalMatrixLists();

    struct OffsetAndCount {
        uint32_t offset;
        uint8_t count;
    };

    size_t currMatrixListOffset = 0;
    std::vector<OffsetAndCount> matrixListsOffsetsAndCounts(globalMatrixLists.size());
    for(size_t i = 0; i < globalMatrixLists.size(); ++i) {
        auto const& list = globalMatrixLists[i];
        matrixListsOffsetsAndCounts[i].offset = static_cast<uint32_t>(currMatrixListOffset);
        matrixListsOffsetsAndCounts[i].count = static_cast<uint8_t>(list.size());
        currMatrixListOffset += list.size();
    }

    for (auto& inst : mGeometryInstanceData) {
        if (inst.getType() == GeometryType::TriangleMesh || inst.getType() == GeometryType::DisplacedTriangleMesh) {
            uint32_t prevFlags = inst.flags;
            uint32_t prevMatrixOffset = inst.globalMatrixOffset;
            uint8_t  prevMatrixCount = inst.matrixCount;

            assert(inst.nodeID < globalMatrixLists.size());
            assert(inst.nodeID < matrixListsOffsetsAndCounts.size());

            const auto& transformList = globalMatrixLists[inst.nodeID];
            if(transformList.empty()) {
                LLOG_ERR << "Transform matrix list is empty for instance " << inst.nodeID << " !!!";
                continue;
            }

            bool isTransformFlipped = doesTransformFlip(transformList[0]);
            bool isObjectFrontFaceCW = getMesh(inst.geometryID).isFrontFaceCW();
            bool isWorldFrontFaceCW = isObjectFrontFaceCW ^ isTransformFlipped;

            inst.globalMatrixOffset = matrixListsOffsetsAndCounts[inst.nodeID].offset;
            inst.matrixCount = matrixListsOffsetsAndCounts[inst.nodeID].count;

            if (isTransformFlipped) inst.flags |= (uint32_t)GeometryInstanceFlags::TransformFlipped;
            else inst.flags &= ~(uint32_t)GeometryInstanceFlags::TransformFlipped;

            if (isObjectFrontFaceCW) inst.flags |= (uint32_t)GeometryInstanceFlags::IsObjectFrontFaceCW;
            else inst.flags &= ~(uint32_t)GeometryInstanceFlags::IsObjectFrontFaceCW;

            if (isWorldFrontFaceCW) inst.flags |= (uint32_t)GeometryInstanceFlags::IsWorldFrontFaceCW;
            else inst.flags &= ~(uint32_t)GeometryInstanceFlags::IsWorldFrontFaceCW;

            dataChanged |= (inst.flags != prevFlags);
            dataChanged |= (inst.globalMatrixOffset != prevMatrixOffset);
            dataChanged |= (inst.matrixCount != prevMatrixCount);
        }
    }

    if (forceUpdate || dataChanged) {
        uint32_t byteSize = (uint32_t)(mGeometryInstanceData.size() * sizeof(GeometryInstanceData));
        mpGeometryInstancesBuffer->setBlob(mGeometryInstanceData.data(), 0, byteSize);
    }
}


Scene::UpdateFlags Scene::updateRaytracingAABBData(bool forceUpdate) {
    // This function updates the global list of AABBs for all procedural primitives.
    // TODO: Move this code to the GPU. Then the CPU copies of some buffers won't be needed anymore.
    Scene::UpdateFlags flags = Scene::UpdateFlags::None;

    size_t curveAABBCount = 0;
    for (const auto& curve : mCurveDesc) curveAABBCount += curve.indexCount;

    size_t customAABBCount = mCustomPrimitiveAABBs.size();
    size_t totalAABBCount = curveAABBCount + customAABBCount;

    if (totalAABBCount > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Procedural primitive count exceeds the maximum");
    }

    // If there are no procedural primitives, clear the CPU buffer and return.
    // We'll leave the GPU buffer to be lazily re-allocated when needed.
    if (totalAABBCount == 0) {
        mRtAABBRaw.clear();
        return flags;
    }

    mRtAABBRaw.resize(totalAABBCount);
    uint32_t offset = 0;

    size_t firstUpdated = std::numeric_limits<size_t>::max();
    size_t lastUpdated = 0;

    if (forceUpdate) {
        // Compute AABBs of curve segments.
        for (const auto& curve : mCurveDesc) {
            // Track range of updated AABBs.
            // TODO: Per-curve flag to indicate changes. For now assume all curves need updating.
            firstUpdated = std::min(firstUpdated, (size_t)offset);
            lastUpdated = std::max(lastUpdated, (size_t)offset + curve.indexCount);

            const auto* indexData = &mCurveIndexData[curve.ibOffset];
            const auto* staticData = &mCurveStaticData[curve.vbOffset];

            for (uint32_t j = 0; j < curve.indexCount; j++) {
                AABB curveSegBB;
                uint32_t v = indexData[j];

                for (uint32_t k = 0; k <= curve.degree; k++) {
                    curveSegBB.include(staticData[v + k].position - float3(staticData[v + k].radius));
                    curveSegBB.include(staticData[v + k].position + float3(staticData[v + k].radius));
                }

                mRtAABBRaw[offset++] = static_cast<RtAABB>(curveSegBB);
            }
            flags |= Scene::UpdateFlags::CurvesMoved;
        }
        assert(offset == curveAABBCount);
    }
    offset = (uint32_t)curveAABBCount;

    if (forceUpdate || mCustomPrimitivesChanged || mCustomPrimitivesMoved) {
        mCustomPrimitiveAABBOffset = offset;

        // Track range of updated AABBs.
        firstUpdated = std::min(firstUpdated, (size_t)offset);
        lastUpdated = std::max(lastUpdated, (size_t)offset + customAABBCount);

        for (auto& aabb : mCustomPrimitiveAABBs) {
            mRtAABBRaw[offset++] = static_cast<RtAABB>(aabb);
        }
        assert(offset == totalAABBCount);
        flags |= Scene::UpdateFlags::CustomPrimitivesMoved;
    }

    // Create/update GPU buffer. This is used in BLAS creation and also bound to the scene for lookup in shaders.
    // Requires unordered access and will be in Non-Pixel Shader Resource state.
    if (mpRtAABBBuffer == nullptr || mpRtAABBBuffer->getElementCount() < (uint32_t)mRtAABBRaw.size()) {
        mpRtAABBBuffer = Buffer::createStructured(mpDevice, sizeof(RtAABB), (uint32_t)mRtAABBRaw.size(), Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, mRtAABBRaw.data(), false);
        mpRtAABBBuffer->setName("Scene::mpRtAABBBuffer");

        // Bind the new buffer to the scene.
        assert(mpSceneBlock);
        mpSceneBlock->setBuffer(kProceduralPrimAABBBufferName, mpRtAABBBuffer);
    }
    else if (firstUpdated < lastUpdated)
    {
        size_t bytes = sizeof(RtAABB) * mRtAABBRaw.size();
        assert(mpRtAABBBuffer && mpRtAABBBuffer->getSize() >= bytes);

        // Update the modified range of the GPU buffer.
        size_t offset = firstUpdated * sizeof(RtAABB);
        bytes = (lastUpdated - firstUpdated) * sizeof(RtAABB);
        mpRtAABBBuffer->setBlob(mRtAABBRaw.data() + firstUpdated, offset, bytes);
    }

    return flags;
}

Scene::UpdateFlags Scene::updateDisplacement(bool forceUpdate) {
    if (!hasGeometryType(GeometryType::DisplacedTriangleMesh)) return UpdateFlags::None;

    // For now we assume that displaced meshes are static.
    // Create AABB and AABB update task buffers.
    if (!mDisplacement.pAABBBuffer) {
        mDisplacement.meshData.resize(mMeshDesc.size());
        mDisplacement.updateTasks.clear();

        uint32_t AABBOffset = 0;

        for (uint32_t meshID = 0; meshID < mMeshDesc.size(); ++meshID) {
            const auto& mesh = mMeshDesc[meshID];

            if (!mesh.isDisplaced()) {
                mDisplacement.meshData[meshID] = {};
                continue;
            }

            uint32_t AABBCount = mesh.getTriangleCount();
            mDisplacement.meshData[meshID] = { AABBOffset, AABBCount };
            AABBOffset += AABBCount;

            DisplacementUpdateTask task;
            task.meshID = meshID;
            task.triangleIndex = 0;
            task.AABBIndex = mDisplacement.meshData[meshID].AABBOffset;
            task.count = mDisplacement.meshData[meshID].AABBCount;
            mDisplacement.updateTasks.push_back(task);
        }

        mDisplacement.pAABBBuffer = Buffer::createStructured(mpDevice, sizeof(RtAABB), AABBOffset, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);

        assert(mDisplacement.updateTasks.size() < std::numeric_limits<uint32_t>::max());
        mDisplacement.pUpdateTasksBuffer = Buffer::createStructured(mpDevice, (uint32_t)sizeof(DisplacementUpdateTask), (uint32_t)mDisplacement.updateTasks.size(), ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, mDisplacement.updateTasks.data());
    }

    assert(!mDisplacement.updateTasks.empty());

    // We cannot access the scene parameter block until its finalized.
    if (!mFinalized) return UpdateFlags::None;

    // Update the AABB data.
    if (!mDisplacement.pUpdatePass) {
        mDisplacement.pUpdatePass = ComputePass::create(mpDevice, "Scene/Displacement/DisplacementUpdate.cs.slang", "main", getSceneDefines());
        mDisplacement.needsUpdate = true;
    }

    if (mDisplacement.needsUpdate) {
        // TODO: Only update objects with modified materials.

        PROFILE(mpDevice, "updateDisplacement");

        mDisplacement.pUpdatePass->getVars()->setParameterBlock(kParameterBlockName, mpSceneBlock);

        auto var = mDisplacement.pUpdatePass->getRootVar()["CB"];
        var["gTaskCount"] = (uint32_t)mDisplacement.updateTasks.size();
        var["gTasks"] = mDisplacement.pUpdateTasksBuffer;
        var["gAABBs"] = mDisplacement.pAABBBuffer;

        mDisplacement.pUpdatePass->execute(mpDevice->getRenderContext(), uint3(DisplacementUpdateTask::kThreadCount, (uint32_t)mDisplacement.updateTasks.size(), 1));

        mCustomPrimitivesChanged = true; // Trigger a BVH update.
        mDisplacement.needsUpdate = false;
        return UpdateFlags::DisplacementChanged;
    }

    return UpdateFlags::None;
}

Scene::UpdateFlags Scene::updateProceduralPrimitives(bool forceUpdate) {
    // Update the AABB buffer.
    // The bounds are updated if any primitive has moved or been added/removed.
    Scene::UpdateFlags flags = updateRaytracingAABBData(forceUpdate);

    // Update the procedural primitives metadata.
    if (forceUpdate || mCustomPrimitivesChanged) {
        // Update the custom primitives buffer.
        if (!mCustomPrimitiveDesc.empty()) {
            if (mpCustomPrimitivesBuffer == nullptr || mpCustomPrimitivesBuffer->getElementCount() < (uint32_t)mCustomPrimitiveDesc.size()) {
                mpCustomPrimitivesBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kCustomPrimitiveBufferName], (uint32_t)mCustomPrimitiveDesc.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, mCustomPrimitiveDesc.data(), false);
                mpCustomPrimitivesBuffer->setName("Scene::mpCustomPrimitivesBuffer");

                // Bind the buffer to the scene.
                assert(mpSceneBlock);
                mpSceneBlock->setBuffer(kCustomPrimitiveBufferName, mpCustomPrimitivesBuffer);
            } else {
                size_t bytes = sizeof(CustomPrimitiveDesc) * mCustomPrimitiveDesc.size();
                assert(mpCustomPrimitivesBuffer && mpCustomPrimitivesBuffer->getSize() >= bytes);
                mpCustomPrimitivesBuffer->setBlob(mCustomPrimitiveDesc.data(), 0, bytes);
            }
        }

        // Update scene constants.
        uint32_t customPrimitiveInstanceOffset = getGeometryInstanceCount();
        uint32_t customPrimitiveInstanceCount = getCustomPrimitiveCount();

        auto var = mpSceneBlock->getRootVar();
        var["customPrimitiveInstanceOffset"] = customPrimitiveInstanceOffset;
        var["customPrimitiveInstanceCount"] = customPrimitiveInstanceCount;
        var["customPrimitiveAABBOffset"] = mCustomPrimitiveAABBOffset;

        flags |= Scene::UpdateFlags::GeometryChanged;
    }

    return flags;
}

void Scene::updateGeometryTypes() {
    mGeometryTypes = GeometryTypeFlags(0);
    if (getMeshCount() > 0) mGeometryTypes |= GeometryTypeFlags::TriangleMesh;
    auto hasDisplaced = std::any_of(mMeshDesc.begin(), mMeshDesc.end(), [](const auto& mesh) { return mesh.isDisplaced(); });
    if (hasDisplaced) mGeometryTypes |= GeometryTypeFlags::DisplacedTriangleMesh;
    if (getCurveCount() > 0) mGeometryTypes |= GeometryTypeFlags::Curve;
    if (getSDFGridCount() > 0) mGeometryTypes |= GeometryTypeFlags::SDFGrid;
    if (getCustomPrimitiveCount() > 0) mGeometryTypes |= GeometryTypeFlags::Custom;
}

void Scene::finalize() {
    assert(mHas16BitIndices || mHas32BitIndices);

    // Prepare the materials.
    // This step is necessary for setting up scene defines, which are used below when creating scene resources.
    // TODO: Remove this when unbounded descriptor arrays are supported (#1321).
    assert(mpMaterialSystem);
    mpMaterialSystem->finalize();


    updateGeometryTypes();
    initSDFGrids();
    mHitInfo.init(*this, mUseCompressedHitInfo);
    initResources(); // Requires scene defines
    mpAnimationController->animate(mpDevice->getRenderContext(), 0); // Requires Scene block to exist
    updateGeometry(true);
    updateGeometryInstances(true);

    if (mpLightProfile) {
        mpLightProfile->bake(mpDevice->getRenderContext());
        mpLightProfile->setShaderData(mpSceneBlock[kLightProfile]);
    }

    if (mpLightLinker) {
        mpLightLinker->setShaderData(mpSceneBlock[kLightLinker]);
    }

    updateBounds();
    createDrawList();
    if (mCameras.size() == 0) {
        // Create a new camera to use in the event of a scene with no cameras
        mCameras.push_back(Camera::create());
        resetCamera();
    }
    setCameraController(mCamCtrlType);
    initializeCameras();
    uploadSelectedCamera();
    addViewpoint();
    updateLights(true);
    updateLightLinker(true); // Important !!! Must be called after lights update!
    updateGridVolumes(true);
    updateEnvMap(true);
    updateMaterials(true);
    uploadResources(); // Upload data after initialization is complete

    updateGeometryStats();
    updateMaterialStats();
    updateLightStats();
    updateGridVolumeStats();

    mFinalized = true;
}

void Scene::printMeshletsStats() const {
    if(mMeshletsData.empty()) return;
    LLOG_INF << "Meshlets count: " << mSceneStats.meshletsCount;
    LLOG_INF << "Meshlets data memory size: " << formatByteSize(mSceneStats.meshletsMemoryInBytes);
}

void Scene::initializeCameras() {
    for (auto& camera : mCameras) {
        updateAnimatable(*camera, *mpAnimationController, true);
        camera->beginFrame();
    }
}

void Scene::updateGeometryStats() {
    auto& s = mSceneStats;

    s.meshCount = getMeshCount();
    s.meshInstanceCount = 0;
    s.meshInstanceOpaqueCount = 0;
    s.transformCount = getAnimationController()->getGlobalMatricesCount();
    s.uniqueVertexCount = 0;
    s.uniqueTriangleCount = 0;
    s.instancedVertexCount = 0;
    s.instancedTriangleCount = 0;
    s.curveCount = getCurveCount();
    s.curveInstanceCount = 0;
    s.uniqueCurvePointCount = 0;
    s.uniqueCurveSegmentCount = 0;
    s.instancedCurvePointCount = 0;
    s.instancedCurveSegmentCount = 0;
    s.sdfGridCount = getSDFGridCount();
    s.sdfGridDescriptorCount = getSDFGridDescCount();
    s.sdfGridInstancesDataCount = 0;

    s.customPrimitiveCount = getCustomPrimitiveCount();

    for (uint32_t instanceID = 0; instanceID < getGeometryInstanceCount(); instanceID++) {
        const auto& instance = getGeometryInstance(instanceID);
        switch (instance.getType()) {
            case GeometryType::TriangleMesh:
            case GeometryType::DisplacedTriangleMesh: {
                s.meshInstanceCount++;
                const auto& mesh = getMesh(instance.geometryID);
                s.instancedVertexCount += mesh.vertexCount;
                s.instancedTriangleCount += mesh.getTriangleCount();

                auto pMaterial = getMaterial(instance.materialID);
                if (pMaterial->isOpaque()) s.meshInstanceOpaqueCount++;
                break;
            }
            case GeometryType::Curve: {
                s.curveInstanceCount++;
                const auto& curve = getCurve(instance.geometryID);
                s.instancedCurvePointCount += curve.vertexCount;
                s.instancedCurveSegmentCount += curve.getSegmentCount();
                break;
            }
            case GeometryType::SDFGrid: {
                s.sdfGridInstancesDataCount++;
                break;
            }
        }
    }

    for (uint32_t meshID = 0; meshID < getMeshCount(); meshID++) {
        const auto& mesh = getMesh(meshID);
        s.uniqueVertexCount += mesh.vertexCount;
        s.uniqueTriangleCount += mesh.getTriangleCount();
    }

    for (uint32_t curveID = 0; curveID < getCurveCount(); curveID++) {
        const auto& curve = getCurve(curveID);
        s.uniqueCurvePointCount += curve.vertexCount;
        s.uniqueCurveSegmentCount += curve.getSegmentCount();
    }

    // Meshlets
    s.meshletsCount = mMeshletsData.size();
    s.meshletsMemoryInBytes += s.meshletsCount * sizeof(PackedMeshletData);
    s.meshletsMemoryInBytes += mMeshletGroups.size() * sizeof(MeshletGroup);
    s.meshletsMemoryInBytes += mMeshletVertices.size() * sizeof(uint32_t);
    s.meshletsMemoryInBytes += mMeshletIndices.size() * sizeof(uint8_t);


    // Calculate memory usage.
    s.indexMemoryInBytes = 0;
    s.vertexMemoryInBytes = 0;
    s.geometryMemoryInBytes = 0;
    s.animationMemoryInBytes = 0;

    if (mpMeshVao) {
        const auto& pIB = mpMeshVao->getIndexBuffer();
        const auto& pVB = mpMeshVao->getVertexBuffer(kStaticDataBufferIndex);
        const auto& pDrawID = mpMeshVao->getVertexBuffer(kDrawIdBufferIndex);

        s.indexMemoryInBytes += pIB ? pIB->getSize() : 0;
        s.vertexMemoryInBytes += pVB ? pVB->getSize() : 0;
        s.geometryMemoryInBytes += pDrawID ? pDrawID->getSize() : 0;
    }

    s.curveIndexMemoryInBytes = 0;
    s.curveVertexMemoryInBytes = 0;

    if (mpCurveVao != nullptr) {
        const auto& pCurveIB = mpCurveVao->getIndexBuffer();
        const auto& pCurveVB = mpCurveVao->getVertexBuffer(kStaticDataBufferIndex);

        s.curveIndexMemoryInBytes += pCurveIB ? pCurveIB->getSize() : 0;
        s.curveVertexMemoryInBytes += pCurveVB ? pCurveVB->getSize() : 0;
    }

    s.sdfGridMemoryInBytes = 0;

    for (const SDFGrid::SharedPtr& pSDFGrid : mSDFGrids) {
        s.sdfGridMemoryInBytes += pSDFGrid->getSize();
    }

    s.geometryMemoryInBytes += mpGeometryInstancesBuffer ? mpGeometryInstancesBuffer->getSize() : 0;
    s.geometryMemoryInBytes += mpMeshesBuffer ? mpMeshesBuffer->getSize() : 0;
    s.geometryMemoryInBytes += mpCurvesBuffer ? mpCurvesBuffer->getSize() : 0;
    s.geometryMemoryInBytes += mpCustomPrimitivesBuffer ? mpCustomPrimitivesBuffer->getSize() : 0;
    s.geometryMemoryInBytes += mpRtAABBBuffer ? mpRtAABBBuffer->getSize() : 0;

    for (const auto& draw : mDrawArgs) {
        assert(draw.pBuffer);
        s.geometryMemoryInBytes += draw.pBuffer->getSize();
    }

    s.animationMemoryInBytes += getAnimationController()->getMemoryUsageInBytes();
}

void Scene::updateMaterialStats() {
    mSceneStats.materials = mpMaterialSystem->getStats();
}

void Scene::updateRaytracingBLASStats() {
    auto& s = mSceneStats;

    s.blasGroupCount = mBlasGroups.size();
    s.blasCount = mBlasData.size();
    s.blasCompactedCount = 0;
    s.blasOpaqueCount = 0;
    s.blasGeometryCount = 0;
    s.blasOpaqueGeometryCount = 0;
    s.blasMemoryInBytes = 0;
    s.blasScratchMemoryInBytes = 0;

    for (const auto& blas : mBlasData) {
        if (blas.useCompaction) s.blasCompactedCount++;
        s.blasMemoryInBytes += blas.blasByteSize;

        // Count number of opaque geometries in BLAS.
        uint64_t opaque = 0;
        for (const auto& desc : blas.geomDescs) {
            if (is_set(desc.flags, RtGeometryFlags::Opaque)) opaque++;
        }

        if (opaque == blas.geomDescs.size()) s.blasOpaqueCount++;
        s.blasGeometryCount += blas.geomDescs.size();
        s.blasOpaqueGeometryCount += opaque;
    }

    if (mpBlasScratch) s.blasScratchMemoryInBytes += mpBlasScratch->getSize();
    if (mpBlasStaticWorldMatrices) s.blasScratchMemoryInBytes += mpBlasStaticWorldMatrices->getSize();
}

void Scene::updateRaytracingTLASStats() {
    auto& s = mSceneStats;

    s.tlasCount = 0;
    s.tlasMemoryInBytes = 0;
    s.tlasScratchMemoryInBytes = 0;

    for (const auto& [i, tlas] : mTlasCache) {
        if (tlas.pTlasBuffer) {
            s.tlasMemoryInBytes += tlas.pTlasBuffer->getSize();
            s.tlasCount++;
        }
        if (tlas.pInstanceDescs) s.tlasScratchMemoryInBytes += tlas.pInstanceDescs->getSize();
    }
    if (mpTlasScratch) s.tlasScratchMemoryInBytes += mpTlasScratch->getSize();
}

void Scene::updateLightStats() {
    auto& s = mSceneStats;

    s.activeLightCount = mActiveLights.size();;
    s.totalLightCount = mLights.size();
    s.pointLightCount = 0;
    s.directionalLightCount = 0;
    s.rectLightCount = 0;
    s.sphereLightCount = 0;
    s.distantLightCount = 0;
    s.environmentLightCount = 0;

    for (const auto& light : mLights) {
        switch (light->getType()) {
            case LightType::Point:
                s.pointLightCount++;
                break;
            case LightType::Directional:
                s.directionalLightCount++;
                break;
            case LightType::Rect:
                s.rectLightCount++;
                break;
            case LightType::Sphere:
                s.sphereLightCount++;
                break;
            case LightType::Distant:
                s.distantLightCount++;
                break;
            case LightType::Env:
                s.environmentLightCount++;
                break;
        }
    }

    s.lightsMemoryInBytes = mpLightsBuffer ? mpLightsBuffer->getSize() : 0;
}

void Scene::updateGridVolumeStats() {
    auto& s = mSceneStats;

    s.gridVolumeCount = mGridVolumes.size();
    s.gridVolumeMemoryInBytes = mpGridVolumesBuffer ? mpGridVolumesBuffer->getSize() : 0;

    s.gridCount = mGrids.size();
    s.gridVoxelCount = 0;
    s.gridMemoryInBytes = 0;

    for (const auto& pGrid : mGrids) {
        s.gridVoxelCount += pGrid->getVoxelCount();
        s.gridMemoryInBytes += pGrid->getGridSizeInBytes();
    }
}

bool Scene::updateAnimatable(Animatable& animatable, const AnimationController& controller, bool force) {
    uint32_t nodeID = animatable.getNodeID();

    // It is possible for this to be called on an object with no associated node in the scene graph (kInvalidNode),
    // e.g. non-animated lights. This check ensures that we return immediately instead of trying to check
    // matrices for a non-existent node.
    if (nodeID == kInvalidNode) return false;

    if (force || (animatable.hasAnimation() && animatable.isAnimated())) {
        if (!controller.isMatrixListChanged(nodeID) && !force) return false;

        const auto& transformList = controller.getGlobalMatrixLists()[nodeID];
        animatable.updateFromAnimation(transformList);
        return true;
    }
    return false;
}

Scene::UpdateFlags Scene::updateSelectedCamera(bool forceUpdate) {
    auto camera = mCameras[mSelectedCamera];

    if (forceUpdate || (camera->hasAnimation() && camera->isAnimated())) {
        updateAnimatable(*camera, *mpAnimationController, forceUpdate);
    } else {
        mpCamCtrl->update();
    }

    UpdateFlags flags = UpdateFlags::None;
    auto cameraChanges = camera->beginFrame();

    if (mCameraSwitched || cameraChanges != Camera::Changes::None) {
        uploadSelectedCamera();
        if (is_set(cameraChanges, Camera::Changes::Movement)) flags |= UpdateFlags::CameraMoved;
        if ((cameraChanges & (~Camera::Changes::Movement)) != Camera::Changes::None) flags |= UpdateFlags::CameraPropertiesChanged;
        if (mCameraSwitched) flags |= UpdateFlags::CameraSwitched;
    }
    mCameraSwitched = false;
    return flags;
}

Scene::UpdateFlags Scene::updateLights(bool forceUpdate) {
    Light::Changes combinedChanges = Light::Changes::None;

    // Animate lights and get list of changes.
    for (const auto& light : mLights) {
        if (light->isActive() || forceUpdate) {
            updateAnimatable(*light, *mpAnimationController, forceUpdate);
        }

        auto changes = light->beginFrame();
        combinedChanges |= changes;
    }

    // Update changed lights.
    uint32_t activeLightIndex = 0;
    mActiveLights.clear();

    for (const auto& light : mLights) {
        if (!light->isActive()) continue;

        mActiveLights.push_back(light);

        auto changes = light->getChanges();
        if (changes != Light::Changes::None || is_set(combinedChanges, Light::Changes::Active) || forceUpdate) {
            // TODO: This is slow since the buffer is not CPU writable. Copy into CPU buffer and upload once instead.
            mpLightsBuffer->setElement(activeLightIndex, light->getData());
        }

        activeLightIndex++;
    }

    if (combinedChanges != Light::Changes::None || forceUpdate) {
        mpSceneBlock["lightCount"] = (uint32_t)mActiveLights.size();
        updateLightStats();
    }


    // Test stuff

    static const std::string kEnvMapSamplersArrayName = "envmapSamplers"; 
    static const std::string kPhySkySamplersArrayName = "physkySamplers"; 

    auto envmapSamplersVar = mpSceneBlock[kEnvMapSamplersArrayName];
    auto physkySamplersVar = mpSceneBlock[kPhySkySamplersArrayName];

    size_t envmapLightSamplerID = 0;
    size_t physkyLightSamplerID = 0;

    for (const auto& light : mLights) {
        switch(light->getType()) {
            case LightType::Env:
                {
                    EnvironmentLight* pLight = dynamic_cast<EnvironmentLight*>(light.get());
                    if(pLight) pLight->getLightSampler()->setShaderData(envmapSamplersVar[envmapLightSamplerID++]);
                }
                break;
            case LightType::PhysSunSky:
                {
                    PhysicalSunSkyLight* pLight = dynamic_cast<PhysicalSunSkyLight*>(light.get());
                    if(pLight) pLight->getLightSampler()->setShaderData(physkySamplersVar[physkyLightSamplerID++]);
                }
                break;
            default:
                break;
        }
    }
    //pEnvLightSamplersBuffer = Buffer::createStructured(mpDevice, mpSceneBlock[kEnvMapSamplersBufferName], (uint32_t)mMeshletPrimIndices.size(), Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
    //pEnvLightSamplersBuffer->setName("Scene::pEnvLightSamplersBuffer");

    //mpSceneBlock->setBuffer(kEnvMapSamplersBufferName, mpGeometryInstancesBuffer);



    // Compute update flags.
    UpdateFlags flags = UpdateFlags::None;
    if (is_set(combinedChanges, Light::Changes::Intensity)) flags |= UpdateFlags::LightIntensityChanged;
    if (is_set(combinedChanges, Light::Changes::Position)) flags |= UpdateFlags::LightsMoved;
    if (is_set(combinedChanges, Light::Changes::Direction)) flags |= UpdateFlags::LightsMoved;
    if (is_set(combinedChanges, Light::Changes::Active)) flags |= UpdateFlags::LightCountChanged;
    const Light::Changes otherChanges = ~(Light::Changes::Intensity | Light::Changes::Position | Light::Changes::Direction | Light::Changes::Active);
    if ((combinedChanges & otherChanges) != Light::Changes::None) flags |= UpdateFlags::LightPropertiesChanged;

    return flags;
}

Scene::UpdateFlags Scene::updateGridVolumes(bool forceUpdate) {
    GridVolume::UpdateFlags combinedUpdates = GridVolume::UpdateFlags::None;

    // Update animations and get combined updates.
    for (const auto& pGridVolume : mGridVolumes) {
        updateAnimatable(*pGridVolume, *mpAnimationController, forceUpdate);
        combinedUpdates |= pGridVolume->getUpdates();
    }

    // Early out if no volumes have changed.
    if (!forceUpdate && combinedUpdates == GridVolume::UpdateFlags::None) return UpdateFlags::None;

    // Upload grids.
    if (forceUpdate) {
        auto var = mpSceneBlock["grids"];
        for (size_t i = 0; i < mGrids.size(); ++i) {
            mGrids[i]->setShaderData(var[i]);
        }
    }

    // Upload volumes and clear updates.
    uint32_t volumeIndex = 0;
    for (const auto& pGridVolume : mGridVolumes) {
        if (forceUpdate || pGridVolume->getUpdates() != GridVolume::UpdateFlags::None) {
            // Fetch copy of volume data.
            auto data = pGridVolume->getData();
            data.densityGrid = pGridVolume->getDensityGrid() ? mGridIDs.at(pGridVolume->getDensityGrid()) : kInvalidGrid;
            data.emissionGrid = pGridVolume->getEmissionGrid() ? mGridIDs.at(pGridVolume->getEmissionGrid()) : kInvalidGrid;
            // Merge grid and volume transforms.
            const auto& densityGrid = pGridVolume->getDensityGrid();
            if (densityGrid) {
                data.transform = data.transform * densityGrid->getTransform();
                data.invTransform = densityGrid->getInvTransform() * data.invTransform;
            }
            mpGridVolumesBuffer->setElement(volumeIndex, data);
        }
        pGridVolume->clearUpdates();
        volumeIndex++;
    }

    mpSceneBlock["gridVolumeCount"] = (uint32_t)mGridVolumes.size();

    UpdateFlags flags = UpdateFlags::None;
    if (is_set(combinedUpdates, GridVolume::UpdateFlags::TransformChanged)) flags |= UpdateFlags::GridVolumesMoved;
    if (is_set(combinedUpdates, GridVolume::UpdateFlags::PropertiesChanged)) flags |= UpdateFlags::GridVolumePropertiesChanged;
    if (is_set(combinedUpdates, GridVolume::UpdateFlags::GridsChanged)) flags |= UpdateFlags::GridVolumeGridsChanged;
    if (is_set(combinedUpdates, GridVolume::UpdateFlags::BoundsChanged)) flags |= UpdateFlags::GridVolumeBoundsChanged;

    return flags;
}

Scene::UpdateFlags Scene::updateEnvMap(bool forceUpdate) {
    UpdateFlags flags = UpdateFlags::None;

    if (mpEnvMap) {
        auto envMapChanges = mpEnvMap->beginFrame();
        if (envMapChanges != EnvMap::Changes::None || mEnvMapChanged || forceUpdate) {
            if (envMapChanges != EnvMap::Changes::None) flags |= UpdateFlags::EnvMapPropertiesChanged;
            mpEnvMap->setShaderData(mpSceneBlock[kEnvMap]);
        }
    }
    mSceneStats.envMapMemoryInBytes = mpEnvMap ? mpEnvMap->getMemoryUsageInBytes() : 0;

    if (mEnvMapChanged) {
        flags |= UpdateFlags::EnvMapChanged;
        mEnvMapChanged = false;
    }

    return flags;
}

Scene::UpdateFlags Scene::updateGeometry(bool forceUpdate) {
    UpdateFlags flags = updateProceduralPrimitives(forceUpdate);
    flags |= updateDisplacement(forceUpdate);

    if (forceUpdate || mCustomPrimitivesChanged) {
        updateGeometryTypes();
        updateGeometryStats();

        // Mark previous BLAS data as invalid. This will trigger a full BLAS/TLAS rebuild.
        // TODO: Support partial rebuild of just the procedural primitives.
        mBlasDataValid = false;
    }

    mCustomPrimitivesMoved = false;
    mCustomPrimitivesChanged = false;
    return flags;
}

Scene::UpdateFlags Scene::update(RenderContext* pContext, double currentTime) {
    // Run scene update callback.
    if (mUpdateCallback) mUpdateCallback(shared_from_this(), currentTime);

    mUpdates = UpdateFlags::None;

    if (mpAnimationController->animate(pContext, currentTime)) {
        mUpdates |= UpdateFlags::SceneGraphChanged;
        if (mpAnimationController->hasSkinnedMeshes()) mUpdates |= UpdateFlags::MeshesChanged;

        for (const auto& inst : mGeometryInstanceData) {
            if (mpAnimationController->isMatrixListChanged(inst.nodeID)) {
                mUpdates |= UpdateFlags::GeometryMoved;
            }
        }

        // We might end up setting the flag even if curves haven't changed (if looping is disabled for example).
        if (mpAnimationController->hasAnimatedCurveCaches()) mUpdates |= UpdateFlags::CurvesMoved;
        if (mpAnimationController->hasAnimatedMeshCaches()) mUpdates |= UpdateFlags::MeshesChanged;
    }

    for (const auto& pGridVolume : mGridVolumes) {
        pGridVolume->updatePlayback(currentTime);
    }

    mUpdates |= updateSelectedCamera(false);
    mUpdates |= updateLights(false);
    mUpdates |= updateLightLinker(false); // Important ! Must be called after lights update !!!
    mUpdates |= updateGridVolumes(false);
    mUpdates |= updateEnvMap(false);
    mUpdates |= updateMaterials(false);
    mUpdates |= updateGeometry(false);
    mUpdates |= updateSDFGrids(pContext);
    pContext->flush();

    if (is_set(mUpdates, UpdateFlags::GeometryMoved)) {
        invalidateTlasCache();
        updateGeometryInstances(false);
    }

    // Update existing BLASes if skinned animation and/or procedural primitives moved.
    bool updateProcedural = is_set(mUpdates, UpdateFlags::CurvesMoved) || is_set(mUpdates, UpdateFlags::CustomPrimitivesMoved);
    bool blasUpdateRequired = is_set(mUpdates, UpdateFlags::MeshesChanged) || updateProcedural;

    if (mBlasDataValid && blasUpdateRequired) {
        invalidateTlasCache();
        buildBlas(pContext);
    }

    // TODO: This is a very simple and straightforward way. We need to make it better, faster and more precise
    if(!mBlasDataValid || is_set(mUpdates, UpdateFlags::MeshesChanged)) {
        mUpdates |= UpdateFlags::MeshletsChanged;
    }

    // Update light collection
    if (mpLightCollection && mpLightCollection->update(pContext)) {
        mUpdates |= UpdateFlags::LightCollectionChanged;
        mSceneStats.emissiveMemoryInBytes = mpLightCollection->getMemoryUsageInBytes();
    }
    else if (!mpLightCollection)
    {
        mSceneStats.emissiveMemoryInBytes = 0;
    }

    if (mRenderSettings != mPrevRenderSettings) {
        mUpdates |= UpdateFlags::RenderSettingsChanged;
        mPrevRenderSettings = mRenderSettings;
    }

    if (mSDFGridConfig != mPrevSDFGridConfig) {
        mUpdates |= UpdateFlags::SDFGridConfigChanged;
        mPrevSDFGridConfig = mSDFGridConfig;
    }

    return mUpdates;
}

bool Scene::useRayTracing() const {
    return mRenderSettings.useRayTracing;
}

bool Scene::useEnvBackground() const {
    return mpEnvMap != nullptr;
}

bool Scene::useEnvLight() const {
    return mRenderSettings.useEnvLight && mpEnvMap != nullptr;
}

bool Scene::useAnalyticLights() const {
    return mRenderSettings.useAnalyticLights && mActiveLights.empty() == false;
}

bool Scene::useEmissiveLights() const {
    return mRenderSettings.useEmissiveLights && mpLightCollection != nullptr && mpLightCollection->getActiveLightCount() > 0;
}

bool Scene::useGridVolumes() const {
    return mRenderSettings.useGridVolumes && mGridVolumes.empty() == false;
}

void Scene::setCamera(const Camera::SharedPtr& pCamera) {
    auto it = std::find(mCameras.begin(), mCameras.end(), pCamera);
    if (it != mCameras.end()) {
        selectCamera((uint32_t)std::distance(mCameras.begin(), it));
    } else if (pCamera) {
        LLOG_WRN << "Selected camera " << pCamera->getName() << " does not exist.";
    }
}

void Scene::selectCamera(uint32_t index) {
    if (index == mSelectedCamera) return;
    if (index >= mCameras.size()) {
        LLOG_WRN << "Selected camera index " << std::to_string(index) << " is invalid.";
        return;
    }

    mSelectedCamera = index;
    mCameraSwitched = true;
    setCameraController(mCamCtrlType);
}

void Scene::resetCamera(bool resetDepthRange) {
    auto camera = getCamera();
    float radius = mSceneBB.radius();
    camera->setPosition(mSceneBB.center());
    camera->setTarget(mSceneBB.center() + float3(0, 0, -1));
    camera->setUpVector(float3(0, 1, 0));

    if (resetDepthRange) {
        float nearZ = std::max(0.1f, radius / 750.0f);
        float farZ = radius * 50;
        camera->setDepthRange(nearZ, farZ);
    }
}

void Scene::setCameraSpeed(float speed) {
    mCameraSpeed = clamp(speed, 0.f, std::numeric_limits<float>::max());
    mpCamCtrl->setCameraSpeed(speed);
}

void Scene::addViewpoint() {
    auto camera = getCamera();
    addViewpoint(camera->getPosition(), camera->getTarget(), camera->getUpVector(), mSelectedCamera);
}

void Scene::addViewpoint(const float3& position, const float3& target, const float3& up, uint32_t cameraIndex) {
    Viewpoint viewpoint = { cameraIndex, position, target, up };
    mViewpoints.push_back(viewpoint);
    mCurrentViewpoint = (uint32_t)mViewpoints.size() - 1;
}

void Scene::removeViewpoint() {
    if (mCurrentViewpoint == 0) {
        LLOG_WRN << "Cannot remove default viewpoint";
        return;
    }
    mViewpoints.erase(mViewpoints.begin() + mCurrentViewpoint);
    mCurrentViewpoint = std::min(mCurrentViewpoint, (uint32_t)mViewpoints.size() - 1);
}

void Scene::selectViewpoint(uint32_t index) {
    if (index >= mViewpoints.size()) {
    LLOG_WRN << "Viewpoint does not exist";
        return;
    }

    auto& viewpoint = mViewpoints[index];
    selectCamera(viewpoint.index);
    auto camera = getCamera();
    camera->setPosition(viewpoint.position);
    camera->setTarget(viewpoint.target);
    camera->setUpVector(viewpoint.up);
    mCurrentViewpoint = index;
}

uint32_t Scene::getGeometryCount() const {
    // The BLASes currently hold the geometries in the order: meshes, curves, custom primitives.
    // We calculate the total number of geometries as the sum of the respective kind.
    size_t totalGeometries = mMeshDesc.size() + mCurveDesc.size() + mCustomPrimitiveDesc.size();
    assert(totalGeometries < std::numeric_limits<uint32_t>::max());
    return (uint32_t)totalGeometries;
}

std::vector<uint32_t> Scene::getGeometryIDs(GeometryType geometryType) const {
    if (!hasGeometryType(geometryType)) return {};

    std::vector<uint32_t> geometryIDs;
    uint32_t geometryCount = getGeometryCount();
    for (uint32_t geometryID = 0; geometryID < geometryCount; geometryID++) {
        if (getGeometryType(geometryID) == geometryType) geometryIDs.push_back(geometryID);
    }
    return geometryIDs;
}

std::vector<uint32_t> Scene::getGeometryIDs(GeometryType geometryType, MaterialType materialType) const {
    if (!hasGeometryType(geometryType)) return {};

    std::vector<uint32_t> geometryIDs;
    uint32_t geometryCount = getGeometryCount();
    for (uint32_t geometryID = 0; geometryID < geometryCount; geometryID++) {
        auto pMaterial = getGeometryMaterial(geometryID);
        if (getGeometryType(geometryID) == geometryType && pMaterial && pMaterial->getType() == materialType) {
            geometryIDs.push_back(geometryID);
        }
    }
    return geometryIDs;
}

Scene::GeometryType Scene::getGeometryType(uint32_t geometryID) const {
    // Map global geometry ID to which type of geometry it represents.
    if (geometryID < mMeshDesc.size()) return mMeshDesc[geometryID].isDisplaced() ? GeometryType::DisplacedTriangleMesh : GeometryType::TriangleMesh;
    else if (geometryID < mMeshDesc.size() + mCurveDesc.size()) return GeometryType::Curve;
    else if (geometryID < mMeshDesc.size() + mCurveDesc.size() + mSDFGridDesc.size()) return GeometryType::SDFGrid;
    else if (geometryID < mMeshDesc.size() + mCurveDesc.size() + mSDFGridDesc.size() + mCustomPrimitiveDesc.size()) return GeometryType::Custom;
    else throw std::runtime_error("'geometryID' is invalid.");
}

uint32_t Scene::getSDFGridGeometryCount() const {
    switch (mSDFGridConfig.implementation) {
        case SDFGrid::Type::None:
            return 0;
        case SDFGrid::Type::NormalizedDenseGrid:
        case SDFGrid::Type::SparseVoxelOctree:
            return mSDFGrids.empty() ? 0 : 1;
        case SDFGrid::Type::SparseVoxelSet:
        case SDFGrid::Type::SparseBrickSet:
            return (uint32_t)mSDFGrids.size();
        default:
            FALCOR_UNREACHABLE();
            return 0;
    }
}

std::vector<uint32_t> Scene::getGeometryInstanceIDsByType(GeometryType type) const {
    std::vector<uint32_t> instanceIDs;
    for (uint32_t i = 0; i < getGeometryInstanceCount(); ++i) {
        const GeometryInstanceData& instanceData = mGeometryInstanceData[i];
        if (instanceData.getType() == type) instanceIDs.push_back(i);
    }
    return instanceIDs;
}

Material::SharedPtr Scene::getGeometryMaterial(uint32_t geometryID) const {
    if (geometryID < mMeshDesc.size()) {
        return mpMaterialSystem->getMaterial(mMeshDesc[geometryID].materialID);
    }
    geometryID -= (uint32_t)mMeshDesc.size();

    if (geometryID < mCurveDesc.size()) {
        return mpMaterialSystem->getMaterial(mCurveDesc[geometryID].materialID);
    }
    geometryID -= (uint32_t)mCurveDesc.size();

    if (geometryID < mSDFGridDesc.size()) {
        return mpMaterialSystem->getMaterial(mSDFGridDesc[geometryID].materialID);
    }
    geometryID -= (uint32_t)mSDFGridDesc.size();

    if (geometryID < mCustomPrimitiveDesc.size()) {
        return nullptr;
    }
    geometryID -= (uint32_t)mCustomPrimitiveDesc.size();

    throw std::runtime_error("'geometryID' is invalid.");
}

uint32_t Scene::getCustomPrimitiveIndex(uint32_t geometryID) const {
    if (getGeometryType(geometryID) != GeometryType::Custom) {
        throw std::runtime_error("Geometry ID is not a custom primitive");
    }

    size_t customPrimitiveOffset = mMeshDesc.size() + mCurveDesc.size();
    assert(geometryID >= (uint32_t)customPrimitiveOffset && geometryID < getGeometryCount());
    return geometryID - (uint32_t)customPrimitiveOffset;
}

const CustomPrimitiveDesc& Scene::getCustomPrimitive(uint32_t index) const {
    if (index >= getCustomPrimitiveCount()) {
        throw std::runtime_error("Custom primitive index " + std::to_string(index) + " is out of range");
    }
    return mCustomPrimitiveDesc[index];
}

const AABB& Scene::getCustomPrimitiveAABB(uint32_t index) const {
    if (index >= getCustomPrimitiveCount()) {
        throw std::runtime_error("Custom primitive index " + std::to_string(index) + " is out of range");
    }
    return mCustomPrimitiveAABBs[index];
}

uint32_t Scene::addCustomPrimitive(uint32_t userID, const AABB& aabb) {
    // Currently each custom primitive has exactly one AABB. This may change in the future.
    assert(mCustomPrimitiveDesc.size() == mCustomPrimitiveAABBs.size());
    if (mCustomPrimitiveAABBs.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Custom primitive count exceeds the maximum");
    }

    const uint32_t index = (uint32_t)mCustomPrimitiveDesc.size();

    CustomPrimitiveDesc desc = {};
    desc.userID = userID;
    desc.aabbOffset = (uint32_t)mCustomPrimitiveAABBs.size();

    mCustomPrimitiveDesc.push_back(desc);
    mCustomPrimitiveAABBs.push_back(aabb);
    mCustomPrimitivesChanged = true;

    return index;
}

void Scene::removeCustomPrimitives(uint32_t first, uint32_t last) {
    if (first > last || last > getCustomPrimitiveCount()) {
        throw std::runtime_error("Invalid custom primitive index range [" + std::to_string(first) + ", " + std::to_string(last) + ")");
    }

    if (first == last) return;

    mCustomPrimitiveDesc.erase(mCustomPrimitiveDesc.begin() + first, mCustomPrimitiveDesc.begin() + last);
    mCustomPrimitiveAABBs.erase(mCustomPrimitiveAABBs.begin() + first, mCustomPrimitiveAABBs.begin() + last);

    // Update AABB offsets for all subsequent primitives.
    // The offset is currently redundant since there is one AABB per primitive. This may change in the future.
    for (uint32_t i = first; i < mCustomPrimitiveDesc.size(); i++) {
        mCustomPrimitiveDesc[i].aabbOffset = i;
    }

    mCustomPrimitivesChanged = true;
}

void Scene::updateCustomPrimitive(uint32_t index, const AABB& aabb) {
    if (index >= getCustomPrimitiveCount()) {
        throw std::runtime_error("Custom primitive index " + std::to_string(index) + " is out of range");
    }

    if (mCustomPrimitiveAABBs[index] != aabb) {
        mCustomPrimitiveAABBs[index] = aabb;
        mCustomPrimitivesMoved = true;
    }
}

GridVolume::SharedPtr Scene::getGridVolumeByName(const std::string& name) const {
    for (const auto& v : mGridVolumes) {
        if (v->getName() == name) return v;
    }

    return nullptr;
}

Light::SharedPtr Scene::getLightByName(const std::string& name) const {
    auto match = std::find_if(mLights.begin(), mLights.end(), [&] (const Light::SharedPtr& l) { return l->getName() == name; });
    if(match != mLights.end()) {
        return *match;
    }
    return nullptr;
}

void Scene::toggleAnimations(bool animate) {
    for (auto& light : mLights) light->setIsAnimated(animate);
    for (auto& camera : mCameras) camera->setIsAnimated(animate);
    mpAnimationController->setEnabled(animate);
}

void Scene::setBlasUpdateMode(UpdateMode mode) {
    if (mode != mBlasUpdateMode) mRebuildBlas = true;
    mBlasUpdateMode = mode;
}

void Scene::createDrawList() {
    LLOG_DBG << "createDrawList() material count " << std::to_string(getMaterialCount());
    // This function creates argument buffers for draw indirect calls to rasterize the scene.
    // The updateMeshInstances() function must have been called before so that the flags are accurate.
    //
    // Note that we create four draw buffers to handle all combinations of:
    // 1) mesh is using 16- or 32-bit indices,
    // 2) mesh triangle winding is CW or CCW after transformation.
    //
    // TODO: Update the draw args if a mesh undergoes animation that flips the winding.

    mDrawArgs.clear();
    mMaterialDrawArgs.clear();
    mMaterialDrawArgs.resize(getMaterialCount());

    for( auto& draws: mMaterialDrawArgs) draws.clear();

    // Helper to create the draw-indirect buffer.
    auto createDrawBuffer = [this](const auto& drawMeshes, bool ccw, bool isDoubleSided, ResourceFormat ibFormat = ResourceFormat::Unknown) {
        if (drawMeshes.empty()) return;
        for (const auto drawMesh: drawMeshes) {
            DrawArgs draw;
            draw.pBuffer = Buffer::create(mpDevice, sizeof(drawMesh), Resource::BindFlags::IndirectArg, Buffer::CpuAccess::None, &drawMesh);
            draw.pBuffer->setName("Scene draw buffer");
            draw.count = 1;//(uint32_t)drawMeshes.size();
            draw.ccw = ccw;
            draw.cullBackface = !isDoubleSided;
            draw.ibFormat = ibFormat;
            mDrawArgs.push_back(draw);
        }
    };

    auto processInstances = [this](const auto& instancesByMaterial, bool isDoubleSided) {
        // Helper to create the draw-indirect buffer.
        auto createDrawBuffer = [this](const auto& drawMeshes, bool ccw, bool isDoubleSided, ResourceFormat ibFormat = ResourceFormat::Unknown) {
            if (drawMeshes.empty()) return;
            for (const auto drawMesh: drawMeshes) {
                DrawArgs draw;
                draw.pBuffer = Buffer::create(mpDevice, sizeof(drawMesh), Resource::BindFlags::IndirectArg, Buffer::CpuAccess::None, &drawMesh);
                draw.pBuffer->setName("Scene draw buffer");
                draw.count = 1;//(uint32_t)drawMeshes.size();
                draw.ccw = ccw;
                draw.cullBackface = !isDoubleSided;
                draw.ibFormat = ibFormat;
                mDrawArgs.push_back(draw);
            }
        };
        
        for (size_t materialID = 0; materialID < instancesByMaterial.size(); materialID++) {
            const auto& instances = instancesByMaterial[materialID];
            LLOG_DBG << "Scene::createDrawList() preapre draw for " << std::to_string(instances.size()) << " instances for materialID " << std::to_string(materialID);

            if (hasIndexBuffer()) {
                std::vector<DrawIndexedArguments> drawClockwiseMeshes[2], drawCounterClockwiseMeshes[2];

                //uint32_t instanceID = 0;
                for (const auto& drawInstance : instances) {
                    const auto instance = drawInstance.instance;
                    const auto& mesh = mMeshDesc[instance->geometryID];
                    bool use16Bit = mesh.use16BitIndices();

                    DrawIndexedArguments draw;
                    draw.IndexCountPerInstance = mesh.indexCount;
                    draw.InstanceCount = 1;
                    draw.StartIndexLocation = mesh.ibOffset * (use16Bit ? 2 : 1);
                    draw.BaseVertexLocation = mesh.vbOffset;
                    
                    draw.StartInstanceLocation = drawInstance.instanceID;
                    draw.MaterialID = materialID; //instance->materialID;

                    int i = use16Bit ? 0 : 1;
                    (instance->isWorldFrontFaceCW()) ? drawClockwiseMeshes[i].push_back(draw) : drawCounterClockwiseMeshes[i].push_back(draw);
                }

                createDrawBuffer(drawClockwiseMeshes[0], false, isDoubleSided, ResourceFormat::R16Uint);
                createDrawBuffer(drawClockwiseMeshes[1], false, isDoubleSided, ResourceFormat::R32Uint);
                createDrawBuffer(drawCounterClockwiseMeshes[0], true, isDoubleSided, ResourceFormat::R16Uint);
                createDrawBuffer(drawCounterClockwiseMeshes[1], true, isDoubleSided, ResourceFormat::R32Uint);
            } else {
                std::vector<DrawArguments> drawClockwiseMeshes, drawCounterClockwiseMeshes;

                for (const auto& drawInstance : instances) {
                    const auto instance = drawInstance.instance;
                    const auto& mesh = mMeshDesc[instance->geometryID];
                    assert(mesh.indexCount == 0);

                    DrawArguments draw;
                    draw.VertexCountPerInstance = mesh.vertexCount;
                    draw.InstanceCount = 1;
                    draw.StartVertexLocation = mesh.vbOffset;
                    
                    draw.StartInstanceLocation = drawInstance.instanceID;
                    draw.MaterialID = materialID; //instance->materialID;

                    (instance->isWorldFrontFaceCW()) ? drawClockwiseMeshes.push_back(draw) : drawCounterClockwiseMeshes.push_back(draw);
                }

                createDrawBuffer(drawClockwiseMeshes, false, isDoubleSided);
                createDrawBuffer(drawCounterClockwiseMeshes, true, isDoubleSided);
            }
        }
    };

    // Sort all instances by their sidedness and materials
    std::vector<std::vector<DrawInstance>> doubleSidedInstancesByMaterial;
    std::vector<std::vector<DrawInstance>> singleSidedInstancesByMaterial;
    doubleSidedInstancesByMaterial.resize(getMaterialCount());
    singleSidedInstancesByMaterial.resize(getMaterialCount());

    LLOG_DBG << "Scene::createDrawList() Number of materials to draw: " << std::to_string(getMaterialCount());
    LLOG_DBG << "Scene::createDrawList() mGeometryInstanceData size: " << std::to_string(mGeometryInstanceData.size());

    uint32_t instanceID = 0;
    for (size_t i = 0; i < mGeometryInstanceData.size(); i++) {
        // Push only primary visible instances
        if (((uint32_t)mGeometryInstanceData[i].flags & (uint32_t)GeometryInstanceFlags::VisibleToPrimaryRays) != 0x0) {
            if (((uint32_t)mGeometryInstanceData[i].flags & (uint32_t)GeometryInstanceFlags::DoubleSided) != 0x0) {
                doubleSidedInstancesByMaterial[mGeometryInstanceData[i].materialID].push_back({instanceID, &mGeometryInstanceData[i]});
            } else {
                singleSidedInstancesByMaterial[mGeometryInstanceData[i].materialID].push_back({instanceID, &mGeometryInstanceData[i]});
            }
        }
        instanceID++;
    }

    // Now prepare draws
    processInstances(doubleSidedInstancesByMaterial, true);
    processInstances(singleSidedInstancesByMaterial, false);

    LLOG_TRC << "Scene::createDrawList() mDrawArgs size: " << std::to_string(mDrawArgs.size());
}

void Scene::initGeomDesc(RenderContext* pContext) {
    // This function initializes all geometry descs to prepare for BLAS build.
    // If the scene has no geometries the 'mBlasData' array will be left empty.

    // First compute total number of BLASes to build:
    // - Triangle meshes have been grouped beforehand and we build one BLAS per mesh group.
    // - Curves and procedural primitives are currently placed in a single BLAS each, if they exist.
    // - SDF grids are placed in individual BLASes.
    const uint32_t totalBlasCount = (uint32_t)mMeshGroups.size() + (mCurveDesc.empty() ? 0 : 1) + getSDFGridGeometryCount() + (mCustomPrimitiveDesc.empty() ? 0 : 1);

    mBlasData.clear();
    mBlasData.resize(totalBlasCount);
    mRebuildBlas = true;

    if (!mMeshGroups.empty()) {
        assert(mpMeshVao);
        const VertexBufferLayout::SharedConstPtr& pVbLayout = mpMeshVao->getVertexLayout()->getBufferLayout(kStaticDataBufferIndex);
        const Buffer::SharedPtr& pVb = mpMeshVao->getVertexBuffer(kStaticDataBufferIndex);
        const Buffer::SharedPtr& pIb = mpMeshVao->getIndexBuffer();
        const auto& globalMatrixLists = mpAnimationController->getGlobalMatrixLists();

        // Normally static geometry is already pre-transformed to world space by the SceneBuilder,
        // but if that isn't the case, we let DXR transform static geometry as part of the BLAS build.
        // For this we need the GPU address of the transform matrix of each mesh in row-major format.
        // Since glm uses column-major format we lazily create a buffer with the transposed matrices.
        // Note that this is sufficient to do once only as the transforms for static meshes can't change.
        // TODO: Use AnimationController's matrix buffer directly when we've switched to a row-major matrix library.
        auto getStaticMatricesBuffer = [&]() {
            if (!mpBlasStaticWorldMatrices) {
                std::vector<glm::mat4> transposedMatrices;
                transposedMatrices.reserve(mpAnimationController->getGlobalMatricesCount());
                for(const auto& matrixList: globalMatrixLists) {
                    for(const auto& m : matrixList) transposedMatrices.push_back(glm::transpose(m));
                }    

                uint32_t float4Count = (uint32_t)transposedMatrices.size() * 4;
                mpBlasStaticWorldMatrices = Buffer::createStructured(mpDevice, sizeof(float4), float4Count, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, transposedMatrices.data(), false);
                mpBlasStaticWorldMatrices->setName("Scene::mpBlasStaticWorldMatrices");

                // Transition the resource to non-pixel shader state as expected by DXR.
                pContext->resourceBarrier(mpBlasStaticWorldMatrices.get(), Resource::State::NonPixelShader);
            }
            return mpBlasStaticWorldMatrices;
        };

        // Iterate over the mesh groups. One BLAS will be created for each group.
        // Each BLAS may contain multiple geometries.
        for (size_t i = 0; i < mMeshGroups.size(); i++) {
            const auto& meshList = mMeshGroups[i].meshList;
            const bool isStatic = mMeshGroups[i].isStatic;
            const bool isDisplaced = mMeshGroups[i].isDisplaced;
            auto& blas = mBlasData[i];
            auto& geomDescs = blas.geomDescs;
            geomDescs.resize(meshList.size());
            blas.hasProceduralPrimitives = false;

            // Track what types of triangle winding exist in the final BLAS.
            // The SceneBuilder should have ensured winding is consistent, but keeping the check here as a safeguard.
            uint32_t triangleWindings = 0; // bit 0 indicates CW, bit 1 CCW.

            for (size_t j = 0; j < meshList.size(); j++) {
                const uint32_t meshID = meshList[j];
                const MeshDesc& mesh = mMeshDesc[meshID];
                bool frontFaceCW = mesh.isFrontFaceCW();
                blas.hasDynamicMesh |= mesh.isDynamic();

                RtGeometryDesc& desc = geomDescs[j];

                if (!isDisplaced) {
                    desc.type = RtGeometryType::Triangles;
                    desc.content.triangles.transform3x4 = 0; // The default is no transform

                    if (isStatic) {
                        // Static meshes will be pre-transformed when building the BLAS.
                        // Lookup the matrix ID here. If it is an identity matrix, no action is needed.
                        assert(mMeshIdToInstanceIds[meshID].size() == 1);
                        uint32_t instanceID = mMeshIdToInstanceIds[meshID][0];
                        assert(instanceID < mGeometryInstanceData.size());
                        auto const& inst = mGeometryInstanceData[instanceID];
                        uint32_t nodeID = inst.nodeID;

                        assert(nodeID < globalMatrixLists.size());
                        static const std::vector<glm::mat4> defaultList = {glm::identity<glm::mat4>()};
                        if (globalMatrixLists[nodeID] != defaultList) {
                            // Get the GPU address of the transform in row-major format.
                            desc.content.triangles.transform3x4 = getStaticMatricesBuffer()->getGpuAddress() + inst.globalMatrixOffset * 64ull;
                            if (glm::determinant(globalMatrixLists[nodeID][0]) < 0.f) frontFaceCW = !frontFaceCW;
                        }
                    }
                    triangleWindings |= frontFaceCW ? 1 : 2;

                    // If this is an opaque mesh, set the opaque flag
                    auto pMaterial = mpMaterialSystem->getMaterial(mesh.materialID);
                    desc.flags = pMaterial->isOpaque() ? RtGeometryFlags::Opaque : RtGeometryFlags::None;

                    // Set the position data
                    desc.content.triangles.vertexData = pVb->getGpuAddress() + (mesh.vbOffset * pVbLayout->getStride());
                    desc.content.triangles.vertexStride = pVbLayout->getStride();
                    desc.content.triangles.vertexCount = mesh.vertexCount;
                    desc.content.triangles.vertexFormat = pVbLayout->getElementFormat(0);

                    // Set index data
                    if (pIb) {
                        // The global index data is stored in a dword array.
                        // Each mesh specifies whether its indices are in 16-bit or 32-bit format.
                        ResourceFormat ibFormat = mesh.use16BitIndices() ? ResourceFormat::R16Uint : ResourceFormat::R32Uint;
                        desc.content.triangles.indexData = pIb->getGpuAddress() + mesh.ibOffset * sizeof(uint32_t);
                        desc.content.triangles.indexCount = mesh.indexCount;
                        desc.content.triangles.indexFormat = ibFormat;
                    } else {
                        assert(mesh.indexCount == 0);
                        desc.content.triangles.indexData = NULL;
                        desc.content.triangles.indexCount = 0;
                        desc.content.triangles.indexFormat = ResourceFormat::Unknown;
                    }
                } else {
                    // Displaced triangle mesh, requires custom intersection.
                    desc.type = RtGeometryType::ProcedurePrimitives;
                    desc.flags = RtGeometryFlags::Opaque;

                    desc.content.proceduralAABBs.count = mDisplacement.meshData[meshID].AABBCount;
                    uint64_t bbStartOffset = mDisplacement.meshData[meshID].AABBOffset * sizeof(RtAABB);
                    desc.content.proceduralAABBs.data = mDisplacement.pAABBBuffer->getGpuAddress() + bbStartOffset;
                    desc.content.proceduralAABBs.stride = sizeof(RtAABB);
                }
            }

            assert(!(isStatic && blas.hasDynamicMesh));

            if (triangleWindings == 0x3) {
                LLOG_WRN << "Mesh group " << std::to_string(i) << " has mixed triangle winding. Back/front face culling won't work correctly.";
            }
        }
    }

    // Procedural primitives other than displaced triangle meshes and SDF grids are placed in two BLASes at the end.
    // The geometries in these BLASes are using the following layout:
    //
    //  +----------+----------+-----+----------+
    //  |          |          |     |          |
    //  |  Curve0  |  Curve1  | ... |  CurveM  |
    //  |          |          |     |          |
    //  |          |          |     |          |
    //  +----------+----------+-----+----------+
    // SDF grids either create a shared BLAS or one BLAS per SDF grid:
    //  +----------+          +----------+ +----------+     +----------+
    //  |          |          |          | |          |     |          |
    //  | SDFGrid  |          | SDFGrid0 | | SDFGrid1 | ... | SDFGridN |
    //  |  Shared  |    or    |          | |          |     |          |
    //  | Geometry |          |          | |          |     |          |
    //  +----------+          +----------+ +----------+     +----------+
    //
    //  +----------+----------+-----+----------+
    //  |          |          |     |          |
    //  |  Custom  |  Custom  | ... |  Custom  |
    //  |  Prim0   |  Prim1   |     |  PrimN   |
    //  |          |          |     |          |
    //  +----------+----------+-----+----------+
    //
    // Each procedural primitive indexes a range of AABBs in a global AABB buffer.
    //
    size_t blasDataIndex = mMeshGroups.size();
    uint64_t bbAddressOffset = 0;
    if (!mCurveDesc.empty()) {
        assert(mpRtAABBBuffer && mpRtAABBBuffer->getElementCount() >= mRtAABBRaw.size());

        auto& blas = mBlasData[blasDataIndex++];
        blas.geomDescs.resize(mCurveDesc.size());
        blas.hasProceduralPrimitives = true;
        blas.hasDynamicCurve |= mpAnimationController->hasAnimatedCurveCaches();

        uint32_t geomIndexOffset = 0;

        for (const auto& curve : mCurveDesc) {
            // One geometry desc per curve.
            RtGeometryDesc& desc = blas.geomDescs[geomIndexOffset++];

            desc.type = RtGeometryType::ProcedurePrimitives;
            desc.flags = RtGeometryFlags::Opaque;
            desc.content.proceduralAABBs.count = curve.indexCount;
            desc.content.proceduralAABBs.data = mpRtAABBBuffer->getGpuAddress() + bbAddressOffset;
            desc.content.proceduralAABBs.stride = sizeof(RtAABB);

            bbAddressOffset += sizeof(RtAABB) * curve.indexCount;
        }
    }

    if (!mSDFGrids.empty()) {
        if (mSDFGridConfig.implementation == SDFGrid::Type::NormalizedDenseGrid ||
            mSDFGridConfig.implementation == SDFGrid::Type::SparseVoxelOctree)
        {
            // All ND SDF Grid instances share the same BLAS and AABB buffer.
            const SDFGrid::SharedPtr& pSDFGrid = mSDFGrids.back();

            auto& blas = mBlasData[blasDataIndex];
            blas.hasProceduralPrimitives = true;
            blas.geomDescs.resize(1);

            RtGeometryDesc& desc = blas.geomDescs.back();
            desc.type = RtGeometryType::ProcedurePrimitives;
            desc.flags = RtGeometryFlags::Opaque;
            desc.content.proceduralAABBs.count = pSDFGrid->getAABBCount();
            desc.content.proceduralAABBs.data = pSDFGrid->getAABBBuffer()->getGpuAddress();
            desc.content.proceduralAABBs.stride = sizeof(RtAABB);

            blasDataIndex++;
        }
        else if (mSDFGridConfig.implementation == SDFGrid::Type::SparseVoxelSet ||
                 mSDFGridConfig.implementation == SDFGrid::Type::SparseBrickSet)
        {
            for (uint32_t s = 0; s < mSDFGrids.size(); s++) {
                const SDFGrid::SharedPtr& pSDFGrid = mSDFGrids[s];

                auto& blas = mBlasData[blasDataIndex + s];
                blas.hasProceduralPrimitives = true;
                blas.geomDescs.resize(1);

                RtGeometryDesc& desc = blas.geomDescs.back();
                desc.type = RtGeometryType::ProcedurePrimitives;
                desc.flags = RtGeometryFlags::Opaque;
                desc.content.proceduralAABBs.count = pSDFGrid->getAABBCount();
                desc.content.proceduralAABBs.data = pSDFGrid->getAABBBuffer()->getGpuAddress();
                desc.content.proceduralAABBs.stride = sizeof(RtAABB);

                assert(desc.content.proceduralAABBs.count > 0);
            }

            blasDataIndex += mSDFGrids.size();
        }
    }

    if (!mCustomPrimitiveDesc.empty()) {
        assert(mpRtAABBBuffer && mpRtAABBBuffer->getElementCount() >= mRtAABBRaw.size());

        auto& blas = mBlasData.back();
        blas.geomDescs.resize(mCustomPrimitiveDesc.size());
        blas.hasProceduralPrimitives = true;

        uint32_t geomIndexOffset = 0;

        for (const auto& customPrim : mCustomPrimitiveDesc) {
            RtGeometryDesc& desc = blas.geomDescs[geomIndexOffset++];
            desc.type = RtGeometryType::ProcedurePrimitives;
            desc.flags = RtGeometryFlags::None;

            desc.content.proceduralAABBs.count = 1; // Currently only one AABB per user-defined prim supported
            desc.content.proceduralAABBs.data = mpRtAABBBuffer->getGpuAddress() + bbAddressOffset;
            desc.content.proceduralAABBs.stride = sizeof(RtAABB);

            bbAddressOffset += sizeof(RtAABB);
        }
    }

    // Verify that the total geometry count matches the expectation.
    size_t totalGeometries = 0;
    for (const auto& blas : mBlasData) totalGeometries += blas.geomDescs.size();
    if (totalGeometries != getGeometryCount()) throw std::runtime_error("Total geometry count mismatch");

    mBlasDataValid = true;
}

void Scene::preparePrebuildInfo(RenderContext* pContext) {
    for (auto& blas : mBlasData) {
        // Determine how BLAS build/update should be done.
        // The default choice is to compact all static BLASes and those that don't need to be rebuilt every frame.
        // For all other BLASes, compaction just adds overhead.
        // TODO: Add compaction on/off switch for profiling.
        // TODO: Disable compaction for skinned meshes if update performance becomes a problem.
        blas.updateMode = mBlasUpdateMode;
        blas.useCompaction = (!blas.hasDynamicGeometry()) || blas.updateMode != UpdateMode::Rebuild;

        // Setup build parameters.
        RtAccelerationStructureBuildInputs& inputs = blas.buildInputs;
        inputs.kind = RtAccelerationStructureKind::BottomLevel;
        inputs.descCount = (uint32_t)blas.geomDescs.size();
        inputs.geometryDescs = blas.geomDescs.data();
        inputs.flags = RtAccelerationStructureBuildFlags::None;

        // Add necessary flags depending on settings.
        if (blas.useCompaction) {
            inputs.flags |= RtAccelerationStructureBuildFlags::AllowCompaction;
        }
        if ((blas.hasDynamicGeometry() || blas.hasProceduralPrimitives) && blas.updateMode == UpdateMode::Refit) {
            inputs.flags |= RtAccelerationStructureBuildFlags::AllowUpdate;
        }
        // Set optional performance hints.
        // TODO: Set FAST_BUILD for skinned meshes if update/rebuild performance becomes a problem.
        // TODO: Add FAST_TRACE on/off switch for profiling. It is disabled by default as it is scene-dependent.
        //if (!blas.hasSkinnedMesh)
        //{
        //    inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        //}

        if (blas.hasDynamicGeometry()) {
            inputs.flags |= RtAccelerationStructureBuildFlags::PreferFastBuild;
        }

        // Get prebuild info.
        blas.prebuildInfo = RtAccelerationStructure::getPrebuildInfo(mpDevice, inputs);

        // Figure out the padded allocation sizes to have proper alignment.
        assert(blas.prebuildInfo.resultDataMaxSize > 0);
        blas.resultByteSize = align_to(kAccelerationStructureByteAlignment, blas.prebuildInfo.resultDataMaxSize);

        uint64_t scratchByteSize = std::max(blas.prebuildInfo.scratchDataSize, blas.prebuildInfo.updateScratchDataSize);
        blas.scratchByteSize = align_to(kAccelerationStructureByteAlignment, scratchByteSize);
    }
}

void Scene::computeBlasGroups() {
    mBlasGroups.clear();
    uint64_t groupSize = 0;

    for (uint32_t blasId = 0; blasId < mBlasData.size(); blasId++) {
        auto& blasData = mBlasData[blasId];
        size_t blasSize = blasData.resultByteSize + blasData.scratchByteSize;

        // Start new BLAS group on first iteration or if group size would exceed the target.
        if (groupSize == 0 || groupSize + blasSize > kMaxBLASBuildMemory) {
            mBlasGroups.push_back({});
            groupSize = 0;
        }

        // Add BLAS to current group.
        assert(mBlasGroups.size() > 0);
        auto& group = mBlasGroups.back();
        group.blasIndices.push_back(blasId);
        blasData.blasGroupIndex = (uint32_t)mBlasGroups.size() - 1;

        // Update data offsets and sizes.
        blasData.resultByteOffset = group.resultByteSize;
        blasData.scratchByteOffset = group.scratchByteSize;
        group.resultByteSize += blasData.resultByteSize;
        group.scratchByteSize += blasData.scratchByteSize;

        groupSize += blasSize;
    }

    // Validation that all offsets and sizes are correct.
    uint64_t totalResultSize = 0;
    uint64_t totalScratchSize = 0;
    std::set<uint32_t> blasIDs;

    for (size_t blasGroupIndex = 0; blasGroupIndex < mBlasGroups.size(); blasGroupIndex++) {
        uint64_t resultSize = 0;
        uint64_t scratchSize = 0;

        const auto& group = mBlasGroups[blasGroupIndex];
        assert(!group.blasIndices.empty());

        for (auto blasId : group.blasIndices) {
            assert(blasId < mBlasData.size());
            const auto& blasData = mBlasData[blasId];

            assert(blasIDs.insert(blasId).second);
            assert(blasData.blasGroupIndex == blasGroupIndex);

            assert(blasData.resultByteSize > 0);
            assert(blasData.resultByteOffset == resultSize);
            resultSize += blasData.resultByteSize;

            assert(blasData.scratchByteSize > 0);
            assert(blasData.scratchByteOffset == scratchSize);
            scratchSize += blasData.scratchByteSize;

            assert(blasData.blasByteOffset == 0);
            assert(blasData.blasByteSize == 0);
        }

        assert(resultSize == group.resultByteSize);
        assert(scratchSize == group.scratchByteSize);
    }
    assert(blasIDs.size() == mBlasData.size());
}

void Scene::buildBlas(RenderContext* pContext) {
    PROFILE(mpDevice, "buildBlas");

    if (!mBlasDataValid) throw std::runtime_error("buildBlas() BLAS data is invalid");
    if (!mpDevice->isFeatureSupported(Device::SupportedFeatures::Raytracing)) {
        throw std::runtime_error("Raytracing is not supported by the current device");
    }

    // Add barriers for the VB and IB which will be accessed by the build.
    if (mpMeshVao) {
        const Buffer::SharedPtr& pVb = mpMeshVao->getVertexBuffer(kStaticDataBufferIndex);
        const Buffer::SharedPtr& pIb = mpMeshVao->getIndexBuffer();
        pContext->resourceBarrier(pVb.get(), Resource::State::NonPixelShader);
        if (pIb) pContext->resourceBarrier(pIb.get(), Resource::State::NonPixelShader);
    }

    if (mpCurveVao) {
        const Buffer::SharedPtr& pCurveVb = mpCurveVao->getVertexBuffer(kStaticDataBufferIndex);
        const Buffer::SharedPtr& pCurveIb = mpCurveVao->getIndexBuffer();
        pContext->resourceBarrier(pCurveVb.get(), Resource::State::NonPixelShader);
        pContext->resourceBarrier(pCurveIb.get(), Resource::State::NonPixelShader);
    }

    if (!mSDFGrids.empty()) {
        if (mSDFGridConfig.implementation == SDFGrid::Type::NormalizedDenseGrid ||
            mSDFGridConfig.implementation == SDFGrid::Type::SparseVoxelOctree)
        {
            pContext->resourceBarrier(mSDFGrids.back()->getAABBBuffer().get(), Resource::State::NonPixelShader);
        }
        else if (mSDFGridConfig.implementation == SDFGrid::Type::SparseVoxelSet ||
                 mSDFGridConfig.implementation == SDFGrid::Type::SparseBrickSet)
        {
            for (const SDFGrid::SharedPtr& pSDFGrid : mSDFGrids) {
                pContext->resourceBarrier(pSDFGrid->getAABBBuffer().get(), Resource::State::NonPixelShader);
            }
        }
    }

    if (mpRtAABBBuffer) {
        pContext->resourceBarrier(mpRtAABBBuffer.get(), Resource::State::NonPixelShader);
    }

    // On the first time, or if a full rebuild is necessary we will:
    // - Update all build inputs and prebuild info
    // - Compute BLAS groups
    // - Calculate total intermediate buffer sizes
    // - Build all BLASes into an intermediate buffer
    // - Calculate total compacted buffer size
    // - Compact/clone all BLASes to their final location

    if (mRebuildBlas) {
        // Invalidate any previous TLASes as they won't be valid anymore.
        invalidateTlasCache();

        if (mBlasData.empty()) {
            LLOG_INF << "Skipping BLAS build due to no geometries";

            mBlasGroups.clear();
            mBlasObjects.clear();
        } else {
            LLOG_INF << "Initiating BLAS build for " << std::to_string(mBlasData.size()) << " mesh groups";

            // Compute pre-build info per BLAS and organize the BLASes into groups
            // in order to limit GPU memory usage during BLAS build.
            preparePrebuildInfo(pContext);
            computeBlasGroups();

            LLOG_INF << "BLAS build split into " << std::to_string(mBlasGroups.size()) << " groups";

            // Compute the required maximum size of the result and scratch buffers.
            uint64_t resultByteSize = 0;
            uint64_t scratchByteSize = 0;
            size_t maxBlasCount = 0;

            for (const auto& group : mBlasGroups) {
                resultByteSize = std::max(resultByteSize, group.resultByteSize);
                scratchByteSize = std::max(scratchByteSize, group.scratchByteSize);
                maxBlasCount = std::max(maxBlasCount, group.blasIndices.size());
            }
            assert(resultByteSize > 0 && scratchByteSize > 0);

            LLOG_INF << "BLAS build result buffer size: " << formatByteSize(resultByteSize);
            LLOG_INF << "BLAS build scratch buffer size: " << formatByteSize(scratchByteSize);

            // Allocate result and scratch buffers.
            // The scratch buffer we'll retain because it's needed for subsequent rebuilds and updates.
            // TODO: Save memory by reducing the scratch buffer to the minimum required for the dynamic objects.
            if (mpBlasScratch == nullptr || mpBlasScratch->getSize() < scratchByteSize) {
                mpBlasScratch = Buffer::create(mpDevice, scratchByteSize, Buffer::BindFlags::UnorderedAccess, Buffer::CpuAccess::None);
                mpBlasScratch->setName("Scene::mpBlasScratch");
            }

            Buffer::SharedPtr pResultBuffer = Buffer::create(mpDevice, resultByteSize, Buffer::BindFlags::AccelerationStructure, Buffer::CpuAccess::None);
            assert(pResultBuffer && mpBlasScratch);

            // Create post-build info pool for readback.
            RtAccelerationStructurePostBuildInfoPool::Desc compactedSizeInfoPoolDesc;
            compactedSizeInfoPoolDesc.queryType = RtAccelerationStructurePostBuildInfoQueryType::CompactedSize;
            compactedSizeInfoPoolDesc.elementCount = (uint32_t)maxBlasCount;
            RtAccelerationStructurePostBuildInfoPool::SharedPtr compactedSizeInfoPool = RtAccelerationStructurePostBuildInfoPool::create(mpDevice, compactedSizeInfoPoolDesc);

            RtAccelerationStructurePostBuildInfoPool::Desc currentSizeInfoPoolDesc;
            currentSizeInfoPoolDesc.queryType = RtAccelerationStructurePostBuildInfoQueryType::CurrentSize;
            currentSizeInfoPoolDesc.elementCount = (uint32_t)maxBlasCount;
            RtAccelerationStructurePostBuildInfoPool::SharedPtr currentSizeInfoPool = RtAccelerationStructurePostBuildInfoPool::create(mpDevice, currentSizeInfoPoolDesc);

            bool hasDynamicGeometry = false;
            bool hasProceduralPrimitives = false;

            mBlasObjects.resize(mBlasData.size());

            // Iterate over BLAS groups. For each group build and compact all BLASes.
            for (size_t blasGroupIndex = 0; blasGroupIndex < mBlasGroups.size(); blasGroupIndex++) {
                auto& group = mBlasGroups[blasGroupIndex];

                // Allocate array to hold intermediate blases for the group.
                std::vector<RtAccelerationStructure::SharedPtr> intermediateBlases(group.blasIndices.size());

                // Insert barriers. The buffers are now ready to be written.
                pContext->uavBarrier(pResultBuffer.get());
                pContext->uavBarrier(mpBlasScratch.get());

                // Reset the post-build info pools to receive new info.
                compactedSizeInfoPool->reset(pContext);
                currentSizeInfoPool->reset(pContext);

                // Build the BLASes into the intermediate result buffer.
                // We output post-build info in order to find out the final size requirements.
                for (size_t i = 0; i < group.blasIndices.size(); ++i) {
                    const uint32_t blasId = group.blasIndices[i];
                    const auto& blas = mBlasData[blasId];

                    hasDynamicGeometry |= blas.hasDynamicGeometry();
                    hasProceduralPrimitives |= blas.hasProceduralPrimitives;

                    RtAccelerationStructure::Desc createDesc = {};
                    createDesc.setBuffer(pResultBuffer, blas.resultByteOffset, blas.resultByteSize);
                    createDesc.setKind(RtAccelerationStructureKind::BottomLevel);
                    auto blasObject = RtAccelerationStructure::create(mpDevice, createDesc);
                    intermediateBlases[i] = blasObject;

                    RtAccelerationStructure::BuildDesc asDesc = {};
                    asDesc.inputs = blas.buildInputs;
                    asDesc.scratchData = mpBlasScratch->getGpuAddress() + blas.scratchByteOffset;
                    asDesc.dest = blasObject.get();

                    // Need to find out the post-build compacted BLAS size to know the final allocation size.
                    RtAccelerationStructurePostBuildInfoDesc postbuildInfoDesc = {};
                    if (blas.useCompaction) {
                        postbuildInfoDesc.type = RtAccelerationStructurePostBuildInfoQueryType::CompactedSize;
                        postbuildInfoDesc.index = (uint32_t)i;
                        postbuildInfoDesc.pool = compactedSizeInfoPool.get();
                    } else {
                        postbuildInfoDesc.type = RtAccelerationStructurePostBuildInfoQueryType::CurrentSize;
                        postbuildInfoDesc.index = (uint32_t)i;
                        postbuildInfoDesc.pool = currentSizeInfoPool.get();
                    }

                    LLOG_DBG << "Acceleration structure build started...";
                    pContext->buildAccelerationStructure(asDesc, 1, &postbuildInfoDesc);
                    LLOG_DBG << "Acceleration structure build done.";
                }

                // Read back the calculated final size requirements for each BLAS.

                group.finalByteSize = 0;
                for (size_t i = 0; i < group.blasIndices.size(); i++) {
                    const uint32_t blasId = group.blasIndices[i];
                    auto& blas = mBlasData[blasId];

                    LLOG_TRC << "Calculating " << ( blas.useCompaction ? "compacted":"") << " blas size...";
                    // Check the size. Upon failure a zero size may be reported.
                    uint64_t byteSize = 0;
                    if (blas.useCompaction) {
                        byteSize = compactedSizeInfoPool->getElement(pContext, (uint32_t)i);
                    } else {
                        byteSize = currentSizeInfoPool->getElement(pContext, (uint32_t)i);
                        // For platforms that does not support current size query, use prebuild size.
                        if (byteSize == 0) {
                            byteSize = blas.prebuildInfo.resultDataMaxSize;
                        }
                    }
                    assert(byteSize <= blas.prebuildInfo.resultDataMaxSize);
                    
                    LLOG_TRC << "Calculated blas size is " << byteSize;

                    if (byteSize == 0) throw std::runtime_error("Acceleration structure build failed for BLAS index (byteSize == 0)" + std::to_string(blasId));

                    blas.blasByteSize = align_to(kAccelerationStructureByteAlignment, byteSize);
                    blas.blasByteOffset = group.finalByteSize;
                    group.finalByteSize += blas.blasByteSize;
                }
                assert(group.finalByteSize > 0);

                LLOG_INF << "BLAS group " << std::to_string(blasGroupIndex) << " final size: " << formatByteSize(group.finalByteSize);

                // Allocate final BLAS buffer.
                auto& pBlas = group.pBlas;
                if (pBlas == nullptr || pBlas->getSize() < group.finalByteSize) {
                    pBlas = Buffer::create(mpDevice, group.finalByteSize, Buffer::BindFlags::AccelerationStructure, Buffer::CpuAccess::None);
                    pBlas->setName("Scene::mBlasGroups[" + std::to_string(blasGroupIndex) + "].pBlas");
                } else {
                    // If we didn't need to reallocate, just insert a barrier so it's safe to use.
                    pContext->uavBarrier(pBlas.get());
                }

                // Insert barrier. The result buffer is now ready to be consumed.
                // TOOD: This is probably not necessary since we flushed above, but it's not going to hurt.
                pContext->uavBarrier(pResultBuffer.get());

                // Compact/clone all BLASes to their final location.
                for (size_t i = 0; i < group.blasIndices.size(); ++i) {
                    const uint32_t blasId = group.blasIndices[i];
                    auto& blas = mBlasData[blasId];

                    RtAccelerationStructure::Desc blasDesc = {};
                    blasDesc.setBuffer(pBlas, blas.blasByteOffset, blas.blasByteSize);
                    blasDesc.setKind(RtAccelerationStructureKind::BottomLevel);
                    mBlasObjects[blasId] = RtAccelerationStructure::create(mpDevice, blasDesc);

                    pContext->copyAccelerationStructure(
                        mBlasObjects[blasId].get(),
                        intermediateBlases[i].get(),
                        blas.useCompaction ? RenderContext::RtAccelerationStructureCopyMode::Compact : RenderContext::RtAccelerationStructureCopyMode::Clone);
                }

                // Insert barrier. The BLAS buffer is now ready for use.
                pContext->uavBarrier(pBlas.get());
            }

            // Release scratch buffer if there is no animated content. We will not need it.
            if (!hasDynamicGeometry && !hasProceduralPrimitives) mpBlasScratch.reset();
        }

        updateRaytracingBLASStats();
        mRebuildBlas = false;
        return;
    }

    // If we get here, all BLASes have previously been built and compacted. We will:
    // - Skip the ones that have no animated geometries.
    // - Update or rebuild in-place the ones that are animated.

    assert(!mRebuildBlas);
    bool updateProcedural = is_set(mUpdates, UpdateFlags::CurvesMoved) || is_set(mUpdates, UpdateFlags::CustomPrimitivesMoved);

    for (const auto& group : mBlasGroups) {
        // Determine if any BLAS in the group needs to be updated.
        bool needsUpdate = false;
        for (uint32_t blasId : group.blasIndices) {
            const auto& blas = mBlasData[blasId];
            if (blas.hasProceduralPrimitives && updateProcedural) needsUpdate = true;
            if (!blas.hasProceduralPrimitives && blas.hasDynamicGeometry()) needsUpdate = true;
        }

        if (!needsUpdate) continue;

        // At least one BLAS in the group needs to be updated.
        // Insert barriers. The buffers are now ready to be written.
        auto& pBlas = group.pBlas;
        assert(pBlas && mpBlasScratch);
        pContext->uavBarrier(pBlas.get());
        pContext->uavBarrier(mpBlasScratch.get());

        // Iterate over all BLASes in group.
        for (uint32_t blasId : group.blasIndices) {
            const auto& blas = mBlasData[blasId];

            // Skip BLASes that do not need to be updated.
            if (blas.hasProceduralPrimitives && !updateProcedural) continue;
            if (!blas.hasProceduralPrimitives && !blas.hasDynamicGeometry()) continue;

            // Rebuild/update BLAS.
            RtAccelerationStructure::BuildDesc asDesc = {};
            asDesc.inputs = blas.buildInputs;
            asDesc.scratchData = mpBlasScratch->getGpuAddress() + blas.scratchByteOffset;
            asDesc.dest = mBlasObjects[blasId].get();

            if (blas.updateMode == UpdateMode::Refit) {
                // Set source address to destination address to update in place.
                asDesc.source = asDesc.dest;
                asDesc.inputs.flags |= RtAccelerationStructureBuildFlags::PerformUpdate;
            } else {
                // We'll rebuild in place. The BLAS should not be compacted, check that size matches prebuild info.
                assert(blas.blasByteSize == blas.prebuildInfo.resultDataMaxSize);
            }
            pContext->buildAccelerationStructure(asDesc, 0, nullptr);
        }

        // Insert barrier. The BLAS buffer is now ready for use.
        pContext->uavBarrier(pBlas.get());
    }
}

void Scene::fillInstanceDesc(std::vector<RtInstanceDesc>& instanceDescs, uint32_t rayCount, bool perMeshHitEntry) const {
    instanceDescs.clear();
    uint32_t instanceContributionToHitGroupIndex = 0;
    uint32_t instanceID = 0;

    for (size_t i = 0; i < mMeshGroups.size(); i++) {
        const auto& meshList = mMeshGroups[i].meshList;
        const bool isStatic = mMeshGroups[i].isStatic;

        assert(mBlasData[i].blasGroupIndex < mBlasGroups.size());
        const auto& pBlas = mBlasGroups[mBlasData[i].blasGroupIndex].pBlas;
        assert(pBlas);

        RtInstanceDesc desc = {};
        desc.accelerationStructure = pBlas->getGpuAddress() + mBlasData[i].blasByteOffset;
        desc.instanceMask = 0xFF;
        desc.instanceContributionToHitGroupIndex = perMeshHitEntry ? instanceContributionToHitGroupIndex : 0;

        instanceContributionToHitGroupIndex += rayCount * (uint32_t)meshList.size();

        // We expect all meshes in a group to have identical triangle winding. Verify that assumption here.
        assert(!meshList.empty());
        const bool frontFaceCW = mMeshDesc[meshList[0]].isFrontFaceCW();
        for (size_t i = 1; i < meshList.size(); i++) {
            assert(mMeshDesc[meshList[i]].isFrontFaceCW() == frontFaceCW);
        }

        // Set the triangle winding for the instance if it differs from the default.
        // The default in DXR is that a triangle is front facing if its vertices appear clockwise
        // from the ray origin, in object space in a left-handed coordinate system.
        // Note that Falcor uses a right-handed coordinate system, so we have to invert the flag.
        // Since these winding direction rules are defined in object space, they are unaffected by instance transforms.
        if (frontFaceCW) desc.flags = desc.flags | RtGeometryInstanceFlags::TriangleFrontCounterClockwise;

        // From the scene builder we can expect the following:
        //
        // If BLAS is marked as static:
        // - The meshes are pre-transformed to world-space.
        // - The meshes are guaranteed to be non-instanced, so only one INSTANCE_DESC with an identity transform is needed.
        //
        // If BLAS is not marked as static:
        // - The meshes are guaranteed to be non-instanced or be identically instanced, one INSTANCE_DESC per TLAS instance is needed.
        // - The global matrices are the same for all meshes in an instance.
        //
        assert(!meshList.empty());
        for(auto meshID: meshList) {
            size_t instanceCount = mMeshIdToInstanceIds[meshID].size();

            assert(instanceCount > 0);
            for (size_t instanceIdx = 0; instanceIdx < instanceCount; instanceIdx++) {
                // Validate that the ordering is matching our expectations:
                // InstanceID() + GeometryIndex() should look up the correct mesh instance.
                for (uint32_t geometryIndex = 0; geometryIndex < (uint32_t)meshList.size(); geometryIndex++) {
                    const auto& instances = mMeshIdToInstanceIds[meshList[geometryIndex]];
                    assert(instances.size() == instanceCount);
                    assert(instances[instanceIdx] == instanceID + geometryIndex);
                }

                //const auto& instance = mGeometryInstanceData[instanceID];
                const auto& instance = mGeometryInstanceData[mMeshIdToInstanceIds[meshID][instanceIdx]];

                desc.instanceID = instanceID; //instance.geometryIndex; //instanceID;
                desc.instanceMask = 0xFF;

                // Instance ray flags
                if((instance.flags & (uint32_t)GeometryInstanceFlags::VisibleToPrimaryRays) == 0) desc.instanceMask |= !(uint8_t)RtGeometryInstanceVisibilityFlags::VisibleToPrimaryRays; 
                if((instance.flags & (uint32_t)GeometryInstanceFlags::VisibleToShadowRays)  == 0) desc.instanceMask &= !(uint8_t)RtGeometryInstanceVisibilityFlags::VisibleToShadowRays;        

                instanceID ++;//= (uint32_t)meshList.size();

                glm::mat4 transform4x4 = glm::identity<glm::mat4>();
                if (!isStatic) {
                    // For non-static meshes, the matrices for all meshes in an instance are guaranteed to be the same.
                    // Just pick the matrix from the first mesh.
                    const uint32_t nodeId = mGeometryInstanceData[desc.instanceID].nodeID;
                    transform4x4 = transpose(mpAnimationController->getGlobalMatrixLists()[nodeId][0]);

                    // Verify that all meshes have matching tranforms.
                    for (uint32_t geometryIndex = 0; geometryIndex < (uint32_t)meshList.size(); geometryIndex++) {
                        assert(nodeId == mGeometryInstanceData[desc.instanceID + geometryIndex].nodeID);
                    }
                }
                std::memcpy(desc.transform, &transform4x4, sizeof(desc.transform));

                // Verify that instance data has the correct instanceIndex and geometryIndex.
                for (uint32_t geometryIndex = 0; geometryIndex < (uint32_t)meshList.size(); geometryIndex++) {
                    assert((uint32_t)instanceDescs.size() == mGeometryInstanceData[desc.instanceID + geometryIndex].instanceIndex);
                    assert(geometryIndex == mGeometryInstanceData[desc.instanceID + geometryIndex].geometryIndex);
                }

                instanceDescs.push_back(desc);
            }
        }
    }

    uint32_t totalBlasCount = (uint32_t)mMeshGroups.size() + (mCurveDesc.empty() ? 0 : 1) + getSDFGridGeometryCount() + (mCustomPrimitiveDesc.empty() ? 0 : 1);
    assert((uint32_t)mBlasData.size() == totalBlasCount);

    size_t blasDataIndex = mMeshGroups.size();
    // One instance for curves.
    if (!mCurveDesc.empty()) {
        const auto& blasData = mBlasData[blasDataIndex++];
        assert(blasData.blasGroupIndex < mBlasGroups.size());
        const auto& pBlas = mBlasGroups[blasData.blasGroupIndex].pBlas;
        assert(pBlas);

        RtInstanceDesc desc = {};
        desc.accelerationStructure = pBlas->getGpuAddress() + blasData.blasByteOffset;
        desc.instanceMask = 0xFF;
        desc.instanceID = instanceID;
        instanceID += (uint32_t)mCurveDesc.size();

        // Start procedural primitive hit group after the triangle hit groups.
        desc.instanceContributionToHitGroupIndex = perMeshHitEntry ? instanceContributionToHitGroupIndex : 0;

        instanceContributionToHitGroupIndex += rayCount * (uint32_t)mCurveDesc.size();

        // For cached curves, the matrices for all curves in an instance are guaranteed to be the same.
        // Just pick the matrix from the first curve.
        auto it = std::find_if(mGeometryInstanceData.begin(), mGeometryInstanceData.end(), [](const auto& inst) { return inst.getType() == GeometryType::Curve; });
        assert(it != mGeometryInstanceData.end());
        const uint32_t nodeId = it->nodeID;
        desc.setTransform(mpAnimationController->getGlobalMatrixLists()[nodeId][0]);

        // Verify that instance data has the correct instanceIndex and geometryIndex.
        for (uint32_t geometryIndex = 0; geometryIndex < (uint32_t)mCurveDesc.size(); geometryIndex++) {
            assert((uint32_t)instanceDescs.size() == mGeometryInstanceData[desc.instanceID + geometryIndex].instanceIndex);
            assert(geometryIndex == mGeometryInstanceData[desc.instanceID + geometryIndex].geometryIndex);
        }

        instanceDescs.push_back(desc);
    }

    // One instance per SDF grid instance.
    if (!mSDFGrids.empty()) {
        bool sdfGridInstancesDataHaveUniqueBLASes = true;
        switch (mSDFGridConfig.implementation) {
            case SDFGrid::Type::NormalizedDenseGrid:
            case SDFGrid::Type::SparseVoxelOctree:
                sdfGridInstancesDataHaveUniqueBLASes = false;
                break;
            case SDFGrid::Type::SparseVoxelSet:
            case SDFGrid::Type::SparseBrickSet:
                sdfGridInstancesDataHaveUniqueBLASes = true;
                break;
            default:
                assert(false);
        }

        for (const GeometryInstanceData& instance : mGeometryInstanceData) {
            if (instance.getType() != GeometryType::SDFGrid) continue;

            const BlasData& blasData = mBlasData[blasDataIndex + (sdfGridInstancesDataHaveUniqueBLASes ? instance.geometryID : 0)];
            const auto& pBlas = mBlasGroups[blasData.blasGroupIndex].pBlas;

            RtInstanceDesc desc = {};
            desc.accelerationStructure = pBlas->getGpuAddress() + blasData.blasByteOffset;
            desc.instanceMask = 0xFF;
            desc.instanceID = instanceID;
            instanceID++;

            // Start SDF grid hit group after the curve hit groups.
            desc.instanceContributionToHitGroupIndex = perMeshHitEntry ? instanceContributionToHitGroupIndex : 0;

            desc.setTransform(mpAnimationController->getGlobalMatrixLists()[instance.nodeID][0]);

            // Verify that instance data has the correct instanceIndex and geometryIndex.
            assert((uint32_t)instanceDescs.size() == instance.instanceIndex);
            assert(0 == instance.geometryIndex);

            instanceDescs.push_back(desc);
        }

        blasDataIndex += (sdfGridInstancesDataHaveUniqueBLASes ? mSDFGrids.size() : 1);
        instanceContributionToHitGroupIndex += rayCount * (uint32_t)mSDFGridDesc.size();
    }

    // One instance with identity transform for custom primitives.
    if (!mCustomPrimitiveDesc.empty()) {
        assert(mBlasData.back().blasGroupIndex < mBlasGroups.size());
        const auto& pBlas = mBlasGroups[mBlasData.back().blasGroupIndex].pBlas;
        assert(pBlas);

        RtInstanceDesc desc = {};
        desc.accelerationStructure = pBlas->getGpuAddress() + mBlasData.back().blasByteOffset;
        desc.instanceMask = 0xFF;
        desc.instanceID = instanceID;
        instanceID += (uint32_t)mCustomPrimitiveDesc.size();

        // Start procedural primitive hit group after the curve hit group.
        desc.instanceContributionToHitGroupIndex = perMeshHitEntry ? instanceContributionToHitGroupIndex : 0;

        instanceContributionToHitGroupIndex += rayCount * (uint32_t)mCustomPrimitiveDesc.size();

        glm::mat4 identityMat = glm::identity<glm::mat4>();
        std::memcpy(desc.transform, &identityMat, sizeof(desc.transform));
        instanceDescs.push_back(desc);
    }
}

void Scene::invalidateTlasCache() {
    for (auto& tlas : mTlasCache) {
        tlas.second.pTlasObject = nullptr;
    }
}

void Scene::buildTlas(RenderContext* pContext, uint32_t rayCount, bool perMeshHitEntry) {
    PROFILE(mpDevice, "buildTlas");

    TlasData tlas;
    auto it = mTlasCache.find(rayCount);
    if (it != mTlasCache.end()) tlas = it->second;

    // Prepare instance descs.
    // Note if there are no instances, we'll build an empty TLAS.
    fillInstanceDesc(mInstanceDescs, rayCount, perMeshHitEntry);

    RtAccelerationStructureBuildInputs inputs = {};
    inputs.kind = RtAccelerationStructureKind::TopLevel;
    inputs.descCount = (uint32_t)mInstanceDescs.size();
    inputs.flags = RtAccelerationStructureBuildFlags::None;

    // Add build flags for dynamic scenes if TLAS should be updating instead of rebuilt
    if ((mpAnimationController->hasAnimations() || mpAnimationController->hasAnimatedVertexCaches()) && mTlasUpdateMode == UpdateMode::Refit) {
        inputs.flags |= RtAccelerationStructureBuildFlags::AllowUpdate;

        // If TLAS has been built already and it was built with ALLOW_UPDATE
        if (tlas.pTlasObject != nullptr && tlas.updateMode == UpdateMode::Refit) inputs.flags |= RtAccelerationStructureBuildFlags::PerformUpdate;
    }

    tlas.updateMode = mTlasUpdateMode;

    // On first build for the scene, create scratch buffer and cache prebuild info. As long as INSTANCE_DESC count doesn't change, we can reuse these
    if (mpTlasScratch == nullptr) {
        // Prebuild
        mTlasPrebuildInfo = RtAccelerationStructure::getPrebuildInfo(mpDevice, inputs);
        mpTlasScratch = Buffer::create(mpDevice, mTlasPrebuildInfo.scratchDataSize, Buffer::BindFlags::UnorderedAccess, Buffer::CpuAccess::None);
        mpTlasScratch->setName("Scene::mpTlasScratch");

        // #SCENE This isn't guaranteed according to the spec, and the scratch buffer being stored should be sized differently depending on update mode
        assert(mTlasPrebuildInfo.updateScratchDataSize <= mTlasPrebuildInfo.scratchDataSize);
    }

    // Setup GPU buffers
    RtAccelerationStructure::BuildDesc asDesc = {};
    asDesc.inputs = inputs;

    // If first time building this TLAS
    if (tlas.pTlasObject == nullptr) {
        {
            // Allocate a new buffer for the TLAS only if the existing buffer isn't big enough.
            if (!tlas.pTlasBuffer || tlas.pTlasBuffer->getSize() < mTlasPrebuildInfo.resultDataMaxSize) {
                tlas.pTlasBuffer = Buffer::create(mpDevice, mTlasPrebuildInfo.resultDataMaxSize, Buffer::BindFlags::AccelerationStructure, Buffer::CpuAccess::None);
                tlas.pTlasBuffer->setName("Scene TLAS buffer");
            }
        }
        if (!mInstanceDescs.empty()) {
            // Allocate a new buffer for the TLAS instance desc input only if the existing buffer isn't big enough.
            if (!tlas.pInstanceDescs || tlas.pInstanceDescs->getSize() < mInstanceDescs.size() * sizeof(RtInstanceDesc))
            {
                tlas.pInstanceDescs = Buffer::create(mpDevice, (uint32_t)mInstanceDescs.size() * sizeof(RtInstanceDesc), Buffer::BindFlags::None, Buffer::CpuAccess::Write, mInstanceDescs.data());
                tlas.pInstanceDescs->setName("Scene instance descs buffer");
            } else {
                tlas.pInstanceDescs->setBlob(mInstanceDescs.data(), 0, mInstanceDescs.size() * sizeof(RtInstanceDesc));
            }
        }

        RtAccelerationStructure::Desc asCreateDesc = {};
        asCreateDesc.setKind(RtAccelerationStructureKind::TopLevel);
        asCreateDesc.setBuffer(tlas.pTlasBuffer, 0, mTlasPrebuildInfo.resultDataMaxSize);
        tlas.pTlasObject = RtAccelerationStructure::create(mpDevice, asCreateDesc);
    }
    // Else update instance descs and barrier TLAS buffers
    else
    {
        assert(mpAnimationController->hasAnimations() || mpAnimationController->hasAnimatedVertexCaches());
        pContext->uavBarrier(tlas.pTlasBuffer.get());
        pContext->uavBarrier(mpTlasScratch.get());
        if (tlas.pInstanceDescs) {
            assert(!mInstanceDescs.empty());
            tlas.pInstanceDescs->setBlob(mInstanceDescs.data(), 0, inputs.descCount * sizeof(RtInstanceDesc));
        }
        asDesc.source = tlas.pTlasObject.get(); // Perform the update in-place
    }

    assert(tlas.pTlasBuffer && tlas.pTlasBuffer->getApiHandle() && mpTlasScratch->getApiHandle());
    assert(inputs.descCount == 0 || (tlas.pInstanceDescs && tlas.pInstanceDescs->getApiHandle()));

    asDesc.inputs.instanceDescs = tlas.pInstanceDescs ? tlas.pInstanceDescs->getGpuAddress() : 0;
    asDesc.scratchData = mpTlasScratch->getGpuAddress();
    asDesc.dest = tlas.pTlasObject.get();

    // Set the source buffer to update in place if this is an update
    if ((inputs.flags & RtAccelerationStructureBuildFlags::PerformUpdate) != RtAccelerationStructureBuildFlags::None) {
        asDesc.source = asDesc.dest;
    }

    // Create TLAS
    if (tlas.pInstanceDescs) {
        pContext->resourceBarrier(tlas.pInstanceDescs.get(), Resource::State::NonPixelShader);
    }
    pContext->buildAccelerationStructure(asDesc, 0, nullptr);
    pContext->uavBarrier(tlas.pTlasBuffer.get());

    mTlasCache[rayCount] = tlas;
    updateRaytracingTLASStats();
}

void Scene::initRayTracing() {
    if (!mRenderSettings.useRayTracing || mRayTraceInitialized) return;

    /*
    auto graphicsQueueIndex = mpDevice->getApiCommandQueueType(LowLevelContextData::CommandQueueType::Direct);
    
    mpRtBuilder = new nvvk::RaytracingBuilderKHR();
    mpRtBuilder->setup(mpDevice->getApiHandle(), mpDevice->nvvkAllocator(), graphicsQueueIndex);
    */

    mRayTraceInitialized = true;
}

void Scene::setRaytracingShaderData(RenderContext* pContext, const ShaderVar& var, uint32_t rayTypeCount) {
    // On first execution or if BLASes need to be rebuilt, create BLASes for all geometries.
    if (!mBlasDataValid) {
        initGeomDesc(pContext);
        buildBlas(pContext);
    }

    // On first execution, when meshes have moved, when there's a new ray type count, or when a BLAS has changed, create/update the TLAS
    //
    // The raytracing shader table has one hit record per ray type and geometry. We need to know the ray type count in order to setup the indexing properly.
    // Note that for DXR 1.1 ray queries, the shader table is not used and the ray type count doesn't matter and can be set to zero.
    //
    auto tlasIt = mTlasCache.find(rayTypeCount);
    if (tlasIt == mTlasCache.end() || !tlasIt->second.pTlasObject) {
        // We need a hit entry per mesh right now to pass GeometryIndex()
        buildTlas(pContext, rayTypeCount, true);

        // If new TLAS was just created, get it so the iterator is valid
        if (tlasIt == mTlasCache.end()) tlasIt = mTlasCache.find(rayTypeCount);
    }
    assert(mpSceneBlock);

    // Bind TLAS.
    assert(tlasIt != mTlasCache.end() && tlasIt->second.pTlasObject);
    mpSceneBlock["rtAccel"].setAccelerationStructure(tlasIt->second.pTlasObject);

    // Bind Scene parameter block.
    getCamera()->setShaderData(mpSceneBlock[kCamera]); // TODO REMOVE: Shouldn't be needed anymore?
    var[kParameterBlockName] = mpSceneBlock;
}

std::vector<uint32_t> Scene::getMeshBlasIDs() const {
    const uint32_t invalidID = uint32_t(-1);
    std::vector<uint32_t> blasIDs(mMeshDesc.size(), invalidID);

    for (uint32_t blasID = 0; blasID < (uint32_t)mMeshGroups.size(); blasID++) {
        for (auto meshID : mMeshGroups[blasID].meshList) {
            assert(meshID < blasIDs.size());
            blasIDs[meshID] = blasID;
        }
    }

    for (auto blasID : blasIDs) assert(blasID != invalidID);
    return blasIDs;
}

uint32_t Scene::getParentNodeID(uint32_t nodeID) const {
    if (nodeID >= mSceneGraph.size()) throw std::runtime_error("Scene::getParentNodeID() - nodeID is out of range");
    return mSceneGraph[nodeID].parent;
}


void Scene::nullTracePass(RenderContext* pContext, const uint2& dim) {
    Device::SharedPtr pDevice = pContext->device();

    if (!pDevice->isFeatureSupported(Device::SupportedFeatures::RaytracingTier1_1)) {
        LLOG_ERR << "Raytracing Tier 1.1 is not supported by the current device.";
        return;
    }

    RtAccelerationStructureBuildInputs inputs = {};
    inputs.kind = RtAccelerationStructureKind::TopLevel;
    inputs.descCount = 0;
    inputs.flags = RtAccelerationStructureBuildFlags::None;

    RtAccelerationStructurePrebuildInfo prebuildInfo = RtAccelerationStructure::getPrebuildInfo(pDevice, inputs);

    auto pScratch = Buffer::create(pDevice, prebuildInfo.scratchDataSize, Buffer::BindFlags::UnorderedAccess, Buffer::CpuAccess::None);
    auto pTlasBuffer = Buffer::create(pDevice, prebuildInfo.resultDataMaxSize, Buffer::BindFlags::AccelerationStructure, Buffer::CpuAccess::None);

    RtAccelerationStructure::Desc createDesc = {};
    createDesc.setKind(RtAccelerationStructureKind::TopLevel);
    createDesc.setBuffer(pTlasBuffer, 0, prebuildInfo.resultDataMaxSize);
    RtAccelerationStructure::SharedPtr tlasObject = RtAccelerationStructure::create(pDevice, createDesc);

    RtAccelerationStructure::BuildDesc asDesc = {};
    asDesc.inputs = inputs;
    asDesc.scratchData = pScratch->getGpuAddress();
    asDesc.dest = tlasObject.get();

    pContext->buildAccelerationStructure(asDesc, 0, nullptr);
    pContext->uavBarrier(pTlasBuffer.get());

    Program::Desc desc;
    desc.addShaderLibrary("Scene/NullTrace.cs.slang").csEntry("main").setShaderModel("6_5");
    auto pass = ComputePass::create(pDevice, desc);
    pass["gOutput"] = Texture::create2D(pDevice, dim.x, dim.y, ResourceFormat::R8Uint, 1, 1, nullptr, ResourceBindFlags::UnorderedAccess);
    pass["gTlas"].setAccelerationStructure(tlasObject);

    for (size_t i = 0; i < 100; i++) {
        pass->execute(pContext, uint3(dim, 1));
    }
}

void Scene::updateNodeTransform(uint32_t nodeID, const float4x4& transform) {
    assert(nodeID < mSceneGraph.size());

    Node& node = mSceneGraph[nodeID];
    node.transformList.resize(1);
    node.transformList[0] = validateTransformMatrix(transform);
    mpAnimationController->setNodeEdited(nodeID);
}

void Scene::updateNodeTransformList(uint32_t nodeID, const std::vector<float4x4>& transformList) {
    assert(nodeID < mSceneGraph.size());

    Node& node = mSceneGraph[nodeID];
    std::vector<float4x4> validTransformList(transformList.size());

    for(size_t i = 0; i < transformList.size(); i++) {
        validTransformList[i] = validateTransformMatrix(transformList[i]);
    }

    if( node.transformList != validTransformList) {
        node.transformList = std::move(validTransformList);
        mpAnimationController->setNodeEdited(nodeID);
    }
}

void Scene::clearNodeTransformList(uint32_t nodeID) {
    assert(nodeID < mSceneGraph.size());

    Node& node = mSceneGraph[nodeID];
    if(!node.transformList.empty()) {
        node.transformList.clear();
        mpAnimationController->setNodeEdited(nodeID);
    }
}
    
void Scene::setEnvMap(EnvMap::SharedPtr pEnvMap) {
    if (mpEnvMap == pEnvMap) return;
    mpEnvMap = pEnvMap;
    mEnvMapChanged = true;
}

void Scene::loadEnvMap(const std::string& filename) {
    EnvMap::SharedPtr pEnvMap = EnvMap::create(mpDevice, filename);
    setEnvMap(pEnvMap);
}

void Scene::setCameraAspectRatio(float ratio) {
    getCamera()->setAspectRatio(ratio);
}

void Scene::setCameraController(CameraControllerType type) {
    if (!mCameraSwitched && mCamCtrlType == type && mpCamCtrl) return;

    auto camera = getCamera();
    switch (type) {
        case CameraControllerType::FirstPerson:
            mpCamCtrl = FirstPersonCameraController::create(camera);
            break;
        case CameraControllerType::Orbiter:
            mpCamCtrl = OrbiterCameraController::create(camera);
            ((OrbiterCameraController*)mpCamCtrl.get())->setModelParams(mSceneBB.center(), mSceneBB.radius(), 3.5f);
            break;
        case CameraControllerType::SixDOF:
            mpCamCtrl = SixDoFCameraController::create(camera);
            break;
        default:
            should_not_get_here();
    }
    mpCamCtrl->setCameraSpeed(mCameraSpeed);
}

std::string Scene::getScript(const std::string& sceneVar) {
#ifdef SCRIPTING
    std::string c;

    // Render settings.
    c += Scripting::makeSetProperty(sceneVar, kRenderSettings, mRenderSettings);

    // Animations.
    if (hasAnimation() && !isAnimated()) {
        c += Scripting::makeSetProperty(sceneVar, kAnimated, false);
    }

    for (size_t i = 0; i < mLights.size(); ++i) {
        const auto& light = mLights[i];
        if (light->hasAnimation() && !light->isAnimated()) {
            c += Scripting::makeSetProperty(sceneVar + "." + kGetLight + "(" + std::to_string(i) + ").", kAnimated, false);
        }
    }

    // Camera.
    if (mSelectedCamera != 0) {
        c += sceneVar + "." + kCamera + " = " + sceneVar + "." + kCameras + "[" + std::to_string(mSelectedCamera) + "]\n";
    }
    c += getCamera()->getScript(sceneVar + "." + kCamera);

    // Camera speed.
    c += Scripting::makeSetProperty(sceneVar, kCameraSpeed, mCameraSpeed);

    // Viewpoints.
    if (hasSavedViewpoints()) {
        for (size_t i = 1; i < mViewpoints.size(); i++) {
            auto v = mViewpoints[i];
            c += Scripting::makeMemberFunc(sceneVar, kAddViewpoint, v.position, v.target, v.up, v.index);
        }
    }
    return c;
#else
    return "";
#endif
}

#ifdef SCRIPTING
    SCRIPT_BINDING(Scene) {
        pybind11::class_<Scene, Scene::SharedPtr> scene(m, "Scene");
        scene.def_property_readonly(kStats.c_str(), [] (const Scene* pScene) { return pScene->getSceneStats().toPython(); });
        scene.def_property_readonly(kBounds.c_str(), &Scene::getSceneBounds, pybind11::return_value_policy::copy);
        scene.def_property(kCamera.c_str(), &Scene::getCamera, &Scene::setCamera);
        scene.def_property(kEnvMap.c_str(), &Scene::getEnvMap, &Scene::setEnvMap);
        scene.def_property_readonly(kAnimations.c_str(), &Scene::getAnimations);
        scene.def_property_readonly(kCameras.c_str(), &Scene::getCameras);
        scene.def_property_readonly(kLights.c_str(), &Scene::getLights);
        scene.def_property_readonly(kMaterials.c_str(), &Scene::getMaterials);
        scene.def_property_readonly(kGridVolumes.c_str(), &Scene::getGridVolumes);
        scene.def_property_readonly("volumes", &Scene::getGridVolumes); // PYTHONDEPRECATED
        scene.def_property(kCameraSpeed.c_str(), &Scene::getCameraSpeed, &Scene::setCameraSpeed);
        scene.def_property(kAnimated.c_str(), &Scene::isAnimated, &Scene::setIsAnimated);
        scene.def_property(kLoopAnimations.c_str(), &Scene::isLooped, &Scene::setIsLooped);
        scene.def_property(kRenderSettings.c_str(), pybind11::overload_cast<void>(&Scene::getRenderSettings, pybind11::const_), &Scene::setRenderSettings);

        scene.def(kSetEnvMap.c_str(), &Scene::loadEnvMap, "filename"_a);
        scene.def(kGetLight.c_str(), &Scene::getLight, "index"_a);
        scene.def(kGetLight.c_str(), &Scene::getLightByName, "name"_a);
        scene.def(kGetMaterial.c_str(), &Scene::getMaterial, "index"_a);
        scene.def(kGetMaterial.c_str(), &Scene::getMaterialByName, "name"_a);
        scene.def(kGetGridVolume.c_str(), &Scene::getGridVolume, "index"_a);
        scene.def(kGetGridVolume.c_str(), &Scene::getGridVolumeByName, "name"_a);
        scene.def("getVolume", &Scene::getGridVolume, "index"_a); // PYTHONDEPRECATED
        scene.def("getVolume", &Scene::getGridVolumeByName, "name"_a); // PYTHONDEPRECATED

        // Viewpoints
        scene.def(kAddViewpoint.c_str(), pybind11::overload_cast<>(&Scene::addViewpoint)); // add current camera as viewpoint
        scene.def(kAddViewpoint.c_str(), pybind11::overload_cast<const float3&, const float3&, const float3&, uint32_t>(&Scene::addViewpoint), "position"_a, "target"_a, "up"_a, "cameraIndex"_a=0); // add specified viewpoint
        scene.def(kRemoveViewpoint.c_str(), &Scene::removeViewpoint); // remove the selected viewpoint
        scene.def(kSelectViewpoint.c_str(), &Scene::selectViewpoint, "index"_a); // select a viewpoint by index

        // RenderSettings
        ScriptBindings::SerializableStruct<Scene::RenderSettings> renderSettings(m, "SceneRenderSettings");
#define field(f_) field(#f_, &Scene::RenderSettings::f_)
        renderSettings.field(useRayTracing);
        renderSettings.field(useEnvLight);
        renderSettings.field(useAnalyticLights);
        renderSettings.field(useEmissiveLights);
        renderSettings.field(useGridVolumes);
#undef field
    }
#endif // SCRIPTING

}  // namespace Falcor
