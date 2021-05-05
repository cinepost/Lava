import hou

from vop_node_adapter_socket import VopNodeSocket


class VopNodeAdapterContext(object):
	def __init__(self, vop_node):
		node_input_names = vop_node.inputNames()
		node_input_data_types = vop_node.inputDataTypes()
		assert len(node_input_names) == len(node_input_data_types), 'Node "%s" input names and data types count should match !!!' % vop_node.path()

		node_output_names = vop_node.outputNames()
		node_output_data_types = vop_node.outputDataTypes()
		assert len(node_output_names) == len(node_output_data_types), 'Node "%s" output names and data types count should match !!!' % vop_node.path()

		inputs_count = len(node_input_names)
		outputs_count = len(node_output_names)

		inputs = [VopNodeSocket(node_input_names[i], node_input_data_types[i], direction=VopNodeSocket.Direction.INPUT) for i in range(0, inputs_count)]
		outputs = [VopNodeSocket(node_output_names[i], node_output_data_types[i], direction=VopNodeSocket.Direction.OUTPUT) for i in range(0, outputs_count)]

		self._vop_node = vop_node
		self._vop_node_name = vop_node.name()
		self._vop_node_path = vop_node.path()
		self._vop_node_type_name = vop_node.type().name()
		self._vop_node_type_category_name = vop_node.type().category().name()
		self._inputs = inputs
		self._outputs = outputs
	
	@property 
	def inputs(self):
		return self._inputs

	@property 
	def outputs(self):
		return self._outputs

	@property
	def vopNode(self):
		return self._vop_node

	@property
	def vopNodeName(self):
		return self._vop_node_name

	@property
	def vopNodePath(self):
		return self._vop_node_path

	@property
	def codeFuncName(self):
		return "_".join(self.vopNodePath.strip("/").split("/"))
	
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