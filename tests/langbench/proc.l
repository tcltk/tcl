int a(int val) { return b(val); }
int b(int val) { return c(val); }
int c(int val) { return d(val); }
int d(int val) { return e(val); }
int e(int val) { return f(val); }
int f(int val) { return g(val, 2); }
int g(int v1, int v2) { return h(v1, v2, 3); }
int h(int v1, int v2, int v3) { return i(v1, v2, v3, 4); }
int i(int v1, int v2, int v3, int v4) { return j(v1, v2, v3, v4, 5); }
int j(int v1, int v2, int v3, int v4, int v5) { return v1 + v2 + v3 + v4 + v5; }

void
main()
{
	int	n = 100000;	// there are 10 procs, so .1M iterations
	int	x;

	while (n > 0) { x = a(n); n--; }
	printf("x=%d\n", x);
}
