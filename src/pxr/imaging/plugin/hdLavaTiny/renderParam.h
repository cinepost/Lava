#ifndef HDLAVA_RENDER_PARAM_H
#define HDLAVA_RENDER_PARAM_H

#include "renderThread.h"

#include "pxr/imaging/hd/renderDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdLavaApi;

class HdLavaRenderParam final : public HdRenderParam {
public:
    HdLavaRenderParam(HdLavaApi* lavaApi, HdLavaRenderThread* renderThread)
        : m_lavaApi(lavaApi)
        , m_renderThread(renderThread) {}
    ~HdLavaRenderParam() override = default;

    HdLavaApi const* GetLavaApi() const { return m_lavaApi; }
    HdLavaApi* AcquireLavaApiForEdit() {
        m_renderThread->StopRender();
        return m_lavaApi;
    }

    HdLavaRenderThread* GetRenderThread() { return m_renderThread; }

    void RestartRender() { m_restartRender.store(true); }
    bool IsRenderShouldBeRestarted() { return m_restartRender.exchange(false); }

private:
    HdLavaApi* m_lavaApi;
    HdLavaRenderThread* m_renderThread;

    std::mutex m_subscribedVolumesMutex;

    std::atomic<bool> m_restartRender;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDLAVA_RENDER_PARAM_H
