struct node {
	double	i;
};

struct node tree[100000];

int
insert(int node, struct node element)
{
	/* using Tcl's {} as NULL */
	if (tree[node] == "{}") {
		tree[node] = element;
	} else if (tree[node].i > element.i) {
		insert(2*node+1, element);
	} else {
		insert(2*node+2, element);
	}
}

int
print(int node)
{
	if (tree[node] == "{}") {
		return;
	}
	print(2*node+1);
	puts(tree[node].i);
	print(2*node+2);
}

int
main()
{
	struct node tmp;
	int	i;

	namespace("import", "::tcl::mathfunc::*");
	for (i = 0; i < 100; i++) {
		tmp.i = floor(rand()*100);
		insert(0, tmp);
	}
	print(0);
}
