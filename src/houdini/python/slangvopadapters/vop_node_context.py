from collections import OrderedDict

import hou

from vop_node_adapter_socket import VopNodeSocket
from functions import incrLastStringNum


class VopNodeContext(object):
	_scoped_args_names_cache = {}
	_scoped_last_arg_names = {}

	#_args_names_cache = OrderedDict()
	#_last_arg_names = {}

	def __init__(self, vop_node_wrapper, shading_context, parent_vop_node_context=None):
		from vop_node_graph import NodeWrapperBase

		assert isinstance(vop_node_wrapper, NodeWrapperBase)
		assert shading_context in ['surface', 'displacement']

		self._shading_context = shading_context
		self._parent_vop_node_context = parent_vop_node_context
		self._vop_node_wrapper = None

		self._inputs = OrderedDict()
		self._outputs = OrderedDict()
		self._input_node_wrappers_names = []
		self._parms = {}

		if vop_node_wrapper:
			self._vop_node_name = vop_node_wrapper.name()
			self._vop_node_path = vop_node_wrapper.path()
			self._vop_node_type_name = vop_node_wrapper.type().name()
			self._vop_node_type_category_name = vop_node_wrapper.type().category().name()

			self._adapter = vop_node_wrapper.adapter()
			self._vop_node_wrapper = vop_node_wrapper

			for parm in vop_node_wrapper.parms():
				parm_value = None
				parm_template = parm.parmTemplate()
				parm_data_type = parm_template.dataType()

				if parm_data_type == hou.parmData.Int:
					parm_value = parm.evalAsInt()
					try:
						if parm.menuItems():
							parm_value = parm.menuItems()[parm_value]
					except:
						pass
				elif parm_data_type == hou.parmData.String:
					parm_value = parm.evalAsString()
				elif parm_data_type == hou.parmData.Float:
					parm_value = parm.evalAsFloat()
				else:
					parm_value = parm.eval()

				if parm_value: self._parms[parm.name()] = parm_value

			# all inputs/outpus
			input_names = vop_node_wrapper.inputNames()
			input_data_types = vop_node_wrapper.inputDataTypes()
			for i in range(len(input_data_types)):
				input_type = input_data_types[i]
				if input_type != 'undef':
					input_name = input_names[i]
					self._inputs[input_name] = VopNodeSocket(None, input_type, direction=VopNodeSocket.Direction.INPUT)

			output_names = vop_node_wrapper.outputNames()
			output_data_types = vop_node_wrapper.outputDataTypes()
			for i in range(len(output_data_types)):
				output_type = output_data_types[i]
				if output_type != 'undef':
					output_name = output_names[i]
					self._outputs[output_name] = VopNodeSocket(None, output_type, direction=VopNodeSocket.Direction.INPUT)

			# connected inputs
			for connection in vop_node_wrapper.inputConnections():
				input_name = connection.inputName()
				input_node_wrapper = connection.inputNodeWrapper()
				input_type = connection.inputDataType()

				input_var_name = ""
				if input_node_wrapper.isSubNetwork():
					input_wrapper_ctx = VopNodeContext(input_node_wrapper, shading_context, parent_vop_node_context=self._parent_vop_node_context)
					input_var_name = input_wrapper_ctx.outputs[input_name].var_name
				elif input_node_wrapper.adapter():
					input_wrapper_ctx = VopNodeContext(input_node_wrapper, shading_context, parent_vop_node_context=self._parent_vop_node_context)
					input_var_name = self.getSafeArgName(input_node_wrapper, input_node_wrapper.adapter().outputVariableName(input_wrapper_ctx, input_name))
				else:
					input_var_name = self.getSafeArgName(input_node_wrapper, input_name)

				self._inputs[connection.outputName()] = VopNodeSocket(input_var_name, input_type, direction=VopNodeSocket.Direction.INPUT)
					
				input_node_wrapper_name = connection.inputNodeWrapperName()
				if not input_node_wrapper_name in self._input_node_wrappers_names:
					self._input_node_wrappers_names.insert(0, input_node_wrapper_name)

			# connected outputs
			for connection in vop_node_wrapper.outputConnections():
				output_name = connection.inputName()
				output_var_name = self._adapter.outputVariableName(self, output_name)
				output_type = connection.inputDataType()

				if vop_node_wrapper.isSubNetwork():
					terminal_child_wrapper = self.vop_node_wrapper.getTerminalWrapperByNetworkOutputName(output_name)
					if terminal_child_wrapper:
						self._outputs[output_name] = VopNodeSocket(self.getSafeArgName(terminal_child_wrapper, output_name), output_type, direction=VopNodeSocket.Direction.OUTPUT)
				else:
					self._outputs[output_name] = VopNodeSocket(self.getSafeArgName(vop_node_wrapper, output_var_name), output_type, direction=VopNodeSocket.Direction.OUTPUT)

		# build adapter specific context
		self._adapter_ctx = {}
		if self._adapter:
			self._adapter_ctx = self._adapter.getAdapterContext(self)

	def children(self):
		for child_vop_node_wrapper in self._vop_node_wrapper.childrenSorted(self._shading_context):
			adapter = child_vop_node_wrapper.adapter()
			if adapter:
				yield (adapter, VopNodeContext(child_vop_node_wrapper, self._shading_context, parent_vop_node_context=self))
			else:
				print "missing adapter", child_vop_node_wrapper.path()

	def terminalChildren(self):
		from vop_node_graph import NodeSubnetWrapper

		if not isinstance(self._vop_node_wrapper, NodeSubnetWrapper):
			return
			yield

		for child_vop_node_wrapper in self._vop_node_wrapper.terminalChildren():
			adapter = child_vop_node_wrapper.adapter()
			if adapter:
				yield (adapter, VopNodeContext(child_vop_node_wrapper, self._shading_context, parent_vop_node_context=self))
			else:
				print "missing adapter", child_vop_node_wrapper.path()

	def inputNodes(self):
		if not self._parent_vop_node_context:
			return
			yield
		else:
			for input_node_wrapper_name in self._input_node_wrappers_names:

				#if self._parent_vop_node_context.vop_node_wrapper.hasChildByName(input_node_wrapper_name):
				input_wrapper = self._parent_vop_node_context.vop_node_wrapper.getChildByName(input_node_wrapper_name)
				if input_wrapper:
					print "input_wrapper", input_wrapper
					adapter = input_wrapper.adapter()
					if adapter:
						yield (adapter, VopNodeContext(input_wrapper, self._shading_context, parent_vop_node_context=self._parent_vop_node_context))
					else:
						print "missing adapter", input_wrapper.path()
	
	def __str__(self):
		return "VopNodeContext for %s" % self._vop_node_path

	def __repr__(self):
		return "VopNodeContext for %s" % self._vop_node_path

	@property
	def shading_context(self):
		return self._shading_context

	@property
	def parms(self):
		return self._parms

	@property 
	def inputs(self):
		return self._inputs

	@property 
	def outputs(self):
		return self._outputs

	def adapter(self):
		return self._adapter

	def outputNames(self):
		return self._outputs.keys()

	def inputNames(self):
		return self._inputs.keys()

	@property
	def vop_node_wrapper(self):
		return self._vop_node_wrapper

	@property
	def parent_context(self):
		return self._parent_vop_node_context

	#@property
	#def vop_node(self):
	#	return self._vop_node

	@property
	def vop_node_name(self):
		return self._vop_node_name

	@property
	def vop_node_path(self):
		return self._vop_node_path

	@property
	def adapter_context(self):
		return self._adapter_ctx

	@property
	def codeFuncName(self):
		return "_".join(self.vop_node_path.strip("/").split("/"))
	
	def filterInputs(self, data_type):
		if not isinstance(data_type, (VopNodeSocket.DataType, str)):
			raise ValueError('Inappropriate: {} passed for data_type parameter whereas a either VopNodeSocket.DataType or str is expected'.format(type(data_type)))

		if isinstance(data_type, str):
			data_type = VopNodeSocket.dataTypeFromString(data_type)

		result = []
		for socket in self.inputs:
			if socket.dataType == data_type: result += [socket]

		return result

	def filterOutputs(self, data_type):
		if not isinstance(data_type, (VopNodeSocket.DataType, str)):
			raise ValueError('Inappropriate: {} passed for data_type parameter whereas a either VopNodeSocket.DataType or str is expected'.format(type(data_type)))

		if isinstance(data_type, str):
			data_type = VopNodeSocket.dataTypeFromString(data_type)

		result = []
		for socket in self.outputs:
			if socket.dataType == data_type: result += [socket]

		return result

	def getSafeArgName(self, vop_node_wrapper, arg_name):
		import re 
		from vop_node_graph import NodeWrapperBase

		assert isinstance(vop_node_wrapper, NodeWrapperBase)

		shader_name = vop_node_wrapper.shaderName()

		if not shader_name in self._scoped_args_names_cache:
			self._scoped_args_names_cache[shader_name] = OrderedDict()
			self._scoped_last_arg_names[shader_name] = {}

		arg_path = "%s/%s" % (vop_node_wrapper.path(), arg_name)

		if arg_path in self._scoped_args_names_cache[shader_name]:
			return self._scoped_args_names_cache[shader_name][arg_path]
		
		if arg_name in self._scoped_last_arg_names[shader_name]:
			incremented_name = incrLastStringNum(self._scoped_last_arg_names[shader_name][arg_name])
			self._scoped_last_arg_names[shader_name][arg_name] = incremented_name
			self._scoped_args_names_cache[shader_name][arg_path] = incremented_name
			return incremented_name
		
		self._scoped_last_arg_names[shader_name][arg_name] = arg_name
		self._scoped_args_names_cache[shader_name][arg_path] = arg_name
		return arg_name

	def printArgsCache(self):
		print "args cache"
		for scope_name in self._scoped_args_names_cache:
			for path in self._scoped_args_names_cache[scope_name]:
				print path, self._scoped_args_names_cache[scope_name][path] 