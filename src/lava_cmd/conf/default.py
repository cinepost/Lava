from falcor import *

def defaultRenderGraph(device):
    import falcor
    g = RenderGraph(device, "DefaultRenderGraph")

    loadRenderPassLibrary("BlitPass")
    loadRenderPassLibrary("AccumulatePass")
    loadRenderPassLibrary("DepthPass")
    loadRenderPassLibrary("GBuffer")
    loadRenderPassLibrary("SkyBox")
    #loadRenderPassLibrary("CSM")
    loadRenderPassLibrary("ForwardLightingPass")

    AccumulatePass = RenderPass(device, "AccumulatePass", {
        'enableAccumulation': True,
        'precisionMode': AccumulatePrecision.Single
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

    SkyBox = RenderPass(device, "SkyBox", {
        'texName': '/home/max/env.exr', 
        'loadAsSrgb': True, 
        'filter': SamplerFilter.Linear,
        'intensity': 0.0
    })
    g.addPass(SkyBox, "SkyBox")

    DepthPass = RenderPass(device, "DepthPass")
    g.addPass(DepthPass, "DepthPass")

    #ShadowPass = RenderPass(device, "CSM", {
    #    'cascadeCount': 4, 
    #    'mapSize': uint2(2048, 2048),
    #    'visibilityBitCount': 16,
    #    'minDistance': 0.01,
    #    'maxDistance': 100.0
    #})
    #g.addPass(ShadowPass, "ShadowPass")

    g.addEdge("GBufferRaster.depth", "SkyBox.depth")
    #g.addEdge("GBufferRaster.depth", "ShadowPass.depth")
    g.addEdge("GBufferRaster.depth", "LightingPass.depth")
    g.addEdge("GBufferRaster.normW", "LightingPass.normals")

    #g.addEdge("ShadowPass.visibility", "LightingPass.visibilityBuffer")
    g.addEdge("SkyBox.target", "LightingPass.color")
    
    #g.addEdge("GBufferRaster.normW", "BlitPass.src")
    
    #g.markOutput("SkyBox.target")
    #g.addEdge("GBufferRaster.diffuseOpacity", "AccumulatePass.input")
    g.addEdge("LightingPass.color", "AccumulatePass.input")
    #g.markOutput("LightingPass.color")
    g.markOutput("AccumulatePass.output")

    return g

def textureResolveDebugRenderGraph(device):
    import falcor
    g = RenderGraph(device, "TextureResolveDebugGraph")

    loadRenderPassLibrary("GBuffer")
    loadRenderPassLibrary("TexturesResolvePass")

    TexturesResolvePass = RenderPass(device, "TexturesResolve")
    g.addPass(TexturesResolvePass, "TexturesResolve")


    GBufferRaster = RenderPass(device, "GBufferRaster", {
        'samplePattern': SamplePattern.Halton, 
        'sampleCount': 16, 
        'disableAlphaTest': False, 
        'forceCullMode': False, 
        'cull': CullMode.CullNone, 
        'useBentShadingNormals': True
    })
    g.addPass(GBufferRaster, "GBufferRaster")
    
    g.addEdge("GBufferRaster.depth", "TexturesResolve.depth")

    g.markOutput("TexturesResolve.debugColor")

    return g

def textureLoadingDebugRenderGraph(device):
    import falcor
    g = RenderGraph(device, "TextureResolveDebugGraph")

    loadRenderPassLibrary("GBuffer")
    loadRenderPassLibrary("TexturesResolvePass")

    TexturesResolvePass = RenderPass(device, "TexturesResolve")
    g.addPass(TexturesResolvePass, "TexturesResolve")


    GBufferRaster = RenderPass(device, "GBufferRaster", {
        'samplePattern': SamplePattern.Halton, 
        'sampleCount': 16, 
        'disableAlphaTest': False, 
        'forceCullMode': False, 
        'cull': CullMode.CullNone, 
        'useBentShadingNormals': True
    })
    g.addPass(GBufferRaster, "GBufferRaster")
    
    g.addEdge("GBufferRaster.depth", "TexturesResolve.depth")

    g.markOutput("GBufferRaster.diffuseOpacity")

    return g

rendering_device = renderer.getDevice()
#render_graph = defaultRenderGraph(rendering_device)
#render_graph = textureResolveDebugRenderGraph(rendering_device)
render_graph = textureLoadingDebugRenderGraph(rendering_device)

try: renderer.addGraph(render_graph)
except NameError: None
