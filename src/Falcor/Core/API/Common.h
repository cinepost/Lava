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
#ifndef SRC_FALCOR_CORE_API_COMMON_H_
#define SRC_FALCOR_CORE_API_COMMON_H_

#include <string>

#include "Falcor/Core/Enum.h"

namespace Falcor {

/// Buffer memory types.
enum class MemoryType {
    DeviceLocal, ///< Device local memory. The buffer can be updated using Buffer::setBlob().
    Upload,      ///< Upload memory. The buffer can be mapped for CPU writes.
    ReadBack,    ///< Read-back memory. The buffer can be mapped for CPU reads.

    // NOTE: In older version of Falcor this enum used to be Buffer::CpuAccess.
    // Use the following mapping to update your code:
    // - CpuAccess::None -> MemoryType::DeviceLocal
    // - CpuAccess::Write -> MemoryType::Upload
    // - CpuAccess::Read -> MemoryType::ReadBack
};

FALCOR_ENUM_INFO(
    MemoryType,
    {
        {MemoryType::DeviceLocal, "DeviceLocal"},
        {MemoryType::Upload, "Upload"},
        {MemoryType::ReadBack, "ReadBack"},
    }
);
FALCOR_ENUM_REGISTER(MemoryType);

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_COMMON_H_
