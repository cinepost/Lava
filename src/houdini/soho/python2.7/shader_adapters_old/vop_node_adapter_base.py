import hou

from vop_node_adapter_registry import VopNodeAdapterRegistry
from vop_node_adapter_context import VopNodeAdapterContext
from code_template import CodeTemplate

class VopNodeAdapterAPI(object):
	@property 
	def context(self):
		return self._context

	@classmethod
	def vopTypeName(cls):
		raise NotImplementedError

	#def getSlangTemplate(self, slang_context=None):
	#	raise NotImplementedError

	#def getOutputs(self, slang_context=None):
	#	return self._context.outputs

	@classmethod
	def getCodeTemplateString(cls):
		pass

	@classmethod
	def getTemplateContext(cls, vop_node):
		return {}

	@classmethod
	def preprocess(cls, vop_node, slang_code_context):
		pass

	@classmethod
	def mangleOutputName(cls, output_name, template_context):
		return output_name

class VopNodeAdapterBase(VopNodeAdapterAPI):
	__metaclass__ = VopNodeAdapterRegistry
	__base__ = True

	def __init__(self, context):
		super(VopNodeAdapterBase, self).__init__()
		self._context = context

	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% block BODY %}
		{% endblock %}
		"""

class VopNodeNetworkAdapterBase(VopNodeAdapterAPI):
	__metaclass__ = VopNodeAdapterRegistry
	__base__ = True

	def __init__(self, context):
		super(VopNodeNetworkAdapterBase, self).__init__()
		self._context = context

	@classmethod
	def getCodeTemplateString(cls):
		return r"""
		{% block VARS %}
		{% endblock %}

		{% block TEMP %}
		{% endblock %}

		{% block BODY %}
		{% endblock %}
		"""