from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate
from ..exceptions import *

class VopNodeAdd(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "add"

	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% block ARGS %}
			{{ OUTPUTS.sum.ARG_TYPE }} {{ OUTPUTS.sum.ARG_NAME }};
		{% endblock %}

		{% block BODY %}
			{{ CREATOR_COMMENT }}
			{{ OUTPUTS.sum.ARG_NAME }} = {% for input in INPUTS %}{{ INPUTS[input].ARG_NAME }}{% if not loop.last %} + {% endif %}{% endfor %};
		{% endblock %}
		"""