#include "renderParam.h"
//#include "volume.h"

#include "pxr/imaging/hd/sceneDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

/*
HdLavaVolumeFieldSubscription HdLavaRenderParam::SubscribeVolumeForFieldUpdates(
    HdLavaVolume* volume, SdfPath const& fieldId) {
    auto sub = HdLavaVolumeFieldSubscription(volume, [](HdLavaVolume* volume) {});
    {
        std::lock_guard<std::mutex> lock(mSubscribedVolumesMutex);
        mSubscribedVolumes[fieldId].push_back(std::move(sub));
    }
    return sub;
}

void HdLavaRenderParam::NotifyVolumesAboutFieldChange(HdSceneDelegate* sceneDelegate, SdfPath const& fieldId) {
    std::lock_guard<std::mutex> lock(mSubscribedVolumesMutex);
    for (auto subscriptionsIt = mSubscribedVolumes.begin();
         subscriptionsIt != mSubscribedVolumes.end();) {
        auto& subscriptions = subscriptionsIt->second;
        for (size_t i = 0; i < subscriptions.size(); ++i) {
            if (auto volume = subscriptions[i].lock()) {
                // Force HdVolume Sync
                sceneDelegate->GetRenderIndex().GetChangeTracker().MarkRprimDirty(volume->GetId(), HdChangeTracker::DirtyTopology);

                // Possible Optimization: notify volume about exact changed field
                // Does not make sense right now because Hydra removes and creates
                // from scratch all HdFields whenever one of them is changed (e.g added/removed/edited primvar)
                // (USD 20.02)
            } else {
                std::swap(subscriptions[i], subscriptions.back());
                subscriptions.pop_back();
            }
        }
        if (subscriptions.empty()) {
            subscriptionsIt = mSubscribedVolumes.erase(subscriptionsIt);
        } else {
            ++subscriptionsIt;
        }
    }
}
*/

PXR_NAMESPACE_CLOSE_SCOPE