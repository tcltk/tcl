void
main(int ac, string av[])
{
	int	i;
	string	buf;
	hash	h;
	FILE	f;


	for (i = 1; i < ac; ++i) {
		f = open(av[i], "rb");
		while (gets(f, &buf) >= 0) {
			h{buf} = 1;
		}
		close(f);
	}
}
