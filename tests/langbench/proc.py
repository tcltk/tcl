#!/usr/bin/python
   
def a(val):
	return b(val)
def b(val):
	return c(val)
def c(val):
	return d(val)
def d(val):
	return e(val)
def e(val):
	return f(val)
def f(val):
	return g(val, 2)
def g(v1, v2):
	return h(v1, v2, 3)
def h(v1, v2, v3):
	return i(v1, v2, v3, 4)
def i(v1, v2, v3, v4):
	return j(v1, v2, v3, v4, 5)
def j(v1, v2, v3, v4, v5):
	return v1 + v2 + v3 + v4 + v5

n = 100000
while n > 0:
	x = a(n)
	n = n - 1
print "x=%d" % x
