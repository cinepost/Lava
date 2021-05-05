from ..vop_node_adapter_base import VopNodeAdapterBase
from ..code_template import CodeTemplate
from ..exceptions import *

class VopNodeOutput(VopNodeAdapterBase):
	@classmethod
	def getTemplateContext(cls, vop_node):
		return {
			'SLANG_CONTEXT_TYPE': vop_node.parm('contexttype').evalAsString(),
		}

	@classmethod
	def vopTypeName(cls):
		return "output"

	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% extends "VopNodeAdapterBase" %}
		
		{% block BODY %}
			{{ CREATOR_COMMENT }}
			// {{ NODE_CONTEXT.SLANG_CONTEXT_TYPE }}
		{% endblock %}
		"""