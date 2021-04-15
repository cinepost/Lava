from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate
from ..exceptions import *

class VopNodeOutput(VopNodeAdapterBase):
	def getSlangTemplate(self, slang_context=None):
		if len(self.context.outputs) != 1:
			raise VopAdapterOutputsCountMismatchError(self)
		
		codeTemplate = CodeTemplate()

		return codeTemplate

	@classmethod
	def vopTypeName(cls):
		return "output"