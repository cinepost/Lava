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
#include "Falcor/stdafx.h"

#include "Falcor/Core/API/RenderContext.h"

#include "Falcor/Utils/Timing/Profiler.h"
#include "Falcor/Scene/SceneBuilder.h"
#include <fstream>

#include "AnimationController.h"

namespace Falcor {
    
namespace {
    const std::string kWorldMatrices = "worldMatrices";
    const std::string kInverseTransposeWorldMatrices = "inverseTransposeWorldMatrices";
    const std::string kPrevWorldMatrices = "prevWorldMatrices";
    const std::string kPrevInverseTransposeWorldMatrices = "prevInverseTransposeWorldMatrices";
}

AnimationController::AnimationController(Scene* pScene, const StaticVertexVector& staticVertexData, const SkinningVertexVector& skinningVertexData, uint32_t prevVertexCount, const std::vector<Animation::SharedPtr>& animations)
    : mpScene(pScene)
    , mAnimations(animations)
    , mNodesEdited(pScene->mSceneGraph.size())
    , mLocalMatrixLists(pScene->mSceneGraph.size())
    , mGlobalMatrixLists(pScene->mSceneGraph.size())
    , mInvTransposeGlobalMatrixLists(pScene->mSceneGraph.size())
    , mMatricesChanged(pScene->mSceneGraph.size())
{
    mpDevice = pScene->device();
    assert(mpDevice);

    // An extra buffer is required to store the previous frame vertex data for skinned and vertex-animated meshes.
    // The buffer contains data for skinned meshes first, followed by vertex-animated meshes.
    //
    // Initialize the previous positions for skinned vertices. AnimatedVertexCache will initialize the remaining data if necessary
    // This ensures we have valid data in the buffer before the skinning pass runs for the first time.
    if (prevVertexCount > 0) {
        std::vector<PrevVertexData> prevVertexData(prevVertexCount);
        for (size_t i = 0; i < skinningVertexData.size(); i++) {
            uint32_t staticIndex = skinningVertexData[i].staticIndex;
            prevVertexData[i].position = staticVertexData[staticIndex].position;
        }
        mpPrevVertexData = Buffer::createStructured(mpDevice, sizeof(PrevVertexData), prevVertexCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess, Buffer::CpuAccess::None, prevVertexData.data(), false);
        mpPrevVertexData->setName("AnimationController::mpPrevVertexData");
    }

    createSkinningPass(staticVertexData, skinningVertexData);

    // Determine length of global animation loop.
    for (const auto& pAnimation : mAnimations) {
        mGlobalAnimationLength = std::max(mGlobalAnimationLength, pAnimation->getDuration());
    }
}

void AnimationController::createBuffers(size_t matrixCount) {
    assert(matrixCount > 0);
    assert(matrixCount * 4 <= std::numeric_limits<uint32_t>::max());
    if(matrixCount == 0) return;
    uint32_t float4Count = (uint32_t)matrixCount * 4;

    if(!mpWorldMatricesBuffer || mpWorldMatricesBuffer->getElementCount() != matrixCount) {
        mpWorldMatricesBuffer = Buffer::createStructured(mpDevice, sizeof(float4), float4Count, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpWorldMatricesBuffer->setName("AnimationController::mpWorldMatricesBuffer");
    }
    if(!mpPrevWorldMatricesBuffer || mpPrevWorldMatricesBuffer->getElementCount() != matrixCount) {
        mpPrevWorldMatricesBuffer = Buffer::createStructured(mpDevice, sizeof(float4), float4Count, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpPrevWorldMatricesBuffer->setName("AnimationController::mpPrevWorldMatricesBuffer");
    }
    if(!mpInvTransposeWorldMatricesBuffer || mpInvTransposeWorldMatricesBuffer->getElementCount() != matrixCount) {
        mpInvTransposeWorldMatricesBuffer = Buffer::createStructured(mpDevice, sizeof(float4), float4Count, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpInvTransposeWorldMatricesBuffer->setName("AnimationController::mpInvTransposeWorldMatricesBuffer");
    }
    if(!mpPrevInvTransposeWorldMatricesBuffer || mpPrevInvTransposeWorldMatricesBuffer->getElementCount() != matrixCount) {
        mpPrevInvTransposeWorldMatricesBuffer = Buffer::createStructured(mpDevice, sizeof(float4), float4Count, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpPrevInvTransposeWorldMatricesBuffer->setName("AnimationController::mpPrevInvTransposeWorldMatricesBuffer");
    }
}

AnimationController::UniquePtr AnimationController::create(Scene* pScene, const StaticVertexVector& staticVertexData, const SkinningVertexVector& skinningVertexData, uint32_t prevVertexCount, const std::vector<Animation::SharedPtr>& animations) {
    return UniquePtr(new AnimationController(pScene, staticVertexData, skinningVertexData, prevVertexCount, animations));
}

void AnimationController::addAnimatedVertexCaches(std::vector<CachedCurve>&& cachedCurves, std::vector<CachedMesh>&& cachedMeshes, const StaticVertexVector& staticVertexData) {
    size_t totalAnimatedMeshVertexCount = 0;

    for (auto& cache : cachedMeshes) {
        totalAnimatedMeshVertexCount += mpScene->getMesh(cache.meshID).vertexCount;
    }

    for (auto& cache : cachedCurves) {
        if (cache.tessellationMode != CurveTessellationMode::LinearSweptSphere) {
            totalAnimatedMeshVertexCount += mpScene->getMesh(cache.geometryID).vertexCount;
        }
    }

    if (totalAnimatedMeshVertexCount > 0) {
        // Initialize remaining previous position data
        std::vector<PrevVertexData> prevVertexData;
        prevVertexData.reserve(totalAnimatedMeshVertexCount);

        for (auto& cache : cachedMeshes) {
            uint32_t offset = mpScene->getMesh(cache.meshID).vbOffset;
            for (size_t i = 0; i < cache.vertexData.front().size(); i++) {
                prevVertexData.push_back({ staticVertexData[offset + i].position });
            }
        }

        for (auto& cache : cachedCurves) {
            if (cache.tessellationMode != CurveTessellationMode::LinearSweptSphere) {
                uint32_t offset = mpScene->getMesh(cache.geometryID).vbOffset;
                uint32_t vertexCount = mpScene->getMesh(cache.geometryID).vertexCount;
                for (size_t i = 0; i < vertexCount; i++) {
                    prevVertexData.push_back({ staticVertexData[offset + i].position });
                }
            }
        }

        uint32_t byteOffset = 0;
        if (!cachedMeshes.empty()) {
            byteOffset = mpScene->getMesh(cachedMeshes.front().meshID).prevVbOffset * sizeof(PrevVertexData);
        } else {
            for (auto& cache : cachedCurves) {
                if (cache.tessellationMode != CurveTessellationMode::LinearSweptSphere) {
                    byteOffset = mpScene->getMesh(cache.geometryID).prevVbOffset * sizeof(PrevVertexData);
                    break;
                }
            }
        }

        mpPrevVertexData->setBlob(prevVertexData.data(), byteOffset, prevVertexData.size() * sizeof(PrevVertexData));
    }

    mpVertexCache = AnimatedVertexCache::create(mpScene, mpPrevVertexData, std::move(cachedCurves), std::move(cachedMeshes));

    // Note: It is a workaround to have two pre-infinity behaviors for the cached animation.
    // We need `Cycle` behavior when the length of cached animation is smaller than the length of mesh animation (e.g., tiger forest).
    // We need `Constant` behavior when both animation lengths are equal (e.g., a standalone tiger).
    if (mpVertexCache->getGlobalAnimationLength() < mGlobalAnimationLength) {
        mpVertexCache->setPreInfinityBehavior(Animation::Behavior::Cycle);
    }
}


void AnimationController::setEnabled(bool enabled) {
    mEnabled = enabled;
}

void AnimationController::setIsLooped(bool looped) {
    mLoopAnimations = looped;

    if (mpVertexCache) {
        mpVertexCache->setIsLooped(looped);
    }
}

void AnimationController::initLocalMatrices() {
    for (size_t i = 0; i < mLocalMatrixLists.size(); i++) {
        mLocalMatrixLists[i] = mpScene->mSceneGraph[i].transformList;
    }
}

bool AnimationController::animate(RenderContext* pContext, double currentTime) {
    PROFILE(mpDevice, "animate");

    std::fill(mMatricesChanged.begin(), mMatricesChanged.end(), false);

    // Check for edited scene nodes and update local matrices.
    const auto& sceneGraph = mpScene->mSceneGraph;
    bool edited = false;
    for (size_t i = 0; i < sceneGraph.size(); ++i) {
        if (mNodesEdited[i]) {
            mLocalMatrixLists[i] = sceneGraph[i].transformList;
            mNodesEdited[i] = false;
            mMatricesChanged[i] = true;
            edited = true;
        }
    }

    bool changed = false;
    double time = mLoopAnimations ? std::fmod(currentTime, mGlobalAnimationLength) : currentTime;

    // Check if animation controller was enabled/disabled since last call.
    // When enabling/disabling, all data for the current and previous frame is initialized,
    // including transformation matrices, dynamic vertex data etc.
    if (mFirstUpdate || mEnabled != mPrevEnabled) {
        initLocalMatrices();
        if (mEnabled) {
            updateLocalMatrices(time);
            mTime = mPrevTime = time;
        }
        updateWorldMatrices(true);
        uploadWorldMatrices(true);

        if (!sceneGraph.empty()) {
            assert(mpWorldMatricesBuffer && mpPrevWorldMatricesBuffer);
            assert(mpInvTransposeWorldMatricesBuffer && mpPrevInvTransposeWorldMatricesBuffer);
            pContext->copyResource(mpPrevWorldMatricesBuffer.get(), mpWorldMatricesBuffer.get());
            pContext->copyResource(mpPrevInvTransposeWorldMatricesBuffer.get(), mpInvTransposeWorldMatricesBuffer.get());
            bindBuffers();
            executeSkinningPass(pContext, true);
        }

        if (mpVertexCache) {
            if (mEnabled && mpVertexCache->hasAnimations()) {
                // Recompute time based on the cycle length of vertex caches.
                double vertexCacheTime = (mGlobalAnimationLength == 0) ? currentTime : time;
                mpVertexCache->animate(pContext, vertexCacheTime);
            }
            mpVertexCache->copyToPrevVertices(pContext);
        }

        mFirstUpdate = false;
        mPrevEnabled = mEnabled;
        changed = true;
    }

    // Perform incremental update.
    // This updates all animated matrices and dynamic vertex data.
    if (edited || mEnabled && (time != mTime || mTime != mPrevTime)) {
        if (edited || hasAnimations()) {
            assert(mpWorldMatricesBuffer && mpPrevWorldMatricesBuffer);
            assert(mpInvTransposeWorldMatricesBuffer && mpPrevInvTransposeWorldMatricesBuffer);
            swap(mpPrevWorldMatricesBuffer, mpWorldMatricesBuffer);
            swap(mpPrevInvTransposeWorldMatricesBuffer, mpInvTransposeWorldMatricesBuffer);
            updateLocalMatrices(time);
            updateWorldMatrices();
            uploadWorldMatrices();
            bindBuffers();
            executeSkinningPass(pContext);
            changed = true;
        }

        if (mpVertexCache && mpVertexCache->hasAnimations()) {
            // Recompute time based on the cycle length of vertex caches.
            double vertexCacheTime = (mGlobalAnimationLength == 0) ? currentTime : time;
            mpVertexCache->animate(pContext, vertexCacheTime);
            changed = true;
        }

        mPrevTime = mTime;
        mTime = time;
    }

    return changed;
}

void AnimationController::updateLocalMatrices(double time) {
    for (auto& pAnimation : mAnimations) {
        uint32_t nodeID = pAnimation->getNodeID();
        assert(nodeID < mLocalMatrixLists.size());
        mLocalMatrixLists[nodeID] = {pAnimation->animate(time)};
        mMatricesChanged[nodeID] = true;
    }
}

size_t AnimationController::getGlobalMatricesCount() const {
    size_t count = 0;
    for(auto const& list: mGlobalMatrixLists) count += list.size();
    return count;
}

void AnimationController::updateWorldMatrices(bool updateAll) {
    const auto& sceneGraph = mpScene->mSceneGraph;

    for (size_t i = 0; i < mGlobalMatrixLists.size(); i++) {
        // Propagate matrix change flag to children.
        if (sceneGraph[i].parent != SceneBuilder::kInvalidNodeID) {
            mMatricesChanged[i] = mMatricesChanged[i] || mMatricesChanged[sceneGraph[i].parent];
        }

        if (!mMatricesChanged[i] && !updateAll) continue;

        mGlobalMatrixLists[i] = mLocalMatrixLists[i];

        if (mpScene->mSceneGraph[i].parent != SceneBuilder::kInvalidNodeID && (mGlobalMatrixLists[i].size() == mGlobalMatrixLists[sceneGraph[i].parent].size())) {
            for(size_t ii = 0; ii < mGlobalMatrixLists[i].size(); ++ii) {
                mGlobalMatrixLists[i][ii] = mGlobalMatrixLists[sceneGraph[i].parent][ii] * mGlobalMatrixLists[i][ii];
            }
        }

        for(size_t ii = 0; ii < mGlobalMatrixLists[i].size(); ++ii){
            mInvTransposeGlobalMatrixLists[i].resize(mGlobalMatrixLists[i].size());
            mInvTransposeGlobalMatrixLists[i][ii] = transpose(inverse(mGlobalMatrixLists[i][ii]));
        }

        if (mpSkinningPass) {
            mSkinningMatrices[i] = mGlobalMatrixLists[i][0] * sceneGraph[i].localToBindSpace;
            mInvTransposeSkinningMatrices[i] = transpose(inverse(mSkinningMatrices[i]));
        }
    }
}

void AnimationController::uploadWorldMatrices(bool uploadAll) {
    if (mGlobalMatrixLists.empty()) return;

    assert(mGlobalMatrixLists.size() == mInvTransposeGlobalMatrixLists.size());

    size_t totalMatricesCount = 0;
    for(auto const& list: mGlobalMatrixLists) totalMatricesCount += list.size();

    createBuffers(totalMatricesCount);
    assert(mpWorldMatricesBuffer && mpInvTransposeWorldMatricesBuffer);

    if(totalMatricesCount != mpWorldMatricesBuffer->getElementCount()) {
        uploadAll = true;
    }

    if (uploadAll) {
        // Upload all matrices.
        std::vector<float4x4> globalMatrices;
        std::vector<float4x4> invTransposeGlobalMatrices;
        for(size_t i = 0; i < mGlobalMatrixLists.size(); ++i) {
            for(auto& m: mGlobalMatrixLists[i]) {
                globalMatrices.push_back(m);
            }
            for(auto& m: mInvTransposeGlobalMatrixLists[i])  {
                invTransposeGlobalMatrices.push_back(m);
            }
        }
        mpWorldMatricesBuffer->setBlob(globalMatrices.data(), 0, mpWorldMatricesBuffer->getSize());
        mpInvTransposeWorldMatricesBuffer->setBlob(invTransposeGlobalMatrices.data(), 0, mpInvTransposeWorldMatricesBuffer->getSize());
    } else {
        // Upload changed matrices only.
        size_t offset = 0;
        for (size_t i = 0; i < mGlobalMatrixLists.size();) {
            // Upload range of changed matrices.
            if (mMatricesChanged[i]) {
                std::vector<float4x4> globalMatrices;
                std::vector<float4x4> invTransposeGlobalMatrices;
                for(size_t ii = i; ii < mGlobalMatrixLists.size(); ++ii) {
                    for(auto& m: mGlobalMatrixLists[ii]) {
                        globalMatrices.push_back(m);
                    }
                    for(auto& m: mInvTransposeGlobalMatrixLists[ii]) { 
                        invTransposeGlobalMatrices.push_back(m);
                    }
                }

                mpWorldMatricesBuffer->setBlob(globalMatrices.data(), offset * sizeof(float4x4), globalMatrices.size() * sizeof(float4x4));
                mpInvTransposeWorldMatricesBuffer->setBlob(invTransposeGlobalMatrices.data(), offset * sizeof(float4x4), invTransposeGlobalMatrices.size() * sizeof(float4x4));
                break;
            }
            offset+=mGlobalMatrixLists[i].size();
        }
    }
}

void AnimationController::bindBuffers() {
    ParameterBlock* pBlock = mpScene->mpSceneBlock.get();
    pBlock->setBuffer(kWorldMatrices, mpWorldMatricesBuffer);
    pBlock->setBuffer(kInverseTransposeWorldMatrices, mpInvTransposeWorldMatricesBuffer);
    bool usePrev = mEnabled && hasAnimations();
    pBlock->setBuffer(kPrevWorldMatrices, usePrev ? mpPrevWorldMatricesBuffer : mpWorldMatricesBuffer);
    pBlock->setBuffer(kPrevInverseTransposeWorldMatrices, usePrev ? mpPrevInvTransposeWorldMatricesBuffer : mpInvTransposeWorldMatricesBuffer);
}

uint64_t AnimationController::getMemoryUsageInBytes() const {
    uint64_t m = 0;
    m += mpWorldMatricesBuffer ? mpWorldMatricesBuffer->getSize() : 0;
    m += mpPrevWorldMatricesBuffer ? mpPrevWorldMatricesBuffer->getSize() : 0;
    m += mpInvTransposeWorldMatricesBuffer ? mpInvTransposeWorldMatricesBuffer->getSize() : 0;
    m += mpPrevInvTransposeWorldMatricesBuffer ? mpPrevInvTransposeWorldMatricesBuffer->getSize() : 0;
    m += mpSkinningMatricesBuffer ? mpSkinningMatricesBuffer->getSize() : 0;
    m += mpInvTransposeSkinningMatricesBuffer ? mpInvTransposeSkinningMatricesBuffer->getSize() : 0;
    m += mpMeshBindMatricesBuffer ? mpMeshBindMatricesBuffer->getSize() : 0;
    m += mpStaticVertexData ? mpStaticVertexData->getSize() : 0;
    m += mpSkinningVertexData ? mpSkinningVertexData->getSize() : 0;
    m += mpPrevVertexData ? mpPrevVertexData->getSize() : 0;
    m += mpVertexCache ? mpVertexCache->getMemoryUsageInBytes() : 0;
    return m;
}

void AnimationController::createSkinningPass(const std::vector<PackedStaticVertexData>& staticVertexData, const SkinningVertexVector& skinningVertexData) {
    if (staticVertexData.empty()) return;

    // We always copy the static data, to initialize the non-skinned vertices.
    assert(mpScene->getMeshVao());
    const Buffer::SharedPtr& pVB = mpScene->getMeshVao()->getVertexBuffer(Scene::kStaticDataBufferIndex);
    assert(pVB->getSize() == staticVertexData.size() * sizeof(staticVertexData[0]));
    pVB->setBlob(staticVertexData.data(), 0, pVB->getSize());

    if (!skinningVertexData.empty()) {
        mSkinningMatrices.resize(mpScene->mSceneGraph.size());
        mInvTransposeSkinningMatrices.resize(mSkinningMatrices.size());
        mMeshBindMatrices.resize(mpScene->mSceneGraph.size());

        mpSkinningPass = ComputePass::create(mpDevice, "Scene/Animation/Skinning.slang");
        auto block = mpSkinningPass->getVars()["gData"];

        // Initialize mesh bind transforms
        std::vector<float4x4> meshInvBindMatrices(mMeshBindMatrices.size());
        for (size_t i = 0; i < mpScene->mSceneGraph.size(); i++) {
            mMeshBindMatrices[i] = mpScene->mSceneGraph[i].meshBind;
            meshInvBindMatrices[i] = glm::inverse(mMeshBindMatrices[i]);
        }

        // Bind vertex data.
        assert(staticVertexData.size() <= std::numeric_limits<uint32_t>::max());
        assert(skinningVertexData.size() <= std::numeric_limits<uint32_t>::max());
        mpStaticVertexData = Buffer::createStructured(mpDevice, block["staticData"], (uint32_t)staticVertexData.size(), ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, staticVertexData.data(), false);
        mpStaticVertexData->setName("AnimationController::mpStaticVertexData");
        mpSkinningVertexData = Buffer::createStructured(mpDevice, block["skinningData"], (uint32_t)skinningVertexData.size(), ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, skinningVertexData.data(), false);
        mpSkinningVertexData->setName("AnimationController::mpSkinningVertexData");

        block["staticData"] = mpStaticVertexData;
        block["skinningData"] = mpSkinningVertexData;
        block["skinnedVertices"] = pVB;
        block["prevSkinnedVertices"] = mpPrevVertexData;

        // Bind transforms.
        assert(mSkinningMatrices.size() * 4 < std::numeric_limits<uint32_t>::max());
        uint32_t float4Count = (uint32_t)mSkinningMatrices.size() * 4;
        mpMeshBindMatricesBuffer = Buffer::createStructured(mpDevice, sizeof(float4), float4Count, ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, mMeshBindMatrices.data(), false);
        mpMeshBindMatricesBuffer->setName("AnimationController::mpMeshBindMatricesBuffer");
        mpMeshInvBindMatricesBuffer = Buffer::createStructured(mpDevice, sizeof(float4), float4Count, ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, meshInvBindMatrices.data(), false);
        mpMeshInvBindMatricesBuffer->setName("AnimationController::mpMeshInvBindMatricesBuffer");
        mpSkinningMatricesBuffer = Buffer::createStructured(mpDevice, sizeof(float4), float4Count, ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpSkinningMatricesBuffer->setName("AnimationController::mpSkinningMatricesBuffer");
        mpInvTransposeSkinningMatricesBuffer = Buffer::createStructured(mpDevice, sizeof(float4), float4Count, ResourceBindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
        mpInvTransposeSkinningMatricesBuffer->setName("AnimationController::mpInvTransposeSkinningMatricesBuffer");

        block["boneMatrices"].setBuffer(mpSkinningMatricesBuffer);
        block["inverseTransposeBoneMatrices"].setBuffer(mpInvTransposeSkinningMatricesBuffer);
        block["meshBindMatrices"].setBuffer(mpMeshBindMatricesBuffer);
        block["meshInvBindMatrices"].setBuffer(mpMeshInvBindMatricesBuffer);

        mSkinningDispatchSize = (uint32_t)skinningVertexData.size();
    }
}

void AnimationController::executeSkinningPass(RenderContext* pContext, bool initPrev) {
    if (!mpSkinningPass) return;
    mpSkinningMatricesBuffer->setBlob(mSkinningMatrices.data(), 0, mpSkinningMatricesBuffer->getSize());
    mpInvTransposeSkinningMatricesBuffer->setBlob(mInvTransposeSkinningMatrices.data(), 0, mpInvTransposeSkinningMatricesBuffer->getSize());
    auto vars = mpSkinningPass->getVars()["gData"];
    vars["inverseTransposeWorldMatrices"].setBuffer(mpInvTransposeWorldMatricesBuffer);
    vars["worldMatrices"].setBuffer(mpWorldMatricesBuffer);
    vars["initPrev"] = initPrev;
    mpSkinningPass->execute(pContext, mSkinningDispatchSize, 1, 1);
}

}  // namespace Falcor
