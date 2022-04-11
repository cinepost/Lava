#ifndef SRC_FALCOR_SCENE_MATERIALX_MATERIALX_H_
#define SRC_FALCOR_SCENE_MATERIALX_MATERIALX_H_

#include <memory>

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/API/Texture.h"
#include "Falcor/Core/API/Sampler.h"
#include "Falcor/Utils/Image/TextureAnalyzer.h"

#include "MxNode.h"

namespace Falcor {

class Device;

class dlldecl MaterialX : public std::enable_shared_from_this<MaterialX> {
  public:
    using UniquePtr = std::unique_ptr<MaterialX>;
    using SharedPtr = std::shared_ptr<MaterialX>;
    using SharedConstPtr = std::shared_ptr<const MaterialX>;

    /** Flags indicating if and what was updated in the material
    */
    enum class UpdateFlags {
        None                = 0x0,  ///< Nothing updated
        DataChanged         = 0x1,  ///< Material data (properties) changed
        ResourcesChanged    = 0x2,  ///< Material resources (textures, sampler) changed
        DisplacementChanged = 0x4,  ///< Displacement changed
    };

    /** Create a new material.
        \param[in] name The material name
    */
    static SharedPtr createShared(std::shared_ptr<Device> pDevice, const std::string& name);

    static UniquePtr createUnique(std::shared_ptr<Device> pDevice, const std::string& name);

    ~MaterialX();

    /** Set the material name.
    */
    void setName(const std::string& name) { mName = name; }

    /** Get the material name.
    */
    const std::string& name() const { return mName; }

    /** Comparison operator
    */
    bool operator==(const MaterialX& other) const;

    std::shared_ptr<Device> device() const { return mpDevice; }

    MxNode::SharedPtr createNode(const MxNode::TypeCreateInfo& info, const std::string& name);

    MxNode::SharedPtr rootNode() const { return mpMxRoot; };

    /**
     * We allow MaterialX to be created without device, so we have to set it anytime later in the process
     * before trying to actually compile material program
     */
    void setDevice(std::shared_ptr<Device> pDevice);

  private:
    MaterialX(std::shared_ptr<Device> pDevice, const std::string& name);

    std::string mName;
    std::shared_ptr<Device> mpDevice = nullptr;

    MxNode::SharedPtr mpMxRoot = nullptr;

    friend class SceneCache;
};

}  // namespace Falcor

#endif  // SRC_FALCOR_SCENE_MATERIALX_MATERIALX_H_