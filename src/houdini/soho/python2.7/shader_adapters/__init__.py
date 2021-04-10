from adapters import *

from vop_node_adapter_socket import VopNodeSocket
from vop_node_adapter_registry import VopNodeAdapterRegistry
from vop_node_adapter_context import VopNodeAdapterContext
from vop_node_adapter_processor import VopNodeAdapterProcessor

def getVopNodeAdapter(vop_node):
	if vop_node.type().category().name() != 'Vop':
		raise ValueError("Non VOP node %s provided !!!", vop_node.path())

	vop_node_adapter_context = VopNodeAdapterContext(vop_node)

	vop_type_name = vop_node.type().name()
	if VopNodeAdapterRegistry.hasRegisteredAdapterType(vop_type_name):
		adapter_class = VopNodeAdapterRegistry.getAdapterClassByTypeName(vop_type_name)
		return adapter_class(vop_node_adapter_context)

	else:
		raise Exception('No vop node adapter of vop type "%s" registered for %s !!!' % (vop_node.type().name(), vop_node.path()))

