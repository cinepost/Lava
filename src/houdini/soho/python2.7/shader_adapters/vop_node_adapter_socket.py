from enum import Enum
import hou


def toCamelCase(snake_str):
	if not snake_str:
		return snake_str

	snake_str = snake_str[0].lower() + snake_str[1:]

	components = snake_str.split('_')
	# We capitalize the first letter of each component except the first one
	# with the 'title' method and join them together.
	return components[0] + ''.join(x.title() for x in components[1:])


def slangDataTypeString(vex_type):
	if vex_type == VopNodeSocket.DataType.INT:    return 'int'
	if vex_type == VopNodeSocket.DataType.FLOAT:  return 'float'
	if vex_type == VopNodeSocket.DataType.VECTOR2:return 'float2'
	if vex_type == VopNodeSocket.DataType.VECTOR: return 'float3'
	if vex_type == VopNodeSocket.DataType.VECTOR4:return 'float4'
	if vex_type == VopNodeSocket.DataType.BSDF:   return 'bsdf'
	if vex_type == VopNodeSocket.DataType.SHADER: return 'shader'
	if vex_type == VopNodeSocket.DataType.SURFACE:return 'surface'
	if vex_type == VopNodeSocket.DataType.DISPLACE:return 'displacement'
	
	if self._data_type == VopNodeSocket.DataType.STRING:
		raise ValueError('Con not convert VopNodeSocket.DataType.STRING to Slang type !!!')

	return 'undef'


class VopNodeSocket(object):
	class Direction(Enum):
		INPUT 			= 1
		OUTPUT 			= 2
		INOUT 			= 3
			
	class DataType(Enum):
		UNDEF   = 0
		INT 	= 1
		FLOAT 	= 2
		STRING 	= 3
		COLOR   = 4
		VECTOR 	= 5
		VECTOR2 = 6
		VECTOR4 = 7
		MATRIX  = 8
		MATRIX3 = 9
		BSDF    = 10
		SHADER  = 11
		SURFACE = 12
		DISPLACE= 13

		@classmethod
		def isSlandDataType(cls, data_type):
			if data_type == cls.STRING: return False	# we have no string is slang shading language
			if data_type == cls.SURFACE: return False	# this is a slang shader context
			if data_type == cls.DISPLACE: return False	# this is a slang shader context

			return True

	_direction = Direction.INPUT

	def __init__(self, socket_name, socket_data_type_string, direction):
		self._var_name = socket_name
		self._data_type = VopNodeSocket.dataTypeFromString(socket_data_type_string)
		self._direction = direction
		self._default_value = None

	#@property
	#def vopName(self):
	#	return self._vop_name

	def setDefaultValue(self, default_value):
		self._default_value = default_value

	@property
	def var_name(self):
		return self._var_name
		#return "_%s" % toCamelCase(self.vopName)
	
	@property
	def var_type(self):
		return slangDataTypeString(self._data_type)
	
	@property
	def vex_type(self):
		return self._data_type

	@property
	def direction(self):
		return self._direction

	@property
	def default_value(self):
		return self._default_value

	@property
	def slangTypeAccessString(self):	
		if self._direction == VopNodeSocket.Direction.INPUT: return None
		if self._direction == VopNodeSocket.Direction.OUTPUT: return "out"
		if self._direction == VopNodeSocket.Direction.INOUT: return "inout"

	@classmethod
	def dataTypeFromString(cls, socket_data_type_string):
		if socket_data_type_string == 'undef': return VopNodeSocket.DataType.UNDEF
		if socket_data_type_string == 'int': return VopNodeSocket.DataType.INT
		if socket_data_type_string == 'float': return VopNodeSocket.DataType.FLOAT
		if socket_data_type_string == 'string': return VopNodeSocket.DataType.STRING
		if socket_data_type_string == 'vector': return VopNodeSocket.DataType.VECTOR
		if socket_data_type_string == 'vector2': return VopNodeSocket.DataType.VECTOR2
		if socket_data_type_string == 'color': return VopNodeSocket.DataType.VECTOR
		if socket_data_type_string == 'vector4': return VopNodeSocket.DataType.VECTOR4
		if socket_data_type_string == 'bsdf': return VopNodeSocket.DataType.BSDF
		if socket_data_type_string == 'shader': return VopNodeSocket.DataType.SHADER
		if socket_data_type_string == 'surface': return VopNodeSocket.DataType.SURFACE
		if socket_data_type_string == 'displacement': return VopNodeSocket.DataType.DISPLACE

		raise ValueError('Unknown socket data type %s', socket_data_type_string)