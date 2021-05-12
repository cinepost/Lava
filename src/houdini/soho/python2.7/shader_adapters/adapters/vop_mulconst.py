from ..vop_node_adapter_base import VopNodeAdapterBase
from .. import code


class VopNodeMulconst(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "mulconst"

	@classmethod
	def generateCode(cls, vop_node_ctx):
		for i in super(VopNodeMulconst, cls).generateCode(vop_node_ctx): yield i

		yield code.Assign(vop_node_ctx.outputs['scaled'].var_name, code.Multiply(vop_node_ctx.inputs['val'].var_name, vop_node_ctx.parms.get("mulconst")))