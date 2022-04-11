#ifndef SRC_FALCOR_SCENE_MATERIALX_MXSYSTEM_H_
#define SRC_FALCOR_SCENE_MATERIALX_MXSYSTEM_H_

#include <memory>
#include <string>
#include <map>

#include "Falcor/Core/Framework.h"
#include "Falcor/Utils/Scripting/Dictionary.h"

#include "MaterialX.h"
#include "MxNode.h"

namespace Falcor {

class Device;

class dlldecl MxSystem {
  public:
    MxSystem() = default;
    MxSystem(MxSystem&) = delete;
    ~MxSystem();
    
    /** Get an instance of the library. It's a singleton, you'll always get the same object
    */
    static MxSystem& instance();

    /** Call this before the app is shutting down to release all the libraries
    */
    void shutdown();

    MaterialX::SharedPtr createMaterial(std::shared_ptr<Device> pDevice, const std::string& name);

  private:
    std::map<std::string, MaterialX::SharedPtr> mMaterialsMap;
    std::map<std::string, MxNode::SharedPtr>    mNodesMap;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_MATERIALX_MXGENERATORSLIBRARY_H_