from string import Template

from .utils import prettifySlangCodeStringSimple

text = r"""
	// hopa
    typedef int Node, Hash;
    void HashPrint(Hash* hash, void (*PrintFunc)(char*, char*))
    {
        unsigned int i;
        if (hash == NULL || hash->heads == NULL)
            return;
        for (i = 0; i < hash->table_size; ++i)
        {
            Node* temp = hash->heads[i];
            while (temp != NULL)
            {
                PrintFunc(temp->entry->key, temp->entry->value);
                temp = temp->next;
            }
        }
    }
"""

class CodeTemplate:
	
	def __init__(self):
		self._code_lines = []

	def addLine(self, line_string):
		if not isinstance(line_string, str):
			raise ValueError('Argument "line_string" should be of type "str", got "%s"' % type(line_string))

		self._code_lines += [line_string]

	def getTemplateString(self):
		return Template('\n'.join(self._code_lines))


class Code(object):
	def __init__(self, other=None):
		self._body = ""
		self._args = ""
		self._temp = ""
		
		if other:
			self._body = other.body
			self._args = other.args
			self._temp = other.temp
		
	@property
	def body(self):
		return self._body

	@body.setter
	def body(self, b):
		self._body = b

	@property
	def args(self):
		return self._args

	@args.setter
	def args(self, a):
		self._args = a

	@property
	def temp(self):
		return self._temp

	@temp.setter
	def temp(self, t):
		self._temp = t

	def __add__(self, other):
		code = Code(self)
		
		if code.args and other.args: code.args = '\n'.join([code.args, other.args])
		elif other.args: code.args = other.args
		
		if code.temp and other.temp: code.temp = '\n'.join([code.temp, other.temp])
		elif other.temp: code.temp = other.temp

		if code.body and other.body: code.body = '\n'.join([code.body, other.body])
		elif other.body: code.body = other.body
		
		return code

	def __repr__(self):
		return 'Code(\nargs:\n%s\nbody:\n%s\n)' % (self.args, self.body)

	def prettify(self):
		self._args = prettifySlangCodeStringSimple(self._args)
		self._body = prettifySlangCodeStringSimple(self._body)

