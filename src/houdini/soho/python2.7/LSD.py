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
# NAME:         mantra.py ( Python )
#
# COMMENTS:     LSD generation using SOHO
#

import time
import soho
import hou
import LSDapi
import LSDmisc
import LSDframe
import LSDgeo
import LSDsettings
import LSDhooks
from soho import SohoParm

reload(LSDapi)
reload(LSDmisc)
reload(LSDframe)
reload(LSDgeo)
reload(LSDsettings)
reload(LSDhooks)

from LSDapi import *


LSDhooks.call("pre_lsdGen")

clockstart = time.time()

controlParameters = {
    # The time at which the scene is being rendered
    'now'     : SohoParm('state:time', 'real',  [0], False,  key='now'),

    # The mode (default, rerender generate, rerender update)
    'mode'    : SohoParm('state:previewmode', 'string', ['default'], False, key='mode'),

    # A string with names of style sheets changed since the last IPR update.
    'dirtystylesheets' : SohoParm('state:dirtystylesheets', 'string', [''], False, key='dirtystylesheets'),

    # A string with names of bundles changed since the last IPR update.
    'dirtybundles' : SohoParm('state:dirtybundles', 'string', [''], False, key='dirtybundles'),

    # The camera (or list of cameras), and masks for visible objects,
    # active lights and visible fog objects
    'camera'  : SohoParm('camera', 'string', ['/obj/cam1'], False),

    # Whether to generate:
    #   Shadow maps for the selected lights
    #   Environment maps for the selected objects
    #   The main image from the camera
    #   A PBR render target
    'shadow'  : SohoParm('render_any_shadowmap','int', [1], False,key='shadow'),
    'env'     : SohoParm('render_any_envmap',   'int', [1], False,key='env'),
    'photon'  : SohoParm('render_any_photonmap', 'int', [1], False, key='photon'),
    'pointcloud'  : SohoParm('render_any_pointcloud', 'int', [1], False, key='pointcloud'),
    'main'    : SohoParm('render_viewcamera','int', [1], False, key='main'),
    'decl'    : SohoParm('declare_all_shops', 'int', [1], False, key='decl'),
    'engine'  : SohoParm('lv_renderengine',  'string', ['micropoly'],
                                            False, key='engine'),

    'lv_inheritproperties' : SohoParm('lv_inheritproperties', 'int', [0], False),

    'lv_embedvex' :SohoParm('lv_embedvex',  'int', [0], False, key='embedvex'),
    'lv_quickexit':SohoParm('lv_quickexit', 'int', [1], False),
    'lv_numpathmap':SohoParm('lv_numpathmap', 'int', [0], False),
    'lv_isuvrendering':SohoParm('lv_isuvrendering', 'bool', [False], False),
    'lv_defaults' : SohoParm('lv_defaults', 'string',
                            ['RenderProperties.json'], False),
}

parmlist = soho.evaluate(controlParameters)

now = parmlist['now'].Value[0]
mode = parmlist['mode'].Value[0]
dirtystylesheets = parmlist['dirtystylesheets'].Value[0]
dirtybundles = parmlist['dirtybundles'].Value[0]
camera  = parmlist['camera'].Value[0]
quickexit = parmlist['lv_quickexit'].Value[0]
LSDapi.ForceEmbedVex = parmlist['embedvex'].Value[0]
decl_shops = parmlist['decl'].Value[0]
numpathmap = parmlist['lv_numpathmap'].Value[0]
uvrender = parmlist['lv_isuvrendering'].Value[0]
propdefs = parmlist['lv_defaults'].Value[0]

if mode != 'default':
    # Don't allow for nested evaluation in IPR mode
    inheritedproperties = False
else:
    inheritedproperties = parmlist['lv_inheritproperties'].Value[0]

options = {}
if inheritedproperties:
    # Turn off object->output driver inheritance
    options['state:inheritance'] = '-rop'
if propdefs and propdefs != 'stdin':
    options['defaults_file'] = propdefs

if not soho.initialize(now, camera, options):
    # If we're UV rendering and there's no camera, create one.
    if uvrender:
        camera = 'ipr_camera'
        if not soho.initialize(now, camera, options):
            soho.error("Unable to initialize UV rendering module with a camera")
    else:
        soho.error("Unable to initialize rendering module with given camera")

#
# Add objects to the scene, we check for parameters on the viewing
# camera.  If the parameters don't exist there, they will be picked up
# by the output driver.
#
objectSelection = {
    # Candidate object selection
    'vobject'     : SohoParm('vobject', 'string',       ['*'], False),
    'alights'     : SohoParm('alights', 'string',       ['*'], False),
    'vfog'        : SohoParm('vfog',    'string',       ['*'], False),

    'forceobject' : SohoParm('forceobject',     'string',       [''], False),
    'forcelights' : SohoParm('forcelights',     'string',       [''], False),
    'forcefog'    : SohoParm('forcefog',        'string',       [''], False),

    'excludeobject' : SohoParm('excludeobject', 'string',       [''], False),
    'excludelights' : SohoParm('excludelights', 'string',       [''], False),
    'excludefog'    : SohoParm('excludefog',    'string',       [''], False),

    'matte_objects'   : SohoParm('matte_objects', 'string',     [''], False),
    'phantom_objects' : SohoParm('phantom_objects', 'string',   [''], False),

    'sololight'     : SohoParm('sololight',     'string',       [''], False),

    'lv_cameralist' : SohoParm('lv_cameralist', 'string',       [''], False),
}

for cam in soho.objectList('objlist:camera'):
    break
else:
    soho.error("Unable to find viewing camera for render")

objparms = cam.evaluate(objectSelection, now)
stdobject = objparms['vobject'].Value[0]
stdlights = objparms['alights'].Value[0]
stdfog = objparms['vfog'].Value[0]
forceobject = objparms['forceobject'].Value[0]
forcelights = objparms['forcelights'].Value[0]
forcefog = objparms['forcefog'].Value[0]
excludeobject = objparms['excludeobject'].Value[0]
excludelights = objparms['excludelights'].Value[0]
excludefog = objparms['excludefog'].Value[0]
sololight = objparms['sololight'].Value[0]
matte_objects = objparms['matte_objects'].Value[0]
phantom_objects = objparms['phantom_objects'].Value[0]
forcelightsparm = 'forcelights'
if sololight:
    stdlights = excludelights = ''
    forcelights = sololight
    forcelightsparm = 'sololight'

# Obtain the list of cameras through which we need to render. The main camera
# may specify a few sub-cameras, for example, in the stereo camera case.
camera_paths = objparms['lv_cameralist'].Value[0].split()
camera_list  = []
for cam_path in camera_paths:
    camera_list.append( soho.getObject( cam_path ))
if len( camera_list ) == 0:
    cam.storeData('NoFileSuffix', True)
    camera_list.append( cam )

# First, we add objects based on their display flags or dimmer values
soho.addObjects(now, stdobject, stdlights, stdfog, True,
    geo_parm='vobject', light_parm='alights', fog_parm='vfog')
soho.addObjects(now, forceobject, forcelights, forcefog, False,
    geo_parm='forceobject', light_parm=forcelightsparm, fog_parm='forcefog')

# Force matte & phantom objects to be visible too
if matte_objects:
    soho.addObjects(now, matte_objects, '', '', False,
        geo_parm='matte_objects', light_parm='', fog_parm='')
if phantom_objects:
    soho.addObjects(now, phantom_objects, '', '', False,
        geo_parm='phantom_objects', light_parm='', fog_parm='')
soho.removeObjects(now, excludeobject, excludelights, excludefog,
    geo_parm='excludeobject', light_parm='excludelights', fog_parm='excludefog')

# site-wide customization hook
LSDhooks.call('pre_lockObjects', parmlist, objparms, now, camera)

# Lock off the objects we've selected
soho.lockObjects(now)

LSDsettings.clearLists()
LSDsettings.initializeFeatures()
LSDsettings.setMattePhantomOverrides(now, matte_objects, phantom_objects)

LSDmisc.initializeMotionBlur(cam, now)

# enabling cryptomatte, period, will add materialname property to objects
if cam.getDefaultedInt('lv_cryptolayers', now, [0])[0] > 0:
    LSDsettings._Settings.GenerateMaterialname = True

if mode == 'update':
    # Only reset saved shaders and properties
    LSDgeo.reset(False)

    LSDhooks.call('pre_iprUpdate', parmlist, objparms, now, camera)
    cmd_updateipr(stash=True)

    #
    # Output camera
    #
    if parmlist['main'].Value[0]:
        cmd_reset(light=False, obj=False, fog=False)
        LSDframe.renderCamera(cam, now)

    #
    # Delete deleted objects
    #
    cmd_comment('Deleting objects')
    for obj in soho.objectList('objlist:deletedinstance'):
        LSDgeo.dereferenceGeometry(obj)
        cmd_delete('object', obj.getName())
    cmd_comment(None)

    cmd_comment('Deleting lights')
    for light in soho.objectList('objlist:deletedlight'):
        LSDgeo.dereferenceGeometry(light)
        cmd_delete('light', light.getName())
    cmd_comment(None)

    cmd_comment('Deleting fogs')
    for fog in soho.objectList('objlist:deletedfog'):
        cmd_delete('fog', fog.getName())
    cmd_comment(None)

    #
    # Delete unused geometry
    #
    cmd_comment('Deleting unused geometry')
    LSDgeo.deleteUnusedGeometry()
    cmd_comment(None)

    #
    # Declare dirty materials
    #
    cmd_comment('Updating materials')
    LSDgeo.declareMaterials(now, soho.objectList('objlist:mat'))
    cmd_comment(None)

    #
    # Declare style sheets
    #
    cmd_comment('Updating style sheets')
    LSDsettings.outputStyleSheets(now, dirtystylesheets, True)
    cmd_comment(None)

    #
    # Declare bundles
    #
    cmd_comment('Updating bundles')
    LSDsettings.outputBundles(now, dirtybundles, True)
    cmd_comment(None)

    #
    # Output retained geometry
    #
    LSDframe.saveRetained(now, soho.objectList('objlist:dirtyinstance'),
                               soho.objectList('objlist:dirtylight'))

    #
    # Output objects
    #
    if parmlist['main'].Value[0]:
        LSDframe.renderObjects(now,
                soho.objectList('objlist:dirtyinstance'),
                soho.objectList('objlist:dirtylight'),
                soho.objectList('objlist:dirtyspace'),
                soho.objectList('objlist:dirtyfog'))

    LSDhooks.call('post_iprUpdate', parmlist, objparms, now, camera)

else:
    LSDgeo.reset()

    # Output LSD header
    LSDmisc.header(now, propdefs)

    if mode == 'generate':
        # Notify mantra that it's rendering for IPR
        print 'cmd_iprmode generate'

    for i in range(0, numpathmap):
        map = soho.getDefaultedString('lv_pathmap%d' % (i+1), [])
        if map and len(map) == 2:
            cmd_pathmap(map[0], map[1])

    if not LSDhooks.call('pre_otl', parmlist, objparms, now, camera):
        #
        # Output OTLs loaded by Houdini
        otls = soho.getDefaultedString('state:otllist', [])
        if soho.getDefaultedInt('lv_otlfullpath', [0])[0]:
            for i in range(len(otls)):
                otls[i] = hou.expandString(otls[i])
        if len(otls):
            cmd_comment("OTLs loaded into the .hip file")
            cmd_loadotl(otls)

        #
        # Output OTL preferences set in Houdini
        otprefs = soho.getDefaultedString('state:otprefer', [])
        if soho.getDefaultedInt('lv_otlfullpath', [0])[0]:
            for i in range(1, len(otprefs), 2):
                otprefs[i] = hou.expandString(otprefs[i])
        if len(otprefs):
            cmd_comment("OTL preferences from the .hip file")
            cmd_otprefer(otprefs)

    if inheritedproperties:
        # Output object level properties which are defined on the output driver
        cmd_comment('Object properties defined on output driver')
        LSDsettings.outputObject(soho.getOutputDriver(), now)

    isphoton = parmlist['engine'].Value[0] == 'photon' or \
               parmlist['engine'].Value[0] == 'viewphoton'

    #
    # If there's only one camera, output it here
    donecamera = False
    do_main = parmlist['main'].Value[0]
    if do_main and mode == "generate" and len(camera_list) == 1:
        sub_camera = camera_list[0]
        if sub_camera:
            if not LSDhooks.call('pre_mainimage', parmlist, objparms, now, sub_camera):
                cmd_comment('Main image from %s' % sub_camera.getName())
                LSDframe.renderCamera(sub_camera, now, forphoton=isphoton)
                donecamera = True

    #
    # Declare all materials
    #
    if LSDhooks.call('pre_materials', parmlist, objparms, now, camera):
        decl_shops = False
    if decl_shops:
        LSDgeo.declareAllMaterials(now, decl_shops > 1)

    #
    # Declare all bundles
    #
    LSDsettings.outputBundles(now, None, False)

    #
    # Declare style sheets
    #
    LSDsettings.outputStyleSheets(now, None, False)

    #
    # Output retained geometry
    #
    LSDhooks.call('pre_geometry', parmlist, objparms, now, camera)
    LSDframe.saveRetained(now, soho.objectList('objlist:instance'),
                               soho.objectList('objlist:light'))

    #
    # Output shadow maps
    #
    first = True
    do_shadow = parmlist['shadow'].Value[0]
    if LSDhooks.call('pre_shadowmaps', parmlist, objparms, now, camera):
        do_shadow = False
    if do_shadow:
        for light in soho.objectList('objlist:light'):
            wrangler = LSDsettings.getWrangler(light, now, 'light_wrangler')
            if light.wrangleInt(wrangler, 'render_shadowmap', now, [0])[0]:
                if first:
                    cmd_comment('')
                    cmd_comment('Shadow Maps')
                    cmd_comment('')
                    first = False
                cmd_comment('Shadow map from %s' % light.getName())
                LSDsettings.setShadowMap(True)
                LSDframe.render(light, now,
                        light.objectList('objlist:shadowmask', now),
                        [],        # Don't output lights
                        soho.objectList('objlist:space'),
                        [],        # Don't output fog when rendering shadow maps
                        fromlight=True,
                        skiplight=light.Object)
                LSDsettings.setShadowMap(False)
                cmd_reset()
    LSDhooks.call('post_shadowmaps', parmlist, objparms, now, camera)
    if not first:
        cmd_comment(None)

    #
    # Output reflection maps
    #
    first = True
    do_env = parmlist['env'].Value[0]
    if LSDhooks.call('pre_envmaps', parmlist, objparms, now, camera):
        do_env = False
    if do_env:
        for obj in soho.objectList('objlist:instance'):
            if obj.getDefaultedInt('render_envmap', now, [0])[0]:
                if first:
                    cmd_comment('')
                    cmd_comment('Environment Maps')
                    cmd_comment('')
                    first = False
                cmd_comment('Environment map from %s' % obj.getName())
                cubemap = LSDframe.CubeMapControl(obj, now)
                if not cubemap.BasePath:
                    cmd_comment(' ERROR: Invalid map name, skipping generation --')
                else:
                    for dir in range(6):
                        cubemap.setFace(dir)
                        LSDframe.render(obj, now,
                                obj.objectList('objlist:reflectmask', now),
                                soho.objectList('objlist:light'),
                                soho.objectList('objlist:space'),
                                soho.objectList('objlist:fog'),
                                cubemap=cubemap)
                        cmd_reset()
                    cubemap.makeMap()
    LSDhooks.call('post_envmaps', parmlist, objparms, now, camera)
    if not first:
        cmd_comment(None)

    #
    # Generate point clouds
    #
    first = True
    do_photon = parmlist['pointcloud'].Value[0]
    if LSDhooks.call('pre_pointclouds', parmlist, objparms, now, camera):
        do_photon = False
    if do_photon:
        for light in soho.objectList('objlist:light'):
            wrangler = LSDsettings.getWrangler(light, now, 'light_wrangler')
            if light.wrangleInt(wrangler, 'render_pointcloud', now, [0])[0]:
                if first:
                    cmd_comment('')
                    cmd_comment('Point Clouds')
                    cmd_comment('')
                    first = False
                soho.getOutputDriver().storeData('pcrender', True)
                cmd_comment('Point cloud from light: %s' % light.getName())

                pccam = cam
                if light.wrangleInt(wrangler, 'pc_camera_override', now, [0])[0]:
                    cam_path = light.wrangleString(
                            wrangler, 'pc_camera', now, [""])[0]
                    pccam = soho.getObject(cam_path)

                # As a temporary solution, point cloud lights need to
                # render all objects in the scene while geometry lights
                # only need to render the light geometry itself.  This
                # heuristic checks for the 'pc_enable' parameter which only
                # exists on geometry lights.
                is_pclight = not light.wrangleInt(wrangler, 'pc_enable', now, [0])[0]
                if is_pclight:
                    # Render the scene
                    LSDframe.render(light, now,
                                soho.objectList('objlist:instance'),
                                light.objectList('objlist:lightmask', now),
                                soho.objectList('objlist:space'),
                                soho.objectList('objlist:fog'),
                                fromlight=True,
                                forphoton=False,
                                viewcam=pccam,
                                skiplight=light.Object)
                else:
                    # Output only the light.  The light wrangler needs to
                    # configure light_contribprimary to true.
                    LSDframe.render(light, now,
                                [],        # Don't output objects
                                [light],
                                soho.objectList('objlist:space'),
                                [],        # Don't output fog
                                fromlight=True,
                                forphoton=False,
                                viewcam=pccam)
                soho.getOutputDriver().clearData('pcrender')
                # Don't reset objects for point cloud generation, similar
                # to photon maps.
                cmd_reset(obj=False)
    LSDhooks.call('post_pointclouds', parmlist, objparms, now, camera)

    #
    # Generate photon maps
    #
    first = True
    do_photon = parmlist['photon'].Value[0]
    if LSDhooks.call('pre_photonmaps', parmlist, objparms, now, camera):
        do_photon = False
    if do_photon:
        for light in soho.objectList('objlist:light'):
            wrangler = LSDsettings.getWrangler(light, now, 'light_wrangler')
            if light.wrangleInt(wrangler, 'render_photonmap', now, [0])[0]:
                if first:
                    cmd_comment('')
                    cmd_comment('Photon Maps')
                    cmd_comment('')
                    first = False
                soho.getOutputDriver().storeData('photonrender', True)
                cmd_comment('Photon map from light: %s' % light.getName())
                LSDframe.render(light, now,
                            soho.objectList('objlist:instance'),
                            light.objectList('objlist:lightmask', now),
                            soho.objectList('objlist:space'),
                            soho.objectList('objlist:fog'),
                            fromlight=True,
                            forphoton=True,
                            viewcam=cam,
                            skiplight=light.Object)
                soho.getOutputDriver().clearData('photonrender')
                # Don't reset objects for photon map generation.  This
                # makes it possible to reuse initialized octrees between
                # photon passes and the main render.  This optimization
                # assumes that the instance list for photon generation
                # matches the main render.
                cmd_reset(obj=False)
                if light.wrangleInt(wrangler, 'photon_prefilter', now, [0])[0]:
                    path = light.wrangleString(wrangler, 'photon_map', now, [""])[0]
                    prefilterpath = light.wrangleString(wrangler, 'photon_prefiltermap', now, [""])[0]
                    filter = light.wrangleString(wrangler, 'photon_filter', now, [""])[0]
                    count = light.wrangleInt(wrangler, 'photon_precount', now, [""])[0]
                    ratio = light.wrangleFloat(wrangler, 'photon_preratio', now, [""])[0]
                    cmd_prefilter(path, prefilterpath, filter, count, ratio)
    LSDhooks.call('post_photonmaps', parmlist, objparms, now, camera)

    #
    # Output main image
    #
    if do_main and donecamera:
        LSDmisc.setCameraBlur(camera_list[0], now)
        LSDframe.renderObjects(now,
                soho.objectList('objlist:instance'),
                soho.objectList('objlist:light'),
                soho.objectList('objlist:space'),
                soho.objectList('objlist:fog'))
        LSDhooks.call('post_mainimage', parmlist, objparms, now, camera_list[0])
    elif do_main:
        for sub_camera in camera_list:
            if sub_camera:
                if not LSDhooks.call('pre_mainimage', parmlist, objparms, now, sub_camera):
                    cmd_comment('Main image from %s' % sub_camera.getName())
                    LSDframe.render(sub_camera, now,
                            soho.objectList('objlist:instance'),
                            soho.objectList('objlist:light'),
                            soho.objectList('objlist:space'),
                            soho.objectList('objlist:fog'),
                            forphoton=isphoton)
                    # Always reset after intermediate (ie, non-last)
                    # cameras, and for the last one only if quickexit
                    # is false.
                    if (not quickexit) or (sub_camera is not camera_list[-1]):
                        cmd_reset()
                LSDhooks.call('post_mainimage', parmlist, objparms, now, sub_camera)

LSDhooks.call("post_lsdGen")
LSDsettings.clearLists()

# IMPORTANT: Don't print ANYTHING here - IPR relies on the fact that the
# cmd_raytrace command is the last thing in the buffer, otherwise the
# render will be interrupted immediately upon starting!

if mode == 'default':
    cmd_comment('Generation time: %g seconds' % (time.time() - clockstart))
    cmd_quit()
