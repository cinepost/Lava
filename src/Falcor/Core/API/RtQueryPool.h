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
#ifndef SRC_FALCOR_CORE_API_RTQUERYPOOL_H_
#define SRC_FALCOR_CORE_API_RTQUERYPOOL_H_

#include <deque>
#include <memory>

#include <slang/slang.h>

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/RtAccelerationStructure.h"

namespace Falcor {

class Device;

class dlldecl RtQueryPool : public std::enable_shared_from_this<RtQueryPool> {
 public:
    using SharedPtr = std::shared_ptr<RtQueryPool>;
    using ApiHandle = QueryPoolHandle;

    enum class QueryType {
        Timestamp,
        CompactedSize,
        SerializationSize,
        CurrentSize,
    };

    struct Desc {
      QueryType type;
      uint32_t count;
    };

    /** Create a new query heap.
        \param[in] type Type of queries.
        \param[in] count Number of queries.
        \return New object, or throws an exception if creation failed.
    */
    static SharedPtr create(std::shared_ptr<Device> pDevice, const Desc& desc) { return SharedPtr(new RtQueryPool(pDevice, desc)); }

    const ApiHandle& getApiHandle() const { return mApiHandle; }
    
    int32_t getResult(int index, int count, uint64_t* data);
    SlangResult reset() { return SLANG_OK; }

 private:
    RtQueryPool(std::shared_ptr<Device> pDevice, const Desc& desc);
    
    std::shared_ptr<Device> mpDevice;
    ApiHandle mApiHandle;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_RTQUERYPOOL_H_
