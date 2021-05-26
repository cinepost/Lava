from ..vop_node_adapter_base import VopNodeNetworkAdapterBase
from ..functions import getVopNodeAdapter

from .. import code

class VopNodeSubnet(VopNodeNetworkAdapterBase):
	
	@classmethod
	def vopTypeName(cls):
		return "subnet"

	@classmethod
	def generateVariables(cls, vop_node_ctx):
		for adapter, context in vop_node_ctx.children():
			for generable in adapter.generateVariables(context):
				yield generable

	@classmethod
	def generateCode(cls, vop_node_ctx):
		#super(VopNodeSubnet, cls).generateCode(vop_node_ctx)

		yield code.EmptyLine()
		yield code.LineComment("Code produced by: {}".format(vop_node_ctx.vop_node_path))
		
		# Append subnet variables
		for adapter, context in vop_node_ctx.children():
			for generable in adapter.generateSubnetVariables(context):
				yield generable

		block = code.Block([])

		for adapter, context in vop_node_ctx.children():
			for generable in adapter.generateCode(context):
				block.append(generable)

		yield block
