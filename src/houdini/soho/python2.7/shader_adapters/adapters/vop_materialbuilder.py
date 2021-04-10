from ..vop_node_adapter_base import VopNodeSubnetAdapterBase
from ..exceptions import *

class VopNodeMaterialbuilder(VopNodeSubnetAdapterBase):
	def getSlangTemplate(self):
		if len(self.context.filterOutputs("surface")) != 1:
			raise VopAdapterOutputsCountMismatchError(self, 'Exactly one output of type "surface" is expected !!!')

		return super(VopNodeMaterialbuilder, self).getSlangTemplate()

	@classmethod
	def vopTypeName(cls):
		return "materialbuilder"