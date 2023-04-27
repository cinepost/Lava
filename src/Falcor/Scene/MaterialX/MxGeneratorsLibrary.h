#ifndef SRC_FALCOR_SCENE_MATERIALX_MXGENERATORSLIBRARY_H_
#define SRC_FALCOR_SCENE_MATERIALX_MXGENERATORSLIBRARY_H_

#include "Falcor/Utils/Scripting/Dictionary.h"
#include "MxGeneratorBase.h"

namespace Falcor {

// The specialized hash function for `unordered_map` keys
struct mx_generator_info_hash_fn {
  std::size_t operator() (const MxGeneratorBase::Info& info) const {
    return ((std::hash<std::string>()(info.nameSpace) 
          ^ (std::hash<std::string>()(info.typeName) << 1)) >> 1) 
          ^ (std::hash<uint8_t>()(info.version) << 1);
  }
};  

class dlldecl MxGeneratorsLibrary {
  public:
    MxGeneratorsLibrary() = default;
    MxGeneratorsLibrary(MxGeneratorsLibrary&) = delete;
    ~MxGeneratorsLibrary();
    using CreateFunc = std::function<MxGeneratorBase::SharedPtr(const Dictionary&)>;

    struct MxGeneratorDesc {
        MxGeneratorDesc(): info() { };
        MxGeneratorDesc(const MxGeneratorBase::Info& info_, CreateFunc func_) : info(info_), func(func_) {};

        const MxGeneratorBase::Info info;
        CreateFunc func = nullptr;
    };

    using DescVec = std::vector<MxGeneratorDesc>;

    /** Get an instance of the library. It's a singleton, you'll always get the same object
    */
    static MxGeneratorsLibrary& instance();

    /** Call this before the app is shutting down to release all the libraries
    */
    void shutdown();

    /** Add a new pass class to the library
    */
    MxGeneratorsLibrary& registerGenerator(const MxGeneratorBase::Info& info, CreateFunc func);

    /** Instantiate a new render pass object.
        \param[in] pRenderContext The render context.
        \param[in] className Render pass class name.
        \param[in] dict Dictionary for serialized parameters.
        \return A new object, or an exception is thrown if creation failed. Nullptr is returned if class name cannot be found.
    */
    MxGeneratorBase::SharedPtr createGenerator(const MxGeneratorBase::Info& info, const Dictionary& dict = {});

    /** Get a list of all the registered generators
    */
    DescVec enumerateGenerators() const;

    /** Load a new render-pass library (DLL/DSO)
    */
    void loadLibrary(const std::string& filename);

    /** Release a previously loaded render-pass library (DLL/DSO)
    */
    void releaseLibrary(const std::string& filename);

    /** Reload render-pass libraries
    */
    void reloadLibraries();

    /** A render-pass library should implement a function called `getPasses` with the following signature
    */
    using LibraryFunc = void(*)(MxGeneratorsLibrary& lib);

    using StrVec = std::vector<std::string>;

    /** Get list of registered render-pass libraries
    */
    static StrVec enumerateLibraries();

  private:

    static MxGeneratorsLibrary* spInstance;

    struct ExtendedDesc : MxGeneratorDesc {
      ExtendedDesc() {};
      ExtendedDesc(const MxGeneratorBase::Info& info_, CreateFunc func_, DllHandle module_) : MxGeneratorDesc(info_, func_), module(module_) {};

      DllHandle module = nullptr;

      ExtendedDesc& operator=(ExtendedDesc& a) { return a; }
      ExtendedDesc& operator=(const ExtendedDesc& a) { return *this; }

      // ExtendedDesc& operator=(ExtendedDesc&& other) { return *this; }
    };

    void registerInternal(const MxGeneratorBase::Info& info, CreateFunc func, DllHandle hmodule);

    struct LibDesc {
        DllHandle module;
        time_t lastModified;
    };
    std::unordered_map<std::string, LibDesc> mLibs;
    std::unordered_map<MxGeneratorBase::Info, ExtendedDesc, mx_generator_info_hash_fn> mGenerators;

    void reloadLibrary(std::string name);
};

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_MATERIALX_MXGENERATORSLIBRARY_H_