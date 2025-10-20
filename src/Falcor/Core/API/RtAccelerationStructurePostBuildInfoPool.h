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
#ifndef SRC_FALCOR_CORE_API_RTACCELERATIONSTRUCTUREPOOL_H_
#define SRC_FALCOR_CORE_API_RTACCELERATIONSTRUCTUREPOOL_H_

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/Object.h"


namespace Falcor {

class Device;
class CopyContext;

enum class RtAccelerationStructurePostBuildInfoQueryType {
  CompactedSize,
  SerializationSize,
  CurrentSize,
};

#if defined(FALCOR_VK)

struct AccelerationStructureQueryDesc {
    RtQueryPool::QueryType queryType;
    RtQueryPool* queryPool; 
    int firstQueryIndex;
};

#endif

class FALCOR_API RtAccelerationStructurePostBuildInfoPool : public Object {
    FALCOR_OBJECT(RtAccelerationStructurePostBuildInfoPool)
  public:
    struct Desc {
      RtAccelerationStructurePostBuildInfoQueryType queryType;
      uint32_t elementCount;
    };

    static SharedPtr create(Device* pDevice, const Desc& desc);
    ~RtAccelerationStructurePostBuildInfoPool();
    uint64_t getElement(CopyContext* pContext, uint32_t index);
    void reset(CopyContext* pContext);

    gfx::IQueryPool* getGFXQueryPool() const { return mpGFXQueryPool.get(); }

  protected:
    RtAccelerationStructurePostBuildInfoPool(Device* pDevice, const Desc& desc);

  private:
    Desc mDesc;

    Slang::ComPtr<gfx::IQueryPool> mpGFXQueryPool;
    bool mNeedFlush = true;
};

struct RtAccelerationStructurePostBuildInfoDesc {
  RtAccelerationStructurePostBuildInfoQueryType type;
  RtAccelerationStructurePostBuildInfoPool* pool;
  uint32_t index;
};

}

#endif  // SRC_FALCOR_CORE_API_RTACCELERATIONSTRUCTUREPOOL_H_