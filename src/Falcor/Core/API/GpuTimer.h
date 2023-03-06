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
#ifndef SRC_FALCOR_CORE_API_GPUTIMER_H_
#define SRC_FALCOR_CORE_API_GPUTIMER_H_

#include "Falcor/Core/API/LowLevelContextData.h"
#include "Falcor/Core/API/QueryHeap.h"
#include "Falcor/Core/API/Buffer.h"


namespace Falcor { 

class Device;

/** Abstracts GPU timer queries. \n
    This class provides mechanism to get elapsed time in miliseconds between a pair of Begin()/End() calls.
*/
class dlldecl GpuTimer : public std::enable_shared_from_this<GpuTimer> {
 public:
    using SharedPtr = std::shared_ptr<GpuTimer>;
    using SharedConstPtr = std::shared_ptr<const GpuTimer>;

    /** Create a new timer object.
        \return A new object, or throws an exception if creation failed.
    */
    static SharedPtr create(std::shared_ptr<Device> pDevice);

    /** Destroy a new object
    */
    ~GpuTimer();

    /** Begin the capture window. \n
        If begin() is called in the middle of a begin()/end() pair, it will be ignored and a warning will be logged.
    */
    void begin();

    /** Begin the capture window. \n
        If end() is called before a begin() was called, it will be ignored and a warning will be logged.
    */
    void end();

     /** Resolve time stamps.
        This must be called after a pair of begin()/end() calls.
        A new measurement can be started after calling resolve() even before getElapsedTime() is called.
    */
    void resolve();

    /** Get the elapsed time in miliseconds between a pair of Begin()/End() calls. \n
        If this function called not after a Begin()/End() pair, zero will be returned and a warning will be logged.
    */
    double getElapsedTime();

 private:
    GpuTimer(std::shared_ptr<Device> pDevice);

    enum Status {
        Begin,
        End,
        Idle
    } mStatus = Idle;

    static std::weak_ptr<QueryHeap> spHeap;
    LowLevelContextData::SharedPtr mpLowLevelData;
    uint32_t mStart = 0;
    uint32_t mEnd = 0;
    double mElapsedTime = 0.0;
    bool mDataPending = false; ///< Set to true when resolved timings are available for readback.

    std::shared_ptr<Device> mpDevice;

    void apiBegin();
    void apiEnd();
    void apiResolve();
    void apiReadback(uint64_t result[2]);
};

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_GPUTIMER_H_

