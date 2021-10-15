#ifndef HDLAVA_LAVA_API_H
#define HDLAVA_LAVA_API_H

// #include "pxr/imaging/hd/camera.h"
#include "pxr/imaging/hd/renderDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdLavaDelegate;
class HdLavaRenderThread;

class HdLavaApiImpl;

class HdLavaApi final {
public:
    HdLavaApi(HdLavaDelegate* delegate);
    ~HdLavaApi();

    void Render(HdLavaRenderThread* renderThread);
    void AbortRender();

private:
    HdLavaApiImpl* m_impl = nullptr;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDLAVA_LAVA_API_H
