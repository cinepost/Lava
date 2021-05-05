from enum import Enum

class SlangType(Enum):
	UNDEF   = 0
	INT     = 1
	INT2 	= 2
	INT3 	= 3
	INT4 	= 4
	FLOAT   = 5
	FLOAT2 	= 6
	FLOAT3 	= 7
	FLOAT4 	= 8

	def __str__(self):
		if self.name == INT: return "int"
		if self.name == FLOAT: return "float"
		if self.name == FLOAT3: return "float3"

		return "undef"


def vopTypeToSlang(vop_type_str):
	if not isinstance(vop_type_str, str):
			raise ValueError('Vop type should be "str", got "%s"' % type(vop_type_str))

	if vop_type_str == "vector": 	return SlangType.FLOAT3
	if vop_type_str == "float":		return SlangType.FLOAT
	if vop_type_str == "int":		return SlangType.INT

	return SlangType.UNDEF


class SlangVariable(object):
	def __init__(self, var_type, var_name, default_value = None):
		if not isinstance(var_name, str):
			raise ValueError('Variable name should be of type "str", got "%s"' % type(var_name))

		if isinstance(var_type, SlangType):
			self._var_type = var_type
		if isinstance(var_type, str):
			self._var_type = vopTypeToSlang(var_type)

		self._var_name = var_name
		self._default_value = default_value

	@property
	def SLANG_TYPE(self):
		return self._var_type

	@property
	def NAME(self):
		return self._var_name

	@property
	def DEFAULT_VALUE(self):
		return self._default_value


class SlangCodeContext(object):
	def __init__(self, parent_context = None):
		if parent_context:
			if not isinstance(parent_context, SlangCodeContext):
				raise ValueError('parent_context should be of type "SlangCodeContext", got "%s"' % type(parent_context))

			self._parent_context = parent_context

		self._shader_parameters = []
		self._variables = []

	def declareShaderParameter(self, slang_variable):
		if not isinstance(slang_variable, SlangVariable):
			raise ValueError('Variable name should be of type "SlangVariable", got "%s"' % type(slang_variable))

		self._shader_parameters += [slang_variable]

	def declareVariable(self, slang_variable):
		if not isinstance(slang_variable, SlangVariable):
			raise ValueError('Variable name should be of type "SlangVariable", got "%s"' % type(slang_variable))

		self._variables += [slang_variable]

	@property
	def VARIABLES(self):
		return self._variables

	@property
	def SHADER_PARAMETERS(self):
		return self._shader_parameters