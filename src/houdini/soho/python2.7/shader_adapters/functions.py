from vop_node_adapter_registry import VopNodeAdapterRegistry
from vop_node_adapter_context import VopNodeAdapterContext

def getVopNodeAdapter(vop_node):
	if vop_node.type().category().name() != 'Vop':
		raise ValueError("Non VOP node %s provided !!!", vop_node.path())

	vop_node_adapter_context = VopNodeAdapterContext(vop_node)

	vop_type_name = vop_node.type().name()
	if VopNodeAdapterRegistry.hasRegisteredAdapterType(vop_type_name):
		adapter_class = VopNodeAdapterRegistry.getAdapterClassByTypeName(vop_type_name)
		#return adapter_class(vop_node_adapter_context)
		return adapter_class
	else:
		raise Exception('No vop node adapter of vop type "%s" registered for %s !!!' % (vop_node.type().name(), vop_node.path()))

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
	if vex_data_type_name == 'vector': return 'float3'
	if vex_data_type_name == 'vector4': return 'float4'
	if vex_data_type_name == 'vector2': return 'float2'
	if vex_data_type_name == 'color': return 'float3'
	if vex_data_type_name == 'color4': return 'float4'
	if vex_data_type_name == 'vector4': return 'float4'
	
	return vex_data_type_name
	#raise ValueError('Unsupported vex data type "%s" !!!' % vex_data_type_name)
