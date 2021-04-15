

# oc.py
#
#   A subset-C parser, (BNF taken from 1996 International Obfuscated C Code Contest)
#
#   Copyright, 2010, Paul McGuire
#
"""
https://www.ioccc.org/1996/august.hint

The following is a description of the OC grammar:

	OC grammar
	==========
	Terminals are in quotes, () is used for bracketing.

	program:	decl*

	decl:		vardecl
			fundecl

	vardecl:	type NAME ;
			type NAME "[" INT "]" ;

	fundecl:	type NAME "(" args ")" "{" body "}"

	args:		/*empty*/
			( arg "," )* arg

	arg:		type NAME

	body:		vardecl* stmt*

	stmt:		ifstmt
			whilestmt
			dowhilestmt
			"return" expr ";"
			expr ";"
			"{" stmt* "}"
			";"

	ifstmt:		"if" "(" expr ")" stmt
			"if" "(" expr ")" stmt "else" stmt

	whilestmt:	"while" "(" expr ")" stmt

	dowhilestmt:	"do" stmt "while" "(" expr ")" ";"

	expr:		expr binop expr
			unop expr
			expr "[" expr "]"
			"(" expr ")"
			expr "(" exprs ")"
			NAME
			INT
			CHAR
			STRING

	exprs:		/*empty*/
			(expr ",")* expr

	binop:		"+" | "-" | "*" | "/" | "%" |
			"=" |
			"<" | "==" | "!="

	unop:		"!" | "-" | "*"

	type:		"int" stars
			"char" stars

	stars:		"*"*
"""

from pyparsing import *

ParserElement.enablePackrat()

LPAR, RPAR, LBRACK, RBRACK, LBRACE, RBRACE, SEMI, COMMA = map(Suppress, "()[]{};,")
INT, FLOAT, FLOAT2, FLOAT3, FLOAT4, WHILE, DO, IF, ELSE, RETURN = map(
	Keyword, "int float float2 float3 float4 while do if else return".split()
)

NAME = Word(alphas + "_", alphanums + "_")
integer = Regex(r"[+-]?\d+")
char = Regex(r"'.'")
string_ = dblQuotedString

#TYPE = Group((INT | CHAR) + ZeroOrMore("*"))
TYPE = INT | FLOAT | FLOAT2 | FLOAT3 | FLOAT4 | Word(alphanums)
expr = Forward()
func_call = Group(NAME + LPAR + Group(Optional(delimitedList(expr))) + RPAR)
operand = func_call | NAME | integer | char | string_
expr <<= infixNotation(
	operand,
	[
		(oneOf("! - *"), 1, opAssoc.RIGHT),
		(oneOf("++ --"), 1, opAssoc.RIGHT),
		(oneOf("++ --"), 1, opAssoc.LEFT),
		(oneOf("* / %"), 2, opAssoc.LEFT),
		(oneOf("+ -"), 2, opAssoc.LEFT),
		(oneOf("< == > <= >= !="), 2, opAssoc.LEFT),
		(Regex(r"(?<!=)=(?!=)"), 2, opAssoc.LEFT),
	],
) + Optional(
	LBRACK + expr + RBRACK | LPAR + Group(Optional(delimitedList(expr))) + RPAR
)

stmt = Forward()

ifstmt = IF - LPAR + expr + RPAR + stmt + Optional(ELSE + stmt)
whilestmt = WHILE - LPAR + expr + RPAR + stmt
dowhilestmt = DO - stmt + WHILE + LPAR + expr + RPAR + SEMI
returnstmt = RETURN - expr + SEMI

stmt << Group(
	ifstmt
	| whilestmt
	| dowhilestmt
	| returnstmt
	| expr + SEMI
	| LBRACE + ZeroOrMore(stmt) + RBRACE
	| SEMI
)

vardecl = Group(TYPE + NAME + Optional(LBRACK + integer + RBRACK)) + SEMI

arg_dir = Keyword("out") | Keyword("inout")
arg = Group(Optional(arg_dir) + TYPE + NAME)
body = ZeroOrMore(vardecl) + ZeroOrMore(stmt)
fundecl = Group(
	TYPE
	+ NAME
	+ LPAR
	+ Optional(Group(delimitedList(arg)))
	+ RPAR
	+ LBRACE
	+ Group(body)
	+ RBRACE
)
decl = fundecl | vardecl
program = Dict(ZeroOrMore(decl))

program.ignore(cStyleComment)

# set parser element names
for vname in (
	"ifstmt whilestmt dowhilestmt returnstmt TYPE "
	"NAME fundecl vardecl program arg body stmt".split()
):
	v = vars()[vname]
	v.setName(vname)

# ~ for vname in "fundecl stmt".split():
# ~ v = vars()[vname]
# ~ v.setDebug()

def prettifySlangCodeString(slang_str=""):
	if not slang_str:
		return slang_str

	if not isinstance(slang_str, (str, unicode)):
		raise ValueError('Slang string should be of type "str" or "unicode", got "%s"' % type(slang_str))

	# split lines by new-line
	#slang_str_lines_list = slang_str.splitlines()

	# strip lines
	#for slang_str_line in slang_str_lines_list:
	#	slang_str_line = slang_str_line.strip()

	test = r"""
	/* A factorial program */
	int
	putstr(char *s)
	{
		while(*s)
			putchar(*s++);
	}
	
	int
	fac(int n)
	{
		if (n == 0)
			return 1;
		else
			return n*fac(n-1);
	}
	
	int
	putn(int n)
	{
		if (9 < n)
			putn(n / 10);
		putchar((n%10) + '0');
	}
	
	int
	facpr(int n)
	{
		putstr("factorial ");
		putn(n);
		putstr(" = ");
		putn(fac(n));
		putstr("\n");
	}
	
	int
	main()
	{
		int i;
		i = 0;
		if(a() == 1){}
		while(i < 10)
			facpr(i++);
		return 0;
	}
	"""

	raw_slang_str = r'{0}'.format(slang_str)

	print slang_str

	test2 = r"""
	int a = 10;
	displacement mat_constant(float3 _uv, float _parm, float3 _ce, out float3 _ce) {
		/* BEGIN CODE BY: /mat/constant */
	
		/* END CODE BY: /mat/constant */
	}

	"""

	ast = program.parseString(raw_slang_str)#, parseAll=True)
	#ast = program.parseString(test2, parseAll=True)
	ast.pprint()

	return slang_str

def prettifySlangCodeStringSimple(slang_str=""):
	if not slang_str: return slang_str

	# split into lines
	lines = slang_str.splitlines()
	
	# strip line leading space, tabs, newlines etc..
	for i in range(0, len(lines)):
		lines[i] = lines[i].lstrip()

	# remove empty lines
	#lines = [line for line in lines if line]

	result = '\n'.join(lines)
	return result