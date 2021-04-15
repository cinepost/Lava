from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate

class VopNodeConstant(VopNodeAdapterBase):
	def getSlangTemplate(self, slang_context=None):
		#return super(VopNodeSubnet, self).getSlangTemplate()
		return CodeTemplate()

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
		{% block ARGS %}
			{{ NODE_CONTEXT.SLANG_CONST_TYPE }} {{ OUTPUTS.Value.ARG_NAME }};
		{% endblock %}

		{% block BODY %}
			{{ CREATOR_COMMENT }}
			{{ OUTPUTS.Value.ARG_NAME }} = {{ NODE_CONTEXT.SLANG_CONST_TYPE }}({{ NODE_CONTEXT.SLANG_CONST_VALUE }});
		{% endblock %}
		"""
