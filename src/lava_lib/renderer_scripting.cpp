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
#include <string>

#include "Falcor/Core/API/Device.h"
#include "Falcor/Utils/Scripting/ScriptBindings.h"

#include "renderer.h"


namespace lava {

namespace {

const std::string kRunScript = "script";
const std::string kLoadScene = "loadScene";
const std::string kSaveConfig = "saveConfig";
const std::string kAddGraph = "addGraph";
const std::string kRemoveGraph = "removeGraph";
const std::string kGetGraph = "getGraph";
const std::string kGetDevice = "getDevice";
const std::string kUI = "ui";
const std::string kResizeSwapChain = "resizeSwapChain";
const std::string kActiveGraph = "activeGraph";
const std::string kScene = "scene";

const std::string kRendererVar = "m";
const std::string kTimeVar = "t";

}

void Renderer::registerBindings(pybind11::module& m) {
    pybind11::class_<Renderer> renderer(m, "Renderer");

    auto getDevice = [](Renderer* pRenderer) { return pRenderer->device(); };
    renderer.def(kGetDevice.c_str(), getDevice);

    renderer.def(kAddGraph.c_str(), &Renderer::addGraph, "graph"_a);
    auto getUI = [](Renderer* pRenderer) { return Falcor::gpFramework->isUiEnabled(); };
    auto setUI = [](Renderer* pRenderer, bool show) { Falcor::gpFramework->toggleUI(show); };
    renderer.def_property(kUI.c_str(), getUI, setUI);



    auto objectHelp = [](pybind11::object o) {
        auto b = pybind11::module::import("builtins");
        auto h = b.attr("help");
        h(o);
    };
    m.def("help", objectHelp, "object"_a);

    // PYTHONDEPRECATED Use the global function defined in the script bindings in Sample.cpp when resizing from a Python script.
    //auto resize = [](Renderer* pRenderer, uint32_t width, uint32_t height) {gpFramework->resizeSwapChain(width, height); };
    //c.func_(kResizeSwapChain.c_str(), resize);
}

}  // namespace lava
