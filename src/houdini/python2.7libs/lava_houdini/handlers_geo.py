import hou


def add_geometry_lava_parameters(node):
	group = node.parmTemplateGroup()

	if(group.findFolder('Lava')):
		# Lava folder and it's parameters are already there. Just skip...
		return

	# We instert Lava folder right after Render of Arnold folder if present
	insert_after_folder = group.findFolder('Render') or group.findFolder('Arnold')

	lava_folder = hou.FolderParmTemplate('folder_lava', "Lava")

	# Lava shading
	shading_folder = hou.FolderParmTemplate('folder_lava_shading', "Shading")
	shading_folder.addParmTemplate(hou.ToggleParmTemplate('lv_matte','Matte shading', False))

	# Lava visibility
	visibility_folder = hou.FolderParmTemplate('folder_lava_visibility', "Visibility")
	visibility_folder.addParmTemplate(hou.ToggleParmTemplate('lv_visibility_primary','Visible to camera rays', True))
	visibility_folder.addParmTemplate(hou.ToggleParmTemplate('lv_visibility_shadows_recv','Receive shadows', True))
	visibility_folder.addParmTemplate(hou.ToggleParmTemplate('lv_visibility_shadows_cast','Cast shadows', True))
	visibility_folder.addParmTemplate(hou.ToggleParmTemplate('lv_visibility_shadows_self','Self shadows', True))

	lava_folder.addParmTemplate(shading_folder)
	lava_folder.addParmTemplate(visibility_folder)
	lava_folder.addParmTemplate(hou.ToggleParmTemplate('lv_skip','Skip', False))


	if insert_after_folder:
		group.insertAfter(insert_after_folder, lava_folder)
	else:
		group.append(lava_folder)
	
	node.setParmTemplateGroup(group)


def on_geo_created(node, node_type):
	if node:
		add_geometry_lava_parameters(node)


def on_instance_created(node, node_type):
	if node:
		add_geometry_lava_parameters(node)