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
#include "Clock.h"

namespace Falcor {

namespace {

constexpr char kTime[] = "time";
constexpr char kFrame[] = "frame";
constexpr char kFramerate[] = "framerate";
constexpr char kTimeScale[] = "timeScale";
constexpr char kExitTime[] = "exitTime";
constexpr char kExitFrame[] = "exitFrame";
constexpr char kPause[] = "pause";
constexpr char kPlay[] = "play";
constexpr char kStop[] = "stop";
constexpr char kStep[] = "step";


constexpr uint64_t kTicksPerSecond = 14400 * (1 << 16); // 14400 is a common multiple of our supported frame-rates. 2^16 gives 64K intra-frame steps

double timeFromFrame(uint64_t frame, uint64_t ticksPerFrame) {
    return double(frame * ticksPerFrame) / (double)kTicksPerSecond;
}

uint64_t frameFromTime(double seconds, uint64_t ticksPerFrame) {
    return uint64_t(seconds * (double)kTicksPerSecond) / ticksPerFrame;
}

}  // namespace

Clock::Clock(std::shared_ptr<Device> pDevice): mpDevice(pDevice) { setTime(0); }

Clock::~Clock() {
    mClockTextures = {};
}

Clock& Clock::setFramerate(uint32_t fps) {
    mFramerate = fps;
    mTicksPerFrame = 0;
    if(fps) {
        if (kTicksPerSecond % fps) logWarning("Clock::setFramerate() - requested FPS can't be accurately representated. Expect roudning errors");
        mTicksPerFrame = kTicksPerSecond / fps;
    }

    if(!mDeferredFrameID && !mDeferredTime) setTime(mTime.now);
    return *this;
}

Clock& Clock::setExitTime(double seconds) {
    mExitTime = seconds;
    mExitFrame = 0;
    return *this;
}

Clock& Clock::setExitFrame(uint64_t frame) {
    mExitFrame = frame;
    mExitTime = 0.0;
    return *this;
}

bool Clock::shouldExit() const {
    return ((mExitTime && getTime() >= mExitTime) || (mExitFrame && getFrame() >= mExitFrame));
}

Clock& Clock::tick() {
    if (mDeferredFrameID) setFrame(mDeferredFrameID.value());
    else if (mDeferredTime) setTime(mDeferredTime.value());
    else if(!mPaused) step();
    return *this;
}

void Clock::updateTimer() {
    mTimer.update();
    mRealtime.update(mRealtime.now + mTimer.delta());
}

void Clock::resetDeferredObjects() {
    mDeferredTime = std::nullopt;
    mDeferredFrameID = std::nullopt;
}

Clock& Clock::setTime(double seconds, bool deferToNextTick) {
    resetDeferredObjects();

    if (deferToNextTick) {
        mDeferredTime = seconds;
    } else {
        updateTimer();
        if (mFramerate) {
            mFrames = frameFromTime(seconds, mTicksPerFrame);
            seconds = timeFromFrame(mFrames, mTicksPerFrame);
        }
        else mFrames = 0;

        mTime.delta = mTime.now - seconds;
        mTime.now = seconds;
    }
    return *this;
}

Clock& Clock::setFrame(uint64_t f, bool deferToNextTick) {
    resetDeferredObjects();

    if (deferToNextTick) {
        mDeferredFrameID = f;
    } else {
        updateTimer();
        mFrames = f;
        if (mFramerate) {
            double secs = timeFromFrame(mFrames, mTicksPerFrame);
            mTime.delta = mTime.now - secs;
            mTime.now = secs;
        }
    }
    return *this;
}

Clock& Clock::play() {
    updateTimer();
    mPaused = false;
    return *this;
}

Clock& Clock::step(int64_t frames) {
    if (frames < 0 && uint64_t(-frames) > mFrames) mFrames = 0;
    else mFrames += frames;

    updateTimer();
    double t = isSimulatingFps() ? timeFromFrame(mFrames, mTicksPerFrame) : ((mTimer.delta() * mScale) + mTime.now);
    mTime.update(t);
    return *this;
}


#ifdef SCRIPTING
SCRIPT_BINDING(Clock) {
    pybind11::class_<Clock> clock(m, "Clock");

    auto setTime = [](Clock* pClock, double t) {pClock->setTime(t, true); };
    clock.def_property(kTime, &Clock::getTime, setTime);
    auto setFrame = [](Clock* pClock, uint64_t f) {pClock->setFrame(f, true); };
    clock.def_property(kFrame, &Clock::getFrame, setFrame);
    clock.def_property(kFramerate, &Clock::getFramerate, &Clock::setFramerate);
    clock.def_property(kTimeScale, &Clock::getTimeScale, &Clock::setTimeScale);
    clock.def_property(kExitTime, &Clock::getExitTime, &Clock::setExitTime);
    clock.def_property(kExitFrame, &Clock::getExitFrame, &Clock::setExitFrame);

    clock.def(kPause, &Clock::pause);
    clock.def(kPlay, &Clock::play);
    clock.def(kStop, &Clock::stop);
    clock.def(kStep, &Clock::step, "frames"_a = 1);
}
#endif

void Clock::start() {
    auto loadTexture = [](std::shared_ptr<Device> pDevice, const std::string& tex) {
        auto pTex = Texture::createFromFile(pDevice, "Framework/Textures/" + tex, false, true);
        if (!pTex) {
            LOG_ERR("Error loading texture: Framework/Textures/%s", tex.c_str());
            #ifdef _WIN32 
            throw std::exception("Failed to load texture");
            #else
            throw std::runtime_error("Failed to load texture " + tex);
            #endif
        }
        return pTex;
    };

    mClockTextures.pRewind = loadTexture(mpDevice, "Rewind.jpg");
    mClockTextures.pPlay = loadTexture(mpDevice, "Play.jpg");
    mClockTextures.pPause = loadTexture(mpDevice, "Pause.jpg");
    mClockTextures.pStop = loadTexture(mpDevice, "Stop.jpg");
    mClockTextures.pNextFrame = loadTexture(mpDevice, "NextFrame.jpg");
    mClockTextures.pPrevFrame = loadTexture(mpDevice, "PrevFrame.jpg");
}

void Clock::shutdown() {
    mClockTextures = {};
}

std::string Clock::getScript(const std::string& var) const {
    std::string s;
    s += Scripting::makeSetProperty(var, kTime, 0);
    s += Scripting::makeSetProperty(var, kFramerate, mFramerate);
    if (mExitTime) s += Scripting::makeSetProperty(var, kExitTime, mExitTime);
    if (mExitFrame) s += Scripting::makeSetProperty(var, kExitFrame, mExitFrame);
    s += std::string("# If ") + kFramerate + " is not zero, you can use the frame property to set the start frame\n";
    s += "# " + Scripting::makeSetProperty(var, kFrame, 0);
    if (mPaused) s += Scripting::makeMemberFunc(var, kPause);
    return s;
}

}  // namespace Falcor
