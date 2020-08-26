#include "rendererPlugin.h"
#include "renderDelegate.h"

#include "pxr/imaging/hd/rendererPluginRegistry.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType) {
    HdRendererPluginRegistry::Define<HdLavaRendererPlugin>();
}

HdRenderDelegate* HdLavaRendererPlugin::CreateRenderDelegate() {
	printf("HdLavaRendererPlugin::CreateRenderDelegate\n");
    return new HdLavaDelegate();
}

HdRenderDelegate* HdLavaRendererPlugin::CreateRenderDelegate(HdRenderSettingsMap const& settingsMap) {
	printf("HdLavaRendererPlugin::CreateRenderDelegate with settingsMap\n");
    auto renderDelegate = new HdLavaDelegate();
    for (auto& entry : settingsMap) {
        renderDelegate->SetRenderSetting(entry.first, entry.second);
    }
    return renderDelegate;
}

void HdLavaRendererPlugin::DeleteRenderDelegate(HdRenderDelegate* renderDelegate) {
	printf("HdLavaRendererPlugin::DeleteRenderDelegate\n");
    delete renderDelegate;
}

PXR_NAMESPACE_CLOSE_SCOPE
