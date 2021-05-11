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

		if vop_node_ctx.shading_context == 'surface':
			for i in cls.generateSurfaceShadingFiles(vop_node_ctx): yield i
		elif vop_node_ctx.shading_context == 'displacement':
			for i in cls.generateDisplacementShadingFiles(vop_node_ctx): yield i
		else:
			return
			yield

	@classmethod
	def generateSurfaceShadingFiles(cls, vop_node_ctx):
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

		for adapter, context in vop_node_ctx.children():
			# Append children code
			for generable in adapter.generateCode(context):
				block.append(generable)

		func = code.FunctionBody(
			code.FunctionDeclaration(code.Value("ShadingResult", "evalMaterial"), shader_vars),
			block
		)

		src = code.Source("/home/max/vop_tests/{}.{}.slang".format(shader_name,  vop_node_ctx.shading_context), [
			code.Comment("%s shader" % vop_node_ctx.shading_context),
			code.EmptyLine(),
			code.Include("qqqq.slang"), 
			code.Import(" Scene.Camera.Camera"), 
			code.EmptyLine(),
			code.Ifndef("VOP_SHADING"),
			code.Define("VOP_SHADING"),
			code.Endif(),
			code.EmptyLine(),
			func
		])
		
		yield src

	@classmethod
	def generateDisplacementShadingFiles(cls, vop_node_ctx):
		return
		yield