from ..vop_node_adapter_base import VopNodeAdapterBase
from ..exceptions import *
from utils import getDefaultTypeValuePair

from .. import code


class VopNodeParameter(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "parameter"

	@classmethod
	def outputVariableName(cls, vop_node_ctx, output_name):
		output_name = super(VopNodeParameter, cls).outputVariableName(vop_node_ctx, output_name)

		if vop_node_ctx.parms.get("exportparm") != "off":
			output_name += "_tmp"

		return output_name

	@classmethod
	def generateShaderVariables(cls, vop_node_ctx):
		shader_variables = [i for i in super(VopNodeParameter, cls).generateShaderVariables(vop_node_ctx)]

		exportparm = vop_node_ctx.parms.get("exportparm")
		parmscope = vop_node_ctx.parms.get("parmscope")
		parmname = vop_node_ctx.parms.get("parmname")

		export = False
		if (exportparm == "whenconnected" and len(vop_node_ctx.inputs) == 1) or (exportparm == "on"):
			export = True

		shader_variables += [code.Value("int", parmname)]

		for var in shader_variables:
			yield var

	@classmethod
	def generateSubnetVariables(cls, vop_node_ctx):
		for i in super(VopNodeParameter, cls).generateSubnetVariables(vop_node_ctx): yield i

		if vop_node_ctx.parms.get("parmscope") == "subnet":
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

		exportparm = vop_node_ctx.parms.get("exportparm")
		parmscope = vop_node_ctx.parms.get("parmscope")
		parmname = vop_node_ctx.parms.get("parmname")

		export = False
		if (exportparm == "whenconnected" and len(vop_node_ctx.inputs) == 1) or (exportparm == "on"):
			export = True
		
		if export:
			if "input" in vop_node_ctx.inputs:
				yield code.Assign(parmname, vop_node_ctx.inputs["input"].var_name)
			else:
				yield code.Assign(vop_node_ctx.outputs.values()[0].var_name, vop_node_ctx.outputs.keys()[0])