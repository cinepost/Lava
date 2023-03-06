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
#include <memory>

#include "Falcor/stdafx.h"

#include "GpuTimer.h"
#include "Buffer.h"
#include "Device.h"
#include "QueryHeap.h"
#include "RenderContext.h"

namespace Falcor {

std::weak_ptr<QueryHeap> GpuTimer::spHeap;

GpuTimer::SharedPtr GpuTimer::create(std::shared_ptr<Device> pDevice) {
    return SharedPtr(new GpuTimer(pDevice));
}

GpuTimer::GpuTimer(std::shared_ptr<Device> pDevice): mpDevice(pDevice) {
    // Create timestamp query heap upon first use.
    // We're allocating pairs of adjacent queries, so need our own heap to meet this requirement.
    if (spHeap.expired()) {
        spHeap = mpDevice->createQueryHeap(QueryHeap::Type::Timestamp, 16 * 1024);
    }
    auto pHeap = spHeap.lock();
    assert(pHeap);
    mStart = pHeap->allocate();
    mEnd = pHeap->allocate();
    if (mStart == QueryHeap::kInvalidIndex || mEnd == QueryHeap::kInvalidIndex) {
        throw std::runtime_error("Can't create GPU timer, no available timestamp queries.");
    }
    assert(mEnd == (mStart + 1));
    mpLowLevelData = mpDevice->getRenderContext()->getLowLevelData();
}

GpuTimer::~GpuTimer() {
    if (auto pHeap = spHeap.lock(); pHeap) {
        pHeap->release(mStart);
        pHeap->release(mEnd);
    }
}

void GpuTimer::begin() {
    if (mStatus == Status::Begin) {
        LLOG_WRN << "GpuTimer::begin() was followed by another call to GpuTimer::begin() without a GpuTimer::end() in-between. Ignoring call.";
        return;
    }

    if (mStatus == Status::End) {
        LLOG_WRN << "GpuTimer::begin() was followed by a call to GpuTimer::end() without querying the data first. The previous results will be discarded.";
    }
    mStatus = Status::Begin;
    apiBegin();
}

void GpuTimer::end() {
    if (mStatus != Status::Begin) {
        LLOG_WRN << "GpuTimer::end() was called without a preciding GpuTimer::begin(). Ignoring call.";
        return;
    }
    mStatus = Status::End;
    apiEnd();
}

void GpuTimer::resolve() {
    if (mStatus == Status::Begin) {
        throw std::runtime_error("GpuTimer::resolve() was called but the GpuTimer::end() wasn't called.");
    }
    else if (mStatus == Status::End)
    {
        apiResolve();

        mDataPending = true;
        mStatus = Status::Idle;
    }
    // If idle, do nothing.
    assert(mStatus == Status::Idle);
}


double GpuTimer::getElapsedTime() {
    if (mStatus == Status::Begin) {
        LLOG_WRN << "GpuTimer::getElapsedTime() was called but the GpuTimer::end() wasn't called. No data to fetch.";
        return 0.0;
    } else if (mStatus == Status::End) {
        LLOG_WRN << "GpuTimer::getElapsedTime() was called but the GpuTimer::resolve() wasn't called. No data to fetch.";
        return 0.0;
    }

    assert(mStatus == Status::Idle);
    if (mDataPending) {
        uint64_t result[2];
        apiReadback(result);

        double start = (double)result[0];
        double end = (double)result[1];
        double range = end - start;
        mElapsedTime = range * mpDevice->getGpuTimestampFrequency();
        mDataPending = false;
    }
    return mElapsedTime;
}

#ifdef SCRIPTING
SCRIPT_BINDING(GpuTimer) {
    pybind11::class_<GpuTimer, GpuTimer::SharedPtr>(m, "GpuTimer");
}
#endif

}  // namespace Falcor
