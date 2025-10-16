/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#ifndef SRC_FALCOR_CORE_API_PARAMETERBLOCK_H_
#define SRC_FALCOR_CORE_API_PARAMETERBLOCK_H_

#include "Falcor/Core/Framework.h"
#include "Falcor/Core/Object.h"

#include "Falcor/Core/Program/ShaderVar.h"
#include "Falcor/Core/Program/ProgramReflection.h"

#include <slang/slang.h>


namespace Falcor {

class Device;
class ProgramVersion;
class CopyContext;
class ShaderResourceView;
class UnorderedAccessView;
class Resource;
class Sampler;
class RtAccelerationStructure;


/** A parameter block. This block stores all the parameter data associated with a specific type in shader code
*/
class FALCOR_API ParameterBlock : public Object {
    FALCOR_OBJECT(ParameterBlock)
public:
    ~ParameterBlock();

    using BindLocation = ParameterBlockReflection::BindLocation;

    /** Create a new object that holds a value of the given type.
    */
    static SharedPtr create(Falcor::SharedPtr<Device> pDevice, const Falcor::SharedPtr<const ProgramVersion>& pProgramVersion, const ReflectionType::SharedConstPtr& pType);
    static SharedPtr create(Device* pDevice, const Falcor::SharedPtr<const ProgramVersion>& pProgramVersion, const ReflectionType::SharedConstPtr& pType);

    /** Create a new object that holds a value described by the given reflector.
    */
    static SharedPtr create(Falcor::SharedPtr<Device> pDevice, const ParameterBlockReflection::SharedConstPtr& pReflection);
    static SharedPtr create(Device* pDevice, const ParameterBlockReflection::SharedConstPtr& pReflection);

    /** Create a new object that holds a value of the type with the given name in the given program.
        \param[in] pProgramVersion Program version object.
        \param[in] typeName Name of the type. If the type does not exist an exception is thrown.
    */
    static SharedPtr create(Falcor::SharedPtr<Device> pDevice, const Falcor::SharedPtr<const ProgramVersion>& pProgramVersion, const std::string& typeName);
    static SharedPtr create(Device* pDevice, const Falcor::SharedPtr<const ProgramVersion>& pProgramVersion, const std::string& typeName);

    gfx::IShaderObject* getShaderObject() const { return mpShaderObject.get(); }

    /** Set a variable into the block.
        The function will validate that the value Type matches the declaration in the shader. If there's a mismatch, an error will be logged and the call will be ignored.
        \param[in] name The variable name. See notes about naming in the ConstantBuffer class description.
        \param[in] value Value to set
    */
    template<typename T>
    void setVariable(const std::string& name, const T& value) {
        return getRootVar()[name].set(value);
    }

    /** Set a variable into the block.
        The function will validate that the value Type matches the declaration in the shader. If there's a mismatch, an error will be logged and the call will be ignored.
        \param[in] offset The variable byte offset inside the buffer
        \param[in] value Value to set
    */
    template<typename T>
    void setVariable(const BindLocation& bindLocation, const T& value);

    template<typename T>
    void setBlob(const BindLocation& bindLocation, const T& blob) const {
        setBlob(bindLocation, &blob, sizeof(blob));
    }

    void setBlob(const BindLocation& bindLocation, const void* pSrc, size_t size) { return setBlob(pSrc, bindLocation, size); }

    void setBlob(const void* pSrc, const BindLocation& bindLocation, size_t size);
    void setBlob(const void* pSrc, size_t offset, size_t size);

    /** Bind a buffer by name.
        If the name doesn't exists, the bind flags don't match the shader requirements or the size doesn't match the required size, the call will fail.
        \param[in] name The name of the buffer in the program
        \param[in] pBuffer The buffer object
        \return false is the call failed, otherwise true
    */
    void setBuffer(const std::string& name, const Falcor::SharedPtr<Buffer>& pBuffer);

    /** Bind a buffer object by index
        If the no buffer exists in the specified index or the bind flags don't match the shader requirements or the size doesn't match the required size, the call will fail.
        \param[in] bindLocation The bind-location in the block
        \param[in] pBuffer The buffer object
        \return false is the call failed, otherwise true
    */
    void setBuffer(const BindLocation& bindLocation, const Falcor::SharedPtr<Buffer>& pBuffer);

    /** Get a buffer
        \param[in] name The name of the buffer
        \return If the name is valid, a shared pointer to the buffer. Otherwise returns nullptr
    */
    Falcor::SharedPtr<Buffer> getBuffer(const std::string& name) const;

    /** Get a buffer
        \param[in] bindLocation The bind location of the buffer
        \return If the name is valid, a shared pointer to the buffer. Otherwise returns nullptr
    */
    Falcor::SharedPtr<Buffer> getBuffer(const BindLocation& bindLocation) const;

    /** Bind a parameter block by name.
        If the name doesn't exists or the size doesn't match the required size, the call will fail.
        \param[in] name The name of the parameter block in the program
        \param[in] pBlock The parameter block
        \return false is the call failed, otherwise true
    */
    void setParameterBlock(const std::string& name, const Falcor::SharedPtr<ParameterBlock>& pBlock);

    /** Bind a parameter block by index.
        If the no parameter block exists in the specified index or the parameter block size doesn't match the required size, the call will fail.
        \param[in] bindLocation The location of the object
        \param[in] pBlock The parameter block
        \return false is the call failed, otherwise true
    */
    void setParameterBlock(const BindLocation& bindLocation, const Falcor::SharedPtr<ParameterBlock>& pBlock);

    /** Get a parameter block.
        \param[in] name The name of the parameter block
        \return If the name is valid, a shared pointer to the parameter block. Otherwise returns nullptr
    */
    Falcor::SharedPtr<ParameterBlock> getParameterBlock(const std::string& name) const;

    /** Get a parameter block.
        \param[in] bindLocation The location of the block
        \return If the indices is valid, a shared pointer to the parameter block. Otherwise returns nullptr
    */
    Falcor::SharedPtr<ParameterBlock> getParameterBlock(const BindLocation& bindLocation) const;

    /** Bind a texture. Based on the shader reflection, it will be bound as either an SRV or a UAV
        \param[in] name The name of the texture object in the shader
        \param[in] pTexture The texture object to bind
    */
    void setTexture(const std::string& name, const Falcor::SharedPtr<Texture>& pTexture);
    void setTexture(const BindLocation& bindLocation, const Falcor::SharedPtr<Texture>& pTexture);

    /** Get a texture object.
        \param[in] name The name of the texture
        \return If the name is valid, a shared pointer to the texture object. Otherwise returns nullptr
    */
    Falcor::SharedPtr<Texture> getTexture(const std::string& name) const;
    Falcor::SharedPtr<Texture> getTexture(const BindLocation& bindLocation) const;

    /** Bind an SRV.
        \param[in] bindLocation The bind-location in the block
        \param[in] pSrv The shader-resource-view object to bind
    */
    void setSrv(const BindLocation& bindLocation, const Falcor::SharedPtr<ShaderResourceView>& pSrv);

    /** Bind a UAV.
        \param[in] bindLocation The bind-location in the block
        \param[in] pSrv The unordered-access-view object to bind
    */
    void setUav(const BindLocation& bindLocation, const Falcor::SharedPtr<ShaderResourceView>& pUav);

    /** Bind an acceleration structure.
        \param[in] bindLocation The bind-location in the block
        \param[in] pAccl The acceleration structure object to bind
        \return false if the binding location does not accept an acceleration structure, true otherwise.
    */
    void setAccelerationStructure(const BindLocation& bindLocation, const Falcor::SharedPtr<RtAccelerationStructure>& pAccl);

    /** Get an SRV object.
        \param[in] bindLocation The bind-location in the block
        \return If the bind-location is valid, a shared pointer to the SRV. Otherwise returns nullptr
    */
    Falcor::SharedPtr<ShaderResourceView> getSrv(const BindLocation& bindLocation) const;

    /** Get a UAV object
        \param[in] bindLocation The bind-location in the block
        \return If the bind-location is valid, a shared pointer to the UAV. Otherwise returns nullptr
    */
    Falcor::SharedPtr<UnorderedAccessView> getUav(const BindLocation& bindLocation) const;

    /** Get an acceleration structure object.
        \param[in] bindLocation The bind-location in the block
        \return If the bind-location is valid, a shared pointer to the acceleration structure. Otherwise returns nullptr
    */
    Falcor::SharedPtr<RtAccelerationStructure> getAccelerationStructure(const BindLocation& bindLocation) const;

    /** Bind a sampler to the program in the global namespace.
        \param[in] name The name of the sampler object in the shader
        \param[in] pSampler The sampler object to bind
        \return false if the sampler was not found in the program, otherwise true
    */
    void setSampler(const std::string& name, const Falcor::SharedPtr<Sampler>& pSampler);

    /** Bind a sampler to the program in the global namespace.
        \param[in] bindLocation The bind-location in the block
        \param[in] pSampler The sampler object to bind
        \return false if the sampler was not found in the program, otherwise true
    */
    void setSampler(const BindLocation& bindLocation, const Falcor::SharedPtr<Sampler>& pSampler);

    /** Gets a sampler object.
        \param[in] bindLocation The bind-location in the block
        \return If the bind-location is valid, a shared pointer to the sampler. Otherwise returns nullptr
    */
    const Falcor::SharedPtr<Sampler>& getSampler(const BindLocation& bindLocation) const;

    /** Gets a sampler object.
        \return If the name is valid, a shared pointer to the sampler. Otherwise returns nullptr
    */
    Falcor::SharedPtr<Sampler> getSampler(const std::string& name) const;

    /** Get the parameter block's reflection interface
    */
    Falcor::SharedPtr<const ParameterBlockReflection> getReflection() const { return mpReflector; }

    /** Get the block reflection type
    */
    Falcor::SharedPtr<const ReflectionType> getElementType() const { return mpReflector->getElementType(); }

    /** Get the size of the reflection type
    */
    size_t getElementSize() const;

    /** Get offset of a uniform variable inside the block, given its name.
    */
    TypedShaderVarOffset getVariableOffset(const std::string& varName) const;

    /** Get an initial var to the contents of this block.
    */
    ShaderVar getRootVar() const;

    /** Try to find a shader var for a member of the block.

        Returns an invalid shader var if no such member is found.
    */
    ShaderVar findMember(const std::string& varName) const;

    /** Try to find a shader var for a member of the block by index.

        Returns an invalid shader var if no such member is found.
    */
    ShaderVar findMember(uint32_t index) const;

    /** Get the size of the parameter-block's buffer
    */
    size_t getSize() const;

    bool updateSpecialization() const;
    Falcor::SharedPtr<const ParameterBlockReflection> getSpecializedReflector() const { return mpSpecializedReflector; }

    bool prepareDescriptorSets(CopyContext* pCopyContext);

    using SpecializationArgs = std::vector<slang::SpecializationArg>;
    void collectSpecializationArgs(SpecializationArgs& ioArgs) const;

    void const* getRawData() const;

public:
    ParameterBlock(Falcor::SharedPtr<Device> pDevice, const std::shared_ptr<const ProgramVersion>& pProgramVersion, const ParameterBlockReflection::SharedConstPtr& pReflection);
    ParameterBlock(Falcor::SharedPtr<Device> pDevice, const ProgramReflection::SharedConstPtr& pReflector);

    //ParameterBlock(Device::SharedPtr pDevice, const std::shared_ptr<const ProgramVersion>& pProgramVersion, const ParameterBlockReflection::SharedConstPtr& pReflection) {
    //    ParameterBlock(pDevice.get(), pProgramVersion, pReflection);
    //}
    //ParameterBlock(Device::SharedPtr pDevice, const ProgramReflection::SharedConstPtr& pReflector) {
    //    ParameterBlock(pDevice.get(), pReflector);
    //}

protected:
    void initializeResourceBindings();
    void createConstantBuffers(const ShaderVar& var);
    void checkForNestedTextureArrayResources();

    static void prepareResource(CopyContext* pContext, Resource* pResource, bool isUav);

    uint mID;

    Device* mpDevice;
    Falcor::SharedPtr<const ProgramVersion> mpProgramVersion;
    Falcor::SharedPtr<const ParameterBlockReflection> mpReflector;
    mutable Falcor::SharedPtr<const ParameterBlockReflection> mpSpecializedReflector;

    Slang::ComPtr<gfx::IShaderObject> mpShaderObject;
    std::map<gfx::ShaderOffset, Falcor::SharedPtr<ParameterBlock>> mParameterBlocks;
    std::map<gfx::ShaderOffset, Falcor::SharedPtr<ShaderResourceView>> mSRVs;
    std::map<gfx::ShaderOffset, Falcor::SharedPtr<UnorderedAccessView>> mUAVs;
    std::map<gfx::ShaderOffset, Falcor::SharedPtr<Resource>> mResources;
    std::map<gfx::ShaderOffset, Falcor::SharedPtr<Sampler>> mSamplers;
    std::map<gfx::ShaderOffset,Falcor::SharedPtr< RtAccelerationStructure>> mAccelerationStructures;
};

template<typename T> void ShaderVar::setImpl(const T& val) const {
    mpBlock->setVariable(mOffset, val);
}

}  // namespace Falcor

#endif  // SRC_FALCOR_CORE_API_PARAMETERBLOCK_H_