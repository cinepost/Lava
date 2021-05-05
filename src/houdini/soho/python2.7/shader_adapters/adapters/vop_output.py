from ..vop_node_adapter_base import VopNodeAdapterBase
from ..exceptions import *
from .. import code


class VopNodeOutput(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "output"

	@classmethod
	def generateCode(cls, vop_node_ctx):
		if len(vop_node_ctx.inputs) > 0:
			for i in super(VopNodeOutput, cls).generateCode(vop_node_ctx): yield i

			for input_name in vop_node_ctx.inputs:
				inp = vop_node_ctx.inputs[input_name]

				yield code.Value(inp.var_type, inp.var_name)