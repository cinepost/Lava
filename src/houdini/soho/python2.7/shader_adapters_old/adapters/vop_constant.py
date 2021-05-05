from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate

class VopNodeConstant(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "constant"

	@classmethod
	def getTemplateContext(cls, vop_node):
		slang_type = "undef"
		slang_value = ""
		consttype = vop_node.parm('consttype').evalAsString()
		if consttype == "color":
			slang_type = "float3"
			slang_value = "%s, %s, %s" % (vop_node.parm('colordefr').evalAsFloat(), vop_node.parm('colordefg').evalAsFloat(), vop_node.parm('colordefb').evalAsFloat())
		
		return {
			'SLANG_CONST_TYPE': slang_type,
			'SLANG_CONST_VALUE': slang_value,
		}

	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% extends "VopNodeAdapterBase" %}
		
		{% block BODY %}
			{{ CREATOR_COMMENT }}
			{{ OUTPUTS.Value.VAR_NAME }} = {{ NODE_CONTEXT.SLANG_CONST_TYPE }}({{ NODE_CONTEXT.SLANG_CONST_VALUE }});
		{% endblock %}
		"""
