#ifndef SRC_LAVA_LIB_AOV_H_
#define SRC_LAVA_LIB_AOV_H_

#include "lava_dll.h"

#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include "types.h"
#include "boost/variant.hpp"

#include "Falcor/RenderGraph/RenderGraph.h"
#include "Falcor/RenderGraph/RenderPass.h"

#include "display.h"

#include "RenderPasses/AccumulatePass/AccumulatePass.h"
#include "RenderPasses/ToneMapperPass/ToneMapperPass.h"
#include "RenderPasses/OpenDenoisePass/OpenDenoisePass.h"
#include "RenderPasses/ImageLoaderPass/ImageLoaderPass.h"

namespace lava {

class Renderer;

enum class AOVBuiltinName: uint8_t {
  MAIN        = 0,
  POSITION,
  DEPTH,
  NORMAL,
  TANGENT_NORMAL,
  ALBEDO,
  SHADOW,
  FRESNEL,
  EMISSION,
  ROUGHNESS,
  OBJECT_ID,
  MATERIAL_ID,
  INSTANCE_ID,
  
  Prim_Id,
  Op_Id,
  VARIANCE,
  MESHLET_ID,
  MICROPOLY_ID,
  MESHLET_COLOR,
  MICROPOLY_COLOR,
  UV,

  EdgeDetectPass,
  AmbientOcclusionPass,
  CryptomattePass,

  UNKNOWN ,
};

inline std::string to_string(AOVBuiltinName name) {
#define type_2_string(a) case AOVBuiltinName::a: return #a;
  switch (name) {
    type_2_string(MAIN);
    type_2_string(POSITION);
    type_2_string(DEPTH);
    type_2_string(NORMAL);
    type_2_string(TANGENT_NORMAL);
    type_2_string(ALBEDO);
    type_2_string(SHADOW);
    type_2_string(FRESNEL);
    type_2_string(EMISSION);
    type_2_string(ROUGHNESS);
    type_2_string(OBJECT_ID);
    type_2_string(MATERIAL_ID);
    type_2_string(INSTANCE_ID);
    type_2_string(Prim_Id);
    type_2_string(Op_Id);
    type_2_string(VARIANCE);
    type_2_string(MESHLET_ID);
    type_2_string(MICROPOLY_ID);
    type_2_string(MESHLET_COLOR);
    type_2_string(MICROPOLY_COLOR);
    type_2_string(UV);

    type_2_string(EdgeDetectPass);
    type_2_string(AmbientOcclusionPass);
    type_2_string(CryptomattePass);
  default:
    should_not_get_here();
    return "unknown";
  }
#undef type_2_string
}

class LAVA_API aov_name_visitor : public boost::static_visitor<std::string> {
  public:
    std::string operator()(AOVBuiltinName name) const;
    const std::string& operator()(const std::string& str) const;
};

class aov_builtin_name_visitor : public boost::static_visitor<AOVBuiltinName> {
  public:
    AOVBuiltinName operator()(AOVBuiltinName name) const;
    AOVBuiltinName operator()(const std::string& str) const;
};


struct AOVName : boost::variant< AOVBuiltinName, std::string > {

  AOVName(): boost::variant< AOVBuiltinName, std::string >("") {};
  AOVName(const char* name):  boost::variant< AOVBuiltinName, std::string >(std::string(name)) {};
  AOVName(const std::string& name):  boost::variant< AOVBuiltinName, std::string >(name) {};
  AOVName(const AOVBuiltinName& name):  boost::variant< AOVBuiltinName, std::string >(name) {};

  operator std::string() const { return boost::apply_visitor(aov_name_visitor(), *this); }
  operator AOVBuiltinName() const { return boost::apply_visitor(aov_builtin_name_visitor(), *this); }
  
  bool operator==(const std::string& str) const { return (boost::apply_visitor(aov_name_visitor(), *this) == str); }
  bool operator==(const AOVBuiltinName& name) const { return (boost::apply_visitor(aov_name_visitor(), *this) == to_string(name)); }
  
  bool operator!=(const std::string& str) const { return (boost::apply_visitor(aov_name_visitor(), *this) != str); }
  bool operator!=(const AOVBuiltinName& name) const { return (boost::apply_visitor(aov_name_visitor(), *this) != to_string(name)); }

  AOVName operator=(const std::string& str) { return AOVName(str); }
  AOVName operator=(const AOVBuiltinName& name) { return AOVName(name); }
  
  std::string operator+(const char* str) const { return std::string(str) + boost::apply_visitor(aov_name_visitor(), *this); }
};

struct AOVPlaneInfo {
  enum class Precision {
    SINGLE,
    DOUBLE,
    COMPENSATED,
    AUTO
  };

  Falcor::ResourceFormat  format;
  AOVName                 name;                                         // AOVBuiltinName::MAIN is a reserved name for beauty pass
  std::string             outputOverrideName;                           // Plane output override name
  std::string             variableName;
  std::string             pfilterTypeName;                              // Pixel filter type name e.g "Box" or "box"
  Falcor::uint2           pfilterSize;                                  // Pixel filter kernel size (in pixels)
  Precision               precision = Precision::AUTO;                  // Keep it on AUTO
  std::string             sourcePassName;                               // Render pass name to bind. If empty main output pass used.
  bool                    enableAccumulation = true;                    // Enable sample accumulation.
  std::string             filenameOverride;                             // When set sepate image output display is created.
};

struct AOVPlaneGeometry {
  Falcor::ResourceFormat resourceFormat; // You can use bytesPerPixel, bitsPerComponent, channelsCount as well. They are the same.
  uint32_t width;
  uint32_t height;
  uint32_t bytesPerPixel;         // Calculated from resourceFormat.
  uint32_t bitsPerComponent[4];   // Calculated from resourceFormat.
  uint32_t channelsCount;         // Calculated from resourceFormat.
};

class LAVA_API AOVPlane: public std::enable_shared_from_this<AOVPlane> {
  public:
    using SharedPtr = std::shared_ptr<AOVPlane>;
    using SharedConstPtr = std::shared_ptr<const AOVPlane>;

    enum class State: uint8_t {
      Enabled,
      Disabled,
    };

    std::string             outputVariableName() const { return (mInfo.variableName != "") ? mInfo.variableName : "output"; }
    const AOVName&          name() const { return mInfo.name; }
    std::string             outputName() const { return (mInfo.outputOverrideName) != "" ? mInfo.outputOverrideName : std::string(mInfo.name); }
    Falcor::ResourceFormat  format() const { return mInfo.format; }
    const AOVPlaneInfo&     info() const { return mInfo; }
    const std::string&      filename() const { return mInfo.filenameOverride; }

    void update(const AOVPlaneInfo& info);

    bool isMain() const { return name() == AOVBuiltinName::MAIN; }

    void setOutputFormat(Falcor::ResourceFormat format);

    const std::string&      sourcePassName() const { return mInfo.sourcePassName; }

    void addMetaDataCallback(std::function<Falcor::Dictionary()> func) { mMetaDataCallbacks.push_back(func); }
    void addMetaDataProvider(Falcor::RenderPass::SharedPtr pPass) { if(pPass) mMetaDataRenderPasses.push_back(pPass); }
    void addMetaData(const Falcor::Dictionary& meta_data);
    void addMetaData(const std::string& key, const std::string& value) { mMetaData[key] = value; }
    void addMetaData(const std::string& key, int value) { mMetaData[key] = value; }
    void addMetaData(const std::string& key, float value) { mMetaData[key] = value; }

    const uint8_t* getImageData();
    Falcor::Dictionary getMetaData() const;
    bool hasMetaData() const { return !mMetaData.isEmpty() || (mMetaDataCallbacks.size() > 0) || (mMetaDataRenderPasses.size() > 0); }

    void setDisplay(Display::SharedPtr pDisplay) { mpDisplay = pDisplay; }
    Display::SharedPtr getDisplay() const { return mpDisplay; }
    bool hasDisplay() const { return mpDisplay != nullptr; }

    const uint8_t* getProcessedImageData();
    bool getAOVPlaneGeometry(AOVPlaneGeometry& aov_plane_geometry) const;

    void setFormat(Falcor::ResourceFormat format);
    void reset() { if (mpAccumulatePass) mpAccumulatePass->reset(); }; // reset associated accumulator

    bool isBound() const;

    Falcor::Dictionary& getRenderPassesDict() { return mRenderPassesDictionary; };
    const Falcor::Dictionary& getRenderPassesDict() const { return mRenderPassesDictionary; };

    bool isEnabled() const { return (mState == State::Enabled); }
    State getState() const { return mState; }

  private:
    AOVPlane(const AOVPlaneInfo& info);
    static SharedPtr create(const AOVPlaneInfo& info);

    bool bindToTexture(Falcor::Texture::SharedPtr pTexture);

    AccumulatePass::SharedPtr               createAccumulationPass( Falcor::RenderContext* pContext, Falcor::RenderGraph::SharedPtr pGraph, const Falcor::Dictionary& dict = {});
    ToneMapperPass::SharedPtr               createTonemappingPass( Falcor::RenderContext* pContext, const Falcor::Dictionary& dict = {});
    OpenDenoisePass::SharedPtr              createOpenDenoisePass( Falcor::RenderContext* pContext, const Falcor::Dictionary& dict = {});

    AccumulatePass::SharedPtr        accumulationPass() { return mpAccumulatePass; }
    AccumulatePass::SharedConstPtr   accumulationPass() const { return mpAccumulatePass; }

    ToneMapperPass::SharedPtr        tonemappingPass() { return mpToneMapperPass; }
    ToneMapperPass::SharedConstPtr   tonemappingPass() const { return mpToneMapperPass; }

    OpenDenoisePass::SharedPtr       denoisingPass() { return mpDenoiserPass; }
    OpenDenoisePass::SharedConstPtr  denoisingPass() const { return mpDenoiserPass; }

    const std::string&               accumulationPassColorInputName() const { return mAccumulatePassColorInputName; }
    const std::string&               accumulationPassDepthInputName() const { return mAccumulatePassDepthInputName; }

    const std::string&               accumulationPassColorOutputName() const { return mAccumulatePassColorOutputName; }

    void createInternalRenderGraph(Falcor::RenderContext* pContext, bool force = false);
    bool compileInternalRenderGraph(Falcor::RenderContext* pContext);

    const uint8_t* getTextureData(Texture* pTexture);

  private:
    void setState(State state) { mState = state; }         

  private:
    AOVPlaneInfo                        mInfo;
    Falcor::ResourceFormat              mFormat = Falcor::ResourceFormat::Unknown;  ///< Internal 'real' format that might be different from requested
    Falcor::Texture::SharedPtr          mpTexture = nullptr;

    Falcor::RenderGraph::SharedPtr      mpRenderGraph = nullptr;                    ///< Shared pointer to renderer graph
    Falcor::RenderGraph::SharedPtr      mpInternalRenderGraph = nullptr;            ///< AOV internal render graph for chained effects

    AccumulatePass::SharedPtr           mpAccumulatePass = nullptr;

    // Internal render grpah passes
    ImageLoaderPass::SharedPtr          mpImageLoaderPass = nullptr;
    ToneMapperPass::SharedPtr           mpToneMapperPass = nullptr;
    OpenDenoisePass::SharedPtr          mpDenoiserPass = nullptr;

    std::string                         mAccumulatePassName;
    std::string                         mAccumulatePassColorInputName;
    std::string                         mAccumulatePassDepthInputName;
    std::string                         mAccumulatePassColorOutputName;

    std::string                         mProcessedPassOutputName;

    Falcor::Dictionary                  mRenderPassesDictionary;

    std::vector<uint8_t>                mOutputData;
    Falcor::Dictionary                  mMetaData;
    std::vector<std::function<Falcor::Dictionary()>>  mMetaDataCallbacks;
    std::vector<Falcor::RenderPass::SharedPtr>        mMetaDataRenderPasses;

    Falcor::Resource::Type              mType;

    Display::SharedPtr                  mpDisplay;

    State                               mState = State::Enabled;

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

enum_class_operators(AOVBuiltinName);

}  // namespace lava


#endif  // SRC_LAVA_LIB_AOV_H_
