sub wordsplit
{
	chomp($_[0]);
	@list = ();
	$word = "";
	foreach $c (split(//, $_[0])) {
		if ($c =~ /\s/o) {
			push(@list, $word) if $word ne "";
			$word = "";
		} else {
			$word .= $c;
		}
	}
	push(@list, $word) if $word ne "";
	return @list;
}

$n = 0;
while (<>) {
	@words = &wordsplit($_);
	$n += $#words + 1;
}
printf "%d\n", $n;
