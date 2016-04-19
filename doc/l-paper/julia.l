#pragma language L

julia(int size, int depth, double zreal, double zimag)
{
	double  z[2];
	double  real_min = -1.2, real_max = 1.2, imag_min = -1.2;
	double  xreal, ximag;
	double delta = (real_max - real_min) / size;
	int    i = 0, j = 0;

	wm("title", ".", "Pretty Julia");
	wm("geometry", ".", "+1+1");
	canvas(".c1", "-width", size, "-height", size);
	pack(".c1");
	update();
	z[0] = zreal;
	z[1] = zimag;
	xreal = real_min;
	while (i < size) {
		j = 0;
		ximag = imag_min;
		while (j < size) {
			double  count = 0.0;
			double  x[2];

			x[0] = xreal;
			x[1] = ximag;
			while ((count < depth) && (complex_abs(x) < 2.0)) {
				count = count + 1;
				x = complex_add(complex_multiply(x, x), z);
			}
			if (complex_abs(x) <= 2.0) {
				.c1("create", "rectangle", i, j, i, j,
				    "-outline", make_color(0, 0, 0));
			} else {
				double  intensity = count / depth;

				if (intensity > 0.001) {
					.c1("create", "rectangle", i, j, i, j,
					    "-outline",
					    make_color(round(255.0 / depth *
						    (depth -
							depth * intensity)),
						round(255.0 / depth * (depth -
							depth * intensity)),
						round(255.0 / depth * depth)));
				}
			}
			j++;
			ximag = ximag + delta;
		} i++;
		puts("-nonewline", format("%d .. ", i));
		flush("stdout");
		update();
		xreal = xreal + delta;
	} z[0] = zreal;
	z[1] = zimag;
	puts("\nDONE");
}

string
make_color(int r, int g, int b)
{
	return format("#%02X%02X%02X", r, g, b);
}

double
complex_multiply(double c1[2], double c2[2])
{
	double  c3[2];

	c3[0] = c1[0] * c2[0] - c1[1] * c2[1];
	c3[1] = c1[0] * c2[1] + c1[1] * c2[0];
	return c3;
}

double
complex_add(double c1[2], double c2[2])
{
	double  c3[2];

	c3[0] = c1[0] + c2[0];
	c3[1] = c1[1] + c2[1];
	return c3;
}

#pragma end

interp alias {} sqrt {} tcl::mathfunc::sqrt
interp alias {} round {} tcl::mathfunc::round
proc complex_abs {x} {
    puts "complex_abs $x"
    set a [lindex $x 0]
    set b [lindex $x 1]
    expr {sqrt(($a * $a)+($b * $b))}
}
#puts [time {julia 100 25 -0.74543 0.11302}]
puts [time {julia 300 25 -0.84543 0.21302}]
# bigger, different coords:
#julia 256 25 -0.84543 0.21302
# if you raise the depth, the islands get smaller:
#julia 256 50 -0.84543 0.21302

