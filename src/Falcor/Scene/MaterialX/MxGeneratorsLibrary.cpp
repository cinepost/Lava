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
#include "Falcor/Utils/Debug/debug.h"
#include "Falcor/Core/Platform/OS.h"
#include "Falcor/Core/API/RenderContext.h"
#include "MxNode.h"

#include "MxGeneratorsLibrary.h"


namespace Falcor {

extern std::vector<MxNode*> gMxNodes;

#ifdef _MSC_VER
static const std::string kMxLibExt = ".dll";
#else
static const std::string kMxLibExt = ".mxl";
#endif

static const std::string kMxTempLibSuffix = ".falcor";

MxGeneratorsLibrary* MxGeneratorsLibrary::spInstance = nullptr;

template<typename Pass>
using PassFunc = typename Pass::SharedPtr(*)(RenderContext* pRenderContext, const Dictionary&);

#define addNodeClass(c, desc) registerNodeClass(#c, desc, (PassFunc<c>)c::create)

static bool addBuiltinGenerators() {
  auto& lib = MxGeneratorsLibrary::instance();

  //lib.addGenerator(ResolvePass, ResolvePass::kDesc);

  return true;
};

// static const bool b = addBuiltinPasses();

static void copyDllFile(const std::string& fullpath) {
    std::ifstream src(fullpath, std::ios::binary);
    std::ofstream dst(fullpath + kMxTempLibSuffix, std::ios::binary);
    dst << src.rdbuf();
    dst.flush();
    dst.close();
}

MxGeneratorsLibrary& MxGeneratorsLibrary::instance() {
  if (!spInstance) spInstance = new MxGeneratorsLibrary();
  return *spInstance;
}

MxGeneratorsLibrary::~MxGeneratorsLibrary() {
    mGenerators.clear();
    while (mLibs.size()) releaseLibrary(mLibs.begin()->first);
}

void MxGeneratorsLibrary::shutdown() {
    safe_delete(spInstance);
}

MxGeneratorsLibrary& MxGeneratorsLibrary::registerGenerator(const MxGeneratorBase::Info& info, CreateFunc func) {
    registerInternal(info, func, nullptr);
    return *this;
}

void MxGeneratorsLibrary::registerInternal(const MxGeneratorBase::Info& info, CreateFunc func, DllHandle module) {
    if (mGenerators.find(info) != mGenerators.end()) {
        logWarning("Trying to register a generator for type `" + std::string(info.typeName) + "` to the generators library,  but a generator type with the same type name already exists. Ignoring the new definition");
    } else {
        mGenerators[info] = ExtendedDesc(info, func, module);
    }
}

std::shared_ptr<MxGeneratorBase> MxGeneratorsLibrary::createGenerator(const MxGeneratorBase::Info& info, const Dictionary& dict) {
    if (mGenerators.find(info) == mGenerators.end()) {
        // See if we can load a DLL with the class's name and retry
        std::string libName = std::string(info.typeName) + kMxLibExt;
        logInfo("Can't find a xmaterial generator named: " + std::string(info.typeName) + ". Trying to load a library: `" + libName + '`' + kMxLibExt);
        loadLibrary(libName);

        if (mGenerators.find(info) == mGenerators.end()) {
            logWarning("Trying to create a generator named `" + std::string(info.typeName) + "`, but no such class exists in the library");
            return nullptr;
        }
    }

    auto& generator = mGenerators[info];
    return generator.func(dict);
}

MxGeneratorsLibrary::DescVec MxGeneratorsLibrary::enumerateGenerators() const {
    DescVec v;
    v.reserve(mGenerators.size());
    for (const auto& g : mGenerators) {
        v.push_back(g.second);
    }
    return v;
}

MxGeneratorsLibrary::StrVec MxGeneratorsLibrary::enumerateLibraries() {
    StrVec libNames;
    for (const auto& lib : spInstance->mLibs) {
        libNames.push_back(lib.first);
    }
    return libNames;
}

void MxGeneratorsLibrary::loadLibrary(const std::string& filename) {
    fs::path filePath = filename;
    
    // render-pass name was privided without an extension and that's fine
    if (filePath.extension() != kMxLibExt) filePath += kMxLibExt;

    //std::string fullpath = getExecutableDirectory() + "/render_passes/" + getFilenameFromPath(filePath.string());
    std::string fullpath;
    
    if (!findFileInRenderPassDirectories(filePath.string(), fullpath)) {
        logWarning("Can't load shader generator library `" + filePath.string() + "`. File not found");
        return;
    }

    if (mLibs.find(fullpath) != mLibs.end()) {
        logInfo("Shader generator library `" + fullpath + "` already loaded. Ignoring `loadLibrary()` call");
        return;
    }

    // Copy the library to a temp file
    copyDllFile(fullpath);

    DllHandle l = loadDll(fullpath + kMxTempLibSuffix);
    mLibs[fullpath] = { l, getFileModifiedTime(fullpath) };
    auto func = (LibraryFunc)getDllProcAddress(l, "getGenerators");

    if(!func) {
        LOG_ERR("RenderPass library getPasses proc address is NULL !!!");
    }

    // Add the DLL project directory to the search paths
    if (isDevelopmentMode()) {
        auto libProjPath = (const char*(*)(void))getDllProcAddress(l, "getProjDir");
        if (libProjPath) {
            const char* projDir = libProjPath();
            addDataDirectory(std::string(projDir) + "/Data/");
        }
    }

    auto& lib = MxGeneratorsLibrary::instance();

    try {
        func(lib);
    } catch (...) {
        logError("Can't get generators from library: " + fullpath);
        throw;
    }

    for (auto& g : lib.mGenerators) {
        registerInternal(g.second.info, g.second.func, l);
    }
}

void MxGeneratorsLibrary::releaseLibrary(const std::string& filename) {
    std::string fullpath;// = getExecutableDirectory() + "/render_passes/" + getFilenameFromPath(filename);
    if(!findFileInRenderPassDirectories(filename, fullpath)) {
        should_not_get_here();
    }

    auto libIt = mLibs.find(fullpath);
    if (libIt == mLibs.end()) {
        logWarning("Can't unload render-pass library `" + fullpath + "`. The library wasn't loaded");
        return;
    }

    // Delete all the classes that were owned by the module
    DllHandle module = libIt->second.module;
    for (auto it = mGenerators.begin(); it != mGenerators.end();) {
        if (it->second.module == module) it = mGenerators.erase(it);
        else ++it;
    }

    // Remove the DLL project directory to the search paths
    if (isDevelopmentMode()) {
        auto libProjPath = (const char*(*)(void))getDllProcAddress(module, "getProjDir");
        if (libProjPath) {
            const char* projDir = libProjPath();
            removeDataDirectory(std::string(projDir) + "/Data/");
        }
    }

    releaseDll(module);
    std::remove((fullpath + kMxTempLibSuffix).c_str());
    mLibs.erase(libIt);
}

void MxGeneratorsLibrary::reloadLibrary(std::string name) {
    
    auto lastTime = getFileModifiedTime(name);
    if ((lastTime == mLibs[name].lastModified) || (lastTime == 0)) return;

    DllHandle module = mLibs[name].module;

    struct NodesToReplace {
        MxNode* pNode;
        MxGeneratorBase::Info info;
        uint32_t nodeId;
    };

    std::vector<NodesToReplace> nodesToReplace;

    for (auto& g : mGenerators) {
        if (g.second.module != module) continue;

        // Go over all the graphs and remove this pass
        for (auto& pNode : gMxNodes) {
            //if (g.second.info == pNode.mpGenerator.info())

            // Loop over the passes
            /*
            for (auto& node : pGraph->mNodeData) {
                if (getClassTypeName(node.second.pPass.get()) == passDesc.first) {
                    nodesToReplace.push_back({ pGraph, passDesc.first, node.first });
                    node.second.pPass = nullptr;
                    pNode->mpExe.reset();
                }
            }
            */
        }
    }

    // OK, we removed all the passes. Reload the library
    releaseLibrary(name);
    loadLibrary(name);

    // Recreate the passes
    for (auto& n : nodesToReplace) {
        n.pNode->mpGenerator = createGenerator(n.info, {});
    }
}

void MxGeneratorsLibrary::reloadLibraries() {
  // Copy the libs vector so we don't screw up the iterator
  auto libs = mLibs;
  for (const auto& l : libs) {
    reloadLibrary(l.first);
  }
}

}  // namespace Falcor
