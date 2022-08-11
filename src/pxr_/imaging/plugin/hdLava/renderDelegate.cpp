/************************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
************************************************************************/

#include "renderDelegate.h"

#include"pxr/imaging/hd/extComputation.h"

#include "pxr/base/tf/diagnosticMgr.h"
#include "pxr/base/tf/getenv.h"

#include "camera.h"
#include "config.h"
#include "renderPass.h"
#include "renderParam.h"
//#include "mesh.h"
//#include "instancer.h"
//#include "domeLight.h"
//#include "distantLight.h"
//#include "light.h"
//#include "material.h"
#include "renderBuffer.h"
//#include "basisCurves.h"
//#include "points.h"

#ifdef USE_VOLUME
#include "volume.h"
#include "field.h"
#endif

#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdio>

PXR_NAMESPACE_OPEN_SCOPE

static HdLavaApi* g_rprApi = nullptr;

class HdLavaDiagnosticMgrDelegate : public TfDiagnosticMgr::Delegate {
public:
    explicit HdLavaDiagnosticMgrDelegate(std::string const& logFile) : m_outputFile(nullptr) {
        if (logFile == "stderr") {
            m_output = stderr;
        } else if (logFile == "stdout") {
            m_output = stdout;
        } else {
            m_outputFile = fopen(logFile.c_str(), "a+");
            if (!m_outputFile) {
                TF_RUNTIME_ERROR("Failed to open error output file: \"%s\". Defaults to stderr\n", logFile.c_str());
                m_output = stderr;
            } else {
                m_output = m_outputFile;
            }
        }
    }
    ~HdLavaDiagnosticMgrDelegate() override {
        if (m_outputFile) {
            fclose(m_outputFile);
        }
    }

    void IssueError(TfError const &err) override {
        IssueDiagnosticBase(err);
    };
    void IssueFatalError(TfCallContext const &context, std::string const &msg) override {
        std::string message = TfStringPrintf(
            "[FATAL ERROR] %s -- in %s at line %zu of %s",
            msg.c_str(),
            context.GetFunction(),
            context.GetLine(),
            context.GetFile());
        IssueMessage(msg);
    };
    void IssueStatus(TfStatus const &status) override {
        IssueDiagnosticBase(status);
    };
    void IssueWarning(TfWarning const &warning) override {
        IssueDiagnosticBase(warning);
    };

private:
    void IssueDiagnosticBase(TfDiagnosticBase const& d) {
        std::string msg = TfStringPrintf(
            "%s -- %s in %s at line %zu of %s",
            d.GetCommentary().c_str(),
            TfDiagnosticMgr::GetCodeName(d.GetDiagnosticCode()).c_str(),
            d.GetContext().GetFunction(),
            d.GetContext().GetLine(),
            d.GetContext().GetFile());
        IssueMessage(msg);
    }

    void IssueMessage(std::string const& message) {
        std::time_t t = std::time(nullptr);
        std::stringstream ss;
        ss << "[" << std::put_time(std::gmtime(&t), "%T%z %F") << "] " << message;
        auto str = ss.str();

        fprintf(m_output, "%s\n", str.c_str());
    }

private:
    FILE* m_output;
    FILE* m_outputFile;
};

TF_DEFINE_PRIVATE_TOKENS(_tokens,
    (openvdbAsset) \
    (percentDone) \
    (renderMode) \
    (batch) \
    (progressive)
);

const TfTokenVector HdLavaDelegate::SUPPORTED_RPRIM_TYPES = {
    HdPrimTypeTokens->mesh,
    HdPrimTypeTokens->basisCurves,
    HdPrimTypeTokens->points,
#ifdef USE_VOLUME
    HdPrimTypeTokens->volume,
#endif
};

const TfTokenVector HdLavaDelegate::SUPPORTED_SPRIM_TYPES = {
    HdPrimTypeTokens->camera,
    HdPrimTypeTokens->material,
    HdPrimTypeTokens->rectLight,
    HdPrimTypeTokens->sphereLight,
    HdPrimTypeTokens->cylinderLight,
    HdPrimTypeTokens->domeLight,
    HdPrimTypeTokens->diskLight,
    HdPrimTypeTokens->distantLight,
    HdPrimTypeTokens->extComputation
};

const TfTokenVector HdLavaDelegate::SUPPORTED_BPRIM_TYPES = {
#ifdef USE_VOLUME
    _tokens->openvdbAsset,
#endif
    HdPrimTypeTokens->renderBuffer
};

HdLavaDelegate::HdLavaDelegate() {
    printf("HdLavaDelegate constructor\n");
    m_isBatch = GetRenderSetting(_tokens->renderMode) == _tokens->batch;
    m_isProgressive = GetRenderSetting(_tokens->progressive).GetWithDefault(true);

    m_rprApi.reset(new HdLavaApi(this));
    g_rprApi = m_rprApi.get();

    m_renderParam.reset(new HdLavaRenderParam(m_rprApi.get(), &m_renderThread));

    m_settingDescriptors = HdLavaConfig::GetRenderSettingDescriptors();
    _PopulateDefaultSettings(m_settingDescriptors);

    m_renderThread.SetRenderCallback([this]() {
        m_rprApi->Render(&m_renderThread);
    });
    m_renderThread.SetStopCallback([this]() {
        m_rprApi->AbortRender();
    });
    m_renderThread.StartThread();

    auto errorOutputFile = TfGetenv("HD_RPR_ERROR_OUTPUT_FILE");
    if (!errorOutputFile.empty()) {
        m_diagnosticMgrDelegate = DiagnostMgrDelegatePtr(
            new HdLavaDiagnosticMgrDelegate(errorOutputFile),
            [](HdLavaDiagnosticMgrDelegate* delegate) {
                TfDiagnosticMgr::GetInstance().RemoveDelegate(delegate);
                delete delegate;
            });
        TfDiagnosticMgr::GetInstance().AddDelegate(m_diagnosticMgrDelegate.get());
    }
}

HdLavaDelegate::~HdLavaDelegate() {
    printf("HdLavaDelegate destructor\n");
    g_rprApi = nullptr;
}

HdRenderParam* HdLavaDelegate::GetRenderParam() const {
    return m_renderParam.get();
}

void HdLavaDelegate::CommitResources(HdChangeTracker* tracker) {
    // CommitResources() is called after prim sync has finished, but before any
    // tasks (such as draw tasks) have run.

}

TfToken HdLavaDelegate::GetMaterialNetworkSelector() const {
    //return LavaUsdMaterialRegistry::GetInstance().GetMaterialNetworkSelector();
    return TfToken();
}

TfTokenVector const& HdLavaDelegate::GetSupportedRprimTypes() const {
    return SUPPORTED_RPRIM_TYPES;
}

TfTokenVector const& HdLavaDelegate::GetSupportedSprimTypes() const {
    return SUPPORTED_SPRIM_TYPES;
}

TfTokenVector const& HdLavaDelegate::GetSupportedBprimTypes() const {
    return SUPPORTED_BPRIM_TYPES;
}

HdResourceRegistrySharedPtr HdLavaDelegate::GetResourceRegistry() const {
    return HdResourceRegistrySharedPtr(new HdResourceRegistry());
}

HdRenderPassSharedPtr HdLavaDelegate::CreateRenderPass(HdRenderIndex* index, HdRprimCollection const& collection) {
    return HdRenderPassSharedPtr(new HdLavaRenderPass(index, collection, m_renderParam.get()));
}

HdInstancer* HdLavaDelegate::CreateInstancer(HdSceneDelegate* delegate, SdfPath const& id, SdfPath const& instancerId) {
    //return new HdLavaInstancer(delegate, id, instancerId);
    return nullptr;
}

void HdLavaDelegate::DestroyInstancer(HdInstancer* instancer) {
    if(instancer) {
        delete instancer;
    }
}

HdRprim* HdLavaDelegate::CreateRprim(TfToken const& typeId, SdfPath const& rprimId, SdfPath const& instancerId) {
    /*
    if (typeId == HdPrimTypeTokens->mesh) {
        return new HdLavaMesh(rprimId, instancerId);
    } else if (typeId == HdPrimTypeTokens->basisCurves) {
        return new HdLavaBasisCurves(rprimId, instancerId);
    } else if (typeId == HdPrimTypeTokens->points) {
        return new HdLavaPoints(rprimId, instancerId);
    }
#ifdef USE_VOLUME
    else if (typeId == HdPrimTypeTokens->volume) {
        return new HdLavaVolume(rprimId);
    }
#endif
    */
    
    //TF_CODING_ERROR("Unknown Rprim Type %s", typeId.GetText());
    return nullptr;
}

void HdLavaDelegate::DestroyRprim(HdRprim* rPrim) {
    if(rPrim) {
        delete rPrim;
    }
}

HdSprim* HdLavaDelegate::CreateSprim(TfToken const& typeId, SdfPath const& sprimId) {
    if (typeId == HdPrimTypeTokens->camera) {
        return new HdLavaCamera(sprimId);
    /*} else if (typeId == HdPrimTypeTokens->domeLight) {
        return new HdLavaDomeLight(sprimId);
    } else if (typeId == HdPrimTypeTokens->distantLight) {
        return new HdLavaDistantLight(sprimId);
    } else if (typeId == HdPrimTypeTokens->rectLight ||
        typeId == HdPrimTypeTokens->sphereLight ||
        typeId == HdPrimTypeTokens->cylinderLight ||
        typeId == HdPrimTypeTokens->diskLight) {
        return new HdLavaLight(sprimId, typeId);
    } else if (typeId == HdPrimTypeTokens->material) {
        return new HdLavaMaterial(sprimId);
    */} else if (typeId == HdPrimTypeTokens->extComputation) {
        return new HdExtComputation(sprimId);
    }

    //TF_CODING_ERROR("Unknown Sprim Type %s", typeId.GetText());
    return nullptr;
}

HdSprim* HdLavaDelegate::CreateFallbackSprim(TfToken const& typeId) {
    // For fallback sprims, create objects with an empty scene path.
    // They'll use default values and won't be updated by a scene delegate.
    if (typeId == HdPrimTypeTokens->camera) {
        return new HdLavaCamera(SdfPath::EmptyPath());
    /*} else if (typeId == HdPrimTypeTokens->domeLight) {
        return new HdLavaDomeLight(SdfPath::EmptyPath());
    } else if (typeId == HdPrimTypeTokens->rectLight ||
        typeId == HdPrimTypeTokens->sphereLight ||
        typeId == HdPrimTypeTokens->cylinderLight ||
        typeId == HdPrimTypeTokens->diskLight) {
        return new HdLavaLight(SdfPath::EmptyPath(), typeId);
    } else if (typeId == HdPrimTypeTokens->distantLight) {
        return new HdLavaDistantLight(SdfPath::EmptyPath());
    } else if (typeId == HdPrimTypeTokens->material) {
        return new HdLavaMaterial(SdfPath::EmptyPath());
    */} else if (typeId == HdPrimTypeTokens->extComputation) {
        return new HdExtComputation(SdfPath::EmptyPath());
    }

    //TF_CODING_ERROR("Unknown Sprim Type %s", typeId.GetText());
    return nullptr;
}

void HdLavaDelegate::DestroySprim(HdSprim* sPrim) {
    delete sPrim;
}

HdBprim* HdLavaDelegate::CreateBprim(TfToken const& typeId, SdfPath const& bprimId) {
    if (typeId == HdPrimTypeTokens->renderBuffer) {
        return new HdLavaRenderBuffer(bprimId);
    }
#ifdef USE_VOLUME
    else if (typeId == _tokens->openvdbAsset) {
        return new HdLavaField(bprimId);
    }
#endif

    TF_CODING_ERROR("Unknown Bprim Type %s", typeId.GetText());
    return nullptr;
}

HdBprim* HdLavaDelegate::CreateFallbackBprim(TfToken const& typeId) {
    return nullptr;
}

void HdLavaDelegate::DestroyBprim(HdBprim* bPrim) {
    delete bPrim;
}

HdAovDescriptor HdLavaDelegate::GetDefaultAovDescriptor(TfToken const& name) const {
    HdParsedAovToken aovId(name);
    if (name != HdAovTokens->color &&
        name != HdAovTokens->normal &&
        name != HdAovTokens->primId &&
        name != HdAovTokens->depth &&
        name != HdLavaUtilsGetCameraDepthName() &&
        !(aovId.isPrimvar && aovId.name == "st")) {
        // TODO: implement support for instanceId and elementId aov
        return HdAovDescriptor();
    }

    if (!m_rprApi->IsAovFormatConversionAvailable()) {
        if (name == HdAovTokens->primId) {
            // Integer images required, no way to support it
            return HdAovDescriptor();
        }
        // Only native RPR format can be used for AOVs when there is no support for AOV format conversion
        return HdAovDescriptor(HdFormatFloat32Vec4, false, VtValue(GfVec4f(0.0f)));
    }

    HdFormat format = HdFormatInvalid;

    float clearColorValue = 0.0f;
    if (name == HdAovTokens->depth ||
        name == HdLavaUtilsGetCameraDepthName()) {
        clearColorValue = name == HdLavaUtilsGetCameraDepthName() ? 0.0f : 1.0f;
        format = HdFormatFloat32;
    } else if (name == HdAovTokens->color) {
        format = HdFormatFloat32Vec4;
    } else if (name == HdAovTokens->primId) {
        format = HdFormatInt32;
    } else {
        format = HdFormatFloat32Vec3;
    }

    return HdAovDescriptor(format, false, VtValue(GfVec4f(clearColorValue)));
}

HdRenderSettingDescriptorList HdLavaDelegate::GetRenderSettingDescriptors() const {
    return m_settingDescriptors;
}

VtDictionary HdLavaDelegate::GetRenderStats() const {
    VtDictionary stats;
    int numCompletedSamples = m_rprApi->GetNumCompletedSamples();
    stats[HdPerfTokens->numCompletedSamples.GetString()] = numCompletedSamples;

    double percentDone = 0.0;
    {
        HdLavaConfig* config;
        auto configInstanceLock = HdLavaConfig::GetInstance(&config);
        percentDone = double(numCompletedSamples) / config->GetMaxSamples();
    }

    stats[_tokens->percentDone.GetString()] = 100.0 * percentDone;
    return stats;
}

bool HdLavaDelegate::IsPauseSupported() const {
    return true;
}

bool HdLavaDelegate::Pause() {
    m_renderThread.PauseRender();
    return true;
}

bool HdLavaDelegate::Resume() {
    m_renderThread.ResumeRender();
    return true;
}

#if PXR_VERSION >= 2005

bool HdLavaDelegate::IsStopSupported() const {
    return true;
}

bool HdLavaDelegate::Stop() {
    m_renderThread.StopRender();
    return true;
}

bool HdLavaDelegate::Restart() {
    m_renderParam->RestartRender();
    m_renderThread.StartRender();
    return true;
}

#endif // PXR_VERSION >= 2005

TfToken const& HdLavaUtilsGetCameraDepthName() {
#if PXR_VERSION < 2002
    return HdAovTokens->linearDepth;
#else
    return HdAovTokens->cameraDepth;
#endif
}

PXR_NAMESPACE_CLOSE_SCOPE

void SetHdLavaRenderDevice(int renderDevice) {
    PXR_INTERNAL_NS::HdLavaConfig* config;
    auto configInstanceLock = PXR_INTERNAL_NS::HdLavaConfig::GetInstance(&config);
    config->SetRenderDevice(renderDevice);
}

void SetHdLavaRenderQuality(int quality) {
    PXR_INTERNAL_NS::HdLavaConfig* config;
    auto configInstanceLock = PXR_INTERNAL_NS::HdLavaConfig::GetInstance(&config);
    config->SetRenderQuality(quality);
}

int GetHdLavaRenderQuality() {
    if (!PXR_INTERNAL_NS::g_rprApi) {
        return -1;
    }
    //return PXR_INTERNAL_NS::g_rprApi->GetCurrentRenderQuality();
    return 0;
}

int HdLavaExportRprSceneOnNextRender(const char* exportPath) {
    if (!PXR_INTERNAL_NS::g_rprApi) {
        return -1;
    }
    //PXR_INTERNAL_NS::g_rprApi->ExportRprSceneOnNextRender(exportPath);
    return 0;
}
