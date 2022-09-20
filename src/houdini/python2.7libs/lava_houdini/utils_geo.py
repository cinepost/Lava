import hou

from utils_common import addParmTemplate, findSubFolderByTag
from lava_version import LAVA_VERSION_STRING


def add_geometry_lava_parameters(node, rebuild=True):
	group = node.parmTemplateGroup()

	# Check for existing Lava folder. If it's there then just remove it and reinstall parameters
	old_lava_folder = group.findFolder('Lava') or group.find('folder_lava')
	lava_folder = hou.FolderParmTemplate('folder_lava', "Lava", tags={'lava_name':'root'})

	# Lava shading
	shading_folder = hou.FolderParmTemplate('folder_lava_shading', "Shading", tags={'lava_name':'shading'})

	addParmTemplate(node, shading_folder, hou.ToggleParmTemplate('lv_biasnormal','Bias Along Normal', False))
	addParmTemplate(node, shading_folder, hou.ToggleParmTemplate('lv_double_sided','Double Sided', True))
	addParmTemplate(node, shading_folder, hou.ToggleParmTemplate('lv_fix_shadow','Fix shadow terminator', True))
	addParmTemplate(node, shading_folder, hou.ToggleParmTemplate('lv_matte','Matte shading', False))
	lava_folder.addParmTemplate(shading_folder)

	# Lava visibility
	visibility_folder = hou.FolderParmTemplate('folder_lava_visibility', "Visibility", tags={'lava_name':'visibility'})

	addParmTemplate(node, visibility_folder, hou.ToggleParmTemplate('lv_visibility_primary','Visible to camera rays', True))
	addParmTemplate(node, visibility_folder, hou.ToggleParmTemplate('lv_visibility_shadows_recv','Receive shadows', True))
	addParmTemplate(node, visibility_folder, hou.ToggleParmTemplate('lv_visibility_shadows_cast','Cast shadows', True))
	addParmTemplate(node, visibility_folder, hou.ToggleParmTemplate('lv_visibility_shadows_self','Self shadows', True))
	lava_folder.addParmTemplate(visibility_folder)
		
	addParmTemplate(node, lava_folder, hou.ToggleParmTemplate('lv_skip','Skip', False))

	if old_lava_folder:
		group.replace(old_lava_folder.name(), lava_folder)
	else:
		# We instert Lava folder right after Render of Arnold folder if present
		insert_after_folder = group.find('Render') or group.findFolder('Arnold')
		if insert_after_folder:
			group.insertAfter(insert_after_folder, lava_folder)
		else:
			group.append(lava_folder)

	node.setParmTemplateGroup(group)


def add_instance_lava_parameters(node, rebuild=False):
	add_geometry_lava_parameters(node, rebuild)

def on_geo_created(node, node_type):
	if node:
		add_geometry_lava_parameters(node)


def on_instance_created(node, node_type):
	if node:
		add_instance_lava_parameters(node)