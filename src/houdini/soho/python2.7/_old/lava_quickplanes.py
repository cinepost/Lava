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
    # variable name
    #       channel name                type            quantize    percomp opts
    "P":
        QuickPlane(
            "",                         "vector3",      "float32",    False,  {'pfilter':['minmax omedian']}),
    "Pz":
        QuickPlane(
            "",                         "float",        "float32",    False,  {'pfilter':['minmax omedian']}),
    "N":
        QuickPlane(
            "",                         "vector3",      "float16",    False,  {'pfilter':['minmax omedian']}),


    "surface_albedo":
        QuickPlane(
            "albedo",                   "vector3",      "uint8",    False,  {}),

    "ambient_occlusion":
        QuickPlane(
            "ambocc",                   "float",        "uint8",    False,  {}),


    "Prim_Id":
        QuickPlane(
            "",                         "float",          "int16",    False,  {}),
    "Material_Id":
        QuickPlane(
            "",                         "float",          "int16",    False,  {}),
}

# Define a list of quickplanes for each lv_quickplane toggle parameter.
__toggleplanedict = {
    'lv_quickplane_P':                      ['P'],
    'lv_quickplane_Pz':                     ['Pz'],
    'lv_quickplane_N':                      ['N'],

    'lv_quickplane_albedo':                 ['surface_albedo'],
    'lv_quickplane_ambocc':                 ['ambient_occlusion'],
}

def getPlaneDict():
    return __quickplanes

def getTogglePlaneDict():
    return __toggleplanedict
