import os
import hou
from .utils_common import findConvertedLtxTexturePaths

def deleteConvertedLTXTexturesTool():
	paths = findConvertedLtxTexturePaths(hou.node("/"))

	if not paths:
		hou.ui.displayMessage("No converted LTX textures found for this project.")
		return

	details_text = "Found textures: \n"
	for path in paths:
		details_text += "%s \n" % path

	if hou.ui.displayConfirmation("Do you really wand to delete %s LTX textures from filesystem ?" % len(paths), severity=hou.severityType.Warning, help=None, title="Delete LTX textures from filesystem", details=details_text, details_label=None, suppress=hou.confirmType.OverwriteFile):

		deleted_count = 0

		for ltx_path in paths:
			if os.path.exists(ltx_path):
				os.remove(ltx_path)
				deleted_count += 1
		
		hou.ui.displayMessage("Complete. %s LTX textures deleted from filesystem!" % deleted_count)