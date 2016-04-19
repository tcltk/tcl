while (<>) {
	push(@l, $_);
}

foreach $_ (sort(@l)) {
	print;
}
