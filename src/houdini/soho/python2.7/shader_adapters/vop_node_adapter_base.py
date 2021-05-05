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
	def generateSubnetVariables(cls, vop_node_ctx):
		return
		yield

	@classmethod
	def outputVariableName(cls, vop_node_ctx, output_name):
		assert isinstance(vop_node_ctx, VopNodeContext)
		return output_name

class VopNodeAdapterBase(VopNodeAdapterAPI):
	__metaclass__ = VopNodeAdapterRegistry
	__base__ = True

	def __init__(self, context):
		super(VopNodeAdapterBase, self).__init__()
		self._context = context

	@classmethod
	def generateCode(cls, vop_node_ctx):
		super(VopNodeAdapterBase, cls).generateCode(vop_node_ctx)

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

	#@classmethod
	#def generateShaderVariables(cls, vop_node_ctx):
	#	for i in super(VopNodeNetworkAdapterBase, cls).generateShaderVariables(vop_node_ctx): yield i

	#@classmethod
	#def generateVariables(cls, vop_node_ctx):
	#	for i in super(VopNodeNetworkAdapterBase, cls).generateVariables(vop_node_ctx): yield i

	@classmethod
	def generateChildren(cls, vop_node_ctx, mode):
		yield []

	@classmethod
	def __generateChildren(cls, vop_node_ctx, mode):
		print "generateChildren for", vop_node_ctx.vop_node_wrapper.path()

		if not issubclass(type(vop_node_ctx.vop_node_wrapper), NodeWrapper):
			raise ValueError('Wrong object of type "%s" passed as avop_node_ctx.vop_node_wrapper !!!' % type(vop_node_ctx.vop_node_wrapper))

		if not vop_node_ctx.vop_node_wrapper.isSubNetwork():
			raise ValueError('Vop node wrapper "%s" should be a subnetwork !!!' % vop_node_ctx.vop_node_wrapper.path())

		func_name = None
		if mode == 'code':
			func_name = 'generateCode'	
		elif mode == 'shader_vars':
			super(VopNodeNetworkAdapterBase, cls).generateShaderVariables(vop_node_ctx)
			func_name = 'generateShaderVariables'
		elif mode == 'vars':
			func_name = 'generateVariables'
		else:
			raise ValueError('Wrong mode "%s" !!!' % mode)

		#vop_node = vop_node_ctx.vop_node

		#graph = NodeGraph(vop_node)
		#children = graph.topologicalSort()
		children_node_wrappers = vop_node_ctx.vop_node_wrapper.topologicalSort()

		for child_node in children:
			child_node_adapter = getVopNodeAdapter(child_node)
			
			if child_node_adapter:
				child_node_context = VopNodeContext(child_node_adapter, child_node, parent_vop_node_context=vop_node_ctx)

				func = getattr(child_node_adapter, func_name)
				#for generable in child_node_adapter.generate(child_node_context):
				for generable in func(child_node_context):
					if generable:
						yield generable

