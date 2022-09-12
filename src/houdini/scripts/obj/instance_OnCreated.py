from lava_houdini import on_instance_created

# Lava object parameters
node = kwargs['node']
node_type = kwargs['type']

on_instance_created(node, node_type)