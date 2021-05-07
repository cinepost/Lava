from ..vop_node_adapter_base import VopNodeAdapterBase

class VopNodeCollect(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "collect"

	@classmethod
	def generateVariables(cls, vop_node_ctx):
		return
		yield

	@classmethod
	def generateCode(cls, vop_node_ctx):
		for i in super(VopNodeCollect, cls).generateCode(vop_node_ctx): yield i

		#for input_adapter, input_vop_node_ctx in vop_node_ctx.inputNodes():
		#	for i in input_adapter.generateCode(input_vop_node_ctx): yield i