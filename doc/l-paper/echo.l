main(int argc, string argv[])
{
	int	i;

	for (i = 0; defined(argv[i]); i++) {
		printf("[%d] = %s\n", i, argv[i]);
	}
}
