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

