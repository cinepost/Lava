#include "USD/hgiVk/shaderProgram.h"
#include "USD/hgiVk/shaderFunction.h"

PXR_NAMESPACE_OPEN_SCOPE


HgiVkShaderProgram::HgiVkShaderProgram(HgiShaderProgramDesc const& desc)
    : HgiShaderProgram(desc)
    , _descriptor(desc)
{
}

HgiVkShaderProgram::~HgiVkShaderProgram()
{
}

HgiShaderFunctionHandleVector const&
HgiVkShaderProgram::GetShaderFunctions() const
{
    return _descriptor.shaderFunctions;
}


PXR_NAMESPACE_CLOSE_SCOPE
