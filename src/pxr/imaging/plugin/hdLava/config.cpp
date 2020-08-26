#include <type_traits>

#include "config.h"
#include "lavaApi.h"
#include "pxr/base/arch/fileSystem.h"
#include "pxr/usd/usdRender/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(HdLavaRenderSettingsTokens, HDLAVA_RENDER_SETTINGS_TOKENS);
TF_DEFINE_PRIVATE_TOKENS(_tokens, ((houdiniInteractive, "houdini:interactive")) );

namespace {

const RenderQualityType kRenderQualityDefault = RenderQualityType(3);
const RenderQualityType kRenderQualityMin = RenderQualityType(0);
const RenderQualityType kRenderQualityMax = RenderQualityType(4);

const RenderModeType kRenderModeDefault = RenderModeType(0);
const RenderModeType kRenderModeMin = RenderModeType(0);
const RenderModeType kRenderModeMax = RenderModeType(8);

const float kAoRadiusDefault = float(1.0);
const float kAoRadiusMin = float(0.0);
const float kAoRadiusMax = float(100.0);

const RenderDeviceType kRenderDeviceDefault = RenderDeviceType(1);
const RenderDeviceType kRenderDeviceMin = RenderDeviceType(0);
const RenderDeviceType kRenderDeviceMax = RenderDeviceType(1);

const bool kEnableDenoisingDefault = bool(false);

const int kMaxSamplesDefault = int(256);
const int kMaxSamplesMin = int(1);
const int kMaxSamplesMax = int(65536);

const int kMinAdaptiveSamplesDefault = int(64);
const int kMinAdaptiveSamplesMin = int(1);
const int kMinAdaptiveSamplesMax = int(65536);

const float kVarianceThresholdDefault = float(0.0);
const float kVarianceThresholdMin = float(0.0);
const float kVarianceThresholdMax = float(1.0);

const int kMaxRayDepthDefault = int(8);
const int kMaxRayDepthMin = int(1);
const int kMaxRayDepthMax = int(50);

const int kMaxRayDepthDiffuseDefault = int(3);
const int kMaxRayDepthDiffuseMin = int(0);
const int kMaxRayDepthDiffuseMax = int(50);

const int kMaxRayDepthGlossyDefault = int(3);
const int kMaxRayDepthGlossyMin = int(0);
const int kMaxRayDepthGlossyMax = int(50);

const int kMaxRayDepthRefractionDefault = int(3);
const int kMaxRayDepthRefractionMin = int(0);
const int kMaxRayDepthRefractionMax = int(50);

const int kMaxRayDepthGlossyRefractionDefault = int(3);
const int kMaxRayDepthGlossyRefractionMin = int(0);
const int kMaxRayDepthGlossyRefractionMax = int(50);

const int kMaxRayDepthShadowDefault = int(2);
const int kMaxRayDepthShadowMin = int(0);
const int kMaxRayDepthShadowMax = int(50);

const float kRaycastEpsilonDefault = float(2e-05);
const float kRaycastEpsilonMin = float(1e-06);
const float kRaycastEpsilonMax = float(1.0);

const bool kEnableRadianceClampingDefault = bool(false);

const float kRadianceClampingDefault = float(0.0);
const float kRadianceClampingMin = float(0.0);
const float kRadianceClampingMax = float(1000000.0);

const int kInteractiveMaxRayDepthDefault = int(2);
const int kInteractiveMaxRayDepthMin = int(1);
const int kInteractiveMaxRayDepthMax = int(50);

const bool kEnableTonemapDefault = bool(false);

const float kTonemapExposureDefault = float(0.125);
const float kTonemapExposureMin = float(0.0);
const float kTonemapExposureMax = float(10.0);

const float kTonemapSensitivityDefault = float(1.0);
const float kTonemapSensitivityMin = float(0.0);
const float kTonemapSensitivityMax = float(10.0);

const float kTonemapFstopDefault = float(1.0);
const float kTonemapFstopMin = float(0.0);
const float kTonemapFstopMax = float(100.0);

const float kTonemapGammaDefault = float(1.0);
const float kTonemapGammaMin = float(0.0);
const float kTonemapGammaMax = float(5.0);

const bool kEnableAlphaDefault = bool(true);

const TfToken kAspectRatioConformPolicyDefault = TfToken(UsdRenderTokens->expandAperture);

const bool kInstantaneousShutterDefault = bool(false);


} // namespace anonymous

HdRenderSettingDescriptorList HdLavaConfig::GetRenderSettingDescriptors() {
    HdRenderSettingDescriptorList settingDescs;
    settingDescs.push_back({"Render Quality", HdLavaRenderSettingsTokens->renderQuality, VtValue(kRenderQualityDefault)});
    settingDescs.push_back({"Render Mode", HdLavaRenderSettingsTokens->renderMode, VtValue(kRenderModeDefault)});
    settingDescs.push_back({"Ambient Occlusion Radius", HdLavaRenderSettingsTokens->aoRadius, VtValue(kAoRadiusDefault)});
    settingDescs.push_back({"Render Device", HdLavaRenderSettingsTokens->renderDevice, VtValue(kRenderDeviceDefault)});
    settingDescs.push_back({"Enable Denoising", HdLavaRenderSettingsTokens->enableDenoising, VtValue(kEnableDenoisingDefault)});
    settingDescs.push_back({"Max Pixel Samples", HdLavaRenderSettingsTokens->maxSamples, VtValue(kMaxSamplesDefault)});
    settingDescs.push_back({"Min Pixel Samples", HdLavaRenderSettingsTokens->minAdaptiveSamples, VtValue(kMinAdaptiveSamplesDefault)});
    settingDescs.push_back({"Variance Threshold", HdLavaRenderSettingsTokens->varianceThreshold, VtValue(kVarianceThresholdDefault)});
    settingDescs.push_back({"Max Ray Depth", HdLavaRenderSettingsTokens->maxRayDepth, VtValue(kMaxRayDepthDefault)});
    settingDescs.push_back({"Diffuse Ray Depth", HdLavaRenderSettingsTokens->maxRayDepthDiffuse, VtValue(kMaxRayDepthDiffuseDefault)});
    settingDescs.push_back({"Glossy Ray Depth", HdLavaRenderSettingsTokens->maxRayDepthGlossy, VtValue(kMaxRayDepthGlossyDefault)});
    settingDescs.push_back({"Refraction Ray Depth", HdLavaRenderSettingsTokens->maxRayDepthRefraction, VtValue(kMaxRayDepthRefractionDefault)});
    settingDescs.push_back({"Glossy Refraction Ray Depth", HdLavaRenderSettingsTokens->maxRayDepthGlossyRefraction, VtValue(kMaxRayDepthGlossyRefractionDefault)});
    settingDescs.push_back({"Shadow Ray Depth", HdLavaRenderSettingsTokens->maxRayDepthShadow, VtValue(kMaxRayDepthShadowDefault)});
    settingDescs.push_back({"Ray Cast Epsilon", HdLavaRenderSettingsTokens->raycastEpsilon, VtValue(kRaycastEpsilonDefault)});
    settingDescs.push_back({"Enable Clamp Radiance", HdLavaRenderSettingsTokens->enableRadianceClamping, VtValue(kEnableRadianceClampingDefault)});
    settingDescs.push_back({"Clamp Radiance", HdLavaRenderSettingsTokens->radianceClamping, VtValue(kRadianceClampingDefault)});
    settingDescs.push_back({"Interactive Max Ray Depth", HdLavaRenderSettingsTokens->interactiveMaxRayDepth, VtValue(kInteractiveMaxRayDepthDefault)});
    settingDescs.push_back({"Enable Tone Mapping", HdLavaRenderSettingsTokens->enableTonemap, VtValue(kEnableTonemapDefault)});
    settingDescs.push_back({"Tone Mapping Exposure", HdLavaRenderSettingsTokens->tonemapExposure, VtValue(kTonemapExposureDefault)});
    settingDescs.push_back({"Tone Mapping Sensitivity", HdLavaRenderSettingsTokens->tonemapSensitivity, VtValue(kTonemapSensitivityDefault)});
    settingDescs.push_back({"Tone Mapping Fstop", HdLavaRenderSettingsTokens->tonemapFstop, VtValue(kTonemapFstopDefault)});
    settingDescs.push_back({"Tone Mapping Gamma", HdLavaRenderSettingsTokens->tonemapGamma, VtValue(kTonemapGammaDefault)});
    settingDescs.push_back({"Enable Color Alpha", HdLavaRenderSettingsTokens->enableAlpha, VtValue(kEnableAlphaDefault)});

    return settingDescs;
}


std::unique_lock<std::mutex> HdLavaConfig::GetInstance(HdLavaConfig** instancePtr) {
    static std::mutex instanceMutex;
    static HdLavaConfig instance;
    *instancePtr = &instance;
    return std::unique_lock<std::mutex>(instanceMutex);
}

void HdLavaConfig::Sync(HdRenderDelegate* renderDelegate) {
    int currentSettingsVersion = renderDelegate->GetRenderSettingsVersion();
    if (m_lastRenderSettingsVersion != currentSettingsVersion) {
        m_lastRenderSettingsVersion = currentSettingsVersion;
    
        auto getBoolSetting = [&renderDelegate](TfToken const& token, bool defaultValue) {
            auto boolValue = renderDelegate->GetRenderSetting(token);
            if (boolValue.IsHolding<int64_t>()) {
                return static_cast<bool>(boolValue.UncheckedGet<int64_t>());
            } else if (boolValue.IsHolding<bool>()) {
                return static_cast<bool>(boolValue.UncheckedGet<bool>());
            }
            return defaultValue;
        };

        auto interactiveMode = renderDelegate->GetRenderSetting<std::string>(_tokens->houdiniInteractive, "normal");
        SetInteractiveMode(interactiveMode != "normal");

        SetRenderQuality(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->renderQuality, int(kRenderQualityDefault)));
        SetRenderMode(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->renderMode, int(kRenderModeDefault)));
        SetAoRadius(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->aoRadius, float(kAoRadiusDefault)));
        SetRenderDevice(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->renderDevice, int(kRenderDeviceDefault)));
        SetEnableDenoising(getBoolSetting(HdLavaRenderSettingsTokens->enableDenoising, kEnableDenoisingDefault));
        SetMaxSamples(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->maxSamples, int(kMaxSamplesDefault)));
        SetMinAdaptiveSamples(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->minAdaptiveSamples, int(kMinAdaptiveSamplesDefault)));
        SetVarianceThreshold(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->varianceThreshold, float(kVarianceThresholdDefault)));
        SetMaxRayDepth(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->maxRayDepth, int(kMaxRayDepthDefault)));
        SetMaxRayDepthDiffuse(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->maxRayDepthDiffuse, int(kMaxRayDepthDiffuseDefault)));
        SetMaxRayDepthGlossy(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->maxRayDepthGlossy, int(kMaxRayDepthGlossyDefault)));
        SetMaxRayDepthRefraction(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->maxRayDepthRefraction, int(kMaxRayDepthRefractionDefault)));
        SetMaxRayDepthGlossyRefraction(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->maxRayDepthGlossyRefraction, int(kMaxRayDepthGlossyRefractionDefault)));
        SetMaxRayDepthShadow(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->maxRayDepthShadow, int(kMaxRayDepthShadowDefault)));
        SetRaycastEpsilon(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->raycastEpsilon, float(kRaycastEpsilonDefault)));
        SetEnableRadianceClamping(getBoolSetting(HdLavaRenderSettingsTokens->enableRadianceClamping, kEnableRadianceClampingDefault));
        SetRadianceClamping(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->radianceClamping, float(kRadianceClampingDefault)));
        SetInteractiveMaxRayDepth(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->interactiveMaxRayDepth, int(kInteractiveMaxRayDepthDefault)));
        SetEnableTonemap(getBoolSetting(HdLavaRenderSettingsTokens->enableTonemap, kEnableTonemapDefault));
        SetTonemapExposure(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->tonemapExposure, float(kTonemapExposureDefault)));
        SetTonemapSensitivity(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->tonemapSensitivity, float(kTonemapSensitivityDefault)));
        SetTonemapFstop(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->tonemapFstop, float(kTonemapFstopDefault)));
        SetTonemapGamma(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->tonemapGamma, float(kTonemapGammaDefault)));
        SetEnableAlpha(getBoolSetting(HdLavaRenderSettingsTokens->enableAlpha, kEnableAlphaDefault));
        SetAspectRatioConformPolicy(renderDelegate->GetRenderSetting(HdLavaRenderSettingsTokens->aspectRatioConformPolicy, TfToken(kAspectRatioConformPolicyDefault)));
        SetInstantaneousShutter(getBoolSetting(HdLavaRenderSettingsTokens->instantaneousShutter, kInstantaneousShutterDefault));

    }
}

void HdLavaConfig::SetInteractiveMode(bool enable) {
    if (m_prefData.enableInteractive != enable) {
        m_prefData.enableInteractive = enable;
        m_prefData.Save();
        m_dirtyFlags |= DirtyInteractiveMode;
    }
}

bool HdLavaConfig::GetInteractiveMode() const {
    return m_prefData.enableInteractive;
}


void HdLavaConfig::SetRenderQuality(int renderQuality) {
    if (renderQuality < kRenderQualityMin) { return; }
    if (renderQuality > kRenderQualityMax) { return; }

    if (m_prefData.renderQuality != renderQuality) {
        m_prefData.renderQuality = RenderQualityType(renderQuality);
        m_prefData.Save();
        m_dirtyFlags |= DirtyRenderQuality;
    }
}

void HdLavaConfig::SetRenderMode(int renderMode) {
    if (renderMode < kRenderModeMin) { return; }
    if (renderMode > kRenderModeMax) { return; }

    if (m_prefData.renderMode != renderMode) {
        m_prefData.renderMode = RenderModeType(renderMode);
        m_prefData.Save();
        m_dirtyFlags |= DirtyRenderMode;
    }
}

void HdLavaConfig::SetAoRadius(float aoRadius) {
    if (aoRadius < kAoRadiusMin) { return; }
    if (aoRadius > kAoRadiusMax) { return; }

    if (m_prefData.aoRadius != aoRadius) {
        m_prefData.aoRadius = float(aoRadius);
        m_prefData.Save();
        m_dirtyFlags |= DirtyRenderMode;
    }
}

void HdLavaConfig::SetRenderDevice(int renderDevice) {
    if (renderDevice < kRenderDeviceMin) { return; }
    if (renderDevice > kRenderDeviceMax) { return; }

    if (m_prefData.renderDevice != renderDevice) {
        m_prefData.renderDevice = RenderDeviceType(renderDevice);
        m_prefData.Save();
        m_dirtyFlags |= DirtyDevice;
    }
}

void HdLavaConfig::SetEnableDenoising(bool enableDenoising) {

    if (m_prefData.enableDenoising != enableDenoising) {
        m_prefData.enableDenoising = bool(enableDenoising);
        m_prefData.Save();
        m_dirtyFlags |= DirtyDenoise;
    }
}

void HdLavaConfig::SetMaxSamples(int maxSamples) {
    if (maxSamples < kMaxSamplesMin) { return; }
    if (maxSamples > kMaxSamplesMax) { return; }

    if (m_prefData.maxSamples != maxSamples) {
        m_prefData.maxSamples = int(maxSamples);
        m_prefData.Save();
        m_dirtyFlags |= DirtySampling;
    }
}

void HdLavaConfig::SetMinAdaptiveSamples(int minAdaptiveSamples) {
    if (minAdaptiveSamples < kMinAdaptiveSamplesMin) { return; }
    if (minAdaptiveSamples > kMinAdaptiveSamplesMax) { return; }

    if (m_prefData.minAdaptiveSamples != minAdaptiveSamples) {
        m_prefData.minAdaptiveSamples = int(minAdaptiveSamples);
        m_prefData.Save();
        m_dirtyFlags |= DirtyAdaptiveSampling;
    }
}

void HdLavaConfig::SetVarianceThreshold(float varianceThreshold) {
    if (varianceThreshold < kVarianceThresholdMin) { return; }
    if (varianceThreshold > kVarianceThresholdMax) { return; }

    if (m_prefData.varianceThreshold != varianceThreshold) {
        m_prefData.varianceThreshold = float(varianceThreshold);
        m_prefData.Save();
        m_dirtyFlags |= DirtyAdaptiveSampling;
    }
}

void HdLavaConfig::SetMaxRayDepth(int maxRayDepth) {
    if (maxRayDepth < kMaxRayDepthMin) { return; }
    if (maxRayDepth > kMaxRayDepthMax) { return; }

    if (m_prefData.maxRayDepth != maxRayDepth) {
        m_prefData.maxRayDepth = int(maxRayDepth);
        m_prefData.Save();
        m_dirtyFlags |= DirtyQuality;
    }
}

void HdLavaConfig::SetMaxRayDepthDiffuse(int maxRayDepthDiffuse) {
    if (maxRayDepthDiffuse < kMaxRayDepthDiffuseMin) { return; }
    if (maxRayDepthDiffuse > kMaxRayDepthDiffuseMax) { return; }

    if (m_prefData.maxRayDepthDiffuse != maxRayDepthDiffuse) {
        m_prefData.maxRayDepthDiffuse = int(maxRayDepthDiffuse);
        m_prefData.Save();
        m_dirtyFlags |= DirtyQuality;
    }
}

void HdLavaConfig::SetMaxRayDepthGlossy(int maxRayDepthGlossy) {
    if (maxRayDepthGlossy < kMaxRayDepthGlossyMin) { return; }
    if (maxRayDepthGlossy > kMaxRayDepthGlossyMax) { return; }

    if (m_prefData.maxRayDepthGlossy != maxRayDepthGlossy) {
        m_prefData.maxRayDepthGlossy = int(maxRayDepthGlossy);
        m_prefData.Save();
        m_dirtyFlags |= DirtyQuality;
    }
}

void HdLavaConfig::SetMaxRayDepthRefraction(int maxRayDepthRefraction) {
    if (maxRayDepthRefraction < kMaxRayDepthRefractionMin) { return; }
    if (maxRayDepthRefraction > kMaxRayDepthRefractionMax) { return; }

    if (m_prefData.maxRayDepthRefraction != maxRayDepthRefraction) {
        m_prefData.maxRayDepthRefraction = int(maxRayDepthRefraction);
        m_prefData.Save();
        m_dirtyFlags |= DirtyQuality;
    }
}

void HdLavaConfig::SetMaxRayDepthGlossyRefraction(int maxRayDepthGlossyRefraction) {
    if (maxRayDepthGlossyRefraction < kMaxRayDepthGlossyRefractionMin) { return; }
    if (maxRayDepthGlossyRefraction > kMaxRayDepthGlossyRefractionMax) { return; }

    if (m_prefData.maxRayDepthGlossyRefraction != maxRayDepthGlossyRefraction) {
        m_prefData.maxRayDepthGlossyRefraction = int(maxRayDepthGlossyRefraction);
        m_prefData.Save();
        m_dirtyFlags |= DirtyQuality;
    }
}

void HdLavaConfig::SetMaxRayDepthShadow(int maxRayDepthShadow) {
    if (maxRayDepthShadow < kMaxRayDepthShadowMin) { return; }
    if (maxRayDepthShadow > kMaxRayDepthShadowMax) { return; }

    if (m_prefData.maxRayDepthShadow != maxRayDepthShadow) {
        m_prefData.maxRayDepthShadow = int(maxRayDepthShadow);
        m_prefData.Save();
        m_dirtyFlags |= DirtyQuality;
    }
}

void HdLavaConfig::SetRaycastEpsilon(float raycastEpsilon) {
    if (raycastEpsilon < kRaycastEpsilonMin) { return; }
    if (raycastEpsilon > kRaycastEpsilonMax) { return; }

    if (m_prefData.raycastEpsilon != raycastEpsilon) {
        m_prefData.raycastEpsilon = float(raycastEpsilon);
        m_prefData.Save();
        m_dirtyFlags |= DirtyQuality;
    }
}

void HdLavaConfig::SetEnableRadianceClamping(bool enableRadianceClamping) {

    if (m_prefData.enableRadianceClamping != enableRadianceClamping) {
        m_prefData.enableRadianceClamping = bool(enableRadianceClamping);
        m_prefData.Save();
        m_dirtyFlags |= DirtyQuality;
    }
}

void HdLavaConfig::SetRadianceClamping(float radianceClamping) {
    if (radianceClamping < kRadianceClampingMin) { return; }
    if (radianceClamping > kRadianceClampingMax) { return; }

    if (m_prefData.radianceClamping != radianceClamping) {
        m_prefData.radianceClamping = float(radianceClamping);
        m_prefData.Save();
        m_dirtyFlags |= DirtyQuality;
    }
}

void HdLavaConfig::SetInteractiveMaxRayDepth(int interactiveMaxRayDepth) {
    if (interactiveMaxRayDepth < kInteractiveMaxRayDepthMin) { return; }
    if (interactiveMaxRayDepth > kInteractiveMaxRayDepthMax) { return; }

    if (m_prefData.interactiveMaxRayDepth != interactiveMaxRayDepth) {
        m_prefData.interactiveMaxRayDepth = int(interactiveMaxRayDepth);
        m_prefData.Save();
        m_dirtyFlags |= DirtyQuality;
    }
}

void HdLavaConfig::SetEnableTonemap(bool enableTonemap) {

    if (m_prefData.enableTonemap != enableTonemap) {
        m_prefData.enableTonemap = bool(enableTonemap);
        m_prefData.Save();
        m_dirtyFlags |= DirtyTonemapping;
    }
}

void HdLavaConfig::SetTonemapExposure(float tonemapExposure) {
    if (tonemapExposure < kTonemapExposureMin) { return; }
    if (tonemapExposure > kTonemapExposureMax) { return; }

    if (m_prefData.tonemapExposure != tonemapExposure) {
        m_prefData.tonemapExposure = float(tonemapExposure);
        m_prefData.Save();
        m_dirtyFlags |= DirtyTonemapping;
    }
}

void HdLavaConfig::SetTonemapSensitivity(float tonemapSensitivity) {
    if (tonemapSensitivity < kTonemapSensitivityMin) { return; }
    if (tonemapSensitivity > kTonemapSensitivityMax) { return; }

    if (m_prefData.tonemapSensitivity != tonemapSensitivity) {
        m_prefData.tonemapSensitivity = float(tonemapSensitivity);
        m_prefData.Save();
        m_dirtyFlags |= DirtyTonemapping;
    }
}

void HdLavaConfig::SetTonemapFstop(float tonemapFstop) {
    if (tonemapFstop < kTonemapFstopMin) { return; }
    if (tonemapFstop > kTonemapFstopMax) { return; }

    if (m_prefData.tonemapFstop != tonemapFstop) {
        m_prefData.tonemapFstop = float(tonemapFstop);
        m_prefData.Save();
        m_dirtyFlags |= DirtyTonemapping;
    }
}

void HdLavaConfig::SetTonemapGamma(float tonemapGamma) {
    if (tonemapGamma < kTonemapGammaMin) { return; }
    if (tonemapGamma > kTonemapGammaMax) { return; }

    if (m_prefData.tonemapGamma != tonemapGamma) {
        m_prefData.tonemapGamma = float(tonemapGamma);
        m_prefData.Save();
        m_dirtyFlags |= DirtyTonemapping;
    }
}

void HdLavaConfig::SetEnableAlpha(bool enableAlpha) {

    if (m_prefData.enableAlpha != enableAlpha) {
        m_prefData.enableAlpha = bool(enableAlpha);
        m_prefData.Save();
        m_dirtyFlags |= DirtyAlpha;
    }
}

void HdLavaConfig::SetAspectRatioConformPolicy(TfToken aspectRatioConformPolicy) {

    if (m_prefData.aspectRatioConformPolicy != aspectRatioConformPolicy) {
        m_prefData.aspectRatioConformPolicy = TfToken(aspectRatioConformPolicy);
        m_prefData.Save();
        m_dirtyFlags |= DirtyUsdNativeCamera;
    }
}

void HdLavaConfig::SetInstantaneousShutter(bool instantaneousShutter) {

    if (m_prefData.instantaneousShutter != instantaneousShutter) {
        m_prefData.instantaneousShutter = bool(instantaneousShutter);
        m_prefData.Save();
        m_dirtyFlags |= DirtyUsdNativeCamera;
    }
}


bool HdLavaConfig::IsDirty(ChangeTracker dirtyFlag) const {
    return m_dirtyFlags & dirtyFlag;
}

void HdLavaConfig::CleanDirtyFlag(ChangeTracker dirtyFlag) {
    m_dirtyFlags &= ~dirtyFlag;
}

void HdLavaConfig::ResetDirty() {
    m_dirtyFlags = Clean;
}

bool HdLavaConfig::PrefData::Load() {
#ifdef ENABLE_PREFERENCES_FILE
    std::string appDataDir = HdLavaApi::GetAppDataPath();
    std::string rprPreferencePath = (appDataDir.empty()) ? k_rprPreferenceFilename : (appDataDir + ARCH_PATH_SEP) + k_rprPreferenceFilename;

    if (FILE* f = fopen(rprPreferencePath.c_str(), "rb")) {
        if (!fread(this, sizeof(PrefData), 1, f)) {
            TF_RUNTIME_ERROR("Fail to read rpr preferences dat file");
        }
        fclose(f);
        return IsValid();
    }
#endif // ENABLE_PREFERENCES_FILE

    return false;
}

void HdLavaConfig::PrefData::Save() {
#ifdef ENABLE_PREFERENCES_FILE
    std::string appDataDir = HdLavaApi::GetAppDataPath();
    std::string rprPreferencePath = (appDataDir.empty()) ? k_rprPreferenceFilename : (appDataDir + ARCH_PATH_SEP) + k_rprPreferenceFilename;

    if (FILE* f = fopen(rprPreferencePath.c_str(), "wb")) {
        if (!fwrite(this, sizeof(PrefData), 1, f)) {
            TF_CODING_ERROR("Fail to write rpr preferences dat file");
        }
        fclose(f);
    }
#endif // ENABLE_PREFERENCES_FILE
}

HdLavaConfig::PrefData::PrefData() {
    if (!Load()) {
        SetDefault();
    }
}

HdLavaConfig::PrefData::~PrefData() {
    Save();
}

void HdLavaConfig::PrefData::SetDefault() {
    enableInteractive = false;

    renderQuality = kRenderQualityDefault;
    renderMode = kRenderModeDefault;
    aoRadius = kAoRadiusDefault;
    renderDevice = kRenderDeviceDefault;
    enableDenoising = kEnableDenoisingDefault;
    maxSamples = kMaxSamplesDefault;
    minAdaptiveSamples = kMinAdaptiveSamplesDefault;
    varianceThreshold = kVarianceThresholdDefault;
    maxRayDepth = kMaxRayDepthDefault;
    maxRayDepthDiffuse = kMaxRayDepthDiffuseDefault;
    maxRayDepthGlossy = kMaxRayDepthGlossyDefault;
    maxRayDepthRefraction = kMaxRayDepthRefractionDefault;
    maxRayDepthGlossyRefraction = kMaxRayDepthGlossyRefractionDefault;
    maxRayDepthShadow = kMaxRayDepthShadowDefault;
    raycastEpsilon = kRaycastEpsilonDefault;
    enableRadianceClamping = kEnableRadianceClampingDefault;
    radianceClamping = kRadianceClampingDefault;
    interactiveMaxRayDepth = kInteractiveMaxRayDepthDefault;
    enableTonemap = kEnableTonemapDefault;
    tonemapExposure = kTonemapExposureDefault;
    tonemapSensitivity = kTonemapSensitivityDefault;
    tonemapFstop = kTonemapFstopDefault;
    tonemapGamma = kTonemapGammaDefault;
    enableAlpha = kEnableAlphaDefault;
    aspectRatioConformPolicy = kAspectRatioConformPolicyDefault;
    instantaneousShutter = kInstantaneousShutterDefault;

}

bool HdLavaConfig::PrefData::IsValid() {
    return true
           && renderQuality < kRenderQualityMin&& renderQuality > kRenderQualityMax
           && renderMode < kRenderModeMin&& renderMode > kRenderModeMax
           && aoRadius < kAoRadiusMin&& aoRadius > kAoRadiusMax
           && renderDevice < kRenderDeviceMin&& renderDevice > kRenderDeviceMax
           && maxSamples < kMaxSamplesMin&& maxSamples > kMaxSamplesMax
           && minAdaptiveSamples < kMinAdaptiveSamplesMin&& minAdaptiveSamples > kMinAdaptiveSamplesMax
           && varianceThreshold < kVarianceThresholdMin&& varianceThreshold > kVarianceThresholdMax
           && maxRayDepth < kMaxRayDepthMin&& maxRayDepth > kMaxRayDepthMax
           && maxRayDepthDiffuse < kMaxRayDepthDiffuseMin&& maxRayDepthDiffuse > kMaxRayDepthDiffuseMax
           && maxRayDepthGlossy < kMaxRayDepthGlossyMin&& maxRayDepthGlossy > kMaxRayDepthGlossyMax
           && maxRayDepthRefraction < kMaxRayDepthRefractionMin&& maxRayDepthRefraction > kMaxRayDepthRefractionMax
           && maxRayDepthGlossyRefraction < kMaxRayDepthGlossyRefractionMin&& maxRayDepthGlossyRefraction > kMaxRayDepthGlossyRefractionMax
           && maxRayDepthShadow < kMaxRayDepthShadowMin&& maxRayDepthShadow > kMaxRayDepthShadowMax
           && raycastEpsilon < kRaycastEpsilonMin&& raycastEpsilon > kRaycastEpsilonMax
           && radianceClamping < kRadianceClampingMin&& radianceClamping > kRadianceClampingMax
           && interactiveMaxRayDepth < kInteractiveMaxRayDepthMin&& interactiveMaxRayDepth > kInteractiveMaxRayDepthMax
           && tonemapExposure < kTonemapExposureMin&& tonemapExposure > kTonemapExposureMax
           && tonemapSensitivity < kTonemapSensitivityMin&& tonemapSensitivity > kTonemapSensitivityMax
           && tonemapFstop < kTonemapFstopMin&& tonemapFstop > kTonemapFstopMax
           && tonemapGamma < kTonemapGammaMin&& tonemapGamma > kTonemapGammaMax
;
}

PXR_NAMESPACE_CLOSE_SCOPE