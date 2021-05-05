from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate
from ..exceptions import *

class VopNodeMultiply(VopNodeAdapterBase):
	def getSlangTemplate(self, slang_context=None):
		if len(self.context.outputs) != 1:
			raise VopAdapterOutputsCountMismatchError(self)
		
		codeTemplate = CodeTemplate()

		return codeTemplate

	@classmethod
	def vopTypeName(cls):
		return "multiply"

	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% extends "VopNodeAdapterBase" %}
		
		{% block BODY %}
			{{ CREATOR_COMMENT }}
			{{ OUTPUTS.product.VAR_NAME }} = {% for input in INPUTS %}{{ INPUTS[input].VAR_NAME }}{% if not loop.last %} * {% endif %}{% endfor %};
		{% endblock %}
		"""