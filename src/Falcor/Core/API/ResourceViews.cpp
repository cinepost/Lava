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

static NullResourceViews gNullViews;
static NullResourceViews gNullBufferViews;
static NullResourceViews gNullTypedBufferViews;

Buffer::SharedPtr getEmptyBuffer(Device::SharedPtr device);
Buffer::SharedPtr getEmptyTypedBuffer(Device::SharedPtr device);
Buffer::SharedPtr createZeroBuffer(Device::SharedPtr device);
Buffer::SharedPtr createZeroTypedBuffer(Device::SharedPtr device);

Texture::SharedPtr getEmptyTexture(Device::SharedPtr device);
Texture::SharedPtr createBlackTexture(Device::SharedPtr device);

void createNullViews(Device::SharedPtr device) {
    gNullViews.srv = ShaderResourceView::create(getEmptyTexture(device), 0, 1, 0, 1);
    gNullViews.dsv = DepthStencilView::create(getEmptyTexture(device), 0, 0, 1);
    gNullViews.uav = UnorderedAccessView::create(getEmptyTexture(device), 0, 0, 1);
    gNullViews.rtv = RenderTargetView::create(getEmptyTexture(device), 0, 0, 1);
    gNullViews.cbv = ConstantBufferView::create(Buffer::SharedPtr());
}

void createNullBufferViews(Device::SharedPtr device) {
    gNullBufferViews.srv = ShaderResourceView::create(getEmptyBuffer(device), 0, 0);
    gNullBufferViews.uav = UnorderedAccessView::create(getEmptyBuffer(device), 0, 0);
}

void createNullTypedBufferViews(Device::SharedPtr device) {
    gNullTypedBufferViews.srv = ShaderResourceView::create(getEmptyTypedBuffer(device), 0, 0);
    gNullTypedBufferViews.uav = UnorderedAccessView::create(getEmptyTypedBuffer(device), 0, 0);
}

void releaseNullViews() {
    gNullViews = {};
}

void releaseNullBufferViews() {
    gNullBufferViews = {};
}

void releaseNullTypedBufferViews() {
    gNullTypedBufferViews = {};
}

ShaderResourceView::SharedPtr  ShaderResourceView::getNullView()  { return gNullViews.srv; }
DepthStencilView::SharedPtr    DepthStencilView::getNullView()    { return gNullViews.dsv; }
UnorderedAccessView::SharedPtr UnorderedAccessView::getNullView() { return gNullViews.uav; }
RenderTargetView::SharedPtr    RenderTargetView::getNullView()    { return gNullViews.rtv; }
ConstantBufferView::SharedPtr  ConstantBufferView::getNullView()  { return gNullViews.cbv; }

ShaderResourceView::SharedPtr  ShaderResourceView::getNullBufferView()  { return gNullBufferViews.srv; }
UnorderedAccessView::SharedPtr UnorderedAccessView::getNullBufferView() { return gNullBufferViews.uav; }

ShaderResourceView::SharedPtr  ShaderResourceView::getNullTypedBufferView()  { return gNullTypedBufferViews.srv; }
UnorderedAccessView::SharedPtr UnorderedAccessView::getNullTypedBufferView() { return gNullTypedBufferViews.uav; }

SCRIPT_BINDING(ResourceView) {
    m.regClass(ShaderResourceView);
    m.regClass(RenderTargetView);
    m.regClass(UnorderedAccessView);
    m.regClass(ConstantBufferView);
    m.regClass(DepthStencilView);
}

}  // namespace Falcor
