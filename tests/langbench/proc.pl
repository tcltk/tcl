sub a { return &b($_[0]); }
sub b { return &c($_[0]); }
sub c { return &d($_[0]); }
sub d { return &e($_[0]); }
sub e { return &f($_[0]); }
sub f { return &g($_[0], 2); }
sub g { return &h($_[0], $_[1], 3); }
sub h { return &i($_[0], $_[1], $_[2], 4); }
sub i { return &j($_[0], $_[1], $_[2], $_[3], 5); }
sub j { return $_[0] + $_[1] + $_[2] + $_[3] + $_[4]; }
$n = 100000;
while ($n > 0) { $x = &a($n); $n--; }
print "$x\n";
