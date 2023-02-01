from ..vop_node_adapter_base import VopNodeAdapterBase
from ..exceptions import *

from .. import code


class VopNodeTypeconvert(VopNodeAdapterBase):
	@classmethod
	def vopTypeName(cls):
		return "typeconvert"

	@classmethod
	def getAdapterContext(cls, vop_node_ctx):
		if not vop_node_ctx:
			return {}
			
		ctx = super(VopNodeTypeconvert, cls).getAdapterContext(vop_node_ctx)

		ctx["intype"] = vop_node_ctx.vop_node.parm("intype").evalAsString()
		ctx["outtype"]  = vop_node_ctx.vop_node.parm("outtype").evalAsString()
		ctx["method"]  = vop_node_ctx.vop_node.parm("method").evalAsString()
		ctx["const"]  = vop_node_ctx.vop_node.parm("const").evalAsString()
		ctx["needconvert"]  = vop_node_ctx.vop_node.parm("needconvert").evalAsString()

		return ctx

	@classmethod
	def generateCode(cls, vop_node_ctx):
		if not vop_node_ctx:
			yield code.EmptyLine()
			yield code.LineComment("Code produced by: autoconvert")

		else:
			for i in super(VopNodeTypeconvert, cls).generateCode(vop_node_ctx): yield i

		#assert len(vop_node_ctx.inputs) == 0
		#assert len(vop_node_ctx.outputs) == 0

		yield code.Assign("hue", "moe")
			