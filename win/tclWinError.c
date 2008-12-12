/*
 * tclWinError.c --
 *
 *	This file contains code for converting from Win32 errors to errno
 *	errors.
 *
 * Copyright (c) 1995-1996 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclWinError.c,v 1.7.12.5 2008/12/12 05:26:52 davygrvy Exp $
 */

#include "tclInt.h"

/*
 * The following table contains the mapping from Win32 errors to errno errors.
 */

static char errorTable[] = {
    0,
    EINVAL,	/* ERROR_INVALID_FUNCTION	1 */
    ENOENT,	/* ERROR_FILE_NOT_FOUND		2 */
    ENOENT,	/* ERROR_PATH_NOT_FOUND		3 */
    EMFILE,	/* ERROR_TOO_MANY_OPEN_FILES	4 */
    EACCES,	/* ERROR_ACCESS_DENIED		5 */
    EBADF,	/* ERROR_INVALID_HANDLE		6 */
    ENOMEM,	/* ERROR_ARENA_TRASHED		7 */
    ENOMEM,	/* ERROR_NOT_ENOUGH_MEMORY	8 */
    ENOMEM,	/* ERROR_INVALID_BLOCK		9 */
    E2BIG,	/* ERROR_BAD_ENVIRONMENT	10 */
    ENOEXEC,	/* ERROR_BAD_FORMAT		11 */
    EACCES,	/* ERROR_INVALID_ACCESS		12 */
    EINVAL,	/* ERROR_INVALID_DATA		13 */
    EFAULT,	/* ERROR_OUT_OF_MEMORY		14 */
    ENOENT,	/* ERROR_INVALID_DRIVE		15 */
    EACCES,	/* ERROR_CURRENT_DIRECTORY	16 */
    EXDEV,	/* ERROR_NOT_SAME_DEVICE	17 */
    ENOENT,	/* ERROR_NO_MORE_FILES		18 */
    EROFS,	/* ERROR_WRITE_PROTECT		19 */
    ENXIO,	/* ERROR_BAD_UNIT		20 */
    EBUSY,	/* ERROR_NOT_READY		21 */
    EIO,	/* ERROR_BAD_COMMAND		22 */
    EIO,	/* ERROR_CRC			23 */
    EIO,	/* ERROR_BAD_LENGTH		24 */
    EIO,	/* ERROR_SEEK			25 */
    EIO,	/* ERROR_NOT_DOS_DISK		26 */
    ENXIO,	/* ERROR_SECTOR_NOT_FOUND	27 */
    EBUSY,	/* ERROR_OUT_OF_PAPER		28 */
    EIO,	/* ERROR_WRITE_FAULT		29 */
    EIO,	/* ERROR_READ_FAULT		30 */
    EIO,	/* ERROR_GEN_FAILURE		31 */
    EACCES,	/* ERROR_SHARING_VIOLATION	32 */
    EACCES,	/* ERROR_LOCK_VIOLATION		33 */
    ENXIO,	/* ERROR_WRONG_DISK		34 */
    ENFILE,	/* ERROR_FCB_UNAVAILABLE	35 */
    ENFILE,	/* ERROR_SHARING_BUFFER_EXCEEDED	36 */
    EINVAL,	/* 37 */
    EINVAL,	/* 38 */
    ENOSPC,	/* ERROR_HANDLE_DISK_FULL	39 */
    EINVAL,	/* 40 */
    EINVAL,	/* 41 */
    EINVAL,	/* 42 */
    EINVAL,	/* 43 */
    EINVAL,	/* 44 */
    EINVAL,	/* 45 */
    EINVAL,	/* 46 */
    EINVAL,	/* 47 */
    EINVAL,	/* 48 */
    EINVAL,	/* 49 */
    ENODEV,	/* ERROR_NOT_SUPPORTED		50 */
    EBUSY,	/* ERROR_REM_NOT_LIST		51 */
    EEXIST,	/* ERROR_DUP_NAME		52 */
    ENOENT,	/* ERROR_BAD_NETPATH		53 */
    EBUSY,	/* ERROR_NETWORK_BUSY		54 */
    ENODEV,	/* ERROR_DEV_NOT_EXIST		55 */
    EAGAIN,	/* ERROR_TOO_MANY_CMDS		56 */
    EIO,	/* ERROR_ADAP_HDW_ERR		57 */
    EIO,	/* ERROR_BAD_NET_RESP		58 */
    EIO,	/* ERROR_UNEXP_NET_ERR		59 */
    EINVAL,	/* ERROR_BAD_REM_ADAP		60 */
    EFBIG,	/* ERROR_PRINTQ_FULL		61 */
    ENOSPC,	/* ERROR_NO_SPOOL_SPACE		62 */
    ENOENT,	/* ERROR_PRINT_CANCELLED	63 */
    ENOENT,	/* ERROR_NETNAME_DELETED	64 */
    EACCES,	/* ERROR_NETWORK_ACCESS_DENIED	65 */
    ENODEV,	/* ERROR_BAD_DEV_TYPE		66 */
    ENOENT,	/* ERROR_BAD_NET_NAME		67 */
    ENFILE,	/* ERROR_TOO_MANY_NAMES		68 */
    EIO,	/* ERROR_TOO_MANY_SESS		69 */
    EAGAIN,	/* ERROR_SHARING_PAUSED		70 */
    EINVAL,	/* ERROR_REQ_NOT_ACCEP		71 */
    EAGAIN,	/* ERROR_REDIR_PAUSED		72 */
    EINVAL,	/* 73 */
    EINVAL,	/* 74 */
    EINVAL,	/* 75 */
    EINVAL,	/* 76 */
    EINVAL,	/* 77 */
    EINVAL,	/* 78 */
    EINVAL,	/* 79 */
    EEXIST,	/* ERROR_FILE_EXISTS		80 */
    EINVAL,	/* 81 */
    ENOSPC,	/* ERROR_CANNOT_MAKE		82 */
    EIO,	/* ERROR_FAIL_I24		83 */
    ENFILE,	/* ERROR_OUT_OF_STRUCTURES	84 */
    EEXIST,	/* ERROR_ALREADY_ASSIGNED	85 */
    EPERM,	/* ERROR_INVALID_PASSWORD	86 */
    EINVAL,	/* ERROR_INVALID_PARAMETER	87 */
    EIO,	/* ERROR_NET_WRITE_FAULT	88 */
    EAGAIN,	/* ERROR_NO_PROC_SLOTS		89 */
    EINVAL,	/* 90 */
    EINVAL,	/* 91 */
    EINVAL,	/* 92 */
    EINVAL,	/* 93 */
    EINVAL,	/* 94 */
    EINVAL,	/* 95 */
    EINVAL,	/* 96 */
    EINVAL,	/* 97 */
    EINVAL,	/* 98 */
    EINVAL,	/* 99 */
    EINVAL,	/* 100 */
    EINVAL,	/* 101 */
    EINVAL,	/* 102 */
    EINVAL,	/* 103 */
    EINVAL,	/* 104 */
    EINVAL,	/* 105 */
    EINVAL,	/* 106 */
    EXDEV,	/* ERROR_DISK_CHANGE		107 */
    EAGAIN,	/* ERROR_DRIVE_LOCKED		108 */
    EPIPE,	/* ERROR_BROKEN_PIPE		109 */
    ENOENT,	/* ERROR_OPEN_FAILED		110 */
    EINVAL,	/* ERROR_BUFFER_OVERFLOW	111 */
    ENOSPC,	/* ERROR_DISK_FULL		112 */
    EMFILE,	/* ERROR_NO_MORE_SEARCH_HANDLES	113 */
    EBADF,	/* ERROR_INVALID_TARGET_HANDLE	114 */
    EFAULT,	/* ERROR_PROTECTION_VIOLATION	115 */
    EINVAL,	/* 116 */
    EINVAL,	/* 117 */
    EINVAL,	/* 118 */
    EINVAL,	/* 119 */
    EINVAL,	/* 120 */
    EINVAL,	/* 121 */
    EINVAL,	/* 122 */
    ENOENT,	/* ERROR_INVALID_NAME		123 */
    EINVAL,	/* 124 */
    EINVAL,	/* 125 */
    EINVAL,	/* 126 */
    EINVAL,	/* ERROR_PROC_NOT_FOUND		127 */
    ECHILD,	/* ERROR_WAIT_NO_CHILDREN	128 */
    ECHILD,	/* ERROR_CHILD_NOT_COMPLETE	129 */
    EBADF,	/* ERROR_DIRECT_ACCESS_HANDLE	130 */
    EINVAL,	/* ERROR_NEGATIVE_SEEK		131 */
    ESPIPE,	/* ERROR_SEEK_ON_DEVICE		132 */
    EINVAL,	/* 133 */
    EINVAL,	/* 134 */
    EINVAL,	/* 135 */
    EINVAL,	/* 136 */
    EINVAL,	/* 137 */
    EINVAL,	/* 138 */
    EINVAL,	/* 139 */
    EINVAL,	/* 140 */
    EINVAL,	/* 141 */
    EAGAIN,	/* ERROR_BUSY_DRIVE		142 */
    EINVAL,	/* 143 */
    EINVAL,	/* 144 */
    EEXIST,	/* ERROR_DIR_NOT_EMPTY		145 */
    EINVAL,	/* 146 */
    EINVAL,	/* 147 */
    EINVAL,	/* 148 */
    EINVAL,	/* 149 */
    EINVAL,	/* 150 */
    EINVAL,	/* 151 */
    EINVAL,	/* 152 */
    EINVAL,	/* 153 */
    EINVAL,	/* 154 */
    EINVAL,	/* 155 */
    EINVAL,	/* 156 */
    EINVAL,	/* 157 */
    EACCES,	/* ERROR_NOT_LOCKED		158 */
    EINVAL,	/* 159 */
    EINVAL,	/* 160 */
    ENOENT,	/* ERROR_BAD_PATHNAME	        161 */
    EINVAL,	/* 162 */
    EINVAL,	/* 163 */
    EINVAL,	/* 164 */
    EINVAL,	/* 165 */
    EINVAL,	/* 166 */
    EACCES,	/* ERROR_LOCK_FAILED		167 */
    EINVAL,	/* 168 */
    EINVAL,	/* 169 */
    EINVAL,	/* 170 */
    EINVAL,	/* 171 */
    EINVAL,	/* 172 */
    EINVAL,	/* 173 */
    EINVAL,	/* 174 */
    EINVAL,	/* 175 */
    EINVAL,	/* 176 */
    EINVAL,	/* 177 */
    EINVAL,	/* 178 */
    EINVAL,	/* 179 */
    EINVAL,	/* 180 */
    EINVAL,	/* 181 */
    EINVAL,	/* 182 */
    EEXIST,	/* ERROR_ALREADY_EXISTS		183 */
    ECHILD,	/* ERROR_NO_CHILD_PROCESS	184 */
    EINVAL,	/* 185 */
    EINVAL,	/* 186 */
    EINVAL,	/* 187 */
    EINVAL,	/* 188 */
    EINVAL,	/* 189 */
    EINVAL,	/* 190 */
    EINVAL,	/* 191 */
    EINVAL,	/* 192 */
    EINVAL,	/* 193 */
    EINVAL,	/* 194 */
    EINVAL,	/* 195 */
    EINVAL,	/* 196 */
    EINVAL,	/* 197 */
    EINVAL,	/* 198 */
    EINVAL,	/* 199 */
    EINVAL,	/* 200 */
    EINVAL,	/* 201 */
    EINVAL,	/* 202 */
    EINVAL,	/* 203 */
    EINVAL,	/* 204 */
    EINVAL,	/* 205 */
    ENAMETOOLONG,/* ERROR_FILENAME_EXCED_RANGE	206 */
    EINVAL,	/* 207 */
    EINVAL,	/* 208 */
    EINVAL,	/* 209 */
    EINVAL,	/* 210 */
    EINVAL,	/* 211 */
    EINVAL,	/* 212 */
    EINVAL,	/* 213 */
    EINVAL,	/* 214 */
    EINVAL,	/* 215 */
    EINVAL,	/* 216 */
    EINVAL,	/* 217 */
    EINVAL,	/* 218 */
    EINVAL,	/* 219 */
    EINVAL,	/* 220 */
    EINVAL,	/* 221 */
    EINVAL,	/* 222 */
    EINVAL,	/* 223 */
    EINVAL,	/* 224 */
    EINVAL,	/* 225 */
    EINVAL,	/* 226 */
    EINVAL,	/* 227 */
    EINVAL,	/* 228 */
    EINVAL,	/* 229 */
    EPIPE,	/* ERROR_BAD_PIPE		230 */
    EAGAIN,	/* ERROR_PIPE_BUSY		231 */
    EPIPE,	/* ERROR_NO_DATA		232 */
    EPIPE,	/* ERROR_PIPE_NOT_CONNECTED	233 */
    EINVAL,	/* 234 */
    EINVAL,	/* 235 */
    EINVAL,	/* 236 */
    EINVAL,	/* 237 */
    EINVAL,	/* 238 */
    EINVAL,	/* 239 */
    EINVAL,	/* 240 */
    EINVAL,	/* 241 */
    EINVAL,	/* 242 */
    EINVAL,	/* 243 */
    EINVAL,	/* 244 */
    EINVAL,	/* 245 */
    EINVAL,	/* 246 */
    EINVAL,	/* 247 */
    EINVAL,	/* 248 */
    EINVAL,	/* 249 */
    EINVAL,	/* 250 */
    EINVAL,	/* 251 */
    EINVAL,	/* 252 */
    EINVAL,	/* 253 */
    EINVAL,	/* 254 */
    EINVAL,	/* 255 */
    EINVAL,	/* 256 */
    EINVAL,	/* 257 */
    EINVAL,	/* 258 */
    EINVAL,	/* 259 */
    EINVAL,	/* 260 */
    EINVAL,	/* 261 */
    EINVAL,	/* 262 */
    EINVAL,	/* 263 */
    EINVAL,	/* 264 */
    EINVAL,	/* 265 */
    EINVAL,	/* 266 */
    ENOTDIR,	/* ERROR_DIRECTORY		267 */
};

static const unsigned int tableLen = sizeof(errorTable);

/*
 * The following tables contains the mapping from Winsock errors to
 * errno errors.
 */

static int wsaErrorTable1[] = {
    EINTR,		/* WSAEINTR	    Interrupted system call. */
    EINVAL,
    EINVAL,
    EINVAL,
    EINVAL,
    EBADF,		/* WSAEBADF	    Bad file number. */
    EINVAL,
    EINVAL,
    EINVAL,
    EACCES,		/* WSAEACCES	    Permission denied. */
    EFAULT,		/* WSAEFAULT	    Bad data address. */
    EINVAL,
    EINVAL,
    EINVAL,
    EINVAL,
    EINVAL,
    EINVAL,
    EINVAL,
    EINVAL,		/* WSAEINVAL	    Invalid argument. */
    EINVAL,
    EMFILE		/* WSAEMFILE	    Too many open files. */
};


static int wsaErrorTable2[] = {
    EWOULDBLOCK,	/* WSAEWOULDBLOCK   Operation would block. */
    EINPROGRESS,	/* WSAEINPROGRESS   Operation now in progress. */
    EALREADY,		/* WSAEALREADY	    Operation already in progress. */
    ENOTSOCK,		/* WSAENOTSOCK	    Socket operation on nonsocket. */
    EDESTADDRREQ,	/* WSAEDESTADDRREQ  Destination address required. */
    EMSGSIZE,		/* WSAEMSGSIZE	    Message too long. */
    EPROTOTYPE,		/* WSAEPROTOTYPE    Protocol wrong type for socket. */
    ENOPROTOOPT,	/* WSAENOPROTOOPT   Protocol not available. */
    EPROTONOSUPPORT,	/* WSAEPROTONOSUPPORT Protocol not supported. */
    ESOCKTNOSUPPORT,	/* WSAESOCKTNOSUPPORT Socket type not supported. */
    EOPNOTSUPP,		/* WSAEOPNOTSUPP    Operation not supported on socket. */
    EPFNOSUPPORT,	/* WSAEPFNOSUPPORT  Protocol family not supported. */
    EAFNOSUPPORT,	/* WSAEAFNOSUPPORT  Address family not supported by protocol family. */
    EADDRINUSE,		/* WSAEADDRINUSE    Address already in use. */
    EADDRNOTAVAIL,	/* WSAEADDRNOTAVAIL Cannot assign requested address. */
    ENETDOWN,		/* WSAENETDOWN	    Network is down. */
    ENETUNREACH,	/* WSAENETUNREACH   Network is unreachable. */
    ENETRESET,		/* WSAENETRESET	    Network dropped connection on reset. */
    ECONNABORTED,	/* WSAECONNABORTED  Software caused connection abort. */
    ECONNRESET,		/* WSAECONNRESET    Connection reset by peer. */
    ENOBUFS,		/* WSAENOBUFS	    No buffer space available. */
    EISCONN,		/* WSAEISCONN	    Socket is already connected. */
    ENOTCONN,		/* WSAENOTCONN	    Socket is not connected. */
    ESHUTDOWN,		/* WSAESHUTDOWN	    Cannot send after socket shutdown. */
    ETOOMANYREFS,	/* WSAETOOMANYREFS  Too many references: cannot splice. */
    ETIMEDOUT,		/* WSAETIMEDOUT	    Connection timed out. */
    ECONNREFUSED,	/* WSAECONNREFUSED  Connection refused. */
    ELOOP,		/* WSAELOOP	    Too many levels of symbolic links. */
    ENAMETOOLONG,	/* WSAENAMETOOLONG  File name too long. */
    EHOSTDOWN,		/* WSAEHOSTDOWN	    Host is down. */
    EHOSTUNREACH,	/* WSAEHOSTUNREACH  No route to host. */
    ENOTEMPTY,		/* WSAENOTEMPTY	    Directory is not empty. */
    EAGAIN,		/* WSAEPROCLIM	    Too many processes. */
    EUSERS,		/* WSAEUSERS	    Too many users. */
    EDQUOT,		/* WSAEDQUOT	    Ran out of disk quota. */
    ESTALE,		/* WSAESTALE	    File handle reference is no longer available. */
    EREMOTE,		/* WSAEREMOTE	    Item is not available locally. */
};

/*
 * These error codes are very windows specific and have no POSIX
 * translation, yet.
 *
 * TODO: Fixme!
 */

static int wsaErrorTable3[] = {
    EINVAL,		/* WSASYSNOTREADY	    WSAStartup cannot function at this time because the underlying system it uses to provide network services is currently unavailable. */
    EINVAL,		/* WSAVERNOTSUPPORTED	    The Windows Sockets version requested is not supported. */
    EINVAL,		/* WSANOTINITIALISED	    Either the application has not called WSAStartup, or WSAStartup failed. */
    EINVAL,
    EINVAL,
    EINVAL,
    EINVAL,
    EINVAL,
    EINVAL,
    EINVAL,
    ENOTCONN,		/* WSAEDISCON		    Returned by WSARecv or WSARecvFrom to indicate the remote party has initiated a graceful shutdown sequence. */
    EINVAL,		/* WSAENOMORE		    No more results can be returned by WSALookupServiceNext. */
    EINVAL,		/* WSAECANCELLED	    A call to WSALookupServiceEnd was made while this call was still processing. The call has been canceled. */
    EINVAL,		/* WSAEINVALIDPROCTABLE	    The procedure call table is invalid. */
    EINVAL,		/* WSAEINVALIDPROVIDER	    The requested service provider is invalid. */
    EINVAL,		/* WSAEPROVIDERFAILEDINIT   The requested service provider could not be loaded or initialized. */
    EINVAL,		/* WSASYSCALLFAILURE	    A system call that should never fail has failed. */
    EINVAL,		/* WSASERVICE_NOT_FOUND	    No such service is known. The service cannot be found in the specified name space. */
    EINVAL,		/* WSATYPE_NOT_FOUND	    The specified class was not found. */
    EINVAL,		/* WSA_E_NO_MORE	    No more results can be returned by WSALookupServiceNext. */
    EINVAL,		/* WSA_E_CANCELLED	    A call to WSALookupServiceEnd was made while this call was still processing. The call has been canceled. */
    EINVAL,		/* WSAEREFUSED		    A database query failed because it was actively refused. */
};

/*
 * These error codes are very windows specific and have no POSIX
 * translation.  Actually, the first four map to h_errno from BSD's
 * netdb.h, but h_errno has no map either to POSIX.  So we're stuck.
 *
 * TODO: Fixme!
 */

static int wsaErrorTable4[] = {
    EINVAL,	/* WSAHOST_NOT_FOUND,		Authoritative Answer: Host not found */
    EINVAL,	/* WSATRY_AGAIN,		Non-Authoritative: Host not found, or SERVERFAIL */
    EINVAL,	/* WSANO_RECOVERY,		Non-recoverable errors, FORMERR, REFUSED, NOTIMP */
    EINVAL,	/* WSANO_DATA,			Valid name, no data record of requested type */
    EINVAL,	/* WSA_QOS_RECEIVERS,		at least one Reserve has arrived */
    EINVAL,	/* WSA_QOS_SENDERS,		at least one Path has arrived */
    EINVAL,	/* WSA_QOS_NO_SENDERS,		there are no senders */
    EINVAL,	/* WSA_QOS_NO_RECEIVERS,	there are no receivers */
    EINVAL,	/* WSA_QOS_REQUEST_CONFIRMED,	Reserve has been confirmed */
    EINVAL,	/* WSA_QOS_ADMISSION_FAILURE,	error due to lack of resources */
    EINVAL,	/* WSA_QOS_POLICY_FAILURE,	rejected for administrative reasons - bad credentials */
    EINVAL,	/* WSA_QOS_BAD_STYLE,		unknown or conflicting style */
    EINVAL,	/* WSA_QOS_BAD_OBJECT,		problem with some part of the filterspec or providerspecific buffer in general */
    EINVAL,	/* WSA_QOS_TRAFFIC_CTRL_ERROR,	problem with some part of the flowspec */
    EINVAL,	/* WSA_QOS_GENERIC_ERROR,	general error */
    EINVAL,	/* WSA_QOS_ESERVICETYPE,	invalid service type in flowspec */
    EINVAL,	/* WSA_QOS_EFLOWSPEC,		invalid flowspec */
    EINVAL,	/* WSA_QOS_EPROVSPECBUF,	invalid provider specific buffer */
    EINVAL,	/* WSA_QOS_EFILTERSTYLE,	invalid filter style */
    EINVAL,	/* WSA_QOS_EFILTERTYPE,		invalid filter type */
    EINVAL,	/* WSA_QOS_EFILTERCOUNT,	incorrect number of filters */
    EINVAL,	/* WSA_QOS_EOBJLENGTH,		invalid object length */
    EINVAL,	/* WSA_QOS_EFLOWCOUNT,		incorrect number of flows */
    EINVAL,	/* WSA_QOS_EUNKOWNPSOBJ,	unknown object in provider specific buffer */
    EINVAL,	/* WSA_QOS_EPOLICYOBJ,		invalid policy object in provider specific buffer */
    EINVAL,	/* WSA_QOS_EFLOWDESC,		invalid flow descriptor in the list */
    EINVAL,	/* WSA_QOS_EPSFLOWSPEC,		inconsistent flow spec in provider specific buffer */
    EINVAL,	/* WSA_QOS_EPSFILTERSPEC,	invalid filter spec in provider specific buffer */
    EINVAL,	/* WSA_QOS_ESDMODEOBJ,		invalid shape discard mode object in provider specific buffer */
    EINVAL,	/* WSA_QOS_ESHAPERATEOBJ,	invalid shaping rate object in provider specific buffer */
    EINVAL,	/* WSA_QOS_RESERVED_PETYPE,	reserved policy element in provider specific buffer */
};

/*
 *----------------------------------------------------------------------
 *
 * TclWinConvertError --
 *
 *	This routine converts a Win32 error into an errno value
 *	if the conversion is possible, else EINVAL.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the errno global variable.  Doesn't translate well
 *	and results in some info loss.
 *
 *----------------------------------------------------------------------
 */

void
TclWinConvertError(
    DWORD errCode)		/* Win32 error code. */
{
    if (errCode >= tableLen) {
	Tcl_SetErrno(EINVAL);
    } else {
	Tcl_SetErrno(errorTable[errCode]);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinConvertWSAError --
 *
 *	This routine converts a WinSock error into a POSIX errno value
 *	if the conversion is possible, else EINVAL.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the errno global variable.  Doesn't translate well
 *	and results in some info loss.
 *
 *----------------------------------------------------------------------
 */

void
TclWinConvertWSAError(
    DWORD errCode)		/* Winsock error code. */
{
    if ((errCode >= WSAEINTR) && (errCode <= WSAEMFILE)) {
	Tcl_SetErrno(wsaErrorTable1[errCode - WSAEINTR]);
    } else if ((errCode >= WSAEWOULDBLOCK) && (errCode <= WSAEREMOTE)) {
	Tcl_SetErrno(wsaErrorTable2[errCode - WSAEWOULDBLOCK]);
    } else if ((errCode >= WSASYSNOTREADY) && (errCode <= WSAEREFUSED)) {
	Tcl_SetErrno(wsaErrorTable3[errCode - WSASYSNOTREADY]);
    } else if ((errCode >= WSAHOST_NOT_FOUND) && (errCode <= WSA_QOS_RESERVED_PETYPE)) {
	Tcl_SetErrno(wsaErrorTable4[errCode - WSAHOST_NOT_FOUND]);
    } else {
	Tcl_SetErrno(EINVAL);
    }
}
