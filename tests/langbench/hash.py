import os
import sys
import re

d = {}
for a in sys.argv:
	f = open(a)
	for line in f:
		d[line] = 1
	f.close
p = re.compile("^Vm[RD]")
f = open("/proc/%d/status" % os.getpid())
for line in f:
	m = p.match(line)
	if m:
		print line,
