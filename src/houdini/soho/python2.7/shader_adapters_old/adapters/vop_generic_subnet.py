from ..vop_node_adapter_base import VopNodeNetworkAdapterBase
from ..code_template import CodeTemplate

class VopNodeGenericSubnet(VopNodeNetworkAdapterBase):
	
	@classmethod
	def vopTypeName(cls):
		return "__generic__subnet__"


	@classmethod
	def getCodeTemplateString(cls):
		return r"""

		{% extends "VopNodeNetworkAdapterBase" %}

		{% block BODY %}
			{{ CREATOR_COMMENT }}
			{{ super() }}
		{% endblock %}

		"""

		return r"""

		{% extends "VopNodeNetworkAdapterBase" %}

		{% block VARS %}
			{% if not PARENT %}{{ CHILDREN_CODE.args }}{% endif %}

			{% for child_path in CHILDREN %}
				{% set child = CHILDREN[child_path] %}
				{% if child.OUTPUTS %}
					// Declares: {{child_path}}
					{% for output_name in child.OUTPUTS %}
						{% set output = child.OUTPUTS[output_name] %}
						{{ output.VAR_TYPE }} {{ output.VAR_NAME}};
					{% endfor %}
				{% endif %}
			{% endfor %}
			{{ super() }}
		{% endblock %}

		{% block TEMP %}
			{{ CHILDREN_CODE.temp }}
			{{ super() }}
		{% endblock %}

		{% block BODY %}
			{{ CREATOR_COMMENT }}
			{% if PARENT %}{{ CHILDREN_CODE.args }}{% endif %}
			{
				{# Root node should declare all temporary variables right inside block #}
				{% if not PARENT %}{{ CHILDREN_CODE.temp }}{% endif %}
				{{ CHILDREN_CODE.body }}
			}
			{{ super() }}
		{% endblock %}
		"""