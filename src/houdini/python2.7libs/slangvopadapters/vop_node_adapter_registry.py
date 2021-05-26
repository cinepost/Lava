import hou


_vop_adapter_registry_by_cls_name = {}
_vop_adapter_registry_by_type_name = {}

class VopNodeAdapterRegistry(type):

#	_vop_adapter_registry_by_cls_name = {}
#	_vop_adapter_registry_by_type_name = {}

	def __new__(cls, clsname, bases, attrs):
		newclass = type.__new__(cls, clsname, bases, attrs)

		if '__base__' in attrs:
			if attrs.pop('__base__', True):
				return newclass

		if newclass.__name__ in _vop_adapter_registry_by_cls_name:
			print "VOP adapter class %s already registered !!!" % newclass.__name__
			return newclass

		if newclass.vopTypeName() in _vop_adapter_registry_by_type_name:
			print "VOP adapter class with type name %s already registered !!!" % newclass.vopTypeName()
			return newclass

		_vop_adapter_registry_by_cls_name[newclass.__name__] = newclass
		_vop_adapter_registry_by_type_name[newclass.vopTypeName()] = newclass
		
		return newclass

	@classmethod
	def getAdapterClassByClassName(cls, class_name):
		if class_name in _vop_adapter_registry_by_cls_name:
			return _vop_adapter_registry_by_cls_name[class_name]

		return None

	@classmethod
	def getAdapterClassByTypeName(cls, type_name):
		if type_name in _vop_adapter_registry_by_type_name:
			return _vop_adapter_registry_by_type_name[type_name]

		return None

	@classmethod
	def registeredAdapterClasses(cls):
		return _vop_adapter_registry_by_cls_name

	@classmethod
	def registeredAdapterTypes(cls):
		return _vop_adapter_registry_by_type_name

	@classmethod
	def hasRegisteredAdapterClass(cls, class_name):
		if class_name in _vop_adapter_registry_by_cls_name:
			return True

		return False

	@classmethod
	def hasRegisteredAdapterType(cls, type_name):
		if type_name in _vop_adapter_registry_by_type_name:
			return True

		return False