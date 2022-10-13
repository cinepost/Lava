from __future__ import print_function
import collections

# Named tuple to define an image plane
#
# varname and channel of planes with "percomp" set must end with _comp
QuickPlane = collections.namedtuple(
    "QuickPlane",
    ["channel", "lsdtype", "quantize", "percomp", "opts"])

# Define a dictionary of standard planes with the relevant parameters.
__quickplanes = {
    # channel name              pass builtin name       type            quantize      percomp     opts
    "position":      QuickPlane("POSITION",            "vector3",      "float32",    False,      {'pfilter':['minmax omedian']}),
    
    "depth":         QuickPlane("DEPTH",               "float",        "float32",    False,      {'pfilter':['minmax omedian']}),
    
    "normals":       QuickPlane("NORMAL",              "vector3",      "float16",    False,      {'pfilter':['minmax omedian']}),

    "albedo":        QuickPlane("ALBEDO",              "vector3",      "uint8",      False,      {}),

    "shadow":        QuickPlane("SHADOW",              "float",        "float16",    False,      {}),

    "occlusion":     QuickPlane("OCCLUSION",           "float",        "float16",    False,      {}),

    "object_id":     QuickPlane("OBJECT_ID",           "int",          "uint16",     False,      {}),

    "material_id":   QuickPlane("MATERIAL_ID",         "int",          "uint16",     False,      {}),

    "instance_id":   QuickPlane("INSTANCE_ID",         "int",          "uint32",     False,      {}),
}

# Define a list of quickplanes for each lv_quickplane toggle parameter.
__toggleplanedict = {
    'lv_quickplane_p':                      ['position'],
    'lv_quickplane_z':                      ['depth'],
    'lv_quickplane_n':                      ['normals'],
    'lv_quickplane_albedo':                 ['albedo'],
    'lv_quickplane_shadow':                 ['shadow'],
    'lv_quickplane_occlusion':              ['occlusion'],
    'lv_quickplane_object_id':              ['object_id'],
    'lv_quickplane_material_id':            ['material_id'],
    'lv_quickplane_instance_id':            ['instance_id'],
}

def getPlaneDict():
    return __quickplanes

def getTogglePlaneDict():
    return __toggleplanedict
