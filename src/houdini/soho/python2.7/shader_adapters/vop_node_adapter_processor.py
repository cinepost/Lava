import hou

from string import Template
from enum import Enum

from vop_node_adapter_base import VopNodeAdapterBase, VopNodeSubnetAdapterBase
from vop_node_adapter_socket import VopNodeSocket

def getSocketsAsFuncParams(sockets):
	result = ""

	for socket in sockets:
		# Check if socket reflects to Sland data type. If not we just skip it. Then it should be handled
		# by the other parts of processing logic
		if not VopNodeSocket.DataType.isSlandDataType(socket.dataType):
			continue

		if socket.direction != VopNodeSocket.Direction.INPUT:
			result += "%s " % socket.slangTypeAccessString
		
		result += "%s %s," % (socket.slangDataTypeString, socket.codeVarName)

	return result

class VopNodeAdapterProcessor(object):
	class ProcessorType(Enum):
		NODE    = 0
		SUBNET  = 1

	_adapter = None
	_vop_node = None
	_processing_mode = None

	def __init__(self, adapter):
		super(VopNodeAdapterProcessor, self).__init__()

		if issubclass(type(adapter), VopNodeAdapterBase):
			self._processing_mode = VopNodeAdapterProcessor.ProcessorType.NODE
		elif issubclass(type(adapter), VopNodeSubnetAdapterBase):
			self._processing_mode = VopNodeAdapterProcessor.ProcessorType.SUBNET
		else:
			raise ValueError('Wrong object of type "%s" passed as a vop node adapter !!!' % type(adapter))

		self._adapter = adapter
		self._vop_node = adapter.context._vop_node

	def process(self):
		inputs = self._adapter.context.inputs
		outputs = self._adapter.context.outputs

		slang_code_template = Template(self._adapter.getSlangTemplate())

		print "processing"
		print self._adapter.getSlangTemplate()

		d = {
			'FUNC_NAME': self._adapter.context.codeFuncName,
			'OP_PATH': self._adapter.context.vopNodePath,
			'PARAMS': "",
			'RETURN_TYPE': "void"
		}

		if len(inputs) > 0:
			d['PARAMS'] = getSocketsAsFuncParams(inputs)

		if len(outputs) > 1:
			# output variables as a function args
			d['PARAMS'] += getSocketsAsFuncParams(outputs)
		else:
			d['RETURN_TYPE'] = outputs[0].slangDataTypeString

		return slang_code_template.substitute(d)

