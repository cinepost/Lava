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

#include "Falcor/Utils/StringUtils.h"
#include "RenderGraphImportExport.h"
#include "RenderGraphIR.h"
#include <fstream>

namespace Falcor {

namespace {

void updateGraphStrings(std::string& graph, fs::path& path, std::string& func) {
    graph = graph.empty() ? "renderGraph" : graph;
    path = path.empty() ? fs::path(graph + ".py") : path;
    func = func.empty() ? RenderGraphIR::getFuncName(graph) : func;
}

#ifdef SCRIPTING
void runScriptFile(const fs::path& path, const std::string& custom) {
    fs::path resolvedPath = AssetResolver::getDefaultResolver().resolvePath(path);
    if (resolvedPath.empty()) {
        FALCOR_THROW("Can't find the file '{}'", path);
    }

    std::string script = readFile(resolvedPath) + custom;
    Scripting::runScript(script);
}
#endif

}  // namespace

bool loadFailed(std::exception e, const std::string& filename) {
    LLOG_ERR << e.what();

    return false;
    //auto res = msgBox(std::string("Error when importing graph from file `" + filename + "`\n" + e.what() + "\n\nWould you like to try and reload the file?").c_str(), MsgBoxType::YesNo);
    //return (res == MsgBoxButton::No);
}

RenderGraph::SharedPtr RenderGraphImporter::import(std::string graphName, fs::path path, std::string funcName) {
#ifdef SCRIPTING
    while(true) {
        try {
            updateGraphStrings(graphName, path, funcName);
            std::string custom;
            if (funcName.size()) custom += "\n" + graphName + '=' + funcName + "()";
            runScriptFile(path, custom);

            auto pGraph = Scripting::getGlobalContext().getObject<RenderGraph::SharedPtr>(graphName);
            if (!pGraph) throw("Unspecified error");

            pGraph->setName(graphName);
            return pGraph;
        } catch (const std::exception& e) {
            if (loadFailed(e, path)) return nullptr;
        }
    }
#else
    return nullptr;
#endif
}

std::vector<RenderGraph::SharedPtr> RenderGraphImporter::importAllGraphs(const fs::path& path) {
#ifdef SCRIPTING
    while(true) {
        try {
            runScriptFile(path, {});
            auto scriptObj = Scripting::getGlobalContext().getObjects<RenderGraph::SharedPtr>();
            std::vector<RenderGraph::SharedPtr> res;
            res.reserve(scriptObj.size());

            for (const auto& s : scriptObj) {
                s.obj->setName(s.name);
                res.push_back(s.obj);
            }

            return res;
        } catch (const std::exception& e) {
            if (loadFailed(e, path)) return {};
        }
    }
#else
    return std::vector<RenderGraph::SharedPtr>();
#endif
}

std::string RenderGraphExporter::getFuncName(const std::string& graphName) {
    return RenderGraphIR::getFuncName(graphName);
}

std::string RenderGraphExporter::getIR(const RenderGraph::SharedPtr& pGraph) {
    RenderGraphIR ir(pGraph->getName());

    // Add the passes
    for (const auto& node : pGraph->mNodeData) {
        const auto& nodeData = node.second;
        ir.createPass(nodeData.pPass->getType(), nodeData.name, nodeData.pPass->getProperties());
    }

    // Add the edges
    for (const auto& edge : pGraph->mEdgeData) {
        const auto& edgeData = edge.second;
        const auto& srcPass = pGraph->mNodeData[pGraph->mpGraph->getEdge(edge.first)->getSourceNode()].name;
        const auto& dstPass = pGraph->mNodeData[pGraph->mpGraph->getEdge(edge.first)->getDestNode()].name;
        std::string src = srcPass + (edgeData.srcField.size() ? '.' + edgeData.srcField : edgeData.srcField);
        std::string dst = dstPass + (edgeData.dstField.size() ? '.' + edgeData.dstField : edgeData.dstField);
        ir.addEdge(src, dst);
    }

    // Graph outputs
    for (const auto& out : pGraph->mOutputs) {
        std::string str = pGraph->mNodeData[out.nodeId].name + '.' + out.field;
        for (auto mask : out.masks) {
            ir.markOutput(str, mask);
        }
    }

    return ir.getIR();
}

bool RenderGraphExporter::save(const RenderGraph::SharedPtr& pGraph, fs::path path) {
    std::string ir = getIR(pGraph);
    std::string funcName;
    std::string graphName = pGraph->getName();
    updateGraphStrings(graphName, path, funcName);

    // Save it to file
    std::ofstream f(path);
    f << ir << std::endl;
    f << graphName << " = " << funcName + "()\n";
    // Try adding it to Mogwai
    f << "try: m.addGraph(" + graphName + ")\n";
    f << "except NameError: None\n";

    return true;
}

}  // namespace Falcor
