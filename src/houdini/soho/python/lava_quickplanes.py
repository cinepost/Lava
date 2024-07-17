from __future__ import print_function
import collections

# Named tuple to define an image plane
#
# varname and channel of planes with "percomp" set must end with _comp
QuickPlane = collections.namedtuple("QuickPlane", ["channel", "lsdtype", "quantize", "percomp", "opts"])


# Define a dictionary of standard planes with the relevant parameters.
__quickplanes = {
    # channel name                  pass builtin name           lsdtype          quantize     percomp     opts
    "position":         QuickPlane("POSITION",                 "vector3",       "float32",    False,      {'pfilter':['closest']}),
    
    "depth":            QuickPlane("DEPTH",                    "float",         "float32",    False,      {'pfilter':['min']}),
    
    "normals":          QuickPlane("NORMAL",                   "vector3",       "float16",    False,      {'pfilter':['closest']}),

    "face_normals":     QuickPlane("FACE_NORMAL",              "vector3",       "float16",    False,      {'pfilter':['closest']}),

    "tangent_normals":  QuickPlane("TANGENT_NORMAL",           "vector3",       "float16",    False,      {'pfilter':['closest']}),

    "albedo":           QuickPlane("ALBEDO",                   "vector3",       "float16",    False,      {}),

    "emission":         QuickPlane("EMISSION",                 "vector3",       "float16",    False,      {}),

    "roughness":        QuickPlane("ROUGHNESS",                "float",         "float16",    False,      {}),

    "shadow":           QuickPlane("SHADOW",                   "vector3",       "float16",    False,      {}),

    "fresnel":          QuickPlane("FRESNEL",                  "float",         "float16",    False,      {}),

    "object_id":        QuickPlane("OBJECT_ID",                "int",           "uint16",     False,      {}),

    "material_id":      QuickPlane("MATERIAL_ID",              "int",           "uint16",     False,      {}),

    "instance_id":      QuickPlane("INSTANCE_ID",              "int",           "uint32",     False,      {}),

    # ipr/diagnostics

    "op_id":            QuickPlane("Op_Id",                    "float",         "float32",    False,      {}),
    "prim_id":          QuickPlane("Prim_Id",                  "float",         "float32",    False,      {}),
    "variance":         QuickPlane("VARIANCE",                 "float",         "float16",    False,      {}),
    "meshlet_color":    QuickPlane("MESHLET_COLOR",            "vector3",       "float16",    False,      {}),
    "micropoly_color":  QuickPlane("MICROPOLY_COLOR",          "vector3",       "float16",    False,      {}),
    "uv":               QuickPlane("UV",                       "vector3",       "float16",    False,      {}),
    "texGrads":         QuickPlane("TEXGRADS",                 "vector4",       "float16",    False,      {}),
    "meshlet_draw":     QuickPlane("MESHLET_DRAW_HEATMAP",     "vector4",       "float16",    False,      {}),
    "aux":              QuickPlane("AUX",                      "vector4",       "float32",    False,      {}),
}

# Define a list of quickplanes for each lv_quickplane toggle parameter.
__toggleplanedict = {
    'lv_quickplane_p':                      ['position'],
    'lv_quickplane_z':                      ['depth'],
    'lv_quickplane_n':                      ['normals'],
    'lv_quickplane_ng':                     ['face_normals'],
    'lv_quickplane_nt':                     ['tangent_normals'],
    'lv_quickplane_albedo':                 ['albedo'],
    'lv_quickplane_emission':               ['emission'],
    'lv_quickplane_roughness':              ['roughness'],
    'lv_quickplane_shadow':                 ['shadow'],
    'lv_quickplane_fresnel':                ['fresnel'],
    'lv_quickplane_object_id':              ['object_id'],
    'lv_quickplane_material_id':            ['material_id'],
    'lv_quickplane_instance_id':            ['instance_id'],

    # ipr/diagnistics
    'lv_quickplane_op_id':                  ['op_id'],
    'lv_quickplane_prim_id':                ['prim_id'],
    'lv_quickplane_variance':               ['variance'],
    'lv_quickplane_meshlet_color':          ['meshlet_color'],
    'lv_quickplane_micropoly_color':        ['micropoly_color'],
    'lv_quickplane_uv':                     ['uv'],
    'lv_quickplane_texgrads':               ['texGrads'],
    'lv_quickplane_meshlet_draw':           ['meshlet_draw'],
    'lv_quickplane_aux':                    ['aux'],
}

def getPlaneDict():
    return __quickplanes

def getTogglePlaneDict():
    return __toggleplanedict
