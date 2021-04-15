import hou

from enum import Enum

from jinja2 import Template
from jinja2.utils import concat

from code_template import Code, CodeTemplate
from functions import getVopNodeAdapter, incrLastStringNum, vexDataTypeToSlang
from vop_node_adapter_base import VopNodeAdapterBase
from vop_node_adapter_socket import VopNodeSocket

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

	def _renderNode(self, vop_node, slang_context = 'surface'):
		if not issubclass(type(vop_node), hou.VopNode):
			raise ValueError('Wrong object of type "%s" passed as a vop node !!!' % type(vop_node))

		code = Code()
		if vop_node.path() in self._node_code_cache:
			return code 

		node_adapter = None
		try:
			node_adapter = getVopNodeAdapter(vop_node)
		except:
			pass

		if node_adapter:
			code_template_string = node_adapter.getCodeTemplateString().strip()
			
			template = Template(code_template_string)

			template_dict = self.buildVopContext(vop_node, node_adapter.getTemplateContext(vop_node))
			template_context = template.new_context(template_dict)

			if 'ARGS' in template.blocks:
				code.args = concat(template.blocks['ARGS'](template_context)).strip()

			if 'BODY' in template.blocks:
				code.body = concat(template.blocks['BODY'](template_context)).strip()

		else:
			code.body = '// Missing adapter for vop type "%s" !!!\n' % vop_node.type().name()

		code.prettify()
		self._node_code_cache[vop_node.path()] = code

		return code

	def _process(self, vop_node, slang_context = 'surface'):
		from functions import getVopNodeAdapter

		if not issubclass(type(vop_node), hou.VopNode):
			raise ValueError('Wrong object of type "%s" passed as a vop node !!!' % type(vop_node))

		code = Code()

		if vop_node.inputConnections():
			for connection in vop_node.inputConnections():
				input_node = connection.inputNode()
				input_node_output_name = connection.inputName()

				code += self._process(input_node, slang_context)
				
			code += self._renderNode(vop_node, slang_context)
			return code

		if vop_node.isSubNetwork():
			terminals = [vop_node.subnetTerminalChild(output_name) for output_name in vop_node.outputNames()]

			for terminal in terminals:
				terminal_node = terminal[0]
				terminal_node_input_name = terminal[1]

				code += self._process(terminal_node, slang_context)

			code += self._renderNode(vop_node, slang_context)
			return code
		
		return self._renderNode(vop_node, slang_context)

	def buildVopContext(self, vop_node, custom_node_context=""):
		from collections import OrderedDict

		if vop_node.type().category().name() != 'Vop':
			raise ValueError("Non VOP node %s provided !!!", vop_node.path())

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
				'ARG_NAME': self.getSafeArgName(input_arg_node, input_arg_name),
				'ARG_TYPE': vexDataTypeToSlang(input_arg_type),
			}
		
		context_outputs = OrderedDict()
		for connection in vop_node.outputConnections():
			output_name = connection.inputName()
			output_node = connection.inputNode()
			output_type = connection.inputDataType()

			context_outputs[output_name] = {
				'ARG_NAME': self.getSafeArgName(output_node, output_name),
				'ARG_TYPE': vexDataTypeToSlang(output_type),
			}

		context = {
			'CREATOR_COMMENT': '// Code produced by: %s' % vop_node.path(),
			'NODE_NAME': vop_node.name(),
			'NODE_PATH': vop_node.path(),
			'PARMS': context_parms,
			'INPUTS': context_inputs,
			'OUTPUTS': context_outputs,
		}

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
			self._args_names_cache[arg_path] = incremented_name
			return incremented_name
		
		self._last_arg_names[arg_name] = arg_name
		self._args_names_cache[arg_path] = arg_name
		return arg_name