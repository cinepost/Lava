import hou

from enum import Enum

from functions import getVopNodeAdapter, incrLastStringNum, vexDataTypeToSlang
from vop_node_adapter_base import VopNodeAdapterBase, VopNodeAdapterAPI
from vop_node_adapter_socket import VopNodeSocket
from vop_node_context import VopNodeContext
from vop_node_graph import NodeWrapper, NodeSubnetWrapper, generateGraph
#from code import SlangCodeContext

def appendSocketsAsFuncArgs(args_str, sockets):
	if not isinstance(sockets, (list, tuple)):
		raise ValueError('Wrong sockets argument of type "%s" passed, should be either list or tuple !!!' % type(sockets))

	# Filter out non slang types (line strings and shading contexts e.g 'surface', 'dispalce')
	sockets = [socket for socket in sockets if VopNodeSocket.DataType.isSlandDataType(socket.dataType)]

	if len(sockets) == 0:
		return args_str

	result = args_str

	if result:
		result += ", "

	for i in range(0, len(sockets)):
		socket = sockets[i]

		if socket.direction != VopNodeSocket.Direction.INPUT:
			result += "%s " % socket.slangTypeAccessString
		
		result += "%s %s" % (socket.slangDataTypeString, socket.codeVarName)

		if i != (len(sockets) - 1): result += ", "

	return result


class VopNodeAdapterProcessor(object):
	class ProcessorType(Enum):
		NODE    = 0
		SUBNET  = 1

	_root_node = None

	_code_cache = {}
	
	def __init__(self, vop_node, slang_context=None):
		super(VopNodeAdapterProcessor, self).__init__()

		if not issubclass(type(vop_node), hou.VopNode):
			raise ValueError('Wrong object of type "%s" passed as a vop node !!!' % type(vop_node))

		self._multi_slang_code_contexts = False
		try:
			if vop_node.supportsMultiCookCodeContexts():
				self._multi_slang_code_contexts = True
		except:
			pass

		self._root_node = vop_node
		self._slang_context = slang_context

		self._node_code_cache = {}
		self._args_names_cache = {}
		self._last_arg_names = {}

		self._slang_code_context = None #SlangCodeContext()

	def generateShaders(self):
		node = self._root_node
		shader_type = node.shaderType()
		if   shader_type == hou.shaderType.VopMaterial:
			print "generateShaders VopMaterial"
			return {
				'surface': self._process(node, 'surface'),
				#'displacement': self._process(node, 'displacement')
			}
		elif shader_type == hou.shaderType.Surface:
			print "generateShaders Surface"
			return {
				'surface': self._process(node, 'surface'),
			}
		else:
			pass

		return {}


	def _process(self, vop_node, slang_context = 'surface'):
		from functions import getVopNodeAdapter
		from utils import printHighlightedSlangCode

		if not issubclass(type(vop_node), hou.VopNode):
			raise ValueError('Wrong object of type "%s" passed as a vop node !!!' % type(vop_node))

		if vop_node.isBypassed():
			return None

		node_adapter = None
		try:
			node_adapter = getVopNodeAdapter(vop_node)
		except:
			pass

		if node_adapter:
			print "generate graph"
			graph = generateGraph(vop_node)
			print "generate", vop_node.path()
			
			#for res in node_adapter.generate(VopNodeContext(node_adapter, NodeSubnetWrapper(vop_node))):
			node_wrapper = graph.nodeWrapper(vop_node.path())
			node_context = VopNodeContext(node_wrapper)
			for res in node_adapter.generateCode(node_context):
				printHighlightedSlangCode(res)
				#pass

			return ""

		return None
    

	def buildVopContext(self, vop_node, custom_node_context):
		from collections import OrderedDict

		if vop_node.type().category().name() != 'Vop':
			raise ValueError("Non VOP node %s provided !!!", vop_node.path())

		node_adapter = None
		try:
			node_adapter = getVopNodeAdapter(vop_node)
		except:
			pass

		context_parms = OrderedDict()
		for parm in vop_node.parms():
			context_parms[parm.name()] = {
				'VALUE': parm.eval(),
			}

		context_inputs = OrderedDict()
		for connection in vop_node.inputConnections():
			input_name = connection.outputName()
			input_arg_name = connection.inputName()
			input_arg_node = connection.inputNode()
			input_arg_type = connection.inputDataType()

			context_inputs[input_name] = {
				'VAR_NAME': self.getSafeArgName(input_arg_node, input_arg_name),
				'VAR_TYPE': vexDataTypeToSlang(input_arg_type),
			}
		
		context_outputs = OrderedDict()
		for connection in vop_node.outputConnections():
			output_name = connection.inputName()
			output_node = connection.inputNode()
			output_type = connection.inputDataType()

			mangled_output_name = output_name
			if node_adapter:
				mangled_output_name = node_adapter.mangleOutputName(output_name, custom_node_context)

			context_outputs[output_name] = {
				'VAR_NAME': self.getSafeArgName(output_node, mangled_output_name),
				'VAR_TYPE': vexDataTypeToSlang(output_type),
			}

		context = {
			'CREATOR_COMMENT': '// Code produced by: %s' % vop_node.path(),
			'SLANG_CONTEXT': self._slang_code_context,
			'NODE_NAME': vop_node.name(),
			'NODE_PATH': vop_node.path(),
			'PARMS': context_parms,
			'INPUTS': context_inputs,
			'OUTPUTS': context_outputs,
			'PARENT': None,
		}

		#if vop_node.path() != self._root_node.path():
		#	if vop_node.parent():
		#		context['PARENT'] = self.buildVopContext(vop_node.parent())

		if custom_node_context:
			context['NODE_CONTEXT'] = custom_node_context

		return context

	def getSafeArgName(self, vop_node, arg_name):
		import re 

		arg_path = "%s/%s" % (vop_node.path(), arg_name)

		if arg_path in self._args_names_cache:
			return self._args_names_cache[arg_path]
		
		if arg_name in self._last_arg_names:
			incremented_name = incrLastStringNum(self._last_arg_names[arg_name])
			self._last_arg_names[arg_name] = incremented_name
			self._args_names_cache[arg_path] = incremented_name
			return incremented_name
		
		self._last_arg_names[arg_name] = arg_name
		self._args_names_cache[arg_path] = arg_name
		return arg_name