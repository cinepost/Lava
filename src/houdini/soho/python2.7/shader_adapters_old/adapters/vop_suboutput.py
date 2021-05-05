from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate

class VopNodeSubOutput(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "suboutput"

	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% extends "VopNodeAdapterBase" %}
		
		{% block BODY %}
		{{ CREATOR_COMMENT }}
		{% for input_name in INPUTS %}{{ input_name }} = {{ INPUTS[input_name].VAR_NAME }};{%- endfor %}
		{% endblock %}
		"""