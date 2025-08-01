#! /bin/bash -norc
dnl	This file is an input file used by the GNU "autoconf" program to
dnl	generate the file "configure", which is run during Tcl installation
dnl	to configure the system for the local environment.

AC_INIT([tcl],[9.1])
AC_PREREQ([2.72])

dnl This is only used when included from macosx/configure.ac
m4_ifdef([SC_USE_CONFIG_HEADERS], [
    AC_CONFIG_HEADERS([tclConfig.h:../unix/tclConfig.h.in])
    AC_CONFIG_COMMANDS_PRE([DEFS="-DHAVE_TCL_CONFIG_H  -imacros tclConfig.h"])
    AH_TOP([
    #ifndef _TCLCONFIG
    #define _TCLCONFIG])
    AH_BOTTOM([
    /* Undef unused package specific autoheader defines so that we can
     * include both tclConfig.h and tkConfig.h at the same time: */
    /* override */ #undef PACKAGE_NAME
    /* override */ #undef PACKAGE_TARNAME
    /* override */ #undef PACKAGE_VERSION
    /* override */ #undef PACKAGE_STRING
    #endif /* _TCLCONFIG */])
])

TCL_VERSION=9.1
TCL_MAJOR_VERSION=9
TCL_MINOR_VERSION=1
TCL_PATCH_LEVEL="a0"
VERSION=${TCL_VERSION}

EXTRA_INSTALL_BINARIES=${EXTRA_INSTALL_BINARIES:-"@:"}
EXTRA_BUILD_HTML=${EXTRA_BUILD_HTML:-"@:"}

#------------------------------------------------------------------------
# Setup configure arguments for bundled packages
#------------------------------------------------------------------------

PKG_CFG_ARGS="$ac_configure_args ${PKG_CFG_ARGS}"

if test -r "$cache_file" -a -f "$cache_file"; then
    case $cache_file in
	[[\\/]]* | ?:[[\\/]]* ) pkg_cache_file=$cache_file ;;
	*) pkg_cache_file=../../$cache_file ;;
    esac
    PKG_CFG_ARGS="${PKG_CFG_ARGS} --cache-file=$pkg_cache_file"
fi

#------------------------------------------------------------------------
# Empty slate for bundled packages, to avoid stale configuration
#------------------------------------------------------------------------
#rm -Rf pkgs
if test -f Makefile; then
    make distclean-packages
fi

#------------------------------------------------------------------------
# Handle the --prefix=... option
#------------------------------------------------------------------------

if test "${prefix}" = "NONE"; then
    prefix=/usr/local
fi
if test "${exec_prefix}" = "NONE"; then
    exec_prefix=$prefix
fi
# Make sure srcdir is fully qualified!
srcdir="`cd "$srcdir" ; pwd`"
TCL_SRC_DIR="`cd "$srcdir"/..; pwd`"

#------------------------------------------------------------------------
# Compress and/or soft link the manpages?
#------------------------------------------------------------------------

SC_CONFIG_MANPAGES

#------------------------------------------------------------------------
# Standard compiler checks
#------------------------------------------------------------------------

# If the user did not set CFLAGS, set it now to keep
# the AC_PROG_CC macro from adding "-g -O2".
if test "${CFLAGS+set}" != "set" ; then
    CFLAGS=""
fi

AC_PROG_CC
AC_C_INLINE


#--------------------------------------------------------------------
# Supply substitutes for missing POSIX header files.  Special notes:
#	- stdlib.h doesn't define strtol or strtoul in some versions
#	  of SunOS
#	- some versions of string.h don't declare procedures such
#	  as strstr
# Do this early, otherwise an autoconf bug throws errors on configure
#--------------------------------------------------------------------

SC_MISSING_POSIX_HEADERS

#--------------------------------------------------------------------
# Determines the correct executable file extension (.exe)
#--------------------------------------------------------------------

AC_EXEEXT

#------------------------------------------------------------------------
# If we're using GCC, see if the compiler understands -pipe.  If so, use it.
# It makes compiling go faster.  (This is only a performance feature.)
#------------------------------------------------------------------------

if test -z "$no_pipe" && test -n "$GCC"; then
    AC_CACHE_CHECK([if the compiler understands -pipe],
	tcl_cv_cc_pipe, [
	hold_cflags=$CFLAGS; CFLAGS="$CFLAGS -pipe"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[tcl_cv_cc_pipe=yes],[tcl_cv_cc_pipe=no])
	CFLAGS=$hold_cflags])
    if test $tcl_cv_cc_pipe = yes; then
	CFLAGS="$CFLAGS -pipe"
    fi
fi

#------------------------------------------------------------------------
# Embedded configuration information, encoding to use for the values, TIP #59
#------------------------------------------------------------------------

SC_TCL_CFG_ENCODING

#--------------------------------------------------------------------
# Look for libraries that we will need when compiling the Tcl shell
#--------------------------------------------------------------------

SC_TCL_LINK_LIBS

# Add the threads support libraries
LIBS="$LIBS$THREADS_LIBS"

SC_ENABLE_SHARED

#--------------------------------------------------------------------
# Look for a native installed tclsh binary (if available)
# If one cannot be found then use the binary we build (fails for
# cross compiling). This is used for NATIVE_TCLSH in Makefile.
#--------------------------------------------------------------------

SC_PROG_TCLSH
if test "$TCLSH_PROG" = ""; then
  TCLSH_PROG='./${TCL_EXE}'
fi

#------------------------------------------------------------------------
#	Add stuff for zlib
#------------------------------------------------------------------------

zlib_ok=yes
AC_CHECK_HEADER([zlib.h],[
  AC_CHECK_TYPE([gz_header],[],[zlib_ok=no],[#include <zlib.h>])],[
  zlib_ok=no])
AS_IF([test $zlib_ok = yes], [
  AC_SEARCH_LIBS([deflateSetHeader],[z],[],[
    zlib_ok=no
  ])])
AS_IF([test $zlib_ok = no], [
  AC_SUBST(ZLIB_OBJS,[\${ZLIB_OBJS}])
  AC_SUBST(ZLIB_SRCS,[\${ZLIB_SRCS}])
  AC_SUBST(ZLIB_INCLUDE,[-I\${ZLIB_DIR}])
  AC_DEFINE(TCL_WITH_INTERNAL_ZLIB, 1, [Tcl with internal zlib])
])

#------------------------------------------------------------------------
#	Add stuff for libtommath

libtommath_ok=yes
AC_ARG_WITH(system-libtommath,
AS_HELP_STRING([--with-system-libtommath],
	[use external libtommath (default: true if available, false otherwise)]),
	[libtommath_ok=${withval}])
if test x"${libtommath_ok}" = x -o x"${libtommath_ok}" != xno; then
    AC_CHECK_HEADER([tommath.h],[
	AC_CHECK_TYPE([mp_int],[],[libtommath_ok=no],[#include <tommath.h>])],[
	libtommath_ok=no])
    AS_IF([test $libtommath_ok = yes], [
	AC_CHECK_LIB([tommath],[mp_log_u32],[MATH_LIBS="$MATH_LIBS -ltommath"],[
    libtommath_ok=no])])
fi
AS_IF([test $libtommath_ok = yes], [
  AC_SUBST(TCL_PC_REQUIRES_PRIVATE, ['libtommath >= 1.2.0,'])
  AC_SUBST(TCL_PC_CFLAGS, ['-DTCL_WITH_EXTERNAL_TOMMATH'])
  AC_DEFINE(TCL_WITH_EXTERNAL_TOMMATH, 1, [Tcl with external libtommath])
], [
  AC_SUBST(TOMMATH_OBJS,[\${TOMMATH_OBJS}])
  AC_SUBST(TOMMATH_SRCS,[\${TOMMATH_SRCS}])
  AC_SUBST(TOMMATH_INCLUDE,[-I\${TOMMATH_DIR}])
])

#--------------------------------------------------------------------
# The statements below define a collection of compile flags.  This
# macro depends on the value of SHARED_BUILD, and should be called
# after SC_ENABLE_SHARED checks the configure switches.
#--------------------------------------------------------------------

SC_CONFIG_CFLAGS

SC_ENABLE_SYMBOLS(bccdebug)

AC_DEFINE(MP_PREC, 4, [Default libtommath precision.])

#--------------------------------------------------------------------
#	Detect what compiler flags to set for 64-bit support.
#--------------------------------------------------------------------

SC_TCL_EARLY_FLAGS

SC_TCL_64BIT_FLAGS

#--------------------------------------------------------------------
#	Check endianness because we can optimize comparisons of
#	Tcl_UniChar strings to memcmp on big-endian systems.
#--------------------------------------------------------------------

AC_C_BIGENDIAN(,,,[#])

#--------------------------------------------------------------------
#	Supply substitutes for missing POSIX library procedures, or
#	set flags so Tcl uses alternate procedures.
#--------------------------------------------------------------------

# Check if Posix compliant getcwd exists, if not we'll use getwd.
AC_CHECK_FUNCS(getcwd, , [AC_DEFINE(USEGETWD, 1, [Is getcwd Posix-compliant?])])
# Nb: if getcwd uses popen and pwd(1) (like SunOS 4) we should really
# define USEGETWD even if the posix getcwd exists. Add a test ?

AC_REPLACE_FUNCS(mkstemp waitpid)
AC_CHECK_FUNC(strerror, , [AC_DEFINE(NO_STRERROR, 1, [Do we have strerror()])])
AC_CHECK_FUNC(getwd, , [AC_DEFINE(NO_GETWD, 1, [Do we have getwd()])])
AC_CHECK_FUNC(wait3, , [AC_DEFINE(NO_WAIT3, 1, [Do we have wait3()])])
AC_CHECK_FUNC(fork, , [AC_DEFINE(NO_FORK, 1, [Do we have fork()])])
AC_CHECK_FUNC(mknod, , [AC_DEFINE(NO_MKNOD, 1, [Do we have mknod()])])
AC_CHECK_FUNC(tcdrain, , [AC_DEFINE(NO_TCDRAIN, 1, [Do we have tcdrain()])])
AC_CHECK_FUNC(uname, , [AC_DEFINE(NO_UNAME, 1, [Do we have uname()])])

if test "`uname -s`" = "Darwin" && \
	test "`uname -r | awk -F. '{print [$]1}'`" -lt 7; then
    # prior to Darwin 7, realpath is not threadsafe, so don't
    # use it when threads are enabled, c.f. bug # 711232
    ac_cv_func_realpath=no
fi
AC_CHECK_FUNC(realpath, , [AC_DEFINE(NO_REALPATH, 1, [Do we have realpath()])])

SC_TCL_IPV6

#--------------------------------------------------------------------
#	Look for thread-safe variants of some library functions.
#--------------------------------------------------------------------

SC_TCL_GETPWUID_R
SC_TCL_GETPWNAM_R
SC_TCL_GETGRGID_R
SC_TCL_GETGRNAM_R
if test "`uname -s`" = "Darwin" && \
	test "`uname -r | awk -F. '{print [$]1}'`" -gt 5; then
    # Starting with Darwin 6 (Mac OSX 10.2), gethostbyX
    # are actually MT-safe as they always return pointers
    # from TSD instead of static storage.
    AC_DEFINE(HAVE_MTSAFE_GETHOSTBYNAME, 1,
	    [Do we have MT-safe gethostbyname() ?])
    AC_DEFINE(HAVE_MTSAFE_GETHOSTBYADDR, 1,
	    [Do we have MT-safe gethostbyaddr() ?])

elif test "`uname -s`" = "HP-UX" && \
	test "`uname -r|sed -e 's|B\.||' -e 's|\..*$||'`" -gt 10; then
    # Starting with HPUX 11.00 (we believe), gethostbyX
    # are actually MT-safe as they always return pointers
    # from TSD instead of static storage.
    AC_DEFINE(HAVE_MTSAFE_GETHOSTBYNAME, 1,
	    [Do we have MT-safe gethostbyname() ?])
    AC_DEFINE(HAVE_MTSAFE_GETHOSTBYADDR, 1,
	    [Do we have MT-safe gethostbyaddr() ?])

else
    SC_TCL_GETHOSTBYNAME_R
    SC_TCL_GETHOSTBYADDR_R
fi

#---------------------------------------------------------------------------
#	Check for serial port interface.
#
#	termios.h is present on all POSIX systems.
#	sys/ioctl.h is almost always present, though what it contains
#	is system-specific.
#	sys/modem.h is needed on HP-UX.
#---------------------------------------------------------------------------

AC_CHECK_HEADERS(termios.h)
AC_CHECK_HEADERS(sys/ioctl.h)
AC_CHECK_HEADERS(sys/modem.h)

#--------------------------------------------------------------------
#	Include sys/select.h if it exists and if it supplies things
#	that appear to be useful and aren't already in sys/types.h.
#	This appears to be true only on the RS/6000 under AIX.  Some
#	systems like OSF/1 have a sys/select.h that's of no use, and
#	other systems like SCO UNIX have a sys/select.h that's
#	pernicious.  If "fd_set" isn't defined anywhere then set a
#	special flag.
#--------------------------------------------------------------------

AC_CACHE_CHECK([for fd_set in sys/types], tcl_cv_type_fd_set, [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>]], [[fd_set readMask, writeMask;]])],
	[tcl_cv_type_fd_set=yes],[tcl_cv_type_fd_set=no])])
tcl_ok=$tcl_cv_type_fd_set
if test $tcl_ok = no; then
    AC_CACHE_CHECK([for fd_mask in sys/select], tcl_cv_grep_fd_mask, [
	AC_EGREP_HEADER(fd_mask, sys/select.h,
	     tcl_cv_grep_fd_mask=present, tcl_cv_grep_fd_mask=missing)])
    if test $tcl_cv_grep_fd_mask = present; then
	AC_DEFINE(HAVE_SYS_SELECT_H, 1, [Should we include <sys/select.h>?])
	tcl_ok=yes
    fi
fi
if test $tcl_ok = no; then
    AC_DEFINE(NO_FD_SET, 1, [Do we have fd_set?])
fi

AC_CACHE_CHECK([for pselect], tcl_cv_func_pselect, [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>]], [[void *func = pselect;]])],[tcl_cv_func_pselect=yes],[tcl_cv_func_pselect=no])])
tcl_ok=$tcl_cv_func_pselect
if test $tcl_ok = yes; then
    AC_DEFINE(HAVE_PSELECT, 1, [Should we use pselect()?])
fi

#------------------------------------------------------------------------
#	Options for the notifier. Checks for epoll(7) on Linux, and
#	kqueue(2) on {DragonFly,Free,Net,Open}BSD
#------------------------------------------------------------------------

AC_MSG_CHECKING([for advanced notifier support])
case x`uname -s` in
  xLinux)
	AC_MSG_RESULT([epoll(7)])
	AC_CHECK_HEADERS([sys/epoll.h],
	    [AC_DEFINE(NOTIFIER_EPOLL, [1], [Is epoll(7) supported?])])
	AC_CHECK_HEADERS([sys/eventfd.h],
	    [AC_DEFINE(HAVE_EVENTFD, [1], [Is eventfd(2) supported?])]);;
  xDragonFlyBSD|xFreeBSD|xNetBSD|xOpenBSD)
	AC_MSG_RESULT([kqueue(2)])
	# Messy because we want to check if *all* the headers are present, and not
	# just *any*
	tcl_kqueue_headers=x
	AC_CHECK_HEADERS([sys/types.h sys/event.h sys/time.h],
	    [tcl_kqueue_headers=${tcl_kqueue_headers}y])
	AS_IF([test $tcl_kqueue_headers = xyyy], [
	    AC_DEFINE(NOTIFIER_KQUEUE, [1], [Is kqueue(2) supported?])]);;
  xDarwin)
	# Assume that we've got CoreFoundation present (checked elsewhere because
	# of wider impact).
	AC_MSG_RESULT([OSX]);;
  *)
	AC_MSG_RESULT([none]);;
esac

#------------------------------------------------------------------------------
#       Find out all about time handling differences.
#------------------------------------------------------------------------------

SC_TIME_HANDLER

#--------------------------------------------------------------------
#	Some systems (e.g., IRIX 4.0.5) lack some fields in struct stat. But
#	we might be able to use fstatfs instead. Some systems (OpenBSD?) also
#	lack blkcnt_t.
#--------------------------------------------------------------------

if test "$ac_cv_cygwin" != "yes"; then
    AC_CHECK_MEMBERS([struct stat.st_blocks, struct stat.st_blksize, struct stat.st_rdev])
fi
AC_CHECK_TYPES([blkcnt_t])
AC_CHECK_FUNC(fstatfs, , [AC_DEFINE(NO_FSTATFS, 1, [Do we have fstatfs()?])])

#--------------------------------------------------------------------
#	Check for various typedefs and provide substitutes if
#	they don't exist.
#--------------------------------------------------------------------

AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T

AC_CACHE_CHECK([for socklen_t], tcl_cv_type_socklen_t, [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
	#include <sys/types.h>
	#include <sys/socket.h>
    ]], [[
	socklen_t foo;
    ]])],[tcl_cv_type_socklen_t=yes],[tcl_cv_type_socklen_t=no])])
if test $tcl_cv_type_socklen_t = no; then
    AC_DEFINE(socklen_t, int, [Define as int if socklen_t is not available])
fi

AC_CHECK_TYPES([intptr_t, uintptr_t],,,[[
#include <stdint.h>
]])

#--------------------------------------------------------------------
#	The check below checks whether <sys/wait.h> defines the type
#	"union wait" correctly.  It's needed because of weirdness in
#	HP-UX where "union wait" is defined in both the BSD and SYS-V
#	environments.  Checking the usability of WIFEXITED seems to do
#	the trick.
#--------------------------------------------------------------------

AC_CACHE_CHECK([union wait], tcl_cv_union_wait, [
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#include <sys/wait.h>]], [[
union wait x;
WIFEXITED(x);		/* Generates compiler error if WIFEXITED
			 * uses an int. */
    ]])],[tcl_cv_union_wait=yes],[tcl_cv_union_wait=no])])
if test $tcl_cv_union_wait = no; then
    AC_DEFINE(NO_UNION_WAIT, 1, [Do we have a usable 'union wait'?])
fi

#--------------------------------------------------------------------
#	Check whether there is an strncasecmp function on this system.
#	This is a bit tricky because under SCO it's in -lsocket and
#	under Sequent Dynix it's in -linet.
#--------------------------------------------------------------------

AC_CHECK_FUNC(strncasecmp, tcl_ok=1, tcl_ok=0)
if test "$tcl_ok" = 0; then
    AC_CHECK_LIB(socket, strncasecmp, tcl_ok=1, tcl_ok=0)
fi
if test "$tcl_ok" = 0; then
    AC_CHECK_LIB(inet, strncasecmp, tcl_ok=1, tcl_ok=0)
fi
if test "$tcl_ok" = 0; then
    AC_LIBOBJ([strncasecmp])
    USE_COMPAT=1
fi

#--------------------------------------------------------------------
#	The code below deals with several issues related to gettimeofday:
#	1. Some systems don't provide a gettimeofday function at all
#	   (set NO_GETTOD if this is the case).
#	2. See if gettimeofday is declared in the <sys/time.h> header file.
#	   if not, set the GETTOD_NOT_DECLARED flag so that tclPort.h can
#	   declare it.
#--------------------------------------------------------------------

AC_CHECK_FUNC(gettimeofday,[],[
    AC_DEFINE(NO_GETTOD, 1, [Do we have gettimeofday()?])
])
AC_CACHE_CHECK([for gettimeofday declaration], tcl_cv_grep_gettimeofday, [
    AC_EGREP_HEADER(gettimeofday, sys/time.h,
	tcl_cv_grep_gettimeofday=present, tcl_cv_grep_gettimeofday=missing)])
if test $tcl_cv_grep_gettimeofday = missing ; then
    AC_DEFINE(GETTOD_NOT_DECLARED, 1, [Is gettimeofday() actually declared in <sys/time.h>?])
fi

#--------------------------------------------------------------------
#	The following code checks to see whether it is possible to get
#	signed chars on this platform.  This is needed in order to
#	properly generate sign-extended ints from character values.
#--------------------------------------------------------------------

AC_C_CHAR_UNSIGNED
AC_CACHE_CHECK([signed char declarations], tcl_cv_char_signed, [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
	signed char *p;
	p = 0;
	]])],[tcl_cv_char_signed=yes],[tcl_cv_char_signed=no])])
if test $tcl_cv_char_signed = yes; then
    AC_DEFINE(HAVE_SIGNED_CHAR, 1, [Are characters signed?])
fi

#--------------------------------------------------------------------
#  Does putenv() copy or not?  We need to know to avoid memory leaks.
#--------------------------------------------------------------------

AC_CACHE_CHECK([for a putenv() that copies the buffer], tcl_cv_putenv_copy, [
    AC_RUN_IFELSE([AC_LANG_SOURCE([[
	#include <stdlib.h>
	#include <string.h>
	#define OURVAR "havecopy=yes"
	int main (int argc, char *argv[])
	{
	    char *foo, *bar;
	    foo = (char *)strdup(OURVAR);
	    putenv(foo);
	    strcpy((char *)(strchr(foo, '=') + 1), "no");
	    bar = getenv("havecopy");
	    if (!strcmp(bar, "no")) {
		/* doesnt copy */
		return 0;
	    } else {
		/* does copy */
		return 1;
	    }
	}
    ]])],
    [tcl_cv_putenv_copy=no],
    [tcl_cv_putenv_copy=yes],
    [tcl_cv_putenv_copy=no])])
if test $tcl_cv_putenv_copy = yes; then
    AC_DEFINE(HAVE_PUTENV_THAT_COPIES, 1,
	[Does putenv() copy strings or incorporate them by reference?])
fi

#--------------------------------------------------------------------
# Check for support of nl_langinfo function
#--------------------------------------------------------------------

SC_ENABLE_LANGINFO

#--------------------------------------------------------------------
# Check for support of cfmakeraw, chflags and mkstemps functions
#--------------------------------------------------------------------

AC_CHECK_FUNCS(cfmakeraw chflags mkstemps)

#--------------------------------------------------------------------
# Darwin specific API checks and defines
#--------------------------------------------------------------------

if test "`uname -s`" = "Darwin" ; then
    AC_CHECK_FUNCS(getattrlist)
    AC_CHECK_HEADERS(copyfile.h)
    AC_CHECK_FUNCS(copyfile)
    if test $tcl_corefoundation = yes; then
	AC_CHECK_HEADERS(libkern/OSAtomic.h)
	AC_CHECK_FUNCS(OSSpinLockLock)
    fi
    AC_DEFINE(TCL_LOAD_FROM_MEMORY, 1,
	[Can this platform load code from memory?])
    AC_DEFINE(TCL_WIDE_CLICKS, 1,
	[Does this platform have wide high-resolution clicks?])
	AC_CACHE_CHECK([if weak import is available], tcl_cv_cc_weak_import, [
	hold_cflags=$CFLAGS; CFLAGS="$CFLAGS -Werror"
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[
		int rand(void) __attribute__((weak_import));
	    ]], [[rand();]])],
	    [tcl_cv_cc_weak_import=yes],[tcl_cv_cc_weak_import=no])
	CFLAGS=$hold_cflags])
    if test $tcl_cv_cc_weak_import = yes; then
	AC_DEFINE(HAVE_WEAK_IMPORT, 1, [Is weak import available?])
    fi
    AC_CACHE_CHECK([if Darwin SUSv3 extensions are available],
	tcl_cv_cc_darwin_c_source, [
	hold_cflags=$CFLAGS; CFLAGS="$CFLAGS -Werror"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
		#define _DARWIN_C_SOURCE 1
		#include <sys/cdefs.h>
	    ]], [[]])],[tcl_cv_cc_darwin_c_source=yes],[tcl_cv_cc_darwin_c_source=no])
	CFLAGS=$hold_cflags])
    if test $tcl_cv_cc_darwin_c_source = yes; then
	AC_DEFINE(_DARWIN_C_SOURCE, 1,
		[Are Darwin SUSv3 extensions available?])
    fi
    # Build .bundle dltest binaries in addition to .dylib
    DLTEST_LD='${CC} -bundle -Wl,-w ${CFLAGS} ${LDFLAGS}'
    DLTEST_SUFFIX=".bundle"
else
    DLTEST_LD='${SHLIB_LD}'
    DLTEST_SUFFIX=""
fi

#--------------------------------------------------------------------
#	Check for support of fts functions (readdir replacement)
#--------------------------------------------------------------------

AC_CACHE_CHECK([for fts], tcl_cv_api_fts, [
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
	    #include <sys/param.h>
	    #include <sys/stat.h>
	    #include <fts.h>
	]], [[
	    char*const p[2] = {"/", NULL};
	    FTS *f = fts_open(p, FTS_PHYSICAL|FTS_NOCHDIR|FTS_NOSTAT, NULL);
	    FTSENT *e = fts_read(f); fts_close(f);
	]])],[tcl_cv_api_fts=yes],[tcl_cv_api_fts=no])])
if test $tcl_cv_api_fts = yes; then
    AC_DEFINE(HAVE_FTS, 1, [Do we have fts functions?])
fi

#--------------------------------------------------------------------
#	The statements below check for systems where POSIX-style non-blocking
#	I/O (O_NONBLOCK) doesn't work or is unimplemented. On these systems
#	(mostly older ones), use the old BSD-style FIONBIO approach instead.
#--------------------------------------------------------------------

SC_BLOCKING_STYLE

#------------------------------------------------------------------------

AC_MSG_CHECKING([whether to use dll unloading])
AC_ARG_ENABLE(dll-unloading,
    AS_HELP_STRING([--enable-dll-unloading],
	[enable the 'unload' command (default: on)]),
    [tcl_ok=$enableval], [tcl_ok=yes])
if test $tcl_ok = yes; then
    AC_DEFINE(TCL_UNLOAD_DLLS, 1, [Do we allow unloading of shared libraries?])
fi
AC_MSG_RESULT([$tcl_ok])

#------------------------------------------------------------------------
#	Check whether the timezone data is supplied by the OS or has
#	to be installed by Tcl. The default is autodetection, but can
#	be overridden on the configure command line either way.
#------------------------------------------------------------------------

AC_MSG_CHECKING([for timezone data])
AC_ARG_WITH(tzdata,
    AS_HELP_STRING([--with-tzdata],
	[install timezone data (default: autodetect)]),
    [tcl_ok=$withval], [tcl_ok=auto])
#
# Any directories that get added here must also be added to the
# search path in ::tcl::clock::Initialize (library/clock.tcl).
#
case $tcl_ok in
    no)
	AC_MSG_RESULT([supplied by OS vendor])
    ;;
    yes)
	# nothing to do here
    ;;
    auto*)
	AC_CACHE_VAL([tcl_cv_dir_zoneinfo], [
	for dir in /usr/share/zoneinfo \
		/usr/share/lib/zoneinfo \
		/usr/lib/zoneinfo
	do
		if test -f $dir/UTC -o -f $dir/GMT
		then
			tcl_cv_dir_zoneinfo="$dir"
			break
		fi
	done])
	if test -n "$tcl_cv_dir_zoneinfo"; then
	    tcl_ok=no
	    AC_MSG_RESULT([$dir])
	else
	    tcl_ok=yes
	fi
    ;;
    *)
	AC_MSG_ERROR([invalid argument: $tcl_ok])
    ;;
esac
if test $tcl_ok = yes
then
    AC_MSG_RESULT([supplied by Tcl])
    INSTALL_TZDATA=install-tzdata
fi

#--------------------------------------------------------------------
#	DTrace support
#--------------------------------------------------------------------

AC_ARG_ENABLE(dtrace,
    AS_HELP_STRING([--enable-dtrace],
	[build with DTrace support (default: off)]),
    [tcl_ok=$enableval], [tcl_ok=no])
if test $tcl_ok = yes; then
    AC_CHECK_HEADER(sys/sdt.h, [tcl_ok=yes], [tcl_ok=no])
fi
if test $tcl_ok = yes; then
    AC_PATH_PROG(DTRACE, dtrace,, [$PATH:/usr/sbin])
    test -z "$ac_cv_path_DTRACE" && tcl_ok=no
fi
AC_MSG_CHECKING([whether to enable DTrace support])
MAKEFILE_SHELL='/bin/sh'
if test $tcl_ok = yes; then
    AC_DEFINE(USE_DTRACE, 1, [Are we building with DTrace support?])
    DTRACE_SRC="\${DTRACE_SRC}"
    DTRACE_HDR="\${DTRACE_HDR}"
    if test "`uname -s`" != "Darwin" ; then
	DTRACE_OBJ="\${DTRACE_OBJ}"
	if test "`uname -s`" = "SunOS" -a "$SHARED_BUILD" = "0" ; then
	    # Need to create an intermediate object file to ensure tclDTrace.o
	    # gets included when linking against the static tcl library.
	    STLIB_LD='stlib_ld () { /usr/ccs/bin/ld -r -o $${1%.a}.o "$${@:2}" && '"${STLIB_LD}"' $${1} $${1%.a}.o ; } && stlib_ld'
	    MAKEFILE_SHELL='/bin/bash'
	    # Force use of Sun ar and ranlib, the GNU versions choke on
	    # tclDTrace.o and the combined object file above.
	    AR='/usr/ccs/bin/ar'
	    RANLIB='/usr/ccs/bin/ranlib'
	fi
    fi
fi
AC_MSG_RESULT([$tcl_ok])

#--------------------------------------------------------------------
# The check below checks whether the cpuid instruction is usable.
#--------------------------------------------------------------------

AC_CACHE_CHECK([whether the cpuid instruction is usable], tcl_cv_cpuid, [
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[
	int index,regsPtr[4];
    __asm__ __volatile__("mov %%ebx, %%edi     \n\t"
		 "cpuid            \n\t"
		 "mov %%ebx, %%esi   \n\t"
		 "mov %%edi, %%ebx  \n\t"
		 : "=a"(regsPtr[0]), "=S"(regsPtr[1]), "=c"(regsPtr[2]), "=d"(regsPtr[3])
		 : "a"(index) : "edi");
    ]])],[tcl_cv_cpuid=yes],[tcl_cv_cpuid=no])])
if test $tcl_cv_cpuid = yes; then
    AC_DEFINE(HAVE_CPUID, 1, [Is the cpuid instruction usable?])
fi

#--------------------------------------------------------------------
#	The statements below define a collection of symbols related to
#	building libtcl as a shared library instead of a static library.
#--------------------------------------------------------------------

TCL_UNSHARED_LIB_SUFFIX=${UNSHARED_LIB_SUFFIX}
TCL_SHARED_LIB_SUFFIX=${SHARED_LIB_SUFFIX}
if test "$ac_cv_cygwin" = "yes" -a "$SHARED_BUILD" != "0"; then
eval "TCL_LIB_FILE=cygtcl${LIB_SUFFIX}"
EXTRA_INSTALL_BINARIES='$(INSTALL_LIBRARY) $(patsubst cyg%.dll,lib%.dll.a,${LIB_FILE}) "$(LIB_INSTALL_DIR)"'
else
eval "TCL_LIB_FILE=libtcl${LIB_SUFFIX}"
fi

# tclConfig.sh needs a version of the _LIB_SUFFIX that has been eval'ed
# since on some platforms TCL_LIB_FILE contains shell escapes.
# (See also: TCL_TRIM_DOTS).

eval "TCL_LIB_FILE=${TCL_LIB_FILE}"

test -z "$TCL_LIBRARY" && TCL_LIBRARY='$(prefix)/lib/tcl$(VERSION)'
PRIVATE_INCLUDE_DIR='$(includedir)'
HTML_DIR='$(DISTDIR)/html'

# Note:  in the following variable, it's important to use the absolute
# path name of the Tcl directory rather than "..":  this is because
# AIX remembers this path and will attempt to use it at run-time to look
# up the Tcl library.

if test "`uname -s`" = "Darwin" ; then
    SC_ENABLE_FRAMEWORK
    TCL_SHLIB_LD_EXTRAS="-compatibility_version ${TCL_VERSION} -current_version ${TCL_VERSION}`echo ${TCL_PATCH_LEVEL} | awk ['{match($0, "\\\.[0-9]+"); print substr($0,RSTART,RLENGTH)}']`"
    TCL_SHLIB_LD_EXTRAS="${TCL_SHLIB_LD_EXTRAS}"' -install_name "${DYLIB_INSTALL_DIR}"/${TCL_LIB_FILE}'
    echo "$LDFLAGS " | grep -q -- '-prebind ' && TCL_SHLIB_LD_EXTRAS="${TCL_SHLIB_LD_EXTRAS}"' -seg1addr 0xA000000'
    TCL_SHLIB_LD_EXTRAS="${TCL_SHLIB_LD_EXTRAS}"' -sectcreate __TEXT __info_plist Tcl-Info.plist'
    EXTRA_TCLSH_LIBS='-sectcreate __TEXT __info_plist Tclsh-Info.plist'
    AC_CONFIG_FILES([Tcl-Info.plist:../macosx/Tcl-Info.plist.in Tclsh-Info.plist:../macosx/Tclsh-Info.plist.in])
    TCL_YEAR="`date +%Y`"
fi

if test "$FRAMEWORK_BUILD" = "1" ; then
    AC_DEFINE(TCL_FRAMEWORK, 1, [Is Tcl built as a framework?])
    # Construct a fake local framework structure to make linking with
    # '-framework Tcl' and running of tcltest work
    AC_CONFIG_COMMANDS([Tcl.framework], [n=Tcl &&
	f=$n.framework && v=Versions/$VERSION &&
	rm -rf $f && mkdir -p $f/$v/Resources &&
	ln -s $v/$n $v/Resources $f && ln -s ../../../$n $f/$v &&
	ln -s ../../../../$n-Info.plist $f/$v/Resources/Info.plist &&
	unset n f v
    ], VERSION=${TCL_VERSION})
    LD_LIBRARY_PATH_VAR="DYLD_FRAMEWORK_PATH"
    # default install directory for bundled packages
    if test "${libdir}" = '${exec_prefix}/lib' -o "`basename ${libdir}`" = 'Frameworks'; then
	PACKAGE_DIR="/Library/Tcl"
    else
	PACKAGE_DIR="$libdir"
    fi
    if test "${libdir}" = '${exec_prefix}/lib'; then
	# override libdir default
	libdir="/Library/Frameworks"
    fi
    TCL_LIB_FILE="Tcl"
    TCL_LIB_FLAG="-framework Tcl"
    TCL_BUILD_LIB_SPEC="-F`pwd | sed -e 's/ /\\\\ /g'` -framework Tcl"
    TCL_LIB_SPEC="-F${libdir} -framework Tcl"
    libdir="${libdir}/Tcl.framework/Versions/\${VERSION}"
    TCL_LIBRARY="${libdir}/Resources/Scripts"
    includedir="${libdir}/Headers"
    PRIVATE_INCLUDE_DIR="${libdir}/PrivateHeaders"
    HTML_DIR="${libdir}/Resources/Documentation/Reference/Tcl"
    EXTRA_INSTALL="install-private-headers html-tcl"
    EXTRA_BUILD_HTML='@ln -fs contents.htm "$(HTML_INSTALL_DIR)/TclTOC.html"'
    EXTRA_INSTALL_BINARIES='@echo "Installing Info.plist to $(LIB_INSTALL_DIR)/Resources/" && $(INSTALL_DATA_DIR) "$(LIB_INSTALL_DIR)/Resources" && $(INSTALL_DATA) Tcl-Info.plist "$(LIB_INSTALL_DIR)/Resources/Info.plist"'
    EXTRA_INSTALL_BINARIES="$EXTRA_INSTALL_BINARIES"' && echo "Installing license.terms to $(LIB_INSTALL_DIR)/Resources/" && $(INSTALL_DATA) "$(TOP_DIR)/license.terms" "$(LIB_INSTALL_DIR)/Resources"'
    EXTRA_INSTALL_BINARIES="$EXTRA_INSTALL_BINARIES"' && echo "Finalizing Tcl.framework" && rm -f "$(LIB_INSTALL_DIR)/../Current" && ln -s "$(VERSION)" "$(LIB_INSTALL_DIR)/../Current" && for f in "$(LIB_FILE)" tclConfig.sh Resources Headers PrivateHeaders; do rm -f "$(LIB_INSTALL_DIR)/../../$$f" && ln -s "Versions/Current/$$f" "$(LIB_INSTALL_DIR)/../.."; done && f="$(STUB_LIB_FILE)" && rm -f "$(LIB_INSTALL_DIR)/../../$$f" && ln -s "Versions/$(VERSION)/$$f" "$(LIB_INSTALL_DIR)/../.."'
    # Don't use AC_DEFINE for the following as the framework version define
    # needs to go into the Makefile even when using autoheader, so that we
    # can pick up a potential make override of VERSION. Also, don't put this
    # into CFLAGS as it should not go into tclConfig.sh
    EXTRA_CC_SWITCHES='-DTCL_FRAMEWORK_VERSION=\"$(VERSION)\"'
else
    # libdir must be a fully qualified path and not ${exec_prefix}/lib
    eval libdir="$libdir"
    # default install directory for bundled packages
    PACKAGE_DIR="$libdir"
    if test "${TCL_LIB_VERSIONS_OK}" = "ok"; then
	TCL_LIB_FLAG="-ltcl${TCL_VERSION}"
    else
	TCL_LIB_FLAG="-ltcl`echo ${TCL_VERSION} | tr -d .`"
    fi
    TCL_BUILD_LIB_SPEC="-L`pwd | sed -e 's/ /\\\\ /g'` ${TCL_LIB_FLAG}"
    TCL_LIB_SPEC="-L${libdir} ${TCL_LIB_FLAG}"
fi
VERSION='${VERSION}'
eval "CFG_TCL_SHARED_LIB_SUFFIX=${TCL_SHARED_LIB_SUFFIX}"
eval "CFG_TCL_UNSHARED_LIB_SUFFIX=${TCL_UNSHARED_LIB_SUFFIX}"
VERSION=${TCL_VERSION}

#--------------------------------------------------------------------
#	Zipfs support - Tip 430
#--------------------------------------------------------------------
AC_ARG_ENABLE(zipfs,
    AS_HELP_STRING([--enable-zipfs],
	[build with Zipfs support (default: on)]),
    [tcl_ok=$enableval], [tcl_ok=yes])
if test "$tcl_ok" = "yes" -a "x$enable_framework" != "xyes"; then
    #
    # Find a native compiler
    #
    AX_CC_FOR_BUILD
    #
    # Find a native zip implementation
    #
    SC_ZIPFS_SUPPORT
    ZIPFS_BUILD=1
    TCL_ZIP_FILE=libtcl${TCL_MAJOR_VERSION}.${TCL_MINOR_VERSION}${TCL_PATCH_LEVEL}.zip
else
    ZIPFS_BUILD=0
    TCL_ZIP_FILE=
fi
# Do checking message here to not mess up interleaved configure output
AC_MSG_CHECKING([for building with zipfs])
if test "${ZIPFS_BUILD}" = 1; then
    if test "${SHARED_BUILD}" = 0; then
       ZIPFS_BUILD=2;
       AC_DEFINE(ZIPFS_BUILD, 2, [Are we building with zipfs enabled?])
     else
       AC_DEFINE(ZIPFS_BUILD, 1, [Are we building with zipfs enabled?])\
    fi
    AC_MSG_RESULT([yes])
else
AC_MSG_RESULT([no])
INSTALL_LIBRARIES=install-libraries
INSTALL_MSGS=install-msgs
fi

# Point to tcl script library if we are not embedding it.
if test "${ZIPFS_BUILD}" = 0; then
TCL_BUILDTIME_LIBRARY=${TCL_SRC_DIR}/library
fi

AC_SUBST(ZIPFS_BUILD)
AC_SUBST(TCL_ZIP_FILE)
AC_SUBST(INSTALL_LIBRARIES)
AC_SUBST(INSTALL_MSGS)
AC_SUBST(TCL_BUILDTIME_LIBRARY)

#--------------------------------------------------------------------
#	The statements below define the symbol TCL_PACKAGE_PATH, which
#	gives a list of directories that may contain packages.  The list
#	consists of one directory for machine-dependent binaries and
#	another for platform-independent scripts.
#--------------------------------------------------------------------

if test "$FRAMEWORK_BUILD" = "1" ; then
    test -z "$TCL_PACKAGE_PATH" && \
	TCL_PACKAGE_PATH="~/Library/Tcl:/Library/Tcl:~/Library/Frameworks:/Library/Frameworks"
    test -z "$TCL_MODULE_PATH"  && \
	TCL_MODULE_PATH="~/Library/Tcl:/Library/Tcl"
elif test "$prefix/lib" != "$libdir"; then
    test -z "$TCL_PACKAGE_PATH" && TCL_PACKAGE_PATH="${libdir}:${prefix}/lib"
else
    test -z "$TCL_PACKAGE_PATH" && TCL_PACKAGE_PATH="${prefix}/lib"
fi

#--------------------------------------------------------------------
#       The statements below define various symbols relating to Tcl
#       stub support.
#--------------------------------------------------------------------

# Replace ${VERSION} with contents of ${TCL_VERSION}
# double-eval to account for TCL_TRIM_DOTS.
#
eval "TCL_STUB_LIB_FILE=libtclstub.a"
eval "TCL_STUB_LIB_FILE=\"${TCL_STUB_LIB_FILE}\""
eval "TCL_STUB_LIB_DIR=\"${libdir}\""

TCL_STUB_LIB_FLAG="-ltclstub"

TCL_BUILD_STUB_LIB_SPEC="-L`pwd | sed -e 's/ /\\\\ /g'` ${TCL_STUB_LIB_FLAG}"
TCL_STUB_LIB_SPEC="-L${TCL_STUB_LIB_DIR} ${TCL_STUB_LIB_FLAG}"
TCL_BUILD_STUB_LIB_PATH="`pwd`/${TCL_STUB_LIB_FILE}"
TCL_STUB_LIB_PATH="${TCL_STUB_LIB_DIR}/${TCL_STUB_LIB_FILE}"

# Install time header dir can be set via --includedir
eval "TCL_INCLUDE_SPEC=\"-I${includedir}\""

#------------------------------------------------------------------------
# tclConfig.sh refers to this by a different name
#------------------------------------------------------------------------

TCL_SHARED_BUILD=${SHARED_BUILD}

AC_SUBST(TCL_VERSION)
AC_SUBST(TCL_MAJOR_VERSION)
AC_SUBST(TCL_MINOR_VERSION)
AC_SUBST(TCL_PATCH_LEVEL)
AC_SUBST(TCL_YEAR)
AC_SUBST(PKG_CFG_ARGS)

AC_SUBST(TCL_ZIP_FILE)
AC_SUBST(TCL_LIB_FILE)
AC_SUBST(TCL_LIB_FLAG)
AC_SUBST(TCL_LIB_SPEC)
AC_SUBST(TCL_STUB_LIB_FILE)
AC_SUBST(TCL_STUB_LIB_FLAG)
AC_SUBST(TCL_STUB_LIB_SPEC)
AC_SUBST(TCL_STUB_LIB_PATH)
AC_SUBST(TCL_INCLUDE_SPEC)
AC_SUBST(TCL_BUILD_STUB_LIB_SPEC)
AC_SUBST(TCL_BUILD_STUB_LIB_PATH)

AC_SUBST(TCL_SRC_DIR)
AC_SUBST(CFG_TCL_SHARED_LIB_SUFFIX)
AC_SUBST(CFG_TCL_UNSHARED_LIB_SUFFIX)

AC_SUBST(TCL_SHARED_BUILD)
AC_SUBST(LD_LIBRARY_PATH_VAR)

AC_SUBST(TCL_BUILD_LIB_SPEC)

AC_SUBST(TCL_LIB_VERSIONS_OK)
AC_SUBST(TCL_SHARED_LIB_SUFFIX)
AC_SUBST(TCL_UNSHARED_LIB_SUFFIX)

AC_SUBST(TCL_HAS_LONGLONG)

AC_SUBST(INSTALL_TZDATA)

AC_SUBST(DTRACE_SRC)
AC_SUBST(DTRACE_HDR)
AC_SUBST(DTRACE_OBJ)
AC_SUBST(MAKEFILE_SHELL)

AC_SUBST(BUILD_DLTEST)
AC_SUBST(TCL_PACKAGE_PATH)
AC_SUBST(TCL_MODULE_PATH)

AC_SUBST(TCL_LIBRARY)
AC_SUBST(PRIVATE_INCLUDE_DIR)
AC_SUBST(HTML_DIR)
AC_SUBST(PACKAGE_DIR)

AC_SUBST(EXTRA_CC_SWITCHES)
AC_SUBST(EXTRA_APP_CC_SWITCHES)
AC_SUBST(EXTRA_INSTALL)
AC_SUBST(EXTRA_INSTALL_BINARIES)
AC_SUBST(EXTRA_BUILD_HTML)
AC_SUBST(EXTRA_TCLSH_LIBS)

AC_SUBST(DLTEST_LD)
AC_SUBST(DLTEST_SUFFIX)

dnl	Disable the automake-friendly normalization of LIBOBJS
dnl	performed by autoconf 2.53 and later.  It's not correct for us.
define([_AC_LIBOBJS_NORMALIZE],[])
AC_CONFIG_FILES([
    Makefile:../unix/Makefile.in
    dltest/Makefile:../unix/dltest/Makefile.in
    tclConfig.sh:../unix/tclConfig.sh.in
    tcl.pc:../unix/tcl.pc.in
])
AC_OUTPUT

dnl Local Variables:
dnl mode: autoconf
dnl End:
