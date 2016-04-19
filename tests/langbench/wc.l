string []
wordsplit(string str)
{
	string	chars[];
	string	list[];
	string	c, word;
	int	i;

	word = "";
	chars = split(str, "");
	foreach (c in chars) {
		if (string("is", "space", c)) {
			if (length(word) > 0) {
				push(&list, word);
			}
			word = "";
		} else {
			append(&word, c);
		}
	}
	if (length(word) > 0) {
		push(&list, word);
	}
	return (list);
}

int
doit(string file)
{
	FILE	f = open(file, "rb");
	string	buf;
	string	words[];
	int	n;

	while (gets(f, &buf) >= 0) {
		words = wordsplit(buf);
		n += llength(words);
	}
	close(f);
	return (n);
}

void
main(int ac, string av[])
{
	int	total, i;

	for (i = 1; i < ac; ++i) {
		total += doit(av[i]);
	}
	printf("%d\n", total);
}
