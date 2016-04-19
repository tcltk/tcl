#!../../unix/tclsh

int
main(string av[])
{
	FILE	f;
	int	i, ul;
	int	space = 0, dd = 0, p = 0, pre = 0;
	string	buf, c, tmp, title, trim, all[];

	/*
	 * -t<title> or --title=<title>
	 */
	while (c = getopt(av, "t:", {"title:"})) {
		switch (c) {
		    case "t":
		    case "title":
			title = optarg;
			break;
		}
	}
	unless (av[optind] && (f = fopen(av[optind], "r"))) {
		die("usage: ${av[0]} filename");
	}
	unless (title) title = av[optind];

	header(title);

	/*
	 * Load up the whole file in all[] and spit out the index.
	 */
	puts("<ul>");
	ul = 1;
	while (buf = <f>) {
		push(&all, buf);
		if (buf =~ /^=head(\d+)\s+(.*)/) {
			i = (int)$1;
			while (ul > i) {
				puts("</ul>");
				ul--;
			}
			while (i > ul) {
				puts("<ul>");
				ul++;
			}
			tmp = $2;
			tmp =~ s/\s+/_/g;
			buf =~ s/^=head(\d+)\s+//;
			puts("<li><a href=\"#${tmp}\">${buf}</a></li>");
		}
	}
	while (ul--) puts("</ul>");
	fclose(f);

	/*
	 * Now walk all[] and process the markup.  We currently handle:
	 * =head%d title
	 * =over
	 * =item name
	 * =proto return_type func(args)
	 * =back
	 * <blank line>
	 * B<bold this>
	 * C<some code>
	 * I<italics>
	 */
	// The <= is intentional to run an empty string through at the end
	// to kick out any final </pre> etc.
	for (i = 0; i <= length(all); i++) {
		buf = inline(all[i]);
		if (buf =~ /^=head(\d+)\s+(.*)/) {
			if ((int)$1 == 1) puts("<HR>");
			tmp = $2;
			tmp =~ s/\s+/_/g;
			printf("<H%d><a name=\"%s\">%s</a></H%d>\n",
			    $1, tmp, $2, $1);
		} else if (buf =~ /^=over/) {
			puts("<dl>");
		} else if (buf =~ /^=item\s+(.*)/) {
			if (dd) {
				puts("</dd>");
				dd--;
			}
			puts("<dt><strong>${$1}</strong></dt><dd>");
			dd++;
		} else if (buf =~ /^=proto\s+([^ \t]+)\s+(.*)/) {
			if (dd) {
				puts("</dd>");
				dd--;
			}
			puts("<dt><b>${$1} ${$2}</b></dt><dd>");
			dd++;
		} else if (buf =~ /^=back/) {
			if (dd) {
				puts("</dd>");
				dd--;
			}
			puts("</dl>");
		} else if (buf =~ /^\s*$/) {
			if (p) {
				puts("</p>");
				p = 0;
			}
			if (pre) {
				/*
				 * If we see a blank line in a preformatted
				 * block, we don't want to stop the pre
				 * unless the next line is not indented.
				 * So peek ahead.
				 */
				if (defined(buf = all[i+1]) && (buf =~ /^\s/)) {
					puts("");
					continue;
				}
				puts("</pre>");
				pre = 0;
				trim = undef;
			}
			space = 1;
		} else {
			if (space) {
				if (buf =~ /^(\s+)[^ \t]+/) {
					trim = $1;
					puts("<pre>");
					pre = 1;
				} else {
					puts("<p>");
					p = 1;
				}
				space = 0;
			}
			if (defined(trim)) buf =~ s/^${trim}//;
			puts(buf);
		}
	}
	puts("</body></html>");
	return (0);
}

/*
 * header and style sheet
 */
void
header(string title)
{
	string	head = <<EOF
<html>
<head>
<title>${title}</title>
<style>
pre {
	background: #eeeedd;
	border-width: 1px;
	border-style: solid solid solid solid;
	border-color: #ccc;
	padding: 5px 5px 5px 5px;
	font-family: monospace;
	font-weight: bolder;
}
body {
	padding-left: 10px;
}
dt {
	font-size: large;
}
</style>
</head>
<body>
EOF
	puts(head);
	puts("<h1>${title}</h1>");
}

/*
 * Process B<bold>, C<code>, I<italic>, F<italic>, L<link>, S<non-breaking>.
 * This will handle nested stuff like C<if (!I<condition>)>
 * but dies if there are nested ones of the same type, i.e.,
 * C<whatever C<some more>>
 */
string
inline(string buf)
{
	string	c, prev, result = "", link, stack[];
	int	B = 0, C = 0, I = 0, L = 0, S = 0;

	unless (buf) buf = "";
	foreach (c in buf) {
		if ((c == "<") && prev) {
			if (prev == "B") {
				if (B++) die("Nested B<> unsupported: ${buf}");
				result[END] = "";
				result .= "<B>";
				push(&stack, "B");
			} else if (prev == "C") {
				if (C++) die("Nested C<> unsupported: ${buf}");
				result[END] = "";
				result .= "<CODE>";
				push(&stack, "CODE");
			} else if (prev == "I" || prev == "F") {
				if (I++) die("Nested I<> unsupported: ${buf}");
				result[END] = "";
				result .= "<I>";
				push(&stack, "I");
			} else if (prev == "L") {
				if (L++) die("Nested L<> unsupported: ${buf}");
				result[END] = "";
				result .= "<a href=\"";
				link = "";
				push(&stack, "L");
			} else if (prev == "S") {
				if (S++) die("Nested S<> unsupported: ${buf}");
				result[END] = "";
				push(&stack, "S");
			} else {
				result .= "&lt;";
				prev = c;
			}
		} else if ((c == ">") && length(stack)) {
			c = pop(&stack);
			if (c == "B") {
				B--;
			} else if (c == "CODE") {
				C--;
			} else if (c == "I") {
				I--;
			} else if (c == "L") {
				L--;
				result .= "\">${link}</a>";
				c = undef;
			} else {
				S--;
				c = undef;
			}
			if (c) {
				result .= "</" . c . ">";
			}
			prev = undef;
		} else {
			if (S && isspace(c)) {
				result .= "&nbsp;";
			} else if (c == "<") {
				result .= "&lt;";
			} else if (c == ">") {
				result .= "&gt;";
			} else {
				result .= c;
			}
			if (L) link .= c;
			prev = c;
		}
	}
	return (result);
}
