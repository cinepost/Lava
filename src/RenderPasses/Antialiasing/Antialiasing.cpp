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
#include "FXAA/FXAA.h"
#include "TAA/TAA.h"

extern "C" falcorexport const char* getProjDir() {
    return PROJECT_DIR;
}

static void regFXAA(ScriptBindings::Module& m) {
    auto c = m.regClass(FXAA);
    c.property("qualitySubPix", &FXAA::getQualitySubPix, &FXAA::setQualitySubPix);
    c.property("edgeThreshold", &FXAA::getQualityEdgeThreshold, &FXAA::setQualityEdgeThreshold);
    c.property("edgeThresholdMin", &FXAA::getQualityEdgeThresholdMin, &FXAA::setQualityEdgeThresholdMin);
    c.property("earlyOut", &FXAA::getEarlyOut, &FXAA::setEarlyOut);
}

static void regTAA(ScriptBindings::Module& m) {
    auto c = m.regClass(TAA);
    c.property("alpha", &TAA::getAlpha, &TAA::setAlpha);
    c.property("sigma", &TAA::getColorBoxSigma, &TAA::setColorBoxSigma);
}

extern "C" falcorexport void getPasses(Falcor::RenderPassLibrary& lib) {
    lib.registerClass("FXAA", "Fast Approximate Anti-Aliasing", FXAA::create);
    lib.registerClass("TAA", "Temporal Anti-Aliasing", TAA::create);
    ScriptBindings::registerBinding(regFXAA);
    ScriptBindings::registerBinding(regTAA);
}
