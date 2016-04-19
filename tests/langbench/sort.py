import os
import sys
import re

l = []
for a in sys.argv:
	f = open(a)
	for line in f:
		l.append(line)
	f.close()
l.sort()
for line in l:
	print line,
