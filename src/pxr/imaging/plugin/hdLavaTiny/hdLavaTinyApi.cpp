#include "rprApi.h"

#include "renderDelegate.h"
// #include "renderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

using LockGuard = std::lock_guard<std::mutex>;

class HdLavaApiImpl {
public:
    HdLavaApiImpl(HdLavaDelegate* delegate)
        : m_delegate(delegate) {
        // Postpone initialization as further as possible to allow Hydra user to set custom render settings before creating a context
        //InitIfNeeded();
    }

    ~HdLavaApiImpl() {
    }

    void InitIfNeeded() {
        if (m_state != kStateUninitialized) {
            return;
        }

        static std::mutex s_lavaInitMutex;
        LockGuard lock(s_lavaInitMutex);

        if (m_state != kStateUninitialized) {
            return;
        }

        try {
            InitLava();
            InitScene();

            m_state = kStateRender;
        } catch (LavaUsdError& e) {
            TF_RUNTIME_ERROR("%s", e.what());
            m_state = kStateInvalid;
        }
    }

    bool EnableAborting() {
        m_isAbortingEnabled.store(true);

        // If abort was called when it was disabled, abort now
        if (m_abortRender) {
            AbortRender();
            return true;
        }
        return false;
    }

    void Render(HdLavaRenderThread* renderThread) {
        // RenderFrame(renderThread);
    }

    void AbortRender() {
        // if (!m_rprContext) {
        //     return;
        // }

        if (m_isAbortingEnabled) {
            // RPR_ERROR_CHECK(m_rprContext->AbortRender(), "Failed to abort render");
        } else {
            // In case aborting is disabled, we postpone abort until it's enabled
            m_abortRender.store(true);
        }
    }

private:
    void InitLava() {
        // m_rprContext = LavaContextPtr(LavaUsdCreateContext(&m_rprContextMetadata), LavaContextDeleter);
        // if (!m_rprContext) {
        //     RPR_THROW_ERROR_MSG("Failed to create RPR context");
        // }

        m_isAbortingEnabled.store(false);
    }

    void InitScene() {
        // m_scene.reset(m_rprContext->CreateScene(&status));
        // if (!m_scene) {
        //     RPR_ERROR_CHECK_THROW(status, "Failed to create scene", m_rprContext.get());
        // }

        // RPR_ERROR_CHECK_THROW(m_rprContext->SetScene(m_scene.get()), "Failed to set context scene");
    }

private:
    HdLavaDelegate* m_delegate;

    // std::unique_ptr<rpr::Scene> m_scene;

    std::atomic<bool> m_isAbortingEnabled;
    std::atomic<bool> m_abortRender;
};

HdLavaApi::HdLavaApi(HdLavaDelegate* delegate) : m_impl(new HdLavaApiImpl(delegate)) {

}

HdLavaApi::~HdLavaApi() {
    delete m_impl;
}

void HdLavaApi::Render(HdLavaRenderThread* renderThread) {
    m_impl->Render(renderThread);
}

void HdLavaApi::AbortRender() {
    m_impl->AbortRender();
}

PXR_NAMESPACE_CLOSE_SCOPE
