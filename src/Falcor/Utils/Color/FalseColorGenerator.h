#ifndef SRC_FALCOR_UTILS_COLOR_FALSECOLORGENERATOR_H_
#define SRC_FALCOR_UTILS_COLOR_FALSECOLORGENERATOR_H_

#include "Falcor/Core/Program/Program.h"
#include "Falcor/Core/Program/ShaderVar.h"


namespace Falcor {


class dlldecl FalseColorGenerator : public std::enable_shared_from_this<FalseColorGenerator> {
public:
    using SharedPtr = std::shared_ptr<FalseColorGenerator>;
    using SharedConstPtr = std::shared_ptr<const FalseColorGenerator>;

    static SharedPtr create(Device::SharedPtr pDevice, uint32_t numColors, const uint32_t* pSeed = nullptr);

    void setShaderData(ShaderVar const& var) const;

protected:
    FalseColorGenerator(Device::SharedPtr pDevice, uint32_t numColors, const uint32_t* pSeed = nullptr);

private:
    uint32_t mNumColors = 0;

    Device::SharedPtr mpDevice;
    Buffer::SharedPtr mpFalseColorsBuffer;
    
};

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_COLOR_FALSECOLORGENERATOR_H_
