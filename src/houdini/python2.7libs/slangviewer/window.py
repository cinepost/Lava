import hou
from PySide2 import QtCore, QtGui, QtWidgets, QtUiTools
from collections import OrderedDict
from slangvopadapters import VopNodeAdapterProcessor, code

allowed_shading_contexts = [
	"surface",
	"displacement",
]

class SlangViewerWindow(QtWidgets.QDialog):
	current_shading_context = "surface"

	def __init__(self,  vop_node):
		super(SlangViewerWindow, self).__init__(hou.ui.mainQtWindow())
		
		self.shading_context = self.current_shading_context
		self.vop_node = None
		if isinstance(vop_node, hou.VopNode):
			self.vop_node = vop_node

		mainLayout = QtWidgets.QVBoxLayout(self)
		
		mainLayout.addLayout(self.initNavigation())
		mainLayout.addLayout(self.initShadingContextChooser())

		mainLayout.addWidget(self.initCodeTabs())
		mainLayout.addWidget(self.initButtonBox())

		self.setLayout(mainLayout)
		self.setWindowTitle('View Slang Code')

		self.refresh()

	def initButtonBox(self):
		closeButton = QtWidgets.QPushButton("Close")
		closeButton.clicked.connect(self.close)

		refreshButton = QtWidgets.QPushButton("Refresh")
		refreshButton.clicked.connect(self.refresh)

		button_box = QtWidgets.QDialogButtonBox(QtCore.Qt.Horizontal, parent=self)
		button_box.addButton(refreshButton, QtWidgets.QDialogButtonBox.ActionRole)
		button_box.addButton(closeButton, QtWidgets.QDialogButtonBox.AcceptRole)

		return button_box

	def initCodeTabs(self):
		self.codeTabsWidget = QtWidgets.QTabWidget(self)
		return self.codeTabsWidget

	def initNavigation(self):
		layout = QtWidgets.QHBoxLayout()
		label = QtWidgets.QLabel(self)
		label.setText("Viewing:")

		self.node_path = QtWidgets.QLineEdit(self)
		self.node_path.setText(self.vop_node.path())
		self.node_path.returnPressed.connect(self.changeNode)
		
		layout.addWidget(label)
		layout.addWidget(self.node_path)
		return layout

	def initShadingContextChooser(self):
		layout = QtWidgets.QHBoxLayout()
		label = QtWidgets.QLabel(self)
		label.setText("Context type")

		self.shading_context_combo = QtWidgets.QComboBox(self)
		for shading_context_name in allowed_shading_contexts:
			self.shading_context_combo.addItem(shading_context_name)

		self.shading_context_combo.setCurrentIndex(allowed_shading_contexts.index(self.shading_context))

		layout.addWidget(label)
		layout.addWidget(self.shading_context_combo)

		return layout

	def clearCodeTabs(self):
		for i in range(self.codeTabsWidget.count()):
			self.codeTabsWidget.widget(i).deleteLater()

	def buildCode(self):
		processor = VopNodeAdapterProcessor(self.vop_node)
		shaders = processor.generateShaders()

		self.clearCodeTabs()

		if self.shading_context in shaders:
			for generable in shaders[self.shading_context]:
				if isinstance(generable, code.Source):
					tab_name = generable.filename().rsplit('/')[-1]
					code_browser = QtWidgets.QTextBrowser(self)
					code_browser.append(str(generable))
					self.codeTabsWidget.addTab(code_browser, tab_name)

	def refresh(self):
		self.node_path.setText(self.vop_node.path())
		self.buildCode()

	def changeNode(self, node_path=None):
		new_node = None

		if node_path == None:
			node_path = self.node_path.text()
		
		try:
			new_node = hou.node(node_path)
		except:
			#raise ValueError("Wrong node path %s provided !!!" % node_path)
			pass

		if new_node:
			self.vop_node = new_node
			self.refresh()
		else:
			self.node_path.setText(self.vop_node.path())

	def keyPressEvent(self, event):
		if event.key() in (QtCore.Qt.Key_Return, QtCore.Qt.Key_Escape, QtCore.Qt.Key_Enter,):
			return

		super(SlangViewerWindow, self).keyPressEvent(self, event)

	#def eventFilter(self, obj, event):
	#	print "event"
	#	if obj is self:
	#		if event.type() == QtCore.QEvent.KeyPress:
	#			if event.key() in (QtCore.Qt.Key_Return, QtCore.Qt.Key_Escape, QtCore.Qt.Key_Enter,):
	#				event.ignore()
	#				return True
	#		if event.type() == QtCore.QEvent.Close:
	#			event.ignore()
	#			return True
	#	return super(SlangViewerWindow, self).eventFilter(obj, event)