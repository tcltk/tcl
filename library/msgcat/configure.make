configure: configure.in
	-autoconf

configure.in: $(PACKAGE).tcl
	version=`echo "source $(PACKAGE).tcl; \
	puts [package provide $(PACKAGE)]" | $(TCLSH)`; \
	(\
	echo "AC_INIT([$(PACKAGE)], [$$version])"; \
	echo "AC_OUTPUT([Makefile])"; \
	) > configure.in
