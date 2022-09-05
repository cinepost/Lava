#include "renderPass.h"
#include "renderDelegate.h"
#include "config.h"
#include "lavaApi.h"
#include "renderBuffer.h"
#include "renderParam.h"

#include "pxr/imaging/hd/renderPassState.h"
#include "pxr/imaging/hd/renderIndex.h"

PXR_NAMESPACE_OPEN_SCOPE

HdLavaRenderPass::HdLavaRenderPass(HdRenderIndex* index, HdRprimCollection const& collection, HdLavaRenderParam* renderParam): HdRenderPass(index, collection), m_renderParam(renderParam) {
    printf("HdLavaRenderPass constructor\n");

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    // create OpenGL frame buffer texture
    glGenTextures(1, &mFramebufferTexture);
    glBindTexture(GL_TEXTURE_2D, mFramebufferTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void HdLavaRenderPass::_Execute(HdRenderPassStateSharedPtr const& renderPassState, TfTokenVector const& renderTags) {
    // To avoid potential deadlock:
    //   main thread locks config instance and requests render stop and
    //   in the same time render thread trying to lock config instance to update its settings.
    // We should release config instance lock before requesting render thread to stop.
    // It could be solved in another way by using shared_mutex and
    // marking current write-lock as read-only after successful config->Sync
    // in such a way main and render threads would have read-only-locks that could coexist
    bool stopRender = false;
    {
        HdLavaConfig* config;
        auto configInstanceLock = HdLavaConfig::GetInstance(&config);
        config->Sync(GetRenderIndex()->GetRenderDelegate());
        if (config->IsDirty(HdLavaConfig::DirtyAll)) {
            stopRender = true;
        }
    }
    if (stopRender) {
        m_renderParam->GetRenderThread()->StopRender();
    }

    auto rprApiConst = m_renderParam->GetLavaApi();

    auto& vp = renderPassState->GetViewport();
    GfVec2i newViewportSize(static_cast<int>(vp[2]), static_cast<int>(vp[3]));
    auto oldViewportSize = rprApiConst->GetViewportSize();
    bool frameBufferDirty = false;
    if (oldViewportSize != newViewportSize) {
        frameBufferDirty = true;
        m_renderParam->AcquireLavaApiForEdit()->SetViewportSize(newViewportSize);
    }

    //if (rprApiConst->GetAovBindings() != renderPassState->GetAovBindings()) {
    //    m_renderParam->AcquireLavaApiForEdit()->SetAovBindings(renderPassState->GetAovBindings());
    //}

    //if (rprApiConst->GetCamera() != renderPassState->GetCamera()) {
    //    m_renderParam->AcquireLavaApiForEdit()->SetCamera(renderPassState->GetCamera());
    //}

    //if (rprApiConst->IsChanged() ||
    //    m_renderParam->IsRenderShouldBeRestarted()) {
    //    for (auto& aovBinding : renderPassState->GetAovBindings()) {
    //        if (aovBinding.renderBuffer) {
    //            auto rprRenderBuffer = static_cast<HdLavaRenderBuffer*>(aovBinding.renderBuffer);
    //            rprRenderBuffer->SetConverged(false);
    //        }
    //    }
    //    m_renderParam->GetRenderThread()->StartRender();
    //}

    uint width = vp[2];
    uint height = vp[3];

    if (frameBufferDirty) {
        // reset OpenGL viewport and orthographic projection
        glViewport(0, 0, width, height);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, width, 0.0, height, -1.0, 1.0);
    }

    DisplayRenderBuffer(width, height);
}

void HdLavaRenderPass::DisplayRenderBuffer(uint width, uint height) {
    // set initial OpenGL state
    //glEnable(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    //glBindTexture(GL_TEXTURE_2D, mFramebufferTexture);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

    // clear current OpenGL color buffer
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // render textured quad with Lava frame buffer contents
    glBegin(GL_QUADS);

    glColor3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.f, 0.f);
    glVertex2f(0.f, 0.f);

    glColor3f(1.0f, 1.0f, 0.0f);
    glTexCoord2f(0.f, 1.f);
    glVertex2f(0.f, height);

    glColor3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(1.f, 1.f);
    glVertex2f(width, height);

    glColor3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(1.f, 0.f);
    glVertex2f(width, 0.f);

    glEnd();
}

bool HdLavaRenderPass::IsConverged() const {
    //for (auto& aovBinding : m_renderParam->GetLavaApi()->GetAovBindings()) {
    //    if (aovBinding.renderBuffer && !aovBinding.renderBuffer->IsConverged()) {
    //        return false;
    //    }
    //}
    //return true;
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE