#ifndef HDLAVA_RENDER_PARAM_H_
#define HDLAVA_RENDER_PARAM_H_

#include "renderThread.h"

#include "pxr/imaging/hd/renderDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdLavaApi;
class HdLavaVolume;

using HdLavaVolumeFieldSubscription = std::shared_ptr<HdLavaVolume>;
using HdLavaVolumeFieldSubscriptionHandle = std::weak_ptr<HdLavaVolume>;

class HdLavaRenderParam final : public HdRenderParam {
public:
    HdLavaRenderParam(HdLavaApi* rprApi, HdLavaRenderThread* renderThread) : mLavaApi(rprApi) , mRenderThread(renderThread) {}
    ~HdLavaRenderParam() override = default;

    HdLavaApi const* GetLavaApi() const { return mLavaApi; }
    HdLavaApi* AcquireLavaApiForEdit() {
        mRenderThread->StopRender();
        return mLavaApi;
    }

    HdLavaRenderThread* GetRenderThread() { return mRenderThread; }

    // Hydra does not mark HdVolume as changed if HdField used by it is changed
    // We implement this volume-to-field dependency by ourself until it's implemented in Hydra
    // More info: https://groups.google.com/forum/#!topic/usd-interest/pabUE0B_5X4
    
    //HdLavaVolumeFieldSubscription SubscribeVolumeForFieldUpdates(HdLavaVolume* volume, SdfPath const& fieldId);
    //void NotifyVolumesAboutFieldChange(HdSceneDelegate* sceneDelegate, SdfPath const& fieldId);

    void RestartRender() { mRestartRender.store(true); }
    bool IsRenderShouldBeRestarted() { return mRestartRender.exchange(false); }

private:
    HdLavaApi* mLavaApi;
    HdLavaRenderThread* mRenderThread;

    std::mutex mSubscribedVolumesMutex;
    std::map<SdfPath, std::vector<HdLavaVolumeFieldSubscriptionHandle>> mSubscribedVolumes;

    std::atomic<bool> mRestartRender;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDLAVA_RENDER_PARAM_H_
