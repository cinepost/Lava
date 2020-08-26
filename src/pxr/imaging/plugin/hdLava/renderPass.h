#ifndef HDLAVA_RENDER_PASS_H_
#define HDLAVA_RENDER_PASS_H_

#include <GL/glew.h>
#include <GL/gl.h>

#include "pxr/imaging/hd/renderPass.h"

#include "lavaApi.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdLavaRenderParam;

class HdLavaRenderPass final : public HdRenderPass {
 public:
    HdLavaRenderPass(HdRenderIndex* index, HdRprimCollection const& collection, HdLavaRenderParam* renderParam);

    ~HdLavaRenderPass() override = default;

    bool IsConverged() const override;

    void _Execute(HdRenderPassStateSharedPtr const& renderPassState,
                  TfTokenVector const& renderTags) override;

 private:
 	void DisplayRenderBuffer(uint width, uint height);

 private:
    HdLavaRenderParam* m_renderParam;

	// OpenGL framebuffer texture
    GLuint mFramebufferTexture = 0;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDLAVA_RENDER_PASS_H_
