#ifndef HDLAVA_RENDER_DELEGATE_H_
#define HDLAVA_RENDER_DELEGATE_H_

#include "api.h"
#include "renderThread.h"

#include "pxr/imaging/hd/renderDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdLavaDiagnosticMgrDelegate;
class HdLavaRenderParam;
class HdLavaApi;

class HdLavaDelegate final : public HdRenderDelegate {
public:

    HdLavaDelegate();
    ~HdLavaDelegate() override;

    HdLavaDelegate(const HdLavaDelegate&) = delete;
    HdLavaDelegate& operator =(const HdLavaDelegate&) = delete;

    const TfTokenVector& GetSupportedRprimTypes() const override;
    const TfTokenVector& GetSupportedSprimTypes() const override;
    const TfTokenVector& GetSupportedBprimTypes() const override;

    HdRenderParam* GetRenderParam() const override;
    HdResourceRegistrySharedPtr GetResourceRegistry() const override;

    HdRenderPassSharedPtr CreateRenderPass(HdRenderIndex* index, HdRprimCollection const& collection) override;

    HdInstancer* CreateInstancer(HdSceneDelegate* delegate, SdfPath const& id, SdfPath const& instancerId) override;
    void DestroyInstancer(HdInstancer* instancer) override;

    HdRprim* CreateRprim(TfToken const& typeId, SdfPath const& rprimId, SdfPath const& instancerId) override;
    void DestroyRprim(HdRprim* rPrim) override;

    HdSprim* CreateSprim(TfToken const& typeId, SdfPath const& sprimId) override;
    HdSprim* CreateFallbackSprim(TfToken const& typeId) override;
    void DestroySprim(HdSprim* sprim) override;

    HdBprim* CreateBprim(TfToken const& typeId,
                         SdfPath const& bprimId) override;
    HdBprim* CreateFallbackBprim(TfToken const& typeId) override;
    void DestroyBprim(HdBprim* bprim) override;

    void CommitResources(HdChangeTracker* tracker) override;

    TfToken GetMaterialBindingPurpose() const override { return HdTokens->full; }
    TfToken GetMaterialNetworkSelector() const override;

    HdAovDescriptor GetDefaultAovDescriptor(TfToken const& name) const override;

    HdRenderSettingDescriptorList GetRenderSettingDescriptors() const override;

    VtDictionary GetRenderStats() const override;

    bool IsPauseSupported() const override;
    bool Pause() override;
    bool Resume() override;

#if PXR_VERSION >= 2005
    bool IsStopSupported() const override;
    bool Stop() override;
    bool Restart() override;
#endif // PXR_VERSION >= 2005

    bool IsBatch() const { return m_isBatch; }
    bool IsProgressive() const { return m_isProgressive; }

private:
    static const TfTokenVector SUPPORTED_RPRIM_TYPES;
    static const TfTokenVector SUPPORTED_SPRIM_TYPES;
    static const TfTokenVector SUPPORTED_BPRIM_TYPES;

    bool m_isBatch;
    bool m_isProgressive;

    std::unique_ptr<HdLavaApi> m_rprApi;
    std::unique_ptr<HdLavaRenderParam> m_renderParam;
    HdRenderSettingDescriptorList m_settingDescriptors;
    HdLavaRenderThread m_renderThread;

    using DiagnostMgrDelegatePtr = std::unique_ptr<HdLavaDiagnosticMgrDelegate, std::function<void (HdLavaDiagnosticMgrDelegate*)>>;
    DiagnostMgrDelegatePtr m_diagnosticMgrDelegate;
};

TfToken const& HdLavaUtilsGetCameraDepthName();

PXR_NAMESPACE_CLOSE_SCOPE

extern "C" {

HDLAVA_API void SetHdLavaRenderDevice(int renderDevice);

HDLAVA_API void SetHdLavaRenderQuality(int quality);

HDLAVA_API int GetHdLavaRenderQuality();

HDLAVA_API int HdLavaExportRprSceneOnNextRender(const char* exportPath);

} // extern "C"

#endif // HDLAVA_RENDER_DELEGATE_H_
