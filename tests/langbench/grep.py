import os
import sys
import re

p = re.compile('[^A-Za-z]fopen\(.*\)')
for a in sys.argv:
	f = open(a)
	for line in f:
		m = p.search(line)
		if m:
			print line,
	f.close()
