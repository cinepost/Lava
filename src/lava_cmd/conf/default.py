from falcor import *

def defaultRenderGraph(device):
    import falcor
    g = RenderGraph(device, "BSDFViewerGraph")

    loadRenderPassLibrary("BlitPass")
    loadRenderPassLibrary("AccumulatePass")
    loadRenderPassLibrary("DepthPass")
    loadRenderPassLibrary("GBuffer")
    loadRenderPassLibrary("SkyBox")
    loadRenderPassLibrary("CSM")
    loadRenderPassLibrary("ForwardLightingPass")

    AccumulatePass = RenderPass(device, "AccumulatePass", {
        'enableAccumulation': True,
        'precisionMode': AccumulatePrecision.Double
    })
    g.addPass(AccumulatePass, "AccumulatePass")
    
    g.addPass(RenderPass(device, "BlitPass"), "BlitPass")
    g.addPass(RenderPass(device, "ForwardLightingPass"), "LightingPass")
    
    GBufferRaster = RenderPass(device, "GBufferRaster", {
        'samplePattern': SamplePattern.Halton, 
        'sampleCount': 16, 
        'disableAlphaTest': True, 
        'forceCullMode': False, 
        'cull': CullMode.CullNone, 
        'useBentShadingNormals': True
    })
    g.addPass(GBufferRaster, "GBufferRaster")

    SkyBox = RenderPass(device, "SkyBox", {'texName': '/home/max/env.exr', 'loadAsSrgb': True, 'filter': SamplerFilter.Linear})
    g.addPass(SkyBox, "SkyBox")

    DepthPass = RenderPass(device, "DepthPass")
    g.addPass(DepthPass, "DepthPass")

    ShadowPass = RenderPass(device, "CSM", {
        'cascadeCount': 4, 
        'mapSize': uint2(2048, 2048),
        'visibilityBitCount': 16,
        'minDistance': 0.01,
        'maxDistance': 100.0
    })
    g.addPass(ShadowPass, "ShadowPass")

    g.addEdge("GBufferRaster.depth", "SkyBox.depth")
    g.addEdge("GBufferRaster.depth", "ShadowPass.depth")
    g.addEdge("GBufferRaster.depth", "LightingPass.depth")
    #g.addEdge("GBufferRaster.normW", "LightingPass.normals")

    #g.addEdge("ShadowPass.visibility", "LightingPass.visibilityBuffer")
    g.addEdge("SkyBox.target", "LightingPass.color")
    
    #g.addEdge("GBufferRaster.normW", "BlitPass.src")
    
    #g.markOutput("SkyBox.target")
    g.addEdge("LightingPass.color", "AccumulatePass.input")
    #g.markOutput("LightingPass.color")
    g.markOutput("AccumulatePass.output")
    #g.markOutput("GBufferRaster.diffuseOpacity")

    return g

rendering_device = renderer.getDevice()
render_graph = defaultRenderGraph(rendering_device)

try: renderer.addGraph(render_graph)
except NameError: None
