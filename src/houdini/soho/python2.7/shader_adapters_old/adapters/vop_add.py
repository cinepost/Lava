from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate
from ..exceptions import *

from ..code import SlangVariable

class VopNodeAdd(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "add"

	@classmethod
	def preprocess(cls, vop_node, slang_code_context):
		slang_code_context.declareVariable(SlangVariable("vector", "sum"))


	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% extends "VopNodeAdapterBase" %}

		{% block BODY %}
			{{ CREATOR_COMMENT }}
			{{ OUTPUTS.sum.VAR_NAME }} = {% for input in INPUTS %}{{ INPUTS[input].VAR_NAME }}{% if not loop.last %} + {% endif %}{% endfor %};
		{% endblock %}
		"""