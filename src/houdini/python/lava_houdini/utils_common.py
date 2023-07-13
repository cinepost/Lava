import os
import hou
from lava_version import LAVA_VERSION_STRING


def generateUDIMStingIndices():
	id_strings = []
	for x in range(1, 11):
		for y in range(0, 10):
			id_strings += [str(1000 + x + y*10)]

	return id_strings


def isFileParm(parm):
	t = parm.parmTemplate()
	if t.type() == hou.parmTemplateType.String:
		if t.stringType() == hou.stringParmType.FileReference:
			return True

		return False


def isStringParm(parm):
	if parm.parmTemplate().type() == hou.parmTemplateType.String:
		return True

	return False


def findConvertedLtxTexturePaths(node):
	paths = []
	
	gUDIMStringIndices = generateUDIMStingIndices()

	for parm in node.parms():
		if isFileParm(parm) or isStringParm(parm):
			parm_value = parm.eval()
			if isinstance(parm_value, str) and parm_value != '':
				filepath = parm_value
				if filepath.lower().endswith((".exr", ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".tif", ".tiff", ".hdr")):
					udim_wildcard_pos = filepath.find('<UDIM>')
					if udim_wildcard_pos != -1:
						# UDIM texture
						for udim_idx_str in gUDIMStringIndices:
							ltx_path = filepath.replace('<UDIM>', udim_idx_str) + ".ltx"
							if os.path.isfile(ltx_path):
								paths += [ltx_path]
					else:
						# Simple texture
						ltx_path = filepath + ".ltx"
						if os.path.isfile(ltx_path):
							paths += [ltx_path]        

	for child in node.children():
		paths += findConvertedLtxTexturePaths(child)

	if paths:
		# deduplicate
		paths = list(set(paths))

	return paths


def findSubFolderByTag(parent_folder, tag_name, tag_value):
	if not parent_folder:
		return None

	if not isinstance(parent_folder, hou.FolderParmTemplate):
		raise ValueError("parent_folder should be of a hou.FolderParmTemplate class instance !!!")

	for parm_template in parent_folder.parmTemplates():
		if isinstance(parm_template, hou.FolderParmTemplate):
			if parm_template.tags().get(tag_name) == tag_value:
				return parm_template

	return None


def addParmTemplate(node, folder, parm_template):
	if not isinstance(folder, hou.FolderParmTemplate):
		raise ValueError("folder should be of a hou.FolderParmTemplate class instance !!!")

	if not isinstance(parm_template, hou.ParmTemplate):
		raise ValueError("parm_template should be of a hou.ParmTemplate class instance !!!")

	if node.parm(parm_template.name()):
		old_parm_template = node.parmTemplateGroup().find(parm_template.name())
		if old_parm_template:
			old_parm_template.setLabel(parm_template.label())
			old_parm_template.setJoinWithNext(parm_template.joinWithNext())
			if hasattr(old_parm_template, 'defaultValue'):
				old_parm_template.setDefaultValue(parm_template.defaultValue())

			node.parmTemplateGroup().replace(old_parm_template.name(), parm_template)
	
	folder.addParmTemplate(parm_template)
