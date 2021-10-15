//
// Copyright 2020 Pixar
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
#include "renderDelegate.h"
#include "mesh.h"
#include "renderPass.h"

// #include "Falcor/Utils/ConfigStore.h"
// #include "Falcor/Core/API/DeviceManager.h"

// #include "lava_lib/renderer.h"
// #include "lava_lib/scene_readers_registry.h"
// #include "lava_lib/reader_lsd/reader_lsd.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

static HdLavaApi* g_lavaApi = nullptr;

const TfTokenVector HdLavaTinyRenderDelegate::SUPPORTED_RPRIM_TYPES =
{
    HdPrimTypeTokens->mesh,
};

const TfTokenVector HdLavaTinyRenderDelegate::SUPPORTED_SPRIM_TYPES =
{
};

const TfTokenVector HdLavaTinyRenderDelegate::SUPPORTED_BPRIM_TYPES =
{
};

HdLavaTinyRenderDelegate::HdLavaTinyRenderDelegate()
    : HdRenderDelegate()
{
    _Initialize();
}

HdLavaTinyRenderDelegate::HdLavaTinyRenderDelegate(
    HdRenderSettingsMap const& settingsMap)
    : HdRenderDelegate(settingsMap)
{
    _Initialize();
}

void listGPUs() {
    auto pDeviceManager = DeviceManager::create();
    std::cout << "Available rendering devices:\n";
    const auto& deviceMap = pDeviceManager->listDevices();
    for( auto const& [gpu_id, name]: deviceMap ) {
        std::cout << "\t[" << to_string(gpu_id) << "] : " << name << "\n";
    }
    std::cout << std::endl;
}

void
HdLavaTinyRenderDelegate::_InitRender() {
    using namespace lava;

    int gpuID = 0; // automatic gpu selection

    // // Populate Renderer_IO_Registry with internal and external scene translators
    // SceneReadersRegistry::getInstance().addReader(
    //   ReaderLSD::myExtensions, 
    //   ReaderLSD::myConstructor
    // );

    // auto pDeviceManager = DeviceManager::create();
    pDeviceManager = DeviceManager::create();
    if (!pDeviceManager)
      exit(EXIT_FAILURE);

    pDeviceManager->setDefaultRenderingDevice(gpuID);

    Falcor::Device::Desc device_desc;
    device_desc.width = 1280;
    device_desc.height = 720;

    std::cout << "Creating rendering device id " << to_string(gpuID) << std::endl;
    // auto pDevice = pDeviceManager->createRenderingDevice(gpuID, device_desc);
    pDevice = pDeviceManager->createRenderingDevice(gpuID, device_desc);
    std::cout << "Rendering device " << to_string(gpuID) << " created" << std::endl;

    // Renderer::SharedPtr pRenderer = Renderer::create(pDevice);
    pRenderer = Renderer::create(pDevice);

    if(!pRenderer->init()) {
      exit(EXIT_FAILURE);
    }
}

void
HdLavaTinyRenderDelegate::_Initialize()
{
    std::cout << "Creating LavaTiny RenderDelegate" << std::endl;
    _resourceRegistry = std::make_shared<HdResourceRegistry>();

    m_lavaApi.reset(new HdLavaApi(this));
    g_lavaApi = m_lavaApi.get();

    m_renderParam.reset(new HdLavaRenderParam(m_lavaApi.get(), &m_renderThread));

    m_renderThread.SetRenderCallback([this]() {
        m_lavaApi->Render(&m_renderThread);
    });
    m_renderThread.SetStopCallback([this]() {
        m_lavaApi->AbortRender();
    });
    m_renderThread.StartThread();

     try {
        // listGPUs();
        _InitRender();
    } catch (const std::runtime_error& e) {
        std::cout << e.what() << std::endl;
    }

    std::cout << "HdLavaTinyRenderDelegate::_InitRender() complete" << std::endl;
}

HdLavaTinyRenderDelegate::~HdLavaTinyRenderDelegate()
{
    _resourceRegistry.reset();
    std::cout << "Destroying Tiny RenderDelegate" << std::endl;

    if (pRenderer) {
        pRenderer.reset();
        std::cout << "pRenderer.reset() complete" << std::endl;
    }

    if (pDevice) {
        pDevice.reset();
        std::cout << "pDevice.reset() complete" << std::endl;
    }

    if (pDeviceManager) {
        pDeviceManager.reset();
        std::cout << "pDeviceManager.reset(); complete" << std::endl;
    }

    g_lavaApi = nullptr;
}

TfTokenVector const&
HdLavaTinyRenderDelegate::GetSupportedRprimTypes() const
{
    return SUPPORTED_RPRIM_TYPES;
}

TfTokenVector const&
HdLavaTinyRenderDelegate::GetSupportedSprimTypes() const
{
    return SUPPORTED_SPRIM_TYPES;
}

TfTokenVector const&
HdLavaTinyRenderDelegate::GetSupportedBprimTypes() const
{
    return SUPPORTED_BPRIM_TYPES;
}

HdResourceRegistrySharedPtr
HdLavaTinyRenderDelegate::GetResourceRegistry() const
{
    return _resourceRegistry;
}

void 
HdLavaTinyRenderDelegate::CommitResources(HdChangeTracker *tracker)
{
    std::cout << "=> CommitResources RenderDelegate" << std::endl;
}

HdRenderPassSharedPtr 
HdLavaTinyRenderDelegate::CreateRenderPass(
    HdRenderIndex *index,
    HdRprimCollection const& collection)
{
    std::cout << "Create RenderPass with Collection=" 
        << collection.GetName() << std::endl; 

    return HdRenderPassSharedPtr(new HdLavaTinyRenderPass(index, collection));  
}

HdRprim *
HdLavaTinyRenderDelegate::CreateRprim(TfToken const& typeId,
                                    SdfPath const& rprimId,
                                    SdfPath const& instancerId)
{
    std::cout << "Create Tiny Rprim type=" << typeId.GetText() 
        << " id=" << rprimId 
        << " instancerId=" << instancerId 
        << std::endl;

    if (typeId == HdPrimTypeTokens->mesh) {
        return new HdLavaTinyMesh(rprimId, instancerId);
    } else {
        TF_CODING_ERROR("Unknown Rprim type=%s id=%s", 
            typeId.GetText(), 
            rprimId.GetText());
    }
    return nullptr;
}

void
HdLavaTinyRenderDelegate::DestroyRprim(HdRprim *rPrim)
{
    std::cout << "Destroy Tiny Rprim id=" << rPrim->GetId() << std::endl;
    delete rPrim;
}

HdSprim *
HdLavaTinyRenderDelegate::CreateSprim(TfToken const& typeId,
                                    SdfPath const& sprimId)
{
    TF_CODING_ERROR("Unknown Sprim type=%s id=%s", 
        typeId.GetText(), 
        sprimId.GetText());
    return nullptr;
}

HdSprim *
HdLavaTinyRenderDelegate::CreateFallbackSprim(TfToken const& typeId)
{
    TF_CODING_ERROR("Creating unknown fallback sprim type=%s", 
        typeId.GetText()); 
    return nullptr;
}

void
HdLavaTinyRenderDelegate::DestroySprim(HdSprim *sPrim)
{
    TF_CODING_ERROR("Destroy Sprim not supported");
}

HdBprim *
HdLavaTinyRenderDelegate::CreateBprim(TfToken const& typeId, SdfPath const& bprimId)
{
    TF_CODING_ERROR("Unknown Bprim type=%s id=%s", 
        typeId.GetText(), 
        bprimId.GetText());
    return nullptr;
}

HdBprim *
HdLavaTinyRenderDelegate::CreateFallbackBprim(TfToken const& typeId)
{
    TF_CODING_ERROR("Creating unknown fallback bprim type=%s", 
        typeId.GetText()); 
    return nullptr;
}

void
HdLavaTinyRenderDelegate::DestroyBprim(HdBprim *bPrim)
{
    TF_CODING_ERROR("Destroy Bprim not supported");
}

HdInstancer *
HdLavaTinyRenderDelegate::CreateInstancer(
    HdSceneDelegate *delegate,
    SdfPath const& id,
    SdfPath const& instancerId)
{
    TF_CODING_ERROR("Creating Instancer not supported id=%s instancerId=%s", 
        id.GetText(), instancerId.GetText());
    return nullptr;
}

void 
HdLavaTinyRenderDelegate::DestroyInstancer(HdInstancer *instancer)
{
    TF_CODING_ERROR("Destroy instancer not supported");
}

HdRenderParam *
HdLavaTinyRenderDelegate::GetRenderParam() const
{
    // return nullptr;
    return m_renderParam.get();
}

PXR_NAMESPACE_CLOSE_SCOPE
