typedef string FILE;

main(int ac, string av[], hash env)
{
	int	i;
	string	regex;
	FILE	fd;

        if (ac < 2) {
                error("Not enough arguments.");
        }
	regex = av[1];
	ac--;
	if (ac == 1) {
		grep(regex, "stdin");
	} else {
		for (i = 2; defined(av[i]); i++) {
			fd = open(av[i], "r");
			grep(regex, fd);
			close(fd);
		}
	}
}

void
grep(string regex, FILE in)
{
	string	buf;

	while (gets(in, &buf) >= 0) {
		if (buf =~ /${regex}/) {
			printf("%s\n", buf);
		}
	}
}
