#ifndef SRC_FALCOR_SCENE_MATERIALX_MXSOCKET_H_
#define SRC_FALCOR_SCENE_MATERIALX_MXSOCKET_H_

#include <memory>

#include <boost/format.hpp>

#include "Falcor/Core/Framework.h"

#include "lava_lib/reader_lsd/scope.h"


namespace Falcor {

class MxNode;

class dlldecl MxSocket : public std::enable_shared_from_this<MxSocket> {
  public:

    enum class Direction {
      Input,
      Output,
    };

    enum class DataType {
      Bool,
      Int,
      Float,
      Double,
      Vector2,
      Vector3,
      Vector4,

      Count
    };

    using SharedPtr = std::shared_ptr<MxSocket>;
    using SharedConstPtr = std::shared_ptr<const MxSocket>;

    const std::string& name() const { return mName; }
    std::shared_ptr<MxNode> node() const { return mpNode; }

    ~MxSocket();

  private:
    MxSocket(std::shared_ptr<MxNode> pNode, const std::string& name, DataType dataType, Direction direction);

    static MxSocket::SharedPtr create(std::shared_ptr<MxNode> pNode, const std::string& name, DataType dataType, Direction direction);

    std::shared_ptr<MxNode> mpNode = nullptr;
    std::string mName;
    std::string mDataType;
    Direction   mDirection;

    friend class MxNode;
};

inline std::string to_string(MxSocket::Direction dir) {
  if (dir == MxSocket::Direction::Input) return "input";
  return "output";
}


}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_MATERIALX_MXSOCKET_H_