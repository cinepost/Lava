import pyparsing as pp
from pyparsing import Word, alphas

from enum import Enum, Flag
from itertools import count


class LavaEnum(Enum):
	def auto(it=count()):
		return next(it)

class LavaStrFlag(Flag):
	def __new__(cls, verbose):
		value = len(cls.__members__)
		obj = object.__new__(cls)
		obj._value_ = 2 ** value
		obj.verbose = verbose
		return obj

class MantraVisibilityParser(object):
	class LavaVisibility(LavaStrFlag):
		PRIMARY = "PRIMARY"
		SHADOW 	= "SHADOW"
		DIFFUSE = "DIFFUSE"
		REFLECT = "REFLECT"
		REFRACT = "REFRACT"
		RECV_SHADOW = "RECV_SHADOW"
		SELF_SHADOW = "SELF_SHADOW"
		
	class VisibilityOperator(LavaEnum):
		AND = auto()
		OR 	= auto()

		def __str__(self):
			return self.name

		def __repr__(self):
			return self.__str__()


	class MantraVisibility(LavaEnum):
		NONE 	= auto()
		PRIMARY = auto()
		SHADOW 	= auto()
		DIFFUSE = auto()
		REFLECT = auto()
		REFRACT = auto()
		ALL 	= auto()

		def __str__(self):
			return self.name

		def __repr__(self):
			return self.__str__()


	def __init__(self):
		visOpOR = pp.Literal("|").setParseAction(self.enumerateVisibilityOperator)
		visOpAND = pp.Literal("&").setParseAction(self.enumerateVisibilityOperator)
		visSign = pp.Optional(pp.Literal("-")).suppress()

		visAll = pp.Literal("*")
		visPrimary = visSign + pp.Keyword("primary")
		visShadow = visSign + pp.Keyword("shadow")
		visDiffuse = visSign + pp.Keyword("diffuse")
		visReflect = visSign + pp.Keyword("reflect")
		visRefract = visSign + pp.Keyword("refract")

		visOperand = pp.Optional(visAll | visPrimary | visShadow | visDiffuse | visReflect | visRefract).setParseAction(self.enumerateVisibilityClass)

		self.expression = pp.infixNotation(visOperand, 
			[
				(visOpAND, 2, pp.opAssoc.LEFT),
				(visOpOR, 2, pp.opAssoc.RIGHT)
			])


	def parseString(self, mantra_string):
		if not mantra_string:
			return None

		return self.expression.parseString(mantra_string)


	@classmethod
	def calculateLavaVisibility(cls, s):
		if not isinstance(s, pp.ParseResults):
			raise ValueError("Exprected ParseResults. Got %s" % type(s))

		lava_visibility = cls.LavaVisibility()

	@classmethod
	def enumerateVisibilityOperator(cls, s):
		if not isinstance(s, pp.ParseResults):
			raise ValueError("Exprected ParseResults. Got %s" % type(s))

		val = s[0]
		if val == "|": return cls.VisibilityOperator.OR
		if val == "&": return cls.VisibilityOperator.AND

		raise ValueError("Cannot enumerate visibility perator! Got %s" % s)


	@classmethod
	def enumerateVisibilityClass(cls, s):
		if not isinstance(s, pp.ParseResults):
			raise ValueError("Exprected ParseResults. Got %s" % type(s))

		val = s[0]
		if not val:
			return cls.MantraVisibility.NONE
		else:
			if val == "*": return cls.MantraVisibility.ALL
			if val == "primary": return cls.MantraVisibility.PRIMARY
			if val == "shadow": return cls.MantraVisibility.SHADOW
			if val == "diffuse": return cls.MantraVisibility.DIFFUSE
			if val == "reflect": return cls.MantraVisibility.REFLECT
			if val == "refract": return cls.MantraVisibility.REFRACT
		
		raise ValueError("Cannot enumerate visibility! Got %s" % s)

def test():
	test_strings = (
		"*",
		"primary",
		"primary|shadow",
		"-primary",
		"-diffuse",
		"-diffuse&-reflect&-refract",
		"-diffuse&-reflect|-refract",
		""
	)

	parser = MantraVisibilityParser()

	for test_string in test_strings:
		print("Testing string: \"%s\"" % test_string)
		res = parser.parseString(test_string)
		print(res)

if __name__ == "__main__":
	test()
