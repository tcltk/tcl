
int
fib(int n)
{
	if (n < 2) {
		return (n);
	} else {
		return (fib(n - 1) + fib(n - 2));
	}
}

void
main()
{
	int	i;

	for (i = 0; i <= 30; ++i) {
		printf("n=%d => %d\n", i, fib(i));
	}
}
