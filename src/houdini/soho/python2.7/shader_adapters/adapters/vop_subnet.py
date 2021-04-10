from ..vop_node_adapter_base import VopNodeSubnetAdapterBase


class VopNodeSubnet(VopNodeSubnetAdapterBase):
	#def __init__(self, inputs, outputs):
	#	super(VopNodeSubnet, self).__init__(inputs, outputs)
	
	def getSlangTemplate(self):
		return ""

	@classmethod
	def vopTypeName(cls):
		return "subnet"