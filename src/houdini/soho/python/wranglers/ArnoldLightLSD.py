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
# NAME: HoudiniLight_IFD.py ( Python )
#
# COMMENTS:     Wrangler for the HoudiniLight object.
#
#               When adding support for a new renderer, if you want
#               users to be able to use standard Houdini Light
#               objects, you should add a wrangler for your renderer.
#

# Here, we define methods for each of the properties on IFD light
# sources.

import soho
from soho import SohoParm

import hou
import shopclerks
from IFDmisc import isPreviewMode

import SOHOcommon

def isarealighttype(ltype):
    return ltype in ['line', 'grid', 'disk', 'sphere', 'geo', 'sun', 'tube']

def isdistantlighttype(ltype):
    return ltype in ['distant', 'sun']

def isarealight(obj, now):
    ltype = obj.getDefaultedString('ar_light_type', now, ['point'])[0]
    return isarealighttype(ltype)

def isdistantlight(obj, now):
    ltype = obj.getDefaultedString('ar_light_type', now, ['point'])[0]
    return isdistantlighttype(ltype)

def isgeolight(obj, now):
    ltype = obj.getDefaultedString('ar_light_type', now, ['point'])[0]
    return ltype == 'geo'

def ispclight(obj, now):
    enable = obj.getDefaultedInt('pc_enable', now, [0])[0]
    return enable

def areashape(obj, now, value):
    obj.evalString('ar_light_type', now, value)
    if not isarealighttype(value[0]):
        return False
    return True

def areasize(obj, now, value):
    if not isarealight(obj, now):
        return False
    if not obj.evalFloat('vm_areasize', now, value):
        if not obj.evalFloat('areasize', now, value):
            return False
    if len(value) == 1:
        value.append(value[0])
    return True

def areafullsphere(obj, now, value):
    if not isarealight(obj, now):
        return False
    return obj.evalInt('areafullsphere', now, value)

def areamap(obj, now, value):
    if not isarealight(obj, now):
        return False
    return obj.evalString('ar_light_color_texture', now, value)

def phantom(obj, now, value):
    if soho.getOutputDriver().getData('pcrender'):
        value[:] = [0]
        return True
    if obj.evalInt('light_contribprimary', now, value):
        if value[0]:
            value[0] = 0
            return True
    return False

def activeradius(obj, now, value):
    if obj.evalInt('activeradiusenable', now, value):
        if value[0]:
            radius = obj.evalFloat('activeradius', now, value)
            return True
    return False

def projection(obj, now, value):
    if isdistantlight(obj, now):
        value[:] = ['ortho']
    else:
        value[:] = ['perspective']
    return True

def shadowmap_res(obj, now, value):
    if not obj.evalInt('res', now, value):
        value[:] = [512,512]
    return True

def focal(obj, now, value):
    if isdistantlight(obj, now):
        return False
    return obj.evalFloat('focal', now, value)

def aperture(obj, now, value):
    if isdistantlight(obj, now):
        return False
    return obj.evalFloat('aperture', now, value)

def orthowidth(obj, now, value):
    if not isdistantlight(obj, now):
        return False
    return obj.evalFloat('orthowidth', now, value)

attenParms = {
    'atten_start': SohoParm('atten_start',      'real', [1], False),
    'atten_type': SohoParm('atten_type',        'string', ['half'], False),
    'atten_dist': SohoParm('atten_dist',        'real', [1e6], False),
    'attenrampenable': SohoParm('attenrampenable', 'int', [0], False),
    'atten_rampstart' : SohoParm('atten_rampstart', 'real', [0], False),
    'atten_rampend' : SohoParm('atten_rampend', 'real', [100], False),
}

def attenString(obj, now, ltype):
    plist = obj.evaluate(attenParms, now)

    atten_start = plist['atten_start'].Value[0]
    atten_type  = plist['atten_type'].Value[0]
    atten_dist  = plist['atten_dist'].Value[0]
    attenrampenable = plist['attenrampenable'].Value[0]
    rampstart   = plist['atten_rampstart'].Value[0]
    rampend     = plist['atten_rampend'].Value[0]

    atten = ' attenstart %g' % atten_start

    if atten_type == 'none':
        atten += ' doatten 0'
    elif atten_type == 'half':
        atten += ' doatten 1 atten %g' % atten_dist
    elif atten_type == 'physical':
        atten += ' doatten 2'

    if attenrampenable:
        parmlist = [ SohoParm('object:instancename', 'string', ['']) ]
        obj.evaluate(parmlist, 0)
        alight = hou.node(parmlist[0].Value[0])
        if alight:
            attenramp = hlight.parmTuple('attenramp')
            if attenramp:
                rampParms = shopclerks.ifdclerk.IfdParmEval(None, soho.Precision, None).getRampParms(attenramp, hou.timeToFrame(now))
                atten += ' doattenramp 1'
                atten += ' rampstart %g rampend %g' % (rampstart, rampend)
                for rp in rampParms:
                    atten += ' ' + rp[0]
                    atten += ' ' + rp[1]
    return atten

def envString(plist):
    areamap     = plist['ar_light_color_texture'].Value[0]

    env = ''
    if areamap != '':
        env += ' envmap "%s"' % areamap

    return env

def get_color(plist):
    color = plist['ar_color'].Value
    if len(color) == 1:
        color.append(color[0])
    if len(color) == 2:
        color.append(color[1])

    intensity = plist['ar_intensity'].Value[0]
    exposure = plist['ar_exposure'].Value[0]
    brightness = intensity * pow(2, exposure)
    color[0] *= brightness
    color[1] *= brightness
    color[2] *= brightness
    return color

# Properties in the HoudiniLight object which get mapped to the
# light shader.
lshaderParms = {
    'ar_light_type': SohoParm('ar_light_type',        'string', ['point'], False),
    'ar_color': SohoParm('ar_color',      'real', [1,1,1], False),
    'ar_intensity' : SohoParm('ar_intensity',     'real', [1], False),
    'ar_exposure' : SohoParm('ar_exposure',       'real', [0], False),
    'light_enable' : SohoParm('light_enable',   'int', [1], False),
#    'coneenable': SohoParm('coneenable',        'int', [0], False),
    'ar_cone_angle' : SohoParm('ar_cone_angle',         'real', [45], False),
    'ar_penumbra_delta' : SohoParm('ar_penumbra_delta',         'real', [10], False),
#    'coneroll'  : SohoParm('coneroll',          'real', [1], False),
    'ar_light_color_texture'   : SohoParm('ar_light_color_texture',           'string', [''], False),
#    'areamapspace': SohoParm('areamapspace',    'string', ['space:object'], False),
#    'areamapnull': SohoParm('areamapnull',      'string', [''], False),
#    'areamapblur': SohoParm('areamapblur',      'real', [0], False),
#    'areamapscale': SohoParm('areamapscale',    'real', [1], False),
#    'light_texture' : SohoParm('light_texture', 'string', [''], False),
#    'singlesided' : SohoParm('singlesided',     'int', [0], False),
#    'reverse'   : SohoParm('reverse',           'int', [0], False),
#    'normalizearea': SohoParm('normalizearea',  'int', [1], False),
#    'projmap'   : SohoParm('projmap',           'string', [''], False),
#    'edgeenable': SohoParm('edgeenable',        'int', [0], False),
#    'edgewidth' : SohoParm('edgewidth',         'real', [0.1], False),
#    'edgerolloff': SohoParm('edgerolloff',      'real', [1], False),
#    'sharpspot' : SohoParm('sharpspot',         'int', [0], False),
}

def light_shader(obj, now, value):
    if obj.evalShader('shop_lightpath', now, value):
        if value[0]:
            return True

    plist = obj.evaluate(lshaderParms, now)
    ltype = plist['ar_light_type'].Value[0]
    coneangle   = plist['ar_cone_angle'].Value[0]
    conedelta   = plist['ar_penumbra_delta'].Value[0]
    projmap     = False#plist['projmap'].Value[0]

    uvrender = obj.getDefaultedInt('vm_isuvrendering', now, [0])[0]
    light_path = obj.getDefaultedString('vm_uvlightpaths', now, ['-diffuse & -volume'])[0]
    light_color = get_color(plist)

    if ltype == 'ambient':
        shader = 'opdef:/Shop/v_ambient lightcolor %g %g %g' % \
                ( light_color[0], light_color[1], light_color[2] )
        value[:] = [shader]
        return True

    shader = 'opdef:/Shop/v_asadlight lightcolor %g %g %g' % \
                ( light_color[0], light_color[1], light_color[2] )

    shader += ' type %s' % ltype

    if ltype == 'spot':
        shader += ' coneangle %g conedelta %g' % \
                    ( coneangle, conedelta )

    shader += attenString(obj, now, ltype)
    shader += envString(plist)

    if projmap:
        shader += ' slide "%s"' % projmap.replace('"', '\\"')

    add_path = light_path if uvrender else None
    shader += SOHOcommon.getLightContribString(obj, now, value, add_path)

    value[:] = [shader]
    return True

lshadowParms = {
    'shadow_type'           : SohoParm('shadow_type','string', ['off'], False),
    'shadow_bias'           : SohoParm('shadow_bias','real', [.05], False),
    'shadow_intensity'      : SohoParm('shadow_intensity', 'real', [1], False),
    'shadow_color'          : SohoParm('shadow_color', 'real', [0,0,0], False),
    'shadow_quality'        : SohoParm('shadow_quality', 'float', [1], False),
    'shadow_softness'       : SohoParm('shadow_softness', 'real', [1], False),
    'shadow_blur'           : SohoParm('shadow_blur', 'real', [0], False),
    'shadow_transparent'    : SohoParm('shadow_transparent', 'int', [1], False),
    'shadowmap_file'        : SohoParm('shadowmap_file', 'string', [''], False),
    'shadowmap_time_zero'   : SohoParm('shadowmap_time_zero', 'int', [1], False),
    'vm_iprraytraceshadows' : SohoParm('vm_iprraytraceshadows', 'int', [1], False),
}

def shadow_shader(obj, now, value):
    return True

def surface_shader(obj, now, value):
    return True

samplerParms = {
    'pc_file'           : SohoParm('pc_file', 'string', '', False),
    'pc_samples'        : SohoParm('pc_samples', 'int', [0], False),
    'selfshadow'        : SohoParm('selfshadow', 'int', [1], False),
    'vm_misbias'        : SohoParm('vm_misbias', 'float', [0], False),
}

def sampler_shader(obj, now, value):
    return True 

def tracer_shader(obj, now, value):
    return True 

def illum_shader(obj, now, value):
    return True


# Shadow map generation
def render_shadowmap(obj, now, value):
    stype = obj.getDefaultedString('shadow_type', now, ['off'])[0]
    if stype == 'off' or stype == 'raytrace':
        value[:] = [0]
        return True
    return obj.evalInt('render_shadowmap', now, value)

def render_pointcloud(obj, now, value):
    if not ispclight(obj, now):
        value[:] = [0]
        return True
    return obj.evalInt('render_pointcloud', now, value)

def vm_picture(obj, now, value):
    if ispclight(obj, now):
        value[:] = ['null:']
        return True

    stype = obj.getDefaultedString('shadow_type', now, ['off'])[0]
    if stype == 'off' or stype == 'raytrace':
        return False
    transparent = obj.getDefaultedInt('shadow_transparent', now, [0])[0]
    if transparent:
        image = 'null:'
    else:
        image = obj.getDefaultedString('shadowmap_file', now, [''])[0]
    value[:] = [image]
    return True

# The Deep Shadow Map parameters
def isDSM(obj, now):
    stype = obj.getDefaultedString('shadow_type', now, ['off'])[0]
    if stype == 'off' or stype == 'raytrace':
        return False
    transparent = obj.getDefaultedInt('shadow_transparent', now, [0])[0]
    if not transparent:
        return False
    return True

def shadowmap_samples(obj, now, value):
    if not isDSM(obj, now):
        value[:] = [1,1]
    elif not obj.evalInt('shadowmap_samples', now, value):
        value[:] = [1,1]
    return True

def vm_dsmfilename(obj, now, value):
    if not isDSM(obj, now):
        return False
    filename = obj.getDefaultedString('shadowmap_file', now, [''])[0]
    value[:] = [filename]
    return True

def vm_deepresolver(obj, now, value):
    filename = []
    if not vm_dsmfilename(obj, now, filename):
        return False
    if len(filename) == 0 or not filename[0]:
        return False
    value[:] = ['shadow']
    return True

def vm_renderengine(obj, now, value):
    if ispclight(obj, now):
        value[0] = 'pbrmicropoly'
        return True
    return False

pbrshaderParms = {
    'pc_file'   : SohoParm('pc_file', 'string', '', False),
}

def vm_pbrshader(obj, now, value):
    if ispclight(obj, now):
        plist = obj.evaluate(pbrshaderParms, now)
        value[0] = 'opdef:/Shop/v_pcwriter pcfile %s' % plist['pc_file'].Value[0]
        return True
    return False

def vm_hidden(obj, now, value):
    if ispclight(obj, now):
        value[0] = 0
        return True
    return False

def vm_setexrdatawindow(obj, now, value):
    stype = obj.getDefaultedString('shadow_type', now, ['off'])[0]
    if stype == 'off' or stype == 'raytrace':
        return False
    value[0] = 0
    return True

def lv_light_type(obj, now, value):
    value[0] = obj.getDefaultedString('ar_light_type', now, ['point'])[0]
    return True

def pre_outputLight(obj):
    return False

# When evaluating an integer or real property, we want to
parmMap = {
    'lv_light_type'     :       lv_light_type,
    'lv_areasize'       :       areasize,
    'lv_areafullsphere' :       areafullsphere,
    'lv_areamap'        :       areamap,
    'lv_phantom'        :       phantom,
    'lv_activeradius'   :       activeradius,
    'res'               :       shadowmap_res,
    'lv_samples'        :       shadowmap_samples,
    'focal'             :       focal,
    'aperture'          :       aperture,
    'orthowidth'        :       orthowidth,
    'lv_areashape'      :       areashape,
    'projection'        :       projection,
    'render_shadowmap'  :       render_shadowmap,
    'render_pointcloud' :       render_pointcloud,
    'lv_picture'        :       vm_picture,
    'lv_deepresolver'   :       vm_deepresolver,
    'lv_dsmfilename'    :       vm_dsmfilename,
    'lv_setexrdatawindow' :     vm_setexrdatawindow,

    # Settings for point cloud lights
    #'lv_renderengine'   :       vm_renderengine,
    #'lv_pbrshader'      :       vm_pbrshader,
    #'lv_hidden'         :       vm_hidden,

    # Shaders
    'shop_lightpath'    :        light_shader,
    #'shop_shadowpath'   :       shadow_shader,
    #'shop_surfacepath'  :       surface_shader,
    #'vm_samplershader'  :       sampler_shader,
    #'vm_tracershader'   :       tracer_shader,
    #'vm_illumshader'    :       illum_shader,

    # All other properties can be added by the user as spare
    # properties if they want.
}

class alightLSD:
    def __init__(self, obj, now, version):
        self.Label = 'Arnold Light LSD'
        self.Version = version

    def evalParm(self, obj, parm, now):
        key = parm.Houdini      # Which houdini parameter is being evaluated?
        if key in parmMap:
            return parmMap[key](obj, now, parm.Value)
        return obj.evalParm(parm, now)

def registerLight(list):
    key = 'ArnoldLight-lava'
    if key not in list:
        list[key] = alightLSD
