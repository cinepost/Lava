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
#include "stdafx.h"
#include "PixelDebug.h"
#include <sstream>
#include <iomanip>

namespace Falcor {

typedef unsigned char byte;

namespace {
    const char kReflectPixelDebugTypesFile[] = "Utils/Debug/ReflectPixelDebugTypes.cs.slang";
}

PixelDebug::SharedPtr PixelDebug::create(std::shared_ptr<Device> pDevice, uint32_t logSize) {
    return SharedPtr(new PixelDebug(pDevice, logSize));
}

void PixelDebug::beginFrame(RenderContext* pRenderContext, const uint2& frameDim) {
    mFrameDim = frameDim;
    if (mRunning) {
        logError("PixelDebug::beginFrame() - Logging is already running, did you forget to call endFrame()? Ignoring call.");
        return;
    }
    mRunning = true;

    // Reset previous data.
    mPixelLogData.clear();
    mAssertLogData.clear();
    mDataValid = false;
    mWaitingForData = false;

    if (mEnabled) {
        // Prepare log buffers.
        if (!mpPixelLog || mpPixelLog->getElementCount() != mLogSize) {
            // Create program for type reflection.
            if (!mpReflectProgram) mpReflectProgram = ComputeProgram::createFromFile(mpDevice, kReflectPixelDebugTypesFile, "main");

            // Allocate GPU buffers.
            mpPixelLog = Buffer::createStructured(mpDevice, mpReflectProgram.get(), "gPixelLog", mLogSize);
            if (mpPixelLog->getStructSize() != sizeof(PixelLogValue)) throw std::runtime_error("Struct PixelLogValue size mismatch between CPU/GPU");

            mpAssertLog = Buffer::createStructured(mpDevice, mpReflectProgram.get(), "gAssertLog", mLogSize);
            if (mpAssertLog->getStructSize() != sizeof(AssertLogValue)) throw std::runtime_error("Struct AssertLogValue size mismatch between CPU/GPU");

            // Allocate staging buffers for readback. These are shared, the data is stored consecutively.
            mpCounterBuffer = Buffer::create(mpDevice, 2 * sizeof(uint32_t), ResourceBindFlags::None, Buffer::CpuAccess::Read);
            mpDataBuffer = Buffer::create(mpDevice, mpPixelLog->getSize() + mpAssertLog->getSize(), ResourceBindFlags::None, Buffer::CpuAccess::Read);
        }

        pRenderContext->clearUAVCounter(mpPixelLog, 0);
        pRenderContext->clearUAVCounter(mpAssertLog, 0);
    }
}

void PixelDebug::endFrame(RenderContext* pRenderContext) {
    if (!mRunning) {
        logError("PixelDebug::endFrame() - Logging is not running, did you forget to call beginFrame()? Ignoring call.");
        return;
    }
    mRunning = false;

    if (mEnabled) {
        // Copy logged data to staging buffers.
        pRenderContext->copyBufferRegion(mpCounterBuffer.get(), 0, mpPixelLog->getUAVCounter().get(), 0, 4);
        pRenderContext->copyBufferRegion(mpCounterBuffer.get(), 4, mpAssertLog->getUAVCounter().get(), 0, 4);
        pRenderContext->copyBufferRegion(mpDataBuffer.get(), 0, mpPixelLog.get(), 0, mpPixelLog->getSize());
        pRenderContext->copyBufferRegion(mpDataBuffer.get(), mpPixelLog->getSize(), mpAssertLog.get(), 0, mpAssertLog->getSize());

        // Create fence first time we need it.
        if (!mpFence) mpFence = GpuFence::create(mpDevice);

        // Submit command list and insert signal.
        pRenderContext->flush(false);
        mpFence->gpuSignal(pRenderContext->getLowLevelData()->getCommandQueue());

        mWaitingForData = true;
    }
}

void PixelDebug::prepareProgram(const Program::SharedPtr& pProgram, const ShaderVar& var) {
    assert(mRunning);

    if (mEnabled) {
        pProgram->addDefine("_PIXEL_DEBUG_ENABLED");
        var["gPixelLog"] = mpPixelLog;
        var["gAssertLog"] = mpAssertLog;
        var["PixelDebugCB"]["gPixelLogSelected"] = mSelectedPixel;
        var["PixelDebugCB"]["gPixelLogSize"] = mLogSize;
        var["PixelDebugCB"]["gAssertLogSize"] = mLogSize;
    } else {
        pProgram->removeDefine("_PIXEL_DEBUG_ENABLED");
    }
}

void PixelDebug::copyDataToCPU() {
    assert(!mRunning);
    if (mWaitingForData) {
        // Wait for signal.
        mpFence->syncCpu();
        mWaitingForData = false;

        if (mEnabled) {
            // Map counter buffer. This tells us how many print() and assert() calls were made.
            uint32_t* uavCounters = (uint32_t*)mpCounterBuffer->map(Buffer::MapType::Read);
            const uint32_t printCount = std::min(mpPixelLog->getElementCount(), uavCounters[0]);
            const uint32_t assertCount = std::min(mpAssertLog->getElementCount(), uavCounters[1]);
            mpCounterBuffer->unmap();

            // Map the data buffer and copy the relevant sections.
            byte* pLog = (byte*)mpDataBuffer->map(Buffer::MapType::Read);

            mPixelLogData.resize(printCount);
            for (uint32_t i = 0; i < printCount; i++) mPixelLogData[i] = ((PixelLogValue*)pLog)[i];
            pLog += mpPixelLog->getSize();

            mAssertLogData.resize(assertCount);
            for (uint32_t i = 0; i < assertCount; i++) mAssertLogData[i] = ((AssertLogValue*)pLog)[i];

            mpDataBuffer->unmap();
            mDataValid = true;
        }
    }
}

}  // namespace Falcor
