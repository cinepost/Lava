#ifndef HDLAVA_CONFIG_H_
#define HDLAVA_CONFIG_H_

#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/renderDelegate.h"

#include <mutex>

PXR_NAMESPACE_OPEN_SCOPE

#define HDLAVA_RENDER_SETTINGS_TOKENS \
    (renderQuality) \
    (renderMode) \
    (aoRadius) \
    (renderDevice) \
    (enableDenoising) \
    (maxSamples) \
    (minAdaptiveSamples) \
    (varianceThreshold) \
    (maxRayDepth) \
    (maxRayDepthDiffuse) \
    (maxRayDepthGlossy) \
    (maxRayDepthRefraction) \
    (maxRayDepthGlossyRefraction) \
    (maxRayDepthShadow) \
    (raycastEpsilon) \
    (enableRadianceClamping) \
    (radianceClamping) \
    (interactiveMaxRayDepth) \
    (enableTonemap) \
    (tonemapExposure) \
    (tonemapSensitivity) \
    (tonemapFstop) \
    (tonemapGamma) \
    (enableAlpha) \
    (aspectRatioConformPolicy) \
    (instantaneousShutter) \

TF_DECLARE_PUBLIC_TOKENS(HdLavaRenderSettingsTokens, HDLAVA_RENDER_SETTINGS_TOKENS);
enum RenderQualityType {
    kRenderQualityLow,
    kRenderQualityMedium,
    kRenderQualityHigh,
    kRenderQualityFull,
    kRenderQualityNorthstar,
};
enum RenderModeType {
    kRenderModeGlobalIllumination,
    kRenderModeDirectIllumination,
    kRenderModeWireframe,
    kRenderModeMaterialIndex,
    kRenderModePosition,
    kRenderModeNormal,
    kRenderModeTexcoord,
    kRenderModeAmbientOcclusion,
    kRenderModeDiffuse,
};
enum RenderDeviceType {
    kRenderDeviceCPU,
    kRenderDeviceGPU,
};


class HdLavaConfig {
public:
    enum ChangeTracker {
        Clean = 0,
        DirtyAll = ~0u,
        DirtyInteractiveMode = 1 << 0,
        DirtyRenderQuality = 1 << 1,
        DirtyRenderMode = 1 << 2,
        DirtyDevice = 1 << 3,
        DirtyDenoise = 1 << 4,
        DirtySampling = 1 << 5,
        DirtyAdaptiveSampling = 1 << 6,
        DirtyQuality = 1 << 7,
        DirtyTonemapping = 1 << 8,
        DirtyAlpha = 1 << 9,
        DirtyUsdNativeCamera = 1 << 10,

    };

    static HdRenderSettingDescriptorList GetRenderSettingDescriptors();
    static std::unique_lock<std::mutex> GetInstance(HdLavaConfig** instance);

    void Sync(HdRenderDelegate* renderDelegate);

    void SetInteractiveMode(bool enable);
    bool GetInteractiveMode() const;

    void SetRenderQuality(int renderQuality);
    RenderQualityType GetRenderQuality() const { return m_prefData.renderQuality; }

    void SetRenderMode(int renderMode);
    RenderModeType GetRenderMode() const { return m_prefData.renderMode; }

    void SetAoRadius(float aoRadius);
    float GetAoRadius() const { return m_prefData.aoRadius; }

    void SetRenderDevice(int renderDevice);
    RenderDeviceType GetRenderDevice() const { return m_prefData.renderDevice; }

    void SetEnableDenoising(bool enableDenoising);
    bool GetEnableDenoising() const { return m_prefData.enableDenoising; }

    void SetMaxSamples(int maxSamples);
    int GetMaxSamples() const { return m_prefData.maxSamples; }

    void SetMinAdaptiveSamples(int minAdaptiveSamples);
    int GetMinAdaptiveSamples() const { return m_prefData.minAdaptiveSamples; }

    void SetVarianceThreshold(float varianceThreshold);
    float GetVarianceThreshold() const { return m_prefData.varianceThreshold; }

    void SetMaxRayDepth(int maxRayDepth);
    int GetMaxRayDepth() const { return m_prefData.maxRayDepth; }

    void SetMaxRayDepthDiffuse(int maxRayDepthDiffuse);
    int GetMaxRayDepthDiffuse() const { return m_prefData.maxRayDepthDiffuse; }

    void SetMaxRayDepthGlossy(int maxRayDepthGlossy);
    int GetMaxRayDepthGlossy() const { return m_prefData.maxRayDepthGlossy; }

    void SetMaxRayDepthRefraction(int maxRayDepthRefraction);
    int GetMaxRayDepthRefraction() const { return m_prefData.maxRayDepthRefraction; }

    void SetMaxRayDepthGlossyRefraction(int maxRayDepthGlossyRefraction);
    int GetMaxRayDepthGlossyRefraction() const { return m_prefData.maxRayDepthGlossyRefraction; }

    void SetMaxRayDepthShadow(int maxRayDepthShadow);
    int GetMaxRayDepthShadow() const { return m_prefData.maxRayDepthShadow; }

    void SetRaycastEpsilon(float raycastEpsilon);
    float GetRaycastEpsilon() const { return m_prefData.raycastEpsilon; }

    void SetEnableRadianceClamping(bool enableRadianceClamping);
    bool GetEnableRadianceClamping() const { return m_prefData.enableRadianceClamping; }

    void SetRadianceClamping(float radianceClamping);
    float GetRadianceClamping() const { return m_prefData.radianceClamping; }

    void SetInteractiveMaxRayDepth(int interactiveMaxRayDepth);
    int GetInteractiveMaxRayDepth() const { return m_prefData.interactiveMaxRayDepth; }

    void SetEnableTonemap(bool enableTonemap);
    bool GetEnableTonemap() const { return m_prefData.enableTonemap; }

    void SetTonemapExposure(float tonemapExposure);
    float GetTonemapExposure() const { return m_prefData.tonemapExposure; }

    void SetTonemapSensitivity(float tonemapSensitivity);
    float GetTonemapSensitivity() const { return m_prefData.tonemapSensitivity; }

    void SetTonemapFstop(float tonemapFstop);
    float GetTonemapFstop() const { return m_prefData.tonemapFstop; }

    void SetTonemapGamma(float tonemapGamma);
    float GetTonemapGamma() const { return m_prefData.tonemapGamma; }

    void SetEnableAlpha(bool enableAlpha);
    bool GetEnableAlpha() const { return m_prefData.enableAlpha; }

    void SetAspectRatioConformPolicy(TfToken aspectRatioConformPolicy);
    TfToken GetAspectRatioConformPolicy() const { return m_prefData.aspectRatioConformPolicy; }

    void SetInstantaneousShutter(bool instantaneousShutter);
    bool GetInstantaneousShutter() const { return m_prefData.instantaneousShutter; }


    bool IsDirty(ChangeTracker dirtyFlag) const;
    void CleanDirtyFlag(ChangeTracker dirtyFlag);
    void ResetDirty();

private:
    HdLavaConfig() = default;

    struct PrefData {
        bool enableInteractive;

        RenderQualityType renderQuality;
        RenderModeType renderMode;
        float aoRadius;
        RenderDeviceType renderDevice;
        bool enableDenoising;
        int maxSamples;
        int minAdaptiveSamples;
        float varianceThreshold;
        int maxRayDepth;
        int maxRayDepthDiffuse;
        int maxRayDepthGlossy;
        int maxRayDepthRefraction;
        int maxRayDepthGlossyRefraction;
        int maxRayDepthShadow;
        float raycastEpsilon;
        bool enableRadianceClamping;
        float radianceClamping;
        int interactiveMaxRayDepth;
        bool enableTonemap;
        float tonemapExposure;
        float tonemapSensitivity;
        float tonemapFstop;
        float tonemapGamma;
        bool enableAlpha;
        TfToken aspectRatioConformPolicy;
        bool instantaneousShutter;


        PrefData();
        ~PrefData();

        void SetDefault();

        bool Load();
        void Save();

        bool IsValid();
    };
    PrefData m_prefData;

    uint32_t m_dirtyFlags = DirtyAll;
    int m_lastRenderSettingsVersion = -1;

    constexpr static const char* k_rprPreferenceFilename = "hdRprPreferences.dat";
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDLAVA_CONFIG_H_
