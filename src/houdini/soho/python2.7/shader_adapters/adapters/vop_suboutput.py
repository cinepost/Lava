from ..vop_node_adapter_base import VopNodeAdapterBase
from .. import code


class VopNodeSubOutput(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "suboutput"

	@classmethod
	def generateVariables(cls, vop_node_ctx):
		return
		yield

	@classmethod
	def generateCode(cls, vop_node_ctx):
		for i in super(VopNodeSubOutput, cls).generateCode(vop_node_ctx): yield i

		inp_name, inp = vop_node_ctx.inputs.items()[0]
		yield code.Assign(vop_node_ctx.parent_context.outputs[inp_name].var_name, inp.var_name)