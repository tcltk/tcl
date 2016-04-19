while (<>) {
	$hash{$_} = 1;
}
open(FD, "/proc/$$/status");
while (<FD>) { print if /^Vm[RD]/; }
