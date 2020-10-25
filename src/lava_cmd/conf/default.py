from falcor import *

def defaultRenderGraph(device):
    import falcor
    g = RenderGraph(device, "BSDFViewerGraph")

    loadRenderPassLibrary("BlitPass")
    loadRenderPassLibrary("AccumulatePass")
    loadRenderPassLibrary("BSDFViewer")
    loadRenderPassLibrary("DepthPass")
    loadRenderPassLibrary("CSM")
    loadRenderPassLibrary("GBuffer")
    loadRenderPassLibrary("SSAO")
    loadRenderPassLibrary("SkyBox")

    #BSDFViewer = RenderPass(device, "BSDFViewer")
    #g.addPass(BSDFViewer, "BSDFViewer")

    g.addPass(RenderPass(device, "BlitPass"), "BlitPass")
    
    GBufferRaster = RenderPass(device, "GBufferRaster", {
        'samplePattern': SamplePattern.Center, 
        'sampleCount': 16, 
        'disableAlphaTest': False, 
        'forceCullMode': False, 
        'cull': CullMode.CullNone, #CullMode.CullBack, 
        'useBentShadingNormals': True
    })

    SkyBox = RenderPass(device, "SkyBox", {'texName': '/home/max/env.exr', 'loadAsSrgb': True, 'filter': SamplerFilter.Linear})
    g.addPass(SkyBox, "SkyBox")

    DepthPass = RenderPass(device, "DepthPass")
    g.addPass(DepthPass, "DepthPass")

    #SSAO = RenderPass(device, "SSAO")

    g.addPass(GBufferRaster, "GBufferRaster")
    #g.addPass(SSAO, "SSAO")

    #AccumulatePass = RenderPass(device, "AccumulatePass", {'enableAccumulation': True, 'precisionMode': AccumulatePrecision.Double})
    #g.addPass(AccumulatePass, "AccumulatePass")
    
    #g.addEdge("BSDFViewer.output", "AccumulatePass.input")
    #g.markOutput("AccumulatePass.output")
    #g.markOutput("BSDFViewer.output")
    g.addEdge("DepthPass.depth", "SkyBox.depth")
    
    g.addEdge("GBufferRaster.normW", "BlitPass.src")
    
    g.markOutput("BlitPass.dst")
    #g.markOutput("SkyBox.target")
    #g.markOutput("SSAO.normals")

    return g

device_manager = DeviceManager.instance()
default_device = device_manager.defaultRenderingDevice()

render_graph = defaultRenderGraph(default_device)

try: renderer.addGraph(render_graph)
except NameError: None

"""
def render_graph_forward_renderer():
    loadRenderPassLibrary("Antialiasing")
    loadRenderPassLibrary("BlitPass")
    loadRenderPassLibrary("CSM")
    loadRenderPassLibrary("DepthPass")
    loadRenderPassLibrary("ForwardLightingPass")
    loadRenderPassLibrary("SSAO")
    loadRenderPassLibrary("ToneMapper")

    skyBox = RenderPass("SkyBox")

    forward_renderer = RenderGraph("ForwardRenderer")
    forward_renderer.addPass(RenderPass("DepthPass"), "DepthPrePass")
    forward_renderer.addPass(RenderPass("ForwardLightingPass"), "LightingPass")
    forward_renderer.addPass(RenderPass("CSM"), "ShadowPass")
    forward_renderer.addPass(RenderPass("BlitPass"), "BlitPass")
    forward_renderer.addPass(RenderPass("ToneMapper", {'autoExposure': True}), "ToneMapping")
    forward_renderer.addPass(RenderPass("SSAO"), "SSAO")
    forward_renderer.addPass(RenderPass("FXAA"), "FXAA")

    forward_renderer.addPass(skyBox, "SkyBox")

    forward_renderer.addEdge("DepthPrePass.depth", "SkyBox.depth")
    forward_renderer.addEdge("SkyBox.target", "LightingPass.color")
    forward_renderer.addEdge("DepthPrePass.depth", "ShadowPass.depth")
    forward_renderer.addEdge("DepthPrePass.depth", "LightingPass.depth")
    forward_renderer.addEdge("ShadowPass.visibility", "LightingPass.visibilityBuffer")
    forward_renderer.addEdge("LightingPass.color", "ToneMapping.src")
    forward_renderer.addEdge("ToneMapping.dst", "SSAO.colorIn")
    forward_renderer.addEdge("LightingPass.normals", "SSAO.normals")
    forward_renderer.addEdge("LightingPass.depth", "SSAO.depth")
    forward_renderer.addEdge("SSAO.colorOut", "FXAA.src")
    forward_renderer.addEdge("FXAA.dst", "BlitPass.src")

    forward_renderer.markOutput("BlitPass.dst")

    return forward_renderer

"""