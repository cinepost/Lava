from ..vop_node_adapter_base import VopNodeAdapterBase
from .. import code


class VopNodeSubInput(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "subinput"

	@classmethod
	def generateVariables(cls, vop_node_ctx):
		return
		yield

	@classmethod
	def generateSubnetVariables(cls, vop_node_ctx):
		for i in super(VopNodeSubInput, cls).generateSubnetVariables(vop_node_ctx): yield i

		output_name = vop_node_ctx.outputNames()[0]
		parent_input_name = output_name[1:]
		parent_input_socket = vop_node_ctx.parent_context.inputs[parent_input_name]
		yield code.Assign(code.Value(parent_input_socket.var_type, output_name), parent_input_socket.var_name)

	@classmethod
	def generateCode(cls, vop_node):
		return
		yield