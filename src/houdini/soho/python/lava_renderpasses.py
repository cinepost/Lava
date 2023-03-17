from __future__ import print_function
import collections

from soho import SohoParm


# Named tuple to define render pass
#
# varname and channel of planes with "percomp" set must end with _comp
RenderPass = collections.namedtuple("RenderPass", ["output", "lsdtype", "quantize", "percomp", "opts", "parms_map"])

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

# Define a dictionary of standard passes with the relevant parameters.
__renderpasses = {
    # pass name                             pass output name         type             quantize     percomp       opts                   parms
    "EdgeDetectPass":           RenderPass("output",                "vector4",       "float16",    False,      {'pfilter':['box']},     edgeDetectPassParms),
    
    "AmbientOcclusionPass":     RenderPass("output",                "float",         "float16",    False,      {'pfilter':['box']},     ambienOcclusionPassParms),
}

def getRenderPassesDict():
    return __renderpasses
