/*
 * tclPosixStr.c --
 *
 *	This file contains procedures that generate strings corresponding to
 *	various POSIX-related codes, such as errno and signals.
 *
 * Copyright © 1991-1994 The Regents of the University of California.
 * Copyright © 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ErrnoId --
 *
 *	Return a textual identifier for the current errno value.
 *
 * Results:
 *	This procedure returns a machine-readable textual identifier that
 *	corresponds to the current errno value (e.g. "EPERM"). The identifier
 *	is the same as the #define name in errno.h.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const char *
Tcl_ErrnoId(void)
{
    switch (errno) {
#if defined(E2BIG) && (!defined(EOVERFLOW) || (E2BIG != EOVERFLOW))
    case E2BIG: return "E2BIG";
#endif
#ifdef EACCES
    case EACCES: return "EACCES";
#endif
#ifdef EADDRINUSE
    case EADDRINUSE: return "EADDRINUSE";
#endif
#ifdef EADDRNOTAVAIL
    case EADDRNOTAVAIL: return "EADDRNOTAVAIL";
#endif
#ifdef EADV
    case EADV: return "EADV";
#endif
#ifdef EAFNOSUPPORT
    case EAFNOSUPPORT: return "EAFNOSUPPORT";
#endif
#ifdef EAGAIN
    case EAGAIN: return "EAGAIN";
#endif
#ifdef EALIGN
    case EALIGN: return "EALIGN";
#endif
#if defined(EALREADY) && (!defined(EBUSY) || (EALREADY != EBUSY))
    case EALREADY: return "EALREADY";
#endif
#ifdef EBADE
    case EBADE: return "EBADE";
#endif
#ifdef EBADF
    case EBADF: return "EBADF";
#endif
#ifdef EBADFD
    case EBADFD: return "EBADFD";
#endif
#ifdef EBADMSG
    case EBADMSG: return "EBADMSG";
#endif
#ifdef ECANCELED
    case ECANCELED: return "ECANCELED";
#endif
#ifdef EBADR
    case EBADR: return "EBADR";
#endif
#ifdef EBADRPC
    case EBADRPC: return "EBADRPC";
#endif
#ifdef EBADRQC
    case EBADRQC: return "EBADRQC";
#endif
#ifdef EBADSLT
    case EBADSLT: return "EBADSLT";
#endif
#ifdef EBFONT
    case EBFONT: return "EBFONT";
#endif
#ifdef EBUSY
    case EBUSY: return "EBUSY";
#endif
#ifdef ECHILD
    case ECHILD: return "ECHILD";
#endif
#ifdef ECHRNG
    case ECHRNG: return "ECHRNG";
#endif
#ifdef ECOMM
    case ECOMM: return "ECOMM";
#endif
#ifdef ECONNABORTED
    case ECONNABORTED: return "ECONNABORTED";
#endif
#ifdef ECONNREFUSED
    case ECONNREFUSED: return "ECONNREFUSED";
#endif
#ifdef ECONNRESET
    case ECONNRESET: return "ECONNRESET";
#endif
#if defined(EDEADLK) && (!defined(EWOULDBLOCK) || (EDEADLK != EWOULDBLOCK))
    case EDEADLK: return "EDEADLK";
#endif
#if defined(EDEADLOCK) && (!defined(EDEADLK) || (EDEADLOCK != EDEADLK))
    case EDEADLOCK: return "EDEADLOCK";
#endif
#ifdef EDESTADDRREQ
    case EDESTADDRREQ: return "EDESTADDRREQ";
#endif
#ifdef EDIRTY
    case EDIRTY: return "EDIRTY";
#endif
#ifdef EDOM
    case EDOM: return "EDOM";
#endif
#ifdef EDOTDOT
    case EDOTDOT: return "EDOTDOT";
#endif
#ifdef EDQUOT
    case EDQUOT: return "EDQUOT";
#endif
#ifdef EDUPPKG
    case EDUPPKG: return "EDUPPKG";
#endif
#ifdef EEXIST
    case EEXIST: return "EEXIST";
#endif
#ifdef EFAULT
    case EFAULT: return "EFAULT";
#endif
#ifdef EFBIG
    case EFBIG: return "EFBIG";
#endif
#ifdef EHOSTDOWN
    case EHOSTDOWN: return "EHOSTDOWN";
#endif
#ifdef EHOSTUNREACH
    case EHOSTUNREACH: return "EHOSTUNREACH";
#endif
#if defined(EIDRM) && (!defined(EINPROGRESS) || (EIDRM != EINPROGRESS))
    case EIDRM: return "EIDRM";
#endif
#ifdef EINIT
    case EINIT: return "EINIT";
#endif
#ifdef EILSEQ
    case EILSEQ: return "EILSEQ";
#endif
#ifdef EINPROGRESS
    case EINPROGRESS: return "EINPROGRESS";
#endif
#ifdef EINTR
    case EINTR: return "EINTR";
#endif
#ifdef EINVAL
    case EINVAL: return "EINVAL";
#endif
#ifdef EIO
    case EIO: return "EIO";
#endif
#ifdef EISCONN
    case EISCONN: return "EISCONN";
#endif
#ifdef EISDIR
    case EISDIR: return "EISDIR";
#endif
#ifdef EISNAME
    case EISNAM: return "EISNAM";
#endif
#ifdef ELBIN
    case ELBIN: return "ELBIN";
#endif
#ifdef EL2HLT
    case EL2HLT: return "EL2HLT";
#endif
#ifdef EL2NSYNC
    case EL2NSYNC: return "EL2NSYNC";
#endif
#ifdef EL3HLT
    case EL3HLT: return "EL3HLT";
#endif
#ifdef EL3RST
    case EL3RST: return "EL3RST";
#endif
#ifdef ELIBACC
    case ELIBACC: return "ELIBACC";
#endif
#ifdef ELIBBAD
    case ELIBBAD: return "ELIBBAD";
#endif
#ifdef ELIBEXEC
    case ELIBEXEC: return "ELIBEXEC";
#endif
#if defined(ELIBMAX) && (!defined(ECANCELED) || (ELIBMAX != ECANCELED))
    case ELIBMAX: return "ELIBMAX";
#endif
#ifdef ELIBSCN
    case ELIBSCN: return "ELIBSCN";
#endif
#ifdef ELNRNG
    case ELNRNG: return "ELNRNG";
#endif
#if defined(ELOOP) && (!defined(ENOENT) || (ELOOP != ENOENT))
    case ELOOP: return "ELOOP";
#endif
#ifdef EMFILE
    case EMFILE: return "EMFILE";
#endif
#ifdef EMLINK
    case EMLINK: return "EMLINK";
#endif
#ifdef EMSGSIZE
    case EMSGSIZE: return "EMSGSIZE";
#endif
#ifdef EMULTIHOP
    case EMULTIHOP: return "EMULTIHOP";
#endif
#ifdef ENAMETOOLONG
    case ENAMETOOLONG: return "ENAMETOOLONG";
#endif
#ifdef ENAVAIL
    case ENAVAIL: return "ENAVAIL";
#endif
#ifdef ENET
    case ENET: return "ENET";
#endif
#ifdef ENETDOWN
    case ENETDOWN: return "ENETDOWN";
#endif
#ifdef ENETRESET
    case ENETRESET: return "ENETRESET";
#endif
#ifdef ENETUNREACH
    case ENETUNREACH: return "ENETUNREACH";
#endif
#ifdef ENFILE
    case ENFILE: return "ENFILE";
#endif
#ifdef ENOANO
    case ENOANO: return "ENOANO";
#endif
#if defined(ENOBUFS) && (!defined(ENOSR) || (ENOBUFS != ENOSR))
    case ENOBUFS: return "ENOBUFS";
#endif
#ifdef ENOCSI
    case ENOCSI: return "ENOCSI";
#endif
#if defined(ENODATA) && (!defined(ECONNREFUSED) || (ENODATA != ECONNREFUSED))
    case ENODATA: return "ENODATA";
#endif
#ifdef ENODEV
    case ENODEV: return "ENODEV";
#endif
#ifdef ENOENT
    case ENOENT: return "ENOENT";
#endif
#ifdef ENOEXEC
    case ENOEXEC: return "ENOEXEC";
#endif
#ifdef ENOLCK
    case ENOLCK: return "ENOLCK";
#endif
#ifdef ENOLINK
    case ENOLINK: return "ENOLINK";
#endif
#ifdef ENOMEM
    case ENOMEM: return "ENOMEM";
#endif
#ifdef ENOMSG
    case ENOMSG: return "ENOMSG";
#endif
#ifdef ENONET
    case ENONET: return "ENONET";
#endif
#ifdef ENOPKG
    case ENOPKG: return "ENOPKG";
#endif
#ifdef ENOPROTOOPT
    case ENOPROTOOPT: return "ENOPROTOOPT";
#endif
#ifdef ENOSPC
    case ENOSPC: return "ENOSPC";
#endif
#if defined(ENOSR) && (!defined(ENAMETOOLONG) || (ENAMETOOLONG != ENOSR))
    case ENOSR: return "ENOSR";
#endif
#if defined(ENOSTR) && (!defined(ENOTTY) || (ENOTTY != ENOSTR))
    case ENOSTR: return "ENOSTR";
#endif
#ifdef ENOSYM
    case ENOSYM: return "ENOSYM";
#endif
#ifdef ENOSYS
    case ENOSYS: return "ENOSYS";
#endif
#ifdef ENOTBLK
    case ENOTBLK: return "ENOTBLK";
#endif
#ifdef ENOTCONN
    case ENOTCONN: return "ENOTCONN";
#endif
#ifdef ENOTRECOVERABLE
    case ENOTRECOVERABLE: return "ENOTRECOVERABLE";
#endif
#ifdef ENOTDIR
    case ENOTDIR: return "ENOTDIR";
#endif
#if defined(ENOTEMPTY) && (!defined(EEXIST) || (ENOTEMPTY != EEXIST))
    case ENOTEMPTY: return "ENOTEMPTY";
#endif
#ifdef ENOTNAM
    case ENOTNAM: return "ENOTNAM";
#endif
#ifdef ENOTSOCK
    case ENOTSOCK: return "ENOTSOCK";
#endif
#ifdef ENOTSUP
    case ENOTSUP: return "ENOTSUP";
#endif
#ifdef ENOTTY
    case ENOTTY: return "ENOTTY";
#endif
#ifdef ENOTUNIQ
    case ENOTUNIQ: return "ENOTUNIQ";
#endif
#ifdef ENXIO
    case ENXIO: return "ENXIO";
#endif
#if defined(EOPNOTSUPP) &&  (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
    case EOPNOTSUPP: return "EOPNOTSUPP";
#endif
#ifdef EOTHER
    case EOTHER: return "EOTHER";
#endif
#if defined(EOVERFLOW) && (!defined(EFBIG) || (EOVERFLOW != EFBIG)) && (!defined(EINVAL) || (EOVERFLOW != EINVAL))
    case EOVERFLOW: return "EOVERFLOW";
#endif
#ifdef EOWNERDEAD
    case EOWNERDEAD: return "EOWNERDEAD";
#endif
#ifdef EPERM
    case EPERM: return "EPERM";
#endif
#if defined(EPFNOSUPPORT) && (!defined(ENOLCK) || (ENOLCK != EPFNOSUPPORT))
    case EPFNOSUPPORT: return "EPFNOSUPPORT";
#endif
#ifdef EPIPE
    case EPIPE: return "EPIPE";
#endif
#ifdef EPROCLIM
    case EPROCLIM: return "EPROCLIM";
#endif
#ifdef EPROCUNAVAIL
    case EPROCUNAVAIL: return "EPROCUNAVAIL";
#endif
#ifdef EPROGMISMATCH
    case EPROGMISMATCH: return "EPROGMISMATCH";
#endif
#ifdef EPROGUNAVAIL
    case EPROGUNAVAIL: return "EPROGUNAVAIL";
#endif
#ifdef EPROTO
    case EPROTO: return "EPROTO";
#endif
#ifdef EPROTONOSUPPORT
    case EPROTONOSUPPORT: return "EPROTONOSUPPORT";
#endif
#ifdef EPROTOTYPE
    case EPROTOTYPE: return "EPROTOTYPE";
#endif
#ifdef ERANGE
    case ERANGE: return "ERANGE";
#endif
#if defined(EREFUSED) && (!defined(ECONNREFUSED) || (EREFUSED != ECONNREFUSED))
    case EREFUSED: return "EREFUSED";
#endif
#ifdef EREMCHG
    case EREMCHG: return "EREMCHG";
#endif
#ifdef EREMDEV
    case EREMDEV: return "EREMDEV";
#endif
#ifdef EREMOTE
    case EREMOTE: return "EREMOTE";
#endif
#ifdef EREMOTEIO
    case EREMOTEIO: return "EREMOTEIO";
#endif
#ifdef EREMOTERELEASE
    case EREMOTERELEASE: return "EREMOTERELEASE";
#endif
#ifdef EROFS
    case EROFS: return "EROFS";
#endif
#ifdef ERPCMISMATCH
    case ERPCMISMATCH: return "ERPCMISMATCH";
#endif
#ifdef ERREMOTE
    case ERREMOTE: return "ERREMOTE";
#endif
#ifdef ESHUTDOWN
    case ESHUTDOWN: return "ESHUTDOWN";
#endif
#ifdef ESOCKTNOSUPPORT
    case ESOCKTNOSUPPORT: return "ESOCKTNOSUPPORT";
#endif
#ifdef ESPIPE
    case ESPIPE: return "ESPIPE";
#endif
#ifdef ESRCH
    case ESRCH: return "ESRCH";
#endif
#ifdef ESRMNT
    case ESRMNT: return "ESRMNT";
#endif
#ifdef ESTALE
    case ESTALE: return "ESTALE";
#endif
#ifdef ESUCCESS
    case ESUCCESS: return "ESUCCESS";
#endif
#if defined(ETIME) && (!defined(ELOOP) || (ETIME != ELOOP))
    case ETIME: return "ETIME";
#endif
#if defined(ETIMEDOUT) && (!defined(ENOSTR) || (ETIMEDOUT != ENOSTR))
    case ETIMEDOUT: return "ETIMEDOUT";
#endif
#ifdef ETOOMANYREFS
    case ETOOMANYREFS: return "ETOOMANYREFS";
#endif
#ifdef ETXTBSY
    case ETXTBSY: return "ETXTBSY";
#endif
#ifdef EUCLEAN
    case EUCLEAN: return "EUCLEAN";
#endif
#ifdef EUNATCH
    case EUNATCH: return "EUNATCH";
#endif
#ifdef EUSERS
    case EUSERS: return "EUSERS";
#endif
#ifdef EVERSION
    case EVERSION: return "EVERSION";
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
    case EWOULDBLOCK: return "EWOULDBLOCK";
#endif
#ifdef EXDEV
    case EXDEV: return "EXDEV";
#endif
#ifdef EXFULL
    case EXFULL: return "EXFULL";
#endif
    }
    return "unknown error";
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ErrnoMsg --
 *
 *	Return a human-readable message corresponding to a given errno value.
 *
 * Results:
 *	The return value is the standard POSIX error message for errno.  This
 *	procedure is used instead of strerror because strerror returns
 *	slightly different values on different machines (e.g. different
 *	capitalizations), which cause problems for things such as regression
 *	tests. This procedure provides messages for most standard errors, then
 *	it calls strerror for things it doesn't understand.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const char *
Tcl_ErrnoMsg(
     int err)			/* Error number (such as in errno variable). */
{
#ifndef _WIN32
    return strerror(err);
#else
    switch (err) {
#if defined(E2BIG) && (!defined(EOVERFLOW) || (E2BIG != EOVERFLOW))
    case E2BIG: return "Argument list too long";
#endif
#ifdef EACCES
    case EACCES: return "Permission denied";
#endif
#ifdef EADDRINUSE
    case EADDRINUSE: return "Address in use";
#endif
#ifdef EADDRNOTAVAIL
    case EADDRNOTAVAIL: return "Address not available";
#endif
#ifdef EADV
    case EADV: return "Advertise error";
#endif
#ifdef EAFNOSUPPORT
    case EAFNOSUPPORT: return "Address family not supported";
#endif
#ifdef EAGAIN
    case EAGAIN: return "Resource unavailable, try again";
#endif
#ifdef EALIGN
    case EALIGN: return "EALIGN";
#endif
#if defined(EALREADY) && (!defined(EBUSY) || (EALREADY != EBUSY))
    case EALREADY: return "Connection already in progress";
#endif
#ifdef EBADE
    case EBADE: return "Bad exchange descriptor";
#endif
#ifdef EBADF
    case EBADF: return "Bad file descriptor";
#endif
#ifdef EBADFD
    case EBADFD: return "File descriptor in bad state";
#endif
#ifdef EBADMSG
    case EBADMSG: return "Bad message";
#endif
#ifdef EBADR
    case EBADR: return "Bad request descriptor";
#endif
#ifdef EBADRPC
    case EBADRPC: return "RPC structure is bad";
#endif
#ifdef EBADRQC
    case EBADRQC: return "Bad request code";
#endif
#ifdef EBADSLT
    case EBADSLT: return "Invalid slot";
#endif
#ifdef EBFONT
    case EBFONT: return "Bad font file format";
#endif
#ifdef EBUSY
    case EBUSY: return "Device or resource busy";
#endif
#ifdef ECANCELED
    case ECANCELED: return "Operation canceled";
#endif
#ifdef ECHILD
    case ECHILD: return "No child processes";
#endif
#ifdef ECHRNG
    case ECHRNG: return "Channel number out of range";
#endif
#ifdef ECOMM
    case ECOMM: return "Communication error on send";
#endif
#ifdef ECONNABORTED
    case ECONNABORTED: return "Connection aborted";
#endif
#ifdef ECONNREFUSED
    case ECONNREFUSED: return "Connection refused";
#endif
#ifdef ECONNRESET
    case ECONNRESET: return "Connection reset";
#endif
#if defined(EDEADLK) && (!defined(EWOULDBLOCK) || (EDEADLK != EWOULDBLOCK))
    case EDEADLK: return "Resource deadlock would occur";
#endif
#if defined(EDEADLOCK) && (!defined(EDEADLK) || (EDEADLOCK != EDEADLK))
    case EDEADLOCK: return "Resource deadlock would occur";
#endif
#ifdef EDESTADDRREQ
    case EDESTADDRREQ: return "Destination address required";
#endif
#ifdef EDIRTY
    case EDIRTY: return "Mounting a dirty fs w/o force";
#endif
#ifdef EDOM
    case EDOM: return "Mathematics argument out of domain of function";
#endif
#ifdef EDOTDOT
    case EDOTDOT: return "Cross mount point";
#endif
#ifdef EDQUOT
    case EDQUOT: return "Disk quota exceeded";
#endif
#ifdef EDUPPKG
    case EDUPPKG: return "Duplicate package name";
#endif
#ifdef EEXIST
    case EEXIST: return "File exists";
#endif
#ifdef EFAULT
    case EFAULT: return "Bad address";
#endif
#ifdef EFBIG
    case EFBIG: return "File too large";
#endif
#ifdef EHOSTDOWN
    case EHOSTDOWN: return "Host is down";
#endif
#ifdef EHOSTUNREACH
    case EHOSTUNREACH: return "Host is unreachable";
#endif
#if defined(EIDRM) && (!defined(EINPROGRESS) || (EIDRM != EINPROGRESS))
    case EIDRM: return "Identifier removed";
#endif
#ifdef EINIT
    case EINIT: return "Initialization error";
#endif
#ifdef EILSEQ
    case EILSEQ: return "Invalid or incomplete multibyte or wide character";
#endif
#ifdef EINPROGRESS
    case EINPROGRESS: return "Operation in progress";
#endif
#ifdef EINTR
    case EINTR: return "Interrupted function";
#endif
#ifdef EINVAL
    case EINVAL: return "Invalid argument";
#endif
#ifdef EIO
    case EIO: return "I/O error";
#endif
#ifdef EISCONN
    case EISCONN: return "Socket is connected";
#endif
#ifdef EISDIR
    case EISDIR: return "Is a directory";
#endif
#ifdef EISNAME
    case EISNAM: return "Is a name file";
#endif
#ifdef ELBIN
    case ELBIN: return "ELBIN";
#endif
#ifdef EL2HLT
    case EL2HLT: return "Level 2 halted";
#endif
#ifdef EL2NSYNC
    case EL2NSYNC: return "Level 2 not synchronized";
#endif
#ifdef EL3HLT
    case EL3HLT: return "Level 3 halted";
#endif
#ifdef EL3RST
    case EL3RST: return "Level 3 reset";
#endif
#ifdef ELIBACC
    case ELIBACC: return "Cannot access a needed shared library";
#endif
#ifdef ELIBBAD
    case ELIBBAD: return "Accessing a corrupted shared library";
#endif
#ifdef ELIBEXEC
    case ELIBEXEC: return "Cannot exec a shared library directly";
#endif
#if defined(ELIBMAX) && (!defined(ECANCELED) || (ELIBMAX != ECANCELED))
    case ELIBMAX: return
	    "Attempting to link in more shared libraries than system limit";
#endif
#ifdef ELIBSCN
    case ELIBSCN: return ".lib section in a.out corrupted";
#endif
#ifdef ELNRNG
    case ELNRNG: return "Link number out of range";
#endif
#if defined(ELOOP) && (!defined(ENOENT) || (ELOOP != ENOENT))
    case ELOOP: return "Too many levels of symbolic links";
#endif
#ifdef EMFILE
    case EMFILE: return "File descriptor value too large";
#endif
#ifdef EMLINK
    case EMLINK: return "Too many links";
#endif
#ifdef EMSGSIZE
    case EMSGSIZE: return "Message too large";
#endif
#ifdef EMULTIHOP
    case EMULTIHOP: return "Multihop attempted";
#endif
#ifdef ENAMETOOLONG
    case ENAMETOOLONG: return "Filename too long";
#endif
#ifdef ENAVAIL
    case ENAVAIL: return "Not available";
#endif
#ifdef ENET
    case ENET: return "ENET";
#endif
#ifdef ENETDOWN
    case ENETDOWN: return "Network is down";
#endif
#ifdef ENETRESET
    case ENETRESET: return "Network dropped connection on reset";
#endif
#ifdef ENETUNREACH
    case ENETUNREACH: return "Network is unreachable";
#endif
#ifdef ENFILE
    case ENFILE: return "Too many files open in system";
#endif
#ifdef ENOANO
    case ENOANO: return "Anode table overflow";
#endif
#if defined(ENOBUFS) && (!defined(ENOSR) || (ENOBUFS != ENOSR))
    case ENOBUFS: return "No buffer space available";
#endif
#ifdef ENOCSI
    case ENOCSI: return "No CSI structure available";
#endif
#if defined(ENODATA) && (!defined(ECONNREFUSED) || (ENODATA != ECONNREFUSED))
    case ENODATA: return "No data available";
#endif
#ifdef ENODEV
    case ENODEV: return "No such device";
#endif
#ifdef ENOENT
    case ENOENT: return "No such file or directory";
#endif
#ifdef ENOEXEC
    case ENOEXEC: return "Executable format error";
#endif
#ifdef ENOLCK
    case ENOLCK: return "No locks available";
#endif
#ifdef ENOLINK
    case ENOLINK: return "Link has been severed";
#endif
#ifdef ENOMEM
    case ENOMEM: return "Not enough space";
#endif
#ifdef ENOMSG
    case ENOMSG: return "No message of desired type";
#endif
#ifdef ENONET
    case ENONET: return "Machine is not on the network";
#endif
#ifdef ENOPKG
    case ENOPKG: return "Package not installed";
#endif
#ifdef ENOPROTOOPT
    case ENOPROTOOPT: return "Protocol not available";
#endif
#ifdef ENOSPC
    case ENOSPC: return "No space left on device";
#endif
#if defined(ENOSR) && (!defined(ENAMETOOLONG) || (ENAMETOOLONG != ENOSR))
    case ENOSR: return "No stream resources";
#endif
#if defined(ENOSTR) && (!defined(ENOTTY) || (ENOTTY != ENOSTR))
    case ENOSTR: return "Not a stream";
#endif
#ifdef ENOSYM
    case ENOSYM: return "Unresolved symbol name";
#endif
#ifdef ENOSYS
    case ENOSYS: return "Functionality not supported";
#endif
#ifdef ENOTBLK
    case ENOTBLK: return "Block device required";
#endif
#ifdef ENOTCONN
    case ENOTCONN: return "Transport endpoint is not connected";
#endif
#ifdef ENOTRECOVERABLE
    case ENOTRECOVERABLE: return "State not recoverable";
#endif
#ifdef ENOTDIR
    case ENOTDIR: return "Not a directory or a symbolic link to a directory";
#endif
#if defined(ENOTEMPTY) && (!defined(EEXIST) || (ENOTEMPTY != EEXIST))
    case ENOTEMPTY: return "Directory not empty";
#endif
#ifdef ENOTNAM
    case ENOTNAM: return "Not a name file";
#endif
#ifdef ENOTSOCK
    case ENOTSOCK: return "Not a socket";
#endif
#ifdef ENOTSUP
    case ENOTSUP: return "Not supported";
#endif
#ifdef ENOTTY
    case ENOTTY: return "Inappropriate I/O control operation";
#endif
#ifdef ENOTUNIQ
    case ENOTUNIQ: return "Name not unique on network";
#endif
#ifdef ENXIO
    case ENXIO: return "No such device or address";
#endif
#if defined(EOPNOTSUPP) &&  (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
    case EOPNOTSUPP: return "Operation not supported on socket";
#endif
#ifdef EOTHER
    case EOTHER: return "Other error";
#endif
#if defined(EOVERFLOW) && (!defined(EFBIG) || (EOVERFLOW != EFBIG)) && (!defined(EINVAL) || (EOVERFLOW != EINVAL))
    case EOVERFLOW: return "Value too large to be stored in data type";
#endif
#ifdef EOWNERDEAD
    case EOWNERDEAD: return "Previous owner died";
#endif
#ifdef EPERM
    case EPERM: return "Operation not permitted";
#endif
#if defined(EPFNOSUPPORT) && (!defined(ENOLCK) || (ENOLCK != EPFNOSUPPORT))
    case EPFNOSUPPORT: return "Protocol family not supported";
#endif
#ifdef EPIPE
    case EPIPE: return "Broken pipe";
#endif
#ifdef EPROCLIM
    case EPROCLIM: return "Too many processes";
#endif
#ifdef EPROCUNAVAIL
    case EPROCUNAVAIL: return "Bad procedure for program";
#endif
#ifdef EPROGMISMATCH
    case EPROGMISMATCH: return "Program version wrong";
#endif
#ifdef EPROGUNAVAIL
    case EPROGUNAVAIL: return "RPC program not available";
#endif
#ifdef EPROTO
    case EPROTO: return "Protocol error";
#endif
#ifdef EPROTONOSUPPORT
    case EPROTONOSUPPORT: return "Protocol not supported";
#endif
#ifdef EPROTOTYPE
    case EPROTOTYPE: return "Protocol wrong type for socket";
#endif
#ifdef ERANGE
    case ERANGE: return "Result too large";
#endif
#if defined(EREFUSED) && (!defined(ECONNREFUSED) || (EREFUSED != ECONNREFUSED))
    case EREFUSED: return "EREFUSED";
#endif
#ifdef EREMCHG
    case EREMCHG: return "Remote address changed";
#endif
#ifdef EREMDEV
    case EREMDEV: return "Remote device";
#endif
#ifdef EREMOTE
    case EREMOTE: return "Pathname hit remote file system";
#endif
#ifdef EREMOTEIO
    case EREMOTEIO: return "Remote i/o error";
#endif
#ifdef EREMOTERELEASE
    case EREMOTERELEASE: return "EREMOTERELEASE";
#endif
#ifdef EROFS
    case EROFS: return "Read-only file system";
#endif
#ifdef ERPCMISMATCH
    case ERPCMISMATCH: return "RPC version is wrong";
#endif
#ifdef ERREMOTE
    case ERREMOTE: return "Object is remote";
#endif
#ifdef ESHUTDOWN
    case ESHUTDOWN: return "Cannot send after socket shutdown";
#endif
#ifdef ESOCKTNOSUPPORT
    case ESOCKTNOSUPPORT: return "Socket type not supported";
#endif
#ifdef ESPIPE
    case ESPIPE: return "Invalid seek";
#endif
#ifdef ESRCH
    case ESRCH: return "No such process";
#endif
#ifdef ESRMNT
    case ESRMNT: return "Srmount error";
#endif
#ifdef ESTALE
    case ESTALE: return "Stale remote file handle";
#endif
#ifdef ESUCCESS
    case ESUCCESS: return "Error 0";
#endif
#if defined(ETIME) && (!defined(ELOOP) || (ETIME != ELOOP))
    case ETIME: return "Timer expired";
#endif
#if defined(ETIMEDOUT) && (!defined(ENOSTR) || (ETIMEDOUT != ENOSTR))
    case ETIMEDOUT: return "Connection timed out";
#endif
#ifdef ETOOMANYREFS
    case ETOOMANYREFS: return "Too many references: cannot splice";
#endif
#ifdef ETXTBSY
    case ETXTBSY: return "Text file busy";
#endif
#ifdef EUCLEAN
    case EUCLEAN: return "Structure needs cleaning";
#endif
#ifdef EUNATCH
    case EUNATCH: return "Protocol driver not attached";
#endif
#ifdef EUSERS
    case EUSERS: return "Too many users";
#endif
#ifdef EVERSION
    case EVERSION: return "Version mismatch";
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
    case EWOULDBLOCK: return "Operation would block";
#endif
#ifdef EXDEV
    case EXDEV: return "Cross-domain link";
#endif
#ifdef EXFULL
    case EXFULL: return "Message tables full";
#endif
    default:
#ifdef NO_STRERROR
	return "Unknown POSIX error";
#else
	return strerror(err);
#endif
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SignalId --
 *
 *	Return a textual identifier for a signal number.
 *
 * Results:
 *	This procedure returns a machine-readable textual identifier that
 *	corresponds to sig.  The identifier is the same as the #define name in
 *	signal.h.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const char *
Tcl_SignalId(
     int sig)			/* Number of signal. */
{
    switch (sig) {
#ifdef SIGABRT
    case SIGABRT: return "SIGABRT";
#endif
#ifdef SIGALRM
    case SIGALRM: return "SIGALRM";
#endif
#ifdef SIGBUS
    case SIGBUS: return "SIGBUS";
#endif
#ifdef SIGCHLD
    case SIGCHLD: return "SIGCHLD";
#endif
#if defined(SIGCLD) && (!defined(SIGCHLD) || (SIGCLD != SIGCHLD))
    case SIGCLD: return "SIGCLD";
#endif
#ifdef SIGCONT
    case SIGCONT: return "SIGCONT";
#endif
#if defined(SIGEMT) && (!defined(SIGXCPU) || (SIGEMT != SIGXCPU))
    case SIGEMT: return "SIGEMT";
#endif
#ifdef SIGFPE
    case SIGFPE: return "SIGFPE";
#endif
#ifdef SIGHUP
    case SIGHUP: return "SIGHUP";
#endif
#ifdef SIGILL
    case SIGILL: return "SIGILL";
#endif
#ifdef SIGINT
    case SIGINT: return "SIGINT";
#endif
#ifdef SIGIO
    case SIGIO: return "SIGIO";
#endif
#if defined(SIGIOT) && (!defined(SIGABRT) || (SIGIOT != SIGABRT))
    case SIGIOT: return "SIGIOT";
#endif
#ifdef SIGKILL
    case SIGKILL: return "SIGKILL";
#endif
#if defined(SIGLOST) && (!defined(SIGIOT) || (SIGLOST != SIGIOT)) && (!defined(SIGURG) || (SIGLOST != SIGURG)) && (!defined(SIGPROF) || (SIGLOST != SIGPROF)) && (!defined(SIGIO) || (SIGLOST != SIGIO))
    case SIGLOST: return "SIGLOST";
#endif
#ifdef SIGPIPE
    case SIGPIPE: return "SIGPIPE";
#endif
#if defined(SIGPOLL) && (!defined(SIGIO) || (SIGPOLL != SIGIO))
    case SIGPOLL: return "SIGPOLL";
#endif
#ifdef SIGPROF
    case SIGPROF: return "SIGPROF";
#endif
#if defined(SIGPWR) && (!defined(SIGXFSZ) || (SIGPWR != SIGXFSZ)) && (!defined(SIGLOST) || (SIGPWR != SIGLOST))
    case SIGPWR: return "SIGPWR";
#endif
#ifdef SIGQUIT
    case SIGQUIT: return "SIGQUIT";
#endif
#if defined(SIGSEGV) && (!defined(SIGBUS) || (SIGSEGV != SIGBUS))
    case SIGSEGV: return "SIGSEGV";
#endif
#ifdef SIGSTOP
    case SIGSTOP: return "SIGSTOP";
#endif
#ifdef SIGSYS
    case SIGSYS: return "SIGSYS";
#endif
#ifdef SIGTERM
    case SIGTERM: return "SIGTERM";
#endif
#ifdef SIGTRAP
    case SIGTRAP: return "SIGTRAP";
#endif
#ifdef SIGTSTP
    case SIGTSTP: return "SIGTSTP";
#endif
#ifdef SIGTTIN
    case SIGTTIN: return "SIGTTIN";
#endif
#ifdef SIGTTOU
    case SIGTTOU: return "SIGTTOU";
#endif
#if defined(SIGURG) && (!defined(SIGIO) || (SIGURG != SIGIO))
    case SIGURG: return "SIGURG";
#endif
#if defined(SIGUSR1) && (!defined(SIGIO) || (SIGUSR1 != SIGIO))
    case SIGUSR1: return "SIGUSR1";
#endif
#if defined(SIGUSR2) && (!defined(SIGURG) || (SIGUSR2 != SIGURG))
    case SIGUSR2: return "SIGUSR2";
#endif
#ifdef SIGVTALRM
    case SIGVTALRM: return "SIGVTALRM";
#endif
#ifdef SIGWINCH
    case SIGWINCH: return "SIGWINCH";
#endif
#ifdef SIGXCPU
    case SIGXCPU: return "SIGXCPU";
#endif
#ifdef SIGXFSZ
    case SIGXFSZ: return "SIGXFSZ";
#endif
#if defined(SIGINFO) && (!defined(SIGPWR) || (SIGINFO != SIGPWR))
    case SIGINFO: return "SIGINFO";
#endif
    }
    return "unknown signal";
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SignalMsg --
 *
 *	Return a human-readable message describing a signal.
 *
 * Results:
 *	This procedure returns a string describing sig that should make sense
 *	to a human.  It may not be easy for a machine to parse.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const char *
Tcl_SignalMsg(
     int sig)			/* Number of signal. */
{
    switch (sig) {
#ifdef SIGABRT
    case SIGABRT: return "SIGABRT";
#endif
#ifdef SIGALRM
    case SIGALRM: return "alarm clock";
#endif
#ifdef SIGBUS
    case SIGBUS: return "bus error";
#endif
#ifdef SIGCHLD
    case SIGCHLD: return "child status changed";
#endif
#if defined(SIGCLD) && (!defined(SIGCHLD) || (SIGCLD != SIGCHLD))
    case SIGCLD: return "child status changed";
#endif
#ifdef SIGCONT
    case SIGCONT: return "continue after stop";
#endif
#if defined(SIGEMT) && (!defined(SIGXCPU) || (SIGEMT != SIGXCPU))
    case SIGEMT: return "EMT instruction";
#endif
#ifdef SIGFPE
    case SIGFPE: return "floating-point exception";
#endif
#ifdef SIGHUP
    case SIGHUP: return "hangup";
#endif
#ifdef SIGILL
    case SIGILL: return "illegal instruction";
#endif
#ifdef SIGINT
    case SIGINT: return "interrupt";
#endif
#ifdef SIGIO
    case SIGIO: return "input/output possible on file";
#endif
#if defined(SIGIOT) && (!defined(SIGABRT) || (SIGABRT != SIGIOT))
    case SIGIOT: return "IOT instruction";
#endif
#ifdef SIGKILL
    case SIGKILL: return "kill signal";
#endif
#if defined(SIGLOST) && (!defined(SIGIOT) || (SIGLOST != SIGIOT)) && (!defined(SIGURG) || (SIGLOST != SIGURG)) && (!defined(SIGPROF) || (SIGLOST != SIGPROF)) && (!defined(SIGIO) || (SIGLOST != SIGIO))
    case SIGLOST: return "resource lost";
#endif
#ifdef SIGPIPE
    case SIGPIPE: return "write on pipe with no readers";
#endif
#if defined(SIGPOLL) && (!defined(SIGIO) || (SIGPOLL != SIGIO))
    case SIGPOLL: return "input/output possible on file";
#endif
#ifdef SIGPROF
    case SIGPROF: return "profiling alarm";
#endif
#if defined(SIGPWR) && (!defined(SIGXFSZ) || (SIGPWR != SIGXFSZ)) && (!defined(SIGLOST) || (SIGPWR != SIGLOST))
    case SIGPWR: return "power-fail restart";
#endif
#ifdef SIGQUIT
    case SIGQUIT: return "quit signal";
#endif
#if defined(SIGSEGV) && (!defined(SIGBUS) || (SIGSEGV != SIGBUS))
    case SIGSEGV: return "segmentation violation";
#endif
#ifdef SIGSTOP
    case SIGSTOP: return "stop";
#endif
#ifdef SIGSYS
    case SIGSYS: return "bad argument to system call";
#endif
#ifdef SIGTERM
    case SIGTERM: return "software termination signal";
#endif
#ifdef SIGTRAP
    case SIGTRAP: return "trace trap";
#endif
#ifdef SIGTSTP
    case SIGTSTP: return "stop signal from tty";
#endif
#ifdef SIGTTIN
    case SIGTTIN: return "background tty read";
#endif
#ifdef SIGTTOU
    case SIGTTOU: return "background tty write";
#endif
#if defined(SIGURG) && (!defined(SIGIO) || (SIGURG != SIGIO))
    case SIGURG: return "urgent I/O condition";
#endif
#if defined(SIGUSR1) && (!defined(SIGIO) || (SIGUSR1 != SIGIO))
    case SIGUSR1: return "user-defined signal 1";
#endif
#if defined(SIGUSR2) && (!defined(SIGURG) || (SIGUSR2 != SIGURG))
    case SIGUSR2: return "user-defined signal 2";
#endif
#ifdef SIGVTALRM
    case SIGVTALRM: return "virtual time alarm";
#endif
#ifdef SIGWINCH
    case SIGWINCH: return "window changed";
#endif
#ifdef SIGXCPU
    case SIGXCPU: return "exceeded CPU time limit";
#endif
#ifdef SIGXFSZ
    case SIGXFSZ: return "exceeded file size limit";
#endif
#if defined(SIGINFO) && (!defined(SIGPWR) || (SIGINFO != SIGPWR))
    case SIGINFO: return "information request";
#endif
    }
    return "unknown signal";
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
