from __future__ import print_function
import collections
import copy

from soho import SohoParm
from lava_renderpasses import RenderPass


cryptomatteMaterialPassParms = {
    'outputMode'                : SohoParm('lv_crypto_material_pass_output_mode',        'int',       [0], skipdefault=False),
    'outputPreview'             : SohoParm('lv_crypto_material_pass_output_preview',     'bool',      [1], skipdefault=False),
    'rank'                      : SohoParm('lv_crypto_material_pass_cryptomatte_rank',   'int',       [3], skipdefault=False),
}

cryptomatteInstancePassParms = {
    'outputMode'                : SohoParm('lv_crypto_instance_pass_output_mode',        'int',       [1], skipdefault=False),
    'outputPreview'             : SohoParm('lv_crypto_instance_pass_output_preview',     'bool',      [1], skipdefault=False),
    'rank'                      : SohoParm('lv_crypto_instance_pass_cryptomatte_rank',   'int',       [3], skipdefault=False),
}

cryptoMaterialPlaneParms = {
    'sourcepass'          : SohoParm('lv_crypto_material_source_pass',          'string',      ['CryptomattePass'], skipdefault=False),
    'outputname_override' : SohoParm('lv_crypto_material_pass_output_name',     'string',      ['CryptoMaterial'], skipdefault=False)
}

cryptoInstancePlaneParms = {
    'sourcepass'          : SohoParm('lv_crypto_instance_source_pass',          'string',      ['CryptomattePass'], skipdefault=False),
    'outputname_override' : SohoParm('lv_crypto_instance_pass_output_name',     'string',      ['CryptoObject'], skipdefault=False)
}

def getRenderPassDict():
    renderpasses = {
        "CryptomatteMaterialPass"   : RenderPass("preview_color",   "vector3",  "float16",    False,   {}, cryptomatteMaterialPassParms,    cryptoMaterialPlaneParms ),
        "CryptomatteInstancePass"   : RenderPass("preview_color",   "vector3",  "float16",    False,   {}, cryptomatteInstancePassParms,    cryptoInstancePlaneParms ),
    }
    for i in ["00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12", "13", "14", "15"]:
        materialPlaneParms = copy.deepcopy(cryptoMaterialPlaneParms)
        materialPlaneParms['outputname_override'] = SohoParm('lv_crypto_material_pass_output'+i+'_name',     'string',      ['CryptoMaterial'+i], skipdefault=False)
        renderpasses["CryptomatteMaterialPass"+i] = RenderPass("output"+i,   "vector4",  "float32",    False,   {}, {},    materialPlaneParms )

        instancePlaneParms = copy.deepcopy(cryptoMaterialPlaneParms)
        instancePlaneParms['outputname_override'] = SohoParm('lv_crypto_instance_pass_output'+i+'_name',     'string',      ['CryptoObject'+i], skipdefault=False)
        renderpasses["CryptomatteInstancePass"+i] = RenderPass("output"+i,   "vector4",  "float32",    False,   {}, {},    instancePlaneParms )

    return renderpasses

def getToggleRenderPassDict():
    togglerenderpassdict = {
        'lv_plane_cryptomaterial':              ['CryptomatteMaterialPass'],
        'lv_plane_cryptoinstance':              ['CryptomatteInstancePass'],
    }
    for i in ["00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12", "13", "14", "15"]:
        togglerenderpassdict['lv_plane_cryptomaterial'+i] = ['CryptomatteMaterialPass' + i]
        togglerenderpassdict['lv_plane_cryptoinstance'+i] = ['CryptomatteMaterialPass' + i]

    return togglerenderpassdict