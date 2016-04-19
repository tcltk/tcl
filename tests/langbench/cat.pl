# One could argue this should be
# while ($foo = <>) { chomp($foo); print $foo . "\n"; }
# to match what tcl does.
# That slows it down by a factor of 2.
foreach $file (@ARGV) {
	open(FD, $file);
	while ($buf = <FD>) {
		print $buf;
	}
}
