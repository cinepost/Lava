from falcor import *

def defaultRenderGraph(device):
    import falcor
    g = RenderGraph(device, "BSDFViewerGraph")

    loadRenderPassLibrary("DepthPass")
    loadRenderPassLibrary("SkyBox")
    loadRenderPassLibrary("GBuffer")

    SkyBox = RenderPass(device, "SkyBox", {'texName': '/home/max/env.exr', 'loadAsSrgb': True, 'filter': SamplerFilter.Linear})
    g.addPass(SkyBox, "SkyBox")

    DepthPass = RenderPass(device, "DepthPass")
    g.addPass(DepthPass, "DepthPass")

    GBufferRaster = RenderPass(device, "GBufferRaster", {
        'samplePattern': SamplePattern.Halton, 
        'sampleCount': 16, 
        'disableAlphaTest': False, 
        'forceCullMode': False, 
        'cull': CullMode.CullNone, 
        'useBentShadingNormals': True
    })
    g.addPass(GBufferRaster, "GBufferRaster")

    g.addEdge("GBufferRaster.depth", "SkyBox.depth")
    
    #g.markOutput("SkyBox.target")
    g.markOutput("GBufferRaster.diffuseOpacity")

    return g

rendering_device = renderer.getDevice()
render_graph = defaultRenderGraph(rendering_device)

try: renderer.addGraph(render_graph)
except NameError: None
