while (<>) {
	print if /[^A-Za-z]fopen\(.*\)/;
}
