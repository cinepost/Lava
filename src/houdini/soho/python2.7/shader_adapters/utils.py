from pygments import highlight
from pygments.lexers import CLexer
from pygments.formatters import TerminalFormatter, Terminal256Formatter, TerminalTrueColorFormatter
from pygments.style import Style
from pygments.token import Keyword, Name, Comment, String, Error, Number, Operator, Generic


class SlangLexer(CLexer):
	name = 'Slang'
	aliases = ['slang']
	filenames = ['*.slang', '*.slangh']

	EXTRA_PREPROC = set((
		'import',
	))

	EXTRA_KEYWORDS = set((
		'numthreads', 'location',
	))

	EXTRA_TYPES = set((
		'SamplerState', 'Texture2D', 'cbuffer',
		'float2', 'float3', 'float4',
		'int2', 'int3', 'int4'
	))

	def get_tokens_unprocessed(self, text):
		for index, token, value in CLexer.get_tokens_unprocessed(self, text):
			if token is Name and value in self.EXTRA_KEYWORDS:
				yield index, Keyword.Pseudo, value
			elif token is Name and value in self.EXTRA_TYPES:
				yield index, Keyword.Type, value
			elif token is Name and value in self.EXTRA_PREPROC:
				yield index, Comment.Preproc, value
			else:
				yield index, token, value


class SlangStyle(Style):
	default_style = ""
	styles = {
		Comment:                'italic #dd4',
		Comment.Preproc:        'noitalic #a84',
		Keyword:                '#005',
		Keyword.Type:			'#f0f',
		Name:                   '#fff',
		Name.Function:          '#0f0',
		Name.Class:             'bold #0f0',
		String:                 '#8d8',
		String.Escape:          '#8d8'
	}


def printHighlightedSlangCode(code):
	print(highlight(code, SlangLexer(), TerminalTrueColorFormatter(style=SlangStyle)))

"""
 styles = {
        Whitespace:                "#bbbbbb",
        Comment:                   "italic #408080",
        Comment.Preproc:           "noitalic #BC7A00",

        #Keyword:                   "bold #AA22FF",
        Keyword:                   "bold #008000",
        Keyword.Pseudo:            "nobold",
        Keyword.Type:              "nobold #B00040",

        Operator:                  "#666666",
        Operator.Word:             "bold #AA22FF",

        Name.Builtin:              "#008000",
        Name.Function:             "#0000FF",
        Name.Class:                "bold #0000FF",
        Name.Namespace:            "bold #0000FF",
        Name.Exception:            "bold #D2413A",
        Name.Variable:             "#19177C",
        Name.Constant:             "#880000",
        Name.Label:                "#A0A000",
        Name.Entity:               "bold #999999",
        Name.Attribute:            "#7D9029",
        Name.Tag:                  "bold #008000",
        Name.Decorator:            "#AA22FF",

        String:                    "#BA2121",
        String.Doc:                "italic",
        String.Interpol:           "bold #BB6688",
        String.Escape:             "bold #BB6622",
        String.Regex:              "#BB6688",
        #String.Symbol:             "#B8860B",
        String.Symbol:             "#19177C",
        String.Other:              "#008000",
        Number:                    "#666666",

        Generic.Heading:           "bold #000080",
        Generic.Subheading:        "bold #800080",
        Generic.Deleted:           "#A00000",
        Generic.Inserted:          "#00A000",
        Generic.Error:             "#FF0000",
        Generic.Emph:              "italic",
        Generic.Strong:            "bold",
        Generic.Prompt:            "bold #000080",
        Generic.Output:            "#888",
        Generic.Traceback:         "#04D",

        Error:                     "border:#FF0000"
"""