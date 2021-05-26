from ..vop_node_adapter_base import VopNodeAdapterBase
from ..exceptions import *
from .. import code

class VopNodeMultiply(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "multiply"

	@classmethod
	def generateCode(cls, vop_node_ctx):
		for i in super(VopNodeMultiply, cls).generateCode(vop_node_ctx): yield i

		assert len(vop_node_ctx.inputs) > 0

		if len(vop_node_ctx.inputs) == 1:
			yield code.Assign(vop_node_ctx.outputs['product'].var_name, vop_node_ctx.inputs["input1"].var_name)
		else:
			var_names = [ vop_node_ctx.inputs[input_name].var_name for input_name in vop_node_ctx.inputs ]
			summ = code.Multiply(var_names[0], var_names[1])
			for var_name in var_names[2:]:
				summ = code.Multiply(summ, var_name)
			
			yield code.Assign(vop_node_ctx.outputs['product'].var_name, summ)
