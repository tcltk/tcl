re = Regexp.compile("[^A-Za-z]fopen\\(.*\\)")
while line = gets()
	print if re =~ line
end
