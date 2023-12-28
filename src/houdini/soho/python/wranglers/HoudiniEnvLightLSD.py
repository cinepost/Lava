from __future__ import print_function
#
# PROPRIETARY INFORMATION.  This software is proprietary to
# Side Effects Software Inc., and is not to be reproduced,
# transmitted, or disclosed in any way without written permission.
#
# Produced by:
#       Side Effects Software Inc
#       123 Front Street West, Suite 1401
#       Toronto, Ontario
#       Canada   M5J 2M2
#       416-504-9876
#
# NAME: HoudiniEnvLightIFD.py ( Python )
#
# COMMENTS:     Wrangler for the HoudiniEnvLight object.
#
#               When adding support for a new renderer, if you want
#               users to be able to use standard Houdini Light
#               objects, you should add a wrangler for your renderer.
#

# Here, we define methods for each of the properties on IFD light
# sources.

import math
import soho
from soho import SohoParm

import SOHOcommon

def isphoton():
    return soho.getOutputDriver().getData("photonrender")

def isgilight(obj, now):
    mode = obj.getDefaultedString('env_mode', now, ['direct'])[0]
    return mode == 'occlusion' and not isphoton()

def isportallight(obj, now):
    mode = obj.getDefaultedString('env_mode', now, ['direct'])[0]
    return mode == 'direct' and obj.getDefaultedInt('env_portalenable', now, [0])[0]

def areashape(obj, now, value):
    if isgilight(obj, now):
        return False
    if isportallight(obj, now):
        value[0] = 'geo'
    else:
        value[0] = 'env'
    return True
    
def areafullsphere(obj, now, value):
    if isgilight(obj, now):
        return False
    value[0] = 1
    if obj.evalInt('env_clipy', now, value):
        value[0] = not value[0]
    return True

def eval_envmap(obj, now, value):
    if obj.evalInt('skymap_enable', now, value):
        if value[0]:
            if obj.evalString('env_skymap', now, value):
                
                # Special handling of cop maps for environment lights
                shader_string = 'envmap "%s"' % value[0]
                shader = soho.processShader(shader_string, False, False)
                
                if len(shader[1]):
                    print(shader[1])
                
                return True
    return obj.evalString('env_map', now, value)

def areamap(obj, now, value):
    if isgilight(obj, now):
        return False
    return eval_envmap(obj, now, value)

def nondiffuse(obj, now, value):
    if obj.evalInt('light_contribdiff', now, value):
        if not value[0]:
            value[0] = 1
            return True
    return False

def nonspecular(obj, now, value):
    if obj.evalInt('light_contribspec', now, value):
        if not value[0]:
            value[0] = 1
            return True
    return False

def visible_primary(obj, now, value):
    if obj.evalInt('light_contribprimary', now, value):
        if value[0]:
            value[0] = 1
            return True
    return False

def envintensity(obj, now, value):
    plist = obj.evaluate(lshaderParms, now)
    value[:] = get_color(plist)
    return True 

# Properties in the HoudiniLight object which get mapped to the
# surface shader.
lshaderParms = {
    'env_mode': SohoParm('env_mode', 'string', ['direct'], False),
    'env_doadaptive': SohoParm('env_doadaptive', 'int', [0], False),
    'env_domaxdist': SohoParm('env_domaxdist', 'int', [0], False),
    'env_maxdist': SohoParm('env_maxdist', 'real', [10], False),
    'env_angle': SohoParm('env_angle', 'real', [90], False),

    'light_color': SohoParm('light_color', 'real', [1,1,1], False),
    'light_intensity' : SohoParm('light_intensity',     'real', [1], False),
    'light_exposure' : SohoParm('light_exposure',     'real', [0], False),

    'env_null': SohoParm('env_null', 'string', [''], False),
    'env_clipy': SohoParm('env_clipy', 'int', [0], False),

    'shadowmask': SohoParm('shadowmask', 'string', ['*'], False),
    'shadow_intensity': SohoParm('shadow_intensity', 'real', [1], False),
    'shadow_transparent': SohoParm('shadow_transparent', 'int', [1], False),
}

def get_color(plist):
    color = plist['light_color'].Value
    if len(color) == 1:
        color.append(color[0])
    if len(color) == 2:
        color.append(color[1])

    intensity = plist['light_intensity'].Value[0]
    exposure = plist['light_exposure'].Value[0]
    brightness = intensity * pow(2, exposure)
    color[0] *= brightness
    color[1] *= brightness
    color[2] *= brightness
    return color

def get_lava_light_intensity(obj, now):
    plist       = obj.evaluate(lshaderParms, now)
    
    intensity = plist['light_intensity'].Value[0]
    exposure = plist['light_exposure'].Value[0]
    
    return intensity * pow(2, exposure)


def light_direct_diffuse_color_multiplier(obj, now, value):
    contribution = [1]
    if obj.evalInt('lv_contribute_direct_diffuse', now, contribution) and (contribution[0] == 0):
        value[0] = (.0, .0, .0)
        return True

    if obj.evalFloat('lv_light_direct_diffuse_color_multiplier', now, value):
        return True

    value = (1.0, 1.0, 1.0)
    return False

def light_direct_specular_color_multiplier(obj, now, value):
    contribution = [1]
    if obj.evalInt('lv_contribute_direct_specular', now, contribution) and (contribution[0] == 0):
        value[0] = (.0, .0, .0)
        return True

    if obj.evalFloat('lv_light_direct_specular_color_multiplier', now, value):
        return True

    value = (1.0, 1.0, 1.0)
    return False

def light_indirect_diffuse_color_multiplier(obj, now, value):
    contribution = [1]
    if obj.evalInt('lv_contribute_indirect_diffuse', now, contribution) and (contribution[0] == 0):
        value[0] = (.0, .0, .0)
        return True

    if obj.evalFloat('lv_light_indirect_diffuse_color_multiplier', now, value):
        return True

    value = (1.0, 1.0, 1.0)
    return True

def light_indirect_specular_color_multiplier(obj, now, value):
    contribution = [1]
    if obj.evalInt('lv_contribute_indirect_specular', now, contribution) and (contribution[0] == 0):
        value[0] = (.0, .0, .0)
        return True

    if obj.evalFloat('lv_light_indirect_specular_color_multiplier', now, value):
        return True

    value = (1.0, 1.0, 1.0)
    return True

def backgroundString(obj, now, value):
    plist = obj.evaluate(lshaderParms, now)

    light_color = get_color(plist)

    if eval_envmap(obj, now, value) and value[0] != '':
        shader = ' envmap "%s"' % value[0]
        shader += ' envtint %g %g %g' % \
                ( light_color[0], light_color[1], light_color[2] )
        shader += ' envnull "%s"' % obj.getName()
    else:
        shader = ' background %g %g %g' % \
                ( light_color[0], light_color[1], light_color[2] )

    value[:] = [shader]
    return True

def light_shader(obj, now, value):
    if obj.evalShader('shop_lightpath', now, value):
        if value[0]:
            return True

    plist = obj.evaluate(lshaderParms, now)

    if isgilight(obj, now):
        # Set the light color to 1 1 1 - this is just a global scale, and
        # we'll adjust it with the background or environment tint.
        shader = 'opdef:/Shop/v_gilight light_color 1 1 1'
        if plist['shadow_transparent'].Value[0]:
            shader += ' istyle "opacity"'
        else:
            shader += ' istyle "occlusion"'
        shader += ' doraysamples 1'
        shader += ' doadaptive %g' % plist['env_doadaptive'].Value[0]
        shader += ' domaxdist %d' % plist['env_domaxdist'].Value[0]
        shader += ' maxdist %g' % plist['env_maxdist'].Value[0]
        shader += ' cone_angle %g' % plist['env_angle'].Value[0]
        shader += ' objmask "%s"' % plist['shadowmask'].Value[0]

        if backgroundString(obj, now, value):
            shader += value[0]
    else:
        eval_envmap(obj, now, value)
        env_map = value[0]

        light_color = get_color(plist)
        env_clipy = plist['env_clipy'].Value[0]

        shader = 'opdef:/Shop/v_asadlight lightcolor %g %g %g doatten 2' % \
                ( light_color[0], light_color[1], light_color[2] )

        if env_map:
            shader += ' envmap "%s"' % env_map
        if env_clipy:
            shader += ' envclipy 1'

    uvrender = obj.getDefaultedInt('vm_isuvrendering', now, [0])[0]
    light_path = obj.getDefaultedString('vm_uvlightpaths', now, ['-diffuse & -volume'])[0]

    force_path = light_path if uvrender else None
    shader += SOHOcommon.getLightContribString(obj, now, value, force_path)

    value[:] = [shader]
    return True


# Properties in the HoudiniLight object which get mapped to the
# shadow shader.
lshadowParms = {
    'shadow_type'       : SohoParm('shadow_type','string', ['off'], False),
    'shadow_intensity'  : SohoParm('shadow_intensity', 'real', [1], False),
    'shadow_transparent': SohoParm('shadow_transparent', 'int', [1], False),
}

def shadow_shader(obj, now, value):
    if obj.evalShader('shop_shadowpath', now, value):
        if value[0]:
            return True
    
    if isgilight(obj, now):
        return False

    plist = obj.evaluate(lshadowParms, now)

    stype = plist['shadow_type'].Value[0]
    intensity = plist['shadow_intensity'].Value[0]
    transparent = plist['shadow_transparent'].Value[0]

    if stype == 'off':
        return False

    shader = 'opdef:/Shop/v_rayshadow'
    if transparent:
        shader += ' shadowtype filter'
    else:
        shader += ' shadowtype fast'
    shader += ' shadowI %g' % intensity
    value[:] = [shader]
    return True

def surface_shader(obj, now, value):
    if obj.evalShader('shop_surfacepath', now, value):
        if value[0]:
            return True

    plist = obj.evaluate(lshaderParms, now)

    eval_envmap(obj, now, value)
    env_map = value[0]

    light_color = get_color(plist)
    env_clipy = plist['env_clipy'].Value[0]

    shader = 'opdef:/Shop/v_arealight alpha 1 lightcolor %g %g %g' % \
            ( light_color[0], light_color[1], light_color[2] )

    if env_map:
        shader += ' envmap "%s"' % env_map
    if env_clipy:
        shader += ' envclipy 1'

    value[:] = [shader]
    return True

def illum_shader(obj, now, value):
    if obj.evalShader('vm_illumshader', now, value):
        if value[0]:
            return True

    if isgilight(obj, now):
        shader = 'diffuselighting glossytodiffuse 0'
    else:
        defmis = -1 # object-centric sampling
        if isportallight(obj, now) or (eval_envmap(obj, now, value) and value[0] != ''):
            defmis = 0

        illumParms = {
            'vm_misbias' : SohoParm('vm_misbias','real', [defmis], False),
            'env_filteramount' : SohoParm('env_filteramount','real', [0], False),
        }

        plist = obj.evaluate(illumParms, now)
        misbias = plist['vm_misbias'].Value[0]
        filteramount = plist['env_filteramount'].Value[0]
        shader = 'mislighting misbias %f filteramount %f' % \
                (misbias, filteramount)

    value[:] = [shader]
    return True

def raybackground(obj, now, value):
    mode = obj.getDefaultedString('env_mode', now, ['direct'])[0]
    value[0] = mode == 'background' and not isphoton()
    return True

# When evaluating an integer or real property, we want to 
parmMap = {
    'lv_light_type'                 :    areashape,
    'lv_areafullsphere'             :    areafullsphere,
    'lv_areamap'                    :    areamap,
    'lv_nondiffuse'                 :    nondiffuse,
    'lv_nonspecular'                :    nonspecular,
    'lv_areashape'                  :    areashape,
    'lv_visible_primary'            :    visible_primary,
    'lv_envintensity'               :    envintensity,

    'lv_direct_diffuse_color_multiplier'       :    light_direct_diffuse_color_multiplier,
    'lv_direct_specular_color_multiplier'      :    light_direct_specular_color_multiplier,
    'lv_indirect_diffuse_color_multiplier'     :    light_indirect_diffuse_color_multiplier,
    'lv_indirect_specular_color_multiplier'    :    light_indirect_specular_color_multiplier,

    'shop_lightpath'    :       light_shader,
    'shop_shadowpath'   :       shadow_shader,
#    'shop_surfacepath'  :       surface_shader,

#    'lv_illumshader'    :       illum_shader,

    # All other properties can be added by the user as spare
    # properties if they want.
}

class envlightIFD:
    def __init__(self, obj, now, version):
        self.Label = 'Houdini Env Light LSD'
        self.Version = version

    def evalParm(self, obj, parm, now):
        key = parm.Houdini      # Which houdini parameter is being evaluated?
        if key in parmMap:
            return parmMap[key](obj, now, parm.Value)
        return obj.evalParm(parm, now)

def registerLight(list):
    key = 'HoudiniEnvLight-lava'
    if key not in list:
        list[key] = envlightIFD
