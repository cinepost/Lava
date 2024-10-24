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
#ifndef SRC_FALCOR_CORE_API_QUERYHEAP_H_
#define SRC_FALCOR_CORE_API_QUERYHEAP_H_

#include <deque>
#include <memory>

#include "Falcor/Core/Framework.h"


namespace Falcor {

class Device;

class dlldecl QueryHeap : public std::enable_shared_from_this<QueryHeap> {
 public:
    using SharedPtr = std::shared_ptr<QueryHeap>;
    using ApiHandle = QueryHeapHandle;

    enum class Type {
        Timestamp,
        Occlusion,
        PipelineStats,
        AccelerationStructureCompactedSize
    };

    static const uint32_t kInvalidIndex = 0xffffffff;

    /** Create a new query heap.
        \param[in] type Type of queries.
        \param[in] count Number of queries.
        \return New object, or throws an exception if creation failed.
    */
    static SharedPtr create(std::shared_ptr<Device> pDevice, Type type, uint32_t count) { return SharedPtr(new QueryHeap(pDevice, type, count)); }

    inline const ApiHandle& getApiHandle() const { return mApiHandle; }
    inline uint32_t getQueryCount() const { return mCount; }
    inline Type getType() const { return mType; }

    /** Allocates a new query.
        \return Query index, or kInvalidIndex if out of queries.
    */
    inline uint32_t allocate() {
        if (mFreeQueries.size()) {
            uint32_t entry = mFreeQueries.front();
            mFreeQueries.pop_front();
            return entry;
        }
        if (mCurrentObject < mCount) {
            return mCurrentObject++;
        } else {
            return kInvalidIndex;
        }
    }

    inline void release(uint32_t entry) {
        assert(entry != kInvalidIndex);
        mFreeQueries.push_back(entry);
    }

 private:
    QueryHeap(std::shared_ptr<Device> pDevice, Type type, uint32_t count);
    ApiHandle mApiHandle;
    uint32_t mCount = 0;
    uint32_t mCurrentObject = 0;
    std::deque<uint32_t> mFreeQueries;
    Type mType;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_QUERYHEAP_H_
