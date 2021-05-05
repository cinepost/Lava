from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate
from ..exceptions import *

from ..code import SlangVariable

class VopNodeParameter(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "parameter"

	@classmethod
	def getTemplateContext(cls, vop_node):
		return {
			'SCOPE': vop_node.parm('parmscope').evalAsString(),
			'PARM_NAME': vop_node.parm('parmname').evalAsString(),
			'EXPORT': vop_node.parm('exportparm').evalAsString(),
		}

	@classmethod
	def mangleOutputName(cls, output_name, template_context):
		return output_name + "_tmp"

	@classmethod
	def preprocess(cls, vop_node, slang_code_context):
		parm_type_str = vop_node.parm('parmtype').evalAsString()
		parm_name = vop_node.parm('parmname').evalAsString()
		slang_code_context.declareShaderParameter(SlangVariable(parm_type_str, parm_name, default_value = {0, 0, 0}))

	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% extends "VopNodeAdapterBase" %}

		{% block BODY %}
			{% for output_name in OUTPUTS %}
			{{ OUTPUTS[output_name].VAR_TYPE }} {{ OUTPUTS[output_name].VAR_NAME }} = lalala;
			{% endfor %}
		{% endblock %}
		"""
