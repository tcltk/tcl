main(int argc, string argv[], hash env)
{
	string	key, val;

	foreach (env as key => val) {
		printf("env{%s} = %s\n", key, val);
	}

}
