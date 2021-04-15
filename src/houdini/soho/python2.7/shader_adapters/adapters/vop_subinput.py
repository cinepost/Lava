from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate

class VopNodeSubinput(VopNodeAdapterBase):
	def getSlangTemplate(self, slang_context=None):
		#return super(VopNodeSubnet, self).getSlangTemplate()
		return CodeTemplate()

	@classmethod
	def vopTypeName(cls):
		return "subinput"