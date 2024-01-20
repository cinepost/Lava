import hou

from .utils_common import addParmTemplate, findSubFolderByTag
from lava_version import LAVA_VERSION_STRING


def add_light_lava_parameters(node, rebuild=True):
	type_name = node.type().name()
	is_env_light = type_name == "envlight"
	group = node.parmTemplateGroup()

	# Check for existing Lava folder. If it's there then just remove it and reinstall parameters
	old_lava_folder = group.findFolder('Lava') or group.find('folder_lava')
	lava_folder = hou.FolderParmTemplate('folder_lava', "Lava", tags={'lava_name':'root'})

	# Patch obsolete parameters
	#if(node.parm("lv_light_specular_color"))


	# Lava light common
	common_folder = hou.FolderParmTemplate('folder_lava_common', "Common", tags={'lava_name':'common'})
	addParmTemplate(node, common_folder, hou.FloatParmTemplate('lv_light_direct_diffuse_color_multiplier','Diffuse Color Multiplier', 3, 
		(1.0, 1.0, 1.0), min=0.0, max=1.0, look=hou.parmLook.ColorSquare, naming_scheme=hou.parmNamingScheme.RGBA, min_is_strict=True))

	addParmTemplate(node, common_folder, hou.FloatParmTemplate('lv_light_direct_specular_color_multiplier','Specular Color Multiplier', 3, 
		(1.0, 1.0, 1.0), min=0.0, max=1.0, look=hou.parmLook.ColorSquare, naming_scheme=hou.parmNamingScheme.RGBA, min_is_strict=True))

	addParmTemplate(node, common_folder, hou.FloatParmTemplate('lv_light_indirect_diffuse_color_multiplier','Indirect Diffuse Color Multiplier', 3, 
		(1.0, 1.0, 1.0), min=0.0, max=1.0, look=hou.parmLook.ColorSquare, naming_scheme=hou.parmNamingScheme.RGBA, min_is_strict=True))

	addParmTemplate(node, common_folder, hou.FloatParmTemplate('lv_light_indirect_specular_color_multiplier','Indirect Specular Color Multiplier', 3, 
		(1.0, 1.0, 1.0), min=0.0, max=1.0, look=hou.parmLook.ColorSquare, naming_scheme=hou.parmNamingScheme.RGBA, min_is_strict=True))
	
	addParmTemplate(node, common_folder, hou.ToggleParmTemplate('lv_contribute_direct_diffuse','Contribute Direct Diffuse', True))
	addParmTemplate(node, common_folder, hou.ToggleParmTemplate('lv_contribute_direct_specular','Contribute Direct Specular', True))
	addParmTemplate(node, common_folder, hou.ToggleParmTemplate('lv_contribute_indirect_diffuse','Contribute Indirect Diffuse', True))
	addParmTemplate(node, common_folder, hou.ToggleParmTemplate('lv_contribute_indirect_specular','Contribute Indirect Specular', True))


	# Lava environment light folder
	if is_env_light:
		phys_sunsky_folder = hou.FolderParmTemplate('folder_lava_envlight', "Physical Sky", tags={'lava_name':'phys_sky'})
		addParmTemplate(node, phys_sunsky_folder, hou.ToggleParmTemplate('lv_enable_physical_sky','Enable physical sky', False))
		lava_folder.addParmTemplate(phys_sunsky_folder)


	# Lava light shadows
	shadows_folder = hou.FolderParmTemplate('folder_lava_shadows', "Shadows", tags={'lava_name':'shadows'})

	if not is_env_light:
		addParmTemplate(node, shadows_folder, hou.FloatParmTemplate('lv_light_radius','Point/Spot radius', 1, (0.0,), min=0.0, max=1.0, min_is_strict=True, disable_when='{ light_type != "point" }'))
	
	lava_folder.addParmTemplate(common_folder)

	if not is_env_light:
		lava_folder.addParmTemplate(shadows_folder)
		
	addParmTemplate(node, lava_folder, hou.ToggleParmTemplate('lv_skip','Skip', False))

	if old_lava_folder:
		group.replace(old_lava_folder.name(), lava_folder)
	else:
		# We instert Lava folder right after Shadow of Light folder if present
		insert_after_folder = group.find('Light') or group.findFolder('Shadow')
		if insert_after_folder:
			group.insertAfter(insert_after_folder, lava_folder)
		else:
			group.append(lava_folder)

	node.setParmTemplateGroup(group)


def on_light_created(node, node_type):
	if node:
		add_light_lava_parameters(node)
