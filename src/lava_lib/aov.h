#ifndef SRC_LAVA_LIB_AOV_H_
#define SRC_LAVA_LIB_AOV_H_

#include <cstddef>
#include <string>
#include <vector>
#include <memory>

#include "types.h"

#include "Falcor/RenderGraph/RenderPass.h"
#include "RenderPasses/AccumulatePass/AccumulatePass.h"

namespace lava {

class Renderer;

struct AOVPlaneInfo {
  enum class AccumulationMode {
    MEDIAN,
    MIN,
    MAX,
    NONE // Last sample written
  };

  enum class Precision {
    SINGLE,
    DOUBLE,
    COMPENSATED,
    AUTO
  };

  Falcor::ResourceFormat  format;
  std::string             name;         // "main" is a reserved name for beauty pass
  std::string             variableName;
  AccumulationMode        accumulationMode = AccumulationMode::MEDIAN; 
  Precision               precision = Precision::AUTO;
};

struct AOVPlaneGeometry {
  Falcor::ResourceFormat resourceFormat; // You can use bytesPerPixel, bitsPerComponent, channelsCount as well. They are the same.
  uint32_t width;
  uint32_t height;
  uint32_t bytesPerPixel;         // Calculated from resourceFormat.
  uint32_t bitsPerComponent[4];   // Calculated from resourceFormat.
  uint32_t channelsCount;         // Calculated from resourceFormat.
};

class AOVPlane: public std::enable_shared_from_this<AOVPlane> {
  public:
    using SharedPtr = std::shared_ptr<AOVPlane>;
    using SharedConstPtr = std::shared_ptr<const AOVPlane>;

    const std::string&      outputVariableName() const { return mInfo.variableName; }
    const std::string&      name() const { return mInfo.name; }
    Falcor::ResourceFormat  format() const { return mInfo.format; }
    AOVPlaneInfo            info() const { return mInfo; }

    bool getImageData(uint8_t* pData) const;
    bool getAOVPlaneGeometry(AOVPlaneGeometry& aov_plane_geometry) const;

    bool isBound() const { return mpResource ? true : false; }
    AccumulatePass::SharedConstPtr   accumulationPass() const { return mpAccumulatePass; }

  private:
    AOVPlane(const AOVPlaneInfo& info);
    static SharedPtr create(const AOVPlaneInfo& info);

    Falcor::Resource::SharedConstPtr resource() const { return mpResource; };
    bool bindToResource(Falcor::Resource::SharedPtr pResource);

    AccumulatePass::SharedPtr   accumulationPass() { return mpAccumulatePass; }

  private:
    AOVPlaneInfo                        mInfo;
    Falcor::Resource::SharedPtr         mpResource = nullptr;

    AccumulatePass::SharedPtr           mpAccumulatePass = nullptr;

    Falcor::Resource::Type              mType = Falcor::Resource::Type::Undefined;

  friend class Renderer;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_AOV_H_
