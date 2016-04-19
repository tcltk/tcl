void
main(int ac, string av[])
{
	int	i;
	FILE	f;
	string	buf;
	string	l[];

	fconfigure("stdout", buffering: "full", translation: "binary");
	for (i = 1; i < ac; ++i) {
		f = open(av[i], "rb");
		while (gets(f, &buf) >= 0) {
			push(&l, buf);
		}
	}
	foreach (buf in sort(l)) {
		puts(buf);
	}
}
