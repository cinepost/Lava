from __future__ import print_function

import math
import soho
from soho import SohoParm

import hou
import shopclerks
from IFDmisc import isPreviewMode, headerParms

import SOHOcommon


def lv_picture(obj, now, value):
    if isPreviewMode():
        value[0] = obj.getDefaultedString('vm_picture', now, ['ip'])[0]
    else:
        value[0] = obj.getDefaultedString('lv_picture', now, ['ip'])[0]
    return True


def lv_image_mplay_sockethost(obj, now, value):
    value[0] = obj.getDefaultedString('vm_image_mplay_sockethost', now, ['localhost'])[0]
    return True

def lv_image_mplay_socketport(obj, now, value):
    value[0] = obj.getDefaultedInt('vm_image_mplay_socketport', now, [0])[0]
    return True    

def lv_main_output_source(obj, now, value):
    value[0] = obj.getDefaultedString('lv_main_output_source', now, [''])[0]
    return True

def lv_background_enabled(obj, now, value):
    value[0] = obj.getDefaultedInt('vm_bgenable', now, [''])[0]
    return True

def lv_background_image(obj, now, value):
    enabled = [0]
    lv_background_enabled(obj, now, enabled)
    if enabled[0] == 1:
        value[0] = obj.getDefaultedString('vm_background', now, [''])[0]
    else:
        value[0] = ""
        return False
        
    return True

def lv_background_color(obj, now, value):
    if not obj.evalFloat('lv_bgcolor', now, value):
        value[0] = [0.0, 0.0, 0.0, 0.0]

    return True

def lv_image_mplay_label(obj, now, value):
    value[0] = obj.getDefaultedString('lv_image_mplay_label', now, [''])[0]
    
    #rop = soho.getOutputDriver()
    #plist = rop.evaluate(headerParms, now)
    #hipname = plist.get('hipname', None)
    #if hipname:
    #    value[0] = hipname.Value[0]
    #    return True    

    return False


# When evaluating an integer or real property, we want to
parmMap = {
    'lv_picture'                :   lv_picture,
    'lv_image_mplay_sockethost' :   lv_image_mplay_sockethost,
    'lv_image_mplay_socketport' :   lv_image_mplay_socketport,
    'lv_image_mplay_label'      :   lv_image_mplay_label,
    'lv_main_output_source'     :   lv_main_output_source,
    'lv_background_enabled'     :   lv_background_enabled,
    'lv_background_image'       :   lv_background_image,
    'lv_background_color'       :   lv_background_color,
}

class hcameraLSD:
    def __init__(self, obj, now, version):
        self.Label = 'Houdini Camera LSD'
        self.Version = version

    def evalParm(self, obj, parm, now):
        if isinstance(parm, SohoParm):
            key = parm.Houdini      # Which houdini parameter is being evaluated?
            if key in parmMap:
                return parmMap[key](obj, now, parm.Value)
            return obj.evalParm(parm, now)
        else:
            return parm

def registerCamera(list):
    key = 'HoudiniCamera-lava'
    if key not in list:
        list[key] = hcameraLSD
