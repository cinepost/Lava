from ..vop_node_adapter_base import VopNodeAdapterBase, VopNodeNetworkAdapterBase
from ..exceptions import *
from .vop_generic_subnet import VopNodeGenericSubnet

from .. import code

class VopNodeMaterialbuilder(VopNodeNetworkAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "materialbuilder"

	@classmethod
	def generateCode(cls, vop_node_ctx):
		shader_name = vop_node_ctx.vop_node_wrapper.name()

		block = code.Block([])

		# Append children variables
		for adapter, context in vop_node_ctx.children():
			# Append children variables
			for generable in adapter.generateVariables(context):
				block.append(generable)

		# Shader variables
		shader_vars = []

		for adapter, context in vop_node_ctx.children():
			# Append shader variables
			for generable in adapter.generateShaderVariables(context):
				shader_vars += [generable]

			# Append children code
			for generable in adapter.generateCode(context):
				block.append(generable)

		func = code.FunctionBody(
			code.FunctionDeclaration(code.Value("ShadingResult", "evalMaterial"), shader_vars),
			block
		)

		#yield func

		src = code.Source("/home/max/vop_tests/{}.slang".format(shader_name), [
			code.Include("qqqq.slang"), 
			code.Import(" Scene.Camera.Camera"), 
			func
		])
		
		yield src