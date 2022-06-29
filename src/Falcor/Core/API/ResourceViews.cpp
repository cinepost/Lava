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
#include "stdafx.h"

#include "Falcor/Core/API/Device.h"
#include "ResourceViews.h"

namespace Falcor {

ShaderResourceView::SharedPtr ShaderResourceView::getNullView(Device::SharedPtr pDevice, ShaderResourceView::Dimension dimension) {
    auto nullViews = pDevice->nullResourceViews();
    assert((size_t)dimension < nullViews.srv.size() && nullViews.srv[(size_t)dimension]);
    return nullViews.srv[(size_t)dimension];
}

UnorderedAccessView::SharedPtr UnorderedAccessView::getNullView(Device::SharedPtr pDevice, UnorderedAccessView::Dimension dimension) {
    auto nullViews = pDevice->nullResourceViews();
    assert((size_t)dimension < nullViews.uav.size() && nullViews.uav[(size_t)dimension]);
    return nullViews.uav[(size_t)dimension];
}

DepthStencilView::SharedPtr DepthStencilView::getNullView(Device::SharedPtr pDevice, DepthStencilView::Dimension dimension) {
    auto nullViews = pDevice->nullResourceViews();
    assert((size_t)dimension < nullViews.dsv.size() && nullViews.dsv[(size_t)dimension]);
    return nullViews.dsv[(size_t)dimension];
}

RenderTargetView::SharedPtr RenderTargetView::getNullView(Device::SharedPtr pDevice, RenderTargetView::Dimension dimension) {
    auto nullViews = pDevice->nullResourceViews();
    assert((size_t)dimension < nullViews.rtv.size() && nullViews.rtv[(size_t)dimension]);
    return nullViews.rtv[(size_t)dimension];
}

ConstantBufferView::SharedPtr ConstantBufferView::getNullView(Device::SharedPtr pDevice) {
    auto nullViews = pDevice->nullResourceViews();
    return nullViews.cbv;
}

#ifdef SCRIPTING
    FALCOR_SCRIPT_BINDING(ResourceView)
    {
        pybind11::class_<ShaderResourceView, ShaderResourceView::SharedPtr>(m, "ShaderResourceView");
        pybind11::class_<RenderTargetView, RenderTargetView::SharedPtr>(m, "RenderTargetView");
        pybind11::class_<UnorderedAccessView, UnorderedAccessView::SharedPtr>(m, "UnorderedAccessView");
        pybind11::class_<ConstantBufferView, ConstantBufferView::SharedPtr>(m, "ConstantBufferView");
        pybind11::class_<DepthStencilView, DepthStencilView::SharedPtr>(m, "DepthStencilView");
    }
#endif
}
