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
#ifndef SRC_FALCOR_SCENE_SDFS_SPARSEVOXELSET_SDFSVS_H_
#define SRC_FALCOR_SCENE_SDFS_SPARSEVOXELSET_SDFSVS_H_

#include "Falcor/Scene/SDFs/SDFGrid.h"


namespace Falcor {

class Device;
class Buffer;
class Texture;

/** A single SDF Sparse Voxel Set. Can only be utilized on the GPU.
*/
class FALCOR_API SDFSVS : public SDFGrid {
    FALCOR_OBJECT(SDFSVS)
public:
    /** Create a new, empty SDF sparse voxel set.
        \return SDFSVS object, or nullptr if errors occurred.
    */
    static SharedPtr create(Falcor::SharedPtr<Device> pDevice);

    virtual size_t getSize() const override;
    virtual uint32_t getMaxPrimitiveIDBits() const override;
    virtual Type getType() const { return Type::SparseVoxelSet; }

    virtual void createResources(RenderContext* pRenderContext, bool deleteScratchData = true) override;

    virtual const Falcor::SharedPtr<Buffer>& getAABBBuffer() const override { return mpVoxelAABBBuffer; }
    virtual uint32_t getAABBCount() const override { return mVoxelCount; }

    virtual void bindShaderData(const ShaderVar& var) const override;

protected:
    virtual void setValuesInternal(const std::vector<float>& cornerValues) override;

private:
    SDFSVS(Falcor::SharedPtr<Device> pDevice):SDFGrid(pDevice) {};

    // CPU data.
    std::vector<int8_t> mValues;

    // Specs.
    Falcor::SharedPtr<Buffer> mpVoxelAABBBuffer;
    Falcor::SharedPtr<Buffer> mpVoxelBuffer;
    uint32_t mVoxelCount = 0;

    // Compute passes used to build the SVS.
    Falcor::SharedPtr<ComputePass> mpCountSurfaceVoxelsPass;
    Falcor::SharedPtr<ComputePass> mpSDFSVSVoxelizerPass;

    // Scratch data used for building.
    Falcor::SharedPtr<Buffer> mpSurfaceVoxelCounter;
    Falcor::SharedPtr<Texture> mpSDFGridTexture;
};

} // namespace Falcor

#endif // SRC_FALCOR_SCENE_SDFS_SPARSEVOXELSET_SDFSVS_H_