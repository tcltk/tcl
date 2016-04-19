#!/usr/bin/python
import os
import sys
   
def cat(file):
    f = open(file)
    for line in f:
        print line,
    f.close()
   
def main():
    for a in sys.argv:
        cat(a)
   
if __name__ == "__main__":
    main()
