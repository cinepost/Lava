class Generable:
	def __str__(self):
		"""Return a single string (possibly containing newlines) representing
		this code construct."""
		return "\n".join(line.rstrip() for line in self.generate())

	def generate(self, with_semicolon=True):
		"""Generate (i.e. yield) the lines making up this code construct."""

		raise NotImplementedError

def generateSafe(generable, with_semicolon=True):
	if isinstance(generable, Generable):
		try:
			return '\n'.join([res for res in generable.generate(with_semicolon=with_semicolon)])
		except:
			try:
				return ' '.join([res for res in generable.generate()])
			except:
				pass

	return generable

class Declarator(Generable):
	def generate(self, with_semicolon=True):
		tp_lines, tp_decl = self.get_decl_pair()
		tp_lines = list(tp_lines)
		
		#yield from tp_lines[:-1]
		
		for line in tp_lines:
			sc = ";"
			if not with_semicolon:
				sc = ""
			if tp_decl is None:
				yield "{}{}".format(line, sc)
			else:
				yield "{} {}{}".format(line, tp_decl, sc)

		#for line in tp_lines[:-1]:
		#	yield line
			
		#	sc = ";"
		#	if not with_semicolon:
		#		sc = ""
		#	if tp_decl is None:
		#		yield "{}{}".format(tp_lines[-1], sc)
		#	else:
		#		yield "{} {}{}".format(tp_lines[-1], tp_decl, sc)

	def get_decl_pair(self):
		"""Return a tuple ``(type_lines, rhs)``.
		*type_lines* is a non-empty list of lines (most often just a
		single one) describing the type of this declarator. *rhs* is the right-
		hand side that actually contains the function/array/constness notation
		making up the bulk of the declarator syntax.
		"""

	def inline(self, with_semicolon=True):
		"""Return the declarator as a single line."""
		tp_lines, tp_decl = self.get_decl_pair()
		tp_lines = " ".join(tp_lines)
		if tp_decl is None:
			return tp_lines
		else:
			return "{} {}".format(tp_lines, tp_decl)
			#return f"{tp_lines} {tp_decl}"


class Value(Declarator):
	"""A simple declarator: *typename* and *name* are given as strings."""

	def __init__(self, typename, name):
		self.typename = typename
		self.name = name

	def get_decl_pair(self):
		return [self.typename], self.name

	def struct_maker_code(self, data):
		raise RuntimeError("named-type values can't be put into structs")

	def struct_format(self):
		raise RuntimeError("named-type values have no struct format")

	def default_value(self):
		return 0

	mapper_method = "map_value"


class NestedDeclarator(Declarator):
	def __init__(self, subdecl):
		self.subdecl = subdecl

	@property
	def name(self):
		return self.subdecl.name

	def struct_format(self):
		return self.subdecl.struct_format()

	def alignment_requirement(self):
		return self.subdecl.alignment_requirement()

	def struct_maker_code(self, data):
		return self.subdecl.struct_maker_code(data)

	def get_decl_pair(self):
		return self.subdecl.get_decl_pair()


class FunctionDeclaration(NestedDeclarator):
	def __init__(self, subdecl, arg_decls):
		NestedDeclarator.__init__(self, subdecl)
		self.arg_decls = arg_decls

	def get_decl_pair(self):
		sub_tp, sub_decl = self.subdecl.get_decl_pair()

		return sub_tp, ("{}({})".format(
			sub_decl,
			", ".join(ad.inline() for ad in self.arg_decls)))

	def struct_maker_code(self, data):
		raise RuntimeError("function pointers can't be put into structs")

	def struct_format(self):
		raise RuntimeError("function pointers have no struct format")

	mapper_method = "map_function_declaration"



class Define(Generable):
	def __init__(self, symbol, value):
		self.symbol = symbol
		self.value = value

	def generate(self):
		#yield f"#define {self.symbol} {self.value}"
		yield "#define {} {}".format(self.symbol, self.value)

	mapper_method = "map_define"


class Include(Generable):
	def __init__(self, filename, system=False):
		self.filename = filename
		self.system = system

	def generate(self):
		if self.system:
			yield "#include <%s>" % self.filename
		else:
			yield '#include "%s"' % self.filename

	mapper_method = "map_include"


class Import(Generable):
	def __init__(self, module):
		self.module = module
		
	def generate(self):
		yield "import %s;" % self.module
		
	mapper_method = "map_import"


class Statement(Generable):
	def __init__(self, text):
		self.text = text

	def generate(self):
		yield self.text+";"

	mapper_method = "map_statement"


class Assign(Generable):
    def __init__(self, lvalue, rvalue):
        self.lvalue = lvalue
        self.rvalue = rvalue

    def generate(self):
    	yield "{} = {};".format(generateSafe(self.lvalue, with_semicolon=False), generateSafe(self.rvalue, with_semicolon=False))

    mapper_method = "map_assignment"


class Add(Generable):
    def __init__(self, lvalue, rvalue):
        self.lvalue = lvalue
        self.rvalue = rvalue

    def generate(self):
        yield "{} + {}".format(generateSafe(self.lvalue, with_semicolon=False), generateSafe(self.rvalue, with_semicolon=False))

    mapper_method = "map_add"


class Multiply(Generable):
    def __init__(self, lvalue, rvalue):
        self.lvalue = lvalue
        self.rvalue = rvalue

    def generate(self):
        yield "{} * {}".format(generateSafe(self.lvalue, with_semicolon=False), generateSafe(self.rvalue, with_semicolon=False))

    mapper_method = "map_multiple"


class EmptyLine(Generable):
	def generate(self):
		yield "\n"

	mapper_method = "map_empty_line"


class Line(Generable):
	def __init__(self, text=""):
		self.text = text

	def generate(self):
		yield self.text

	mapper_method = "map_line"


class Comment(Generable):
	def __init__(self, text, skip_space=False):
		self.text = text
		if skip_space:
			self.fmt_str = "/*%s*/"
		else:
			self.fmt_str = "/* %s */"

	def generate(self):
		yield self.fmt_str % self.text

	mapper_method = "map_comment"


class MultilineComment(Generable):
	def __init__(self, text, skip_space=False):
		self.text = text
		self.skip_space = skip_space

	def generate(self):
		yield "/**"
		if self.skip_space is True:
			line_begin, comment_end = "*", "*/"
		else:
			line_begin, comment_end = " * ", " */"
		for line in self.text.splitlines():
			yield line_begin + line
		yield comment_end

	mapper_method = "map_multiline_comment"


class LineComment(Generable):
	def __init__(self, text):
		assert "\n" not in text
		self.text = text

	def generate(self):
		yield "// %s" % self.text

	mapper_method = "map_line_comment"


def add_comment(comment, stmt):
	if comment is None:
		return stmt

	if isinstance(stmt, Block):
		result = Block([Comment(comment), Line()])
		result.extend(stmt.contents)
		return result
	else:
		return Block([Comment(comment), Line(), stmt])


class FunctionBody(Generable):
	def __init__(self, fdecl, body):
		"""Initialize a function definition. *fdecl* is expected to be
		a :class:`FunctionDeclaration` instance, while *body* is a
		:class:`Block`.
		"""

		self.fdecl = fdecl
		self.body = body

	def generate(self):
		#yield from self.fdecl.generate(with_semicolon=False)
		#yield from self.body.generate()

		for i in self.fdecl.generate(with_semicolon=False):
			yield i

		for i in self.body.generate():
			yield i

	mapper_method = "map_function_body"


class Block(Generable):
	def __init__(self, contents=[]):
		if(isinstance(contents, Block)):
			contents = contents.contents
		self.contents = contents[:]

		for item in contents:
			assert isinstance(item, Generable)

	def generate(self):
		yield "{"
		for item in self.contents:
			for item_line in item.generate():
				yield "  " + item_line
		yield "}"

	def append(self, data):
		self.contents.append(data)

	def extend(self, data):
		self.contents.extend(data)

	def insert(self, i, data):
		self.contents.insert(i, data)

	def extend_log_block(self, descr, data):
		self.contents.append(Comment(descr))
		self.contents.extend(data)
		self.contents.append(Line())

	mapper_method = "map_block"

class Source(Generable):
	def __init__(self, filename, contents=[], header_comment=True):
		self.filename = filename
		self.contents = []

		if header_comment:
			self.contents += [Comment("File: {}".format(self.filename)), EmptyLine()]

		for generable in contents:
			assert not isinstance(generable, Source)
			assert isinstance(generable, Generable)
			self.contents += [generable]

	def append(self, generable):
		assert not isinstance(generable, Source)
		assert isinstance(generable, Generable)
		self.contents.append(generable)

	def insert(self, i, generable):
		assert not isinstance(generable, Source)
		assert isinstance(generable, Generable)
		self.contents.insert(i, generable)

	def generate(self):
		with open(self.filename, 'w') as f:
			for generable in self.contents:
				generated_string = str(generable)
				f.write(generated_string)
				yield generated_string

	def decode(self, mode='utf-8'):
		return str(self).decode(mode)

	mapper_method = "map_source"