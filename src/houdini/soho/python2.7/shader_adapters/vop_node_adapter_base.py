import hou

from vop_node_adapter_registry import VopNodeAdapterRegistry
from vop_node_context import VopNodeContext
from vop_node_graph import NodeWrapper
from functions import getVopNodeAdapter
import code

class VopNodeAdapterAPI(object):
	@classmethod
	def vopTypeName(cls):
		raise NotImplementedError

	@classmethod
	def generateCode(cls, vop_node_ctx):
		if not isinstance(vop_node_ctx, VopNodeContext):
			raise ValueError('vop_node_ctx should be of type VopNodeContext, got %s !!!' % type(vop_node_ctx))

		return
		yield

		#for input_adapter, input_vop_node_ctx in vop_node_ctx.inputNodes():
		#	for i in input_adapter.generateCode(input_vop_node_ctx): yield i
		
	@classmethod
	def getAdapterContext(cls, vop_node_ctx):
		if not isinstance(vop_node_ctx, VopNodeContext):
			raise ValueError('vop_node_ctx should be of type VopNodeContext, got %s !!!' % type(vop_node_ctx))

		return {}

	@classmethod
	def generateShaderVariables(cls, vop_node_ctx):
		if not isinstance(vop_node_ctx, VopNodeContext):
			raise ValueError('vop_node_ctx should be of type VopNodeContext, got %s !!!' % type(vop_node_ctx))

		return
		yield

	@classmethod
	def generateVariables(cls, vop_node_ctx):
		return
		yield

	@classmethod
	def generateIncludes(cls, vop_node_ctx):
		return
		yield

	@classmethod
	def generateImports(cls, vop_node_ctx):
		return
		yield

	@classmethod
	def generateSubnetVariables(cls, vop_node_ctx):
		return
		yield

	@classmethod
	def outputVariableName(cls, vop_node_ctx, output_name):
		assert isinstance(vop_node_ctx, VopNodeContext)
		return output_name

	@classmethod
	def allowedInShadingContext(cls, vop_node_ctx):
		return True


class VopNodeAdapterBase(VopNodeAdapterAPI):
	__metaclass__ = VopNodeAdapterRegistry
	__base__ = True

	def __init__(self, context):
		super(VopNodeAdapterBase, self).__init__()
		self._context = context

	@classmethod
	def generateCode(cls, vop_node_ctx):
		for i in super(VopNodeAdapterBase, cls).generateCode(vop_node_ctx): yield i

		yield code.EmptyLine()
		yield code.LineComment("Code produced by: {}".format(vop_node_ctx.vop_node_path))

	@classmethod
	def generateShaderVariables(cls, vop_node_ctx):
		for i in super(VopNodeAdapterBase, cls).generateShaderVariables(vop_node_ctx): yield i

	@classmethod
	def generateVariables(cls, vop_node_ctx):
		if not vop_node_ctx.outputs:
			return
			yield

		else:	
			for output_name in vop_node_ctx.outputs:
				output = vop_node_ctx.outputs[output_name]
				yield code.Value(output.var_type, output.var_name)


class VopNodeNetworkAdapterBase(VopNodeAdapterAPI):
	__metaclass__ = VopNodeAdapterRegistry
	__base__ = True

	def __init__(self, context):
		super(VopNodeNetworkAdapterBase, self).__init__()
		self._context = context

	@classmethod
	def generateCode(cls, vop_node_ctx):
		super(VopNodeNetworkAdapterBase, cls).generateCode(vop_node_ctx)
		if not vop_node_ctx.vop_node_wrapper.isSubNetwork():
			raise ValueError('Vop node wrapper "%s" should be a subnetwork !!!' % vop_node_ctx.vop_node_wrapper.path())

	@classmethod
	def generateChildren(cls, vop_node_ctx, mode):
		yield []

