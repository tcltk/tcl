# This file provides environment initialization and runtime library
# support for the L language.  It is loaded automatically by init.tcl.
#
# This stuff should probably be in its own namespace or only turned on when
# processing L source.  It breaks tcl scripts.
#
# Copyright (c) 2007-2009 BitMover, Inc.

if {[info exists ::L_libl_initted]} { return }
set ::L_libl_initted 1
set ::L_putenv_bug -1

set ::%%suppress_calling_main 0

proc %%call_main_if_defined {} {
	if {$::tcl_interactive} { return }
	if {[llength [info proc main]] && !${::%%suppress_calling_main}} {
		incr ::argc
		if {![info exists ::argv]}  { set ::argv {} }
		if {![info exists ::argv0]} { set ::argv0 "L" }
		set ::argv [linsert $::argv 0 $::argv0]
		switch [llength [info args main]] {
		    0 {
			set ::%%suppress_calling_main 1
			set ret [main]
		    }
		    1 {
			set ::%%suppress_calling_main 1
			set ret [main $::argv]
		    }
		    2 {
			set ::%%suppress_calling_main 1
			set ret [main $::argc $::argv]
		    }
		    3 {
			set ::%%suppress_calling_main 1
			set ret [main $::argc $::argv [dict create {*}[array get ::env]]]
		    }
		    default {
			error "Too many parameters for main()."
			set ret 1
		    }
		}
		if {$ret == ""} { set ret 0 }
		if {$ret != 0}  { exit $ret }
	}
}

# Warn if any of the functions in the %%L_fnsCalled hash are not defined.
proc %%check_L_fns {} {
	foreach f [dict keys ${::%%L_fnsCalled}] {
		if {![llength [info commands $f]] && ![llength [info procs $f]]} {
			puts stderr "L Warning: function $f not defined"
		}
	}
}

# This loads the Lver() command created by the build.
if {[file exists [file join $::tcl_library Lver.tcl]]} {
	source [file join $::tcl_library Lver.tcl]
	package provide L [Lver]
}

#lang L
#pragma fntrace=off
/*
 * Types for compatibility with older versions of the compiler.
 * The tcl typedef lets the tcl cast work now that it's not
 * hard-coded.
 */
typedef	poly	hash{poly};
typedef	poly	tcl;

typedef	string	FILE;
typedef	string	FMT;
typedef void	&fnhook_t(int pre, int ac, poly av[], poly ret);

struct	stat {
	int	st_dev;
	int	st_ino;
	int	st_mode;
	int	st_nlink;
	int	st_uid;
	int	st_gid;
	int	st_size;
	int	st_atime;
	int	st_mtime;
	int	st_ctime;
	string	st_type;
};

typedef struct {
	string	argv[];	// args passed in
	string	path;	// if defined, this is the path to the exe
			// if not defined, the executable was not found
	int	exit;	// if defined, the process exited with this val
	int	signal;	// if defined, the signal that killed the process
	string	error;	// if defined, an error message or output from stderr
} STATUS;

typedef struct dirent {
	string	name;
	string	type;
	int	hidden;
} dirent;

FILE    stdin  = "stdin";
FILE    stderr = "stderr";
FILE    stdout = "stdout";
string	stdio_lasterr;
STATUS	stdio_status;

extern	string	::argv[];
extern	int	::argc;
extern	string	errorCode[];

int	optind = 0;
string	optarg, optopt;

extern dirent[]	getdirx(string path);
extern string	getopt(string av[], string opts, string lopts[]);
extern void	getoptReset(void);

/* These are pre-defined identifiers set by the compiler. */
extern int	SYSTEM_ARGV__;
extern int	SYSTEM_IN_STRING__;
extern int	SYSTEM_IN_ARRAY__;
extern int	SYSTEM_IN_FILENAME__;
extern int	SYSTEM_IN_HANDLE__;
extern int	SYSTEM_OUT_STRING__;
extern int	SYSTEM_OUT_ARRAY__;
extern int	SYSTEM_OUT_FILENAME__;
extern int	SYSTEM_OUT_HANDLE__;
extern int	SYSTEM_ERR_STRING__;
extern int	SYSTEM_ERR_ARRAY__;
extern int	SYSTEM_ERR_FILENAME__;
extern int	SYSTEM_ERR_HANDLE__;
extern int	SYSTEM_BACKGROUND__;

/* Used internally by popen() and pclose() for stderr callbacks. */
typedef struct {
	FILE	pipe;
	string	cmd;
	string	cb;
} stderr_ctxt_t;

private stderr_ctxt_t	callbacks{FILE};
private int		signame_to_num(string signame);

string
basename(string path)
{
	return (File_tail(path));
}

string
caller(int stacks)
{
	string	err;

	try {
		return (uplevel(1, "info level -${stacks}"));
	} catch (&err) {
		stdio_lasterr = err;
		return (undef);
	}
}

int
chdir(string dir)
{
	string	err;

	try {
		cd(dir);
		return (0);
	} catch (&err) {
		stdio_lasterr = err;
		return (-1);
	}
}

int
chmod(string path, string permissions)
{
	string	err;

	try {
		File_attributes(path, permissions: "0${permissions}");
		return (0);
	} catch (&err) {
		stdio_lasterr = err;
		return (-1);
	}
}

int
chown(string owner, string group, string path)
{
	string	opts[] = {}, res;

	if ((owner == "") && (group == "")) return (0);
	unless (owner == "") {
		push(&opts, {"-owner", owner});
	}
	unless (group == "") {
		push(&opts, {"-group", group});
	}
	try {
		File_attributes(path, (expand)opts);
		return (0);
	} catch (&res) {
		stdio_lasterr = res;
		return (-1);
	}
}

int
cpus(void)
{
	FILE	f = fopen("/proc/cpuinfo", "r");
	int	n = 0;
	string	buf;

	unless (f) return (1);
	while (buf = <f>) {
		if (buf =~ /^processor\s/) n++;
	}
	fclose(f);
	return (n);
}

void
die_(string func, int line, FMT fmt, ...args)
{
	warn_(func, line, fmt, (expand)args);
	exit(1);
}

string
dirname(string path)
{
	return (File_dirname(path));
}

int
exists(string path)
{
	return (File_exists(path));
}

int
fclose(_mustbetype FILE f)
{
	string	err;

	unless (f) return (-1);

	try {
		close(f);
		return (0);
	} catch (&err) {
		stdio_lasterr = err;
		return (-1);
	}
}

FILE
fopen(string path, string mode)
{
	int	v = 0;
	FILE	f;
	string	err;

	unless (path) {
		warn("fopen: pathname is not defined");
		return (undef);
	}
	unless (mode) {
		warn("fopen: mode is not defined");
		return (undef);
	}

	/* new mode, v, means be verbose w/ errors */
	if (mode =~ /v/) {
		mode =~ s/v//g;
		v = 1;
	}
	try {
		f = open(path, mode);
		return (f);
	} catch (&err) {
		stdio_lasterr = err;
		if (v) fprintf(stderr, "fopen(%s, %s) = %s\n", path, mode, err);
		return (undef);
	}
}

int
Fprintf(string fname, FMT fmt, ...args)
{
	int	ret;
	FILE	f;

	unless (f = fopen(fname, "w")) return (-1);
	ret = fprintf(f, fmt, (expand)args);
	fclose(f);
	return (ret);
}

int
fprintf(_mustbetype FILE f, FMT fmt, ...args)
{
	string	err;

	try {
		puts(nonewline: f, format(fmt, (expand)args));
		return (0);
	} catch (&err) {
		stdio_lasterr = err;
		return (-1);
	}
}

int
frename_(string oldPath, string newPath)
{
	string	err;

	try {
		File_rename(oldPath, newPath);
		return (0);
	} catch (&err) {
		stdio_lasterr = err;
		return (-1);
	}
}

string
ftype(string path)
{
	string	err;

	try {
		return (File_type(path));
	} catch (&err) {
		stdio_lasterr = err;
		return (undef);
	}
}

string[]
getdir(string dir, ...args)
{
	int	i;
	string	pattern, ret[];

	switch (length(args)) {
	    case 0:
		pattern = "*";
		break;
	    case 1:
		pattern = args[0];
		break;
	    default:
		return (undef);
	}
	ret = lsort(glob(nocomplain:, directory: dir, pattern));

	// Strip any leading ./
	for (i = 0; i < length(ret); ++i) {
		ret[i] =~ s|^\./||;
	}
	return (ret);
}

string
getenv(string varname)
{
	string	val;

	try {
		val = Array_get("::env", varname){varname};
		return (length(val) ? val : undef);
	} catch {
		return (undef);
	}
}

int
getpid()
{
	return ((int)(pid()[END]));
}

void
here_(string file, int line, string func)
{
	puts(stderr, "${func}() in ${basename(file)}:${line}");
}

int
isdir(string path)
{
	return (File_isdirectory(path));
}

int
isreg(string path)
{
	return (File_isfile(path));
}

int
islink(string path)
{
	return (ftype(path) == "link");
}

int
isspace(string buf)
{
	return (String_isSpace(strict:, buf));
}

string
lc(string s)
{
	return (String_tolower(s));
}

int
isalpha(string buf)
{
	return (String_isAlpha(strict:, buf));
}

int
isalnum(string buf)
{
	return (String_isAlnum(strict:, buf));
}

int
islower(string buf)
{
	return (String_isLower(strict:, buf));
}

int
isupper(string buf)
{
	return (String_isUpper(strict:, buf));
}

int
isdigit(string buf)
{
	return (String_isDigit(strict:, buf));
}

int
iswordchar(string buf)
{
	return (String_isWordchar(strict:, buf));
}

int
link(string sourcePath, string targetPath)
{
	string	err;

	try {
		File_link(hard: targetPath, sourcePath);
		return (0);
	} catch (&err) {
		stdio_lasterr = err;
		return (-1);
	}
}

int
lstat(string path, struct stat &buf)
{
	string	err, st_hash{string};

	try {
		File_lstat(path, "ret");
		st_hash = Array_get("ret");
		buf->st_dev   = (int)st_hash{"dev"};
		buf->st_ino   = (int)st_hash{"ino"};
		buf->st_mode  = (int)st_hash{"mode"};
		buf->st_nlink = (int)st_hash{"nlink"};
		buf->st_uid   = (int)st_hash{"uid"};
		buf->st_gid   = (int)st_hash{"gid"};
		buf->st_size  = (int)st_hash{"size"};
		buf->st_atime = (int)st_hash{"atime"};
		buf->st_mtime = (int)st_hash{"mtime"};
		buf->st_ctime = (int)st_hash{"ctime"};
		buf->st_type  = st_hash{"type"};
		return (0);
	} catch (&err) {
		stdio_lasterr = err;
		return (-1);
	}
}

private int tm_start = Clock_clicks(milliseconds:);

int
milli()
{
	return (Clock_clicks(milliseconds:) - tm_start);
}

void
milli_reset()
{
	tm_start = Clock_clicks(milliseconds:);
}

int
mkdir(string path)
{
	string	err;

	try {
		File_mkdir(path);
		return (0);
	} catch (&err) {
		stdio_lasterr = err;
		return (-1);
	}
}

int
mtime(string path)
{
	try {
		return (File_mtime(path));
	} catch {
		return (0);
	}
}

string
normalize(string path)
{
	return (File_normalize(path));
}

int
ord(string c)
{
	int	n = -1;

	if (length(c)) scan(c[0], "%c", &n);
	return (n);
}

int
pclose(_mustbetype FILE f, _optional STATUS &status_ref)
{
	string	err;
	STATUS	status;

	// Put the pipe in blocking mode so that Tcl knows to throw
	// an error if the program exited with exit_code != 0.
	fconfigure(f, blocking: 1);

	status.exit = 0;
	try {
		close(f);
	} catch (&err) {
		status.error = stdio_lasterr = err;
		switch (errorCode[0]) {
		    case "CHILDSTATUS":
			status.exit = (int)errorCode[2];
			break;
		    case "CHILDKILLED":
			status.signal = signame_to_num(errorCode[2]);
			break;
		}
	}

	// Call the user's callback.
	if (callbacks{f}) {
		stderr_cb_(callbacks{f});
		undef(callbacks{f});
	}

	stdio_status = status;
	if (defined(&status_ref)) status_ref = status;
	return ((status.exit == 0) ? 0 : -1);
}

string
platform()
{
	string	p;

	eval("set p $::tcl_platform(platform)");
	return (p);
}

private void
stderr_cb_(stderr_ctxt_t ctxt)
{
	if (Chan_names(ctxt.pipe) == "") return;  // if closed
	eval({ctxt.cb, ctxt.cmd, ctxt.pipe});
	try {
		if (eof(ctxt.pipe)) ::close(ctxt.pipe);
	} catch {}
}

private void
stderr_gui_cb_(_argused string cmd, FILE fd)
{
	string	data;
	widget	top = ".__stderr";
	widget	f = top . ".f";
	widget	t = f . ".t";
	widget	vs = f . ".vs";
	widget	hs = f . ".hs";

	unless (read(fd, &data)) return;

	unless (Winfo_exists((string)top)) {
		tk_make_readonly_tag_();

		toplevel(top);
		Wm_title((string)top, "Error Output");
		Wm_protocol((string)top, "WM_DELETE_WINDOW",
		    "wm withdraw ${top}");
		ttk::frame(f);
		text(t, wrap: "none", highlightthickness: 0, insertwidth: 0,
		    xscrollcommand: "${hs} set",
		    yscrollcommand: "${vs} set");
		bindtags(t, {t, "ReadonlyText", "all"});
		ttk::scrollbar(vs, orient: "vertical", command: "${t} yview");
		ttk::scrollbar(hs, orient: "horizontal", command: "${t} xview");
		grid(t,  row: 0, column: 0, sticky: "nesw");
		grid(vs, row: 0, column: 1, sticky: "ns");
		grid(hs, row: 1, column: 0, sticky: "ew");
		Grid_rowconfigure((string)f, t, weight: 1);
		Grid_columnconfigure((string)f, t, weight: 1);

		ttk::frame("${top}.buttons");
		    ttk::button("${top}.buttons.close",
			text: "Close", command: "wm withdraw ${top}");
		    pack("${top}.buttons.close", side: "right", padx: "5 15");
		    ttk::button("${top}.buttons.save",
			text: "Save to Log", command: "tk_save_to_log_ ${t}");
		    pack("${top}.buttons.save", side: "right");

		grid("${top}.buttons", row: 1, column: 0, sticky: "esw");

		grid(f,  row: 0, column: 0, sticky: "nesw");
		Grid_rowconfigure((string)top, f, weight: 1);
		Grid_columnconfigure((string)top, f, weight: 1);
	}

	unless(Winfo_viewable((string)top)) {
		Wm_deiconify((string)top);
	}

	/* Make sure the error is not obscured by other windows. */
	After_idle("raise ${top}");
	Text_insertEnd(t, "cmd: ${cmd}\n" . data);
	Update_idletasks();
}

FILE
popen_(poly cmd, string mode, void &stderr_cb(string cmd, FILE f), int flags)
{
	int		v = 0;
	int		redir;
	FILE		f, rdPipe, wrPipe;
	string		arg, argv[], err;
	stderr_ctxt_t	ctxt;

	if (mode =~ /v/) {
		mode =~ s/v//g;
		v = 1;
	}

	if (flags & SYSTEM_ARGV__) {
		argv = (string[])cmd;
		cmd = join(" ", argv);
	} else {
		try {
			argv = shsplit(cmd);
		} catch (&err) {
			stdio_lasterr = err;
			return (undef);
		}
	}

	/*
	 * Re-direct stderr to this process' stderr unless the caller
	 * redirected it inside their command or specified a callback.
	 */
	redir = 0;
	foreach (arg in argv) {
		if (arg =~ /^2>/) {
			redir = 1;
			break;
		}
	}

	unless (redir) {
		/* Caller did not redirect stderr */
		unless (flags & SYSTEM_OUT_HANDLE__) {
			/* or give us a callback */
			if (tk_loaded_()) {
				stderr_cb = &stderr_gui_cb_;
			} else {
				push(&argv, "2>@stderr");
			}
		}
		if (stderr_cb) {
			try {
				{rdPipe,wrPipe} = Chan_pipe();
			} catch (&err) {
				stdio_lasterr = err;
				return (undef);
			}
			fconfigure(rdPipe, blocking: "off", buffering: "line");
			push(&argv, "2>@${wrPipe}");
			ctxt.pipe = rdPipe;
			ctxt.cmd  = cmd;
			ctxt.cb   = (string)stderr_cb;
			fileevent(rdPipe, "readable", {&stderr_cb_, ctxt});
		}

		/*
		 * If they didn't redirect, and they passed us undef as the
		 * callback argument, we end up doing nothing and just let
		 * Tcl eat stderr.
		 */
	}

	try {
		f = open("|${argv}", mode);
		if (wrPipe) ::close(wrPipe);
		if (stderr_cb) callbacks{f} = ctxt;
		return (f);
	} catch (&err) {
		stdio_lasterr = err;
		if (v) fprintf(stderr, "popen(%s, %s) = %s\n", cmd, mode, err);
		return (undef);
	}
}

int
printf(FMT fmt, ...args)
{
	try {
		puts(nonewline:, format(fmt, (expand)args));
		return (0);
	} catch {
		return (-1);
	}
}

string
require(...args)
{
	try {
		return (Package_require((expand)args));
	} catch {
		return (undef);
	}
}

int
rmdir(string dir)
{
	string	err;

	try {
		File_delete(dir);
		return (0);
	} catch (&err) {
		stdio_lasterr = err;
		return (-1);
	}
}

void
perror(...args)
{
	string	msg = args[0];

	if (defined(msg)) {
		puts("${msg}: ${stdio_lasterr}");
	} else {
		puts(stdio_lasterr);
	}
}

extern int ::L_putenv_bug;

string
putenv(FMT var_fmt, _argused ...args)
{
	string	ret;

	unless (var_fmt =~ /([^=]+)=(.*)/) return (undef);
	if (::L_putenv_bug == -1) {
		// test for macos-x86's putenv bug
		eval("set ::env(_L_ENV_TEST) =====");
		switch (ret=(string)getenv("_L_ENV_TEST")) {
		    case "=====":
			::L_putenv_bug = 0;
			break;
		    case "====":
			::L_putenv_bug = 1;
			break;
		    default:
			die("fatal error: ret='${ret}'");
		}
	}
	if (::L_putenv_bug && ($2[0] == "=")) {
		if (::catch("set ::env(${$1}) [format =${$2} {*}$args]", &ret)) {
			return (undef);
		}
		undef(ret[0]);  // strip leading =
	} else {
		if (::catch("set ::env(${$1}) [format {${$2}} {*}$args]", &ret)) {
			return (undef);
		}
	}
	return (ret);
}

int
size(string path)
{
	try {
		return (File_size(path));
	} catch {
		return (-1);
	}
}

void
sleep(float seconds)
{
	after((int)(seconds * 1000));
}

string
sprintf(_argused FMT fmt, _argused ...args)
{
	string	err;

	try {
		return (format(fmt, (expand)args));
	} catch (&err) {
		stdio_lasterr = err;
		return (undef);
	}
}

int
stat(string path, struct stat &buf)
{
	string	err, st_hash{string};

	try {
		File_stat(path, "ret");
		st_hash = Array_get("ret");
		buf->st_dev   = (int)st_hash{"dev"};
		buf->st_ino   = (int)st_hash{"ino"};
		buf->st_mode  = (int)st_hash{"mode"};
		buf->st_nlink = (int)st_hash{"nlink"};
		buf->st_uid   = (int)st_hash{"uid"};
		buf->st_gid   = (int)st_hash{"gid"};
		buf->st_size  = (int)st_hash{"size"};
		buf->st_atime = (int)st_hash{"atime"};
		buf->st_mtime = (int)st_hash{"mtime"};
		buf->st_ctime = (int)st_hash{"ctime"};
		buf->st_type  = st_hash{"type"};
		return (0);
	} catch (&err) {
		stdio_lasterr = err;
		return (-1);
	}
}

int
strchr(string s, string c)
{
	return (String_first(c, s));
}

int
streq(string a, string b)
{
	return (a == b);
}

int
strlen(string s)
{
	return (length(s));
}

int
strneq(string a, string b, int n)
{
	return (String_equal(length: n, a, b) != "0");
}

int
strrchr(string s, string c)
{
	return (String_last(c, s));
}

int
symlink(string sourcePath, string targetPath)
{
	string	err;

	try {
		File_link(symbolic: targetPath, sourcePath);
		return (0);
	} catch (&err) {
		stdio_lasterr = err;
		return (-1);
	}
}

private int
signame_to_num(string signame)
{
	switch (signame) {
	    case "SIGHUP":	return (1);
	    case "SIGINT":	return (2);
	    case "SIGQUIT":	return (3);
	    case "SIGABRT":	return (6);
	    case "SIGKILL":	return (9);
	    case "SIGALRM":	return (14);
	    case "SIGTERM":	return (15);
	    default:		return (undef);
	}
}

private struct {
	FILE	chIn;
	FILE	chOut;
	FILE	chErr;
	string	nmIn;
	int	flags;
	STATUS	status;
	int	started;
} spawn_handles{int};

/*
 * This is used as a filevent handler for a spawned process' stdout.
 * Read what's there and write it to whatever user output channel
 * system_() set up for it.  When we see EOF, close the process output
 * channel and reap the exit status.  Stuff it in a private global for
 * eventual use by waitpid().
 */
private void
spawn_checker_(FILE f)
{
	int	mypid = pid(f)[END];
	int	flags = spawn_handles{mypid}.flags;
	FILE	chIn  = spawn_handles{mypid}.chIn;
	FILE	chOut = spawn_handles{mypid}.chOut;

	// Can't happen?  But be paranoid.
	unless (defined(chOut)) return;

	::catch("puts -nonewline $chOut [read $f]");

	if (eof(f)) {
		spawn_handles{mypid}.chOut = undef;

		// Need to configure the channel to be blocking
		// before we call close so it will fail with an
		// error if there was one instead of ignoring it.
		fconfigure(f, blocking: 1);

		try {
			close(f);
			spawn_handles{mypid}.status.exit = 0;
		} catch {
			switch (errorCode[0]) {
			    case "CHILDSTATUS":
				spawn_handles{mypid}.status.exit =
					(int)errorCode[2];
				break;
			    case "CHILDKILLED":
				spawn_handles{mypid}.status.signal =
					signame_to_num(errorCode[2]);
				break;
			}
		}
		if (flags & SYSTEM_OUT_FILENAME__) {
			close(chOut);
		}
		if (flags & SYSTEM_ERR_FILENAME__) {
			close(spawn_handles{mypid}.chErr);
		}
		if (flags & SYSTEM_IN_FILENAME__) {
			close(chIn);
		} else if (flags & (SYSTEM_IN_ARRAY__ | SYSTEM_IN_STRING__)) {
			close(chIn);
			unlink(spawn_handles{mypid}.nmIn);
		}
		set("::%L_pid${mypid}_done", 1);  // waitpid() vwaits on this
		set("::%L_zombies", 1);		  // and this
	}
}

int
system_(poly argv, poly in, poly &out_ref, poly &err_ref, STATUS &status_ref,
	int flags)
{
	int	mypid, ret = 0, userErrRedirect = 0, userOutRedirect = 0;
	int	spawn = (flags & SYSTEM_BACKGROUND__);
	string	arg, err, nmErr, nmIn, nmOut, out, path, res;
	FILE	chErr, chIn, chOut, f;
	STATUS	status;

	/*
	 * Alias our locals "err" and "out" to the values of
	 * the "&err_ref" and "&out_ref" actuals. This lets us access
	 * these parameters whether they are passed by value or by
	 * reference. The system() API allows either and the flags arg
	 * tells us what the user passed in (the by-reference
	 * flavors are when you pass a string or array by reference,
	 * to get stdout or stderr; the by-value flavors are when you
	 * pass a file name or handle).
	 */
	eval('unset err out');
	eval('upvar 0 &err_ref err');
	eval('upvar 0 &out_ref out');

	/*
	 * If out or err were passed in by reference, check that the
	 * reference is defined.  If not, then just ignore it.
	 */
	if ((flags & (SYSTEM_OUT_STRING__ | SYSTEM_OUT_ARRAY__)) &&
	    !defined(&out_ref)) {
		flags &= ~(SYSTEM_OUT_STRING__ | SYSTEM_OUT_ARRAY__);
	}
	if ((flags & (SYSTEM_ERR_STRING__ | SYSTEM_ERR_ARRAY__)) &&
	    !defined(&err_ref)) {
		flags &= ~(SYSTEM_ERR_STRING__ | SYSTEM_ERR_ARRAY__);
	}

	unless (flags & SYSTEM_ARGV__) {
		try {
			argv = shsplit(argv);
		} catch (&res) {
			stdio_lasterr = res;
			ret = undef;
			goto out;
		}
	}
	status.argv   = argv;
	status.exit   = undef;
	status.signal = undef;
	if (length(path = auto_execok(argv[0]))) {
		status.path = path;
	} else {
		status.path = undef;
		ret = undef;
		goto out;
	}

	/* Check for user I/O re-direction. */
	foreach (arg in (string[])argv) {
		switch (arg) {
		    case /^</:
			if (flags & (SYSTEM_IN_HANDLE__ | SYSTEM_IN_FILENAME__ |
				     SYSTEM_IN_STRING__ | SYSTEM_IN_ARRAY__)) {
				stdio_lasterr = "cannot both specify and re-direct stdin";
				ret = undef;
				goto out;
			}
			break;
		    case /^>/:
			userOutRedirect = 1;
			if (flags & (SYSTEM_OUT_HANDLE__ | SYSTEM_OUT_FILENAME__ |
				     SYSTEM_OUT_STRING__ | SYSTEM_OUT_ARRAY__)) {
				stdio_lasterr = "cannot both specify and re-direct stdout";
				ret = undef;
				goto out;
			}
			break;
		    case /^2>/:
			userErrRedirect = 1;
			if (flags & (SYSTEM_ERR_HANDLE__ | SYSTEM_ERR_FILENAME__ |
				     SYSTEM_ERR_STRING__ | SYSTEM_ERR_ARRAY__)) {
				stdio_lasterr = "cannot both specify and re-direct stderr";
				ret = undef;
				goto out;
			}
			break;
		}
	}

	if (flags & (SYSTEM_IN_ARRAY__ | SYSTEM_IN_STRING__)) {
		chIn = File_Tempfile(&nmIn);
		if (flags & SYSTEM_IN_ARRAY__) {
			in = join("\n", in);
			puts(chIn, in);
		} else {
			puts(nonewline: chIn, in);
		}
		close(chIn);
		chIn = fopen(nmIn, "r");
	} else if (flags & SYSTEM_IN_FILENAME__) {
		if (defined(in)) {
			unless (defined(chIn = fopen(in, "r"))) {
				ret = undef;
				goto out;
			}
		}
	} else if (flags & SYSTEM_IN_HANDLE__) {
		unless (defined(in)) {
			stdio_lasterr = "stdin channel not open";
			ret = undef;
			goto out;
		}
		chIn = in;
	}
	if (flags & (SYSTEM_OUT_STRING__ | SYSTEM_OUT_ARRAY__)) {
		chOut = File_Tempfile(&nmOut);
	} else if (flags & SYSTEM_OUT_FILENAME__) {
		if (defined(out)) {
			unless (defined(chOut = fopen(out, "w"))) {
				ret = undef;
				goto out;
			}
		}
	} else if (flags & SYSTEM_OUT_HANDLE__) {
		unless (defined(out)) {
			stdio_lasterr = "stdout channel not open";
			ret = undef;
			goto out;
		}
		chOut = out;
	} else unless (userOutRedirect) {
		chOut = "stdout";
	}
	if (flags & (SYSTEM_ERR_STRING__ | SYSTEM_ERR_ARRAY__)) {
		chErr = File_Tempfile(&nmErr);
	} else if (flags & SYSTEM_ERR_FILENAME__) {
		if (defined(err)) {
			unless (defined(chErr = fopen(err, "w"))) {
				ret = undef;
				goto out;
			}
		}
	} else if (flags & SYSTEM_ERR_HANDLE__) {
		unless (defined(err)) {
			stdio_lasterr = "stderr channel not open";
			ret = undef;
			goto out;
		}
		chErr = err;
	} else unless (userErrRedirect) {
		chErr = "stderr";
	}

	if (defined(chIn))  push(&argv, "<@${chIn}");
	if (defined(chOut) && !spawn) push(&argv, ">@${chOut}");
	if (defined(chErr)) push(&argv, "2>@${chErr}");

	if (spawn) {
		/* For spawn(). */
		try {
			f = open("|${argv}", "r");
			mypid = pid(f)[END];
			unless (defined(chOut)) chOut = "stdout";
			spawn_handles{mypid}.chIn   = chIn;
			spawn_handles{mypid}.nmIn   = nmIn;
			spawn_handles{mypid}.chOut  = chOut;
			spawn_handles{mypid}.chErr  = chErr;
			spawn_handles{mypid}.status = status;
			spawn_handles{mypid}.flags  = flags;
			spawn_handles{mypid}.started = 1;
			unset(nocomplain: "::%L_pid${mypid}_done");
			fconfigure(f, blocking: 0, buffering: "none");
			fileevent(f, "readable", {&spawn_checker_, f});
			return (mypid);
		} catch (&res) {
			stdio_lasterr = res;
			ret = undef;
			goto out;
		}
	} else {
		/* For system(). */
		try {
			exec("--", (expand)argv);
			ret = 0;
			status.exit = ret;
		} catch (&res) {
			stdio_lasterr = res;
			switch (errorCode[0]) {
			    case "CHILDSTATUS":
				ret = (int)errorCode[2];
				status.exit = ret;
				break;
			    case "CHILDKILLED":
				status.signal = signame_to_num(errorCode[2]);
				ret = undef;
				goto out;
			    default:
				ret = undef;
				goto out;
			}
		}
	}

	if (flags & (SYSTEM_OUT_STRING__ | SYSTEM_OUT_ARRAY__)) {
		close(chOut);
		if (defined(chOut = fopen(nmOut, "r"))) {
			int n = read(chOut, &out_ref, -1);
			if (n < 0) {
				ret = undef;
				goto out;
			} else if (n == 0) {
				out_ref = undef;
			} else if (flags & SYSTEM_OUT_ARRAY__) {
				// Chomp and split.  Use Tcl's split since L's
				// strips trailing null fields.
				if (length((string)out_ref) &&
				    ((string)out_ref)[END] == "\n") {
					((string)out_ref)[END] = "";
				}
				out_ref = ::split(out_ref, "\n");
			}
		} else {
			ret = undef;
			goto out;
		}
	}
	if (flags & (SYSTEM_ERR_STRING__ | SYSTEM_ERR_ARRAY__)) {
		close(chErr);
		if (defined(chErr = fopen(nmErr, "r"))) {
			int n = read(chErr, &err_ref, -1);
			if (n < 0) {
				ret = undef;
				goto out;
			} else if (n == 0) {
				err_ref = undef;
			} else if (flags & SYSTEM_ERR_ARRAY__) {
				// Chomp and split.  Use Tcl's split since L's
				// strips trailing null fields.
				if (length((string)err_ref) &&
				    ((string)err_ref)[END] == "\n") {
					((string)err_ref)[END] = "";
				}
				err_ref = ::split(err_ref, "\n");
			}
		} else {
			ret = undef;
		}
	}
 out:
	if (flags & (SYSTEM_IN_ARRAY__|SYSTEM_IN_FILENAME__|SYSTEM_IN_STRING__)) {
		if (defined(chIn)) close(chIn);
	}
	if (flags & (SYSTEM_OUT_ARRAY__|SYSTEM_OUT_FILENAME__|SYSTEM_OUT_STRING__)) {
		if (defined(chOut)) close(chOut);
	}
	if (flags & (SYSTEM_ERR_ARRAY__|SYSTEM_ERR_FILENAME__|SYSTEM_ERR_STRING__)) {
		if (defined(chErr)) close(chErr);
	}
	if (flags & (SYSTEM_IN_ARRAY__ | SYSTEM_IN_STRING__)) {
		if (defined(nmIn)) unlink(nmIn);
	}
	if (flags & (SYSTEM_OUT_ARRAY__ | SYSTEM_OUT_STRING__)) {
		if (defined(nmOut)) unlink(nmOut);
	}
	if (flags & (SYSTEM_ERR_ARRAY__ | SYSTEM_ERR_STRING__) ) {
		if (defined(nmErr)) unlink(nmErr);
	}
	stdio_status = status;
	if (defined(&status_ref)) status_ref = status;
	return (ret);
}

/* Like system() but do not re-direct stderr; used for `cmd`. */
string
backtick_(string cmd)
{
	string	argv[], err, path, res;

	stdio_status = undef;

	try {
		argv = shsplit(cmd);
	} catch (&err) {
		stdio_lasterr = err;
		return (undef);
	}

	stdio_status.argv = argv;
	if (length(path = auto_execok(argv[0]))) {
		stdio_status.path = path;
	}

	try {
		res = exec(ignorestderr: "--", (expand)argv);
		stdio_status.exit = 0;
		return (res);
	} catch (&err) {
		switch (errorCode[0]) {
		    case "CHILDSTATUS":
			stdio_lasterr = "child process exited abnormally";
			err =~ s/child process exited abnormally\z//;
			stdio_status.exit = (int)errorCode[2];
			break;
		    case "CHILDKILLED":
			stdio_lasterr = err;
			stdio_status.signal = signame_to_num(errorCode[2]);
			break;
		    default:
			stdio_lasterr = err;
			return (undef);
		}
		return (err);
	}
}

string
trim(string s)
{
	return (String_trim(s));
}

int
unlink(string path)
{
	string	err;

	try {
		File_delete(path);
		return (0);
	} catch (&err) {
		stdio_lasterr = err;
		return (-1);
	}
}

string
uc(string s)
{
	return (String_toupper(s));
}

int
waitpid(int pid, STATUS &status, int nohang)
{
	int	p, running;

	// If we don't call vwait, Tcl will never enter the
	// event loop and call the rest of our code, so we
	// want to force an update of the event loop before
	// we do our checks.
	if (nohang) update();

	// If no pid, go find the first unreaped one.
	while (pid == -1) {
		running = 0;
		foreach (p in keys(spawn_handles)) {
			unless (defined(spawn_handles{p}.started)) {
				continue;
			}
			if (Info_exists("::%L_pid${p}_done")) {
				pid = p;
				break;
			} else {
				running++;
			}
		}
		if (pid >= 0) break;
		if (nohang || !running) {
			return (-1);
		} else {
			vwait("::%L_zombies");
		}
	}

	unless (defined(spawn_handles{pid}.started)) return (-1);
	unless (Info_exists("::%L_pid${pid}_done")) {
		if (nohang) return (0);
		vwait("::%L_pid${pid}_done");
	}
	stdio_status = spawn_handles{pid}.status;
	if (defined(&status)) status = stdio_status;
	undef(spawn_handles{pid});
	return (pid);
}

int
wait(STATUS &status)
{
	return (waitpid(-1, &status, 0));
}

void
warn_(string file, int line, FMT fmt, ...args)
{
	string	out = format(fmt, (expand)args);

	unless (length(out) && (out[END] == "\n")) {
		out .= " at ${file} line ${line}.\n";
	}
	puts(nonewline:, stderr, out);
	flush(stderr);
}

/* L function tracing support. */

extern	string	L_fnsDeclared{string}{string};
private	int	L_start_level;
private FILE	L_fn_f = stderr;
private int	L_fn_tr_inhook = 0;

/*
 * Called once before each top-level proc generated by the L compiler
 * is run (so we must be idempotent).  Walk the list of all declared L
 * functions and enable Tcl entry or exit traces on those marked with
 * tracing attributes.  When functions are compiled they inherit
 * attributes from the #pragma or command-line attributes currently in
 * effect.  These can be overridden here with environment variables.
 */
void
LtraceInit()
{
	string	args{string}, s;

	L_start_level = Info_level();

	if (s = getenv("L_TRACE_HOOK"))  args{"fnhook"} = s;
	if (s = getenv("L_TRACE_ALL"))   args{"fntrace"} = s;
	if (s = getenv("L_TRACE_FILES")) args{"trace_files"} = s;
	if (s = getenv("L_TRACE_FUNCS")) args{"trace_funcs"} = s;
	if (s = getenv("L_TRACE_OUT"))   args{"trace_out"} = s;
	if (s = getenv("L_TRACE_DEPTH")) args{"trace_depth"} = s;
	if (s = getenv("L_DISASSEMBLE")) args{"dis"} = s;
	if (s = getenv("L_TRACE")) {
		args{"trace_out"}   = s;
		args{"trace_funcs"} = "*";
	}
	Ltrace(args);
}

/*
 * This is passed a hash of named args.
 */
void
Ltrace(string args{string})
{
	string	file, fn, func, hook, s, v, what;
	string	attrs{string};
	string	trace_files = args{"trace_files"};
	string	trace_funcs = args{"trace_funcs"};

	/* Mark any function specified as a hook so we don't trace it. */
	foreach (func=>attrs in L_fnsDeclared) {
		hook = attrs{"fnhook"};
		if (hook && L_fnsDeclared{hook}) {
			L_fnsDeclared{hook}{"ishook"} = "yes";
			tracefn(func, "remove", "enter", hook);
			tracefn(func, "remove", "leave", hook);
		}
	}
	hook = args{"fnhook"};

	/*
	 * Valid formats for trace_out:
	 *    trace_out=stdout|stderr           send to stdout or stderr
	 *    trace_out=host:port		send to TCP socket
	 *    trace_out=filename		send to file
	 */
	if (v=args{"trace_out"}) {
		if (L_fn_f && (L_fn_f != stdout) && (L_fn_f != stderr)) {
			fclose(L_fn_f);
			L_fn_f = undef;
		}
		switch (v) {
		    case "stderr":
			L_fn_f = stderr;
			break;
		    case "stdout":
			L_fn_f = stdout;
			break;
		    case /^([^:]+):(\d+)$/:
			try {
				L_fn_f = socket($1, $2);
			} catch {
				warn("cannot connect to ${$1}:${$2} for "
				     "trace output\n");
			}
			break;
		    default:
			L_fn_f = fopen(v, "w");
			unless (L_fn_f) {
				warn("cannot open file '${v}' for "
				     "trace output\n");
			}
			break;
		}
	}
	if (v=args{"trace_depth"}) {
		foreach (func=>attrs in L_fnsDeclared) {
			if (attrs{"file"} == "libl.tcl") continue;
			L_fnsDeclared{func}{"trace_depth"} = v;
		}
	}
	/*
	 * If no trace_files or trace_funcs are given, then trace
	 * whatever functions are already marked for it.
	 *
	 * If trace_files or trace_funcs starts with +/-, add to or
	 * subtract from what is already marked for tracing.
	 *
	 * Otherwise, trace_files/trace_funcs is *setting* what to trace,
	 * so start by turning off all tracing.
	 */
	if ((!trace_files && !trace_funcs) ||
	    ((trace_files[0] == "+") ||
	     (trace_files[0] == "-") ||
	     (trace_funcs[0] == "+") ||
	     (trace_funcs[0] == "-"))) {
		foreach (func=>attrs in L_fnsDeclared) {
			switch (attrs{"fntrace"}) {
			    case "on":
				tracefn(func, "add", "enter", hook);
				tracefn(func, "add", "leave", hook);
				break;
			    case "entry":
				tracefn(func, "add", "enter", hook);
				tracefn(func, "remove", "leave", hook);
				break;
			    case "exit":
				tracefn(func, "remove", "enter", hook);
				tracefn(func, "add", "leave", hook);
				break;
			    case "off":
				tracefn(func, "remove", "enter", hook);
				tracefn(func, "remove", "leave", hook);
				break;
			}
		}
	} else {
		foreach (func=>attrs in L_fnsDeclared) {
			tracefn(func, "remove", "enter", hook);
			tracefn(func, "remove", "leave", hook);
		}
	}
	/*
	 * Turn on or off tracing for all functions.
	 */
	if (v=args{"fntrace"}) {
		foreach (func=>attrs in L_fnsDeclared) {
			switch (v) {
			    case "on":
				tracefn(func, "add", "enter", hook);
				tracefn(func, "add", "leave", hook);
				continue;
			    case "entry":
				tracefn(func, "add", "enter", hook);
				tracefn(func, "remove", "leave", hook);
				continue;
			    case "exit":
				tracefn(func, "remove", "enter", hook);
				tracefn(func, "add", "leave", hook);
				continue;
			    case "off":
				tracefn(func, "remove", "enter", hook);
				tracefn(func, "remove", "leave", hook);
				continue;
			}
		}
	}
	if (trace_files) {
		foreach (file in split(/[:,]/, trace_files)) {
			switch (file[0]) {
			    case '+':
				what = "add";
				undef(file[0]);
				break;
			    case '-':
				what = "remove";
				undef(file[0]);
				break;
			    default:
				what = "add";
				break;
			}
			if ((file[0] == '/') && (file[END] == '/')) {
				// Pattern is a regexp.
				undef(file[0]);
				undef(file[END]);
				foreach (func=>attrs in L_fnsDeclared) {
					if (attrs{"file"} =~ /${file}/) {
						tracefn(func, what, "enter", hook);
						tracefn(func, what, "leave", hook);
					}
				}
			} else {
				// Pattern is a glob.
				foreach (func=>attrs in L_fnsDeclared) {
					if (attrs{"file"} =~ /${file}/l) {
						tracefn(func, what, "enter", hook);
						tracefn(func, what, "leave", hook);
					}
				}
			}
		}
	}
	if (trace_funcs) {
		foreach (func in split(/[:,]/, trace_funcs)) {
			switch (func[0]) {
			    case '+':
				what = "add";
				undef(func[0]);
				break;
			    case '-':
				what = "remove";
				undef(func[0]);
				break;
			    default:
				what = "add";
				break;
			}
			// Lib L override.
			if (func[0] == '!') {
				what = "!" . what;
				undef(func[0]);
			}
			if ((func[0] == '/') && (func[END] == '/')) {
				// Pattern is a regexp.
				undef(func[0]);
				undef(func[END]);
				foreach (fn=>attrs in L_fnsDeclared) {
					if (attrs{"name"} =~ /${func}/) {
						tracefn(fn, what, "enter", hook);
						tracefn(fn, what, "leave", hook);
					}
				}
			} else {
				// Pattern is a glob.
				foreach (fn=>attrs in L_fnsDeclared) {
					if (attrs{"name"} =~ /${func}/l) {
						tracefn(fn, what, "enter", hook);
						tracefn(fn, what, "leave", hook);
					}
				}
			}
		}
	}
	/* Disassemble. */
	if (v=args{"dis"}) {
		if ((v == "1") || (v =~ /yes/i)) v = "*";
		foreach (s in split(/[:,]/, v)) {
			switch (s[0]) {
			    case '+':
				what = "yes";
				undef(s[0]);
				break;
			    case '-':
				what = "no";
				undef(s[0]);
				break;
			    default:
				what = "yes";
				break;
			}
			// Check pattern against both func and file names.
			if ((s[0] == '/') && (s[END] == '/')) {
				// Pattern is a regexp.
				undef(s[0]);
				undef(s[END]);
				foreach (fn=>attrs in L_fnsDeclared) {
					if ((attrs{"name"} =~ /${s}/) ||
					    (attrs{"file"} =~ /${s}/)) {
						L_fnsDeclared{fn}{"dis"} = what;
					}
				}
			} else {
				// Pattern is a glob.
				foreach (fn=>attrs in L_fnsDeclared) {
					if ((attrs{"name"} =~ /${s}/l) ||
					    (attrs{"file"} =~ /${s}/l)) {
						L_fnsDeclared{fn}{"dis"} = what;
					}
				}
			}
		}
		foreach (fn=>attrs in L_fnsDeclared) {
			if (attrs{"dis_done"}) continue;
			if (attrs{"dis"} == "yes") {
				s = ::tcl::unsupported::disassemble("proc", fn);
				puts(L_fn_f, "Disassembly for ${fn}:");
				puts(L_fn_f, s);
				L_fnsDeclared{fn}{"dis_done"} = "yes";
			}
		}
	}
}

/*
 * Add or remove a Tcl proc trace.  If specified, the hook arg
 * overrides anything already specified for the function.
 */
private void
tracefn(string fn, string what, string op, string hook)
{
	string	attrs{string} = L_fnsDeclared{fn};
	struct {
		string	op;
		string	cmd;
	} trace, traces[];

	// A leading ! allows lib L funcs to be traced.
	if (what[0] == '!') {
		undef(what[0]);
	} else if (attrs{"file"} == "libl.tcl") {
		return;
	}
	if (attrs{"ishook"}) goto remove;
	switch (what) {
	    case "add":
		// Do nothing if there's already a Tcl trace
		if (Trace_infoExec(fn) =~ /{${op}/) {  // yes, {$op
			return;
		}
		if (hook) {
			L_fnsDeclared{fn}{"fnhook"} = hook;
		} else unless (attrs{"fnhook"}) {
			L_fnsDeclared{fn}{"fnhook"} = "L_def_fn_hook";
		}
		if (L_fnsDeclared{fn}{"fnhook"} == "def") {
			L_fnsDeclared{fn}{"fnhook"} = "L_def_fn_hook";
		}
		switch (op) {
		    case "enter":
			Trace_addExec(fn, "enter", &L_fn_pre_hook);
			break;
		    case "leave":
			Trace_addExec(fn, "leave", &L_fn_post_hook);
			break;
		}
		undef(L_fnsDeclared{fn}{"fntrace_${op}"});
		break;
	    case "remove":
remove:
		traces = Trace_infoExec(fn);
		foreach (trace in traces) {
			if (trace.op == op) Trace_removeExec(fn, op, trace.cmd);
		}
		L_fnsDeclared{fn}{"fntrace_${op}"} = "off";
		break;
	}
}

void
L_fn_pre_hook(string av[], _argused string op)
{
	string	s;
	string	attrs{string} = L_fnsDeclared{av[0]};

	if (L_fn_tr_inhook || (attrs{"fntrace_enter"} == "off") ||
	    ((s=attrs{"trace_depth"}) &&
	     ((Info_level() - L_start_level) > (int)s))) {
		    return;
	}

	/* Use the unmangled func name. */
	av[0] = attrs{"name"};

	++L_fn_tr_inhook;
	if (::catch("${attrs{"fnhook"}} 1 $av 0", &s)) {
		/*
		 * Dump the error to stderr because the return below doesn't
		 * propagate the errorinfo up out of this trace hook to the
		 * traced proc although the Tcl docs say it should.
		 */
		puts(stderr, "trace hook error: ${s}");
		--L_fn_tr_inhook;
		eval("return -code error -errorinfo $s");
	}
	--L_fn_tr_inhook;
}

void
L_fn_post_hook(string av[], _argused string code, _argused string result,
	       _argused string op)
{
	string	s;
	string	attrs{string} = L_fnsDeclared{av[0]};

	if (L_fn_tr_inhook || (attrs{"fntrace_leave"} == "off") ||
	    ((s=attrs{"trace_depth"}) &&
	     ((Info_level() - L_start_level) > (int)s))) {
		    return;
	}

	/* Use the unmangled func name. */
	av[0] = attrs{"name"};

	++L_fn_tr_inhook;
	if (::catch("${attrs{"fnhook"}} 0 $av $result", &s)) {
		/*
		 * Dump the error to stderr because the return below doesn't
		 * propagate the errorinfo up out of this trace hook to the
		 * traced proc although the Tcl docs say it should.
		 */
		puts(stderr, "trace hook error: ${s}");
		--L_fn_tr_inhook;
		eval("return -code error -errorinfo $s");
	}
	--L_fn_tr_inhook;
}

private string
argStr(poly arg)
{
	return (defined(arg) ? "'${arg}'" : "<undef>");
}

void
L_def_fn_hook(int pre, poly av[], poly ret)
{
	int	i;
	int	ac = length(av);

	fprintf(L_fn_f, "%d: %s %s%s", milli(),
		pre?"enter":"exit", av[0], i>1?":":"");
	for (i = 1; i < ac; ++i) {
		fprintf(L_fn_f, " %s", argStr(av[i]));
	}
	unless (pre) fprintf(L_fn_f, " ret %s", argStr(ret));
	fprintf(L_fn_f, "\n");
}

/*
 * Some GUI helper functions
 */

int
tk_loaded_()
{
	return (Info_exists("::tk_patchLevel"));
}

void
tk_make_readonly_tag_()
{
	string  script, event, events[];

	events = bind("Text");
	foreach (event in events) {
		script = bind("Text", event);
		if (script =~ /%W (insert|delete|edit)/) continue;
		if (script =~ /text(paste|insert|transpose)/i) continue;
		script =~ s/tk_textCut/tk_textCopy/g;
		bind("ReadonlyText", event, script);
	}
}

void
tk_save_to_log_(widget t)
{
	FILE    fp;
	string  file, data;

	file = tk_getSaveFile(parent: Winfo_toplevel((string)t));
	if (file == "") return;

	data = trim(Text_get(t, 1.0, "end"));

	fp = fopen(file, "w");
	puts(fp, data);
	fclose(fp);
}

/*
 * This is top-level run-time initialization code that gets called
 * before main().
 */

/*
 * Exit on a broken stdout pipe, so that things like
 *   tclsh myscript.l | more
 * exit silently when you type 'q'.
 */
fconfigure(stdout, epipe: "exit");

/*
 * This catches accesses to an formal reference parameter when undef is
 * passed in instead of a variable reference.  Throw a run-time error.
 */
void
L_undef_ref_parm_accessed_(_argused string name1, _argused string name2,
			   string op)
{
	string	msg;

	switch (op) {
	    case "read":  msg = "read"; break;
	    case "write": msg = "written"; break;
	    default: return;  // should be impossible
	}
	eval("return -code error -level 2 {undefined reference parameter ${msg}}");
}
string	L_undef_ref_parm_ = "L_undef_ref_parm_ object";
Trace_addVariable("::L_undef_ref_parm_", {"read","write"},
		  &L_undef_ref_parm_accessed_);

#lang tcl
set ::L_libl_done 1
