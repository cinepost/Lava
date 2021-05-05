from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate

class VopNodeSubInput(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "subinput"

	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% extends "VopNodeAdapterBase" %}
		
		{% block BODY %}
		{% endblock %}
		"""