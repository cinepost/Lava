/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#include "Falcor/stdafx.h"
#include "Falcor/Utils/Debug/debug.h"

#include "ResourceViews.h"


namespace Falcor {

class Device;


using DeviceUID = uint8_t;

static NullResourceViews gNullViews[FALCOR_MAX_DEVICES];
static NullResourceViews gNullBufferViews[FALCOR_MAX_DEVICES];
static NullResourceViews gNullTypedBufferViews[FALCOR_MAX_DEVICES];

Buffer::SharedPtr getEmptyBuffer(std::shared_ptr<Device> pDevice);
Buffer::SharedPtr getEmptyTypedBuffer(std::shared_ptr<Device> pDevice);
Buffer::SharedPtr getEmptyConstantBuffer(std::shared_ptr<Device> pDevice);

Buffer::SharedPtr createZeroBuffer(std::shared_ptr<Device> pDevice);
Buffer::SharedPtr createZeroTypedBuffer(std::shared_ptr<Device> pDevice);

Texture::SharedPtr getEmptyTexture(std::shared_ptr<Device> pDevice);
Texture::SharedPtr createBlackTexture(std::shared_ptr<Device> pDevice);

void ReleaseBlackTextures(Device::SharedPtr pDevice);
void ReleaseZeroBuffers(Device::SharedPtr pDevice);
void ReleaseZeroTypedBuffers(Device::SharedPtr pDevice);
void ReleaseZeroConstantBuffers(Device::SharedPtr pDevice);

void createNullViews(std::shared_ptr<Device> pDevice) {
    assert(pDevice);
    gNullViews[pDevice->uid()].srv = ShaderResourceView::create(pDevice, getEmptyTexture(pDevice), 0, 1, 0, 1);
    gNullViews[pDevice->uid()].dsv = DepthStencilView::create(pDevice, getEmptyTexture(pDevice), 0, 0, 1);
    gNullViews[pDevice->uid()].uav = UnorderedAccessView::create(pDevice, getEmptyTexture(pDevice), 0, 0, 1);
    gNullViews[pDevice->uid()].rtv = RenderTargetView::create(pDevice, getEmptyTexture(pDevice), 0, 0, 1);
    gNullViews[pDevice->uid()].cbv = ConstantBufferView::create(pDevice, getEmptyConstantBuffer(pDevice));
}

void createNullBufferViews(std::shared_ptr<Device> pDevice) {
    assert(pDevice);
    gNullBufferViews[pDevice->uid()].srv = ShaderResourceView::create(pDevice, getEmptyBuffer(pDevice), 0, 0);
    gNullBufferViews[pDevice->uid()].uav = UnorderedAccessView::create(pDevice, getEmptyBuffer(pDevice), 0, 0);
}

void createNullTypedBufferViews(std::shared_ptr<Device> pDevice) {
    assert(pDevice);
    gNullTypedBufferViews[pDevice->uid()].srv = ShaderResourceView::create(pDevice, getEmptyTypedBuffer(pDevice), 0, 0);
    gNullTypedBufferViews[pDevice->uid()].uav = UnorderedAccessView::create(pDevice, getEmptyTypedBuffer(pDevice), 0, 0);
}

void releaseNullViews(Device::SharedPtr pDevice) {
    gNullViews[pDevice->uid()] = {};
}

void releaseNullBufferViews(Device::SharedPtr pDevice) {
    gNullBufferViews[pDevice->uid()] = {};
}

void releaseNullTypedBufferViews(Device::SharedPtr pDevice) {
    gNullTypedBufferViews[pDevice->uid()] = {};
}

ShaderResourceView::SharedPtr  ShaderResourceView::getNullView(std::shared_ptr<Device> pDevice)  { return gNullViews[pDevice->uid()].srv; }
DepthStencilView::SharedPtr    DepthStencilView::getNullView(std::shared_ptr<Device> pDevice)    { return gNullViews[pDevice->uid()].dsv; }
UnorderedAccessView::SharedPtr UnorderedAccessView::getNullView(std::shared_ptr<Device> pDevice) { return gNullViews[pDevice->uid()].uav; }
RenderTargetView::SharedPtr    RenderTargetView::getNullView(std::shared_ptr<Device> pDevice)    { return gNullViews[pDevice->uid()].rtv; }
ConstantBufferView::SharedPtr  ConstantBufferView::getNullView(std::shared_ptr<Device> pDevice)  { return gNullViews[pDevice->uid()].cbv; }

ShaderResourceView::SharedPtr  ShaderResourceView::getNullBufferView(std::shared_ptr<Device> pDevice)  { return gNullBufferViews[pDevice->uid()].srv; }
UnorderedAccessView::SharedPtr UnorderedAccessView::getNullBufferView(std::shared_ptr<Device> pDevice) { return gNullBufferViews[pDevice->uid()].uav; }

ShaderResourceView::SharedPtr  ShaderResourceView::getNullTypedBufferView(std::shared_ptr<Device> pDevice)  { return gNullTypedBufferViews[pDevice->uid()].srv; }
UnorderedAccessView::SharedPtr UnorderedAccessView::getNullTypedBufferView(std::shared_ptr<Device> pDevice) { return gNullTypedBufferViews[pDevice->uid()].uav; }

SCRIPT_BINDING(ResourceView) {
        pybind11::class_<ShaderResourceView, ShaderResourceView::SharedPtr>(m, "ShaderResourceView");
        pybind11::class_<RenderTargetView, RenderTargetView::SharedPtr>(m, "RenderTargetView");
        pybind11::class_<UnorderedAccessView, UnorderedAccessView::SharedPtr>(m, "UnorderedAccessView");
        pybind11::class_<ConstantBufferView, ConstantBufferView::SharedPtr>(m, "ConstantBufferView");
        pybind11::class_<DepthStencilView, DepthStencilView::SharedPtr>(m, "DepthStencilView");
}

}  // namespace Falcor
