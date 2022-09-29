import hou

from utils_common import addParmTemplate, findSubFolderByTag
from lava_version import LAVA_VERSION_STRING


def add_light_lava_parameters(node, rebuild=True):
	group = node.parmTemplateGroup()

	# Check for existing Lava folder. If it's there then just remove it and reinstall parameters
	old_lava_folder = group.findFolder('Lava') or group.find('folder_lava')
	lava_folder = hou.FolderParmTemplate('folder_lava', "Lava", tags={'lava_name':'root'})

	# Lava light shadows
	shading_folder = hou.FolderParmTemplate('folder_lava_shading', "Shadows", tags={'lava_name':'shadows'})

	addParmTemplate(node, shading_folder, hou.FloatParmTemplate('lv_light_radius','Point/Spot radius', 1, (0.0,), min=0.0, max=1.0, min_is_strict=True, disable_when='{ light_type != "point" }'))
	lava_folder.addParmTemplate(shading_folder)
		
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
