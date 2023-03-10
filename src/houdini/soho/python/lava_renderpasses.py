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
    'depthDistanceRange'        : SohoParm('lv_edgedetect_pass_depth_distance_range',    'float',     [3.0, 5.0], skipdefault=False),
    'normalThresholdRange'      : SohoParm('lv_edgedetect_pass_normal_thresold_range',   'float',     [3.0, 4.0], skipdefault=False),
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
