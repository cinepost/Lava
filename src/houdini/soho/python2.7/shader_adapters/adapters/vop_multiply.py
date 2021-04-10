from ..vop_node_adapter_base import VopNodeAdapterBase
from ..exceptions import *

class VopNodeMultiply(VopNodeAdapterBase):
	#def __init__(self, inputs, outputs):
	#	super(VopNodeMultiply, self).__init__(inputs, outputs)
	
	def getSlangTemplate(self):
		if len(self.context.outputs) != 1:
			raise VopAdapterOutputsCountMismatchError(self)
			
		return ""

	@classmethod
	def vopTypeName(cls):
		return "multiply"