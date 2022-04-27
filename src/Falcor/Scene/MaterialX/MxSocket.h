#ifndef SRC_FALCOR_SCENE_MATERIALX_MXSOCKET_H_
#define SRC_FALCOR_SCENE_MATERIALX_MXSOCKET_H_

#include <memory>

#include <boost/format.hpp>

#include "Falcor/Core/Framework.h"
#include "MxTypes.h"

namespace Falcor {

class MxNode;

class dlldecl MxSocket : public std::enable_shared_from_this<MxSocket> {
  public:

    using Direction = MxSocketDirection;
    using DataType = MxSocketDataType;

    using SharedPtr = std::shared_ptr<MxSocket>;
    using SharedConstPtr = std::shared_ptr<const MxSocket>;

    const std::string& name() const { return mName; }
    std::shared_ptr<MxNode> node() const { return mpNode; }

    const std::string path() const;

    Direction direction() const { return mDirection; }
    DataType  dataType() const { return mDataType; }

    bool setInput(MxSocket::SharedPtr pSocket);

    ~MxSocket();

  private:
    MxSocket(std::shared_ptr<MxNode> pNode, const std::string& name, DataType dataType, Direction direction);

    static MxSocket::SharedPtr create(std::shared_ptr<MxNode> pNode, const std::string& name, DataType dataType, Direction direction);

    std::shared_ptr<MxNode> mpNode = nullptr;
    std::string mName;
    DataType    mDataType;
    Direction   mDirection;

    MxSocket::SharedPtr mInput = nullptr;

    friend class MxNode;
};

inline std::string to_string(MxSocketDirection dir) {
  if (dir == MxSocketDirection::INPUT) return "input";
  return "output";
}


}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_MATERIALX_MXSOCKET_H_