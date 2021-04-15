from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate

class VopNodeSubnet(VopNodeAdapterBase):
	
	@classmethod
	def vopTypeName(cls):
		return "subnet"

	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% block ARGS %}
			{% for output_name in OUTPUTS %}
			{{ OUTPUTS[output_name].ARG_TYPE }} {{ OUTPUTS[output_name].ARG_NAME }};
			{% endfor %}
		{% endblock %}

		{% block BODY %}
			{{ CREATOR_COMMENT }}
			{
			}
		{% endblock %}
		"""