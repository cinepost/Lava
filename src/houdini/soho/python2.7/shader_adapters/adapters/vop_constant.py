from ..vop_node_adapter_base import VopNodeAdapterBase
from .. import code


class VopNodeConstant(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "constant"

	@classmethod
	def generateCode(cls, vop_node_ctx):
		for i in super(VopNodeConstant, cls).generateCode(vop_node_ctx): yield i

		consttype = vop_node_ctx.parms.get("consttype")

		def_value_str = "0";

		# 3 components
		if consttype == "color":
			val1 = vop_node_ctx.parms.get("colordefr") or 0
			val2 = vop_node_ctx.parms.get("colordefg") or 0
			val3 = vop_node_ctx.parms.get("colordefb") or 0
			def_value_str = "float3({}, {}, {})".format(val1, val2, val3)
		
		if consttype == "vector":
			val1 = vop_node_ctx.parms.get("float3def1") or 0
			val2 = vop_node_ctx.parms.get("float3def2") or 0
			val3 = vop_node_ctx.parms.get("float3def3") or 0
			def_value_str = "float3({}, {}, {})".format(val1, val2, val3)

		# 4 components
		if consttype == "coloralpha":
			val1 = vop_node_ctx.parms.get("color4defr") or 0
			val2 = vop_node_ctx.parms.get("color4defg") or 0
			val3 = vop_node_ctx.parms.get("color4defb") or 0
			val4 = vop_node_ctx.parms.get("color4defa") or 0
			def_value_str = "float4({}, {}, {}, {})".format(val1, val2, val3, val4)

		if consttype == "float4":
			val1 = vop_node_ctx.parms.get("float4def1") or 0
			val2 = vop_node_ctx.parms.get("float4def2") or 0
			val3 = vop_node_ctx.parms.get("float4def3") or 0
			val4 = vop_node_ctx.parms.get("float4def4") or 0
			def_value_str = "float4({}, {}, {}, {})".format(val1, val2, val3, val4)

		yield code.Assign(vop_node_ctx.outputs.values()[0].var_name, def_value_str)