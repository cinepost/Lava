from LSDapi import *

import os
import types
import importlib
import hou



def parmTypeToLava(data_type, num_components):
	if data_type == hou.parmData.Float:
		if num_components == 1:
			return "float"
		if num_components == 2:
			return "vector2"
		if num_components == 3:
			return "vector3"
		if num_components == 4:
			return "vector4"

	if data_type == hou.parmData.Int:
		if num_components == 1:
			return "int"
		if num_components == 2:
			return "int2"
		if num_components == 3:
			return "int3"
		if num_components == 4:
			return "int4"

	if data_type == hou.parmData.String:
		return "string"

	return "unknown"


class LavaParm(object):
	name = None
	type = None
	value = [None]

def evalParmToLava(parm, persistence_dict=None):
	if not parm:
		return LavaParm()

	retval = []

	parm_node = parm.node()
	parm_template = parm.parmTemplate()
	parm_key = "%s/%s" % (parm_node.path(), parm_template.name())

	if persistence_dict:
		if parm_key in persistence_dict:
			return LavaParm()

	num_components = parm_template.numComponents()
	naming_scheme = parm_template.namingScheme()

	lava_parm_type = parmTypeToLava(parm_template.dataType(), num_components)
	lava_parm_name = parm_template.name()

	if num_components == 1:
		lava_parm = LavaParm()
		lava_parm.name = parm.name()
		lava_parm.type = lava_parm_type
		lava_parm.value = [parm.eval()]
		return lava_parm

	base_name = parm_template.name()

	if naming_scheme == hou.parmNamingScheme.RGBA:
		for i in 'rgba'[:num_components]:
			retval += [parm_node.parm("%s%s" % (base_name,i)).eval()]

	elif naming_scheme == hou.parmNamingScheme.XYZW:
		for i in 'xyzw'[:num_components]:
			retval += [parm_node.parm("%s%s" % (base_name,i)).eval()]

	elif naming_scheme == hou.parmNamingScheme.XYWH:
		for i in 'xywh'[:num_components]:
			retval += [parm_node.parm("%s%s" % (base_name,i)).eval()]

	elif naming_scheme == hou.parmNamingScheme.UVW:
		for i in 'uvw'[:num_components]:
			retval += [parm_node.parm("%s%s" % (base_name,i)).eval()]

	else:
		raise NotImplementedError("Unimplemented naming scheme %s" % naming_scheme.name())

	lava_parm = LavaParm()
	lava_parm.name = lava_parm_name
	lava_parm.type = lava_parm_type
	lava_parm.value = retval	

	persistence_dict[parm_key] = lava_parm # mark this parameter already processed

	return lava_parm	

def outputNetwork(vop_node_path, now):
	vop_node = hou.node(vop_node_path) or None
	
	if not vop_node:
		raise ValueError('No material found at path %s !!!' % vop_node_path)


	vop_type = vop_node.type()
	vop_category_name = vop_type.category().name()

	# Process only materials
	if vop_category_name == "Vop":
		if not vop_node.isMaterialFlagSet():
			return

	outputNode(vop_node, now, recursive=True)

	
def outputNode(vop_node, now, recursive=True):
	if not vop_node:
		return 

	for input_node in vop_node.inputs():
		outputNode(input_node, now, recursive=recursive)

	isSubNetwork = vop_node.isSubNetwork()

	print('\n')
	cmd_comment("Shading node %s" % vop_node.path())
	cmd_start('node')

	cmd_property('object', 'is_subnet', [True if isSubNetwork else False])
	cmd_property('object', 'node_namespace', ["houdini"])
	cmd_property('object', 'node_name', [vop_node.name()])
	cmd_property('object', 'node_type', [vop_node.type().name()])
	cmd_property('object', 'node_uuid', [vop_node.path()])
	
	written_nodes_dict = {}
	persistence_dict = {}

	# Output node parameters (non - default)
	for parm in vop_node.parms():
		if not parm.isAtDefault():
			lava_parm = evalParmToLava(parm, persistence_dict=persistence_dict)
			if lava_parm.name:
				cmd_declare('node', lava_parm.type, lava_parm.name, lava_parm.value)

			
	# Output node inputs and outputs
	for conn in vop_node.inputConnections():
		cmd_socket('input', conn.outputDataType(), conn.outputName())

	for conn in vop_node.outputConnections():
		cmd_socket('output', conn.inputDataType(), conn.inputName())

	
	if isSubNetwork and recursive:
		for output_name in vop_node.outputNames():
			terminal_child_tuple = vop_node.subnetTerminalChild(output_name) or None
			if terminal_child_tuple:
				terminal_child_node = terminal_child_tuple[0]
				if terminal_child_node:
					if terminal_child_node.path() not in written_nodes_dict:
						outputNode(terminal_child_node, now, recursive=recursive)
						written_nodes_dict[terminal_child_node.path()] = True

		#for child_vop_node in vop_node.children():
		#	outputNode(child_vop_node, now, recursive=recursive)

	cmd_end()

	# output edges
	for connection in vop_node.inputConnections():
		src_node_uuid = connection.inputNode().path()
		dst_node_uuid = connection.outputNode().path()
		src_output_socket = connection.inputName()
		dst_input_socket = connection.outputName()
		cmd_edge(src_node_uuid, src_output_socket, dst_node_uuid, dst_input_socket)

"""

	shader_type = node.shaderType()
		if   shader_type == hou.shaderType.VopMaterial:
			print "generateShaders VopMaterial"
			return {
				'surface': self._process(node, 'surface'),
				'displacement': self._process(node, 'displacement')
			}
		elif shader_type == hou.shaderType.Surface:
			print "generateShaders Surface"
			return {
				'surface': self._process(node, 'surface'),
			}
		else:


def processMaterialNode(vop_node):
	print "Registered Slang adapters", slangvopadapters.VopNodeAdapterRegistry.registeredAdapterTypes()

	if vop_node.shaderType() != hou.shaderType.VopMaterial:
		# For now only VopMaterial shader types
		raise ValueError('Non VopMaterial material node "%s" specified !!!' % vop_node.path())


	processor = slangvopadapters.VopNodeAdapterProcessor(vop_node)

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
"""

