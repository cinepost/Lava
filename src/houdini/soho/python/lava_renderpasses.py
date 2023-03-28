from __future__ import print_function
import collections
import copy

from soho import SohoParm


# Named tuple to define render pass
#
# varname and channel of planes with "percomp" set must end with _comp
RenderPass = collections.namedtuple("RenderPass", ["output", "lsdtype", "quantize", "percomp", "opts", "parms_map", "plane_parms_map"])

edgeDetectPassParms = {
    'traceDepth'                : SohoParm('lv_edgedetect_pass_trace_depth',             'bool',      [1], skipdefault=False),
    'traceNormal'               : SohoParm('lv_edgedetect_pass_trace_normal',            'bool',      [0], skipdefault=False),
    'traceMaterialID'           : SohoParm('lv_edgedetect_pass_trace_material_id',       'bool',      [0], skipdefault=False),
    'traceInstanceID'           : SohoParm('lv_edgedetect_pass_trace_instance_id',       'bool',      [0], skipdefault=False),
    'ignoreAlpha'               : SohoParm('lv_edgedetect_pass_ignore_alpha',            'bool',      [1], skipdefault=False),
    'depthDistanceRange'        : SohoParm('lv_edgedetect_pass_depth_distance_range',    'float',     [5.0, 10.0], skipdefault=False),
    'normalThresholdRange'      : SohoParm('lv_edgedetect_pass_normal_threshold_range',  'float',     [0.5, 1.0], skipdefault=False),

    'lowPassFilterSize'         : SohoParm('lv_edgedetect_pass_lowpass_filter_size',     'int',       [0], skipdefault=False),

    'depthKernelSize'           : SohoParm('lv_edgedetect_pass_depth_kernel_size',       'int',       [0], skipdefault=False),
    'normalKernelSize'          : SohoParm('lv_edgedetect_pass_normal_kernel_size',      'int',       [0], skipdefault=False),
    'materialKernelSize'        : SohoParm('lv_edgedetect_pass_material_kernel_size',    'int',       [0], skipdefault=False),
    'instanceKernelSize'        : SohoParm('lv_edgedetect_pass_instance_kernel_size',    'int',       [0], skipdefault=False),
}

ambienOcclusionPassParms = {
    'shadingRate'               : SohoParm('lv_ambocc_pass_shading_rate',                'int',       [1], skipdefault=False),
    'distanceRange'             : SohoParm('lv_ambocc_pass_distance_range',              'float',     [1.0, 2.0], skipdefault=False),
}


# cryptomatteMaterialPassParms = {
#     'outputMode'                : SohoParm('lv_crypto_material_pass_output_mode',        'int',       [0], skipdefault=False),
#     'outputPreview'             : SohoParm('lv_crypto_material_pass_output_preview',     'bool',      [1], skipdefault=False),
#     'rank'                      : SohoParm('lv_crypto_material_pass_cryptomatte_rank',   'int',       [3], skipdefault=False),
# }

# cryptomatteInstancePassParms = {
#     'outputMode'                : SohoParm('lv_crypto_instance_pass_output_mode',        'int',       [1], skipdefault=False),
#     'outputPreview'             : SohoParm('lv_crypto_instance_pass_output_preview',     'bool',      [1], skipdefault=False),
#     'rank'                      : SohoParm('lv_crypto_instance_pass_cryptomatte_rank',   'int',       [3], skipdefault=False),
# }

# cryptoMaterialPlaneParms = {
#     'sourcepass'          : SohoParm('lv_crypto_material_source_pass',          'string',      ['CryptomattePass'], skipdefault=False),
#     'outputname_override' : SohoParm('lv_crypto_material_pass_output_name',     'string',      [''], skipdefault=False)
# }

# cryptoInstancePlaneParms = {
#     'sourcepass'          : SohoParm('lv_crypto_instance_source_pass',          'string',      ['CryptomattePass'], skipdefault=False),
#     'outputname_override' : SohoParm('lv_crypto_instance_pass_output_name',     'string',      [''], skipdefault=False)
# }

# Define a dictionary of standard passes with the relevant parameters.
__renderpasses = {
    # pass name                             pass resource name        type        quantize     percomp   opts                    render pass parms              output plane parms    
    "EdgeDetectPass"          : RenderPass("output",                 "vector4",  "float16",    False,   {'pfilter':['box']},     edgeDetectPassParms,           None),
    
    "AmbientOcclusionPass"    : RenderPass("output",                 "float",    "float16",    False,   {'pfilter':['box']},     ambienOcclusionPassParms,      None),

    #"CryptomatteMaterialPass" : RenderPass("preview_color",          "vector4",  "float16",    False,   {},                      cryptomatteMaterialPassParms,  cryptoMaterialPlaneParms ),

    #"CryptomatteInstancePass" : RenderPass("preview_color",          "vector4",  "float16",    False,   {},                      cryptomatteInstancePassParms,  cryptoInstancePlaneParms ),
}


# Define a list of renderpasses.
__togglerenderpassdict = {
    # render passes as output planes
    'lv_plane_edgedetect':                  ['EdgeDetectPass'],
    'lv_plane_ambientocclusion':            ['AmbientOcclusionPass'],
    #'lv_plane_cryptomaterial':              ['CryptomatteMaterialPass'],
    #'lv_plane_cryptoinstance':              ['CryptomatteInstancePass'],
}

def getRenderPassDict():
    import lava_cryptomattepasses
    renderpasses = copy.deepcopy(__renderpasses)
    renderpasses.update(lava_cryptomattepasses.getRenderPassDict())
    return renderpasses

def getToggleRenderPassDict():
    import lava_cryptomattepasses
    togglerenderpasses = copy.deepcopy(__togglerenderpassdict)
    togglerenderpasses.update(lava_cryptomattepasses.getToggleRenderPassDict())
    return togglerenderpasses