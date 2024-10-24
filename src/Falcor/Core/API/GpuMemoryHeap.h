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
#ifndef SRC_FALCOR_CORE_API_GPUMEMORYHEAP_H_
#define SRC_FALCOR_CORE_API_GPUMEMORYHEAP_H_

#include <queue>
#include <memory>
#include <unordered_map>

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/GpuFence.h"

namespace Falcor {

class Device;

class dlldecl GpuMemoryHeap {
 public:
    using SharedPtr = std::shared_ptr<GpuMemoryHeap>;
    using SharedConstPtr = std::shared_ptr<const GpuMemoryHeap>;

    enum class Type {
        Default,
        Upload,
        Readback,
        Count     // TODO: i've put it here. no idea why exactly
    };

    struct BaseData {
        ResourceHandle pResourceHandle;
        GpuAddress offset = 0;
        uint8_t* pData = nullptr;
    };

    struct Allocation : public BaseData {
        uint64_t pageID = 0;
        uint64_t fenceValue = 0;

        static const uint64_t kMegaPageId = -1;
        bool operator<(const Allocation& other)  const { return fenceValue > other.fenceValue; }
    };

    ~GpuMemoryHeap();

    /** Create a new GPU memory heap.
        \param[in] type The type of heap.
        \param[in] pageSize Page size in bytes.
        \param[in] pFence Fence to use for synchronization.
        \return A new object, or throws an exception if creation failed.
    */
    static SharedPtr create(std::shared_ptr<Device> pDevice, Type type, size_t pageSize, const GpuFence::SharedPtr& pFence);

    Allocation allocate(size_t size, size_t alignment = 1);
    void release(Allocation& data);
    size_t getPageSize() const { return mPageSize; }
    void executeDeferredReleases();

private:
    GpuMemoryHeap(std::shared_ptr<Device> pDevice, Type type, size_t pageSize, const GpuFence::SharedPtr& pFence);

    struct PageData : public BaseData {
        uint32_t allocationsCount = 0;
        size_t currentOffset = 0;

        using UniquePtr = std::unique_ptr<PageData>;
    };

    Type mType;
    GpuFence::SharedPtr mpFence;
    size_t mPageSize = 0;
    size_t mCurrentPageId = 0;
    PageData::UniquePtr mpActivePage;

    std::priority_queue<Allocation> mDeferredReleases;
    std::unordered_map<size_t, PageData::UniquePtr> mUsedPages;
    std::queue<PageData::UniquePtr> mAvailablePages;

    std::shared_ptr<Device> mpDevice; 

    void allocateNewPage();
    void initBasePageData(BaseData& data, size_t size);
};

inline const std::string to_string(GpuMemoryHeap::Type t) {
        #define type_2_string(a) case GpuMemoryHeap::Type::a: return #a;
        switch(t) {
            type_2_string(Default);
            type_2_string(Upload);
            type_2_string(Readback);
            type_2_string(Count);
            default:
                should_not_get_here();
                return "";
        }
        #undef type_2_string
    }

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_GPUMEMORYHEAP_H_
