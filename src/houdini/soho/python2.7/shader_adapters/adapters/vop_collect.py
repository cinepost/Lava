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
