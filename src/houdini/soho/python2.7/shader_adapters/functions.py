from vop_node_adapter_registry import VopNodeAdapterRegistry
from vop_node_adapter_context import VopNodeAdapterContext

def getVopNodeAdapter(vop_node_wrapper):
	if vop_node_wrapper.type().category().name() != 'Vop':
		raise ValueError("Non VOP node %s provided !!!", vop_node_wrapper.path())

	vop_node_adapter_context = VopNodeAdapterContext(vop_node_wrapper)

	vop_type_name = vop_node_wrapper.type().name()
	if VopNodeAdapterRegistry.hasRegisteredAdapterType(vop_type_name):
		adapter_class = VopNodeAdapterRegistry.getAdapterClassByTypeName(vop_type_name)
		if adapter_class:
			return adapter_class
	
	if vop_node_wrapper.isSubNetwork():
		# If this it a subnetwork node, we can use generic subnet adapter
		subnet_adapter_class = VopNodeAdapterRegistry.getAdapterClassByTypeName("__generic__subnet__")
		if subnet_adapter_class:
			return subnet_adapter_class

	print('No vop node adapter of vop type "%s" registered for %s !!!' % (vop_node_wrapper.type().name(), vop_node_wrapper.path()))
	return None

def incrLastStringNum(text):
	import re

	match = re.search('(\d+)([^\d]*)$', text)
	if not match:
		return text + "1"
		
	new_num = int(match.group(1)) + 1
	return text.replace(match.group(0), '%d%s' % (new_num, match.group(2)))

def vexDataTypeToSlang(vex_data_type_name):
	if vex_data_type_name == 'int': return 'int'
	if vex_data_type_name == 'float': return 'float'
	if vex_data_type_name == 'vector2': return 'float2'
	if vex_data_type_name in ['color', 'vector', 'point', 'normal']: return 'float3'
	if vex_data_type_name in ['color4', 'vector4', 'coloralpha']: return 'float4'
	
	return vex_data_type_name
	#raise ValueError('Unsupported vex data type "%s" !!!' % vex_data_type_name)
