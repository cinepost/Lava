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
# NAME: LSDframe.py ( Python )
#
# COMMENTS:
#

import sys, math, os, re, json, fnmatch, uuid, base64, time
import tempfile
import hou, soho
from soho import SohoParm
import SOHOcommon
import LSDgeo
import LSDmisc
import LSDsettings
import LSDhooks
from LSDapi import *
from hutil.file import insertFileSuffix
import quickplanes

_OutputObjects = set()

CubeMapSuffixes = [ ".zp.pic", ".xp.pic", ".zm.pic",
                    ".xm.pic", ".yp.pic", ".ym.pic"]
CubeMapMatrices = [
    hou.Matrix4((1,0,0,0,  0,1,0,0,  0,0,1,0, 0,0,0,1)),       # +z
    hou.Matrix4((0,0,-1,0, 0,1,0,0,  1,0,0,0, 0,0,0,1)),       # +x
    hou.Matrix4((1,0,0,0,  0,-1,0,0, 0,0,-1,0,  0,0,0,1)),     # -z
    hou.Matrix4((0,0,1,0,  0,1,0,0,  -1,0,0,0,  0,0,0,1)),     # -x
    hou.Matrix4((1,0,0,0,  0,0,-1,0, 0,1,0,0, 0,0,0,1)),       # +y
    hou.Matrix4((1,0,0,0,  0,0,1,0,  0,-1,0,0,  0,0,0,1)),     # -y
]

class CubeMapControl:
    def __init__(self, obj, now):
        self.Object = obj
        self.Face = -1
        self.BasePath = obj.getDefaultedString('lv_picture', now, [''])[0]

    def setFace(self, dir):
        self.Face = dir
        self.Filename = self.BasePath + CubeMapSuffixes[dir]
        self.Transform = CubeMapMatrices[dir]

    def makeMap(self):
        cmd_makecubemap(self.BasePath, CubeMapSuffixes)

def objectSingleTransform(space, obj, now,
                        invert, xfunc, cubemap):
    xform = []
    if not obj.evalFloat(space, now, xform):
        return False
    if len(xform) != 16:
        return False
    if invert:
        xform = list(hou.Matrix4(xform).inverted().asTuple())
    if cubemap:
        # We want to generate the cube-map in "world".  To do this, we
        # find the position of the object's center and construct a
        # transform based on that position.
        P = hou.Vector3((0,0,0)) * hou.Matrix4(xform)
        hxform = hou.hmath.buildTranslate(P[0], P[1], P[2])
        # Transform to orient for the face we're rendering
        hxform *= cubemap.Transform
        xform = list(hxform.asTuple())
    xfunc(xform)
    return True

def objectTransform(space, obj, times, xfunc=cmd_transform, mxfunc=cmd_mtransform, invert=False,
                        cubemap=None):
    if LSDhooks.call('pre_objectTransform', space, obj, times,
                xfunc, invert, cubemap):
        return
    if not objectSingleTransform(space, obj, times[0], invert,
                                 xfunc, cubemap):
        return
    seg = 1
    while seg < len(times):
        objectSingleTransform(space, obj, times[seg], invert,
                        mxfunc, cubemap)
        seg += 1
    LSDhooks.call('post_objTransform', space, obj, times, xfunc, invert,cubemap)

def defplane(channel, variable, vextype, idx, wrangler, cam, now,
                filename=None,
                component=None,
                lightexport=None,
                showrelightingbuffer=False,
                excludedcm=False):
    if len(variable):
        if LSDhooks.call('pre_defplane', variable, vextype, idx,
                    wrangler, cam, now, filename, lightexport):
            return

        if not channel:
            channel = variable
        cmd_start('plane')
        cmd_property('plane', 'variable', [variable])
        cmd_property('plane', 'vextype', [vextype])
        if filename:
            soho.makeFilePathDirsIfEnabled(filename)
            cmd_property('plane', 'planefile', [filename])
        cmd_property('plane', 'channel', [channel])
        if lightexport is not None:
            cmd_property('plane', 'lightexport', [lightexport])
        if component:
            cmd_property('plane', 'component', [component])
        if showrelightingbuffer:
            cmd_property('plane', 'showrelightingbuffer', [showrelightingbuffer])
        if excludedcm:
            cmd_property('plane', 'excludedcm', [excludedcm])

        plist = LSDsettings.evaluateImagePlane(idx, wrangler, cam, now)
        cmd_propertyV('plane', plist)

        if LSDhooks.call('post_defplane', variable, vextype, idx,
                    wrangler, cam, now, filename, lightexport):
            return
        cmd_end()

def iprplane(channel):
    if len(channel):
        if LSDhooks.call('pre_iprplane', channel):
            return
        cmd_start('plane')
        cmd_property('plane', 'variable', [channel])
        cmd_property('plane', 'vextype', ['float'])
        cmd_property('plane', 'channel', [channel])
        cmd_property('plane', 'sfilter', ['closest'])
        cmd_property('plane', 'pfilter', ['minmax idcover'])
        if LSDhooks.call('post_iprplane', channel):
            return
        cmd_end()

displayParms = {
    'lv_device'  :SohoParm('lv_device',   'string',     [''], False),
    'lv_foptions':SohoParm('lv_foptions', 'string',     [''], False),
    'lv_numaux'  :SohoParm('lv_numaux',   'int',        [0], False),
    'lv_exportcomponents'  :SohoParm('lv_exportcomponents',   'string',        [''], False),
}

planeDisplayParms = {
    'disable'   :SohoParm('lv_disable_plane%d', 'int',  [0], False,
                                key='disable'),
    'excludedcm': SohoParm('lv_excludedcm_plane%d', 'int',  [0], False,
                                key='excludedcm'),
    'variable'  :SohoParm('lv_variable_plane%d','string', [''], False,
                                key='variable'),
    'vextype'   :SohoParm('lv_vextype_plane%d', 'string', ['float'], False,
                                key='vextype'),
    'usefile'   :SohoParm('lv_usefile_plane%d', 'int', [0], False,
                                key='usefile'),
    'filename'  :SohoParm('lv_filename_plane%d', 'string', [''], False,
                                key='filename'),
    'channel'   :SohoParm('lv_channel_plane%d', 'string', [''], False,
                                key='channel'),
    'showrelightingbuffer' :SohoParm('lv_show_relighting_buffer%d', 'bool', [0], False,
                                key='showrelightingbuffer'),

    # Batch light exports
    'lexport'   :SohoParm('lv_lightexport%d', 'int', [0], False,
                                key='lightexport'),
    'cexport'   :SohoParm('lv_componentexport%d', 'int', [0], False,
                                key='componentexport'),
    'lightscope':SohoParm('lv_lightexport_scope%d', 'string', ['*'], False,
                                key='lightscope'),
    'lightselect':SohoParm('lv_lightexport_select%d', 'string', ['*'], False,
                                key='lightselect'),

    # Some parameters for backward compatibility with H9.0
    'h9channel' : SohoParm('lv_picture_plane%d', 'string', [''], False,
                                key='h9channel'),
}

stereoParms = {
    'lv_s3dleftcamera'  :SohoParm('lv_s3dleftcamera',  'bool',   [0],  False),
    'lv_s3drightcamera' :SohoParm('lv_s3drightcamera', 'bool',   [0],  False),
    'lv_filenamesuffix' :SohoParm('lv_filenamesuffix', 'string', [''], False),
}

uvRenderParms = {
    'lv_uvmkpath'       :SohoParm('lv_uvmkpath', 'int', [1], False),
    'lv_uvobjects'      :SohoParm('lv_uvobjects', 'int', [0], False),
    'lv_uvtype'         :SohoParm('lv_uvtype', 'string', ['udim'], False),
    'lv_uvhidecage'     : SohoParm('lv_uvhidecage', 'bool', [True], False),
    'lv_uvunwrapres'    :SohoParm('lv_uvunwrapres', 'int', [1024, 1024], False),
    'lv_isuvrendering'  :SohoParm('lv_isuvrendering', 'bool', [False], False),
    'lv_uv_unwrap_method' :SohoParm('lv_uv_unwrap_method', 'int', [0], False),
    'lv_uv_cmd_bias'  :SohoParm('lv_uv_cmd_bias', 'float', [0.01], False),
    'lv_uv_cmd_maxdist' :SohoParm('lv_uv_cmd_maxdist', 'float', [-1], False),
    'lv_uv_flip_normal': SohoParm('lv_uv_flip_normal', 'bool', [False], False),
}

uvObjectParms = {
    'lv_uvobjectenable'   :SohoParm('lv_uvobjectenable%d', 'int', [1], False, key='enable'),
    'lv_uvobject'         :SohoParm('lv_uvobject%d', 'string', [''], False, key='name'),
    'lv_uvcageobject'     :SohoParm('lv_uvcageobject%d', 'string', [''], False, key='cagelist'),
    'lv_uvhires'        :SohoParm('lv_uvhires%d', 'string', [''], False, key='hireslist'),
    'lv_uvoutputpicture'  :SohoParm('lv_uvoutputpicture%d', 'string', [''], False, key='picture'),
    'lv_uvoutputobject' :SohoParm('lv_uvoutputobject%d', 'string', [''], False, key='outobj')
}

tilingParms = {
    'lv_tile_render'    :SohoParm('lv_tile_render',  'bool', [0], False),
    'lv_tile_count_x'   :SohoParm('lv_tile_count_x', 'int',  [4], False),
    'lv_tile_count_y'   :SohoParm('lv_tile_count_y', 'int',  [4], False),
    'lv_tile_index'     :SohoParm('lv_tile_index',   'int',  [0], False),
    'lv_tile_filename_suffix'
            :SohoParm('lv_tile_filename_suffix','string', ['_tile%02d_'], False),
}

cryptoParms = {
    'lv_cryptolayerenable' :SohoParm('lv_cryptolayerenable%d', 'int',    [1],  False, key='enable'),
    'lv_cryptolayerprop'   :SohoParm('lv_cryptolayerprop%d',   'string', [''], False, key='layerprop'),
    'lv_cryptolayername'   :SohoParm('lv_cryptolayername%d',   'string', [''], False, key='layername'),
    'lv_cryptolayerrank'   :SohoParm('lv_cryptolayerrank%d',   'int',    [6],  False, key='layerrank'),
    'lv_cryptolayeroutputenable'   :SohoParm('lv_cryptolayeroutputenable%d', 'int',  [0],  False, key='layeroutputenable'),
    'lv_cryptolayeroutput' :SohoParm('lv_cryptolayeroutput%d', 'string', [''], False, key='layeroutput'),
    'lv_cryptolayersidecarenable'   :SohoParm('lv_cryptolayersidecarenable%d', 'int',    [0],  False, key='layersidecareneable'),
    'lv_cryptolayersidecar'     :SohoParm('lv_cryptolayersidecar%d',       'string', [''], False, key='layersidecar')
}

def envmapDisplay(cam, now, cubemap):
    if LSDhooks.call('pre_envmapDisplay', cam, now, cubemap):
        return
    filename = cubemap.Filename
    cmd_image(cubemap.Filename, '', '')
    LSDsettings.outputImageFormatOptions(None, cam, now)
    defplane("C", 'Cf+Af', 'vector4', -1, None, cam, now)
    soho.indent(-1, "", None)
    LSDhooks.call('post_envmapDisplay', cam, now, cubemap)

def lightDisplay(wrangler, light, now):
    if LSDhooks.call('pre_lightDisplay', wrangler, light, now):
        # The hook has output the light display for us
        return True
    filename = light.wrangleString(wrangler, 'lv_picture', now, [''])[0]
    if not filename:
        return False

    deep = None
    if not light.wrangleInt(wrangler, 'render_pointcloud', now, [0])[0]:
        deep = light.wrangleString(wrangler, 'lv_deepresolver', now, [''])[0]
        if deep == 'null':
            deep = None
    if deep:
        plist = LSDsettings.evaluateDeepResolver(deep, wrangler, light, now)
        if not plist:
            return False

    cmd_image(filename, '', '')
    LSDsettings.outputImageFormatOptions(wrangler, light, now)
    cmd_start('plane')
    if deep:
        cmd_property('plane', 'planefile', ['null:'])
        cmd_property('plane', 'variable', ['Of'])
        cmd_property('plane', 'vextype', ['vector'])
    else:
        soho.makeFilePathDirsIfEnabled(filename)
        cmd_property('plane', 'planefile', [filename])
        cmd_property('plane', 'variable', ['Z-Far'])
        cmd_property('plane', 'vextype', ['float'])
        cmd_property('plane', 'quantize', ['float'])
    cmd_property('plane', 'pfilter', ['ubox'])      # Unit box pixel filter
    cmd_end()
    if deep:
        # Make intermdiate directories
        for parm in plist:
            if parm.Houdini == 'lv_dsmfilename':
                soho.makeFilePathDirsIfEnabled(parm.Value[0])
        cmd_propertyAndParms('image', 'deepresolver', deep, plist)
    soho.indent(-1, "", None)
    LSDhooks.call('post_lightDisplay', wrangler, light, now)
    return True

def lightExportPlanes(wrangler, cam, now, lexport,
        lscope, lselect, channel, comp,
        variable, vextype, plane, filename, excludedcm,
        unique_names):
    # Define a plane for each light in the selection
    def _getName(channel, unique_names):
        # Ensue that each light gets a unique channel name.  This allows lights
        # defined within subnets that have the same name to have unique channel
        # names.
        if unique_names.has_key(channel):
            num = 0
            while num < 1000000:
                tmp = '_'.join([channel, '%X' % num])
                if not unique_names.has_key(tmp):
                    channel = tmp
                    break
                num += 1
        unique_names[channel] = True
        return channel

    if lexport == 2:
        # Merge all lights
        lightlist = []
        for l in cam.objectList('objlist:light', now, lscope, lselect):
            lightlist.append(l.getName())
        lightexport = ' '.join(lightlist)
        # If there are no lights, we can't pass in an empty string since
        # then mantra will think that light exports are disabled.  So pass
        # down an string that presumably doesn't match any light name.
        if not lightexport:
            lightexport = '__nolights__'
        defplane(_getName(channel, unique_names), variable, vextype, plane,
                wrangler, cam, now, filename, comp, lightexport,
                excludedcm=excludedcm)
    elif lexport == 1:
        for l in cam.objectList('objlist:light', now, lscope, lselect):
            lchannel = []
            suffix = l.getDefaultedString('lv_export_suffix', now, [''])[0]
            if not l.evalString('lv_export_prefix', now, lchannel):
                lchannel = [l.getName()[1:].rsplit('/', 1)[-1]]
            if lchannel[0]:
                lchannel = '%s_%s%s' % (lchannel[0], channel, suffix)
            elif suffix:
                lchannel = '%s%s' % (channel, suffix)
            else:
                soho.error("Empty suffix for per-light exports")
            lchannel = _getName(lchannel, unique_names)
            defplane(lchannel, variable, vextype, plane,
                    wrangler, cam, now, filename, comp, l.getName(),
                    excludedcm=excludedcm)
    else:
        defplane(channel, variable, vextype, plane, wrangler, cam, now,
                filename, comp,
                excludedcm=excludedcm)

def quickImagePlanes(wrangler, cam, now, components):
    def _quickPlane(wrangler, cam, now, variable, channel,
                    vextype, quantize, opts):
        if LSDhooks.call('pre_defplane', variable, vextype, -1,
                    wrangler, cam, now, '', 0):
            return
        cmd_start('plane')
        cmd_property('plane', 'variable', [variable])
        cmd_property('plane', 'channel', [channel])
        cmd_property('plane', 'vextype', [vextype])
        if quantize != 'float16':
            cmd_property('plane', 'quantize', [quantize])
        for opt, optvalue in opts.iteritems():
            cmd_property('plane', opt, optvalue)

        if LSDhooks.call('post_defplane', variable, vextype, -1,
                    wrangler, cam, now, '', 0):
            return
        cmd_end()

    quickplanedict = quickplanes.getPlaneDict()
    toggleplanedict = quickplanes.getTogglePlaneDict()

    plist = {}
    for p in toggleplanedict.keys():
        plist[p] = SohoParm(p, 'int', [0])
    plist = cam.wrangle(wrangler, plist, now)

    for parmname, varnames in toggleplanedict.iteritems():
        is_set = plist.get(parmname, None)
        if not is_set or is_set.Value[0] == 0:
            continue
        for variable in varnames:
            plane = quickplanedict[variable]
            channel = cam.wrangleString(wrangler,
                                parmname+'_channel', now, [''])[0]
            if not channel:
                channel = variable if len(plane.channel) == 0 else plane.channel
            if plane.percomp:
                for comp in components.split():
                    compvariable = re.sub('_comp$', "_" + comp, variable)
                    compchannel = re.sub('_comp$', "_" + comp, channel)
                    _quickPlane(cam, wrangler, now, compvariable, compchannel,
                                plane.vextype, plane.quantize, plane.opts)
            else:
                _quickPlane(cam, wrangler, now, variable, channel,
                            plane.vextype, plane.quantize, plane.opts)

def cameraDisplay(wrangler, cam, now):
    if LSDhooks.call('pre_cameraDisplay', wrangler, cam, now):
        return True
    filename = cam.wrangleString(wrangler, 'lv_picture', now, ['ip'])[0]
    plist       = cam.wrangle(wrangler, displayParms, now)
    device      = plist['lv_device'].Value[0]
    foptions    = plist['lv_foptions'].Value[0]
    numaux      = plist['lv_numaux'].Value[0]
    components  = plist['lv_exportcomponents'].Value[0]

    flipbook = cam.wrangleInt(wrangler, 'flipbook_output', now, [0])[0]
    if flipbook:
        filename = 'ip'
        device = ''              # inherit from choice

    if not filename:
        return False

    is_preview = LSDmisc.isPreviewMode()

    plist           = cam.wrangle(wrangler, stereoParms, now)
    is_left_cam     = plist['lv_s3dleftcamera'].Value[0]
    is_right_cam    = plist['lv_s3drightcamera'].Value[0]
    file_suffix     = plist['lv_filenamesuffix'].Value[0]
    no_suffix       = cam.getData('NoFileSuffix')

    if no_suffix is not None and no_suffix:
        file_suffix = None
    if is_left_cam:
        cmd_declare('plane', 'string', 'IPlay.s3dleftplane', ['C'])
    else:
        cmd_declare('plane', 'string', 'IPlay.s3dleftplane', [''])

    if is_right_cam:
        cmd_declare('plane', 'string', 'IPlay.s3drightplane', ['C'])
    else:
        cmd_declare('plane', 'string', 'IPlay.s3drightplane', [''])
    filename = insertFileSuffix(filename, file_suffix)

    cmd_image(filename, device, foptions)
    LSDsettings.outputImageFormatOptions(wrangler, cam, now)

    # uv render may have Cf+Af disabled for performance reasons
    skipCf = cam.wrangleInt(wrangler, 'lv_bake_skipcf', now, [0])[0]
    if not skipCf:
        defplane("C", 'Cf+Af', 'vector4', -1, wrangler, cam, now)
    else:
        defplane("C", 'Of', 'vector', -1, wrangler, cam, now)

    lv_relightingbuffer = [0]
    soho.evalInt("lv_relightingbuffer", lv_relightingbuffer)

    lv_stylesheets = [0]
    soho.evalInt("lv_stylesheets", lv_stylesheets)

    if is_preview and lv_relightingbuffer[0]:
        defplane("C_Relighting", 'Cf+Af', 'vector4', -1,
                 wrangler, cam, now, showrelightingbuffer=True)

    # Initialize property for Op_Id generation
    LSDsettings._Settings.GenerateOpId = cam.getDefaultedInt('lv_generate_opid', now, [0])[0]

    quickImagePlanes(wrangler, cam, now, components)

    primary_filename = filename
    unique_names_per_filename = {}
    unique_names_per_filename[primary_filename] = {}

    for plane in range(1, numaux+1):
        for s in planeDisplayParms:
            planeDisplayParms[s].setIndex(plane)
        plist = cam.wrangle(wrangler, planeDisplayParms, now)
        disable  = plist['disable'].Value[0]
        excludedcm = plist['excludedcm'].Value[0]
        usefile  = plist['usefile'].Value[0]
        filename = plist['filename'].Value[0]
        channel  = plist['channel'].Value[0]
        variable = plist['variable'].Value[0]
        vextype  = plist['vextype'].Value[0]
        lexport  = plist['lightexport'].Value[0]
        cexport  = plist['componentexport'].Value[0]
        lscope   = plist['lightscope'].Value[0]
        lselect  = plist['lightselect'].Value[0]
        h9channel = plist['h9channel'].Value[0]

        # Don't add the IPR planes again if they're explicitly listed.
        if is_preview and (variable == 'Op_Id' or variable == 'Prim_Id'):
            continue

        if lv_stylesheets[0] and variable == 'Sty_Id':
            continue

        if h9channel and not channel:
            # Backward compatibility with H9 parameters.
            channel = channel

        filename = insertFileSuffix(filename, file_suffix)
        unique_names = unique_names_per_filename[primary_filename]

        if not disable and variable and vextype:
            if not channel:
                channel = variable

            if not usefile:
                filename = None
            elif not primary_filename == 'ip':
                if not unique_names_per_filename.has_key(filename):
                    unique_names_per_filename[filename] = {}
                unique_names = unique_names_per_filename[filename]

            if variable == 'Op_Id':
                LSDsettings._Settings.GenerateOpId = True

            if cexport == 1:
                for comp in components.split():
                    cchannel = channel + "_" + comp
                    lightExportPlanes(wrangler, cam, now,
                                lexport, lscope, lselect, cchannel, comp,
                                variable, vextype, plane, filename, excludedcm,
                                unique_names)
            else:
                lightExportPlanes(wrangler, cam, now,
                                lexport, lscope, lselect, channel, None,
                                variable, vextype, plane, filename, excludedcm,
                                unique_names)

            if is_preview and lv_relightingbuffer[0] and variable == 'Pixel_Samples':
                defplane('Pixel_Samples_Relighting', variable, vextype,
                         -1, wrangler, cam, now,
                         showrelightingbuffer=True,
                         excludedcm=excludedcm)

    # Add IPR planes after real planes
    if is_preview:
        LSDsettings._Settings.GenerateOpId = True
        iprplane('Op_Id')
        iprplane('Prim_Id')

    if lv_stylesheets[0]:
        iprplane('Sty_Id')

    soho.indent(-1, "", None)

    deep = cam.getDefaultedString('lv_deepresolver', now, [''])[0]
    if deep and not is_preview:
        parms = LSDsettings.evaluateDeepResolver(deep, wrangler, cam, now)
        if deep and parms:
            # adjust DCM file name for stereo cameras
            for parm in parms:
                if parm.Houdini in ('lv_dcmfilename', 'lv_dsmfilename'):
                    dcmfilename = insertFileSuffix(parm.Value[0], file_suffix)
                    soho.makeFilePathDirsIfEnabled(dcmfilename)
                    parm.Value = [dcmfilename]
            cmd_propertyAndParms('image', 'deepresolver', deep, parms)

    numcrypto = cam.getDefaultedInt('lv_cryptolayers', now, [0])[0]

    ropnode = hou.node(soho.getOutputDriver().getName())
    cryptoargs = []
    for o in range(1, numcrypto+1):
        cryptodict = {}
        for p in cryptoParms:
            cryptoParms[p].setIndex(o)
        plist = cam.wrangle(wrangler, cryptoParms, now)
        if plist['enable'].Value[0] == 0:
            continue

        cryptodict['prop'] = plist['layerprop'].Value[0]
        cryptodict['rank'] = plist['layerrank'].Value[0]
        cryptodict['name'] = plist['layername'].Value[0]

        # check if lv_cryptolayeroutputenable# parameter exists (older
        # instance of mantra ROP may not have this parameter, in which case we
        # must always evaluate output path):
        parmname = cryptoParms['lv_cryptolayeroutputenable'].Houdini
        cryptooutputpath = ''
        if ropnode.parm(parmname) == None or plist['layeroutputenable'].Value[0] != 0:
            cryptooutputpath = plist['layeroutput'].Value[0]
            cryptooutputpath = insertFileSuffix(cryptooutputpath, file_suffix)
            cryptodict['output'] = cryptooutputpath
            soho.makeFilePathDirsIfEnabled(cryptooutputpath)

        tmplayerpath = primary_filename
        if cryptooutputpath != '':
            tmplayerpath = cryptooutputpath

        if plist['layersidecareneable'].Value[0] != 0:
            sidecar = insertFileSuffix(plist['layersidecar'].Value[0], file_suffix)
            sidecaroutputpath = os.path.join(os.path.dirname(tmplayerpath), sidecar)
            soho.makeFilePathDirsIfEnabled(sidecaroutputpath)
            cryptodict['sidecar'] = sidecar

        cryptoargs.append(json.dumps(cryptodict))

    if len(cryptoargs) != 0:
        cmd_property('image', 'cryptoresolver', cryptoargs)

    LSDhooks.call('post_cameraDisplay', wrangler, cam, now)
    return True

camDofParms = {
    'projection':SohoParm('projection', 'string', ['perspective'], False),
    'focal'     :SohoParm('focal',      'real', [50], False),
    'fstop'     :SohoParm('fstop',      'real', [5.6], False),
    'focus'     :SohoParm('focus',      'real', [10], False),
    'focalunits':SohoParm('focalunits', 'string', ['mm'], False)
}

def getZoom(light, proj, focal, aperture):
    if proj == 'perspective':
        if aperture == 0:
            soho.error("Zero aperture for '%s'" % light.getName())
        return focal / aperture
    return 1

def getWindow(cam, wrangler, now):
    window = cam.getCameraScreenWindow(wrangler, now)
    windowmask = cam.objectList('objlist:windowmask', now)
    if windowmask is not None:
        window_bounds = SOHOcommon.getObjectScreenBoundsFull(
            now, wrangler, cam, windowmask,
            LSDgeo.getObjectBounds, LSDmisc.xform_mbsamples)

        if window_bounds is not None:
            window[0] = max(window[0], window_bounds[0])
            window[1] = min(window[1], window_bounds[1])
            window[2] = max(window[2], window_bounds[2])
            window[3] = min(window[3], window_bounds[3])
    return window

def outputCameraSegment(cam, wrangler, now, cubemap, dof, proj):
    if LSDhooks.call('pre_outputCameraSegment', cam, wrangler, now,
                    cubemap, dof, proj):
        return
    orthowidth = cam.wrangleFloat(wrangler, 'orthowidth', now, [2])[0]
    cmd_property('camera', 'orthowidth', [orthowidth])

    if cubemap:
        focal = 1.0
        aperture = 2.0
    else:
        focal  = cam.wrangleFloat(wrangler, 'focal', now, [50])[0]
        aperture = cam.wrangleFloat(wrangler, 'aperture', now, [41.4214])[0]

    zoom = getZoom(cam, proj, focal, aperture)
    cmd_property('camera', 'zoom', [zoom])

    if dof:
        plist = cam.wrangle(wrangler, camDofParms, now)
        focal = soho.houdiniUnitLength(plist['focal'].Value[0],
                                       plist['focalunits'].Value[0])
        cmd_property('camera', 'focus', plist['focus'].Value)
        cmd_property('camera', 'focal', [focal])
        cmd_property('camera', 'fstop', plist['fstop'].Value)

    window = getWindow(cam, wrangler, now)
    cmd_property('image', 'window', window)
    LSDhooks.call('post_outputCameraSegment', cam, wrangler, now,
                    cubemap, dof, proj)

def _patternMatching(all_obj, obj_regex, cage_regex, hires_regex, 
                    picture_regex, outobj_regex,
                    obj_list, cage_list, hires_list, picture_list, outobj_list):
    old = re.split('/', obj_regex)

    for obj in all_obj:

        obj_path = obj.getName()

        if re.match(fnmatch.translate(obj_regex), obj_path):
            obj_list.append(obj_path)
            new = re.split('/', obj_path)

            sub_list = []
            j = 1 
            for i in range(1, len(old)):
                string_list = []
                while (new[j] != old[i]):
                    string_list.append(new[j])
                    j = j + 1

                    if (j == len(new)):
                        break

                    if (i == len(old) -1): 
                        continue

                    if (re.match(fnmatch.translate(old[i+1]), new[j])): 
                        break

                if (len(string_list) > 0):
                    sub_list.append("/".join(string_list))
                else:
                    j = j + 1

            cage_path = cage_regex
            hires_path = hires_regex
            picture_path = picture_regex
            outobj_path = outobj_regex
            for sub in sub_list:
                cage_path = re.sub('\*', sub, cage_path, 1)
                hires_path = re.sub('\*', sub, hires_path, 1)
                picture_path = re.sub('\*', sub, picture_path, 1)
                outobj_path = re.sub('\*', sub, outobj_path, 1)

            cage_list.append(cage_path)
            hires_list.append(hires_path)
            picture_list.append(picture_path)
            outobj_list.append(outobj_path)

def outputCamera(cam, viewcam, now, fromlight, forphoton, cubemap):
    if LSDhooks.call('pre_outputCamera', cam, viewcam, now,
                    fromlight, forphoton, cubemap):
        return True
    times = LSDmisc.xform_mbsamples(cam, now)
    if fromlight:
        wrangler = LSDsettings.getWrangler(cam, now, 'light_wrangler')
    else:
        wrangler = LSDsettings.getWrangler(cam, now, 'camera_wrangler')
    if forphoton:
        cmd_photon()
    else:
        if fromlight:
            if not lightDisplay(wrangler, cam, now):
                return False
        elif cubemap:
            envmapDisplay(cam, now, cubemap)
        else:
            if not cameraDisplay(wrangler, cam, now):
                return False

    LSDsettings.outputGlobal(wrangler, cam, now)

    if forphoton:
        cmd_property('renderer', 'progressaction', ['Generating photon map'])
        # Set the default for the photon target
        target = cam.wrangleString(wrangler, 'lv_photontarget', now, ['*'])
        cmd_property('light', 'photontarget', target)
    elif fromlight and viewcam != None:
        cmd_property('renderer', 'progressaction', ['Generating point cloud'])
    elif fromlight:
        cmd_property('renderer', 'progressaction', ['Generating depth map'])
    elif cubemap:
        cmd_property('renderer', 'progressaction', ['Generating reflection map'])

    # Photon maps need to use the light ("cam" above) to generate global
    # settings, but should use the actual scene camera to indicate the
    # projection for dicing.  Change cameras here.
    if viewcam != None:
        cam = viewcam
        wrangler = LSDsettings.getWrangler(cam, now, 'camera_wrangler')

    res = cam.wrangleInt(wrangler, 'res', now, [256, 256])
    par = cam.wrangleFloat(wrangler, 'aspect', now, [1.0])
    if cubemap:
        par = [1.0]
    if not fromlight and not cubemap:
        if cam.wrangleInt(wrangler, 'override_camerares', now, [0])[0]:
            orgres = res
            orgpar = par
            frac = cam.wrangleString(wrangler, 'res_fraction', now, ['specific'])[0]
            try:
                ffrac = float(frac)
                if ffrac > 0.0001:
                    res = [max(int(ffrac*orgres[0]), 2),
                           max(int(ffrac*orgres[1]), 2)]
                else:
                    frac = 'specific'
            except:
                frac = 'specific'
            if frac == 'specific':
                res = cam.wrangleInt(wrangler, 'res_override', now, orgres)
                par = cam.wrangleFloat(wrangler, 'aspect_override', now, orgpar)

    uvrender = False
    if not fromlight and not forphoton:
        uvlist = cam.wrangle(wrangler, uvRenderParms, now)
        uvrender = uvlist['lv_isuvrendering'].Value[0]

    if uvrender:
        uvnum = uvlist['lv_uvobjects'].Value[0]
        mkpath = uvlist['lv_uvmkpath'].Value[0]
        res = uvlist['lv_uvunwrapres'].Value
        hidecage = uvlist['lv_uvhidecage'].Value[0]

        # Write object paths and image output paths to the LSD as JSON.
        obj_list = []
        cage_list = []
        hires_list = []
        picture_list = []
        outobj_list = []

        for o in range(1, uvnum+1):
            for p in uvObjectParms:
                uvObjectParms[p].setIndex(o)

            plist = cam.wrangle(wrangler, uvObjectParms, now)

            enable = plist['enable'].Value[0]
            if not enable:
                continue

            # object pattern matching
            obj_regex = plist['name'].Value[0]
            cage_regex = plist['cagelist'].Value[0]
            hires_regex = plist['hireslist'].Value[0]
            picture_regex = plist['picture'].Value[0]
            outobj_regex = plist['outobj'].Value[0]

            if (not obj_regex) or (not picture_regex):
                soho.error("Empty input for object/output path!")

            # warn if output picture is using a limited format.
            # we want to bake to multi-channel formats, if possible.
            if uvlist['lv_uvtype'].Value == 'udim':
                picture_sp = picture_regex.split(".")
                if not picture_sp[-1] in ['rat', 'pic', 'exr']:
                    picture_regex = '.'.join(picture_sp[:-1])+'.rat'
                    soho.warning("'Output Picture %d' using 8-bit image format. Switched to .RAT format " \
                        "to support all baking features. To output to 8-bit image formats select one from " \
                        "the 'Extract Image Format' menu."
                            % (o))

            all_obj = soho.objectList('objlist:instance')
            old_size = len(obj_list)

            # if object name contains special character, then wildcard matching
            # is automatically enabled
            if not re.match("^[a-zA-Z0-9_/]*$", obj_regex):
                _patternMatching(all_obj, obj_regex, cage_regex, hires_regex, picture_regex, outobj_regex,
                                obj_list, cage_list, hires_list, picture_list, outobj_list)

            else:
                for obj in all_obj:
                    if (obj_regex == obj.getName()):
                        obj_list.append(obj_regex)
                        cage_list.append(cage_regex)
                        hires_list.append(hires_regex)
                        picture_list.append(picture_regex)
                        outobj_list.append(outobj_regex)
                        break

            if len(obj_list) == old_size:
                soho.error("No match for object %d: '%s'" % (o, obj_regex))

        flipbook = cam.wrangleInt(wrangler, 'flipbook_output', now, [0])[0]
        if flipbook:
            picture_list = ["ip"] * len(picture_list)

        cmd_property('renderer', 'uvobjectlist',
                    [json.dumps({ 'objlist': obj_list })])
        cmd_property('renderer', 'uvcagelist',
                    [json.dumps({ 'cagelist': cage_list })])
        cmd_property('renderer', 'uvhireslist',
                    [json.dumps({ 'hireslist': hires_list })])
        cmd_property('renderer', 'uvpicturelist',
                    [json.dumps({ 'imglist': picture_list })])
        cmd_property('renderer', 'uvoutobjectlist',
                    [json.dumps({ 'outobjlist': outobj_list })])
        cmd_property('renderer', 'uvmkpath', [mkpath])

        if hidecage:
            # Hide all the low res cages that have high res versions
            for i in range(len(obj_list)):
                if hires_list[i]:
                    LSDsettings.hideUVObject(obj_list[i])
                # cage mesh (for sample dir) should not be renderable
                LSDsettings.hideUVObject(cage_list[i])

    cmd_property('image', 'resolution', res)
    cmd_property('image', 'pixelaspect', par)

    near   = cam.wrangleFloat(wrangler, 'near', now, [0.001])[0]
    far    = cam.wrangleFloat(wrangler, 'far', now, [1000])[0]
    if uvrender:
        near = 1e-6
        far = 1e6
    cmd_property('camera', 'clip', [near, far])

    vrrender = False
    vrmode = cam.wrangleInt(wrangler, 'vrlayout', now, [-1])[0]
    if vrmode != -1:
        vrrender = True

    if cubemap:
        proj = 'perspective'
    elif uvrender:
        proj = 'lens'
    elif vrrender:
        proj = 'lens'
    else:
        proj   = cam.wrangleString(wrangler, 'projection', now,
                                    ['perspective'])[0]
        if viewcam and proj == 'lens':
            # We switched cameras *after* outputting globals, which output
            # the lens shader on the light source.  We need to make sure to
            # output the proper lens shader here
            LSDsettings.outputLensShader(cam, wrangler, now)

    cmd_property('camera', 'projection', [proj])
    if uvrender:
        unwrap_method = uvlist['lv_uv_unwrap_method'].Value[0]
        cmd_bias = uvlist['lv_uv_cmd_bias'].Value[0]
        cmd_maxdist = uvlist['lv_uv_cmd_maxdist'].Value[0]
        flipnormal = uvlist['lv_uv_flip_normal'].Value[0]
        cmd_property('renderer', 'lensshader',
                     ['opdef:/Shop/v_uvlens unwrap_method %d cmd_bias %g cmd_maxdist %g flipnormal "%d"' \
                   % (unwrap_method, cmd_bias, cmd_maxdist, flipnormal)], False)

    if vrrender:
        vrlayout = cam.wrangleInt(wrangler, 'vrlayout', now, [0])[0]
        vrprojection = cam.wrangleInt(wrangler, 'vrprojection', now, [0])[0]
        vrswapleftright = cam.wrangleInt(wrangler, 'vrswapleftright', now, [0])[0]
        vrpreserveaspectratio = cam.wrangleInt(wrangler, 'vrpreserveaspectratio', now, [0])[0]
        vrmergemode = cam.wrangleInt(wrangler, 'vrmergemode', now, [0])[0]
        vrmergeangle = cam.wrangleFloat(wrangler, 'vrmergeangle', now, [0])[0]
        vrhorizontalfov = cam.wrangleFloat(wrangler, 'vrhorizontalfov', now, [0])[0]
        vrverticalfov = cam.wrangleFloat(wrangler, 'vrverticalfov', now, [0])[0]
        vrperspectivefov = cam.wrangleFloat(wrangler, 'vrperspectivefov', now, [0])[0]
        vrperspectiveclipnear = cam.wrangleFloat(wrangler, 'vrperspectiveclipnear', now, [0])[0]
        vrperspectiveclipfar = cam.wrangleFloat(wrangler, 'vrperspectiveclipfar', now, [0])[0]
        vrperspectivedistort = cam.wrangleFloat(wrangler, 'vrperspectivedistort', now, [0])[0]
        vrperspectivedistortcubic = cam.wrangleFloat(wrangler, 'vrperspectivedistortcubic', now, [0])[0]
        vrusestereoeye = cam.wrangleInt(wrangler, 'vrusestereoeye', now, [0])[0]
        vreyeseparation = cam.wrangleFloat(wrangler, 'vreyeseparation', now, [0])[0]
        vreyetoneckdistance = cam.wrangleFloat(wrangler, 'vreyetoneckdistance', now, [0])[0]
        cmd_property('renderer', 'lensshader',
                     ['opdef:/Shop/v_vrlens layout %d projection %d swapLeftRight %d preserveAspectRatio %d \
                     mergeMode %g mergeAngle %g \
                     horizontalFOV %g verticalFOV %g \
                     perspectiveFOV %g perspectiveClipNear %g perspectiveClipFar %g perspectiveDistort %g perspectiveDistortCubic %g \
                     useStereoEye %d eyeSeparation %g eyeToNeckDistance %g' \
                   % (vrlayout, vrprojection, vrswapleftright, vrpreserveaspectratio,
                   vrmergemode, vrmergeangle,
                   vrhorizontalfov, vrverticalfov,
                   vrperspectivefov, vrperspectiveclipnear, vrperspectiveclipfar, vrperspectivedistort, vrperspectivedistortcubic,
                   vrusestereoeye, vreyeseparation, vreyetoneckdistance)], False)

    dof = cam.wrangleInt(wrangler, 'lv_dof', now, [0])[0]

    crop = cam.getCameraCropWindow(wrangler, now)

    window = getWindow(cam, wrangler, now)
    cropmask = cam.objectList('objlist:cropmask', now)
    if cropmask is not None:
        crop_bounds = SOHOcommon.getObjectScreenBoundsFull(
            now, wrangler, cam, cropmask,
            LSDgeo.getObjectBounds, LSDmisc.xform_mbsamples, window=window)

        if crop_bounds is not None:
            crop[0] = max(crop[0], crop_bounds[0])
            crop[1] = min(crop[1], crop_bounds[1])
            crop[2] = max(crop[2], crop_bounds[2])
            crop[3] = min(crop[3], crop_bounds[3])

    overscan = cam.wrangleInt(wrangler, 'lv_overscan', now, [0, 0])
    if len(overscan) == 2 and (overscan[0] > 0 or overscan[1] > 0):
        crop[0] -= float(max(0, overscan[0])) / res[0]
        crop[1] += float(max(0, overscan[0])) / res[0]
        crop[2] -= float(max(0, overscan[1])) / res[1]
        crop[3] += float(max(0, overscan[1])) / res[1]

    if crop != [0,1,0,1]:
        cmd_property('image', 'crop', crop)

    # Tiled renders
    is_preview      = LSDmisc.isPreviewMode()
    plist           = cam.wrangle(wrangler, tilingParms, now)
    is_tiled        = plist['lv_tile_render'].Value[0]
    if is_tiled and not (is_preview or fromlight or forphoton):
        tile_count_x = plist['lv_tile_count_x'].Value[0]
        tile_count_y = plist['lv_tile_count_y'].Value[0]
        tile_index   = plist['lv_tile_index'].Value[0]
        tile_filename_suffix = plist['lv_tile_filename_suffix'].Value[0]
        cmd_property('image', 'tiledrenderindex', [tile_index])
        cmd_property('image', 'tiledrendercount', [tile_count_x, tile_count_y])
        cmd_property('image', 'tiledrendersuffix', [tile_filename_suffix])

    # MB Info
    if LSDmisc.CameraShutter:
        cmd_property('object', 'velocityscale', [LSDmisc.CameraShutter])
    if LSDmisc.CameraBlur:
        shutter_open = -LSDmisc.CameraDelta
        shutter_close = shutter_open + LSDmisc.CameraShutter
        cmd_declare('global', 'vector2', 'camera:shutter',
            [shutter_open * LSDmisc.FPS, shutter_close * LSDmisc.FPS] )
        LSDmisc.ouputMotionBlurInfo(cam,now,required=True)

    # Stereoscopy info
    plist           = cam.wrangle(wrangler, stereoParms, now)
    is_left_cam     = plist['lv_s3dleftcamera'].Value[0]
    is_right_cam    = plist['lv_s3drightcamera'].Value[0]
    if is_left_cam or is_right_cam:
        eye_list = ['', 'left', 'right', 'both']
        cmd_declare('global', 'string', 'camera:stereoeye',
            [eye_list[ is_left_cam + 2*is_right_cam]])

    objectTransform('space:world', cam, times, invert=True,
                    cubemap=cubemap)

    for time in times:
        cmd_start('segment')
        outputCameraSegment(cam, wrangler, time, cubemap, dof, proj)
        cmd_end()

    LSDhooks.call('post_outputCamera', cam, viewcam, now,
                    fromlight, forphoton, cubemap)
    return True

def isGeoLight(light, wrangler, now):
    ltype = light.wrangleString(wrangler, 'lv_areashape', now, [''])[0]
    return ltype == 'geo'

def outputLight(light, now):
    # Find the wrangler for evaluating soho parameters
    wrangler = LSDsettings.getWrangler(light, now, 'light_wrangler')
    if LSDhooks.call('pre_outputLight', wrangler, light, now):
        return

    times = LSDmisc.xform_mbsamples(light, now)

    cmd_start('light')

    objectTransform('space:world', light, times)

    if isGeoLight(light, wrangler, now):
        LSDgeo.instanceGeometry(light, now, times)

    LSDsettings.outputObject(light, now, wrangler=wrangler)
    LSDsettings.outputLight(wrangler, light, now)

    # Now, we need to output projection information so that NDC
    # coordinates can be computed (if required)
    proj = light.wrangleString(wrangler, 'projection', now, ['perspective'])[0]
    focal = light.wrangleFloat(wrangler, 'focal', now, [50])[0]
    aperture = light.wrangleFloat(wrangler, 'aperture', now, [41.4214])[0]
    orthowidth = light.wrangleFloat(wrangler, 'orthowidth', now, [2])[0]
    res = light.wrangleInt(wrangler, 'res', now, [512, 512])
    windowmask = light.objectList('objlist:windowmask', now)

    if res[0] <= 0 or res[1] <= 0:
        soho.error("Zero resolution for light '%s'" % light.getName())
    aspect = res[0] / float(res[1]);

    cmd_property('light', 'projection', [proj])

    zoom = getZoom(light, proj, focal, aperture)

    cmd_property('light', 'zoom', [zoom, zoom * aspect])

    if proj == 'ortho' or proj == 'orthographic':
        cmd_property('light', 'orthowidth', [orthowidth, orthowidth])

    if windowmask is not None:
        window_bounds = SOHOcommon.getObjectScreenBoundsFull(
            now, wrangler, light, windowmask,
            LSDgeo.getObjectBounds, LSDmisc.xform_mbsamples)

        if window_bounds is not None:
            window = [0.0,1.0,0.0,1.0]
            window[0] = max(window[0], window_bounds[0])
            window[1] = min(window[1], window_bounds[1])
            window[2] = max(window[2], window_bounds[2])
            window[3] = min(window[3], window_bounds[3])
            cmd_property('light', 'window', window)

    cmd_end()
    LSDhooks.call('post_outputLight', wrangler, light, now)

def outputFog(fog, now):
    if LSDhooks.call('pre_outputFog', fog, now):
        return
    times = LSDmisc.xform_mbsamples(fog, now)
    cmd_start('fog')
    objectTransform('space:world', fog, times)
    LSDsettings.outputFog(None, fog, now)
    cmd_end()
    LSDhooks.call('post_outputFog', fog, now)

def getVars(string):
    envvars = re.findall('\${?([a-zA-Z_0-9]+)', string)
    return (v for v in envvars if v in os.environ)

def getUsedEnvVars(hobj):
    envvars = set()
    children = hobj.allSubChildren()
    for child in children:
        for parm in child.parms():
            tpl = parm.parmTemplate()
            if tpl.dataType() != hou.parmData.String:
                continue
            keyframes = parm.keyframes()
            if keyframes:
                for keyframe in keyframes:
                    expr = keyframe.expression()
                    envvars.update(getVars(expr))

            else:
                value = parm.unexpandedString()
                envvars.update(getVars(value))

    return envvars

def expandEnvVars(envvars):
    for envvar in envvars:
        value = os.environ.get(envvar)
        # skip undefined or empty variables
        if value is None or len(value) == 0:
            continue
        # skip standard houdini variables like HFS, HH, etc.
        if value.startswith(os.environ['HFS']):
            continue
        cmd_if('"${}" == ""'.format(envvar))
        cmd_setenv(envvar, value)
        cmd_endif()

def engineinstance(obj, now, times, procedural, unload, requirelod, doorient,
                   boundsop):
    now = time.time()
    def error(msg):
        cmd_comment(msg)
        soho.error(msg)
        return False
    hobj = hou.node(obj.getName())
    if not hobj:
        return error('No HOM object "%s" for engine procedural' % obj.getName())
    if not hobj.displayNode():
        return error('No display SOP found for object %s - %s'
                    % (obj.getName(), 'needed for engine procedural'))
    # Now make an HDA using a unique name for the object type
    htype = hobj.type()
    hdadef = htype.definition()
    use_library_type = True
    unlock = False
    if not hdadef:
        use_library_type = False
    elif not hobj.matchesCurrentDefinition():
        use_library_type = False
    elif hdadef.hasSection('EditableNodes'):
        # Temporarily allow editing of contents so the HDA gets saved properly
        use_library_type = False
        unlock = True

    if use_library_type:
        # If we have a locked HDA that matches the current definition with no
        # editable nodes, we can use this HDA directly in the engine procedural.
        hdaname = htype.name()
    else:
        # In this case, the object is either an unlocked asset or an object
        # that isn't defined by an HDA.  We have to create an HDA for the
        # object and embed it into the LSD stream.
        hdaname = 'engine_' + str(uuid.uuid4()).replace('-', '')

        # save and load object to avoid recooking
        # creating a digital asset directly from hobj would trigger it's
        # caches to be invalidated, triggering a recook when displaying
        # the object in houdini after a render
        tmpdir = hou.getenv('HOUDINI_TEMP_DIR')
        fp, temphippath = tempfile.mkstemp(dir=tmpdir)
        temphippath = temphippath.replace("\\", "/")
        os.close(fp)
        hobj.parent().saveChildrenToFile(
            [hobj], [], temphippath)

        # create container and load contents
        tempparent = hou.node('/obj').createNode('subnet')
        for c in tempparent.children():
            c.destroy()
        tempparent.loadChildrenFromFile(temphippath, ignore_load_warnings=True)
        tempobj = tempparent.children()[0]
        os.remove(temphippath)

        tmpdir = hou.getenv('HOUDINI_TEMP_DIR')
        fp, hdapath = tempfile.mkstemp(dir=tmpdir)
        hdapath = temphippath.replace("\\", "/")
        os.close(fp)
        if unlock:
            tempobj.allowEditingOfContents()

        envvars = getUsedEnvVars(tempobj)
        expandEnvVars(envvars)

        tempobj.createDigitalAsset(hdaname,
            hdapath,
            'Mantra: Engine Generator Node',
            ignore_external_references=True,
            change_node_type=False,
            create_backup=False)
        tempparent.destroy()
        for d in hou.hda.definitionsInFile(hdapath):
            if d.nodeTypeName() == hdaname:
                d.setParmTemplateGroup(hobj.parmTemplateGroup(),
                                       create_backup=False)
                d.save(hdapath, create_backup=False)
                break
        else:
            return error('Error creating embedded engine procedural HDA for %s'
                            % obj.getName())
        data = base64.b64encode(open(hdapath, 'rb').read())

        # cleanup
        hou.hda.uninstallFile(hdapath)
        os.remove(hdapath)

        # write hda to ifd
        cmd_textblock(hdaname, data, encoding='b64')
        cmd_loadotl([hdaname], from_text_block=True)
        cmd_erase_textblock(hdaname)

    # Get the bounding box from the display SOP
    #bounds = hou.BoundingBox()
    #bounds.setTo((-1000,-1000,-1000, 1000, 1000, 1000))
    cmd_comment('Engine time for HDA: %.4f s.' % (time.time() - now))
    first = True
    for t in times:
        f = hou.timeToFrame(t)
        if boundsop == '':
            # Use display node if no bounding SOP is specified.
            g = hobj.displayNode().geometryAtFrame(f)
        else:
            boundsopnode = hobj.node(boundsop)
            if boundsopnode is None:
                return error('Bad Bounding Box SOP specified for engine object %s '
                             'at frame %g' % (obj.getName(), f))
            g = boundsopnode.geometryAtFrame(f)
        if not g:
            return error('Bad SOP geometry for engine object %s at frame %g' %
                         (obj.getName(), f))
        b = g.boundingBox()
        if first:
            bounds = b
            first = False
        else:
            bounds.enlargeToContain(b)
    bmin = bounds.minvec()
    bmax = bounds.maxvec()
    # Enlarge bounds for good measure
    d = max(bounds.sizevec()) * 0.01
    proc_args = [
        procedural,
        'objecthda',    '1',
        'size', '%g %g %g' % (bmax[0]-bmin[0]+d*2,
                                   bmax[1]-bmin[1]+d*2,
                                   bmax[2]-bmin[2]+d*2),
        'unload', '%d' % unload,
        'requirelod', '%d' % requirelod,
        'doorient', '%d' % doorient,
        'opname', hdaname,
    ]
    unload_parm = obj
    cmd_procedural([bmin[0]-d, bmin[1]-d, bmin[2]-d,
                    bmax[0]+d, bmax[1]+d, bmax[2]+d], [' '.join(proc_args)])
    cmd_comment('Engine Total Time: %.4f s.' % (time.time() - now))

def outputInstance(obj, now, check_renderable=False):
    # Define any materials used by object style sheet.
    (ss_bundles, ss_mats) = LSDsettings.getBundlesAndMaterialsFromObjectStyleSheet(obj, now)
    for mat_path in ss_mats:
        if hou.node(mat_path):
            LSDsettings.outputMaterial(mat_path, now)
    for bundle in ss_bundles:
        LSDsettings.outputBundle(bundle)

    if LSDgeo.isObjectFastPointInstancer(obj,now):
        outputPointInstance(obj,now)
        return

    if LSDhooks.call('pre_outputInstance', obj, now):
        return

    wrangler = LSDsettings.getWrangler(obj, now, 'object_wrangler')
    # Call the object wranglers 'instanceGeometry', skip inbuilt soho code if
    # it returns True. Note. It's the responsibility of object wrangler to
    # emit LSD correctly.
    if obj.wrangleInt(wrangler,'instanceGeometry', now, [0])[0] :
        LSDhooks.call('post_outputInstance', obj, now)
        return

    cmd_start('object')
    times = LSDmisc.xform_mbsamples(obj, now)
    objectTransform('space:world', obj, times)
    # TODO Shader space transform
    # Get the displacement bound from any materials applied to the
    # geometry.
    LSDgeo.instanceGeometry(obj, now, times)
    proc = obj.wrangleString(wrangler, 'lv_auto_engine_procedural', now, [''])[0]
    if proc:
        boundsop = obj.wrangleString(wrangler, 'lv_auto_engine_boundsop', now,[''])[0]
        unload = obj.wrangleInt(wrangler, 'lv_auto_engine_unload', now, [1])[0]
        lod = obj.wrangleInt(wrangler, 'lv_auto_engine_requirelod', now, [0])[0]
        orient = obj.wrangleInt(wrangler, 'lv_auto_engine_doorient', now,[1])[0]
        engineinstance(obj, now, times, proc,
                unload=unload,
                requirelod=lod,
                doorient=orient,
                boundsop=boundsop)
    # Now, output settings, with at least the displacement bound as
    # defined on the geometry
    LSDsettings.outputObject(obj, now, check_renderable=check_renderable)
    cmd_end()
    LSDhooks.call('post_outputInstance', obj, now)

def outputPointInstance(obj, now):
    if LSDhooks.call("pre_outputPointInstance", obj, now):
        return

    # Grab the SOP to instance
    def_inst_path = [None]
    obj.evalString('instancepath', now, def_inst_path)

    instancexform = [True]
    obj.evalInt('instancexform', now, instancexform)

    ptmotionblur = ['deform']
    obj.evalString('ptmotionblur', now, ptmotionblur)

    renderboxes = [False]
    obj.evalInt('renderboxes', now, renderboxes)

    # Grab the geometry and output the points
    (geo, npts, attrib_map) = LSDgeo.getInstancerAttributes(obj, now)
    if geo is None or npts is None:
        LSDhooks.call("post_outputPointInstance", obj, now)
        return

    # No point xform attribute? Treat as an empty object.
    if 'geo:pointxform' not in attrib_map:
        LSDhooks.call("post_outputPointInstance", obj, now)
        return

    # No objects associated with this instance
    if not def_inst_path[0] and \
       'instance' not in attrib_map and \
       'instancefile' not in attrib_map:
        soho.warning('No instance master and no instance attribute on %s. '
                     'Object will not be rendered' %
                     obj.getName())
        LSDhooks.call("post_outputPointInstance", obj, now)
        return

    # Collect the unique set of shaders and evaluate them before output.
    pt_material_shaders = {}

    if 'shop_materialpath' in attrib_map:
        LSDgeo.getPointInstanceSHOPs(
            pt_material_shaders, now, geo,
            'shop_materialpath', npts,
            shader_types=['shop_surfacepath', 'shop_displacepath', 'lv_matteshader'],
            override='material_override' )

    cmd_comment('Point instance object %s' % obj.getName() )

    # Spit out materials for all the unique SHOPs
    for shop_material_path_hash in pt_material_shaders:
        shop_material_path = shop_material_path_hash.split('-')[0]
        shop = soho.getObject(shop_material_path)
        cmd_start('material')
        LSDsettings.outputObject(shop, now, name=shop_material_path_hash, output_shader=False)

        shader_infos = pt_material_shaders[shop_material_path_hash]

        for shader_info_type in shader_infos:
            shader_info = shader_infos[shader_info_type]
            cmd_shader('object', LSDsettings.oshaderMap[shader_info_type], shader_info[0], shader_info[1])
        cmd_end()

    cmd_start( 'object' )
    times = LSDmisc.xform_mbsamples(obj, now)
    objectTransform('space:world', obj, times)

    cmd_declare('object', 'vector2', 'camera:shutter',
        LSDmisc.obj_shutter_open_close(obj, now))
    soho.indent()
    sys.stdout.write('cmd_procedural ptinstance ')
    if def_inst_path[0]:
        path = LSDmisc.absoluteObjectPath(obj, now, def_inst_path[0])
        sys.stdout.write( 'instancepath %s ' % path )
    sys.stdout.write( 'instancexform %d ' % instancexform[0] )
    sys.stdout.write( 'ptmotionblur %s ' % ptmotionblur[0] )
    if renderboxes[0]:
        sys.stdout.write( 'renderboxes 1 ' )
    sys.stdout.write( 'process_material 1\n')

    details = obj.getData("lv_details")
    LSDsettings.outputObject(obj, now)
    for d in details:
        cmd_geometry(d)

    cmd_end()

    LSDhooks.call("post_outputPointInstance", obj, now)

class ObjectInfo:
    obj = None
    check_renderable = False

    def __init__(self, obj, check_renderable):
        self.obj = obj
        self.check_renderable = check_renderable

def addCollectedInstance(obj, check_renderable, obj_list, coll):
    # Have we already output it?
    obj_name = obj.getName()
    if obj_name in coll:
        # See if the check_renderable flag needs to be set.
        if check_renderable:
            obj_list[coll[obj_name]].check_renderable = True
    else:
        coll[obj_name] = len(obj_list)
        obj_list.append(ObjectInfo(obj, check_renderable))


def collectInstances(obj, now, obj_list, coll):
    # Make sure that any instantiated objects marked unrenderable
    # are output
    proc = LSDgeo.getProcedural(obj, now)
    if proc:
        shop = proc[3]
        proc_obj_paths = shop.getDefaultedString('op:objpaths', now, [])
        for proc_obj_path in proc_obj_paths:
            proc_obj = soho.getObject(proc_obj_path)
            addCollectedInstance(proc_obj, True, obj_list, coll)
    elif LSDgeo.isObjectFastPointInstancer(obj,now):
        inst_paths = LSDgeo.getInstantiatedObjects(obj, now)
        for inst_path in inst_paths:
            all_obj = soho.objectList('objlist:instance')
            inst_obj = soho.getObject( inst_path )
            if inst_obj:
                # Issue a warning for any objects that instantiate a procedural
                if LSDgeo.getProcedural(inst_obj, now):
                    soho.error( "Fast point instancing on object '%s' cannot instantiate "
                                "procedural on object '%s'." % (inst_obj.getName(), inst_path))
                    continue

                addCollectedInstance(inst_obj, True, obj_list, coll)

    addCollectedInstance(obj, False, obj_list, coll)

def outputSpace(obj, now):
    if LSDhooks.call('pre_outputSpace', obj, now):
        return
    times = LSDmisc.xform_mbsamples(obj, now)
    cmd_start('object')
    cmd_property('object', 'name', [obj.getName()])
    objectTransform('space:world', obj, times)
    cmd_end()
    LSDhooks.call('post_outputSpace', obj, now)

def outputObjects(now, objlist, lightlist, spacelist, foglist,
                  skipobject, skiplight):

    if LSDhooks.call('pre_outputObjects', now, objlist, lightlist, spacelist,
            foglist, skipobject, skiplight):
        return

    LSDhooks.call('pre_outputSpaceList', now, spacelist)

    done = False
    for space in spacelist:
        outputSpace(space, now)
        done = True
    if done:
        cmd_comment(None)

    LSDhooks.call('post_outputSpaceList', now, spacelist)

    LSDhooks.call('pre_outputLightList', now, lightlist, skiplight)

    done = False
    for light in lightlist:
        if light.Object != skiplight:
            outputLight(light, now)
            done = True
    if done:
        cmd_comment(None)

    LSDhooks.call('post_outputSpaceList', now, lightlist, skiplight)

    LSDhooks.call('pre_outputFogList', now, foglist)

    done = False
    for fog in foglist:
        outputFog(fog, now)
        done = True
    if done:
        cmd_comment(None)

    LSDhooks.call('post_outputFogList', now, foglist)

    LSDhooks.call('pre_outputInstanceList', now, objlist, skipobject)

    done = False
    depobjlist = []
    coll = {}
    for obj in objlist:
        if obj.Object != skipobject:
            collectInstances(obj, now, depobjlist, coll)

    for objinfo in depobjlist:
        outputInstance(objinfo.obj, now, objinfo.check_renderable)
    if done:
        cmd_comment(None)

    LSDhooks.call('post_outputInstanceList', now, objlist, skipobject)

    LSDhooks.call('post_outputObjects', now, objlist, lightlist, spacelist,
            foglist, skipobject, skiplight)

def saveRetained(now, objlist, lightlist):

    if LSDhooks.call('pre_saveRetained', now, objlist, lightlist):
        return

    cmd_comment('Retained geometry')
    for obj in objlist:
        if LSDgeo.isObjectFastPointInstancer(obj,now):
            inst_paths = LSDgeo.getInstantiatedObjects(obj, now)
            for inst_path in inst_paths:
                inst_obj = soho.getObject( inst_path )
                if inst_obj:
                    mbinfo = LSDmisc.geo_mbsamples(inst_obj, now)
                    LSDgeo.saveRetained(inst_obj, now, mbinfo[0], mbinfo[1], mbinfo[2], mbinfo[3])

        mbinfo = LSDmisc.geo_mbsamples(obj, now)
        LSDgeo.saveRetained(obj, now, mbinfo[0], mbinfo[1], mbinfo[2], mbinfo[3])
    for light in lightlist:
        wrangler = LSDsettings.getWrangler(light, now, 'light_wrangler')
        if isGeoLight(light, wrangler, now):
            mbinfo = LSDmisc.geo_mbsamples(light, now)
            LSDgeo.saveRetained(light, now, mbinfo[0], mbinfo[1], mbinfo[2], mbinfo[3])
    cmd_comment(None)

    LSDhooks.call('post_saveRetained', now, objlist, lightlist)

# This operation will produce the block containing the camera, imager and
# global variables.
def renderCamera(cam, now,
                 fromlight=False, forphoton=False, cubemap=None,
                 viewcam=None):
    cmd_time(now)

    wrangler = LSDsettings.getWrangler(cam, now, 'camera_wrangler')
    uvrender = False
    uvlist = cam.wrangle(wrangler, uvRenderParms, now)
    if not fromlight and not forphoton:
        uvrender = uvlist['lv_isuvrendering'].Value[0]

    # Declare baking parms
    LSDsettings.declareBakingParms(now, False)

    # Declare uv baking parms required by shaders
    for pname in ('lv_uv_flip_normal', 'lv_uv_unwrap_method'):
        parm = uvlist[pname]
        if parm.Value == parm.Default:
            continue
        cmd_declare('global', parm.Type, 'global:%s' % re.sub('^lv_', '', pname), parm.Value)

    type = 'unknown'
    label = [cam.getName()]
    if uvrender:
        type = 'uvrender'
        label.append(type)
    if fromlight:
        type = 'shadow'
        label.append(type)
    if forphoton:
        type = 'photon'
        label.append(type)
    if cubemap:
        type = 'envmap'
        label.append(type)
    if len(label) == 1:
        type = 'beauty'
        label.append(type)
    label = '.'.join(label)

    cmd_property('renderer', 'rendertype', [type])
    cmd_property('renderer', 'renderlabel', [label])

    LSDmisc.setCameraBlur(cam, now)
    if not outputCamera(cam, viewcam, now, fromlight, forphoton, cubemap):
        cmd_comment('Error evaluating camera parameters: %s' % cam.getName())
        return False
    return True

# Output all objects in the scene
def renderObjects(now, objlist, lightlist, spacelist, foglist,
                  skipobject=-1, skiplight=-1):

    if LSDhooks.call('pre_renderObjects', now, objlist, lightlist, spacelist,
            foglist, skipobject, skiplight):
        return

    _OutputObjects = set()
    outputObjects(now, objlist, lightlist, spacelist, foglist,
                  skipobject, skiplight)

    LSDhooks.call('post_renderObjects', now, objlist, lightlist, spacelist,
            foglist, skipobject, skiplight)

    cmd_raytrace()

# A full render
# viewcam can be provided to indicate that a different camera should be
# used to create the camera viewing transform from the one used to produce
# settings.
def render(cam, now, objlist, lightlist, spacelist, foglist,
                     fromlight=False, forphoton=False, cubemap=None,
                     viewcam=None, skiplight=-1):
    if LSDhooks.call('pre_render', cam, now, objlist, lightlist, spacelist,
            foglist, fromlight, forphoton, cubemap, viewcam):
        return

    if renderCamera(cam, now, fromlight, forphoton, cubemap, viewcam):
        skipobject = -1
        if cubemap:
            # When rendering a cube-environment, we don't want to include
            # the object at the center of the environment
            skipobject = cubemap.Object.Object   # Object handle
        renderObjects(now, objlist, lightlist, spacelist, foglist,
                      skipobject, skiplight)
    LSDhooks.call('post_render', cam, now, objlist, lightlist, spacelist,
            foglist, fromlight, forphoton, cubemap, viewcam)

