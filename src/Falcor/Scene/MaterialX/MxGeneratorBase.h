#ifndef SRC_FALCOR_SCENE_MATERIALX_MXGENERATORBASE_H_
#define SRC_FALCOR_SCENE_MATERIALX_MXGENERATORBASE_H_

#include <memory>

#include "Falcor/Core/Framework.h"

#include "Falcor/Utils/Scripting/Dictionary.h"
#include "Falcor/Utils/InternalDictionary.h"

#include "MxNode.h"


namespace Falcor {

class MxNode;

class dlldecl MxGeneratorBase : public std::enable_shared_from_this<MxGeneratorBase> {
  public:
    using SharedPtr = std::shared_ptr<MxGeneratorBase>;

    virtual ~MxGeneratorBase() = default;

    struct Info: MxNode::TypeCreateInfo {
      Info() = default; 
      
      bool operator<(const Info &i)  const {

        if (nameSpace < i.nameSpace) return true; else 
        if (nameSpace == i.nameSpace) {
          if (typeName < i.typeName) return true; else 
          if (typeName == i.typeName) {
            if (version < i.version) return true;
          }
        }
        return false;
      }

      bool operator==(const Info &i) const {
        return nameSpace == i.nameSpace && typeName == i.typeName && version == i.version;
      }
    };

    /** Will be called during graph compilation. You should throw an exception in case the compilation failed
    */
    virtual void generateCode() {};

  protected:
    friend class MxNode;
    MxGeneratorBase();
    
};

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_MATERIALX_MXGENERATORBASE_H_
