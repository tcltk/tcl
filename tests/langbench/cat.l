void
main(int ac, string av[])
{
	string	buf;
	FILE	f;
	int	i;

	fconfigure("stdout", buffering:"full", translation:"binary");
	for (i = 1; i < ac; ++i) {
		unless (f = open(av[i], "rb")) continue;
		while (gets(f, &buf) >= 0) {
			// roughly 40% slower than puts
			// printf("%s\n", buf);
			puts(buf);
		}
		close(f);
	}
}
