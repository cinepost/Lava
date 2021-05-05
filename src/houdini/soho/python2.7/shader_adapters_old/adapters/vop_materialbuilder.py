from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate
from ..exceptions import *

from .vop_generic_subnet import VopNodeGenericSubnet


class VopNodeMaterialbuilder(VopNodeGenericSubnet):
	@classmethod
	def vopTypeName(cls):
		return "materialbuilder"

	@classmethod
	def getCodeTemplateString(cls):
		s = super(VopNodeMaterialbuilder, cls).getCodeTemplateString()

		#return s

		return r"""
		{% extends "VopNodeGenericSubnet" %}

		{% block BODY %}
		ShadingResult evalMaterial(ShadingData sd, LightData light, float shadowFactor) {
    		{{ super() }}
		};
		{% endblock %}
		"""