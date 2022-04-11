#ifndef SRC_FALCOR_SCENE_MATERIALX_MXNODE_H_
#define SRC_FALCOR_SCENE_MATERIALX_MXNODE_H_

#include <memory>
#include <boost/format.hpp>

#include "Falcor/Core/Framework.h"

#include "MxSocket.h"


namespace Falcor {

class MxGeneratorBase;

class dlldecl MxNode : public std::enable_shared_from_this<MxNode> {
  public:

    struct TypeCreateInfo {
      std::string nameSpace;
      std::string typeName;
      uint8_t     version;

      bool operator==(const TypeCreateInfo& other) const;
      bool operator!=(const TypeCreateInfo& other) const;
    };

    using UniquePtr = std::unique_ptr<MxNode>;
    using SharedPtr = std::shared_ptr<MxNode>;
    using SharedConstPtr = std::shared_ptr<const MxNode>;

    const std::string& name() const { return mName; }
    const std::string path() const;

    bool hasParent() const { return mpParent != nullptr; }

    MxNode::SharedPtr parent() const { return mpParent; }

    /** Comparison operator
    */
    bool operator==(const MxNode& other) const;

    MxNode::SharedPtr createNode(const TypeCreateInfo& info, const std::string& name);

    const std::vector<MxNode::SharedPtr>& children() const { return mChildNodes; };

    MxNode::SharedPtr node(const std::string& name); 
    MxNode::SharedConstPtr node(const std::string& name) const; 

    MxSocket::SharedPtr addInputSocket(const std::string& name, MxSocket::DataType dataType);
    MxSocket::SharedPtr addOutputSocket(const std::string& name, MxSocket::DataType dataType);

    MxSocket::SharedPtr addDataSocket(const std::string& name, MxSocket::DataType dataType, MxSocket::Direction direction);

    MxSocket::SharedPtr socket(const std::string& name, MxSocket::Direction direction);
    
    MxSocket::SharedPtr inputSocket(const std::string& name);
    MxSocket::SharedPtr outputSocket(const std::string& name);

    const std::string& nameSpace() const { return mInfo.nameSpace; }
    const std::string& typeName() const { return mInfo.typeName; }

    const TypeCreateInfo& info() const { return mInfo; }

    ~MxNode();

  private:
    MxNode(const TypeCreateInfo& info, const std::string& name, MxNode::SharedPtr pParent);

    static MxNode::SharedPtr create(const TypeCreateInfo& info, const std::string& name, MxNode::SharedPtr pParent);

    std::string mName;
    TypeCreateInfo mInfo;
    MxNode::SharedPtr mpParent = nullptr;

    std::map<std::string, MxNode::SharedPtr>   mChildNodesMap;
    std::vector<MxNode::SharedPtr>             mChildNodes;

    std::shared_ptr<MxGeneratorBase> mpGenerator = nullptr;

    std::map<std::string, MxSocket::SharedPtr> mInputs;
    std::map<std::string, MxSocket::SharedPtr> mOutputs;

    friend class SceneCache;
    friend class MaterialX;
    friend class MxGeneratorsLibrary;
};

inline std::string to_string(MxNode::TypeCreateInfo info) {
  return boost::str(boost::format("%1%::%2%::%3%") % info.nameSpace % info.typeName % info.version);
}


}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_MATERIALX_MXNODE_H_