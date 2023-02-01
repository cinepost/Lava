from ..vop_node_adapter_base import VopNodeAdapterBase
from ..functions import vexDataTypeToSlang
from utils import getDefaultTypeValuePair

from .. import code


class VopNodeSubnetConnector(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "subnetconnector"

	@classmethod
	def generateVariables(cls, vop_node_ctx):
		return
		yield

	@classmethod
	def generateSubnetVariables(cls, vop_node_ctx):
		for i in super(VopNodeSubnetConnector, cls).generateSubnetVariables(vop_node_ctx): yield i

		if vop_node_ctx.parms.get("connectorkind") in ['input', 'inputoutput']:
			output_name = vop_node_ctx.outputNames()[0]
			parent_input_name = output_name[1:]
			parent_input_socket = vop_node_ctx.parent_context.inputs[parent_input_name]

			if parent_input_socket.var_name:
				yield code.Assign(code.Value(parent_input_socket.var_type, output_name), parent_input_socket.var_name)
			else:
				default_type, default_value = getDefaultTypeValuePair(vop_node_ctx, "parmtype")
				yield code.Assign(code.Value(default_type, output_name), default_value)

	@classmethod
	def generateCode(cls, vop_node_ctx):
		for i in super(VopNodeSubnetConnector, cls).generateCode(vop_node_ctx): yield i

		if vop_node_ctx.parms.get("connectorkind") in ['output', 'inputoutput']:
			inp = vop_node_ctx.inputs['suboutput']

			output_socket = vop_node_ctx.parent_context.outputs[vop_node_ctx.outputNames()[0]]
			yield code.Assign(output_socket.var_name, inp.var_name)