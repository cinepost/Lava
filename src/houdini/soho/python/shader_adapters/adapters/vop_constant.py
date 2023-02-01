from ..vop_node_adapter_base import VopNodeAdapterBase
from utils import getDefaultTypeValuePair

from .. import code


class VopNodeConstant(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "constant"

	@classmethod
	def generateCode(cls, vop_node_ctx):
		for i in super(VopNodeConstant, cls).generateCode(vop_node_ctx): yield i

		default_type, default_value = getDefaultTypeValuePair(vop_node_ctx, "consttype")

		yield code.Assign(vop_node_ctx.outputs.values()[0].var_name, default_value)