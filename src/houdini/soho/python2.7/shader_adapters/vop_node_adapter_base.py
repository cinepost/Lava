import hou

from vop_node_adapter_registry import VopNodeAdapterRegistry
from vop_node_adapter_context import VopNodeAdapterContext
from code_template import CodeTemplate

class VopNodeAdapterAPI(object):
	@property 
	def context(self):
		return self._context

	@classmethod
	def vopTypeName(cls):
		raise NotImplementedError

	def getSlangTemplate(self, slang_context=None):
		raise NotImplementedError

	def getOutputs(self, slang_context=None):
		return self._context.outputs

	@classmethod
	def getCodeTemplateString(cls):
		return ""

	@classmethod
	def getTemplateContext(cls, vop_node):
		return {}

class VopNodeAdapterBase(VopNodeAdapterAPI):
	__metaclass__ = VopNodeAdapterRegistry
	__base__ = True

	def __init__(self, context):
		super(VopNodeAdapterBase, self).__init__()
		self._context = context
