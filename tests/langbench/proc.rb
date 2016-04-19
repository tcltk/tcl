def a(i)
	return b(i)
end
def b(i)
	return c(i)
end
def c(i)
	return d(i)
end
def d(i)
	return e(i)
end
def e(i)
	return f(i)
end
def f(i)
	return g(i, 2)
end
def g(v1, v2)
	return h(v1, v2, 3)
end
def h(v1, v2, v3)
	return i(v1, v2, v3, 4)
end
def i(v1, v2, v3, v4)
	return j(v1, v2, v3, v4, 5)
end
def j(v1, v2, v3, v4, v5)
	return v1 + v2 + v3 + v4 + v5
end
n = 100000;
while n > 0
	x = a(n)
	n -= 1
end
print "#{x}\n";
