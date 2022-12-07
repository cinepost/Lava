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
# NAME: LSDgeo.py ( Python )
#
# COMMENTS:     LSD Geometry I/O for SOHO
#

import re
import hashlib
from imp import reload  # This way we can use reload in Python 2 and 3.
import soho, hou, sys
import sohoglue
import SOHOcommon
import LSDmisc
import LSDapi
import LSDsettings
import LSDframe
import LSDmaterialx
from LSDapi import *
from soho import SohoParm
from sohog import SohoGeometry

reload(LSDmaterialx)

theDetailRefs = {}
theDetailRefsInv = {}
theSavedDetails = {}
theSopTBound = {}
theSavedProperties = {}
theSavedOPropMaps = {}
theSavedShaders = {}
theShopRefs = {}
theShopRefsInv = {}
theMaterialOverrideRefs = {}
theMaterialOverrideRefsInv = {}

theOverrideFormatStr='<<%s>>'


def _SohoGeometry(soppath, time = 0.0):
    geo = SohoGeometry(soppath, time)
    return  geo.tesselate({
        #'geo:convstyle': 'lod', 
        'geo:triangulate': True
    })

def safeSopPathName(soppath):
    return re.sub(r'[^\w\d-]','_',soppath)

def hashSopPathName(soppath):
    sopname = soppath.rsplit("/")[-1]
    sopname_len = len(sopname)
    safe_soppath = safeSopPathName(soppath)
    max_hexdigest_len = 240 - sopname_len
    return "%s_%s" % (sopname, hashlib.sha256(safe_soppath).hexdigest()[:max_hexdigest_len])

def _dummyGeometry():
    print("""PGEOMETRY V5
NPoints 0 NPrims 0
NPointGroups 0 NPrimGroups 0
NPointAttrib 0 NVertexAttrib 0 NPrimAttrib 0 NAttrib 0
beginExtra
endExtra""")

def reset(full = True):
    global theDetailRefs, theDetailRefsInv, theSavedDetails, theSavedProperties
    global theSavedShaders, theSopTBound, theShopRefs
    global theShopRefsInv, theMaterialOverrideRefs, theMaterialOverrideRefsInv
    global theSavedOPropMaps
    if full:
        theDetailRefs = {}
        theDetailRefsInv = {}
        theSavedDetails = {}
        theSavedShaders = {}
        theSopTBound = {}
        theShopRefs = {}
        theShopRefsInv = {}
        theMaterialOverrideRefs = {}
        theMaterialOverrideRefsInv = {}
    theSavedProperties = {}
    theSavedOPropMaps = {}

proceduralParms = [
    # We ask the SHOP to evaluate both the shader string AND the bounding box
    SohoParm('shop_geometrypath', 'shader', [], False),
    SohoParm('shop_geometrypath', 'bounds', [1,1,1,-1,-1,-1], False),
    SohoParm('shop_geometrypath', 'string', [], False),
    SohoParm('shop_cvexpath', 'shader', [], False),
]

def getProcedural(obj, now): 
    if obj.getDefaultedInt('shop_disable_geometry_shader', now, [0])[0]:
        return None
    proc = obj.evaluate(proceduralParms, now)
    shader = proc[0].Value
    bounds = proc[1].Value
    type   = getattr(proc[0], "ShopType", soho.ShopTypeDefault)
    if len(shader) < 1 or len(shader[0]) < 1 or len(bounds) != 6:
        return None
    return (bounds, shader, type, soho.getObject(proc[2].Value[0]))

def _forceGeometry(obj, now):
    return obj.getDefaultedInt('lv_forcegeometry', now, [1])[0]

def _getDiskFiles(obj, eval_time, times, velblur):
    auto_archive = obj.getDefaultedString('lv_auto_archive', eval_time, [''])[0]
    if auto_archive == 'off' or auto_archive == 'none':
        auto_archive = None
    files = []
    if velblur:
        times = [eval_time, eval_time]
    
    for now in times:
        filename = []
        if not obj.evalString("lv_archive", now, filename):
            return None
        files.append(filename[0])
        if auto_archive:
            # Save out .bgeo file
            tag = 'auto_archived-%s' % filename[0]
            processed = obj.getData(tag)
            if not processed:
                obj.storeData(tag, True)    # Indicate we've archived this file
                if auto_archive == 'force' or not os.path.exists(filename[0]):
                    soppath = obj.getDefaultedString('object:soppath',
                                                now, [''])[0]
                    gdp = _SohoGeometry(soppath, now)
                    options = getSaveOptions(obj, now)
                    if gdp.Handle >= 0:
                        soho.makeFilePathDirsIfEnabled(filename[0])
                    if gdp.Handle < 0 or not gdp.save(filename[0], options):
                        soho.error('Unable to create archive: %s' % filename[0])
    return files

def isObjectFastPointInstancer(obj, now):
    if not obj:
        return False
    plist = obj.evaluate( [ SohoParm('ptinstance', 'int', [0], False ) ], now )
    return plist[0].Value[0] == 2

def _getMaterialOverride(obj, now):
    if obj is None:
        return 'compact'
    return obj.getDefaultedString('lv_materialoverride', now, ['compact'])[0]

_invalidChars = re.compile('[^-a-zA-Z0-9.,_:]')

def hashPathOverride(path, over):
    # Given a SHOP path and an override value, create a hash which is
    # a "nice" name for mantra.  Mantra uses this algorithm exactly to
    # perform it's mapping
    over = over.replace(' ', '')        # Remove spaces
    if not over or over == '{}' or over == '{,}':
        return path                     # An empty override uses the path
    return path + '+' + _invalidChars.sub('', over)

def _getCompactPropertyOverrideHash(path, over):
    if path is None or len(path) == 0 or over is None:
        return path

    # Evaluate the override into a python dict
    try:
        over_dict = eval(over)
    except:
        return path
        
    if len(over_dict) == 0:
        return path

    return path + '+' + '+'.join(sorted(over_dict.keys()))


def processOverrideProperty(soppath, shoppath, overrides, now):
    if shoppath:
        hashpath = hashPathOverride(shoppath, overrides)
        referenceMaterialOverride(soppath, shoppath, hashpath)
        if theSavedProperties.get(hashpath, None) == None:
            try:
                dict = eval(overrides)
            except:
                dict = None
            over = soho.PropertyOverride(dict)
            shop = soho.getObject(shoppath)
            cmd_start('material')
            LSDsettings.outputObject(shop, now, name=hashpath)
            if LSDsettings._Settings.GenerateMaterialname:
                cmd_property('object', 'materialname', [shoppath])
            theSavedProperties[hashpath] = True 
            cmd_end()
    return

def processCompactOverrideProperty(soppath, shop_id, shop_infos, now, opropmap):
    shoppath = shop_id.split('+')[0]
    referenceMaterialOverride(soppath, shoppath, shop_id)
    odict = shop_infos.get('properties', {})
    over = soho.PropertyOverride(odict)
    if theSavedProperties.get(shop_id, None) == None:
        shop = soho.getObject(shoppath)

        cmd_start('material')
        propmap = {}
        LSDsettings.outputObject(shop, now, name=shop_id,
                                          output_shader=False,
                                          opropmap=propmap)
        if LSDsettings._Settings.GenerateMaterialname:
            cmd_property('object', 'materialname', [shoppath])
        theSavedProperties[shop_id] = True
        theSavedOPropMaps[shop_id] = propmap
        opropmap.update(propmap)        # Merge property maps

        for shop_type, shop_info in shop_infos.items():
            if shop_type == 'properties':
                continue
            cmd_shader('object', LSDsettings.oshaderMap[shop_type], shop_info[0], shop_info[1])
        cmd_end()
    else:
        propmap = theSavedOPropMaps.get(shop_id, None)
        if propmap:
            opropmap.update(propmap)
    del over
    return
    
def processStyleSheetProperty(mat_path, now):
    if theSavedProperties.get(mat_path, None) == None:
        LSDsettings.outputMaterial(mat_path, now)
        theSavedProperties[mat_path] = True
    return

def processGlobalPropertyOverrides(obj, soppath, gdp, pathattrib, overattrib, now):
    paths = gdp.attribProperty(pathattrib, 'geo:allstrings')
    overs = gdp.attribProperty(overattrib, 'geo:allstrings')
    fullpaths = []
    for p in paths:
        hou_shop = obj.node(p)
        if hou_shop:
            fullpaths.append(hou_shop.path())
        else:
            fullpaths.append(p)
    n = min(len(paths), len(overs))
    for i in range(n):
        processOverrideProperty(soppath, fullpaths[i], overs[i], now)
    return

def processPrimOrPointPropertyOverrides(obj, creator_obj, soppath, gdp, style,
                                        pathattrib, overattrib, now, opropmap):
    nitems = gdp.globalValue(style+'count')[0]
    if not nitems:
        return False

    ostyle = _getMaterialOverride(soho.getObject(creator_obj.path()), now)
    if ostyle == 'none':
        return False

    have_prim_props = False
    
    if ostyle == 'full':
        fullpaths = {}
        paths = gdp.attribProperty(pathattrib, 'geo:allstrings')
        for p in paths:
            hou_shop = creator_obj.node(p)
            if hou_shop:
                fullpaths[p] = hou_shop.path()
            else:
                fullpaths[p] = p
        for item in range(nitems):
            path = gdp.value(pathattrib, item)[0]
            if path:
                over = gdp.value(overattrib, item)[0]
                processOverrideProperty(soppath, fullpaths[path], over, now)
                have_prim_props = True
    elif ostyle == 'compact':
        unique_shops = set()
        for item in range(nitems):
            path = gdp.value(pathattrib, item)[0]
            if path:
                over = gdp.value(overattrib, item)[0]
                unique_shops.add( _getCompactPropertyOverrideHash(path, over) )

        shops = {}
        for shop_hash in unique_shops:
            shop_id = shop_hash.split( '+' )

            shop_path = shop_id.pop(0)
            shop_obj = soho.getObject( shop_path )

            odict = {}
            if len(shop_id)>0:
                for override in shop_id:
                    odict[override] = theOverrideFormatStr % override
                if odict:
                    # Stash override dict for later use by property overrides
                    shops.setdefault(shop_hash, {})
                    shops[shop_hash]['properties'] = odict
            over = soho.PropertyOverride(odict)
            for oshader in LSDsettings.oshaderParms:
                shader_type = oshader.Houdini
                shader = LSDsettings.getObjectShader(shop_obj, shader_type, now)
                if shader:
                    shops.setdefault(shop_hash, {})
                    shops[shop_hash][shader_type] = shader

            del over
        for shop_id, shop_infos in shops.items():
            processCompactOverrideProperty(soppath, shop_id, shop_infos, now, opropmap)
            have_prim_props = True
        
    return have_prim_props

def processPrimStyleSheet(gdp, style, stylesheetattrib, now):
    if style == 'geo:global':
        nitems = 1
    else:
        nitems = gdp.globalValue(style+'count')[0]

    if not nitems:
        return False

    have_prim_stylesheets = False
    
    unique_mats = set()
    unique_bundles = set()
    for item in range(nitems):
        stylesheet = gdp.value(stylesheetattrib, item)
        if len(stylesheet) > 0:
            stylesheet = stylesheet[0]
        else:
            stylesheet = '' # Fall back in case tuple size is zero.
        (ss_bundles, ss_mats) = LSDsettings.getBundlesAndMaterialsFromStyleSheet(stylesheet)
        for mat_path in ss_mats:
            if mat_path not in unique_mats and hou.node(mat_path):
                unique_mats.add(mat_path)
        for bundle in ss_bundles:
            unique_bundles.add(bundle)

    for mat_path in unique_mats:
        processStyleSheetProperty(mat_path, now)
        have_prim_stylesheets = True
    for bundle in unique_bundles:
        LSDsettings.outputBundle(bundle)
    
    return have_prim_stylesheets

def saveMaterial(now, fullpath):
    # If the material has overrides (from the Material SOP), we must
    # update each material override instance.  There could be as many
    # one per primitive for a given detail.
    if hasMaterialOverrides(fullpath):
        details = getReferencingDetails(fullpath)
        if details != None:
            for detail in details:
                gdp = _SohoGeometry(detail, now)
                if gdp.Handle >= 0:
                    saveProperties(None, detail, gdp, now)
                    #pass

    # It's possible that we don't need to output the base material
    # (ie. not an override) when only overrides are used; however,
    # we ignore this and always output the base material.  Note that
    # this could affect the computed displacement bound.
    if theSavedProperties.get(fullpath, None) == None:
        shop = soho.getObject(fullpath)
        LSDsettings.outputObject(shop, now)

        cmd_start('material')
        LSDmaterialx.outputNetwork(fullpath, now)
        if LSDsettings._Settings.GenerateMaterialname:
            cmd_property('object', 'material_name', [fullpath])
        theSavedProperties[fullpath] = True
        cmd_end()
    return

def processPropertyStrings(obj, strings, now):
    for path in strings:
        if path:
            # Handle relative paths to SHOPs
            hou_shop = obj.node(path)
            if hou_shop:
                fullpath = hou_shop.path()
            else:
                fullpath = path
            saveMaterial(now, fullpath)
    return

def traverseMaterials(parent, now, shaders_too):
    try:
        kids = parent.children()
    except:
        return
    for n in kids:
        processed = False
        t = n.type()
        if t.category() == hou.shopNodeTypeCategory():
            if t.name()=='material' or t.name()=='vopmaterial' or shaders_too:
                saveMaterial(now, n.path())
                processed = True
        if t.category() == hou.vopNodeTypeCategory():
            if n.isMaterialFlagSet():
                saveMaterial(now, n.path())
                processed = True
        if not processed or n.childTypeCategory() == hou.shopNodeTypeCategory():
            # This isn't a material, so traverse all the children
            traverseMaterials(n, now, shaders_too)

def declareAllMaterials(now, shaders_too):
    root = hou.node('/')
    traverseMaterials(root, now, shaders_too)

def declareMaterials(now, shaders):
    for shop in shaders:
        fullpath = shop.getName()
        saveMaterial(now, fullpath)

def processShaderStrings(obj, strings, now, shadertype):
    for path in strings:
        if path:
            # Handle relative paths to SHOPs
            hou_shop = obj.node(path)
            if hou_shop:
                fullpath = hou_shop.path()
            else:
                fullpath = path

            # the key needs to depend on shadertype as well since shop path may
            # point to a multi-context shop node, while here we are processing
            # the shop node for a specific context
            pathkey = fullpath + str(shadertype)
            if not theSavedShaders.has_key(pathkey):
                theSavedShaders[pathkey] = True
                shop = soho.getObject(fullpath)
                # The 'shop:string' token will cause the shop to
                # package up all its parameters into a single string
                parm = SohoParm('shop:string', 'shader', skipdefault=False)
                parm.ShopType = shadertype
                if shop.evalParm(parm, now):
                    cmd_shop(fullpath, parm.Value[0], shadertype)

class SavePropertiesStatus:
    def __init__(self, cpath=None, prim_props=False, opropmap={}):
        self.CreatorPath = cpath
        self.HasPrimProps = prim_props
        self.OverridePropMap = opropmap

def saveProperties(obj, soppath, gdp, now):
    # Relative paths in properties & shaders are specified relative to
    # the object creator (not the SOP), so, we need to find the object
    try:
        sop = hou.node(soppath)
    except:
        return SavePropertiesStatus()
    if not sop:
        return SavePropertiesStatus()
    if isObjectFastPointInstancer(obj,now):
        return SavePropertiesStatus()
    creator_obj = sop.creator()
    if not creator_obj:
        return SavePropertiesStatus()
    creator_path = None
    prim_props = False
    opropmap = {}
    for style in ['geo:global', 'geo:prim', 'geo:point']:
        # First process material overrides.
        attr = gdp.attribute(style, 'shop_materialpath')
        if attr >= 0:
            creator_path = creator_obj.path()   # Only set path if required
            over = gdp.attribute(style, 'material_override')
            if over >= 0:
                # Here, there are material overrides.  Therefore, it's
                # possible that a unique material might be needed for
                # each and every primitive.  If the override attribute
                # is unique on each primitive, it gets pretty ugly
                # fast.
                if style == 'geo:global':
                    processGlobalPropertyOverrides(creator_obj, soppath, gdp,
                        attr,over, now)
                else:
                    prim_props = processPrimOrPointPropertyOverrides(obj, creator_obj, soppath, gdp, style, attr, over, now, opropmap)
            else:
                strs = gdp.attribProperty(attr, 'geo:allstrings')
                processPropertyStrings(creator_obj, strs, now)

        # Now process material style sheets.
        attr = gdp.attribute(style, 'material_stylesheet')
        if attr >= 0:
            processPrimStyleSheet(gdp, style, attr, now)

        # Now process backward compatibility for old style attributes
        # (shader SOPs).  We need to save out the SHOP parameters
        # since mantra doesn't know how to resolve the indirect
        # references.
        attributes = [('shop_lv_surface',  'surface'),
                      ('shop_lv_photon',   'photon'),
                      ('shop_lv_displace', 'displacement'),
                      ('spriteshop',       'surface')]
        for (name, typename) in attributes:
            attr = gdp.attribute(style, name)
            if attr >= 0:
                strs = gdp.attribProperty(attr, 'geo:allstrings')
                processShaderStrings(creator_obj, strs, now, soho.getShopType(typename))
                creator_path = creator_obj.path()       # Only set path if required
    return SavePropertiesStatus(cpath=creator_path,
                    prim_props=prim_props,
                    opropmap=opropmap)

def getSaveOptions(obj, now, saveinfo=False):
    options = {
        # Don't bother saving out artist info with all geometry
        "geo:saveinfo":saveinfo,
        # Allow for very long JSON lines
        "json:textwidth":0,
	# Disable saving of index because we are not seekable anyways
	"geo:skipsaveindex":True,
    }
    groups = [True]
    if not obj.evalInt('lv_savegroups', now, groups):
        # Force inheritance to output driver
        soho.getOutputDriver().evalInt('lv_savegroups', now, groups)
    if groups[0]:
        options['savegroups'] = True
        options['geo:savegroups'] = True
    else:
        options['savegroups'] = False
        options['geo:savegroups'] = False

    # -1 -> Don't add normals
    #  0 -> Add vertex normals if no normals
    #  1 -> Add point normals if no normals
    add_normals_to = obj.getDefaultedInt('lv_addnormalsto', now, [1])[0] - 1

    if add_normals_to != -1:
        render_as_points = obj.getDefaultedInt('lv_renderpoints', now, [2])[0]
        if render_as_points == 1:
            add_normals_to = -1

    if add_normals_to != -1:
        options['geo:add_normals_to'] = add_normals_to

        # Get cusp angle if vertex normals
        if add_normals_to == 0:
            cusp_angle = obj.getDefaultedFloat('lv_cuspangle', now, [60.0])[0]
            options['geo:cusp_angle'] = cusp_angle
    
    return options


def saveArchive(objpath, now, geofile, matfile):
    obj = soho.getObject(objpath)
    soppath = obj.getDefaultedString('object:soppath', now, [''])[0]
    if not soppath:
        sys.stderr.write('Unable to find render SOP for %s\n' % objpath)
        return

    # When saving geometry for archives, make sure to save the header with
    # information like the bounding box, etc.
    options = getSaveOptions(obj, now, saveinfo=True)
    name = soppath

    gdp = _SohoGeometry(soppath, now)
    if gdp.Handle < 0:
        sys.stderr.write('No geometry found for %s\n' % soppath)
        return

    if matfile:
        soho.makeFilePathDirsIfEnabled(matfile)
        fp = open(matfile, 'w')
        if fp:
            save_stdout = sys.stdout
            sys.stdout = fp
            LSDmisc.header(now, None)
            cmd_comment("Save materials for %s at time %g" % (soppath, now))
            if geofile:
                cmd_comment('Corresponding geometry file: %s' % geofile)
            status = saveProperties(obj, soppath, gdp, now)
            if status.CreatorPath:
                cmd_property('geometry', 'basepath', [status.CreatorPath])
            if status.HasPrimProps:
                cmd_property( 'geometry', 'materialoverride',
                    [_getMaterialOverride(obj, now)])
            if status.OverridePropMap:
                cmd_property('geometry', 'overridepropmap',
                    [repr(status.OverridePropMap)])
            sys.stdout.flush()
            sys.stdout = save_stdout
        else:
            sys.stderr.write('Unable to save materials to %s\n' % matfile)
    if geofile:
        soho.makeFilePathDirsIfEnabled(geofile)
        if not gdp.save(geofile, options):
            sys.stderr.write('Unable to save geometry to %s\n' % geofile)


def saveRetainedProceduralRefs(proc, now):
    shop = proc[3]
    obj_paths = shop.getDefaultedString('op:objpaths', now, [])
    if not obj_paths:
        return
 
    for obj_path in obj_paths:
        obj = soho.getObject(obj_path)
        mbinfo = LSDmisc.geo_mbsamples(obj, now)
        saveRetained(obj, now, mbinfo[0], mbinfo[1], mbinfo[2], mbinfo[3])

def saveRetained(obj, now, times, velblur, accel_attrib, mbsegments):
    #
    # There are three cases for LSD geometry
    #   - Geometry defined by a procedural SHOP
    #     This geometry is output on a per-instance basis and not retained.
    #   - Geometry defined by a disk file
    #   - Geometry defined by a SOP
    #
    wrangler = LSDsettings.getWrangler(obj, now, 'object_wrangler')

    # Call the object wranglers 'retainShaders'. Note. It's the responsibility
    # of object wrangler to emit LSD correctly.
    obj.wrangleInt(wrangler,'retainShaders', now, [0])[0]

    # Call the object wranglers 'retainGeometry', skip inbuilt soho code if
    # it returns True. Note. It's the responsibility of object wrangler to
    # emit LSD correctly.
    if obj.wrangleInt(wrangler,'retainGeometry', now, [0])[0] :
        return
    
    proc = getProcedural(obj, now)
    if proc:
        saveRetainedProceduralRefs(proc, now)
        if not _forceGeometry(obj, now):
            return

    # Check for disk files (archives)
    files = _getDiskFiles(obj, now, times, velblur)
    if files and len(files) == len(times):
        baseName = 'arch-%s' % obj.getName()
        name = baseName
        details = theSavedDetails.get(name, None)
        refs = theDetailRefs.get(name, None)
        if details == None:
            seg = 0
            details = []
            for eval_time in times:
                
                details.append(name)
                cmd_start('geo')
                if velblur and seg > 0:
                    if velblur > 1:
                        cmd_detail('-a "%s" %g -V %g %g %s'%(accel_attrib, mbsegments, now-times[0], times[1]-now,name),
                                'arch-%s' % obj.getName())
                    elif velblur == 1:
                        cmd_detail('-V %g %g %s'%(now-times[0], times[1]-now,name),
                                'arch-%s' % obj.getName())
                else:
                    if velblur:
                        eval_time = now
                    LSDsettings.outputGeometry(obj, eval_time)
                    cmd_detail(name, files[seg])
                cmd_end()
                seg += 1
                name = '%s-%d' % (baseName, seg)
            theSavedDetails[baseName] = details
            theDetailRefs[baseName] = set([obj.getName()])
        else:
            refs.add(obj.getName())
        theDetailRefsInv[obj.getName()] = baseName
        obj.storeData("lv_details", details)
        return

    # Geometry defined by SOPs
    soppath = obj.getDefaultedString("object:soppath", now, [''])[0]
    if not soppath:
        # No SOP associated with this object -- this may cause problems later.
        return

    tbound = (times[0]-now, times[-1]-now, len(times))

    details = theSavedDetails.get(soppath, None)

    if details == None or theSopTBound[soppath] != tbound:
        binary = [True]
        if not obj.evalInt('lv_binarygeometry', now, binary):
            # Force inheritance to output driver
            soho.getOutputDriver().evalInt('lv_binarygeometry', now, binary)
        if binary[0]:
            stdoutname = 'stdout.bgeo'
        else:
            stdoutname = 'stdout.geo'
        options = getSaveOptions(obj, now)
        details = []
        name = soppath

        # Check to see whether to save geometry to external files or inline
        plist = obj.evaluate([
            SohoParm('lv_inlinestorage', 'int', LSDmisc.InlineGeoDefault,False),
            SohoParm('lv_reuseoutlinecache', 'int', [0], False),
        ], now)
        inline = plist[0].Value[0]
        reuse = plist[1].Value[0]
        nframes = LSDmisc.SequenceLength
        if nframes == 1:
            reuse = False
        for seg, eval_time in enumerate(times):
            details.append(name)
            if velblur and seg > 0:
                cmd_comment("Save geometry for %s velocity blur" % (soppath))
                cmd_start('geo')
                if velblur > 1:
                    cmd_detail('-a "%s" %g -V %g %g %s' %(accel_attrib, mbsegments, now-times[0], times[1]-now, name), details[0])
                elif velblur == 1:
                    cmd_detail('-V %g %g %s' %(now-times[0], times[1]-now, name), details[0])
            else:
                if velblur:
                    eval_time = now
                gdp = _SohoGeometry(soppath, eval_time)
                saved = False
                if gdp.Handle >= 0:
                    status = saveProperties(obj, soppath, gdp, eval_time)
                    cmd_comment("Save geometry for %s at time %g" % (soppath, eval_time))
                    cmd_start('geo')
                    if status.HasPrimProps:
                        cmd_property( 'geometry', 'materialoverride',
                            [_getMaterialOverride(obj, eval_time)])
                    if status.OverridePropMap:
                        cmd_property('geometry', 'overridepropmap',
                            [repr(status.OverridePropMap)])

                    LSDsettings.outputGeometry(obj, eval_time)
                    options['geo:sample'] = seg
                    if status.CreatorPath:
                        cmd_property('geometry', 'basepath', [status.CreatorPath])
                    if inline:
                        # Save the detail statement first
                        cmd_detail(name, "stdin")
                        # Then save the geometry inline in the LSD
                        saved = gdp.save(stdoutname, options)
                    else:
                        # Find out where to save the external asset.
                        sessionid = LSDmisc.ExternalSessionId
                        canreuse = reuse
                        if not reuse and LSDmisc.PipeStream:
                            # Pass the -T option to delete file after rendering.
                            rootpath = LSDmisc.getLocalStoragePath()
                            tmpfile = '-T '
                            varname = '$_TMP_LOCAL_STORAGE'
                        else:
                            rootpath = LSDmisc.getSharedStoragePath()
                            tmpfile = ''
                            varname = '$_TMP_SHARED_STORAGE'

                        if nframes > 1:
                            # If we're rendering more than one frames, check to
                            # see if the geometry is static or dynamic.
                            if not gdp.globalValue('geo:timedependent')[0]:
                                # Instead of storing the session id per-frame,
                                # use a shared session id.
                                sessionid = LSDmisc.ExternalSharedSessionId
                                # The geometry doesn't change frame to frame
                                if LSDmisc.SequenceNumber != nframes:
                                    # If we aren't on the last frame, we don't
                                    # want mantra to delete the file after its
                                    # been used (since it will be used again for
                                    # subsequent frames).
                                    tmpfile = ''
                                if LSDmisc.SequenceNumber != 1:
                                    # For frames other than the first frame,
                                    # we can just reuse the shared geometry
                                    canreuse = True

                        # Create a unique filename for the geometry
                        
                        #path = '%s_%s' % (sessionid, gdp.globalValue('geo:sopid')[0])
                        path = '%s_%s' % (sessionid, hashSopPathName(soppath))
                        
                        if seg:
                            path += '-%d' % seg
                        path += '.bgeo.sc'

                        # Save geometry to external file first.
                        #
                        # By saving geometry first, we ensure the geometry will
                        # be flushed to disk before the cmd_detail statement is
                        # written to a pipe.
                        savepath = '/'.join([rootpath, path])
                        if canreuse and os.path.exists(savepath):
                            saved = True
                        else:
                            saved = gdp.save(savepath, options)

                        # Instead of explicitly specifying the rootpath for the
                        # cmd_detail command, use the $_TMP_SHARED_STORAGE
                        # variable (defined in LSDmisc.py).  This makes it
                        # easier to re-locate LSD files.
                        path = '/'.join([varname, path])
                        cmd_detail(tmpfile + name, path)
                if not saved:
                    if obj.wrangleInt(wrangler,'soho_soperror', now, [1])[0]:
                        msg = 'Unable to save geometry for: %s' % soppath
                        if gdp.Error:
                            msg = ''.join([msg, '\n\tSOP ', gdp.Error])
                        soho.error(msg)
                    cmd_comment('Saving geometry failed')
                    cmd_detail(name, "stdin")
                    _dummyGeometry()
            cmd_end()
            name = "%s-%d" % (soppath, seg + 1)
        theSavedDetails[soppath] = details
        theDetailRefs[soppath] = set([obj.getName()])
        theSopTBound[soppath] = tbound
    else:
        refs = theDetailRefs.get(soppath, None)
        refs.add(obj.getName())
    theDetailRefsInv[obj.getName()] = soppath
    obj.storeData("lv_details", details)

def dereferenceGeometry(obj):
    #
    # Use our object-name -> geometry-name mapping to find
    # the specified object's geometry name.  Then delete
    # the mapping and remove the object name from the set
    # of objects that reference the geometry.
    #
    detail = theDetailRefsInv.get(obj.getName(), None)
    if detail != None:
        theDetailRefs[detail].remove(obj.getName())
        del theDetailRefsInv[obj.getName()]

def deleteUnusedGeometry():
    #
    # Find all the geometries that have no referencing objects
    # and delete them.
    #
    deleted = []
    for detail, refs in theDetailRefs.items():
        if len(refs) == 0:
            details = theSavedDetails.get(detail, None)
            if details != None:
                deleted.extend(details)
                del theSavedDetails[detail]

    for detail in deleted:
        if theDetailRefs.has_key(detail):
            del theDetailRefs[detail]
        cmd_delete('geometry', detail)
        dereferenceMaterials(detail)
        _SohoGeometry.release(detail)

def instanceGeometry(obj, now, times):
    # Now, we need to save the geometry for the object out
    proc = getProcedural(obj, now)
    if proc:
        cmd_procedural(proc[0], proc[1], proc[2])
        cmd_declare('object', 'vector2', 'camera:shutter',
            LSDmisc.obj_shutter_open_close(obj, now))
        if not _forceGeometry(obj, now):
            return 0

    # No procedural, so there must should be detail handles
    data = obj.getData("lv_details")
    if not data:
        return 0

    details = data

    mbinfo = LSDmisc.geo_mbsamples(obj, now)
    if len(mbinfo[0]) == 1:
        details = [ details[0] ]

    for d in details:
        cmd_geometry(d)
    return

def getReferencingDetails(shop):
    return theShopRefsInv.get(shop, None)

def hasMaterialOverrides(shop):
    return theShopRefsInv.has_key(shop)

def referenceMaterialOverride(detail, shop, material):
    # Create the following mappings:
    # * detail -> SHOP
    # * SHOP -> detail
    # * (detail, shop) -> material
    # * material -> (detail, shop)
    refs = theShopRefs.get(detail, None)
    if refs == None:
        theShopRefs[detail] = set([shop])
    else:
        refs.add(shop)
    refs = theShopRefsInv.get(shop, None)
    if refs == None:
        theShopRefsInv[shop] = set([detail])
    else:
        refs.add(detail)
    refs = theMaterialOverrideRefs.get((detail, shop), None)
    if refs == None:
        theMaterialOverrideRefs[(detail, shop)] = set([material])
    else:
        refs.add(material)
    refs = theMaterialOverrideRefsInv.get(material, None)
    if refs == None:
        theMaterialOverrideRefsInv[material] = set([(detail, shop)])
    else:
        refs.add((detail, shop))
    
def dereferenceMaterials(detail):
    # Remove all detail -> SHOP mappings and SHOP -> detail mappings for
    # the specified detail.  Furthermore, invoke
    # dereferenceMaterialOverrides() for all (detail, SHOP) pairs involving
    # the specified detail.
    shops = theShopRefs.get(detail, None)
    if shops != None:
        for shop in shops:
            theShopRefsInv[shop].remove(detail)
            if len(theShopRefsInv[shop]) == 0:
                del theShopRefsInv[shop]
            dereferenceMaterialOverrides(detail, shop)
        del theShopRefs[detail]

def dereferenceMaterialOverrides(detail, shop):
    # Remove all (detail, SHOP) -> material and material -> (detail, SHOP)
    # mappings.  If a material is unreferenced, delete it.
    materials = theMaterialOverrideRefs.get((detail, shop), None)
    if materials != None:
        for material in materials:
            theMaterialOverrideRefsInv[material].remove((detail, shop))
            if len(theMaterialOverrideRefsInv[material]) == 0:
                cmd_delete('material', material)
                del theMaterialOverrideRefsInv[material]
        del theMaterialOverrideRefs[(detail, shop)]

def _computeVBounds(geo, box, tscale):
    vbox = [0, 0, 0, 0, 0, 0]
    vhandle = geo.attribute('geo:point', 'v')
    if vhandle >= 0:
        npts = geo.globalValue('geo:pointcount')[0]
        for pt in range(npts):
            v = geo.value(vhandle, pt)
            vbox[0] = min(vbox[0], v[0]*tscale)
            vbox[1] = min(vbox[1], v[1]*tscale)
            vbox[2] = min(vbox[2], v[2]*tscale)
            vbox[3] = max(vbox[0], v[0]*tscale)
            vbox[4] = max(vbox[1], v[1]*tscale)
            vbox[5] = max(vbox[2], v[2]*tscale)
    return [vbox[0]+box[0], vbox[1]+box[1], vbox[2]+box[2],
            vbox[3]+box[3], vbox[4]+box[4], vbox[5]+box[5]]

# Returns the full bounds of an object over the entire shutter range
def getObjectBounds(obj, now):
    (times, velblur, accel_attrib, mbsegments) = LSDmisc.geo_mbsamples(obj, now)
    proc = getProcedural(obj, times[0])
    if proc:
        return proc[0]
    
    soppath = obj.getDefaultedString("object:soppath", now, [''])[0]
    if not soppath:
        return None
    details = theSavedDetails.get(soppath, None)
    if not details or len(details) == 0:
        return None
    detail = details[0]

    fullbounds = SOHOcommon.emptyBounds()
    if velblur:
        gdb = _SohoGeometry(detail, times[0])
        fullbounds = gdb.globalValue('geo:boundingbox')
        fullbounds[:] = _computeVBounds(gdb, fullbounds, times[-1]-times[0])
    else:
        for geo_now in times:
            gdb = _SohoGeometry(detail, geo_now)
            dbounds = gdb.globalValue('geo:boundingbox')
            fullbounds[:] = SOHOcommon.enlargeBounds(fullbounds, dbounds)
    return fullbounds

theOverrideFormatStr='<<%s>>'

def getInstancerAttributes(obj, now):
    attribs = [
        'geo:pointxform',               # Required
        'v',
        'instance',
        'instancefile',
        'shop_materialpath',
        'material_override'
    ]

    sop_path = []
    if not obj.evalString('object:soppath', now, sop_path):
        return          # No geometry associated with this object

    geo = _SohoGeometry(sop_path[0], now)
    if geo.Handle < 0:  # No geometry data available
        return

    npts = geo.globalValue('geo:pointcount')[0]
    if not npts:
        return

    attrib_map = {}

    for attrib in attribs:
        handle = geo.attribute('geo:point', attrib)
        if handle >= 0:
            attrib_map[attrib] = handle

    return (geo, npts, attrib_map)

def getInstantiatedObjects(obj, now):
    (geo, npts, attrib_map) = getInstancerAttributes(obj, now)
    if not geo or not npts:
        return []

    inst_path = []
    obj.evalString('instancepath', now, inst_path)

    inst_path[0] = LSDmisc.absoluteObjectPath(obj, now, inst_path[0])
    
    # See if there's a per-point instance assignment
    if 'instance' not in attrib_map:
        return inst_path

    unique_inst = set(inst_path)
    for pt in range(npts):
        inst_path = geo.value(attrib_map['instance'], pt)[0]
        unique_inst.add(LSDmisc.absoluteObjectPath(obj, now, inst_path))

    return list(unique_inst)

# This class emulates a function call, but instead of doing something
# functional, simply stores the arguments to the call for later use.
class copyingXFormCall:
    def __init__(self): self.items = []
    def __call__(self, xform): self.items.append( hou.Matrix4(xform) )


def getPointInstanceSHOPHash( geo, pt, attrib_handle, override_handle ):
    shop_path_hash = geo.value(attrib_handle, pt)[0]
    if shop_path_hash is None or \
       len(shop_path_hash) == 0 or \
       override_handle is None:
        return (shop_path_hash,None)

    if override_handle < 0:
        return (shop_path_hash,None)
    overrides = eval(geo.value(override_handle, pt)[0])
    if overrides is None or len(overrides) == 0:
        return (shop_path_hash,None)

    return (shop_path_hash + '+' + '+'.join( sorted(overrides.keys()) ), overrides)

def getPointInstanceSHOPs(pt_shaders, now, geo, attrib_name, npts, shader_types = None, override = None ):
    if shader_types is None:
        shader_types = [attrib_name]

    attrib_handle = geo.attribute('geo:point', attrib_name)
    if attrib_handle < 0:
        return

    override_handle = geo.attribute( 'geo:point', override ) if override is not None else -1

    # Collect unique SHOPs from the point attribute
    unique_shops = set()
    for pt in range(npts):
        (shop_hash,overrides) = getPointInstanceSHOPHash(geo, pt, attrib_handle, override_handle)
        if shop_hash:
            unique_shops.add( shop_hash )

    for shop_hash in unique_shops:
        shop_id = shop_hash.split( '+' )

        shop_path = shop_id.pop(0)
        shop_obj = soho.getObject( shop_path )

        odict = {}
        if len(shop_id)>0:
            for override in shop_id:
                odict[override] = theOverrideFormatStr % override
        over = soho.PropertyOverride(odict)
        for shader_type in shader_types:
            shader = LSDsettings.getObjectShader(shop_obj, shader_type, now)
            if shader:
                pt_shaders.setdefault(shop_hash, {})
                pt_shaders[shop_hash][shader_type] = shader
        del over
