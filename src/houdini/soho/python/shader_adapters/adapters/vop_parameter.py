from ..vop_node_adapter_base import VopNodeAdapterBase
from ..exceptions import *
from ..functions import vexDataTypeToSlang
from utils import getDefaultTypeValuePair

from .. import code


class VopNodeParameter(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "parameter"

	@classmethod
	def outputVariableName(cls, vop_node_ctx, output_name):
		output_name = super(VopNodeParameter, cls).outputVariableName(vop_node_ctx, output_name)

		if cls.doExport(vop_node_ctx):
			output_name += "_tmp"

		return output_name

	@classmethod
	def generateShaderVariables(cls, vop_node_ctx):
		for i in super(VopNodeParameter, cls).generateShaderVariables(vop_node_ctx): yield i

		parmname = vop_node_ctx.parms.get("parmname")
		parmtype = vop_node_ctx.parms.get("parmtype")

		if cls.doExport(vop_node_ctx):
			default_type, default_value = getDefaultTypeValuePair(vop_node_ctx, "parmtype")
			
			assert vexDataTypeToSlang(parmtype) == default_type
			yield code.Assign(code.Value(vexDataTypeToSlang(parmtype), parmname), default_value)

	@classmethod
	def generateSubnetVariables(cls, vop_node_ctx):
		if vop_node_ctx.parms.get("parmscope") == "subnet":
			for i in super(VopNodeParameter, cls).generateSubnetVariables(vop_node_ctx): yield i
			
			output_name = "_%s" % vop_node_ctx.parms.get("parmname")
			parent_input_name = output_name[1:]
			parent_input_socket = vop_node_ctx.parent_context.inputs[parent_input_name]

			if parent_input_socket.var_name:
				yield code.Assign(code.Value(parent_input_socket.var_type, output_name), parent_input_socket.var_name)
			else:
				default_type, default_value = getDefaultTypeValuePair(vop_node_ctx, "parmtype")
				yield code.Assign(code.Value(default_type, output_name), default_value)

	@classmethod
	def generateCode(cls, vop_node_ctx):
		for i in super(VopNodeParameter, cls).generateCode(vop_node_ctx): yield i

		parmname = vop_node_ctx.parms.get("parmname")

		if cls.doExport(vop_node_ctx):
			yield code.Assign(vop_node_ctx.outputs.values()[0].var_name, parmname)

	@classmethod
	def doExport(cls, vop_node_ctx):
		exportparm = vop_node_ctx.parms.get("exportparm")
		exportcontext = vop_node_ctx.parms.get("exportcontext")

		if exportcontext == 'surface' and vop_node_ctx.shading_context != 'surface':
			return False

		if exportcontext == 'displace' and vop_node_ctx.shading_context != 'displacement':
			return False


		if (exportparm == "whenconnected" and vop_node_ctx.inputs["input"].isConnected()) or (exportparm == "on"):
			return True

		return False