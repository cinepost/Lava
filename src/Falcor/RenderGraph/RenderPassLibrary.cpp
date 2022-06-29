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
#include <fstream>
#include <vector>
#include <map>

#ifdef _MSC_VER
#include <filesystem>
namespace fs = filesystem;
#else
#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;
#endif

#include "Falcor/stdafx.h"

#include "Falcor/Core/API/RenderContext.h"
#include "Falcor/Utils/StringUtils.h"
#include "RenderPasses/ResolvePass.h"
#include "Falcor/Core/API/Device.h"
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Core/Platform/OS.h"
#include "RenderGraph.h"

#include "RenderPassLibrary.h"

namespace Falcor {

    extern std::vector<RenderGraph*> gRenderGraphs;

    #ifdef _MSC_VER
    static const std::string kPassLibExt = ".dll";
    #else
    static const std::string kPassLibExt = ".rpl";
    #endif

    static const std::string kPassTempLibSuffix = ".falcor";

    RenderPassLibrary* RenderPassLibrary::spInstance = nullptr;

    template<typename Pass>
    using PassFunc = typename Pass::SharedPtr(*)(RenderContext* pRenderContext, const Dictionary&);

    RenderPassLibrary& RenderPassLibrary::instance()
    {
        if (!spInstance) spInstance = new RenderPassLibrary;
        return *spInstance;
    }

    
    RenderPassLibrary::~RenderPassLibrary()
    {
        mPasses.clear();
        while (mLibs.size()) releaseLibrary(mLibs.begin()->first);
    }

    void RenderPassLibrary::shutdown()
    {
        safe_delete(spInstance);
    }

    static bool addBuiltinPasses()
    {
        auto& lib = RenderPassLibrary::instance();

        lib.registerPass(ResolvePass::kInfo, ResolvePass::create);

        return true;
    };

    static const bool b = addBuiltinPasses();

    RenderPassLibrary& RenderPassLibrary::registerPass(const RenderPass::Info& info, CreateFunc func) {
        registerInternal(info, func, nullptr);
        return *this;
    }

    void RenderPassLibrary::registerInternal(const RenderPass::Info& info, CreateFunc func, SharedLibraryHandle library)
    {
        if (mPasses.find(info.type) != mPasses.end())
        {
            LLOG_WRN << "Trying to register a render-pass '" << info.type << "' to the render-passes library, but a render-pass with the same name already exists. Ignoring the new definition.";
        }
        else
        {
            mPasses[info.type] = ExtendedDesc(info, func, library);
        }
    }

    std::shared_ptr<RenderPass> RenderPassLibrary::createPass(RenderContext* pRenderContext, const char* className, const Dictionary& dict) {
        if (mPasses.find(className) == mPasses.end()) {
            // See if we can load a DLL with the class's name and retry
            std::string libName = std::string(className) + kPassLibExt;
            LLOG_INF << "Can't find a render-pass named `" << std::string(className) << "`. Trying to load a render-pass library `" << libName << '`' << kPassLibExt;
            loadLibrary(pRenderContext->device(), libName);

            if (mPasses.find(className) == mPasses.end()) {
                LLOG_WRN << "Trying to create a render-pass named `" << std::string(className) << "`, but no such class exists in the library";
                return nullptr;
            }
        }

        auto& renderPass = mPasses[className];
        return renderPass.func(pRenderContext, dict);
    }

    RenderPassLibrary::DescVec RenderPassLibrary::enumerateClasses() const {
        DescVec v;
        v.reserve(mPasses.size());
        for (const auto& p : mPasses) v.push_back(p.second);
        return v;
    }

    RenderPassLibrary::StrVec RenderPassLibrary::enumerateLibraries() {
        StrVec libNames;
        for (const auto& lib : spInstance->mLibs) {
            libNames.push_back(lib.first);
        }
        return libNames;
    }

    void copyDllFile(const std::string& fullpath) {
        std::ifstream src(fullpath, std::ios::binary);
        std::ofstream dst(fullpath + kPassTempLibSuffix, std::ios::binary);
        dst << src.rdbuf();
        dst.flush();
        dst.close();
    }

    void RenderPassLibrary::loadLibrary(Device::SharedPtr pDevice, const std::string& filename) {
        fs::path filePath = filename;
        
        // render-pass name was privided without an extension and that's fine
        if (filePath.extension() != kPassLibExt) filePath += kPassLibExt;

        //std::string fullpath = getExecutableDirectory() + "/render_passes/" + getFilenameFromPath(filePath.string());
        std::string fullpath;
        
        if (!findFileInRenderPassDirectories(filePath.string(), fullpath)) {
            LLOG_WRN << "Can't load render-pass library `" << filePath.string() << "`. File not found";
            return;
        }

        if (mLibs.find(fullpath) != mLibs.end()) {
            LLOG_INF << "Render-pass library `" << fullpath << "` already loaded. Ignoring `loadLibrary()` call";
            return;
        }

        // Copy the library to a temp file
        copyDllFile(fullpath);

        DllHandle l = loadDll(fullpath + kPassTempLibSuffix);
        mLibs[fullpath] = { l, getFileModifiedTime(fullpath) };
        auto func = (LibraryFunc)getDllProcAddress(l, "getPasses");

        if(!func) {
            LLOG_ERR << "RenderPass library getPasses proc address is NULL !!!";
        }

        // Add the DLL project directory to the search paths
        if (isDevelopmentMode()) {
            auto libProjPath = (const char*(*)(void))getDllProcAddress(l, "getProjDir");
            if (libProjPath) {
                const char* projDir = libProjPath();
                addDataDirectory(std::string(projDir) + "/Data/");
            }
        }

        RenderPassLibrary lib;

        try {
            func(lib);
        } catch (...) {
            LLOG_ERR << "Can't get passes from library " << fullpath;
            throw;
        }

        for (auto& p : lib.mPasses) {
            const auto& desc = p.second;
            registerInternal(desc.info, desc.func, l);
        }

        mLibDevices[filename].push_back(pDevice);
    }

    void RenderPassLibrary::releaseLibrary(const std::string& filename) {
        std::string fullpath;// = getExecutableDirectory() + "/render_passes/" + getFilenameFromPath(filename);
        if(!findFileInRenderPassDirectories(filename, fullpath)) {
            should_not_get_here();
        }

        auto libIt = mLibs.find(fullpath);
        if (libIt == mLibs.end()) {
            LLOG_WRN << "Can't unload render-pass library `" << fullpath << "`. The library wasn't loaded";
            return;
        }

        for (auto pDevice: mLibDevices[filename]) {
            pDevice->flushAndSync();
        }

        // Delete all the classes that were owned by the module
        SharedLibraryHandle library = libIt->second.library;
        for (auto it = mPasses.begin(); it != mPasses.end();) {
            if (it->second.library == library) it = mPasses.erase(it);
            else ++it;
        }

        // Remove the DLL project directory to the search paths
        if (isDevelopmentMode()) {
            auto libProjPath = (const char*(*)(void))getDllProcAddress(library, "getProjDir");
            if (libProjPath) {
                const char* projDir = libProjPath();
                removeDataDirectory(std::string(projDir) + "/Data/");
            }
        }

        releaseDll(library);
        std::remove((fullpath + kPassTempLibSuffix).c_str());
        mLibs.erase(libIt);
    }

    void RenderPassLibrary::reloadLibrary(RenderContext* pRenderContext, const std::string& filename) {
        assert(pRenderContext);

        auto lastTime = getFileModifiedTime(filename);
        if ((lastTime == mLibs[filename].lastModified) || (lastTime == 0)) return;

        SharedLibraryHandle library = mLibs[filename].library;

        struct PassesToReplace {
            RenderGraph* pGraph;
            std::string className;
            uint32_t nodeId;
        };

        std::vector<PassesToReplace> passesToReplace;

        for (auto& passDesc : mPasses) {
            if (passDesc.second.library != library) continue;

            // Go over all the graphs and remove this pass
            for (auto& pGraph : gRenderGraphs) {
                // Loop over the passes
                for (auto& node : pGraph->mNodeData) {
                    if (getClassTypeName(node.second.pPass.get()) == passDesc.first) {
                        passesToReplace.push_back({ pGraph, passDesc.first, node.first });
                        node.second.pPass = nullptr;
                        pGraph->mpExe.reset();
                    }
                }
            }
        }

        // OK, we removed all the passes. Reload the library
        releaseLibrary(filename);
        loadLibrary(pRenderContext->device(), filename);

        // Recreate the passes
        for (auto& r : passesToReplace) {
            r.pGraph->mNodeData[r.nodeId].pPass = createPass(pRenderContext, r.className.c_str());
            r.pGraph->mpExe = nullptr;
        }
    }

    void RenderPassLibrary::reloadLibraries(RenderContext* pRenderContext) {
        // Copy the libs vector so we don't screw up the iterator
        auto libs = mLibs;
        for (const auto& l : libs) reloadLibrary(pRenderContext, l.first);
    }
}
