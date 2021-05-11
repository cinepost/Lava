from ..vop_node_adapter_base import VopNodeAdapterBase
from ..exceptions import *
from .. import code


class VopNodeOutput(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "output"

	@classmethod
	def generateVariables(cls, vop_node_ctx):
		return
		yield

	@classmethod
	def allowedInShadingContext(cls, vop_node_ctx):
		if vop_node_ctx.parms.get("contexttype") == 'surface' and vop_node_ctx.shading_context == 'surface':
			return True

		if vop_node_ctx.parms.get("contexttype") == 'displace' and vop_node_ctx.shading_context == 'displacement':
			return True

		return False

	@classmethod
	def generateCode(cls, vop_node_ctx):
		if all([not inp.isConnected() for inp in vop_node_ctx.inputs.values()]):
			return 
			yield

		for i in super(VopNodeOutput, cls).generateCode(vop_node_ctx): yield i

		if vop_node_ctx.shading_context == 'surface':
			for i in cls.generateCode_surface(vop_node_ctx): yield i
		elif vop_node_ctx.shading_context == 'displacement':
			for i in cls.generateCode_displacement(vop_node_ctx): yield i

	@classmethod
	def generateCode_surface(cls, vop_node_ctx):
		for input_name in ['Cf', 'Of', 'Af', 'N', 'F']:
			input_socket = vop_node_ctx.inputs[input_name]

			if input_socket.isConnected():
				yield code.Assign('P', input_socket.var_name)

	@classmethod
	def generateCode_displacement(cls, vop_node_ctx):
		for input_name in ['P', 'N']:
			input_socket = vop_node_ctx.inputs[input_name]

			if input_socket.isConnected():
				yield code.Assign('P', input_socket.var_name)
