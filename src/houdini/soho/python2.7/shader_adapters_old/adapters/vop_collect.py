from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate

class VopNodeCollect(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "collect"

	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% extends "VopNodeAdapterBase" %}
		
		{% block BODY %}
			{{ CREATOR_COMMENT }}
		
		{% endblock %}
		"""
