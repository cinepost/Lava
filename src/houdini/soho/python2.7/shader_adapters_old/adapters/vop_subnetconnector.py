from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate
from ..functions import vexDataTypeToSlang

class VopNodeSubnetConnector(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "subnetconnector"

	@classmethod
	def getTemplateContext(cls, vop_node):
		out_name = vop_node.parm('parmname').evalAsString()
		out_type_name = vop_node.parm('parmtype').evalAsString()
		out_connector_kind = vop_node.parm('connectorkind').evalAsString()
		return {
			'CONNECTOR_KIND': out_connector_kind,
			'OUTPUT_VAR_NAME': out_name,
			'OUTPUT_VAR_TYPE': vexDataTypeToSlang(out_type_name),
		}

		return result

	@classmethod
	def mangleOutputName(cls, output_name, template_context):
		print "CONNECTOR_KIND", template_context
		return output_name + "_tmp"

	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% extends "VopNodeAdapterBase" %}
		
		{% block BODY %}
		{{ CREATOR_COMMENT }}
		{#
		{{ NODE_CONTEXT.OUTPUT_VAR_NAME }} = {{ INPUTS.suboutput.VAR_NAME }};
		#}
		{% endblock %}
		"""