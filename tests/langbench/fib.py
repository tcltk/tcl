def fib(n):
   if n < 2:
      return n
   else:
      return fib(n-1) + fib(n-2)

for i in range(30):
    print "n=%d => %d" % (i, fib(i))
