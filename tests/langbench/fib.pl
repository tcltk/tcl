sub fib
{
	my($n) = @_[0];

	return $n if $n < 2;
	return &fib($n - 1) + &fib($n - 2);
}

for ($i = 0; $i <= 30; ++$i) {
	printf "n=%d => %d\n", $i, &fib($i);
}
