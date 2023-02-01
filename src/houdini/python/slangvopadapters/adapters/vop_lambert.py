from ..vop_node_adapter_base import VopNodeAdapterBase
from .. import code


class VopNodeLambert(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "lambert"

	@classmethod
	def generateCode(cls, vop_node_ctx):
		for i in super(VopNodeLambert, cls).generateCode(vop_node_ctx): yield i

		out_color = vop_node_ctx.outputs['clr']
		if out_color.isConnected():
			yield code.Assign(out_color.var_name, "123")