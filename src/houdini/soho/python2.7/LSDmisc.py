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
# NAME:         LSDmisc.py ( Python )
#
# COMMENTS:     Misc functions used for LSD generation
#

import os, sys, time
import soho, hou
import LSDsettings
import LSDhooks
import LSDgeo
from LSDapi import *
from soho import SohoParm
import posixpath

Feature = LSDsettings.getFeature
FPS = 24
FPSinv = 1.0 / FPS
SequenceNumber  = 0
SequenceLength  = 1
CameraBlur      = False
CameraShutter   = 0
CameraShutterF  = 0             # Frame based shutter
CameraDelta     = 0
CameraStyle     = 'trailing'
InlineGeoDefault = [0]          # By default, outline geometry

# State information for saving external resources
PipeStream      = True          # Whether SOHO is writing to a pipe
ExternalSessionId = ''          # A string based on the frame number/hip file
ExternalSharedSessionId = ''    # A string based on the hip file alone
TmpSharedStorage = ''        # Directory used to store side-car files
TmpLocalStorage = ''     # Directory used to store piped temporary files

headerParms = {
    "ropname"           : SohoParm("object:name", "string", key="ropname"),
    "hip"               : SohoParm("$HIP",        "string", key="hip"),
    "hipname"           : SohoParm("$HIPNAME",    "string", key="hipname"),
    "seqnumber"         : SohoParm("state:sequencenumber", "int",
                                        key="seqnumber"),
    "seqlength"         : SohoParm("state:sequencelength", "int",
                                        key="seqlength"),

    "soho_program"      : SohoParm("soho_program", "string"),
    "hver"              : SohoParm("state:houdiniversion", "string", ["9.0"], False, key="hver"),
    "lv_hippath"        : SohoParm("lv_hippath", "string", key="lv_hippath"),
    "lv_verbose"        : SohoParm("lv_verbose", "int"),

    "houdinipid"        : SohoParm("soho:houdinipid", "int", key="houdinipid"),
    "pipepid"           : SohoParm("soho:pipepid", "int", key="pipepid"),
    "pipestream"        : SohoParm("soho:pipestream", "int", key="pipestream"),
    "tmpsharedstorage"  : SohoParm("lv_tmpsharedstorage", "string",
                                    key="tmpsharedstorage"),
    "tmplocalstorage"   : SohoParm("lv_tmplocalstorage", "string",
                                    key="tmplocalstorage"),
}

configParms = {
    "lv_vtoff"                  : SohoParm("lv_vtoff", "bool", key="lv_vtoff", skipdefault=False),
    "lv_fconv"                  : SohoParm("lv_fconv", "bool", key="lv_fconv", skipdefault=False),
    "lv_async_geo"              : SohoParm("lv_async_geo", "bool", key="lv_async_geo", skipdefault=False),
    "lv_async_vtex"             : SohoParm("lv_async_vtex", "bool", key="lv_async_vtex", skipdefault=False),
    "lv_cull_mode"              : SohoParm("lv_cull_mode", "string", key="lv_cull_mode", skipdefault=False),
    "lv_vtex_conv_quality"      : SohoParm("lv_vtex_conv_quality", "string", key="lv_vtex_conv_quality", skipdefault=False),
    "lv_vtex_tlc"               : SohoParm("lv_vtex_tlc", "string", key="lv_vtex_tlc", skipdefault=False),
    "lv_vtex_tlc_level"         : SohoParm("lv_vtex_tlc_level", "int", key="lv_vtex_tlc_level", skipdefault=False),

    "lv_geo_tangent_generation" : SohoParm("lv_geo_tangent_generation", "string", key="lv_geo_tangent_generation", skipdefault=False),
}

objXformMotion = [
    SohoParm('xform_motionsamples',     'int', [2], False),
]

objGeoMotion = [
    SohoParm('geo_motionsamples',       'int', [1], False),
    SohoParm('geo_velocityblur',        'int', [0], False),
	SohoParm('geo_accelattribute',		'string', ["accel"], False),
]


def fullFilePath(file):
    path = sys.path
    for dir in path:
        full = os.path.join(dir, file)
        try:
            if os.stat(full):
                return full
        except:
            pass
    return file

def absoluteObjectPath(obj_rel_to, now, path):
    if posixpath.isabs(path):
        return path
    rel_to = obj_rel_to.getDefaultedString("object:name", now, [''])[0]
    return posixpath.normpath(posixpath.join(rel_to, path))
        
def header(now, propdefs):
    global      PipeStream, TmpSharedStorage, TmpLocalStorage
    global      ExternalSessionId, ExternalSharedSessionId
    global      SequenceNumber, SequenceLength
    global      InlineGeoDefault

    LSDhooks.call('pre_header', now)

    rop = soho.getOutputDriver()
    plist = rop.evaluate(headerParms, now)
    hver = plist["hver"].Value[0]
    cmd_comment("LSD created by Houdini Version: %s" % hver)
    cmd_comment("Generation Time: %s" % time.strftime("%b %d, %Y at %H:%M:%S"))
    soho_program = plist.get('soho_program', None)
    target  = LSDsettings.theVersion
    hip     = plist.get('hip', None)
    hipname = plist.get('hipname', None)
    ropname = plist.get('ropname', None)
    tmpsharedstorage = plist.get('tmpsharedstorage', None)
    tmplocalstorage = plist.get('tmplocalstorage', None)
    houdinipid = plist['houdinipid'].Value[0]
    pipepid = plist['pipepid'].Value[0]
    pipestream = plist.get('pipestream', None)
    seqnumber = plist.get('seqnumber', None)
    seqlength = plist.get('seqlength', None)
    if seqnumber:
        # The sequence number is the one-based number in the frame sequence
        # being rendered.
        SequenceNumber = seqnumber.Value[0]
    if seqlength:
        # Number of frames in the sequence being rendered
        SequenceLength = seqlength.Value[0]
    if soho_program:
        cmd_comment("    Soho Script: %s"%fullFilePath(soho_program.Value[0]))
    if target:
        cmd_comment("  Render Target: %s" % target)
    defs = LSDsettings.SettingDefs
    if len(defs):
        cmd_comment("    Render Defs: %s" % defs[0])
        for i in range(1, len(defs)):
            cmd_comment("               : %s" % defs[i])
    if hip and hipname:
        cmd_comment("       HIP File: %s/%s, $T=%g, $FPS=%g" % (hip.Value[0], hipname.Value[0], now, FPS))
    
    if ropname:
        cmd_comment("  Output driver: %s" % ropname.Value[0])

    cmd_version(hver)
    if propdefs:
        # Output property defaults before we output any other settings
        cmd_defaults(propdefs)

    # Renderer configuation
    cfg_plist = rop.evaluate(configParms, now)
    cmd_comment("Renderer configuration")
    vtoff = cfg_plist.get('lv_vtoff', None)
    cmd_declare_parm('global', 'vtoff', vtoff)

    fconv = cfg_plist.get('lv_fconv', None)
    cmd_config('fconv', fconv)

    async_geo = cfg_plist.get('lv_async_geo', None)
    cmd_declare_parm('global', 'async_geo', async_geo)

    async_vtex = cfg_plist.get('lv_async_vtex', None)
    cmd_config('async_vtex', async_vtex)

    cull_mode = cfg_plist.get('lv_cull_mode', None)
    cmd_config('cull_mode', cull_mode)

    vtex_conv_quality = cfg_plist.get('lv_vtex_conv_quality', None)
    cmd_config('vtex_conv_quality', vtex_conv_quality)

    vtex_tlc = cfg_plist.get('lv_vtex_tlc', None)
    cmd_config('vtex_tlc', vtex_tlc)

    vtex_tlc_level = cfg_plist.get('lv_vtex_tlc_level', None)
    cmd_config('vtex_tlc_level', vtex_tlc_level)

    geo_tangent_generation = cfg_plist.get('lv_geo_tangent_generation', None)
    cmd_config('geo_tangent_generation', geo_tangent_generation)

    cmd_comment(None)
    cmd_declare('global', 'float', 'global:fps', [FPS])
    
    # TODO: do we really need this !?
    #cmd_hscript('fps %g; tcur %g' % (FPS, now))

    verbose = plist.get('lv_verbose', None)
    if verbose:
        cmd_property('renderer', 'verbose', verbose.Value);

    cmd_comment(None)

    if not pipestream:
        cmd_comment('Unable to determine whether writing to a pipe')
        PipeStream = True
    else:
        PipeStream = (pipestream.Value[0] != 0)
    
    if hip:
        hipvar = hip.Value[0]
    else:
        hipvar = ""
    hipvar = rop.getDefaultedString('lv_hippath', now, [hipvar])[0]
    # Setting the HIP variable in a conditional so users can override
    # the HIP variable in the .ifd file.
    cmd_setenv('HIP', '$HIP_OVERRIDE')
    cmd_if('"$HIP" == ""')
    cmd_setenv('HIP', hipvar)
    cmd_endif()

    def isValidTempDir(path):
        # Make sure we can either create the sub-directories of the path, or
        # that the path itself is writeable.
        if os.path.isdir(path):
            return os.access(path, os.W_OK)
        head, tail = os.path.split(path)
        return isValidTempDir(head) if head else False

    if not tmpsharedstorage:
        # We still need to create a storage path, so put it in the HIP
        # directory.  This is where assets will be saved for the LSD.
        tmpsharedstorage = hou.expandString("$HIP/ifds/storage")
    else:
        tmpsharedstorage = tmpsharedstorage.Value[0]
    if not isValidTempDir(tmpsharedstorage):
        soho.warning("Path specified by lv_tmpsharedstorage is read-only.  %s" % (
                    "this may cause issues with non-inline geometry"))
        tmpsharedstorage = hou.expandString("$HOUDINI_TEMP_DIR/ifds/storage")

    if not tmplocalstorage:
        # We still need to create a storage path, so put it in the
        # HOUDINI_TEMP_DIR directory.  This is where assets will be saved for
        # the LSD.
        tmplocalstorage = hou.expandString("$HOUDINI_TEMP_DIR/ifds/storage")
    else:
        tmplocalstorage = tmplocalstorage.Value[0]


    # Set the external storage variable set into a conditional so if the
    # variable is set before mantra is run, that value will be used instead.
    cmd_if('"$_TMP_SHARED_STORAGE" == ""')
    cmd_setenv('_TMP_SHARED_STORAGE', tmpsharedstorage)
    cmd_endif()
    cmd_if('"$_TMP_LOCAL_STORAGE" == ""')
    cmd_setenv('_TMP_LOCAL_STORAGE', tmplocalstorage)
    cmd_endif()
    TmpSharedStorage = tmpsharedstorage # Stash for later
    TmpLocalStorage = tmplocalstorage   # Stash for later

    frame = now * FPS + 1
    frame_frac = int((frame%1) * 100)
    if PipeStream:
        ExternalSessionId = '%d_%s.%d_%03d' % (
                pipepid, hipname.Value[0], int(frame), frame_frac)
        ExternalSharedSessionId = '%d_%s_shared' % (
                houdinipid, hipname.Value[0])
    else:
        ExternalSessionId = '%s.%d_%03d' % (
                hipname.Value[0], int(frame), frame_frac)
        ExternalSharedSessionId = '%s_shared' % (hipname.Value[0])

    if os.getenv('MANTRA_DEBUG_INLINE_STORAGE'):
        # Normally, we want to rely on the lv_inlinestorage parameter (which
        # defaults to False).  For convenience we can set this variable to
        # override the setting.  Note that this may impact performance since
        # Houdini and mantra are able to multi-thread saving/loading
        # geometry.  This should be used for debugging only.
        InlineGeoDefault = [1]

    LSDhooks.call('post_header', now)

def _getBlur(obj, now, shutter=.5, offset=None, style='trailing', allow=1):
    allow = obj.getDefaultedInt('allowmotionblur', now, [allow])[0]
    shadowtype = obj.getDefaultedString('shadow_type', now, ['off'])[0]
    shutter = obj.getDefaultedFloat('shutter', now, [shutter])[0]*FPSinv
    offset = obj.getDefaultedFloat('shutteroffset', now, [offset])[0]
    style   = obj.getDefaultedString('motionstyle', now, [style])[0]
    if style == 'centered':
        delta = shutter * .5
    elif style == 'leading':
        delta = shutter
    else:
        delta = 0
    if offset is not None:
        # shutterOffset maps -1 to leading blur 0 to center, and +1 to trailing
        delta -= (offset - 1) * 0.5 * shutter
   
    if shadowtype == 'depthmap' and not isPreviewMode():
        shadowblur = obj.getDefaultedInt('shadowmotionblur', now, [1])[0]
        if shadowblur == False:
            allow = False
    if shutter == 0:
        allow = False
    return allow, delta, shutter, offset, style

def getSharedStoragePath():
    global TmpSharedStorage
    if not os.path.isdir(TmpSharedStorage):
        umask = os.umask(0)
        try:
            os.makedirs(TmpSharedStorage)
        except:
            TmpSharedStorage =hou.expandString('$HOUDINI_TEMP_DIR/ifds/storage')
            if not os.path.isdir(TmpSharedStorage):
                os.makedirs(TmpSharedStorage)
        os.umask(umask)
    return TmpSharedStorage

def getLocalStoragePath():
    global TmpLocalStorage
    if not os.path.isdir(TmpLocalStorage):
        umask = os.umask(0)
        try:
            os.makedirs(TmpLocalStorage)
        except:
            TmpLocalStorage = hou.expandString('$HOUDINI_TEMP_DIR/ifds/storage')
            if not os.path.isdir(TmpLocalStorage):
                os.makedirs(TmpLocalStorage)
        os.umask(umask)
    return TmpLocalStorage
    
def initializeMotionBlur(cam, now):
    #
    # Initialize motion blur settings from the main camera.
    #
    global      CameraShutter, CameraShutterOffset, CameraShutterF
    global      CameraStyle, CameraDelta, CameraBlur
    global      FPS, FPSinv

    LSDhooks.call('pre_initialize', cam, now)

    FPS = soho.getDefaultedFloat('state:fps', [24])[0]
    FPSinv = 1.0 / FPS

    CameraBlur,CameraDelta,CameraShutter,CameraShutterOffset,CameraStyle = \
        _getBlur(cam, now, shutter=.5, offset=None, style='trailing', allow=0)
    CameraShutterF = CameraShutter*FPS

def ouputMotionBlurInfo(obj,now,required=False):
    motionInfo = {
        'xform' : SohoParm('xform_motionsamples', 'int', [2], not required, key='xform'),
        'geo'   : SohoParm('geo_motionsamples',   'int', [1], not required, key='geo')
    }
    
    # Write out the number of transform and geometry motion samples
    # if motion blur is enabled.
    if CameraBlur:
        plist = obj.evaluate(motionInfo, now)
        xform = plist.get('xform', None)
        geo = plist.get('geo', None)
        nseg = xform.Value[0] if xform else 1
        if nseg > 1:
            cmd_property('object', 'xformsamples', [nseg])
        nseg = geo.Value[0] if geo else 1
        if nseg > 1:
            cmd_property('object', 'geosamples', [nseg])

def setCameraBlur(cam, now):
    #
    # For each frame rendered, the camera may disable motion blur
    #
    global      CameraBlur
    CameraBlur,delta,shutter,offset,style = _getBlur(cam, now,
                                            shutter=CameraShutterF,
                                            offset=CameraShutterOffset,
                                            style=CameraStyle,
                                            allow=0)

def _fillTime(now, nseg, delta, shutter):
    t0 = now - delta
    t1 = t0 + shutter
    times = []
    tinc = (t1 - t0)/float(nseg-1)
    for i in range(nseg):
        times.append(t0)
        t0 += tinc
    return times

def xform_mbsamples(obj, now):
    times = [now]
    if CameraBlur:
        allowmb,delta,shutter,offset,style = _getBlur(obj, now,
                                                shutter=CameraShutterF,
                                                offset=CameraShutterOffset,
                                                style=CameraStyle,
                                                allow=1)
        if allowmb:
            plist = obj.evaluate(objXformMotion, now)
            nseg  = plist[0].Value[0]
            if allowmb and nseg > 1:
                times = _fillTime(now, nseg, delta, shutter)
    return times

def obj_shutter_open_close(obj, now):
    times = [0, 0]
    if CameraBlur:
        allowmb,delta,shutter,offset,style = _getBlur(obj, now,
                                                shutter=CameraShutterF,
                                                offset=CameraShutterOffset,
                                                style=CameraStyle,
                                                allow=1)
        if allowmb:
            times = [-delta*FPS, (-delta+shutter)*FPS]
    return times

def geo_mbsamples(obj, now):
    times = [now]
    vblur = False
    accel_attrib = ""
    nseg = 0
    if CameraBlur:
        allowmb,delta,shutter,offset,style = _getBlur(obj, now,
                                                shutter=CameraShutterF,
                                                offset=CameraShutterOffset,
                                                style=CameraStyle,
                                                allow=1)
        if allowmb:
            plist = obj.evaluate(objGeoMotion, now)
            nseg  = plist[0].Value[0]
            vblur = plist[1].Value[0]
            accel_attrib = plist[2].Value[0]
            if vblur:
                if vblur > 1:	# Acceleration blur
                    nseg = max(2, nseg)
                else:           # Plain velocity blur
                    nseg = 2
                    accel_attrib = ""
                times = _fillTime(now, 2, delta, shutter)
            elif nseg > 1:
                times = _fillTime(now, nseg, delta, shutter)
    return (times, vblur, accel_attrib, nseg)

def objecthandle(obj, seg=0):
    name = "lv_handle%d" % seg
    handle = obj.getData(name)
    if handle == None:
        if seg != 0:
            handle = '"%s-%d"' % (obj.getName(), seg)
        else:
            handle = '"%s"' % obj.getName()
        obj.storeData(name, handle)
    return handle

def isPreviewMode():
    mode = soho.getDefaultedString('state:previewmode', ['default'])[0]
    return mode != 'default'
