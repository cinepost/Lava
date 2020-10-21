from falcor import *

def BSDFViewerGraph(device):
    import falcor
    g = RenderGraph(device, "BSDFViewerGraph")

    loadRenderPassLibrary("AccumulatePass")
    loadRenderPassLibrary("BSDFViewer")
    loadRenderPassLibrary("DepthPass")
    loadRenderPassLibrary("CSM")
    loadRenderPassLibrary("GBuffer")
    loadRenderPassLibrary("SSAO")
    loadRenderPassLibrary("SkyBox")

    #BSDFViewer = RenderPass(device, "BSDFViewer")
    #g.addPass(BSDFViewer, "BSDFViewer")
    
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
    #g.markOutput("GBufferRaster.normW")
    g.addEdge("DepthPass.depth", "SkyBox.depth")
    g.markOutput("SkyBox.target")
    
    #g.markOutput("SSAO.normals")

    return g

def defaultRenderGraph(device):
    g = RenderGraph(device, "DefaultRenderGraph")

    #loadRenderPassLibrary("DebugPasses.rpl")
    loadRenderPassLibrary("GBuffer.rpl")
    loadRenderPassLibrary("SkyBox.rpl")
    loadRenderPassLibrary("AccumulatePass.rpl")
    
    SkyBox = RenderPass(device, "SkyBox")
    g.addPass(SkyBox, "SkyBox")

    AccumulatePass = RenderPass(device, "AccumulatePass", {'enableAccumulation': True, 'precisionMode': AccumulatePrecision.Single})
    g.addPass(AccumulatePass, "AccumulatePass")
    
    GBufferRaster = RenderPass(device, "GBufferRaster", {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'disableAlphaTest': False, 'forceCullMode': False, 'cull': CullMode.CullBack, 'useBentShadingNormals': True})
    g.addPass(GBufferRaster, "GBufferRaster")
    
    #SplitScreenPass = RenderPass(device, "SplitScreenPass", {'splitLocation': 0.0, 'showTextLabels': False, 'leftLabel': 'Left side', 'rightLabel': 'Right side'})
    #g.addPass(SplitScreenPass, "SplitScreenPass")
    #g.addEdge("SplitScreenPass.output", "AccumulatePass.input")
    #g.addEdge("GBufferRaster.texC", "SplitScreenPass.leftInput")
    #g.addEdge("GBufferRaster.diffuseOpacity", "SplitScreenPass.rightInput")
    
    #g.markOutput("GBufferRaster.texC")
    
    #g.markOutput("AccumulatePass.output")
    g.markOutput("SkyBox.target")
    return g

device_manager = DeviceManager.instance()
default_device = device_manager.defaultRenderingDevice()

#render_graph = defaultRenderGraph(default_device)
render_graph = BSDFViewerGraph(default_device)
#render_graph = TestGraph(default_device)

try: renderer.addGraph(render_graph)
except NameError: None
