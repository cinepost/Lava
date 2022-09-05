//
// Copyright 2018 Intel
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#ifndef HDLAVA_RENDERER_PLUGIN_H_
#define HDLAVA_RENDERER_PLUGIN_H_

#include "pxr/imaging/hd/rendererPlugin.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdLavaRendererPlugin final : public HdRendererPlugin {
public:
    HdLavaRendererPlugin() = default;
    ~HdLavaRendererPlugin() override = default;

    HdLavaRendererPlugin(const HdLavaRendererPlugin&) = delete;
    HdLavaRendererPlugin& operator =(const HdLavaRendererPlugin&) = delete;

    HdRenderDelegate* CreateRenderDelegate() override;
    HdRenderDelegate* CreateRenderDelegate(HdRenderSettingsMap const& settingsMap) override;

    void DeleteRenderDelegate(HdRenderDelegate* renderDelegate) override;

    bool IsSupported() const override { return true; }
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDLAVA_RENDERER_PLUGIN_H_
