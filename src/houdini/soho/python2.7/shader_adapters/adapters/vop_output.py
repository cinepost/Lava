from ..vop_node_adapter_base import VopNodeAdapterBase
from ..exceptions import *
from .. import code


class VopNodeOutput(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "output"

	@classmethod
	def generateVariables(cls, vop_node_ctx):
		return
		yield

	@classmethod
	def allowedInShadingContext(cls, vop_node_ctx):
		if vop_node_ctx.parms.get("contexttype") == 'surface' and vop_node_ctx.shading_context == 'surface':
			return True

		if vop_node_ctx.parms.get("contexttype") == 'displace' and vop_node_ctx.shading_context == 'displacement':
			return True

		return False

	@classmethod
	def generateCode(cls, vop_node_ctx):
		#if all([not inp.isConnected() for inp in vop_node_ctx.inputs.values()]):
		#	return 
		#	yield

		for i in super(VopNodeOutput, cls).generateCode(vop_node_ctx): yield i

		for input_name in vop_node_ctx.inputs:				
			inp = vop_node_ctx.inputs[input_name]
			if inp.isConnected():
				yield code.Value(inp.var_type, inp.var_name)