from lava_houdini import on_geo_created

# Lava object parameters
node = kwargs['node']
node_type = kwargs['type']

on_geo_created(node, node_type)