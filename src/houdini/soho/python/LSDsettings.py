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
# NAME:         LSDsettings.py ( Python )
#
# COMMENTS:     Load up settings dependent on what render target was
#               specified.
#
#               The LSD keyword for the parameter is stored in the
#               Key field of the SohoParm.
#

import hou, soho
import sys, os
import hutil.json
from soho import SohoParm
from LSDapi import *
import LSDmisc
import LSDhooks
import LSDlava
from future.utils import iteritems


class Settings:
    def __init__(self):
        self.Global = []
        self.Object = []
        self.Light = []
        self.Fog = []
        self.Geometry = []

        self.DeepResolver = {}
        self.Measure = {}
        self.Bokeh = {}

        self.MPlane = []        # Main image plane
        self.IPlane = []        # Auxilliary image plane
        self.IOption = []       # Image format options

        self.GenerateOpId = False
        self.ShadowMap = False
        self.GenerateMaterialname = True
        self.MatteOverrides = {}        # Objects forced to be matte
        self.PhantomOverrides = {}      # Objects forced to be phantom

        self.MissingWranglers = {}
        self.SavedMaterials = {}         # Material shops already written out
        self.SavedBundles = {}           # Bundles already written out
        self.UVHiddenObjects = {}
        self.ParsedStyleSheets = {}

        self.Features = {}

_Settings = Settings()
SettingDefs = []

# Order versions in descending order
theVersion = 'lava%d.%d' % (hou.applicationVersion()[0], hou.applicationVersion()[1])

_UserAttributes = {
    'lv_username'       :SohoParm('lv_username%d', 'string', [''], False,
                                    key='lv_username'),
    'lv_usertype'       :SohoParm('lv_usertype%d', 'string', [''], False,
                                    key='lv_usertype'),
    'lv_userint'        :SohoParm('lv_userint%d', 'real', [0], False,
                                    key='lv_userint'),
    'lv_userscalar'     :SohoParm('lv_userscalar%d', 'real', [0], False,
                                    key='lv_userscalar'),
    'lv_user3tuple'     :SohoParm('lv_user3tuple%d', 'real', [0,0,0], False,
                                    key='lv_user3tuple'),
    'lv_user4tuple'     :SohoParm('lv_user4tuple%d', 'real', [0,0,0,0], False,
                                    key='lv_user4tuple'),
    'lv_user9tuple'     :SohoParm('lv_user9tuple%d', 'real',
                                    [1,0,0, 0,1,0, 0,0,1], False,
                                    key='lv_user9tuple'),
    'lv_user16tuple'    :SohoParm('lv_user16tuple%d', 'real',
                                    [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1], False,
                                    key='lv_user16tuple'),
    'lv_userstring'     :SohoParm('lv_userstring%d', 'string', [''], False,
                                    key='lv_userstring'),
}

def _outputUserAttribute(i, obj, now):
    for s in _UserAttributes:
        _UserAttributes[s].setIndex(i)
    plist = obj.evaluate(_UserAttributes, now)
    token = plist['lv_username'].Value[0]
    if not token:
        return
    type = plist['lv_usertype'].Value[0]
    # Types (aliases) expected:
    #   real            (float)
    #   bool
    #   int             (integer)
    #   vector2
    #   vector3         (vector)
    #   vector4
    #   matrix3
    #   matrix4         (matrix)
    #   string
    if not type:
        return
    if type == 'string':
        value = plist['lv_userstring'].Value
    elif type == 'int':
        value = plist['lv_userint'].Value
    elif type == 'float':
        value = plist['lv_userscalar'].Value
    elif type == 'vector':
        value = plist['lv_user3tuple'].Value
    elif type == 'vector4':
        value = plist['lv_user4tuple'].Value
    elif type == 'matrix3':
        value = plist['lv_user9tuple'].Value
    elif type == 'matrix':
        value = plist['lv_user16tuple'].Value
    else:
        return
    cmd_declare('object', type, token, value)

def clearLists():
    global      _Settings
    _Settings = Settings()

def getLightWrangler(obj, now, style):
    wrangler = getWrangler(obj, now, style)
    if wrangler:
        return wrangler

    if style != 'light_wrangler':
        return None
            
    wrangler = soho.LightWranglers.get('ArnoldLight-lava', None)
    if not wrangler:
        return None

    return wrangler(obj, now, theVersion)

def getWrangler(obj, now, style):
    wrangler = obj.getDefaultedString(style, now, [''])[0]
    if not wrangler and style != 'camera_wrangler':
        return None

    wname = wrangler
    wrangler = '%s-lava' % wrangler

    if style == 'light_wrangler':
        wrangler = soho.LightWranglers.get(wrangler, None)
    elif style == 'camera_wrangler':
        wrangler = soho.CameraWranglers.get(wrangler, None)
        if not wrangler:
            wrangler = soho.CameraWranglers.get('HoudiniCamera-lava', None)
    elif style == 'object_wrangler':
         wrangler = soho.ObjectWranglers.get(wrangler, None)
    if not wrangler:
        if wanem not in _Settings.MissingWranglers:
            _Settings.MissingWranglers[wname] = True
            soho.warning('Object %s has an unsupported wrangler (%s)'
                        % (obj.getName(), wname))
        return None
    return wrangler(obj, now, theVersion)

def addGlobal(style, token, storage, houdini, skipdefault=True):
    parm = SohoParm(houdini, storage, None, skipdefault, key=token)
    parm.Style = style
    _Settings.Global.append(parm)

def addObject(token, storage, houdini, skipdefault=True):   
    _Settings.Object.append(SohoParm(houdini, storage, None, skipdefault, token))

def addLight(token, storage, houdini, skipdefault=True):
    _Settings.Light.append(SohoParm(houdini, storage, None, skipdefault, token))

def addFog(token, storage, houdini, skipdefault=True):
    _Settings.Fog.append(SohoParm(houdini, storage, None, skipdefault, token))

def addGeometry(token, storage, houdini, skipdefault=True):
    _Settings.Geometry.append(SohoParm(houdini, storage, None, skipdefault, token))

def addDeepResolver(driver, token, storage, houdini, default=None):
    if driver not in _Settings.DeepResolver:
        _Settings.DeepResolver[driver] = []
    _Settings.DeepResolver[driver].append(SohoParm(houdini, storage, default,
                                             default == None, token))

def addMeasure(driver, token, storage, houdini, skipdefault=True):
    if driver not in _Settings.Measure:
        _Settings.Measure[driver] = []
    _Settings.Measure[driver].append(SohoParm(houdini, storage, None, skipdefault, token))

def addBokeh(driver, token, storage, houdini, skipdefault=True):
    if driver not in _Settings.Bokeh:
        _Settings.Bokeh[driver] = []
    _Settings.Bokeh[driver].append(SohoParm(houdini, storage, None, skipdefault, token))

def addImagePlane(token, storage, houdini, skipdefault=True, mainimage=True):
    if mainimage:
        _Settings.MPlane.append(SohoParm(houdini, storage, None, skipdefault, token))
    iplane = houdini + '_plane%d'
    _Settings.IPlane.append(SohoParm(iplane, storage, None, skipdefault, token))

def addImageOption(token, storage, houdini, skipdefault=True):
    _Settings.IOption.append(SohoParm(houdini, storage, None, skipdefault, token))

def hideUVObject(name):
    _Settings.UVHiddenObjects[name] = True

pbrParms = {
    'pathtype'          : SohoParm('lv_pbrpathtype', 'string', ['diffuse'], True),
    'raylimiteval'      : SohoParm('lv_raylimiteval', 'string', ['none'], True),
    'colorlimit'        : SohoParm('lv_colorlimit', 'real', [10], True),
    'multilight'        : SohoParm('lv_pbrmultilight', 'int', [1], True),
    'colorspace'        : SohoParm('lv_colorspace', 'string', ['linear'], True),
}

# Set up keys so that evaluation will index things according to the shader
# parameter name rather than the houdini name.
def _setKeys(plist):
    #for key in plist.keys():
    for key in plist:
        plist[key].Key = key

camXtraParms = {
    'lv_bokeh'        : SohoParm('lv_bokeh',    'string', ['radial'], False),
    'lv_numuserattrib': SohoParm('lv_numuserattrib', 'int', [0], True),
}

def _isDefaultBokeh(bokeh, plist):
    if bokeh != 'radial':
        return False
    return True

gshaderParms = [
    SohoParm('lv_lensshader', 'shader', [''], True, shoptype="cvex", key='lensshader'),
    SohoParm('lv_generatorshader', 'shader', [''], True, shoptype="atmosphere", key='generatorshader'),
]

oshaderParms = [
    SohoParm('shop_materialpath', 'string', [''], True, key='materialname'),
    SohoParm('shop_surfacepath',  'shader', [''], True, shoptype="surface", key='surface'),
    SohoParm('shop_displacepath', 'shader', [''], True, shoptype="displace", key='displace'),
    SohoParm('lv_matteshader',    'shader', [''], True, shoptype="surface", key='matteshader'),
    SohoParm('shop_cvexpath',     'shader', [''], True, shoptype="cvex", key='cvex'),
]

ostylesheetParms = [
    SohoParm('shop_materialstylesheet',  'string', [''], True, key='materialstylesheet'),
]

oshaderSkipParms = {
    'shop_surfacepath' : SohoParm('shop_disable_surface_shader', 'bool', [False], False, key='surface'),
    'shop_displacepath' : SohoParm('shop_disable_displace_shader', 'bool', [False], False, key='displace'),
    'lv_matteshader' : SohoParm('shop_disable_surface_shader',  'bool', [False], False, key='matteshader'),
}

oshaderMap = {       
    'shop_materialpath' : 'surface',
    'shop_surfacepath'  : 'surface',
    'lv_materialpath'   : 'surface',
    'lv_matteshader'    : 'matteshader',
    'shop_displacepath' : 'displace',
    'shop_cvexpath'     : 'cvex',
}

lshaderParms = [
    SohoParm('shop_lightpath',  'shader', [''], True, shoptype="light", key='shader'),
    SohoParm('shop_shadowpath', 'shader', [''], True, shoptype="lightshadow", key='shadow'),
    SohoParm('lv_samplershader','shader', [''], True, shoptype="light", key='samplershader'),
    SohoParm('lv_tracershader','shader', [''], True, shoptype="light", key='tracershader'),
    SohoParm('lv_illumshader','shader', [''], True, shoptype="surface", key='illumshader'),
]

lshaderSkipParms = {
    'shop_lightpath' : SohoParm('shop_disable_light_shader',  'bool', [False], False, key='shader'),
    'shop_shadowpath' : SohoParm('shop_disable_shadow_shader', 'bool', [False], False, key='shadow'),
}

objXtraParms = {
    'lv_measure'      : SohoParm('lv_measure',    'string', ['nonraster'],False),
    'lv_numuserattrib': SohoParm('lv_numuserattrib', 'int', [0], True),
    'lv_phantom'      : SohoParm('lv_phantom', 'bool',      [0], True),
}

stylesheetParms = {
    'declare_stylesheets': SohoParm('declare_stylesheets', 'string', ['*'], False),
    'apply_stylesheets'  : SohoParm('apply_stylesheets', 'string', [''], False),
    'declare_bundles'    : SohoParm('declare_bundles', 'int', [0], False)
}

bakingParms = {
    'bake_layerexport'             :SohoParm('lv_bake_layerexport',             'int',    [0],          key='bake_layerexport'),
    'bake_samples'                 :SohoParm('lv_bake_samples',                 'int',    [16],        key='bake_samples'),
    'bake_tangentnormalflipx'      :SohoParm('lv_bake_tangentnormalflipx',      'int',    [0],          key='bake_tangentnormalflipx'),
    'bake_tangentnormalflipy'      :SohoParm('lv_bake_tangentnormalflipy',      'int',    [0],          key='bake_tangentnormalflipy'),
    'bake_tangentnormalincludedisp':SohoParm('lv_bake_tangentnormalincludedisp','int',    [1],          key='bake_tangentnormalincludedisp'),
    'bake_occlusionbias'           :SohoParm('lv_bake_occlusionbias',           'float',  [0.5],        key='bake_occlusionbias'),
    'bake_cavitydistance'          :SohoParm('lv_bake_cavitydistance',          'float',  [1.0],        key='bake_cavitydistance'),
    'bake_cavitybias'              :SohoParm('lv_bake_cavitybias',              'float',  [0.5],        key='bake_cavitybias'),
    'bake_curvatureocc'            :SohoParm('lv_bake_curvatureocc',            'bool',	  [0],          key='bake_curvatureocc'),
    'bake_curvaturesdist'          :SohoParm('lv_bake_curvaturesdist',          'float',  [0.1],        key='bake_curvaturesdist'),
    'bake_curvaturescale'          :SohoParm('lv_bake_curvaturescale',          'float',  [1.0],        key='bake_curvaturescale'),
    'bake_curvaturebias'           :SohoParm('lv_bake_curvaturebias',           'float',  [0.5],        key='bake_curvaturebias'),
}

def setShadowMap(state):
    _Settings.ShadowMap = state

def _isDefaultMeasure(measure, plist):
    if measure != 'nonraster':
        return False
    for p in plist:
        if p.Key == 'zimportance':
            if p.Value[0] != 1:
                return False
        elif p.Key == 'offscreenquality':
            if p.Value[0] != 0.25:
                return False
        else:
            return False        # Unexpected parameter
    return True

def _outputShaderList(objtype, obj, wrangler, now, shaderParms, skipParms):
    plist = obj.wrangle(wrangler, shaderParms, now)
    skiplist = None
    if skipParms:
        skiplist = obj.wrangle(wrangler, skipParms, now)

    for parm in plist:
        skip = False
        if skiplist and parm.Key in skiplist:
            skip = skiplist[parm.Key].Value[0]
        if not skip:
            if parm.Key == 'materialname':
                if parm.Value[0] == '':
                    parm.Value[0] = 'defaultshader'
                # Convert to full path
                try:
                    sop = hou.node(obj.getName())
                    hou_shop = sop.node(parm.Value[0])
                    if hou_shop:
                        parm.Value[0] = hou_shop.path()
                except:
                    pass
                if _Settings.GenerateMaterialname:
                    cmd_property('object', parm.Key, [parm.Value[0]])
            else:
                cmd_shader(objtype, parm.Key, parm.Value[0], getattr(parm, "ShopType", soho.ShopTypeDefault))

def _getObjectStyleSheets( obj, wrangler, now):
    stylesheets = []
    plist = obj.wrangle(wrangler, ostylesheetParms, now)
    for parm in plist:
        stylesheet_str = parm.Value[0].strip()
        if stylesheet_str:
            stylesheets.append((parm.Key, stylesheet_str))
    
    return stylesheets

def _outputObjectStylesheet(objtype, obj, wrangler, now):
    stylesheets = _getObjectStyleSheets(obj, wrangler, now)
    for (parmname, stylesheet) in stylesheets:
        text_name = "objstylesheet:" + obj.getName()
        cmd_textblock(text_name, stylesheet)
        cmd_property(objtype, parmname, ["text:"+text_name])

def getBundlesAndMaterialsFromObjectStyleSheet(obj, now, wrangler=None):
    materials = []
    bundles = []
    stylesheets = _getObjectStyleSheets(obj, wrangler, now)
    for (parmname, stylesheet) in stylesheets:
        (ss_bundles, ss_mats) = getBundlesAndMaterialsFromStyleSheet(stylesheet)
        materials.extend(ss_mats)
        bundles.extend(ss_bundles)
    return (bundles, materials)

def getMaterialsFromOverrideSet(override_set, materials, unique_materials):
    # Look for named material SHOPs.
    try:
        material = override_set["material"]["name"]
        # Material may be specified in a short format (just a string), but it
        # may be in UT_Option format, ie, a dict that has 'type' and 'value'.
        if isinstance(material, dict):
            material = material["value"]
        if material not in unique_materials: 
            unique_materials.add(material)
            materials.append(material)
    except:
        pass
    # Look for CVEX script SHOPs.
    try:
        for category in override_set.values():
            for value in category.values():
                try:
                    material = value["script"]["node"]
                    if material not in unique_materials: 
                        unique_materials.add(material)
                        materials.append(material)
                except:
                    continue
    except:
        return

def getBundlesAndMaterialsFromStyleSheet(stylesheet_string):
    if _Settings.ParsedStyleSheets.get(stylesheet_string, None) == None:
        materials = []
        bundles = []
        try:
            stylesheet = hutil.json.object_from_json_data(
                hutil.json.utf8Loads(stylesheet_string))
        except:
            _Settings.ParsedStyleSheets[stylesheet_string] = (bundles, materials)
            return _Settings.ParsedStyleSheets[stylesheet_string]

        unique_materials = set()
        unique_bundles = set()
        styles = stylesheet.get("styles", [])
        for entry in styles:
            if "overrides" in entry:
                getMaterialsFromOverrideSet(entry["overrides"], materials, unique_materials)
            if "target" in entry:
                try:
                    bundle = entry["target"]["objectBundle"]
                    if bundle not in unique_bundles: 
                        unique_bundles.add(bundle)
                        bundles.append(bundle)
                except:
                    continue

        # Output SHOPs referenced in shared override sets.
        override_sets = stylesheet.get("overrideDefinitions", {})
        for entry in override_sets.values():
            getMaterialsFromOverrideSet(entry, materials, unique_materials)

        # Output SHOPs referenced in shared scripts.
        shared_scripts = stylesheet.get("scriptDefinitions", {})
        for entry in shared_scripts.values():
            try:
                material = entry["node"]
                if material not in unique_materials: 
                    unique_materials.add(material)
                    materials.append(material)
            except:
                continue

        _Settings.ParsedStyleSheets[stylesheet_string] = (bundles, materials)

    return _Settings.ParsedStyleSheets[stylesheet_string]

def outputBundle(bundle_name):
    if _Settings.SavedBundles.get(bundle_name, None) == None:
        try:
            bundle = hou.nodeBundle(bundle_name)
            node_paths = list(node.path() for node in bundle.nodes())
            cmd_bundle(bundle_name, node_paths)
        except:
            cmd_bundle(bundle_name, [])
        _Settings.SavedBundles[bundle_name] = True

def outputBundles(now, dirtybundles, for_update):
    ss_parms = soho.evaluate(stylesheetParms)
    if for_update:
        bundle_list = dirtybundles.split()
    elif ss_parms['declare_bundles'].Value[0]:
        bundle_list = list(bundle.name() for bundle in hou.nodeBundles())
    else:
        bundle_list = []
    for bundle in bundle_list:
        outputBundle(bundle)

def outputMaterial(shop_path, now):
    if _Settings.SavedMaterials.get(shop_path, None) == None:
        shop = soho.getObject(shop_path)
        cmd_start('material')
        outputObject(shop, now, name=shop_path, output_shader=True)
        if _Settings.GenerateMaterialname:
            cmd_property('object', 'materialname', [shop_path])
        cmd_end()
        _Settings.SavedMaterials[shop_path] = True

# Return a tuple of the shader with its shop type if the given shader type
# is not skipped for the given node - otherwise return None.
def getObjectShader(shop, shader_type, now):
    skiplist = shop.evaluate(oshaderSkipParms, now)
    shader_prop = oshaderMap[shader_type]

    skip = False
    if skiplist and shader_prop in skiplist:
        skip = skiplist[shader_prop].Value[0]

    if not skip:
        shader = []
        shop_type = []
        if shop.evalShaderAndType(shader_type, now, shader, shop_type):
            return (shader[0], shop_type[0])

    return None

def outputGlobal(wrangler, obj, now):
    if LSDhooks.call('pre_outputGlobal', wrangler, obj, now):
        return
    plist = obj.wrangle(wrangler, _Settings.Global, now)
    if plist:
        cmd_propertyV(None, plist)

    xparms = obj.wrangle(wrangler, camXtraParms, now)
    lv_bokeh = xparms.get('lv_bokeh', None)
    if lv_bokeh:
        bokeh = lv_bokeh.Value[0]
        plist = evaluateBokeh(bokeh, obj, now)
        if not _isDefaultBokeh(bokeh, plist):
            cmd_propertyAndParms('camera', 'bokeh', bokeh, plist)

    lv_numuserattrib = xparms.get('lv_numuserattrib', None)
    if lv_numuserattrib:
        n = lv_numuserattrib.Value[0]
        for i in range(1, n+1):
            _outputUserAttribute(i, obj, now)

    (val, type) = obj.wrangleShaderAndType(wrangler, 'lv_pbrshader', now, [''])
    shader = val[0]
    if shader:
        cmd_shader('renderer', 'pbrshader', shader, type)
    else:
        _setKeys(pbrParms)
        plist = obj.evaluate(pbrParms, now)

        # Add the shader parameters
        shader = 'pathtracer use_renderstate 0'
        for parm in plist.values():
            shader += ' ' + parm.Key + ' ' + parm.toString()

        cmd_shader('renderer', 'pbrshader', shader, 
                    soho.getShopType( 'atmosphere' ))

    # Output other global shaders
    _outputShaderList('renderer', obj, wrangler, now, gshaderParms, None)

    LSDhooks.call('post_outputGlobal', wrangler, obj, now)

def outputLensShader(obj, wrangler, now):
    _outputShaderList('renderer', obj, wrangler, now, gshaderParms, None)

def outputObject(obj, now, name=None, wrangler=None, output_shader=True, check_renderable=False, opropmap=None):
    if LSDhooks.call('pre_objectSettings', obj, now, name, wrangler):
        return

    plist = obj.wrangle(wrangler, _Settings.Object, now)

    if check_renderable:
        render    = obj.getDefaultedInt('object:render', now, [1])[0]
        if not render:
            for p in plist:
                if p.Houdini == 'lv_renderable':
                    p.Value = [render]
                    render = None
                    break
    
    if name:
        for p in plist:
            if p.Houdini == 'object:name':
                p.Value = [name]
                break
    else:
        name = obj.getName()

    if plist != None and len(plist):
        cmd_propertyV("object", plist)
        if opropmap != None:
            for p in plist:
                map = soho.decodeParmId(p.ParmId)
                # When building the parameter map, we're most interested in the
                # parameter that actually defines the value (i.e. typically a
                # material level parameter), not the actual node parameter.
                pname = map.get('refparameter', map.get('parameter', None))
                if pname:
                    opropmap[pname] = p.Key
    if _Settings.GenerateOpId:
        cmd_property('object', 'id', obj.getDefaultedInt('object:id', now, [0]))

    if check_renderable and (render is not None) and (not render):
        cmd_property('object', 'renderable', [0])

    if name in _Settings.UVHiddenObjects:
        cmd_property('object', 'renderable', [0])

    xparms = obj.wrangle(wrangler, objXtraParms, now)

    # Handle phantom objects (which should appear in shadow maps)
    if not _Settings.ShadowMap:
        if name in _Settings.PhantomOverrides:
            cmd_property('object', 'phantom', [1])
        else:
            lv_phantom = xparms.get('lv_phantom', None)
            if lv_phantom:
                cmd_property('object', 'phantom', lv_phantom.Value)

    lv_measure = xparms.get('lv_measure', None)
    lv_numuserattrib = xparms.get('lv_numuserattrib', None)

    if name in _Settings.MatteOverrides:
        cmd_property('object', 'matte', [1])

    if lv_measure:
        measure = lv_measure.Value[0]
        plist = evaluateMeasure(measure, obj, now)
        if not _isDefaultMeasure(measure, plist):
            cmd_propertyAndParms('object', 'measure', measure, plist)
    if lv_numuserattrib:
        n = lv_numuserattrib.Value[0]
        for i in range(1, n+1):
            _outputUserAttribute(i, obj, now)
    # For velocity motion blur, we need to have access to the velocity
    # scale in the instance as well for volume rendering.
    mbinfo = LSDmisc.geo_mbsamples(obj, now)
    if mbinfo[1] and len(mbinfo[0]) == 2:
        times = mbinfo[0]
        val = mbinfo[1]
        cmd_property('object', 'velocityblur', [val])
        cmd_property('object', 'velocityscale', [times[1]-times[0]])
    LSDmisc.ouputMotionBlurInfo(obj,now)

    if output_shader:
        _outputShaderList('object', obj, wrangler, now, oshaderParms, oshaderSkipParms)
        _outputObjectStylesheet('object', obj, wrangler, now)

    
    # We return the displacement bound so that geometry with multiple
    # displacement bounds could
    LSDhooks.call('post_objectSettings', obj, now, name, wrangler)
    return

def outputGeometry(obj, now):
    if LSDhooks.call('pre_geometrySettings', obj, now):
        return
    plist = obj.evaluate(_Settings.Geometry, now)
    if plist and len(plist):
        cmd_propertyV('geometry', plist)
    LSDhooks.call('post_geometrySettings', obj, now)

def outputStyleSheets(now, dirtystylesheets, for_update):
    ss_parms = soho.evaluate(stylesheetParms)
    ss_declare = ss_parms['declare_stylesheets'].Value[0]
    ss_apply = ss_parms['apply_stylesheets'].Value[0]
    if dirtystylesheets is not None:
        ss_dirty_list = dirtystylesheets.split()
    else:
        ss_dirty_list = None
    # Declare style sheets and any materials used by the style sheets.
    for style in hou.styles.styles(ss_declare):
        if ss_dirty_list is None or style in ss_dirty_list:
            if ss_dirty_list is not None:
                ss_dirty_list.remove(style)
            stylesheet = hou.styles.stylesheet(style)
            (ss_bundles, ss_mats) = getBundlesAndMaterialsFromStyleSheet(stylesheet)
            for mat_path in ss_mats:
                if hou.node(mat_path):
                    outputMaterial(mat_path, now)
            for bundle in ss_bundles:
                outputBundle(bundle)
            cmd_textblock("stylesheet:" + style, stylesheet)

    # Remove style sheets that have been deleted.
    if ss_dirty_list is not None:
        for style in ss_dirty_list:
            cmd_textblock("stylesheet:" + style, '')

    # Output command to apply the requested style sheets.
    if for_update or (ss_apply != '' and not ss_apply.isspace()):
        cmd_stylesheet(ss_apply)

def declareBakingParms(now, for_update):
    plist = soho.evaluate(bakingParms, now)
    for pname, parm in iteritems(plist):
        if parm.Value == parm.Default:
            continue
        cmd_declare('global', parm.Type, 'global:%s' % pname, parm.Value)

def setMattePhantomOverrides(now, matte_objects, phantom_objects):
    _Settings.MatteOverrides = {}
    _Settings.PhantomOverrides = {}
    rop = soho.getOutputDriver()
    if matte_objects:
        for obj in rop.objectList('objlist:instance', now, matte_objects):
            _Settings.MatteOverrides[obj.getName()] = True
    if phantom_objects:
        for obj in rop.objectList('objlist:instance', now, phantom_objects):
            _Settings.PhantomOverrides[obj.getName()] = True

def outputLight(wrangler, obj, now):
    if LSDhooks.call('pre_lightSettings', wrangler, obj, now):
        return

    plist = obj.wrangle(wrangler, _Settings.Light, now)
    if plist:
        cmd_propertyV('light', plist)

    _outputShaderList('light', obj, wrangler, now, lshaderParms, lshaderSkipParms)

    if LSDhooks.call('post_lightSettings', wrangler, obj, now):
        return

fshaderParms = [
    SohoParm('shop_fogpath',  'shader', [''], True, shoptype="atmosphere", key='shader'),
]

fshaderSkipParms = {
    'shop_fogpath' : SohoParm('shop_disable_fog_shader', 'bool', [False], True),
}

def outputFog(wrangler, obj, now):
    if LSDhooks.call('pre_fogSettings', wrangler, obj, now):
        return

    plist = obj.wrangle(wrangler, _Settings.Fog, now)
    if plist:
        cmd_propertyV('fog', plist)

    _outputShaderList('fog', obj, wrangler, now, fshaderParms, fshaderSkipParms)

    LSDhooks.call('post_fogSettings', wrangler, obj, now)

def evaluateDeepResolver(driver, wrangler, obj, now):
    deep = _Settings.DeepResolver.get(driver, None)
    if not deep:
        return None
    return obj.wrangle(wrangler, deep, now)

def evaluateMeasure(driver, obj, now):
    measure = _Settings.Measure.get(driver, None)
    if measure:
        return obj.evaluate(measure, now)
    return []

def evaluateBokeh(driver, obj, now):
    bokeh = _Settings.Bokeh.get(driver, None)
    if bokeh:
        return obj.evaluate(bokeh, now)
    return []

def evaluateImagePlane(idx, wrangler, obj, now):
    if idx < 0:
        return obj.wrangle(wrangler, _Settings.MPlane, now)
    # Auxilliary image plane
    for s in _Settings.IPlane:
        s.setIndex(idx)
    return obj.evaluate(_Settings.IPlane, now)

_iplay_specific = {
    'rendermode' : SohoParm('lv_image_mplay_rendermode', 'string', ['current'], False, key='rendermode'),
    'framemode'  : SohoParm('lv_image_mplay_framemode', 'string', ['append'], False, key='framemode'),
    'trange'     : SohoParm('trange', 'int', [0], False, key='trange')
}

def outputMPlayFormatOptions(wrangler, cam, now):
    plist = cam.wrangle(wrangler, _iplay_specific, now)
    rendermode = plist['rendermode'].Value[0]
    framemode  = plist['framemode'].Value[0]
    trange     = plist['trange'].Value[0]
    curframe = hou.timeToFrame(now)
    if trange:
        frange = cam.wrangleInt(wrangler, 'f', now, [curframe, curframe])
        if len(frange) < 2:
            frange = [curframe, curframe]
    else:
        frange = [curframe, curframe]

    # There are 4 combinations of rendermode and framemode
    #   rendermode/framemode |  append    |  match
    #   ---------------------+------------+-----------
    #         new            | new-append | new-frame
    #       current          |   append   |  replace
    #  However, we only perform "new" render mode if we the render
    #  frame is at the beginning of the frame range
    if abs(curframe-frange[0]) < 0.01:
        rendermode = 'current'
    
    if rendermode == 'new':
        if framemode == 'append':
            rendermode = 'new-append'
        else:
            rendermode = 'new-frame'
    else:
        if framemode == 'append':
            rendermode = 'append'
        else:
            rendermode = 'replace'
    
    #socketport = cam.wrangleString(wrangler, 'vm_image_mplay_socketport', now, [0])[0]
    #cmd_declare('plane', 'int', 'IPlay.socketport', [socketport])

    #sockethost = cam.wrangleString(wrangler, 'vm_image_mplay_sockethost', now, ['localhost'])[0]
    #cmd_declare('plane', 'string', 'IPlay.sockethost', [sockethost])

    cmd_declare('plane', 'string', 'IPlay.rendermode', [rendermode])
    frange = '%d %d' % (int(frange[0]), int(frange[1]))
    cmd_declare('plane', 'string', 'IPlay.framerange', [frange])
    cmd_declare('plane', 'int', 'IPlay.currentframe', [int(curframe)])
    rendersource = soho.getDefaultedString('lv_rendersource', [soho.getOutputDriver().getName()])
    cmd_declare('plane', 'string', 'IPlay.rendersource', rendersource)

def outputImageFormatOptions(wrangler, cam, now):
    if LSDhooks.call('pre_outputImageFormatOptions', wrangler, cam, now):
        return
    # Output image format options for the image

    # First, we have to output some special options for the "ip"
    # device.
    outputMPlayFormatOptions(wrangler, cam, now)

    # Now, output the generic format options
    plist = cam.wrangle(wrangler, _Settings.IOption, now)
    for p in plist:
        cmd_declare('plane', p.Type, p.Key, p.Value)
    LSDhooks.call('post_outputImageFormatOptions', wrangler, cam, now)

#
# Feature Sets
#
def addFeature(name, value):
    _Settings.Features[name] = value

def getFeature(name):
    return _Settings.Features.get(name, None)

def initializeFeatures():
    LSDlava.initializeFeatures(sys.modules[__name__])
