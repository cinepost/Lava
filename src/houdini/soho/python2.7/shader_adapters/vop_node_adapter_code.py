from string import Template

class VopNodeAdapterCode(str):
	
	def addLine(self, line: str):
		if self != "":
			self += "\n"
			self += line
			return

		self = line

	
