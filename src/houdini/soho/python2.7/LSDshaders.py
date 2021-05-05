#from LSDapi import *

import os
import types
import importlib

def reload_package(package):
	assert(hasattr(package, "__package__"))
	fn = package.__file__
	fn_dir = os.path.dirname(fn) + os.sep
	module_visit = {fn}
	del fn

	def reload_recursive_ex(module):
		reload(module)

		for module_child in vars(module).values():
			if isinstance(module_child, types.ModuleType):
				fn_child = getattr(module_child, "__file__", None)
				if (fn_child is not None) and fn_child.startswith(fn_dir):
					if fn_child not in module_visit:
						# print("reloading:", fn_child, "from", module)
						module_visit.add(fn_child)
						reload_recursive_ex(module_child)

	return reload_recursive_ex(package)


import hou
import shader_adapters


def processMaterialNode(vop_node):
	#print shader_adapters.VopNodeAdapterRegistry.registeredAdapterClasses()
	print shader_adapters.VopNodeAdapterRegistry.registeredAdapterTypes()

	if vop_node.shaderType() != hou.shaderType.VopMaterial:
		# For now only VopMaterial shader types
		raise ValueError('Non VopMaterial material node "%s" specified !!!' % vop_node.path())


	processor = shader_adapters.VopNodeAdapterProcessor(vop_node)

	shaders = processor.generateShaders()

	print shaders['surface']
	

def _processMaterialNode(mat_node):
	if mat_node.shaderType() != hou.shaderType.VopMaterial:
		# For now only VopMaterial shader types
		raise ValueError('Non VopMaterial material node "%s" specified !!!' % mat_node.path())

	if not mat_node.isSubNetwork():
		# Just subnets for now
		raise ValueError('VopMaterial material node "%s" is not a subnetwork !!!' % mat_node.path())

	# now we should collect shaders from the output child. so let's find it
	vop_material_net_out = None
	for child in mat_node.children():
		if child.shaderType() == hou.shaderType.VopMaterial:
			vop_material_net_out = child

	if not vop_material_net_out:
		raise ValueError('VopMaterial material node "%s" no shader output (collect) !!!' % mat_node.path())

	# now check the inputs
	for input_node in vop_material_net_out.inputs():
		processShaderNode(input_node)


def processShaderNode(shader_node):
	shader_type = shader_node.shaderType()
	is_subnet = shader_node.isSubNetwork()
	shader_node_type_name = shader_node.type().name()

	if shader_type == hou.shaderType.VopMaterial:
		return

	if shader_type == hou.shaderType.BSDF:
		return

	if shader_type == hou.shaderType.Surface:
		# Surface shader node
		pass

	if shader_type == hou.shaderType.Invalid:
		# Non shading node
		pass


