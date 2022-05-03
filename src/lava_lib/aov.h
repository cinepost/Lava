#ifndef SRC_LAVA_LIB_AOV_H_
#define SRC_LAVA_LIB_AOV_H_

#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include "types.h"
#include "boost/variant.hpp"

#include "Falcor/RenderGraph/RenderPass.h"
#include "RenderPasses/AccumulatePass/AccumulatePass.h"

namespace lava {

class Renderer;

enum class AOVBuiltinName {
  MAIN,
  DEPTH,
  NORMAL,
  OBJECT_ID,
  MATERIAL_ID,
  INSTANCE_ID
};

inline std::string to_string(AOVBuiltinName name) {
#define type_2_string(a) case AOVBuiltinName::a: return #a;
  switch (name) {
    type_2_string(MAIN);
    type_2_string(DEPTH);
    type_2_string(NORMAL);
    type_2_string(OBJECT_ID);
    type_2_string(MATERIAL_ID);
    type_2_string(INSTANCE_ID);
  default:
    should_not_get_here();
    return "unknown";
  }
#undef type_2_string
}

class aov_name_visitor : public boost::static_visitor<std::string> {
  public:
    std::string operator()(AOVBuiltinName name) const;
    const std::string& operator()(const std::string& str) const;
};

struct AOVName : boost::variant< AOVBuiltinName, std::string > {

  AOVName(): boost::variant< AOVBuiltinName, std::string >("") {};
  AOVName(const char* name):  boost::variant< AOVBuiltinName, std::string >(std::string(name)) {};
  AOVName(const std::string& name):  boost::variant< AOVBuiltinName, std::string >(name) {};
  AOVName(const AOVBuiltinName& name):  boost::variant< AOVBuiltinName, std::string >(name) {};

  operator std::string() const { return boost::apply_visitor(aov_name_visitor(), *this); }
  
  bool operator==(const std::string& str) const { return (boost::apply_visitor(aov_name_visitor(), *this) == str); }
  bool operator==(const AOVBuiltinName& name) const { return (boost::apply_visitor(aov_name_visitor(), *this) == to_string(name)); }
  
  AOVName operator=(const std::string& str) { return AOVName(str); }
  AOVName operator=(const AOVBuiltinName& name) { return AOVName(name); }
  
  std::string operator+(const char* str) const { return std::string(str) + boost::apply_visitor(aov_name_visitor(), *this); }
};

struct AOVPlaneInfo {
  enum class AccumulationMode {
    MEDIAN, // Average value calculation. Good for color/color+alpha image data
    MIN,    // Keep minum value
    MAX,    // Keep maximum value
    CLOSEST,// Closest surface value. Good for Depth/Normal type aovs 
    NONE    // Last sample value written
  };

  enum class Precision {
    SINGLE,
    DOUBLE,
    COMPENSATED,
    AUTO
  };

  Falcor::ResourceFormat  format;
  AOVName                 name;                                         // AOVBuiltinName::MAIN is a reserved name for beauty pass
  std::string             variableName;
  AccumulationMode        accumulationMode = AccumulationMode::MEDIAN;  // Default.
  Precision               precision = Precision::AUTO;                  // Keep it on AUTO
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
    const AOVName&          name() const { return mInfo.name; }
    Falcor::ResourceFormat  format() const { return mInfo.format; }
    AOVPlaneInfo            info() const { return mInfo; }

    bool getImageData(uint8_t* pData) const;
    bool getAOVPlaneGeometry(AOVPlaneGeometry& aov_plane_geometry) const;

    void setFormat(Falcor::ResourceFormat format);

    bool isBound() const;

  private:
    AOVPlane(const AOVPlaneInfo& info);
    static SharedPtr create(const AOVPlaneInfo& info);

    bool bindToTexture(Falcor::Texture::SharedPtr pTexture);

    AccumulatePass::SharedPtr         createAccumulationPass( Falcor::RenderContext* pContext, Falcor::RenderGraph::SharedPtr pGraph);
    AccumulatePass::SharedPtr         accumulationPass() { return mpAccumulatePass; }
    AccumulatePass::SharedConstPtr    accumulationPass() const { return mpAccumulatePass; }

    const std::string&                accumulationPassInputName() const;
    const std::string&                accumulationPassOutputName() const;

  private:
    AOVPlaneInfo                        mInfo;
    Falcor::Texture::SharedPtr          mpTexture = nullptr;

    Falcor::RenderGraph::SharedPtr      mpRenderGraph = nullptr;
    AccumulatePass::SharedPtr           mpAccumulatePass = nullptr;

    std::string                         mAccumulatePassName;
    std::string                         mAccumulatePassInputName;
    std::string                         mAccumulatePassOutputName;

    Falcor::Resource::Type              mType = Falcor::Resource::Type::Undefined;

  friend class Renderer;
};

inline std::string to_string(AOVName name) {
  return boost::apply_visitor(aov_name_visitor(), name);
}

inline std::ostream& operator<<(std::ostream& os, const AOVBuiltinName& name) {
  os << to_string(name);
  return os;
};

inline std::string operator+(const char* str, const AOVName& name) { 
  return str + boost::apply_visitor(aov_name_visitor(), name); 
}

//enum_class_operators(AOVBuiltinName);

}  // namespace lava


#endif  // SRC_LAVA_LIB_AOV_H_
