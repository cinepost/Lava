import hou
from lava_version import LAVA_VERSION_STRING


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
