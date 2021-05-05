from ..functions import vexDataTypeToSlang

def getDefaultTypeValuePair(vop_node_ctx, type_parm_name):
	parmtype = vop_node_ctx.parms.get(type_parm_name)

	def_value_str = "0"

	if not parmtype:
		raise ValueError("Unable to get default type from %s" % vop_node_ctx.vop_node_path)

	print "parmtype", vop_node_ctx.vop_node_path, parmtype

	# 3 components
	if parmtype == "color":
		val1 = vop_node_ctx.parms.get("colordefr") or 0
		val2 = vop_node_ctx.parms.get("colordefg") or 0
		val3 = vop_node_ctx.parms.get("colordefb") or 0
		def_value_str = "float3({}, {}, {})".format(val1, val2, val3)

	if parmtype == "float3":
		val1 = vop_node_ctx.parms.get("float3def1") or 0
		val2 = vop_node_ctx.parms.get("float3def2") or 0
		val3 = vop_node_ctx.parms.get("float3def3") or 0
		def_value_str = "float3({}, {}, {})".format(val1, val2, val3)
		
	if parmtype == "vector":
		val1 = vop_node_ctx.parms.get("vectordef1") or 0
		val2 = vop_node_ctx.parms.get("vectordef2") or 0
		val3 = vop_node_ctx.parms.get("vectordef3") or 0
		def_value_str = "float3({}, {}, {})".format(val1, val2, val3)

	# 4 components
	if parmtype == "coloralpha":
		val1 = vop_node_ctx.parms.get("color4defr") or 0
		val2 = vop_node_ctx.parms.get("color4defg") or 0
		val3 = vop_node_ctx.parms.get("color4defb") or 0
		val4 = vop_node_ctx.parms.get("color4defa") or 0
		def_value_str = "float4({}, {}, {}, {})".format(val1, val2, val3, val4)

	if parmtype == "float4":
		val1 = vop_node_ctx.parms.get("float4def1") or 0
		val2 = vop_node_ctx.parms.get("float4def2") or 0
		val3 = vop_node_ctx.parms.get("float4def3") or 0
		val4 = vop_node_ctx.parms.get("float4def4") or 0
		def_value_str = "float4({}, {}, {}, {})".format(val1, val2, val3, val4)

	return vexDataTypeToSlang(parmtype), def_value_str