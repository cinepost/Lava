from lava_houdini import on_light_created

# Lava object parameters
node = kwargs['node']
node_type = kwargs['type']

on_light_created(node, node_type)