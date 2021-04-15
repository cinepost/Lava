from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate
from ..exceptions import *

class VopNodeParameter(VopNodeAdapterBase):
	def getSlangTemplate(self, slang_context=None):
	
		codeTemplate = CodeTemplate()

		if self.context.vopNode.parm('exportcontext').eval() != slang_context:
			return codeTemplate

		codeTemplate.addLine("/* BEGIN CODE BY: ${OP_PATH} */")
		codeTemplate.addLine("Ce = product1;")
		codeTemplate.addLine("/* END CODE BY: ${OP_PATH} */")

		return codeTemplate

	def getOutputs(self, slang_context=None):
		if self.context.vopNode.parm('exportcontext').eval() != slang_context:
			return []

		return super(VopNodeParameter, self).getOutputs(slang_context)

	@classmethod
	def vopTypeName(cls):
		return "parameter"