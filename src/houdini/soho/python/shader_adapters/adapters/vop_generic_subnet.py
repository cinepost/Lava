from ..vop_node_adapter_base import VopNodeNetworkAdapterBase
from ..functions import getVopNodeAdapter

from .. import code

class VopNodeGenericSubnet(VopNodeNetworkAdapterBase):
	
	@classmethod
	def vopTypeName(cls):
		return "__generic__subnet__"


	@classmethod
	def generateCode(cls, vop_node_ctx):
		super(VopNodeGenericSubnet, cls).generateCode(vop_node_ctx)
		block = code.Block([])

		yield block
