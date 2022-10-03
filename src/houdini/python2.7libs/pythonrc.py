import hou

from lava_version import LAVA_VERSION_STRING
from lava_houdini import add_geometry_lava_parameters, add_instance_lava_parameters, add_light_lava_parameters

def sceneLoadedEventCallback(event_type): 
	if event_type == hou.hipFileEventType.AfterLoad:

		rebuild = False
		if hasattr(hou.session, '__LAVA_VERSION_STRING__'):
			if hou.session.__LAVA_VERSION_STRING__ != LAVA_VERSION_STRING:
				rebuild = True

		# TODO: Remove. We force parameters interface rebuilding (missing/changed parameters) while in active development phase !!!
		rebuild = True

		# If UI is available we should install lava parameters on all supported node types or modify parameters if needed
		if hou.isUIAvailable():
			# Loop over all object nodes
			for node in hou.nodeType(hou.objNodeTypeCategory(), "geo").instances():
				add_geometry_lava_parameters(node, rebuild)
			
			# Loop over all instance nodes
			for node in hou.nodeType(hou.objNodeTypeCategory(), "instance").instances():
				add_instance_lava_parameters(node, rebuild)

			# Loop over all light nodes
			for node in hou.nodeType(hou.objNodeTypeCategory(), "hlight::2.0").instances():
				add_light_lava_parameters(node, rebuild)

		# Store current LAVA_VERSION_STRING
		hou.session.__LAVA_VERSION_STRING__ = LAVA_VERSION_STRING

hou.hipFile.addEventCallback(sceneLoadedEventCallback)