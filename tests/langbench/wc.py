#!/usr/bin/python
import os
import sys

def wordsplit(line):
    list = []
    word = ""
    for c in line:
	if c.isspace():
	    if len(word) > 0:
		list.append(word)
	    word = ""
	else:
	    word += c
    if len(word) > 0:
	list.append(word)
    return list

def main():
    n = 0
    for a in sys.argv[1:]:
    	f = open(a)
    	for line in f:
        	words = wordsplit(line)
		n += len(words)
    	f.close()
    print "%d\n" % n
   
if __name__ == "__main__":
    main()
