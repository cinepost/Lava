from __future__ import print_function
import collections

# Named tuple to define render pass
#
# varname and channel of planes with "percomp" set must end with _comp
RenderPass = collections.namedtuple("RenderPass", ["output", "lsdtype", "quantize", "percomp", "opts"])


# Define a dictionary of standard passes with the relevant parameters.
__renderpasses = {
    # pass name                    pass output name        type             quantize      percomp     opts
    "EdgeDetectPass":  RenderPass("output",                "vector4",       "float16",    False,      {'pfilter':['box']}),
    
    "OcclusionPass":   RenderPass("output",                "float",         "float16",    False,      {'pfilter':['box']}),
}

def getRenderPassesDict():
    return __renderpasses
