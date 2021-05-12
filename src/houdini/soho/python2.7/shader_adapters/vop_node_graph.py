import hou
import uuid
from collections import defaultdict
from vop_node_context import VopNodeContext
from functions import getVopNodeAdapter

class ConnectionWrapper(object):
	def __init__(self, connection, parent_net):
		self._parent_net = parent_net
		self._from_node_name = connection.inputNode().name()
		self._to_node = connection.outputNode().name()
		self._inputName = connection.inputName()
		self._inputDataType = connection.inputDataType()
		self._outputName = connection.outputName()
		self._inputNodeName = connection.inputNode().name()

	def inputNodeWrapper(self):
		return self._parent_net._children[self._from_node_name]

	def inputName(self):
		return self._inputName

	def outputName(self):
		return self._outputName

	def inputDataType(self):
		return self._inputDataType

	def inputNodeWrapperName(self):
		return self._inputNodeName


class NodeWrapperBase(object):
	def __init__(self, vop_node, parent):
		if parent:
			if not issubclass(type(parent), NodeWrapperBase):
				raise ValueError('Wrong object of type "%s" passed as a parent !!!' % type(parent))	

		self._name = str(uuid.uuid4())[:8]
		self._parent = parent

		self._isSubNetwork = False

		self._parms = ()
		self._inputNames = []
		self._outputNames = []
		self._inputConnections = []
		self._outputConnections = []

		self._adapter = None
		self._vop_path = None
		self._bypassed = None

		if vop_node:
			self._vop_name = vop_node.name()
			self._vop_path = vop_node.path()
			self._vop_node = vop_node
			self._vop_type = vop_node.type()

			self._inputNames = vop_node.inputNames()
			self._outputNames = vop_node.outputNames()

			if self._parent:
				for conn in vop_node.inputConnections():
					self._inputConnections += [ConnectionWrapper(conn, self._parent)]

				for conn in vop_node.outputConnections():
					self._outputConnections += [ConnectionWrapper(conn, self._parent)]

			self._parms = vop_node.parms()

	def adapter(self):
		return self._adapter

	def isBypassed(self):
		return self._bypassed

	def isMaterialManager(self):
		return False

	def isConnected(self):
		if self._inputConnections or self._outputConnections:
			return True

	def isSubNetwork(self):
		return self._isSubNetwork

	def hasChildByName(self, child_name):
		return None

	def type(self):
		return self._vop_type

	def vop_node(self):
		return self._vop_node

	def name(self):
		if self._vop_name:
			return self._vop_name

		return self._name

	def path(self):
		if self._vop_path:
			return self._vop_path

		if self._parent:
			return "{}/{}".format(self._parent.path(), self.name())

		return self.name()

	def parms(self):
		return self._parms

	def inputNames(self):
		return self._inputNames

	def outputNames(self):
		return self._outputNames

	def inputConnections(self):
		return self._inputConnections

	def outputConnections(self):
		return self._outputConnections

	def children(self):
		return ()

	def parent(self):
		return self._parent


class NodeManagerWrapper(NodeWrapperBase):
	_vop_path_map = {}

	def __init__(self, vop_node, parent):
		super(NodeManagerWrapper, self).__init__(vop_node, parent)

		self._dep_graphs = {}
		self._children_chains = {}
		self._children = {}

		if vop_node:
			for child_vop_node in vop_node.children():
				#if len(child_vop_node.inputConnections()) > 0 or len(child_vop_node.outputConnections()) > 0:
				child = None
				if child_vop_node.isSubNetwork():
					child = NodeSubnetWrapper(child_vop_node, self)
				else:
					child = NodeWrapper(child_vop_node, self)

				self._children[child_vop_node.name()] = child
				NodeSubnetWrapper._vop_path_map[child_vop_node.path()] = child

	def children(self):
		return self._children.values()

	def hasChildByName(self, child_name):
		if child_name in self._children:
			return True

		return None

	def getChildByName(self, child_name):
		if child_name in self._children:
			return self._children[child_name]

		return None

	def nodeWrapper(self, vop_path):
		if vop_path in NodeSubnetWrapper._vop_path_map:
			return NodeSubnetWrapper._vop_path_map[vop_path]

		return None


class NodeWrapper(NodeWrapperBase):
	def __init__(self, vop_node, parent):
		super(NodeWrapper, self).__init__(vop_node, parent)

		self._inputNames = []
		self._outputNames = []
		self._inputDataTypes = []
		self._outputDataTypes = []

		if vop_node:
			self._bypassed = vop_node.isBypassed()
			self._adapter = getVopNodeAdapter(vop_node)
			self._inputNames = vop_node.inputNames()
			self._outputNames = vop_node.outputNames()
			self._inputDataTypes = vop_node.inputDataTypes()
			self._outputDataTypes = vop_node.outputDataTypes()
			self._shader_name = vop_node.shaderName()

	def __str__(self):
		return "NodeWrapper %s" % self.path()

	def __repr__(self):
		return "NodeWrapper %s" % self.path()
	
	def inputNames(self):
		return self._inputNames

	def outputNames(self):
		return self._outputNames

	def inputDataTypes(self):
		return self._inputDataTypes

	def outputDataTypes(self):
		return self._outputDataTypes

	def shaderName(self):
		return self._shader_name or self.parent().shaderName()

	def allowedInShadingContext(self, shading_context):
		if not self._adapter:
			return False

		return self._adapter.allowedInShadingContext(VopNodeContext(self, shading_context))

	def inputNodeWrappers(self):
		if not self._parent:
			return
			yield
		else:
			result_node_wrapper_names = []
			for connection in self.inputConnections():
				input_wrapper_name = connection.inputNodeWrapperName()
				if not input_wrapper_name in result_node_wrapper_names:
					input_wrapper = self.parent().getChildByName(input_wrapper_name)
					if input_wrapper:
						yield input_wrapper


class NodeSubnetWrapper(NodeManagerWrapper, NodeWrapper):
	def __init__(self, vop_node, parent):
		super(NodeSubnetWrapper, self).__init__(vop_node, parent)
		
		if vop_node:
			assert vop_node.isSubNetwork()

		self._isSubNetwork = True

		for shading_context in ["surface", "displacement"]:
			self._prepareDependencyGraph(shading_context)

	def __str__(self):
		return "NodeSubnetWrapper %s" % self.path()

	def __repr__(self):
		return "NodeSubnetWrapper %s" % self.path()

	def isSubNetwork(self):
		return True

	def terminalChildren(self):
		terminal_children_names = []

		if self._vop_node.outputNames():
			for output_name in reversed(self._vop_node.outputNames()):
				node, socket_name = self._vop_node.subnetTerminalChild(output_name)
				if not node.name() in terminal_children_names:
					terminal_children_names += [node.name()]
					yield self._children[node.name()] 
		else:
			return
			yield


	def getTerminalWrapperByNetworkOutputName(self, output_name):
		for termial_node_wrapper in self._children.values():
			if output_name in termial_node_wrapper.outputNames():
				return termial_node_wrapper

		return None

	def childrenSorted(self, shading_context):
		return self.topologicalSort(shading_context)

	# function to add an edge to graph
	def _addDependencyEdge(self, from_node_wrapper, to_node_wrapper, shading_context):
		self._dep_graphs[shading_context][from_node_wrapper.name()].append(to_node_wrapper.name())

	def _nodeWrappersChain(self, node_wrapper, shading_context):
		if node_wrapper.allowedInShadingContext(shading_context):
			for input_wrapper in node_wrapper.inputNodeWrappers():
				for i in self._nodeWrappersChain(input_wrapper, shading_context): yield i

			yield node_wrapper

		else:
			#print "not allowed in shading context", node_wrapper
			return
			yield

	def _prepareDependencyGraph(self, shading_context):
		self._dep_graphs[shading_context] = defaultdict(list) #dictionary containing adjacency list

		self._children_chains[shading_context] = {}

		for termial_node_wrapper in self.terminalChildren():
			
			# check termial node has any input connections, otherwise just ignore it
			if termial_node_wrapper.inputConnections():
			
				for node_wrapper in self._nodeWrappersChain(termial_node_wrapper, shading_context):
					self._children_chains[shading_context][node_wrapper.name()] = node_wrapper

		for to_node_wrapper in self._children_chains[shading_context].values():
			for connection in to_node_wrapper.inputConnections():
				from_node_wrapper = connection.inputNodeWrapper()
				self._addDependencyEdge(from_node_wrapper, to_node_wrapper, shading_context)


	# A recursive function used by topologicalSort
	def topologicalSortUtil(self, node_wrapper, visited, stack, shading_context):
  
		# Mark the current node as visited.
		visited[node_wrapper.name()] = True
  
		# Recur for all the vertices adjacent to this vertex
		for node_name in self._dep_graphs[shading_context][node_wrapper.name()]:
			if visited[node_name] == False:
				self.topologicalSortUtil(self._children[node_name], visited, stack, shading_context)

		# Push current vertex to stack which stores result
		stack.insert(0, node_wrapper)

	# The function to do Topological Sort. It uses recursive topologicalSortUtil()
	def topologicalSort(self, shading_context):
		# Mark all the nodes as not visited
		visited = {}
		#for node_name in self._children.keys():
		for node_name in self._children_chains[shading_context].keys():
			visited[node_name] = False

		# nodes stack
		stack = []

		# Call the recursive helper function to store Topological
		# Sort starting from all vertices one by one		
		for node_wrapper in self._children_chains[shading_context].values():
			if node_wrapper.isConnected() and not node_wrapper.isBypassed():
				if visited[node_wrapper.name()] == False:
					self.topologicalSortUtil(node_wrapper, visited, stack, shading_context)

		#stack.reverse()
		print "stack", self.name(), [wrapper.path() for wrapper in stack]
		return tuple(stack)


class NodeMaterialManagerWrapper(NodeManagerWrapper):
	def __init__(self, vop_node, parent):
		assert vop_node.isMaterialManager()
		super(NodeMaterialManagerWrapper, self).__init__(vop_node, parent)

		#if vop_node:
		#	self._name = vop_node.path()

	def isMaterialManager(self):
		return True


def findMaterialManager(vop_node):
	if vop_node.isMaterialManager():
		return vop_node

	if vop_node.parent():
		return findMaterialManager(vop_node.parent())

	return None


def generateGraph(vop_node):
	mat_mgr_node = findMaterialManager(vop_node)
	if mat_mgr_node:
		return NodeMaterialManagerWrapper(mat_mgr_node, None)

	return None


# NodeGraph class is used to build nodes processing queue
class NodeGraph(object):
	def __init__(self, vop_subnet_node):
		from collections import defaultdict

		if not issubclass(type(vop_subnet_node), hou.VopNode):
			raise ValueError('Wrong object of type "%s" passed as a vop node !!!' % type(vop_subnet_node))

		if not vop_subnet_node.isSubNetwork():
			raise ValueError('Vop node "%s" should be a subnetwork !!!' % vop_subnet_node.path())

		self._subnet_node = vop_subnet_node

		self._graph = defaultdict(list) #dictionary containing adjacency list
		self._nodes = []

		for vop_node in vop_subnet_node.children():
			if node.inputConnections() or node.outputConnections():
				self._nodes += [node]

		for to_node in self._nodes:
			for connection in to_node.inputConnections():
				from_node = connection.inputNode()

				self._addEdge(from_node, to_node)

	# function to add an edge to graph
	def _addEdge(self, from_node, to_node):
		self._graph[from_node.path()].append(to_node.path())

	# A recursive function used by topologicalSort
	def topologicalSortUtil(self, node_path, visited, stack):
  
		# Mark the current node as visited.
		visited[node_path] = True
  
		# Recur for all the vertices adjacent to this vertex
		for path in self._graph[node_path]:
			if visited[path] == False:
				self.topologicalSortUtil(path, visited, stack)

		# Push current vertex to stack which stores result
		stack.insert(0, hou.node(node_path))

	# The function to do Topological Sort. It uses recursive 
	# topologicalSortUtil()
	def topologicalSort(self):
		# Mark all the nodes as not visited
		visited = {}
		for node in self._nodes:
			visited[node.path()] = False

		# nodes stack
		stack = []

		# Call the recursive helper function to store Topological
		# Sort starting from all vertices one by one
		for node in self._nodes:
			if visited[node.path()] == False:
				self.topologicalSortUtil(node.path(), visited, stack)

		#stack.reverse()

		return tuple(stack)