from __future__ import print_function
import collections
import copy

import hou
from soho import SohoParm
from lava_renderpasses import RenderPass

__crytomatteLayerPostfix = ["00", "01", "02", "03", "04", "05", "06", "07"]

cryptomatteMaterialPassParms = {
    'samplesPerFrame'           : SohoParm('lv_samples',                                 'int',       [0],  skipdefault=False),
    'typeName'                  : SohoParm('lv_crypto_material_pass_output_name',        'string',    ['CryptoMaterial'], skipdefault=False)
    'outputMode'                : SohoParm('lv_crypto_material_pass_output_mode',        'int',       [0],  skipdefault=False),
    'outputPreview'             : SohoParm('lv_crypto_material_pass_output_preview',     'bool',      [1],  skipdefault=False),
    'rank'                      : SohoParm('lv_crypto_material_pass_cryptomatte_rank',   'int',       [3],  skipdefault=False),
    'manifestFilename'          : SohoParm('lv_crypto_material_pass_manifest_file',      'string',    [''], skipdefault=False),
}

cryptomatteInstancePassParms = {
    'samplesPerFrame'           : SohoParm('lv_samples',                                 'int',       [0],  skipdefault=False),
    'typeName'                  : SohoParm('lv_crypto_instance_pass_output_name',        'string',    ['CryptoObject'], skipdefault=False),
    'outputMode'                : SohoParm('lv_crypto_instance_pass_output_mode',        'int',       [1], skipdefault=False),
    'outputPreview'             : SohoParm('lv_crypto_instance_pass_output_preview',     'bool',      [1], skipdefault=False),
    'rank'                      : SohoParm('lv_crypto_instance_pass_cryptomatte_rank',   'int',       [3], skipdefault=False),
    'manifestFilename'          : SohoParm('lv_crypto_instance_pass_manifest_file',      'string',    [''], skipdefault=False),
}

cryptoMaterialPlaneParms = {
    'sourcepass'          : SohoParm('lv_crypto_material_source_pass',          'string',      ['CryptomattePass'], skipdefault=False),
    'outputname_override' : SohoParm('lv_crypto_material_pass_output_name',     'string',      ['CryptoMaterial'], skipdefault=False),
}

cryptoInstancePlaneParms = {
    'sourcepass'          : SohoParm('lv_crypto_instance_source_pass',          'string',      ['CryptomattePass'], skipdefault=False),
    'outputname_override' : SohoParm('lv_crypto_instance_pass_output_name',     'string',      ['CryptoObject'], skipdefault=False)
}

def getRenderPassDict(cam = None, now = None):

    materialPlaneParms = copy.deepcopy(cryptoMaterialPlaneParms)
    if(cam.getDefaultedInt('lv_crypto_material_filenameenable', now, [0])[0]):
            materialPlaneParms['filename'] = SohoParm('lv_crypto_material_filename', 'string', [''])
    
    instancePlaneParms = copy.deepcopy(cryptoInstancePlaneParms)
    if(cam.getDefaultedInt('lv_crypto_instance_filenameenable', now, [0])[0]):
            instancePlaneParms['filename'] = SohoParm('lv_crypto_instance_filename', 'string', [''])

    renderpasses = {
        "CryptomatteMaterialPass"   : RenderPass("CryptomatteMaterialPass", "preview_color",   "vector3",  "float16",    False,   {}, cryptomatteMaterialPassParms,    materialPlaneParms ),
        "CryptomatteInstancePass"   : RenderPass("CryptomatteInstancePass", "preview_color",   "vector3",  "float16",    False,   {}, cryptomatteInstancePassParms,    instancePlaneParms ),
    }

    material_rank = cam.getDefaultedInt('lv_crypto_material_pass_cryptomatte_rank', now, [0])[0]
    instance_rank = cam.getDefaultedInt('lv_crypto_instance_pass_cryptomatte_rank', now, [0])[0]
    material_data_layers_count = (material_rank >> 1) + (material_rank - 2 * (material_rank >> 1))
    instance_data_layers_count = (instance_rank >> 1) + (instance_rank - 2 * (instance_rank >> 1))

    for i in range(material_data_layers_count):
        postfix = __crytomatteLayerPostfix[i]
        materialPlaneParms = copy.deepcopy(cryptoMaterialPlaneParms)
        materialPlaneParms['accumulation'] = SohoParm('__fake_parm_name__', 'bool', [False], skipdefault=False)
        materialPlaneParms['sourcepass'] = SohoParm('__fake_parm_name__', 'string', ['CryptomatteMaterialPass'], skipdefault=False)
        materialPlaneParms['outputname_override'] = SohoParm('lv_crypto_material_pass_output' + postfix + '_name',     'string',      ['CryptoMaterial' + postfix], skipdefault=False)
        
        if(cam.getDefaultedInt('lv_crypto_material_filenameenable', now, [0])[0]):
            materialPlaneParms['filename'] = SohoParm('lv_crypto_material_filename', 'string', [''])

        renderpasses["CryptomatteMaterialPass" + postfix] = RenderPass("CryptomatteMaterialPass", "output" + postfix,   "vector4",  "float32",    False,   {}, {},    materialPlaneParms )

    for i in range(instance_data_layers_count):
        postfix = __crytomatteLayerPostfix[i]
        instancePlaneParms = copy.deepcopy(cryptoInstancePlaneParms)
        instancePlaneParms['accumulation'] = SohoParm('__fake_parm_name__', 'bool', [False], skipdefault=False)
        instancePlaneParms['sourcepass'] = SohoParm('__fake_parm_name__', 'string', ['CryptomatteInstancePass'], skipdefault=False)
        instancePlaneParms['outputname_override'] = SohoParm('lv_crypto_instance_pass_output' + postfix + '_name',     'string',      ['CryptoObject' + postfix], skipdefault=False)
        
        if(cam.getDefaultedInt('lv_crypto_instance_filenameenable', now, [0])[0]):
            instancePlaneParms['filename'] = SohoParm('lv_crypto_instance_filename', 'string', [''])

        renderpasses["CryptomatteInstancePass" + postfix] = RenderPass("CryptomatteInstancePass", "output" + postfix,   "vector4",  "float32",    False,   {}, {},    instancePlaneParms )

    return renderpasses

def getToggleRenderPassDict(cam = None, now = None):
    togglerenderpassdict = {
        'lv_plane_cryptomaterial':              ['CryptomatteMaterialPass'],
        'lv_plane_cryptoinstance':              ['CryptomatteInstancePass'],
    }
    
    material_rank = cam.getDefaultedInt('lv_crypto_material_pass_cryptomatte_rank', now, [0])[0]
    instance_rank = cam.getDefaultedInt('lv_crypto_instance_pass_cryptomatte_rank', now, [0])[0]
    material_data_layers_count = (material_rank >> 1) + (material_rank - 2 * (material_rank >> 1))
    instance_data_layers_count = (instance_rank >> 1) + (instance_rank - 2 * (instance_rank >> 1))

    for i in range(material_data_layers_count):
        postfix = __crytomatteLayerPostfix[i]
        togglerenderpassdict['lv_plane_cryptomaterial'] += ['CryptomatteMaterialPass' + postfix]

    for i in range(instance_data_layers_count):
        postfix = __crytomatteLayerPostfix[i]
        togglerenderpassdict['lv_plane_cryptoinstance'] += ['CryptomatteInstancePass' + postfix]

    return togglerenderpassdict