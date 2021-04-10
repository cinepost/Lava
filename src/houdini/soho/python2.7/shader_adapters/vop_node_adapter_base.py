import hou

from vop_node_adapter_registry import VopNodeAdapterRegistry
from vop_node_adapter_context import VopNodeAdapterContext


class VopNodeAdapterAPI(object):
	@property 
	def context(self):
		return self._context

	@classmethod
	def vopTypeName(cls):
		raise NotImplementedError

	def getSlangTemplate(self):
		raise NotImplementedError


class VopNodeAdapterBase(VopNodeAdapterAPI):
	__metaclass__ = VopNodeAdapterRegistry
	__base__ = True

	def __init__(self, context):
		super(VopNodeAdapterBase, self).__init__()
		self._context = context

#.subnetTerminalChild('surface')

class VopNodeSubnetAdapterBase(VopNodeAdapterAPI):
	__metaclass__ = VopNodeAdapterRegistry
	__base__ = True

	def __init__(self, context):
		super(VopNodeSubnetAdapterBase, self).__init__()
		self._context = context

	def getSlangTemplate(self):
		code  = "${RETURN_TYPE} ${FUNC_NAME}(${PARAMS}) {\n"
		code += "	/* CODE BEGIN ${OP_PATH} */\n"
		code += "	/* CODE END ${OP_PATH} */\n"
		code += "}"

		return code