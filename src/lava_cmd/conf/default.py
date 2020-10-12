from falcor import *

def BSDFViewerGraph(device):
    g = RenderGraph(device, "BSDFViewerGraph")

    #loadRenderPassLibrary("AccumulatePass")
    loadRenderPassLibrary("BSDFViewer")

    BSDFViewer = RenderPass(device, "BSDFViewer")
    g.addPass(BSDFViewer, "BSDFViewer")
    
    #AccumulatePass = RenderPass(device, "AccumulatePass", {'enableAccumulation': True, 'precisionMode': AccumulatePrecision.Double})
    #g.addPass(AccumulatePass, "AccumulatePass")
    
    #g.addEdge("BSDFViewer.output", "AccumulatePass.input")
    #g.markOutput("AccumulatePass.output")
    g.markOutput("BSDFViewer.output")
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
#try: m.addGraph(render_graph)
try: renderer.addGraph(render_graph)
except NameError: None
