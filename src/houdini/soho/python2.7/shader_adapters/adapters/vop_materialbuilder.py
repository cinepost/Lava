from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate
from ..exceptions import *

class VopNodeMaterialbuilder(VopNodeAdapterBase):
	def getSlangTemplate(self, slang_context=None):
		if len(self.context.filterOutputs("surface")) != 1:
			raise VopAdapterOutputsCountMismatchError(self, 'Exactly one output of type "surface" is expected !!!')

		if slang_context == 'surface':
			return self.getSlangSurfaceTemplate()
		elif slang_context == 'displacement':
			return self.getSlangDisplacementTemplate()
		
		raise ValueError('Unknown slang context %s pased to MaterialBuilder adapter !!!' % slang_context)


	@classmethod
	def vopTypeName(cls):
		return "materialbuilder"

	def getSlangSurfaceTemplate(self):
		codeTemplate = CodeTemplate()

		codeTemplate.addLine("surface ${FUNC_NAME}(${ARGS}) {")
		codeTemplate.addLine("/* BEGIN CODE BY: ${OP_PATH} */")
		codeTemplate.addLine("${SUPER_BLOCK}")
		codeTemplate.addLine("/* END CODE BY: ${OP_PATH} */")
		codeTemplate.addLine("}")

		return codeTemplate

	def getSlangDisplacementTemplate(self):
		codeTemplate = CodeTemplate()

		codeTemplate.addLine("displacement ${FUNC_NAME}(${ARGS}) {")
		codeTemplate.addLine("/* BEGIN CODE BY: ${OP_PATH} */")
		codeTemplate.addLine("${SUPER_BLOCK}")
		codeTemplate.addLine("/* END CODE BY: ${OP_PATH} */")
		codeTemplate.addLine("}")

		return codeTemplate