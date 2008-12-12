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
 * RCS: @(#) $Id: tclWinError.c,v 1.7.12.6 2008/12/12 20:28:46 davygrvy Exp $
 */

#include "tclInt.h"

/* ISO hack for dumb VC++ */
#ifdef _MSC_VER
#define   snprintf	_snprintf
#endif

#define ERR_BUF_SIZE	1024
typedef struct ThreadSpecificData {
    char sysMsgSpace[ERR_BUF_SIZE];
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;

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

/*
 * Some earlier <winerror.h> header files do not contain all the
 * error codes of the newest releases.  Add them, here.  This allows
 * us to not require the latest/newest Platform SDK to build Tcl.
 */

#ifndef ERROR_EXE_CANNOT_MODIFY_SIGNED_BINARY
#define ERROR_EXE_CANNOT_MODIFY_SIGNED_BINARY 217L
#endif
#ifndef ERROR_EXE_CANNOT_MODIFY_STRONG_SIGNED_BINARY
#define ERROR_EXE_CANNOT_MODIFY_STRONG_SIGNED_BINARY 218L
#endif
#ifndef ERROR_OPLOCK_NOT_GRANTED
#define ERROR_OPLOCK_NOT_GRANTED         300L
#endif
#ifndef ERROR_INVALID_OPLOCK_PROTOCOL
#define ERROR_INVALID_OPLOCK_PROTOCOL    301L
#endif
#ifndef ERROR_DISK_TOO_FRAGMENTED
#define ERROR_DISK_TOO_FRAGMENTED        302L
#endif
#ifndef ERROR_DELETE_PENDING
#define ERROR_DELETE_PENDING             303L
#endif
#ifndef ERROR_SCOPE_NOT_FOUND
#define ERROR_SCOPE_NOT_FOUND            318L
#endif
#ifndef ERROR_CANNOT_DETECT_DRIVER_FAILURE
#define ERROR_CANNOT_DETECT_DRIVER_FAILURE 1080L
#endif
#ifndef ERROR_CANNOT_DETECT_PROCESS_ABORT
#define ERROR_CANNOT_DETECT_PROCESS_ABORT 1081L
#endif
#ifndef ERROR_NO_RECOVERY_PROGRAM
#define ERROR_NO_RECOVERY_PROGRAM        1082L
#endif
#ifndef ERROR_SERVICE_NOT_IN_EXE
#define ERROR_SERVICE_NOT_IN_EXE         1083L
#endif
#ifndef ERROR_NOT_SAFEBOOT_SERVICE
#define ERROR_NOT_SAFEBOOT_SERVICE       1084L
#endif
#ifndef ERROR_NO_MORE_USER_HANDLES
#define ERROR_NO_MORE_USER_HANDLES       1158L
#endif
#ifndef ERROR_MESSAGE_SYNC_ONLY
#define ERROR_MESSAGE_SYNC_ONLY          1159L
#endif
#ifndef ERROR_SOURCE_ELEMENT_EMPTY
#define ERROR_SOURCE_ELEMENT_EMPTY       1160L
#endif
#ifndef ERROR_DESTINATION_ELEMENT_FULL
#define ERROR_DESTINATION_ELEMENT_FULL   1161L
#endif
#ifndef ERROR_ILLEGAL_ELEMENT_ADDRESS
#define ERROR_ILLEGAL_ELEMENT_ADDRESS    1162L
#endif
#ifndef ERROR_MAGAZINE_NOT_PRESENT
#define ERROR_MAGAZINE_NOT_PRESENT       1163L
#endif
#ifndef ERROR_DEVICE_REINITIALIZATION_NEEDED
#define ERROR_DEVICE_REINITIALIZATION_NEEDED 1164L
#endif
#ifndef ERROR_DEVICE_REQUIRES_CLEANING
#define ERROR_DEVICE_REQUIRES_CLEANING   1165L
#endif
#ifndef ERROR_DEVICE_DOOR_OPEN
#define ERROR_DEVICE_DOOR_OPEN           1166L
#endif
#ifndef ERROR_DEVICE_NOT_CONNECTED
#define ERROR_DEVICE_NOT_CONNECTED       1167L
#endif
#ifndef ERROR_NOT_FOUND
#define ERROR_NOT_FOUND                  1168L
#endif
#ifndef ERROR_NO_MATCH
#define ERROR_NO_MATCH                   1169L
#endif
#ifndef ERROR_SET_NOT_FOUND
#define ERROR_SET_NOT_FOUND              1170L
#endif
#ifndef ERROR_POINT_NOT_FOUND
#define ERROR_POINT_NOT_FOUND            1171L
#endif
#ifndef ERROR_NO_TRACKING_SERVICE
#define ERROR_NO_TRACKING_SERVICE        1172L
#endif
#ifndef ERROR_NO_VOLUME_ID
#define ERROR_NO_VOLUME_ID               1173L
#endif
#ifndef ERROR_UNABLE_TO_REMOVE_REPLACED
#define ERROR_UNABLE_TO_REMOVE_REPLACED  1175L
#endif
#ifndef ERROR_UNABLE_TO_MOVE_REPLACEMENT
#define ERROR_UNABLE_TO_MOVE_REPLACEMENT 1176L
#endif
#ifndef ERROR_UNABLE_TO_MOVE_REPLACEMENT_2
#define ERROR_UNABLE_TO_MOVE_REPLACEMENT_2 1177L
#endif
#ifndef ERROR_JOURNAL_DELETE_IN_PROGRESS
#define ERROR_JOURNAL_DELETE_IN_PROGRESS 1178L
#endif
#ifndef ERROR_JOURNAL_NOT_ACTIVE
#define ERROR_JOURNAL_NOT_ACTIVE         1179L
#endif
#ifndef ERROR_POTENTIAL_FILE_FOUND
#define ERROR_POTENTIAL_FILE_FOUND       1180L
#endif
#ifndef ERROR_JOURNAL_ENTRY_DELETED
#define ERROR_JOURNAL_ENTRY_DELETED      1181L
#endif
#ifndef ERROR_NO_SUCH_SITE
#define ERROR_NO_SUCH_SITE               1249L
#endif
#ifndef ERROR_DOMAIN_CONTROLLER_EXISTS
#define ERROR_DOMAIN_CONTROLLER_EXISTS   1250L
#endif
#ifndef ERROR_ONLY_IF_CONNECTED
#define ERROR_ONLY_IF_CONNECTED          1251L
#endif
#ifndef ERROR_OVERRIDE_NOCHANGES
#define ERROR_OVERRIDE_NOCHANGES         1252L
#endif
#ifndef ERROR_BAD_USER_PROFILE
#define ERROR_BAD_USER_PROFILE           1253L
#endif
#ifndef ERROR_NOT_SUPPORTED_ON_SBS
#define ERROR_NOT_SUPPORTED_ON_SBS       1254L
#endif
#ifndef ERROR_SERVER_SHUTDOWN_IN_PROGRESS
#define ERROR_SERVER_SHUTDOWN_IN_PROGRESS 1255L
#endif
#ifndef ERROR_HOST_DOWN
#define ERROR_HOST_DOWN                  1256L
#endif
#ifndef ERROR_NON_ACCOUNT_SID
#define ERROR_NON_ACCOUNT_SID            1257L
#endif
#ifndef ERROR_NON_DOMAIN_SID
#define ERROR_NON_DOMAIN_SID             1258L
#endif
#ifndef ERROR_APPHELP_BLOCK
#define ERROR_APPHELP_BLOCK              1259L
#endif
#ifndef ERROR_ACCESS_DISABLED_BY_POLICY
#define ERROR_ACCESS_DISABLED_BY_POLICY  1260L
#endif
#ifndef ERROR_REG_NAT_CONSUMPTION
#define ERROR_REG_NAT_CONSUMPTION        1261L
#endif
#ifndef ERROR_CSCSHARE_OFFLINE
#define ERROR_CSCSHARE_OFFLINE           1262L
#endif
#ifndef ERROR_PKINIT_FAILURE
#define ERROR_PKINIT_FAILURE             1263L
#endif
#ifndef ERROR_SMARTCARD_SUBSYSTEM_FAILURE
#define ERROR_SMARTCARD_SUBSYSTEM_FAILURE 1264L
#endif
#ifndef ERROR_DOWNGRADE_DETECTED
#define ERROR_DOWNGRADE_DETECTED         1265L
#endif
#ifndef ERROR_MACHINE_LOCKED
#define ERROR_MACHINE_LOCKED             1271L
#endif
#ifndef ERROR_CALLBACK_SUPPLIED_INVALID_DATA
#define ERROR_CALLBACK_SUPPLIED_INVALID_DATA 1273L
#endif
#ifndef ERROR_SYNC_FOREGROUND_REFRESH_REQUIRED
#define ERROR_SYNC_FOREGROUND_REFRESH_REQUIRED 1274L
#endif
#ifndef ERROR_DRIVER_BLOCKED
#define ERROR_DRIVER_BLOCKED             1275L
#endif
#ifndef ERROR_INVALID_IMPORT_OF_NON_DLL
#define ERROR_INVALID_IMPORT_OF_NON_DLL  1276L
#endif
#ifndef ERROR_ACCESS_DISABLED_WEBBLADE
#define ERROR_ACCESS_DISABLED_WEBBLADE   1277L
#endif
#ifndef ERROR_ACCESS_DISABLED_WEBBLADE_TAMPER
#define ERROR_ACCESS_DISABLED_WEBBLADE_TAMPER 1278L
#endif
#ifndef ERROR_RECOVERY_FAILURE
#define ERROR_RECOVERY_FAILURE           1279L
#endif
#ifndef ERROR_ALREADY_FIBER
#define ERROR_ALREADY_FIBER              1280L
#endif
#ifndef ERROR_ALREADY_THREAD
#define ERROR_ALREADY_THREAD             1281L
#endif
#ifndef ERROR_STACK_BUFFER_OVERRUN
#define ERROR_STACK_BUFFER_OVERRUN       1282L
#endif
#ifndef ERROR_PARAMETER_QUOTA_EXCEEDED
#define ERROR_PARAMETER_QUOTA_EXCEEDED   1283L
#endif
#ifndef ERROR_DEBUGGER_INACTIVE
#define ERROR_DEBUGGER_INACTIVE          1284L
#endif
#ifndef ERROR_DELAY_LOAD_FAILED
#define ERROR_DELAY_LOAD_FAILED          1285L
#endif
#ifndef ERROR_VDM_DISALLOWED
#define ERROR_VDM_DISALLOWED             1286L
#endif
#ifndef ERROR_UNIDENTIFIED_ERROR
#define ERROR_UNIDENTIFIED_ERROR         1287L
#endif
#ifndef ERROR_WRONG_TARGET_NAME
#define ERROR_WRONG_TARGET_NAME          1396L
#endif
#ifndef ERROR_MUTUAL_AUTH_FAILED
#define ERROR_MUTUAL_AUTH_FAILED         1397L
#endif
#ifndef ERROR_TIME_SKEW
#define ERROR_TIME_SKEW                  1398L
#endif
#ifndef ERROR_CURRENT_DOMAIN_NOT_ALLOWED
#define ERROR_CURRENT_DOMAIN_NOT_ALLOWED 1399L
#endif
#ifndef ERROR_INVALID_MONITOR_HANDLE
#define ERROR_INVALID_MONITOR_HANDLE     1461L
#endif
#ifndef ERROR_INSTALL_SERVICE_FAILURE
#define ERROR_INSTALL_SERVICE_FAILURE    1601L
#endif
#ifndef ERROR_INSTALL_USEREXIT
#define ERROR_INSTALL_USEREXIT           1602L
#endif
#ifndef ERROR_INSTALL_FAILURE
#define ERROR_INSTALL_FAILURE            1603L
#endif
#ifndef ERROR_INSTALL_SUSPEND
#define ERROR_INSTALL_SUSPEND            1604L
#endif
#ifndef ERROR_UNKNOWN_PRODUCT
#define ERROR_UNKNOWN_PRODUCT            1605L
#endif
#ifndef ERROR_UNKNOWN_FEATURE
#define ERROR_UNKNOWN_FEATURE            1606L
#endif
#ifndef ERROR_UNKNOWN_COMPONENT
#define ERROR_UNKNOWN_COMPONENT          1607L
#endif
#ifndef ERROR_UNKNOWN_PROPERTY
#define ERROR_UNKNOWN_PROPERTY           1608L
#endif
#ifndef ERROR_INVALID_HANDLE_STATE
#define ERROR_INVALID_HANDLE_STATE       1609L
#endif
#ifndef ERROR_BAD_CONFIGURATION
#define ERROR_BAD_CONFIGURATION          1610L
#endif
#ifndef ERROR_INDEX_ABSENT
#define ERROR_INDEX_ABSENT               1611L
#endif
#ifndef ERROR_INSTALL_SOURCE_ABSENT
#define ERROR_INSTALL_SOURCE_ABSENT      1612L
#endif
#ifndef ERROR_INSTALL_PACKAGE_VERSION
#define ERROR_INSTALL_PACKAGE_VERSION    1613L
#endif
#ifndef ERROR_INSTALL_PACKAGE_VERSION
#define ERROR_INSTALL_PACKAGE_VERSION    1613L
#endif
#ifndef ERROR_PRODUCT_UNINSTALLED
#define ERROR_PRODUCT_UNINSTALLED        1614L
#endif
#ifndef ERROR_BAD_QUERY_SYNTAX
#define ERROR_BAD_QUERY_SYNTAX           1615L
#endif
#ifndef ERROR_INVALID_FIELD
#define ERROR_INVALID_FIELD              1616L
#endif
#ifndef ERROR_DEVICE_REMOVED
#define ERROR_DEVICE_REMOVED             1617L
#endif
#ifndef ERROR_INSTALL_ALREADY_RUNNING
#define ERROR_INSTALL_ALREADY_RUNNING    1618L
#endif
#ifndef ERROR_INSTALL_PACKAGE_OPEN_FAILED
#define ERROR_INSTALL_PACKAGE_OPEN_FAILED 1619L
#endif
#ifndef ERROR_INSTALL_PACKAGE_INVALID
#define ERROR_INSTALL_PACKAGE_INVALID    1620L
#endif
#ifndef ERROR_INSTALL_UI_FAILURE
#define ERROR_INSTALL_UI_FAILURE         1621L
#endif
#ifndef ERROR_INSTALL_LOG_FAILURE
#define ERROR_INSTALL_LOG_FAILURE        1622L
#endif
#ifndef ERROR_INSTALL_LANGUAGE_UNSUPPORTED
#define ERROR_INSTALL_LANGUAGE_UNSUPPORTED 1623L
#endif
#ifndef ERROR_INSTALL_TRANSFORM_FAILURE
#define ERROR_INSTALL_TRANSFORM_FAILURE  1624L
#endif
#ifndef ERROR_INSTALL_PACKAGE_REJECTED
#define ERROR_INSTALL_PACKAGE_REJECTED   1625L
#endif
#ifndef ERROR_FUNCTION_NOT_CALLED
#define ERROR_FUNCTION_NOT_CALLED        1626L
#endif
#ifndef ERROR_FUNCTION_FAILED
#define ERROR_FUNCTION_FAILED            1627L
#endif
#ifndef ERROR_INVALID_TABLE
#define ERROR_INVALID_TABLE              1628L
#endif
#ifndef ERROR_DATATYPE_MISMATCH
#define ERROR_DATATYPE_MISMATCH          1629L
#endif
#ifndef ERROR_UNSUPPORTED_TYPE
#define ERROR_UNSUPPORTED_TYPE           1630L
#endif
#ifndef ERROR_CREATE_FAILED
#define ERROR_CREATE_FAILED              1631L
#endif
#ifndef ERROR_INSTALL_TEMP_UNWRITABLE
#define ERROR_INSTALL_TEMP_UNWRITABLE    1632L
#endif
#ifndef ERROR_INSTALL_PLATFORM_UNSUPPORTED
#define ERROR_INSTALL_PLATFORM_UNSUPPORTED 1633L
#endif
#ifndef ERROR_INSTALL_NOTUSED
#define ERROR_INSTALL_NOTUSED            1634L
#endif
#ifndef ERROR_PATCH_PACKAGE_OPEN_FAILED
#define ERROR_PATCH_PACKAGE_OPEN_FAILED  1635L
#endif
#ifndef ERROR_PATCH_PACKAGE_INVALID
#define ERROR_PATCH_PACKAGE_INVALID      1636L
#endif
#ifndef ERROR_PATCH_PACKAGE_UNSUPPORTED
#define ERROR_PATCH_PACKAGE_UNSUPPORTED  1637L
#endif
#ifndef ERROR_PRODUCT_VERSION
#define ERROR_PRODUCT_VERSION            1638L
#endif
#ifndef ERROR_INVALID_COMMAND_LINE
#define ERROR_INVALID_COMMAND_LINE       1639L
#endif
#ifndef ERROR_INSTALL_REMOTE_DISALLOWED
#define ERROR_INSTALL_REMOTE_DISALLOWED  1640L
#endif
#ifndef ERROR_SUCCESS_REBOOT_INITIATED
#define ERROR_SUCCESS_REBOOT_INITIATED   1641L
#endif
#ifndef ERROR_PATCH_TARGET_NOT_FOUND
#define ERROR_PATCH_TARGET_NOT_FOUND     1642L
#endif
#ifndef ERROR_PATCH_PACKAGE_REJECTED
#define ERROR_PATCH_PACKAGE_REJECTED     1643L
#endif
#ifndef ERROR_INSTALL_TRANSFORM_REJECTED
#define ERROR_INSTALL_TRANSFORM_REJECTED 1644L
#endif
#ifndef ERROR_INSTALL_REMOTE_PROHIBITED
#define ERROR_INSTALL_REMOTE_PROHIBITED  1645L
#endif
#ifndef RPC_X_WRONG_PIPE_ORDER
#define RPC_X_WRONG_PIPE_ORDER           1831L
#endif
#ifndef RPC_X_WRONG_PIPE_VERSION
#define RPC_X_WRONG_PIPE_VERSION         1832L
#endif
#ifndef RPC_S_INVALID_ASYNC_HANDLE
#define RPC_S_INVALID_ASYNC_HANDLE       1914L
#endif
#ifndef RPC_S_INVALID_ASYNC_CALL
#define RPC_S_INVALID_ASYNC_CALL         1915L
#endif
#ifndef RPC_X_PIPE_CLOSED
#define RPC_X_PIPE_CLOSED                1916L
#endif
#ifndef RPC_X_PIPE_DISCIPLINE_ERROR
#define RPC_X_PIPE_DISCIPLINE_ERROR      1917L
#endif
#ifndef RPC_X_PIPE_EMPTY
#define RPC_X_PIPE_EMPTY                 1918L
#endif
#ifndef ERROR_NO_SITENAME
#define ERROR_NO_SITENAME                1919L
#endif
#ifndef ERROR_CANT_ACCESS_FILE
#define ERROR_CANT_ACCESS_FILE           1920L
#endif
#ifndef ERROR_CANT_RESOLVE_FILENAME
#define ERROR_CANT_RESOLVE_FILENAME      1921L
#endif
#ifndef RPC_S_ENTRY_TYPE_MISMATCH
#define RPC_S_ENTRY_TYPE_MISMATCH        1922L
#endif
#ifndef RPC_S_NOT_ALL_OBJS_EXPORTED
#define RPC_S_NOT_ALL_OBJS_EXPORTED      1923L
#endif
#ifndef RPC_S_INTERFACE_NOT_EXPORTED
#define RPC_S_INTERFACE_NOT_EXPORTED     1924L
#endif
#ifndef RPC_S_PROFILE_NOT_ADDED
#define RPC_S_PROFILE_NOT_ADDED          1925L
#endif
#ifndef RPC_S_PRF_ELT_NOT_ADDED
#define RPC_S_PRF_ELT_NOT_ADDED          1926L
#endif
#ifndef RPC_S_PRF_ELT_NOT_REMOVED
#define RPC_S_PRF_ELT_NOT_REMOVED        1927L
#endif
#ifndef RPC_S_GRP_ELT_NOT_ADDED
#define RPC_S_GRP_ELT_NOT_ADDED          1928L
#endif
#ifndef RPC_S_GRP_ELT_NOT_REMOVED
#define RPC_S_GRP_ELT_NOT_REMOVED        1929L
#endif
#ifndef ERROR_KM_DRIVER_BLOCKED
#define ERROR_KM_DRIVER_BLOCKED          1930L
#endif
#ifndef ERROR_CONTEXT_EXPIRED
#define ERROR_CONTEXT_EXPIRED            1931L
#endif
#ifndef ERROR_PER_USER_TRUST_QUOTA_EXCEEDED
#define ERROR_PER_USER_TRUST_QUOTA_EXCEEDED 1932L
#endif
#ifndef ERROR_ALL_USER_TRUST_QUOTA_EXCEEDED
#define ERROR_ALL_USER_TRUST_QUOTA_EXCEEDED 1933L
#endif
#ifndef ERROR_USER_DELETE_TRUST_QUOTA_EXCEEDED
#define ERROR_USER_DELETE_TRUST_QUOTA_EXCEEDED 1934L
#endif
#ifndef ERROR_AUTHENTICATION_FIREWALL_FAILED
#define ERROR_AUTHENTICATION_FIREWALL_FAILED 1935L
#endif
#ifndef ERROR_REMOTE_PRINT_CONNECTIONS_BLOCKED
#define ERROR_REMOTE_PRINT_CONNECTIONS_BLOCKED 1936L
#endif
#ifndef ERROR_INVALID_CMM
#define ERROR_INVALID_CMM                2010L
#endif
#ifndef ERROR_INVALID_PROFILE
#define ERROR_INVALID_PROFILE            2011L
#endif
#ifndef ERROR_TAG_NOT_FOUND
#define ERROR_TAG_NOT_FOUND              2012L
#endif
#ifndef ERROR_TAG_NOT_PRESENT
#define ERROR_TAG_NOT_PRESENT            2013L
#endif
#ifndef ERROR_DUPLICATE_TAG
#define ERROR_DUPLICATE_TAG              2014L
#endif
#ifndef ERROR_PROFILE_NOT_ASSOCIATED_WITH_DEVICE
#define ERROR_PROFILE_NOT_ASSOCIATED_WITH_DEVICE 2015L
#endif
#ifndef ERROR_PROFILE_NOT_FOUND
#define ERROR_PROFILE_NOT_FOUND          2016L
#endif
#ifndef ERROR_INVALID_COLORSPACE
#define ERROR_INVALID_COLORSPACE         2017L
#endif
#ifndef ERROR_ICM_NOT_ENABLED
#define ERROR_ICM_NOT_ENABLED            2018L
#endif
#ifndef ERROR_DELETING_ICM_XFORM
#define ERROR_DELETING_ICM_XFORM         2019L
#endif
#ifndef ERROR_INVALID_TRANSFORM
#define ERROR_INVALID_TRANSFORM          2020L
#endif
#ifndef ERROR_COLORSPACE_MISMATCH
#define ERROR_COLORSPACE_MISMATCH        2021L
#endif
#ifndef ERROR_INVALID_COLORINDEX
#define ERROR_INVALID_COLORINDEX         2022L
#endif
#ifndef ERROR_COLORSPACE_MISMATCH
#define ERROR_COLORSPACE_MISMATCH        2021L
#endif
#ifndef ERROR_INVALID_COLORINDEX
#define ERROR_INVALID_COLORINDEX         2022L
#endif
#ifndef ERROR_CONNECTED_OTHER_PASSWORD
#define ERROR_CONNECTED_OTHER_PASSWORD   2108L
#endif
#ifndef ERROR_CONNECTED_OTHER_PASSWORD_DEFAULT
#define ERROR_CONNECTED_OTHER_PASSWORD_DEFAULT 2109L
#endif
#ifndef ERROR_PRINTER_NOT_FOUND
#define ERROR_PRINTER_NOT_FOUND          3012L
#endif
#ifndef ERROR_PRINTER_DRIVER_WARNED
#define ERROR_PRINTER_DRIVER_WARNED      3013L
#endif
#ifndef ERROR_PRINTER_DRIVER_BLOCKED
#define ERROR_PRINTER_DRIVER_BLOCKED     3014L
#endif
#ifndef ERROR_DHCP_ADDRESS_CONFLICT
#define ERROR_DHCP_ADDRESS_CONFLICT      4100L
#endif
#ifndef ERROR_WMI_GUID_NOT_FOUND
#define ERROR_WMI_GUID_NOT_FOUND         4200L
#endif
#ifndef ERROR_WMI_INSTANCE_NOT_FOUND
#define ERROR_WMI_INSTANCE_NOT_FOUND     4201L
#endif
#ifndef ERROR_WMI_CASEID_NOT_FOUND
#define ERROR_WMI_CASEID_NOT_FOUND       4202L
#endif
#ifndef ERROR_WMI_TRY_AGAIN
#define ERROR_WMI_TRY_AGAIN              4203L
#endif
#ifndef ERROR_WMI_DP_NOT_FOUND
#define ERROR_WMI_DP_NOT_FOUND           4204L
#endif
#ifndef ERROR_WMI_UNRESOLVED_INSTANCE_REF
#define ERROR_WMI_UNRESOLVED_INSTANCE_REF 4205L
#endif
#ifndef ERROR_WMI_ALREADY_ENABLED
#define ERROR_WMI_ALREADY_ENABLED        4206L
#endif
#ifndef ERROR_WMI_GUID_DISCONNECTED
#define ERROR_WMI_GUID_DISCONNECTED      4207L
#endif
#ifndef ERROR_WMI_SERVER_UNAVAILABLE
#define ERROR_WMI_SERVER_UNAVAILABLE     4208L
#endif
#ifndef ERROR_WMI_DP_FAILED
#define ERROR_WMI_DP_FAILED              4209L
#endif
#ifndef ERROR_WMI_INVALID_MOF
#define ERROR_WMI_INVALID_MOF            4210L
#endif
#ifndef ERROR_WMI_INVALID_REGINFO
#define ERROR_WMI_INVALID_REGINFO        4211L
#endif
#ifndef ERROR_WMI_ALREADY_DISABLED
#define ERROR_WMI_ALREADY_DISABLED       4212L
#endif
#ifndef ERROR_WMI_READ_ONLY
#define ERROR_WMI_READ_ONLY              4213L
#endif
#ifndef ERROR_WMI_SET_FAILURE
#define ERROR_WMI_SET_FAILURE            4214L
#endif
#ifndef ERROR_INVALID_MEDIA
#define ERROR_INVALID_MEDIA              4300L
#endif
#ifndef ERROR_INVALID_LIBRARY
#define ERROR_INVALID_LIBRARY            4301L
#endif
#ifndef ERROR_INVALID_MEDIA_POOL
#define ERROR_INVALID_MEDIA_POOL         4302L
#endif
#ifndef ERROR_DRIVE_MEDIA_MISMATCH
#define ERROR_DRIVE_MEDIA_MISMATCH       4303L
#endif
#ifndef ERROR_MEDIA_OFFLINE
#define ERROR_MEDIA_OFFLINE              4304L
#endif
#ifndef ERROR_LIBRARY_OFFLINE
#define ERROR_LIBRARY_OFFLINE            4305L
#endif
#ifndef ERROR_EMPTY
#define ERROR_EMPTY                      4306L
#endif
#ifndef ERROR_NOT_EMPTY
#define ERROR_NOT_EMPTY                  4307L
#endif
#ifndef ERROR_MEDIA_UNAVAILABLE
#define ERROR_MEDIA_UNAVAILABLE          4308L
#endif
#ifndef ERROR_RESOURCE_DISABLED
#define ERROR_RESOURCE_DISABLED          4309L
#endif
#ifndef ERROR_INVALID_CLEANER
#define ERROR_INVALID_CLEANER            4310L
#endif
#ifndef ERROR_UNABLE_TO_CLEAN
#define ERROR_UNABLE_TO_CLEAN            4311L
#endif
#ifndef ERROR_OBJECT_NOT_FOUND
#define ERROR_OBJECT_NOT_FOUND           4312L
#endif
#ifndef ERROR_DATABASE_FAILURE
#define ERROR_DATABASE_FAILURE           4313L
#endif
#ifndef ERROR_DATABASE_FULL
#define ERROR_DATABASE_FULL              4314L
#endif
#ifndef ERROR_MEDIA_INCOMPATIBLE
#define ERROR_MEDIA_INCOMPATIBLE         4315L
#endif
#ifndef ERROR_RESOURCE_NOT_PRESENT
#define ERROR_RESOURCE_NOT_PRESENT       4316L
#endif
#ifndef ERROR_INVALID_OPERATION
#define ERROR_INVALID_OPERATION          4317L
#endif
#ifndef ERROR_MEDIA_NOT_AVAILABLE
#define ERROR_MEDIA_NOT_AVAILABLE        4318L
#endif
#ifndef ERROR_DEVICE_NOT_AVAILABLE
#define ERROR_DEVICE_NOT_AVAILABLE       4319L
#endif
#ifndef ERROR_REQUEST_REFUSED
#define ERROR_REQUEST_REFUSED            4320L
#endif
#ifndef ERROR_INVALID_DRIVE_OBJECT
#define ERROR_INVALID_DRIVE_OBJECT       4321L
#endif
#ifndef ERROR_LIBRARY_FULL
#define ERROR_LIBRARY_FULL               4322L
#endif
#ifndef ERROR_MEDIUM_NOT_ACCESSIBLE
#define ERROR_MEDIUM_NOT_ACCESSIBLE      4323L
#endif
#ifndef ERROR_UNABLE_TO_LOAD_MEDIUM
#define ERROR_UNABLE_TO_LOAD_MEDIUM      4324L
#endif
#ifndef ERROR_UNABLE_TO_INVENTORY_DRIVE
#define ERROR_UNABLE_TO_INVENTORY_DRIVE  4325L
#endif
#ifndef ERROR_UNABLE_TO_INVENTORY_SLOT
#define ERROR_UNABLE_TO_INVENTORY_SLOT   4326L
#endif
#ifndef ERROR_UNABLE_TO_INVENTORY_TRANSPORT
#define ERROR_UNABLE_TO_INVENTORY_TRANSPORT 4327L
#endif
#ifndef ERROR_TRANSPORT_FULL
#define ERROR_TRANSPORT_FULL             4328L
#endif
#ifndef ERROR_CONTROLLING_IEPORT
#define ERROR_CONTROLLING_IEPORT         4329L
#endif
#ifndef ERROR_UNABLE_TO_EJECT_MOUNTED_MEDIA
#define ERROR_UNABLE_TO_EJECT_MOUNTED_MEDIA 4330L
#endif
#ifndef ERROR_CLEANER_SLOT_SET
#define ERROR_CLEANER_SLOT_SET           4331L
#endif
#ifndef ERROR_CLEANER_SLOT_NOT_SET
#define ERROR_CLEANER_SLOT_NOT_SET       4332L
#endif
#ifndef ERROR_CLEANER_CARTRIDGE_SPENT
#define ERROR_CLEANER_CARTRIDGE_SPENT    4333L
#endif
#ifndef ERROR_UNEXPECTED_OMID
#define ERROR_UNEXPECTED_OMID            4334L
#endif
#ifndef ERROR_CANT_DELETE_LAST_CASE
#define ERROR_CANT_DELETE_LAST_CASE      4335L
#endif
#ifndef ERROR_MESSAGE_EXCEEDS_MAX_SIZE
#define ERROR_MESSAGE_EXCEEDS_MAX_SIZE   4336L
#endif
#ifndef ERROR_VOLUME_CONTAINS_SYS_FILES
#define ERROR_VOLUME_CONTAINS_SYS_FILES  4337L
#endif
#ifndef ERROR_INDIGENOUS_TYPE
#define ERROR_INDIGENOUS_TYPE            4338L
#endif
#ifndef ERROR_NO_SUPPORTING_DRIVES
#define ERROR_NO_SUPPORTING_DRIVES       4339L
#endif
#ifndef ERROR_CLEANER_CARTRIDGE_INSTALLED
#define ERROR_CLEANER_CARTRIDGE_INSTALLED 4340L
#endif
#ifndef ERROR_IEPORT_FULL
#define ERROR_IEPORT_FULL                4341L
#endif
#ifndef ERROR_FILE_OFFLINE
#define ERROR_FILE_OFFLINE               4350L
#endif
#ifndef ERROR_REMOTE_STORAGE_NOT_ACTIVE
#define ERROR_REMOTE_STORAGE_NOT_ACTIVE  4351L
#endif
#ifndef ERROR_REMOTE_STORAGE_MEDIA_ERROR
#define ERROR_REMOTE_STORAGE_MEDIA_ERROR 4352L
#endif
#ifndef ERROR_NOT_A_REPARSE_POINT
#define ERROR_NOT_A_REPARSE_POINT        4390L
#endif
#ifndef ERROR_REPARSE_ATTRIBUTE_CONFLICT
#define ERROR_REPARSE_ATTRIBUTE_CONFLICT 4391L
#endif
#ifndef ERROR_INVALID_REPARSE_DATA
#define ERROR_INVALID_REPARSE_DATA       4392L
#endif
#ifndef ERROR_REPARSE_TAG_INVALID
#define ERROR_REPARSE_TAG_INVALID        4393L
#endif
#ifndef ERROR_REPARSE_TAG_MISMATCH
#define ERROR_REPARSE_TAG_MISMATCH       4394L
#endif
#ifndef ERROR_VOLUME_NOT_SIS_ENABLED
#define ERROR_VOLUME_NOT_SIS_ENABLED     4500L
#endif
#ifndef ERROR_DEPENDENT_RESOURCE_EXISTS
#define ERROR_DEPENDENT_RESOURCE_EXISTS  5001L
#endif
#ifndef ERROR_DEPENDENCY_NOT_FOUND
#define ERROR_DEPENDENCY_NOT_FOUND       5002L
#endif
#ifndef ERROR_DEPENDENCY_ALREADY_EXISTS
#define ERROR_DEPENDENCY_ALREADY_EXISTS  5003L
#endif
#ifndef ERROR_RESOURCE_NOT_ONLINE
#define ERROR_RESOURCE_NOT_ONLINE        5004L
#endif
#ifndef ERROR_HOST_NODE_NOT_AVAILABLE
#define ERROR_HOST_NODE_NOT_AVAILABLE    5005L
#endif
#ifndef ERROR_RESOURCE_NOT_AVAILABLE
#define ERROR_RESOURCE_NOT_AVAILABLE     5006L
#endif
#ifndef ERROR_RESOURCE_NOT_FOUND
#define ERROR_RESOURCE_NOT_FOUND         5007L
#endif
#ifndef ERROR_SHUTDOWN_CLUSTER
#define ERROR_SHUTDOWN_CLUSTER           5008L
#endif
#ifndef ERROR_CANT_EVICT_ACTIVE_NODE
#define ERROR_CANT_EVICT_ACTIVE_NODE     5009L
#endif
#ifndef ERROR_OBJECT_ALREADY_EXISTS
#define ERROR_OBJECT_ALREADY_EXISTS      5010L
#endif
#ifndef ERROR_OBJECT_IN_LIST
#define ERROR_OBJECT_IN_LIST             5011L
#endif
#ifndef ERROR_GROUP_NOT_AVAILABLE
#define ERROR_GROUP_NOT_AVAILABLE        5012L
#endif
#ifndef ERROR_GROUP_NOT_FOUND
#define ERROR_GROUP_NOT_FOUND            5013L
#endif
#ifndef ERROR_GROUP_NOT_ONLINE
#define ERROR_GROUP_NOT_ONLINE           5014L
#endif
#ifndef ERROR_HOST_NODE_NOT_RESOURCE_OWNER
#define ERROR_HOST_NODE_NOT_RESOURCE_OWNER 5015L
#endif
#ifndef ERROR_HOST_NODE_NOT_GROUP_OWNER
#define ERROR_HOST_NODE_NOT_GROUP_OWNER  5016L
#endif
#ifndef ERROR_RESMON_CREATE_FAILED
#define ERROR_RESMON_CREATE_FAILED       5017L
#endif
#ifndef ERROR_RESMON_ONLINE_FAILED
#define ERROR_RESMON_ONLINE_FAILED       5018L
#endif
#ifndef ERROR_RESOURCE_ONLINE
#define ERROR_RESOURCE_ONLINE            5019L
#endif
#ifndef ERROR_QUORUM_RESOURCE
#define ERROR_QUORUM_RESOURCE            5020L
#endif
#ifndef ERROR_NOT_QUORUM_CAPABLE
#define ERROR_NOT_QUORUM_CAPABLE         5021L
#endif
#ifndef ERROR_CLUSTER_SHUTTING_DOWN
#define ERROR_CLUSTER_SHUTTING_DOWN      5022L
#endif
#ifndef ERROR_INVALID_STATE
#define ERROR_INVALID_STATE              5023L
#endif
#ifndef ERROR_RESOURCE_PROPERTIES_STORED
#define ERROR_RESOURCE_PROPERTIES_STORED 5024L
#endif
#ifndef ERROR_NOT_QUORUM_CLASS
#define ERROR_NOT_QUORUM_CLASS           5025L
#endif
#ifndef ERROR_CORE_RESOURCE
#define ERROR_CORE_RESOURCE              5026L
#endif
#ifndef ERROR_QUORUM_RESOURCE_ONLINE_FAILED
#define ERROR_QUORUM_RESOURCE_ONLINE_FAILED 5027L
#endif
#ifndef ERROR_QUORUMLOG_OPEN_FAILED
#define ERROR_QUORUMLOG_OPEN_FAILED      5028L
#endif
#ifndef ERROR_CLUSTERLOG_CORRUPT
#define ERROR_CLUSTERLOG_CORRUPT         5029L
#endif
#ifndef ERROR_CLUSTERLOG_RECORD_EXCEEDS_MAXSIZE
#define ERROR_CLUSTERLOG_RECORD_EXCEEDS_MAXSIZE 5030L
#endif
#ifndef ERROR_CLUSTERLOG_EXCEEDS_MAXSIZE
#define ERROR_CLUSTERLOG_EXCEEDS_MAXSIZE 5031L
#endif
#ifndef ERROR_CLUSTERLOG_CHKPOINT_NOT_FOUND
#define ERROR_CLUSTERLOG_CHKPOINT_NOT_FOUND 5032L
#endif
#ifndef ERROR_CLUSTERLOG_NOT_ENOUGH_SPACE
#define ERROR_CLUSTERLOG_NOT_ENOUGH_SPACE 5033L
#endif
#ifndef ERROR_QUORUM_OWNER_ALIVE
#define ERROR_QUORUM_OWNER_ALIVE         5034L
#endif
#ifndef ERROR_NETWORK_NOT_AVAILABLE
#define ERROR_NETWORK_NOT_AVAILABLE      5035L
#endif
#ifndef ERROR_NODE_NOT_AVAILABLE
#define ERROR_NODE_NOT_AVAILABLE         5036L
#endif
#ifndef ERROR_ALL_NODES_NOT_AVAILABLE
#define ERROR_ALL_NODES_NOT_AVAILABLE    5037L
#endif
#ifndef ERROR_RESOURCE_FAILED
#define ERROR_RESOURCE_FAILED            5038L
#endif
#ifndef ERROR_CLUSTER_INVALID_NODE
#define ERROR_CLUSTER_INVALID_NODE       5039L
#endif
#ifndef ERROR_CLUSTER_NODE_EXISTS
#define ERROR_CLUSTER_NODE_EXISTS        5040L
#endif
#ifndef ERROR_CLUSTER_JOIN_IN_PROGRESS
#define ERROR_CLUSTER_JOIN_IN_PROGRESS   5041L
#endif
#ifndef ERROR_CLUSTER_NODE_NOT_FOUND
#define ERROR_CLUSTER_NODE_NOT_FOUND     5042L
#endif
#ifndef ERROR_CLUSTER_LOCAL_NODE_NOT_FOUND
#define ERROR_CLUSTER_LOCAL_NODE_NOT_FOUND 5043L
#endif
#ifndef ERROR_CLUSTER_NETWORK_EXISTS
#define ERROR_CLUSTER_NETWORK_EXISTS     5044L
#endif
#ifndef ERROR_CLUSTER_NETWORK_NOT_FOUND
#define ERROR_CLUSTER_NETWORK_NOT_FOUND  5045L
#endif
#ifndef ERROR_CLUSTER_NETINTERFACE_EXISTS
#define ERROR_CLUSTER_NETINTERFACE_EXISTS 5046L
#endif
#ifndef ERROR_CLUSTER_NETINTERFACE_NOT_FOUND
#define ERROR_CLUSTER_NETINTERFACE_NOT_FOUND 5047L
#endif
#ifndef ERROR_CLUSTER_INVALID_REQUEST
#define ERROR_CLUSTER_INVALID_REQUEST    5048L
#endif
#ifndef ERROR_CLUSTER_INVALID_NETWORK_PROVIDER
#define ERROR_CLUSTER_INVALID_NETWORK_PROVIDER 5049L
#endif
#ifndef ERROR_CLUSTER_NODE_DOWN
#define ERROR_CLUSTER_NODE_DOWN          5050L
#endif
#ifndef ERROR_CLUSTER_NODE_UNREACHABLE
#define ERROR_CLUSTER_NODE_UNREACHABLE   5051L
#endif
#ifndef ERROR_CLUSTER_NODE_NOT_MEMBER
#define ERROR_CLUSTER_NODE_NOT_MEMBER    5052L
#endif
#ifndef ERROR_CLUSTER_JOIN_NOT_IN_PROGRESS
#define ERROR_CLUSTER_JOIN_NOT_IN_PROGRESS 5053L
#endif
#ifndef ERROR_CLUSTER_INVALID_NETWORK
#define ERROR_CLUSTER_INVALID_NETWORK    5054L
#endif
#ifndef ERROR_CLUSTER_NODE_UP
#define ERROR_CLUSTER_NODE_UP            5056L
#endif
#ifndef ERROR_CLUSTER_IPADDR_IN_USE
#define ERROR_CLUSTER_IPADDR_IN_USE      5057L
#endif
#ifndef ERROR_CLUSTER_NODE_NOT_PAUSED
#define ERROR_CLUSTER_NODE_NOT_PAUSED    5058L
#endif
#ifndef ERROR_CLUSTER_NO_SECURITY_CONTEXT
#define ERROR_CLUSTER_NO_SECURITY_CONTEXT 5059L
#endif
#ifndef ERROR_CLUSTER_NETWORK_NOT_INTERNAL
#define ERROR_CLUSTER_NETWORK_NOT_INTERNAL 5060L
#endif
#ifndef ERROR_CLUSTER_NODE_ALREADY_UP
#define ERROR_CLUSTER_NODE_ALREADY_UP    5061L
#endif
#ifndef ERROR_CLUSTER_NODE_ALREADY_DOWN
#define ERROR_CLUSTER_NODE_ALREADY_DOWN  5062L
#endif
#ifndef ERROR_CLUSTER_NETWORK_ALREADY_ONLINE
#define ERROR_CLUSTER_NETWORK_ALREADY_ONLINE 5063L
#endif
#ifndef ERROR_CLUSTER_NETWORK_ALREADY_OFFLINE
#define ERROR_CLUSTER_NETWORK_ALREADY_OFFLINE 5064L
#endif
#ifndef ERROR_CLUSTER_NODE_ALREADY_MEMBER
#define ERROR_CLUSTER_NODE_ALREADY_MEMBER 5065L
#endif
#ifndef ERROR_CLUSTER_LAST_INTERNAL_NETWORK
#define ERROR_CLUSTER_LAST_INTERNAL_NETWORK 5066L
#endif
#ifndef ERROR_CLUSTER_NETWORK_HAS_DEPENDENTS
#define ERROR_CLUSTER_NETWORK_HAS_DEPENDENTS 5067L
#endif
#ifndef ERROR_INVALID_OPERATION_ON_QUORUM
#define ERROR_INVALID_OPERATION_ON_QUORUM 5068L
#endif
#ifndef ERROR_DEPENDENCY_NOT_ALLOWED
#define ERROR_DEPENDENCY_NOT_ALLOWED     5069L
#endif
#ifndef ERROR_CLUSTER_NODE_PAUSED
#define ERROR_CLUSTER_NODE_PAUSED        5070L
#endif
#ifndef ERROR_NODE_CANT_HOST_RESOURCE
#define ERROR_NODE_CANT_HOST_RESOURCE    5071L
#endif
#ifndef ERROR_CLUSTER_NODE_NOT_READY
#define ERROR_CLUSTER_NODE_NOT_READY     5072L
#endif
#ifndef ERROR_CLUSTER_NODE_SHUTTING_DOWN
#define ERROR_CLUSTER_NODE_SHUTTING_DOWN 5073L
#endif
#ifndef ERROR_CLUSTER_NODE_SHUTTING_DOWN
#define ERROR_CLUSTER_NODE_SHUTTING_DOWN 5073L
#endif
#ifndef ERROR_CLUSTER_NODE_SHUTTING_DOWN
#define ERROR_CLUSTER_NODE_SHUTTING_DOWN 5073L
#endif
#ifndef ERROR_CLUSTER_JOIN_ABORTED
#define ERROR_CLUSTER_JOIN_ABORTED       5074L
#endif
#ifndef ERROR_CLUSTER_INCOMPATIBLE_VERSIONS
#define ERROR_CLUSTER_INCOMPATIBLE_VERSIONS 5075L
#endif
#ifndef ERROR_CLUSTER_MAXNUM_OF_RESOURCES_EXCEEDED
#define ERROR_CLUSTER_MAXNUM_OF_RESOURCES_EXCEEDED 5076L
#endif
#ifndef ERROR_CLUSTER_SYSTEM_CONFIG_CHANGED
#define ERROR_CLUSTER_SYSTEM_CONFIG_CHANGED 5077L
#endif
#ifndef ERROR_CLUSTER_RESOURCE_TYPE_NOT_FOUND
#define ERROR_CLUSTER_RESOURCE_TYPE_NOT_FOUND 5078L
#endif
#ifndef ERROR_CLUSTER_RESTYPE_NOT_SUPPORTED
#define ERROR_CLUSTER_RESTYPE_NOT_SUPPORTED 5079L
#endif
#ifndef ERROR_CLUSTER_RESNAME_NOT_FOUND
#define ERROR_CLUSTER_RESNAME_NOT_FOUND  5080L
#endif
#ifndef ERROR_CLUSTER_NO_RPC_PACKAGES_REGISTERED
#define ERROR_CLUSTER_NO_RPC_PACKAGES_REGISTERED 5081L
#endif
#ifndef ERROR_CLUSTER_OWNER_NOT_IN_PREFLIST
#define ERROR_CLUSTER_OWNER_NOT_IN_PREFLIST 5082L
#endif
#ifndef ERROR_CLUSTER_DATABASE_SEQMISMATCH
#define ERROR_CLUSTER_DATABASE_SEQMISMATCH 5083L
#endif
#ifndef ERROR_RESMON_INVALID_STATE
#define ERROR_RESMON_INVALID_STATE       5084L
#endif
#ifndef ERROR_CLUSTER_GUM_NOT_LOCKER
#define ERROR_CLUSTER_GUM_NOT_LOCKER     5085L
#endif
#ifndef ERROR_QUORUM_DISK_NOT_FOUND
#define ERROR_QUORUM_DISK_NOT_FOUND      5086L
#endif
#ifndef ERROR_DATABASE_BACKUP_CORRUPT
#define ERROR_DATABASE_BACKUP_CORRUPT    5087L
#endif
#ifndef ERROR_CLUSTER_NODE_ALREADY_HAS_DFS_ROOT
#define ERROR_CLUSTER_NODE_ALREADY_HAS_DFS_ROOT 5088L
#endif
#ifndef ERROR_RESOURCE_PROPERTY_UNCHANGEABLE
#define ERROR_RESOURCE_PROPERTY_UNCHANGEABLE 5089L
#endif
#ifndef ERROR_CLUSTER_MEMBERSHIP_INVALID_STATE
#define ERROR_CLUSTER_MEMBERSHIP_INVALID_STATE 5890L
#endif
#ifndef ERROR_CLUSTER_QUORUMLOG_NOT_FOUND
#define ERROR_CLUSTER_QUORUMLOG_NOT_FOUND 5891L
#endif
#ifndef ERROR_CLUSTER_MEMBERSHIP_HALT
#define ERROR_CLUSTER_MEMBERSHIP_HALT    5892L
#endif
#ifndef ERROR_CLUSTER_INSTANCE_ID_MISMATCH
#define ERROR_CLUSTER_INSTANCE_ID_MISMATCH 5893L
#endif
#ifndef ERROR_CLUSTER_NETWORK_NOT_FOUND_FOR_IP
#define ERROR_CLUSTER_NETWORK_NOT_FOUND_FOR_IP 5894L
#endif
#ifndef ERROR_CLUSTER_PROPERTY_DATA_TYPE_MISMATCH
#define ERROR_CLUSTER_PROPERTY_DATA_TYPE_MISMATCH 5895L
#endif
#ifndef ERROR_CLUSTER_EVICT_WITHOUT_CLEANUP
#define ERROR_CLUSTER_EVICT_WITHOUT_CLEANUP 5896L
#endif
#ifndef ERROR_CLUSTER_PARAMETER_MISMATCH
#define ERROR_CLUSTER_PARAMETER_MISMATCH 5897L
#endif
#ifndef ERROR_NODE_CANNOT_BE_CLUSTERED
#define ERROR_NODE_CANNOT_BE_CLUSTERED   5898L
#endif
#ifndef ERROR_CLUSTER_WRONG_OS_VERSION
#define ERROR_CLUSTER_WRONG_OS_VERSION   5899L
#endif
#ifndef ERROR_CLUSTER_CANT_CREATE_DUP_CLUSTER_NAME
#define ERROR_CLUSTER_CANT_CREATE_DUP_CLUSTER_NAME 5900L
#endif
#ifndef ERROR_CLUSCFG_ALREADY_COMMITTED
#define ERROR_CLUSCFG_ALREADY_COMMITTED  5901L
#endif
#ifndef ERROR_CLUSCFG_ROLLBACK_FAILED
#define ERROR_CLUSCFG_ROLLBACK_FAILED    5902L
#endif
#ifndef ERROR_CLUSCFG_SYSTEM_DISK_DRIVE_LETTER_CONFLICT
#define ERROR_CLUSCFG_SYSTEM_DISK_DRIVE_LETTER_CONFLICT 5903L
#endif
#ifndef ERROR_CLUSTER_OLD_VERSION
#define ERROR_CLUSTER_OLD_VERSION        5904L
#endif
#ifndef ERROR_CLUSTER_MISMATCHED_COMPUTER_ACCT_NAME
#define ERROR_CLUSTER_MISMATCHED_COMPUTER_ACCT_NAME 5905L
#endif
#ifndef ERROR_ENCRYPTION_FAILED
#define ERROR_ENCRYPTION_FAILED          6000L
#endif
#ifndef ERROR_DECRYPTION_FAILED
#define ERROR_DECRYPTION_FAILED          6001L
#endif
#ifndef ERROR_FILE_ENCRYPTED
#define ERROR_FILE_ENCRYPTED             6002L
#endif
#ifndef ERROR_NO_RECOVERY_POLICY
#define ERROR_NO_RECOVERY_POLICY         6003L
#endif
#ifndef ERROR_NO_EFS
#define ERROR_NO_EFS                     6004L
#endif
#ifndef ERROR_WRONG_EFS
#define ERROR_WRONG_EFS                  6005L
#endif
#ifndef ERROR_NO_USER_KEYS
#define ERROR_NO_USER_KEYS               6006L
#endif
#ifndef ERROR_FILE_NOT_ENCRYPTED
#define ERROR_FILE_NOT_ENCRYPTED         6007L
#endif
#ifndef ERROR_NOT_EXPORT_FORMAT
#define ERROR_NOT_EXPORT_FORMAT          6008L
#endif
#ifndef ERROR_FILE_READ_ONLY
#define ERROR_FILE_READ_ONLY             6009L
#endif
#ifndef ERROR_DIR_EFS_DISALLOWED
#define ERROR_DIR_EFS_DISALLOWED         6010L
#endif
#ifndef ERROR_EFS_SERVER_NOT_TRUSTED
#define ERROR_EFS_SERVER_NOT_TRUSTED     6011L
#endif
#ifndef ERROR_BAD_RECOVERY_POLICY
#define ERROR_BAD_RECOVERY_POLICY        6012L
#endif
#ifndef ERROR_EFS_ALG_BLOB_TOO_BIG
#define ERROR_EFS_ALG_BLOB_TOO_BIG       6013L
#endif
#ifndef ERROR_VOLUME_NOT_SUPPORT_EFS
#define ERROR_VOLUME_NOT_SUPPORT_EFS     6014L
#endif
#ifndef ERROR_EFS_DISABLED
#define ERROR_EFS_DISABLED               6015L
#endif
#ifndef ERROR_EFS_VERSION_NOT_SUPPORT
#define ERROR_EFS_VERSION_NOT_SUPPORT    6016L
#endif
#ifndef SCHED_E_SERVICE_NOT_LOCALSYSTEM
#define SCHED_E_SERVICE_NOT_LOCALSYSTEM  6200L
#endif
#ifndef ERROR_CTX_WINSTATION_NAME_INVALID
#define ERROR_CTX_WINSTATION_NAME_INVALID 7001L
#endif
#ifndef ERROR_CTX_INVALID_PD
#define ERROR_CTX_INVALID_PD             7002L
#endif
#ifndef ERROR_CTX_PD_NOT_FOUND
#define ERROR_CTX_PD_NOT_FOUND           7003L
#endif
#ifndef ERROR_CTX_WD_NOT_FOUND
#define ERROR_CTX_WD_NOT_FOUND           7004L
#endif
#ifndef ERROR_CTX_CANNOT_MAKE_EVENTLOG_ENTRY
#define ERROR_CTX_CANNOT_MAKE_EVENTLOG_ENTRY 7005L
#endif
#ifndef ERROR_CTX_SERVICE_NAME_COLLISION
#define ERROR_CTX_SERVICE_NAME_COLLISION 7006L
#endif
#ifndef ERROR_CTX_CLOSE_PENDING
#define ERROR_CTX_CLOSE_PENDING          7007L
#endif
#ifndef ERROR_CTX_NO_OUTBUF
#define ERROR_CTX_NO_OUTBUF              7008L
#endif
#ifndef ERROR_CTX_MODEM_INF_NOT_FOUND
#define ERROR_CTX_MODEM_INF_NOT_FOUND    7009L
#endif
#ifndef ERROR_CTX_INVALID_MODEMNAME
#define ERROR_CTX_INVALID_MODEMNAME      7010L
#endif
#ifndef ERROR_CTX_MODEM_RESPONSE_ERROR
#define ERROR_CTX_MODEM_RESPONSE_ERROR   7011L
#endif
#ifndef ERROR_CTX_MODEM_RESPONSE_TIMEOUT
#define ERROR_CTX_MODEM_RESPONSE_TIMEOUT 7012L
#endif
#ifndef ERROR_CTX_MODEM_RESPONSE_NO_CARRIER
#define ERROR_CTX_MODEM_RESPONSE_NO_CARRIER 7013L
#endif
#ifndef ERROR_CTX_MODEM_RESPONSE_NO_DIALTONE
#define ERROR_CTX_MODEM_RESPONSE_NO_DIALTONE 7014L
#endif
#ifndef ERROR_CTX_MODEM_RESPONSE_BUSY
#define ERROR_CTX_MODEM_RESPONSE_BUSY    7015L
#endif
#ifndef ERROR_CTX_MODEM_RESPONSE_VOICE
#define ERROR_CTX_MODEM_RESPONSE_VOICE   7016L
#endif
#ifndef ERROR_CTX_TD_ERROR
#define ERROR_CTX_TD_ERROR               7017L
#endif
#ifndef ERROR_CTX_WINSTATION_NOT_FOUND
#define ERROR_CTX_WINSTATION_NOT_FOUND   7022L
#endif
#ifndef ERROR_CTX_WINSTATION_ALREADY_EXISTS
#define ERROR_CTX_WINSTATION_ALREADY_EXISTS 7023L
#endif
#ifndef ERROR_CTX_WINSTATION_BUSY
#define ERROR_CTX_WINSTATION_BUSY        7024L
#endif
#ifndef ERROR_CTX_BAD_VIDEO_MODE
#define ERROR_CTX_BAD_VIDEO_MODE         7025L
#endif
#ifndef ERROR_CTX_GRAPHICS_INVALID
#define ERROR_CTX_GRAPHICS_INVALID       7035L
#endif
#ifndef ERROR_CTX_LOGON_DISABLED
#define ERROR_CTX_LOGON_DISABLED         7037L
#endif
#ifndef ERROR_CTX_NOT_CONSOLE
#define ERROR_CTX_NOT_CONSOLE            7038L
#endif
#ifndef ERROR_CTX_CLIENT_QUERY_TIMEOUT
#define ERROR_CTX_CLIENT_QUERY_TIMEOUT   7040L
#endif
#ifndef ERROR_CTX_CONSOLE_DISCONNECT
#define ERROR_CTX_CONSOLE_DISCONNECT     7041L
#endif
#ifndef ERROR_CTX_CONSOLE_CONNECT
#define ERROR_CTX_CONSOLE_CONNECT        7042L
#endif
#ifndef ERROR_CTX_SHADOW_DENIED
#define ERROR_CTX_SHADOW_DENIED          7044L
#endif
#ifndef ERROR_CTX_WINSTATION_ACCESS_DENIED
#define ERROR_CTX_WINSTATION_ACCESS_DENIED 7045L
#endif
#ifndef ERROR_CTX_INVALID_WD
#define ERROR_CTX_INVALID_WD             7049L
#endif
#ifndef ERROR_CTX_SHADOW_INVALID
#define ERROR_CTX_SHADOW_INVALID         7050L
#endif
#ifndef ERROR_CTX_SHADOW_DISABLED
#define ERROR_CTX_SHADOW_DISABLED        7051L
#endif
#ifndef ERROR_CTX_CLIENT_LICENSE_IN_USE
#define ERROR_CTX_CLIENT_LICENSE_IN_USE  7052L
#endif
#ifndef ERROR_CTX_CLIENT_LICENSE_NOT_SET
#define ERROR_CTX_CLIENT_LICENSE_NOT_SET 7053L
#endif
#ifndef ERROR_CTX_LICENSE_NOT_AVAILABLE
#define ERROR_CTX_LICENSE_NOT_AVAILABLE  7054L
#endif
#ifndef ERROR_CTX_LICENSE_CLIENT_INVALID
#define ERROR_CTX_LICENSE_CLIENT_INVALID 7055L
#endif
#ifndef ERROR_CTX_LICENSE_EXPIRED
#define ERROR_CTX_LICENSE_EXPIRED        7056L
#endif
#ifndef ERROR_CTX_SHADOW_NOT_RUNNING
#define ERROR_CTX_SHADOW_NOT_RUNNING     7057L
#endif
#ifndef ERROR_CTX_SHADOW_ENDED_BY_MODE_CHANGE
#define ERROR_CTX_SHADOW_ENDED_BY_MODE_CHANGE 7058L
#endif
#ifndef ERROR_ACTIVATION_COUNT_EXCEEDED
#define ERROR_ACTIVATION_COUNT_EXCEEDED  7059L
#endif
#ifndef FRS_ERR_INVALID_API_SEQUENCE
#define FRS_ERR_INVALID_API_SEQUENCE     8001L
#endif
#ifndef FRS_ERR_STARTING_SERVICE
#define FRS_ERR_STARTING_SERVICE         8002L
#endif
#ifndef FRS_ERR_STOPPING_SERVICE
#define FRS_ERR_STOPPING_SERVICE         8003L
#endif
#ifndef FRS_ERR_INTERNAL_API
#define FRS_ERR_INTERNAL_API             8004L
#endif
#ifndef FRS_ERR_INTERNAL
#define FRS_ERR_INTERNAL                 8005L
#endif
#ifndef FRS_ERR_SERVICE_COMM
#define FRS_ERR_SERVICE_COMM             8006L
#endif
#ifndef FRS_ERR_INSUFFICIENT_PRIV
#define FRS_ERR_INSUFFICIENT_PRIV        8007L
#endif
#ifndef FRS_ERR_AUTHENTICATION
#define FRS_ERR_AUTHENTICATION           8008L
#endif
#ifndef FRS_ERR_PARENT_INSUFFICIENT_PRIV
#define FRS_ERR_PARENT_INSUFFICIENT_PRIV 8009L
#endif
#ifndef FRS_ERR_PARENT_AUTHENTICATION
#define FRS_ERR_PARENT_AUTHENTICATION    8010L
#endif
#ifndef FRS_ERR_CHILD_TO_PARENT_COMM
#define FRS_ERR_CHILD_TO_PARENT_COMM     8011L
#endif
#ifndef FRS_ERR_PARENT_TO_CHILD_COMM
#define FRS_ERR_PARENT_TO_CHILD_COMM     8012L
#endif
#ifndef FRS_ERR_SYSVOL_POPULATE
#define FRS_ERR_SYSVOL_POPULATE          8013L
#endif
#ifndef FRS_ERR_SYSVOL_POPULATE_TIMEOUT
#define FRS_ERR_SYSVOL_POPULATE_TIMEOUT  8014L
#endif
#ifndef FRS_ERR_SYSVOL_IS_BUSY
#define FRS_ERR_SYSVOL_IS_BUSY           8015L
#endif
#ifndef FRS_ERR_SYSVOL_DEMOTE
#define FRS_ERR_SYSVOL_DEMOTE            8016L
#endif
#ifndef FRS_ERR_INVALID_SERVICE_PARAMETER
#define FRS_ERR_INVALID_SERVICE_PARAMETER 8017L
#endif
#ifdef ERROR_DS_NOT_INSTALLED
#   if ERROR_DS_NOT_INSTALLED == ERROR_ONLY_IF_CONNECTED
#	undef ERROR_DS_NOT_INSTALLED
#   endif
#endif
#ifndef ERROR_DS_NOT_INSTALLED
#define ERROR_DS_NOT_INSTALLED           8200L
#endif
#ifdef ERROR_DS_MEMBERSHIP_EVALUATED_LOCALLY
#   if ERROR_DS_MEMBERSHIP_EVALUATED_LOCALLY == RPC_S_ENTRY_TYPE_MISMATCH
#	undef ERROR_DS_MEMBERSHIP_EVALUATED_LOCALLY
#   endif
#endif
#ifndef ERROR_DS_MEMBERSHIP_EVALUATED_LOCALLY
#define ERROR_DS_MEMBERSHIP_EVALUATED_LOCALLY 8201L
#endif
#ifdef ERROR_DS_NO_ATTRIBUTE_OR_VALUE
#   if ERROR_DS_NO_ATTRIBUTE_OR_VALUE == RPC_S_NOT_ALL_OBJS_EXPORTED
#	undef ERROR_DS_NO_ATTRIBUTE_OR_VALUE
#   endif
#endif
#ifndef ERROR_DS_NO_ATTRIBUTE_OR_VALUE
#define ERROR_DS_NO_ATTRIBUTE_OR_VALUE   8202L
#endif
#ifdef ERROR_DS_INVALID_ATTRIBUTE_SYNTAX
#   if ERROR_DS_INVALID_ATTRIBUTE_SYNTAX == RPC_S_INTERFACE_NOT_EXPORTED
#	undef ERROR_DS_INVALID_ATTRIBUTE_SYNTAX
#   endif
#endif
#ifndef ERROR_DS_INVALID_ATTRIBUTE_SYNTAX
#define ERROR_DS_INVALID_ATTRIBUTE_SYNTAX 8203L
#endif
#ifdef ERROR_DS_ATTRIBUTE_TYPE_UNDEFINED
#   if ERROR_DS_ATTRIBUTE_TYPE_UNDEFINED == RPC_S_PROFILE_NOT_ADDED
#	undef ERROR_DS_ATTRIBUTE_TYPE_UNDEFINED
#   endif
#endif
#ifndef ERROR_DS_ATTRIBUTE_TYPE_UNDEFINED
#define ERROR_DS_ATTRIBUTE_TYPE_UNDEFINED 8204L
#endif
#ifdef ERROR_DS_ATTRIBUTE_OR_VALUE_EXISTS
#   if ERROR_DS_ATTRIBUTE_OR_VALUE_EXISTS == RPC_S_PRF_ELT_NOT_ADDED
#	undef ERROR_DS_ATTRIBUTE_OR_VALUE_EXISTS
#   endif
#endif
#ifndef ERROR_DS_ATTRIBUTE_OR_VALUE_EXISTS
#define ERROR_DS_ATTRIBUTE_OR_VALUE_EXISTS 8205L
#endif
#ifdef ERROR_DS_BUSY
#   if ERROR_DS_BUSY == RPC_S_PRF_ELT_NOT_REMOVED
#	undef ERROR_DS_BUSY
#   endif
#endif
#ifndef ERROR_DS_BUSY
#define ERROR_DS_BUSY                    8206L
#endif
#ifdef ERROR_DS_UNAVAILABLE
#   if ERROR_DS_UNAVAILABLE == RPC_S_GRP_ELT_NOT_ADDED
#	undef ERROR_DS_UNAVAILABLE
#   endif
#endif
#ifndef ERROR_DS_UNAVAILABLE
#define ERROR_DS_UNAVAILABLE             8207L
#endif
#ifdef ERROR_DS_NO_RIDS_ALLOCATED
#   if ERROR_DS_NO_RIDS_ALLOCATED == RPC_S_GRP_ELT_NOT_REMOVED
#	undef ERROR_DS_NO_RIDS_ALLOCATED
#   endif
#endif
#ifndef ERROR_DS_NO_RIDS_ALLOCATED
#define ERROR_DS_NO_RIDS_ALLOCATED       8208L
#endif
#ifdef ERROR_DS_NO_MORE_RIDS
#   if ERROR_DS_NO_MORE_RIDS == ERROR_KM_DRIVER_BLOCKED
#	undef ERROR_DS_NO_MORE_RIDS
#   endif
#endif
#ifndef ERROR_DS_NO_MORE_RIDS
#define ERROR_DS_NO_MORE_RIDS            8209L
#endif
#ifdef ERROR_DS_INCORRECT_ROLE_OWNER
#   if ERROR_DS_INCORRECT_ROLE_OWNER == ERROR_CONTEXT_EXPIRED
#	undef ERROR_DS_INCORRECT_ROLE_OWNER
#   endif
#endif
#ifndef ERROR_DS_INCORRECT_ROLE_OWNER
#define ERROR_DS_INCORRECT_ROLE_OWNER    8210L
#endif
#ifdef ERROR_DS_RIDMGR_INIT_ERROR
#   if ERROR_DS_RIDMGR_INIT_ERROR == ERROR_PER_USER_TRUST_QUOTA_EXCEEDED
#	undef ERROR_DS_RIDMGR_INIT_ERROR
#   endif
#endif
#ifndef ERROR_DS_RIDMGR_INIT_ERROR
#define ERROR_DS_RIDMGR_INIT_ERROR       8211L
#endif
#ifdef ERROR_DS_OBJ_CLASS_VIOLATION
#   if ERROR_DS_OBJ_CLASS_VIOLATION == ERROR_ALL_USER_TRUST_QUOTA_EXCEEDED
#	undef ERROR_DS_OBJ_CLASS_VIOLATION
#   endif
#endif
#ifndef ERROR_DS_OBJ_CLASS_VIOLATION
#define ERROR_DS_OBJ_CLASS_VIOLATION     8212L
#endif
#ifdef ERROR_DS_CANT_ON_NON_LEAF
#   if ERROR_DS_CANT_ON_NON_LEAF == ERROR_USER_DELETE_TRUST_QUOTA_EXCEEDED
#	undef ERROR_DS_CANT_ON_NON_LEAF
#   endif
#endif
#ifndef ERROR_DS_CANT_ON_NON_LEAF
#define ERROR_DS_CANT_ON_NON_LEAF        8213L
#endif
#ifdef ERROR_DS_CANT_ON_RDN
#   if ERROR_DS_CANT_ON_RDN == ERROR_AUTHENTICATION_FIREWALL_FAILED
#	undef ERROR_DS_CANT_ON_RDN
#   endif
#endif
#ifndef ERROR_DS_CANT_ON_RDN
#define ERROR_DS_CANT_ON_RDN             8214L
#endif
#ifdef ERROR_DS_CANT_MOD_OBJ_CLASS
#   if ERROR_DS_CANT_MOD_OBJ_CLASS == ERROR_REMOTE_PRINT_CONNECTIONS_BLOCKED
#	undef ERROR_DS_CANT_MOD_OBJ_CLASS
#   endif
#endif
#ifndef ERROR_DS_CANT_MOD_OBJ_CLASS
#define ERROR_DS_CANT_MOD_OBJ_CLASS      8215L
#endif
#ifdef ERROR_DS_CROSS_DOM_MOVE_ERROR
#   if ERROR_DS_CROSS_DOM_MOVE_ERROR == 1937L
#	undef ERROR_DS_CROSS_DOM_MOVE_ERROR
#   endif
#endif
#ifndef ERROR_DS_CROSS_DOM_MOVE_ERROR
#define ERROR_DS_CROSS_DOM_MOVE_ERROR    8216L
#endif
#ifdef ERROR_DS_GC_NOT_AVAILABLE
#   if ERROR_DS_GC_NOT_AVAILABLE == 1938L
#	undef ERROR_DS_GC_NOT_AVAILABLE
#   endif
#endif
#ifndef ERROR_DS_GC_NOT_AVAILABLE
#define ERROR_DS_GC_NOT_AVAILABLE        8217L
#endif
#ifndef ERROR_SHARED_POLICY
#define ERROR_SHARED_POLICY              8218L
#endif
#ifndef ERROR_POLICY_OBJECT_NOT_FOUND
#define ERROR_POLICY_OBJECT_NOT_FOUND    8219L
#endif
#ifndef ERROR_POLICY_ONLY_IN_DS
#define ERROR_POLICY_ONLY_IN_DS          8220L
#endif
#ifndef ERROR_PROMOTION_ACTIVE
#define ERROR_PROMOTION_ACTIVE           8221L
#endif
#ifndef ERROR_NO_PROMOTION_ACTIVE
#define ERROR_NO_PROMOTION_ACTIVE        8222L
#endif
#ifndef ERROR_DS_OPERATIONS_ERROR
#define ERROR_DS_OPERATIONS_ERROR        8224L
#endif
#ifndef ERROR_DS_PROTOCOL_ERROR
#define ERROR_DS_PROTOCOL_ERROR          8225L
#endif
#ifndef ERROR_DS_TIMELIMIT_EXCEEDED
#define ERROR_DS_TIMELIMIT_EXCEEDED      8226L
#endif
#ifndef ERROR_DS_SIZELIMIT_EXCEEDED
#define ERROR_DS_SIZELIMIT_EXCEEDED      8227L
#endif
#ifndef ERROR_DS_ADMIN_LIMIT_EXCEEDED
#define ERROR_DS_ADMIN_LIMIT_EXCEEDED    8228L
#endif
#ifndef ERROR_DS_COMPARE_FALSE
#define ERROR_DS_COMPARE_FALSE           8229L
#endif
#ifndef ERROR_DS_COMPARE_TRUE
#define ERROR_DS_COMPARE_TRUE            8230L
#endif
#ifndef ERROR_DS_AUTH_METHOD_NOT_SUPPORTED
#define ERROR_DS_AUTH_METHOD_NOT_SUPPORTED 8231L
#endif
#ifndef ERROR_DS_STRONG_AUTH_REQUIRED
#define ERROR_DS_STRONG_AUTH_REQUIRED    8232L
#endif
#ifndef ERROR_DS_INAPPROPRIATE_AUTH
#define ERROR_DS_INAPPROPRIATE_AUTH      8233L
#endif
#ifndef ERROR_DS_AUTH_UNKNOWN
#define ERROR_DS_AUTH_UNKNOWN            8234L
#endif
#ifndef ERROR_DS_REFERRAL
#define ERROR_DS_REFERRAL                8235L
#endif
#ifndef ERROR_DS_UNAVAILABLE_CRIT_EXTENSION
#define ERROR_DS_UNAVAILABLE_CRIT_EXTENSION 8236L
#endif
#ifndef ERROR_DS_CONFIDENTIALITY_REQUIRED
#define ERROR_DS_CONFIDENTIALITY_REQUIRED 8237L
#endif
#ifndef ERROR_DS_INAPPROPRIATE_MATCHING
#define ERROR_DS_INAPPROPRIATE_MATCHING  8238L
#endif
#ifndef ERROR_DS_CONSTRAINT_VIOLATION
#define ERROR_DS_CONSTRAINT_VIOLATION    8239L
#endif
#ifndef ERROR_DS_NO_SUCH_OBJECT
#define ERROR_DS_NO_SUCH_OBJECT          8240L
#endif
#ifndef ERROR_DS_ALIAS_PROBLEM
#define ERROR_DS_ALIAS_PROBLEM           8241L
#endif
#ifndef ERROR_DS_INVALID_DN_SYNTAX
#define ERROR_DS_INVALID_DN_SYNTAX       8242L
#endif
#ifndef ERROR_DS_IS_LEAF
#define ERROR_DS_IS_LEAF                 8243L
#endif
#ifndef ERROR_DS_ALIAS_DEREF_PROBLEM
#define ERROR_DS_ALIAS_DEREF_PROBLEM     8244L
#endif
#ifndef ERROR_DS_UNWILLING_TO_PERFORM
#define ERROR_DS_UNWILLING_TO_PERFORM    8245L
#endif
#ifndef ERROR_DS_LOOP_DETECT
#define ERROR_DS_LOOP_DETECT             8246L
#endif
#ifndef ERROR_DS_NAMING_VIOLATION
#define ERROR_DS_NAMING_VIOLATION        8247L
#endif
#ifndef ERROR_DS_OBJECT_RESULTS_TOO_LARGE
#define ERROR_DS_OBJECT_RESULTS_TOO_LARGE 8248L
#endif
#ifndef ERROR_DS_AFFECTS_MULTIPLE_DSAS
#define ERROR_DS_AFFECTS_MULTIPLE_DSAS   8249L
#endif
#ifndef ERROR_DS_SERVER_DOWN
#define ERROR_DS_SERVER_DOWN             8250L
#endif
#ifndef ERROR_DS_LOCAL_ERROR
#define ERROR_DS_LOCAL_ERROR             8251L
#endif
#ifndef ERROR_DS_ENCODING_ERROR
#define ERROR_DS_ENCODING_ERROR          8252L
#endif
#ifndef ERROR_DS_DECODING_ERROR
#define ERROR_DS_DECODING_ERROR          8253L
#endif
#ifndef ERROR_DS_FILTER_UNKNOWN
#define ERROR_DS_FILTER_UNKNOWN          8254L
#endif
#ifndef ERROR_DS_PARAM_ERROR
#define ERROR_DS_PARAM_ERROR             8255L
#endif
#ifndef ERROR_DS_NOT_SUPPORTED
#define ERROR_DS_NOT_SUPPORTED           8256L
#endif
#ifndef ERROR_DS_NO_RESULTS_RETURNED
#define ERROR_DS_NO_RESULTS_RETURNED     8257L
#endif
#ifndef ERROR_DS_CONTROL_NOT_FOUND
#define ERROR_DS_CONTROL_NOT_FOUND       8258L
#endif
#ifndef ERROR_DS_CLIENT_LOOP
#define ERROR_DS_CLIENT_LOOP             8259L
#endif
#ifndef ERROR_DS_REFERRAL_LIMIT_EXCEEDED
#define ERROR_DS_REFERRAL_LIMIT_EXCEEDED 8260L
#endif
#ifndef ERROR_DS_SORT_CONTROL_MISSING
#define ERROR_DS_SORT_CONTROL_MISSING    8261L
#endif
#ifndef ERROR_DS_OFFSET_RANGE_ERROR
#define ERROR_DS_OFFSET_RANGE_ERROR      8262L
#endif
#ifndef ERROR_DS_ROOT_MUST_BE_NC
#define ERROR_DS_ROOT_MUST_BE_NC         8301L
#endif
#ifndef ERROR_DS_ADD_REPLICA_INHIBITED
#define ERROR_DS_ADD_REPLICA_INHIBITED   8302L
#endif
#ifndef ERROR_DS_ATT_NOT_DEF_IN_SCHEMA
#define ERROR_DS_ATT_NOT_DEF_IN_SCHEMA   8303L
#endif
#ifndef ERROR_DS_MAX_OBJ_SIZE_EXCEEDED
#define ERROR_DS_MAX_OBJ_SIZE_EXCEEDED   8304L
#endif
#ifndef ERROR_DS_OBJ_STRING_NAME_EXISTS
#define ERROR_DS_OBJ_STRING_NAME_EXISTS  8305L
#endif
#ifndef ERROR_DS_NO_RDN_DEFINED_IN_SCHEMA
#define ERROR_DS_NO_RDN_DEFINED_IN_SCHEMA 8306L
#endif
#ifndef ERROR_DS_RDN_DOESNT_MATCH_SCHEMA
#define ERROR_DS_RDN_DOESNT_MATCH_SCHEMA 8307L
#endif
#ifndef ERROR_DS_NO_REQUESTED_ATTS_FOUND
#define ERROR_DS_NO_REQUESTED_ATTS_FOUND 8308L
#endif
#ifndef ERROR_DS_USER_BUFFER_TO_SMALL
#define ERROR_DS_USER_BUFFER_TO_SMALL    8309L
#endif
#ifndef ERROR_DS_ATT_IS_NOT_ON_OBJ
#define ERROR_DS_ATT_IS_NOT_ON_OBJ       8310L
#endif
#ifndef ERROR_DS_ILLEGAL_MOD_OPERATION
#define ERROR_DS_ILLEGAL_MOD_OPERATION   8311L
#endif
#ifndef ERROR_DS_OBJ_TOO_LARGE
#define ERROR_DS_OBJ_TOO_LARGE           8312L
#endif
#ifndef ERROR_DS_BAD_INSTANCE_TYPE
#define ERROR_DS_BAD_INSTANCE_TYPE       8313L
#endif
#ifndef ERROR_DS_MASTERDSA_REQUIRED
#define ERROR_DS_MASTERDSA_REQUIRED      8314L
#endif
#ifndef ERROR_DS_OBJECT_CLASS_REQUIRED
#define ERROR_DS_OBJECT_CLASS_REQUIRED   8315L
#endif
#ifndef ERROR_DS_MISSING_REQUIRED_ATT
#define ERROR_DS_MISSING_REQUIRED_ATT    8316L
#endif
#ifndef ERROR_DS_ATT_NOT_DEF_FOR_CLASS
#define ERROR_DS_ATT_NOT_DEF_FOR_CLASS   8317L
#endif
#ifndef ERROR_DS_ATT_ALREADY_EXISTS
#define ERROR_DS_ATT_ALREADY_EXISTS      8318L
#endif
#ifndef ERROR_DS_CANT_ADD_ATT_VALUES
#define ERROR_DS_CANT_ADD_ATT_VALUES     8320L
#endif
#ifndef ERROR_DS_SINGLE_VALUE_CONSTRAINT
#define ERROR_DS_SINGLE_VALUE_CONSTRAINT 8321L
#endif
#ifndef ERROR_DS_RANGE_CONSTRAINT
#define ERROR_DS_RANGE_CONSTRAINT        8322L
#endif
#ifndef ERROR_DS_ATT_VAL_ALREADY_EXISTS
#define ERROR_DS_ATT_VAL_ALREADY_EXISTS  8323L
#endif
#ifndef ERROR_DS_CANT_REM_MISSING_ATT
#define ERROR_DS_CANT_REM_MISSING_ATT    8324L
#endif
#ifndef ERROR_DS_CANT_REM_MISSING_ATT_VAL
#define ERROR_DS_CANT_REM_MISSING_ATT_VAL 8325L
#endif
#ifndef ERROR_DS_ROOT_CANT_BE_SUBREF
#define ERROR_DS_ROOT_CANT_BE_SUBREF     8326L
#endif
#ifndef ERROR_DS_NO_CHAINING
#define ERROR_DS_NO_CHAINING             8327L
#endif
#ifndef ERROR_DS_NO_CHAINED_EVAL
#define ERROR_DS_NO_CHAINED_EVAL         8328L
#endif
#ifndef ERROR_DS_NO_PARENT_OBJECT
#define ERROR_DS_NO_PARENT_OBJECT        8329L
#endif
#ifndef ERROR_DS_PARENT_IS_AN_ALIAS
#define ERROR_DS_PARENT_IS_AN_ALIAS      8330L
#endif
#ifndef ERROR_DS_CANT_MIX_MASTER_AND_REPS
#define ERROR_DS_CANT_MIX_MASTER_AND_REPS 8331L
#endif
#ifndef ERROR_DS_CHILDREN_EXIST
#define ERROR_DS_CHILDREN_EXIST          8332L
#endif
#ifndef ERROR_DS_OBJ_NOT_FOUND
#define ERROR_DS_OBJ_NOT_FOUND           8333L
#endif
#ifndef ERROR_DS_ALIASED_OBJ_MISSING
#define ERROR_DS_ALIASED_OBJ_MISSING     8334L
#endif
#ifndef ERROR_DS_BAD_NAME_SYNTAX
#define ERROR_DS_BAD_NAME_SYNTAX         8335L
#endif
#ifndef ERROR_DS_ALIAS_POINTS_TO_ALIAS
#define ERROR_DS_ALIAS_POINTS_TO_ALIAS   8336L
#endif
#ifndef ERROR_DS_CANT_DEREF_ALIAS
#define ERROR_DS_CANT_DEREF_ALIAS        8337L
#endif
#ifndef ERROR_DS_OUT_OF_SCOPE
#define ERROR_DS_OUT_OF_SCOPE            8338L
#endif
#ifndef ERROR_DS_OBJECT_BEING_REMOVED
#define ERROR_DS_OBJECT_BEING_REMOVED    8339L
#endif
#ifndef ERROR_DS_CANT_DELETE_DSA_OBJ
#define ERROR_DS_CANT_DELETE_DSA_OBJ     8340L
#endif
#ifndef ERROR_DS_GENERIC_ERROR
#define ERROR_DS_GENERIC_ERROR           8341L
#endif
#ifndef ERROR_DS_DSA_MUST_BE_INT_MASTER
#define ERROR_DS_DSA_MUST_BE_INT_MASTER  8342L
#endif
#ifndef ERROR_DS_CLASS_NOT_DSA
#define ERROR_DS_CLASS_NOT_DSA           8343L
#endif
#ifndef ERROR_DS_INSUFF_ACCESS_RIGHTS
#define ERROR_DS_INSUFF_ACCESS_RIGHTS    8344L
#endif
#ifndef ERROR_DS_ILLEGAL_SUPERIOR
#define ERROR_DS_ILLEGAL_SUPERIOR        8345L
#endif
#ifndef ERROR_DS_ATTRIBUTE_OWNED_BY_SAM
#define ERROR_DS_ATTRIBUTE_OWNED_BY_SAM  8346L
#endif
#ifndef ERROR_DS_NAME_TOO_MANY_PARTS
#define ERROR_DS_NAME_TOO_MANY_PARTS     8347L
#endif
#ifndef ERROR_DS_NAME_TOO_LONG
#define ERROR_DS_NAME_TOO_LONG           8348L
#endif
#ifndef ERROR_DS_NAME_VALUE_TOO_LONG
#define ERROR_DS_NAME_VALUE_TOO_LONG     8349L
#endif
#ifndef ERROR_DS_NAME_UNPARSEABLE
#define ERROR_DS_NAME_UNPARSEABLE        8350L
#endif
#ifndef ERROR_DS_NAME_TYPE_UNKNOWN
#define ERROR_DS_NAME_TYPE_UNKNOWN       8351L
#endif
#ifndef ERROR_DS_NOT_AN_OBJECT
#define ERROR_DS_NOT_AN_OBJECT           8352L
#endif
#ifndef ERROR_DS_SEC_DESC_TOO_SHORT
#define ERROR_DS_SEC_DESC_TOO_SHORT      8353L
#endif
#ifndef ERROR_DS_SEC_DESC_INVALID
#define ERROR_DS_SEC_DESC_INVALID        8354L
#endif
#ifndef ERROR_DS_NO_DELETED_NAME
#define ERROR_DS_NO_DELETED_NAME         8355L
#endif
#ifndef ERROR_DS_SUBREF_MUST_HAVE_PARENT
#define ERROR_DS_SUBREF_MUST_HAVE_PARENT 8356L
#endif
#ifndef ERROR_DS_NCNAME_MUST_BE_NC
#define ERROR_DS_NCNAME_MUST_BE_NC       8357L
#endif
#ifndef ERROR_DS_CANT_ADD_SYSTEM_ONLY
#define ERROR_DS_CANT_ADD_SYSTEM_ONLY    8358L
#endif
#ifndef ERROR_DS_CLASS_MUST_BE_CONCRETE
#define ERROR_DS_CLASS_MUST_BE_CONCRETE  8359L
#endif
#ifndef ERROR_DS_INVALID_DMD
#define ERROR_DS_INVALID_DMD             8360L
#endif
#ifndef ERROR_DS_OBJ_GUID_EXISTS
#define ERROR_DS_OBJ_GUID_EXISTS         8361L
#endif
#ifndef ERROR_DS_NOT_ON_BACKLINK
#define ERROR_DS_NOT_ON_BACKLINK         8362L
#endif
#ifndef ERROR_DS_NO_CROSSREF_FOR_NC
#define ERROR_DS_NO_CROSSREF_FOR_NC      8363L
#endif
#ifndef ERROR_DS_SHUTTING_DOWN
#define ERROR_DS_SHUTTING_DOWN           8364L
#endif
#ifndef ERROR_DS_UNKNOWN_OPERATION
#define ERROR_DS_UNKNOWN_OPERATION       8365L
#endif
#ifndef ERROR_DS_INVALID_ROLE_OWNER
#define ERROR_DS_INVALID_ROLE_OWNER      8366L
#endif
#ifndef ERROR_DS_COULDNT_CONTACT_FSMO
#define ERROR_DS_COULDNT_CONTACT_FSMO    8367L
#endif
#ifndef ERROR_DS_CROSS_NC_DN_RENAME
#define ERROR_DS_CROSS_NC_DN_RENAME      8368L
#endif
#ifndef ERROR_DS_CANT_MOD_SYSTEM_ONLY
#define ERROR_DS_CANT_MOD_SYSTEM_ONLY    8369L
#endif
#ifndef ERROR_DS_REPLICATOR_ONLY
#define ERROR_DS_REPLICATOR_ONLY         8370L
#endif
#ifndef ERROR_DS_OBJ_CLASS_NOT_DEFINED
#define ERROR_DS_OBJ_CLASS_NOT_DEFINED   8371L
#endif
#ifndef ERROR_DS_OBJ_CLASS_NOT_SUBCLASS
#define ERROR_DS_OBJ_CLASS_NOT_SUBCLASS  8372L
#endif
#ifndef ERROR_DS_NAME_REFERENCE_INVALID
#define ERROR_DS_NAME_REFERENCE_INVALID  8373L
#endif
#ifndef ERROR_DS_CROSS_REF_EXISTS
#define ERROR_DS_CROSS_REF_EXISTS        8374L
#endif
#ifndef ERROR_DS_CANT_DEL_MASTER_CROSSREF
#define ERROR_DS_CANT_DEL_MASTER_CROSSREF 8375L
#endif
#ifndef ERROR_DS_SUBTREE_NOTIFY_NOT_NC_HEAD
#define ERROR_DS_SUBTREE_NOTIFY_NOT_NC_HEAD 8376L
#endif
#ifndef ERROR_DS_NOTIFY_FILTER_TOO_COMPLEX
#define ERROR_DS_NOTIFY_FILTER_TOO_COMPLEX 8377L
#endif
#ifndef ERROR_DS_DUP_RDN
#define ERROR_DS_DUP_RDN                 8378L
#endif
#ifndef ERROR_DS_DUP_OID
#define ERROR_DS_DUP_OID                 8379L
#endif
#ifndef ERROR_DS_DUP_MAPI_ID
#define ERROR_DS_DUP_MAPI_ID             8380L
#endif
#ifndef ERROR_DS_DUP_SCHEMA_ID_GUID
#define ERROR_DS_DUP_SCHEMA_ID_GUID      8381L
#endif
#ifndef ERROR_DS_DUP_LDAP_DISPLAY_NAME
#define ERROR_DS_DUP_LDAP_DISPLAY_NAME   8382L
#endif
#ifndef ERROR_DS_SEMANTIC_ATT_TEST
#define ERROR_DS_SEMANTIC_ATT_TEST       8383L
#endif
#ifndef ERROR_DS_SYNTAX_MISMATCH
#define ERROR_DS_SYNTAX_MISMATCH         8384L
#endif
#ifndef ERROR_DS_EXISTS_IN_MUST_HAVE
#define ERROR_DS_EXISTS_IN_MUST_HAVE     8385L
#endif
#ifndef ERROR_DS_EXISTS_IN_MAY_HAVE
#define ERROR_DS_EXISTS_IN_MAY_HAVE      8386L
#endif
#ifndef ERROR_DS_NONEXISTENT_MAY_HAVE
#define ERROR_DS_NONEXISTENT_MAY_HAVE    8387L
#endif
#ifndef ERROR_DS_NONEXISTENT_MUST_HAVE
#define ERROR_DS_NONEXISTENT_MUST_HAVE   8388L
#endif
#ifndef ERROR_DS_AUX_CLS_TEST_FAIL
#define ERROR_DS_AUX_CLS_TEST_FAIL       8389L
#endif
#ifndef ERROR_DS_NONEXISTENT_POSS_SUP
#define ERROR_DS_NONEXISTENT_POSS_SUP    8390L
#endif
#ifndef ERROR_DS_SUB_CLS_TEST_FAIL
#define ERROR_DS_SUB_CLS_TEST_FAIL       8391L
#endif
#ifndef ERROR_DS_BAD_RDN_ATT_ID_SYNTAX
#define ERROR_DS_BAD_RDN_ATT_ID_SYNTAX   8392L
#endif
#ifndef ERROR_DS_EXISTS_IN_AUX_CLS
#define ERROR_DS_EXISTS_IN_AUX_CLS       8393L
#endif
#ifndef ERROR_DS_EXISTS_IN_SUB_CLS
#define ERROR_DS_EXISTS_IN_SUB_CLS       8394L
#endif
#ifndef ERROR_DS_EXISTS_IN_POSS_SUP
#define ERROR_DS_EXISTS_IN_POSS_SUP      8395L
#endif
#ifndef ERROR_DS_RECALCSCHEMA_FAILED
#define ERROR_DS_RECALCSCHEMA_FAILED     8396L
#endif
#ifndef ERROR_DS_TREE_DELETE_NOT_FINISHED
#define ERROR_DS_TREE_DELETE_NOT_FINISHED 8397L
#endif
#ifndef ERROR_DS_CANT_DELETE
#define ERROR_DS_CANT_DELETE             8398L
#endif
#ifndef ERROR_DS_ATT_SCHEMA_REQ_ID
#define ERROR_DS_ATT_SCHEMA_REQ_ID       8399L
#endif
#ifndef ERROR_DS_BAD_ATT_SCHEMA_SYNTAX
#define ERROR_DS_BAD_ATT_SCHEMA_SYNTAX   8400L
#endif
#ifndef ERROR_DS_CANT_CACHE_ATT
#define ERROR_DS_CANT_CACHE_ATT          8401L
#endif
#ifndef ERROR_DS_CANT_CACHE_CLASS
#define ERROR_DS_CANT_CACHE_CLASS        8402L
#endif
#ifndef ERROR_DS_CANT_REMOVE_ATT_CACHE
#define ERROR_DS_CANT_REMOVE_ATT_CACHE   8403L
#endif
#ifndef ERROR_DS_CANT_REMOVE_CLASS_CACHE
#define ERROR_DS_CANT_REMOVE_CLASS_CACHE 8404L
#endif
#ifndef ERROR_DS_CANT_RETRIEVE_DN
#define ERROR_DS_CANT_RETRIEVE_DN        8405L
#endif
#ifndef ERROR_DS_MISSING_SUPREF
#define ERROR_DS_MISSING_SUPREF          8406L
#endif
#ifndef ERROR_DS_CANT_RETRIEVE_INSTANCE
#define ERROR_DS_CANT_RETRIEVE_INSTANCE  8407L
#endif
#ifndef ERROR_DS_CODE_INCONSISTENCY
#define ERROR_DS_CODE_INCONSISTENCY      8408L
#endif
#ifndef ERROR_DS_DATABASE_ERROR
#define ERROR_DS_DATABASE_ERROR          8409L
#endif
#ifndef ERROR_DS_GOVERNSID_MISSING
#define ERROR_DS_GOVERNSID_MISSING       8410L
#endif
#ifndef ERROR_DS_MISSING_EXPECTED_ATT
#define ERROR_DS_MISSING_EXPECTED_ATT    8411L
#endif
#ifndef ERROR_DS_NCNAME_MISSING_CR_REF
#define ERROR_DS_NCNAME_MISSING_CR_REF   8412L
#endif
#ifndef ERROR_DS_SECURITY_CHECKING_ERROR
#define ERROR_DS_SECURITY_CHECKING_ERROR 8413L
#endif
#ifndef ERROR_DS_SCHEMA_NOT_LOADED
#define ERROR_DS_SCHEMA_NOT_LOADED       8414L
#endif
#ifndef ERROR_DS_SCHEMA_ALLOC_FAILED
#define ERROR_DS_SCHEMA_ALLOC_FAILED     8415L
#endif
#ifndef ERROR_DS_ATT_SCHEMA_REQ_SYNTAX
#define ERROR_DS_ATT_SCHEMA_REQ_SYNTAX   8416L
#endif
#ifndef ERROR_DS_GCVERIFY_ERROR
#define ERROR_DS_GCVERIFY_ERROR          8417L
#endif
#ifndef ERROR_DS_DRA_SCHEMA_MISMATCH
#define ERROR_DS_DRA_SCHEMA_MISMATCH     8418L
#endif
#ifndef ERROR_DS_CANT_FIND_DSA_OBJ
#define ERROR_DS_CANT_FIND_DSA_OBJ       8419L
#endif
#ifndef ERROR_DS_CANT_FIND_EXPECTED_NC
#define ERROR_DS_CANT_FIND_EXPECTED_NC   8420L
#endif
#ifndef ERROR_DS_CANT_FIND_NC_IN_CACHE
#define ERROR_DS_CANT_FIND_NC_IN_CACHE   8421L
#endif
#ifndef ERROR_DS_CANT_RETRIEVE_CHILD
#define ERROR_DS_CANT_RETRIEVE_CHILD     8422L
#endif
#ifndef ERROR_DS_SECURITY_ILLEGAL_MODIFY
#define ERROR_DS_SECURITY_ILLEGAL_MODIFY 8423L
#endif
#ifndef ERROR_DS_CANT_REPLACE_HIDDEN_REC
#define ERROR_DS_CANT_REPLACE_HIDDEN_REC 8424L
#endif
#ifndef ERROR_DS_BAD_HIERARCHY_FILE
#define ERROR_DS_BAD_HIERARCHY_FILE      8425L
#endif
#ifndef ERROR_DS_BUILD_HIERARCHY_TABLE_FAILED
#define ERROR_DS_BUILD_HIERARCHY_TABLE_FAILED 8426L
#endif
#ifndef ERROR_DS_CONFIG_PARAM_MISSING
#define ERROR_DS_CONFIG_PARAM_MISSING    8427L
#endif
#ifndef ERROR_DS_COUNTING_AB_INDICES_FAILED
#define ERROR_DS_COUNTING_AB_INDICES_FAILED 8428L
#endif
#ifndef ERROR_DS_HIERARCHY_TABLE_MALLOC_FAILED
#define ERROR_DS_HIERARCHY_TABLE_MALLOC_FAILED 8429L
#endif
#ifndef ERROR_DS_INTERNAL_FAILURE
#define ERROR_DS_INTERNAL_FAILURE        8430L
#endif
#ifndef ERROR_DS_UNKNOWN_ERROR
#define ERROR_DS_UNKNOWN_ERROR           8431L
#endif
#ifndef ERROR_DS_ROOT_REQUIRES_CLASS_TOP
#define ERROR_DS_ROOT_REQUIRES_CLASS_TOP 8432L
#endif
#ifndef ERROR_DS_REFUSING_FSMO_ROLES
#define ERROR_DS_REFUSING_FSMO_ROLES     8433L
#endif
#ifndef ERROR_DS_MISSING_FSMO_SETTINGS
#define ERROR_DS_MISSING_FSMO_SETTINGS   8434L
#endif
#ifndef ERROR_DS_UNABLE_TO_SURRENDER_ROLES
#define ERROR_DS_UNABLE_TO_SURRENDER_ROLES 8435L
#endif
#ifndef ERROR_DS_DRA_GENERIC
#define ERROR_DS_DRA_GENERIC             8436L
#endif
#ifndef ERROR_DS_DRA_INVALID_PARAMETER
#define ERROR_DS_DRA_INVALID_PARAMETER   8437L
#endif
#ifndef ERROR_DS_DRA_BUSY
#define ERROR_DS_DRA_BUSY                8438L
#endif
#ifndef ERROR_DS_DRA_BAD_DN
#define ERROR_DS_DRA_BAD_DN              8439L
#endif
#ifndef ERROR_DS_DRA_BAD_NC
#define ERROR_DS_DRA_BAD_NC              8440L
#endif
#ifndef ERROR_DS_DRA_DN_EXISTS
#define ERROR_DS_DRA_DN_EXISTS           8441L
#endif
#ifndef ERROR_DS_DRA_INTERNAL_ERROR
#define ERROR_DS_DRA_INTERNAL_ERROR      8442L
#endif
#ifndef ERROR_DS_DRA_INCONSISTENT_DIT
#define ERROR_DS_DRA_INCONSISTENT_DIT    8443L
#endif
#ifndef ERROR_DS_DRA_CONNECTION_FAILED
#define ERROR_DS_DRA_CONNECTION_FAILED   8444L
#endif
#ifndef ERROR_DS_DRA_BAD_INSTANCE_TYPE
#define ERROR_DS_DRA_BAD_INSTANCE_TYPE   8445L
#endif
#ifndef ERROR_DS_DRA_OUT_OF_MEM
#define ERROR_DS_DRA_OUT_OF_MEM          8446L
#endif
#ifndef ERROR_DS_DRA_MAIL_PROBLEM
#define ERROR_DS_DRA_MAIL_PROBLEM        8447L
#endif
#ifndef ERROR_DS_DRA_REF_ALREADY_EXISTS
#define ERROR_DS_DRA_REF_ALREADY_EXISTS  8448L
#endif
#ifndef ERROR_DS_DRA_REF_NOT_FOUND
#define ERROR_DS_DRA_REF_NOT_FOUND       8449L
#endif
#ifndef ERROR_DS_DRA_OBJ_IS_REP_SOURCE
#define ERROR_DS_DRA_OBJ_IS_REP_SOURCE   8450L
#endif
#ifndef ERROR_DS_DRA_DB_ERROR
#define ERROR_DS_DRA_DB_ERROR            8451L
#endif
#ifndef ERROR_DS_DRA_NO_REPLICA
#define ERROR_DS_DRA_NO_REPLICA          8452L
#endif
#ifndef ERROR_DS_DRA_ACCESS_DENIED
#define ERROR_DS_DRA_ACCESS_DENIED       8453L
#endif
#ifndef ERROR_DS_DRA_NOT_SUPPORTED
#define ERROR_DS_DRA_NOT_SUPPORTED       8454L
#endif
#ifndef ERROR_DS_DRA_RPC_CANCELLED
#define ERROR_DS_DRA_RPC_CANCELLED       8455L
#endif
#ifndef ERROR_DS_DRA_SOURCE_DISABLED
#define ERROR_DS_DRA_SOURCE_DISABLED     8456L
#endif
#ifndef ERROR_DS_DRA_SINK_DISABLED
#define ERROR_DS_DRA_SINK_DISABLED       8457L
#endif
#ifndef ERROR_DS_DRA_NAME_COLLISION
#define ERROR_DS_DRA_NAME_COLLISION      8458L
#endif
#ifndef ERROR_DS_DRA_SOURCE_REINSTALLED
#define ERROR_DS_DRA_SOURCE_REINSTALLED  8459L
#endif
#ifndef ERROR_DS_DRA_MISSING_PARENT
#define ERROR_DS_DRA_MISSING_PARENT      8460L
#endif
#ifndef ERROR_DS_DRA_PREEMPTED
#define ERROR_DS_DRA_PREEMPTED           8461L
#endif
#ifndef ERROR_DS_DRA_ABANDON_SYNC
#define ERROR_DS_DRA_ABANDON_SYNC        8462L
#endif
#ifndef ERROR_DS_DRA_SHUTDOWN
#define ERROR_DS_DRA_SHUTDOWN            8463L
#endif
#ifndef ERROR_DS_DRA_INCOMPATIBLE_PARTIAL_SET
#define ERROR_DS_DRA_INCOMPATIBLE_PARTIAL_SET 8464L
#endif
#ifndef ERROR_DS_DRA_SOURCE_IS_PARTIAL_REPLICA
#define ERROR_DS_DRA_SOURCE_IS_PARTIAL_REPLICA 8465L
#endif
#ifndef ERROR_DS_DRA_EXTN_CONNECTION_FAILED
#define ERROR_DS_DRA_EXTN_CONNECTION_FAILED 8466L
#endif
#ifndef ERROR_DS_INSTALL_SCHEMA_MISMATCH
#define ERROR_DS_INSTALL_SCHEMA_MISMATCH 8467L
#endif
#ifndef ERROR_DS_DUP_LINK_ID
#define ERROR_DS_DUP_LINK_ID             8468L
#endif
#ifndef ERROR_DS_NAME_ERROR_RESOLVING
#define ERROR_DS_NAME_ERROR_RESOLVING    8469L
#endif
#ifndef ERROR_DS_NAME_ERROR_NOT_FOUND
#define ERROR_DS_NAME_ERROR_NOT_FOUND    8470L
#endif
#ifndef ERROR_DS_NAME_ERROR_NOT_UNIQUE
#define ERROR_DS_NAME_ERROR_NOT_UNIQUE   8471L
#endif
#ifndef ERROR_DS_NAME_ERROR_NO_MAPPING
#define ERROR_DS_NAME_ERROR_NO_MAPPING   8472L
#endif
#ifndef ERROR_DS_NAME_ERROR_DOMAIN_ONLY
#define ERROR_DS_NAME_ERROR_DOMAIN_ONLY  8473L
#endif
#ifndef ERROR_DS_NAME_ERROR_NO_SYNTACTICAL_MAPPING
#define ERROR_DS_NAME_ERROR_NO_SYNTACTICAL_MAPPING 8474L
#endif
#ifndef ERROR_DS_CONSTRUCTED_ATT_MOD
#define ERROR_DS_CONSTRUCTED_ATT_MOD     8475L
#endif
#ifndef ERROR_DS_WRONG_OM_OBJ_CLASS
#define ERROR_DS_WRONG_OM_OBJ_CLASS      8476L
#endif
#ifndef ERROR_DS_DRA_REPL_PENDING
#define ERROR_DS_DRA_REPL_PENDING        8477L
#endif
#ifndef ERROR_DS_DS_REQUIRED
#define ERROR_DS_DS_REQUIRED             8478L
#endif
#ifndef ERROR_DS_INVALID_LDAP_DISPLAY_NAME
#define ERROR_DS_INVALID_LDAP_DISPLAY_NAME 8479L
#endif
#ifndef ERROR_DS_NON_BASE_SEARCH
#define ERROR_DS_NON_BASE_SEARCH         8480L
#endif
#ifndef ERROR_DS_CANT_RETRIEVE_ATTS
#define ERROR_DS_CANT_RETRIEVE_ATTS      8481L
#endif
#ifndef ERROR_DS_BACKLINK_WITHOUT_LINK
#define ERROR_DS_BACKLINK_WITHOUT_LINK   8482L
#endif
#ifndef ERROR_DS_EPOCH_MISMATCH
#define ERROR_DS_EPOCH_MISMATCH          8483L
#endif
#ifndef ERROR_DS_SRC_NAME_MISMATCH
#define ERROR_DS_SRC_NAME_MISMATCH       8484L
#endif
#ifndef ERROR_DS_SRC_AND_DST_NC_IDENTICAL
#define ERROR_DS_SRC_AND_DST_NC_IDENTICAL 8485L
#endif
#ifndef ERROR_DS_DST_NC_MISMATCH
#define ERROR_DS_DST_NC_MISMATCH         8486L
#endif
#ifndef ERROR_DS_NOT_AUTHORITIVE_FOR_DST_NC
#define ERROR_DS_NOT_AUTHORITIVE_FOR_DST_NC 8487L
#endif
#ifndef ERROR_DS_SRC_GUID_MISMATCH
#define ERROR_DS_SRC_GUID_MISMATCH       8488L
#endif
#ifndef ERROR_DS_CANT_MOVE_DELETED_OBJECT
#define ERROR_DS_CANT_MOVE_DELETED_OBJECT 8489L
#endif
#ifndef ERROR_DS_PDC_OPERATION_IN_PROGRESS
#define ERROR_DS_PDC_OPERATION_IN_PROGRESS 8490L
#endif
#ifndef ERROR_DS_CROSS_DOMAIN_CLEANUP_REQD
#define ERROR_DS_CROSS_DOMAIN_CLEANUP_REQD 8491L
#endif
#ifndef ERROR_DS_ILLEGAL_XDOM_MOVE_OPERATION
#define ERROR_DS_ILLEGAL_XDOM_MOVE_OPERATION 8492L
#endif
#ifndef ERROR_DS_CANT_WITH_ACCT_GROUP_MEMBERSHPS
#define ERROR_DS_CANT_WITH_ACCT_GROUP_MEMBERSHPS 8493L
#endif
#ifndef ERROR_DS_NC_MUST_HAVE_NC_PARENT
#define ERROR_DS_NC_MUST_HAVE_NC_PARENT  8494L
#endif
#ifndef ERROR_DS_CR_IMPOSSIBLE_TO_VALIDATE
#define ERROR_DS_CR_IMPOSSIBLE_TO_VALIDATE 8495L
#endif
#ifndef ERROR_DS_DST_DOMAIN_NOT_NATIVE
#define ERROR_DS_DST_DOMAIN_NOT_NATIVE   8496L
#endif
#ifndef ERROR_DS_MISSING_INFRASTRUCTURE_CONTAINER
#define ERROR_DS_MISSING_INFRASTRUCTURE_CONTAINER 8497L
#endif
#ifndef ERROR_DS_CANT_MOVE_ACCOUNT_GROUP
#define ERROR_DS_CANT_MOVE_ACCOUNT_GROUP 8498L
#endif
#ifndef ERROR_DS_CANT_MOVE_RESOURCE_GROUP
#define ERROR_DS_CANT_MOVE_RESOURCE_GROUP 8499L
#endif
#ifndef ERROR_DS_INVALID_SEARCH_FLAG
#define ERROR_DS_INVALID_SEARCH_FLAG     8500L
#endif
#ifndef ERROR_DS_NO_TREE_DELETE_ABOVE_NC
#define ERROR_DS_NO_TREE_DELETE_ABOVE_NC 8501L
#endif
#ifndef ERROR_DS_COULDNT_LOCK_TREE_FOR_DELETE
#define ERROR_DS_COULDNT_LOCK_TREE_FOR_DELETE 8502L
#endif
#ifndef ERROR_DS_COULDNT_IDENTIFY_OBJECTS_FOR_TREE_DELETE
#define ERROR_DS_COULDNT_IDENTIFY_OBJECTS_FOR_TREE_DELETE 8503L
#endif
#ifndef ERROR_DS_SAM_INIT_FAILURE
#define ERROR_DS_SAM_INIT_FAILURE        8504L
#endif
#ifndef ERROR_DS_SENSITIVE_GROUP_VIOLATION
#define ERROR_DS_SENSITIVE_GROUP_VIOLATION 8505L
#endif
#ifndef ERROR_DS_CANT_MOD_PRIMARYGROUPID
#define ERROR_DS_CANT_MOD_PRIMARYGROUPID 8506L
#endif
#ifndef ERROR_DS_ILLEGAL_BASE_SCHEMA_MOD
#define ERROR_DS_ILLEGAL_BASE_SCHEMA_MOD 8507L
#endif
#ifndef ERROR_DS_NONSAFE_SCHEMA_CHANGE
#define ERROR_DS_NONSAFE_SCHEMA_CHANGE   8508L
#endif
#ifndef ERROR_DS_SCHEMA_UPDATE_DISALLOWED
#define ERROR_DS_SCHEMA_UPDATE_DISALLOWED 8509L
#endif
#ifndef ERROR_DS_CANT_CREATE_UNDER_SCHEMA
#define ERROR_DS_CANT_CREATE_UNDER_SCHEMA 8510L
#endif
#ifndef ERROR_DS_INSTALL_NO_SRC_SCH_VERSION
#define ERROR_DS_INSTALL_NO_SRC_SCH_VERSION 8511L
#endif
#ifndef ERROR_DS_INSTALL_NO_SCH_VERSION_IN_INIFILE
#define ERROR_DS_INSTALL_NO_SCH_VERSION_IN_INIFILE 8512L
#endif
#ifndef ERROR_DS_INVALID_GROUP_TYPE
#define ERROR_DS_INVALID_GROUP_TYPE      8513L
#endif
#ifndef ERROR_DS_NO_NEST_GLOBALGROUP_IN_MIXEDDOMAIN
#define ERROR_DS_NO_NEST_GLOBALGROUP_IN_MIXEDDOMAIN 8514L
#endif
#ifndef ERROR_DS_NO_NEST_LOCALGROUP_IN_MIXEDDOMAIN
#define ERROR_DS_NO_NEST_LOCALGROUP_IN_MIXEDDOMAIN 8515L
#endif
#ifndef ERROR_DS_GLOBAL_CANT_HAVE_LOCAL_MEMBER
#define ERROR_DS_GLOBAL_CANT_HAVE_LOCAL_MEMBER 8516L
#endif
#ifndef ERROR_DS_GLOBAL_CANT_HAVE_UNIVERSAL_MEMBER
#define ERROR_DS_GLOBAL_CANT_HAVE_UNIVERSAL_MEMBER 8517L
#endif
#ifndef ERROR_DS_UNIVERSAL_CANT_HAVE_LOCAL_MEMBER
#define ERROR_DS_UNIVERSAL_CANT_HAVE_LOCAL_MEMBER 8518L
#endif
#ifndef ERROR_DS_GLOBAL_CANT_HAVE_CROSSDOMAIN_MEMBER
#define ERROR_DS_GLOBAL_CANT_HAVE_CROSSDOMAIN_MEMBER 8519L
#endif
#ifndef ERROR_DS_LOCAL_CANT_HAVE_CROSSDOMAIN_LOCAL_MEMBER
#define ERROR_DS_LOCAL_CANT_HAVE_CROSSDOMAIN_LOCAL_MEMBER 8520L
#endif
#ifndef ERROR_DS_HAVE_PRIMARY_MEMBERS
#define ERROR_DS_HAVE_PRIMARY_MEMBERS    8521L
#endif
#ifndef ERROR_DS_STRING_SD_CONVERSION_FAILED
#define ERROR_DS_STRING_SD_CONVERSION_FAILED 8522L
#endif
#ifndef ERROR_DS_NAMING_MASTER_GC
#define ERROR_DS_NAMING_MASTER_GC        8523L
#endif
#ifndef ERROR_DS_DNS_LOOKUP_FAILURE
#define ERROR_DS_DNS_LOOKUP_FAILURE      8524L
#endif
#ifndef ERROR_DS_COULDNT_UPDATE_SPNS
#define ERROR_DS_COULDNT_UPDATE_SPNS     8525L
#endif
#ifndef ERROR_DS_CANT_RETRIEVE_SD
#define ERROR_DS_CANT_RETRIEVE_SD        8526L
#endif
#ifndef ERROR_DS_KEY_NOT_UNIQUE
#define ERROR_DS_KEY_NOT_UNIQUE          8527L
#endif
#ifndef ERROR_DS_WRONG_LINKED_ATT_SYNTAX
#define ERROR_DS_WRONG_LINKED_ATT_SYNTAX 8528L
#endif
#ifndef ERROR_DS_SAM_NEED_BOOTKEY_PASSWORD
#define ERROR_DS_SAM_NEED_BOOTKEY_PASSWORD 8529L
#endif
#ifndef ERROR_DS_SAM_NEED_BOOTKEY_FLOPPY
#define ERROR_DS_SAM_NEED_BOOTKEY_FLOPPY 8530L
#endif
#ifndef ERROR_DS_CANT_START
#define ERROR_DS_CANT_START              8531L
#endif
#ifndef ERROR_DS_INIT_FAILURE
#define ERROR_DS_INIT_FAILURE            8532L
#endif
#ifndef ERROR_DS_NO_PKT_PRIVACY_ON_CONNECTION
#define ERROR_DS_NO_PKT_PRIVACY_ON_CONNECTION 8533L
#endif
#ifndef ERROR_DS_SOURCE_DOMAIN_IN_FOREST
#define ERROR_DS_SOURCE_DOMAIN_IN_FOREST 8534L
#endif
#ifndef ERROR_DS_DESTINATION_DOMAIN_NOT_IN_FOREST
#define ERROR_DS_DESTINATION_DOMAIN_NOT_IN_FOREST 8535L
#endif
#ifndef ERROR_DS_DESTINATION_AUDITING_NOT_ENABLED
#define ERROR_DS_DESTINATION_AUDITING_NOT_ENABLED 8536L
#endif
#ifndef ERROR_DS_CANT_FIND_DC_FOR_SRC_DOMAIN
#define ERROR_DS_CANT_FIND_DC_FOR_SRC_DOMAIN 8537L
#endif
#ifndef ERROR_DS_SRC_OBJ_NOT_GROUP_OR_USER
#define ERROR_DS_SRC_OBJ_NOT_GROUP_OR_USER 8538L
#endif
#ifndef ERROR_DS_SRC_SID_EXISTS_IN_FOREST
#define ERROR_DS_SRC_SID_EXISTS_IN_FOREST 8539L
#endif
#ifndef ERROR_DS_SRC_AND_DST_OBJECT_CLASS_MISMATCH
#define ERROR_DS_SRC_AND_DST_OBJECT_CLASS_MISMATCH 8540L
#endif
#ifndef ERROR_SAM_INIT_FAILURE
#define ERROR_SAM_INIT_FAILURE           8541L
#endif
#ifndef ERROR_DS_DRA_SCHEMA_INFO_SHIP
#define ERROR_DS_DRA_SCHEMA_INFO_SHIP    8542L
#endif
#ifndef ERROR_DS_DRA_SCHEMA_CONFLICT
#define ERROR_DS_DRA_SCHEMA_CONFLICT     8543L
#endif
#ifndef ERROR_DS_DRA_EARLIER_SCHEMA_CONFLICT
#define ERROR_DS_DRA_EARLIER_SCHEMA_CONFLICT 8544L
#endif
#ifndef ERROR_DS_DRA_OBJ_NC_MISMATCH
#define ERROR_DS_DRA_OBJ_NC_MISMATCH     8545L
#endif
#ifndef ERROR_DS_NC_STILL_HAS_DSAS
#define ERROR_DS_NC_STILL_HAS_DSAS       8546L
#endif
#ifndef ERROR_DS_GC_REQUIRED
#define ERROR_DS_GC_REQUIRED             8547L
#endif
#ifndef ERROR_DS_LOCAL_MEMBER_OF_LOCAL_ONLY
#define ERROR_DS_LOCAL_MEMBER_OF_LOCAL_ONLY 8548L
#endif
#ifndef ERROR_DS_NO_FPO_IN_UNIVERSAL_GROUPS
#define ERROR_DS_NO_FPO_IN_UNIVERSAL_GROUPS 8549L
#endif
#ifndef ERROR_DS_CANT_ADD_TO_GC
#define ERROR_DS_CANT_ADD_TO_GC          8550L
#endif
#ifndef ERROR_DS_NO_CHECKPOINT_WITH_PDC
#define ERROR_DS_NO_CHECKPOINT_WITH_PDC  8551L
#endif
#ifndef ERROR_DS_SOURCE_AUDITING_NOT_ENABLED
#define ERROR_DS_SOURCE_AUDITING_NOT_ENABLED 8552L
#endif
#ifndef ERROR_DS_CANT_CREATE_IN_NONDOMAIN_NC
#define ERROR_DS_CANT_CREATE_IN_NONDOMAIN_NC 8553L
#endif
#ifndef ERROR_DS_INVALID_NAME_FOR_SPN
#define ERROR_DS_INVALID_NAME_FOR_SPN    8554L
#endif
#ifndef ERROR_DS_FILTER_USES_CONTRUCTED_ATTRS
#define ERROR_DS_FILTER_USES_CONTRUCTED_ATTRS 8555L
#endif
#ifndef ERROR_DS_UNICODEPWD_NOT_IN_QUOTES
#define ERROR_DS_UNICODEPWD_NOT_IN_QUOTES 8556L
#endif
#ifndef ERROR_DS_MACHINE_ACCOUNT_QUOTA_EXCEEDED
#define ERROR_DS_MACHINE_ACCOUNT_QUOTA_EXCEEDED 8557L
#endif
#ifndef ERROR_DS_MUST_BE_RUN_ON_DST_DC
#define ERROR_DS_MUST_BE_RUN_ON_DST_DC   8558L
#endif
#ifndef ERROR_DS_SRC_DC_MUST_BE_SP4_OR_GREATER
#define ERROR_DS_SRC_DC_MUST_BE_SP4_OR_GREATER 8559L
#endif
#ifndef ERROR_DS_CANT_TREE_DELETE_CRITICAL_OBJ
#define ERROR_DS_CANT_TREE_DELETE_CRITICAL_OBJ 8560L
#endif
#ifndef ERROR_DS_INIT_FAILURE_CONSOLE
#define ERROR_DS_INIT_FAILURE_CONSOLE    8561L
#endif
#ifndef ERROR_DS_SAM_INIT_FAILURE_CONSOLE
#define ERROR_DS_SAM_INIT_FAILURE_CONSOLE 8562L
#endif
#ifndef ERROR_DS_FOREST_VERSION_TOO_HIGH
#define ERROR_DS_FOREST_VERSION_TOO_HIGH 8563L
#endif
#ifndef ERROR_DS_DOMAIN_VERSION_TOO_HIGH
#define ERROR_DS_DOMAIN_VERSION_TOO_HIGH 8564L
#endif
#ifndef ERROR_DS_FOREST_VERSION_TOO_LOW
#define ERROR_DS_FOREST_VERSION_TOO_LOW  8565L
#endif
#ifndef ERROR_DS_DOMAIN_VERSION_TOO_LOW
#define ERROR_DS_DOMAIN_VERSION_TOO_LOW  8566L
#endif
#ifndef ERROR_DS_INCOMPATIBLE_VERSION
#define ERROR_DS_INCOMPATIBLE_VERSION    8567L
#endif
#ifndef ERROR_DS_LOW_DSA_VERSION
#define ERROR_DS_LOW_DSA_VERSION         8568L
#endif
#ifndef ERROR_DS_NO_BEHAVIOR_VERSION_IN_MIXEDDOMAIN
#define ERROR_DS_NO_BEHAVIOR_VERSION_IN_MIXEDDOMAIN 8569L
#endif
#ifndef ERROR_DS_NOT_SUPPORTED_SORT_ORDER
#define ERROR_DS_NOT_SUPPORTED_SORT_ORDER 8570L
#endif
#ifndef ERROR_DS_NAME_NOT_UNIQUE
#define ERROR_DS_NAME_NOT_UNIQUE         8571L
#endif
#ifndef ERROR_DS_MACHINE_ACCOUNT_CREATED_PRENT4
#define ERROR_DS_MACHINE_ACCOUNT_CREATED_PRENT4 8572L
#endif
#ifndef ERROR_DS_OUT_OF_VERSION_STORE
#define ERROR_DS_OUT_OF_VERSION_STORE    8573L
#endif
#ifndef ERROR_DS_INCOMPATIBLE_CONTROLS_USED
#define ERROR_DS_INCOMPATIBLE_CONTROLS_USED 8574L
#endif
#ifndef ERROR_DS_NO_REF_DOMAIN
#define ERROR_DS_NO_REF_DOMAIN           8575L
#endif
#ifndef ERROR_DS_RESERVED_LINK_ID
#define ERROR_DS_RESERVED_LINK_ID        8576L
#endif
#ifndef ERROR_DS_LINK_ID_NOT_AVAILABLE
#define ERROR_DS_LINK_ID_NOT_AVAILABLE   8577L
#endif
#ifndef ERROR_DS_AG_CANT_HAVE_UNIVERSAL_MEMBER
#define ERROR_DS_AG_CANT_HAVE_UNIVERSAL_MEMBER 8578L
#endif
#ifndef ERROR_DS_MODIFYDN_DISALLOWED_BY_INSTANCE_TYPE
#define ERROR_DS_MODIFYDN_DISALLOWED_BY_INSTANCE_TYPE 8579L
#endif
#ifndef ERROR_DS_NO_OBJECT_MOVE_IN_SCHEMA_NC
#define ERROR_DS_NO_OBJECT_MOVE_IN_SCHEMA_NC 8580L
#endif
#ifndef ERROR_DS_MODIFYDN_DISALLOWED_BY_FLAG
#define ERROR_DS_MODIFYDN_DISALLOWED_BY_FLAG 8581L
#endif
#ifndef ERROR_DS_MODIFYDN_WRONG_GRANDPARENT
#define ERROR_DS_MODIFYDN_WRONG_GRANDPARENT 8582L
#endif
#ifndef ERROR_DS_NAME_ERROR_TRUST_REFERRAL
#define ERROR_DS_NAME_ERROR_TRUST_REFERRAL 8583L
#endif
#ifndef ERROR_NOT_SUPPORTED_ON_STANDARD_SERVER
#define ERROR_NOT_SUPPORTED_ON_STANDARD_SERVER 8584L
#endif
#ifndef ERROR_DS_CANT_ACCESS_REMOTE_PART_OF_AD
#define ERROR_DS_CANT_ACCESS_REMOTE_PART_OF_AD 8585L
#endif
#ifndef ERROR_DS_CR_IMPOSSIBLE_TO_VALIDATE_V2
#define ERROR_DS_CR_IMPOSSIBLE_TO_VALIDATE_V2 8586L
#endif
#ifndef ERROR_DS_THREAD_LIMIT_EXCEEDED
#define ERROR_DS_THREAD_LIMIT_EXCEEDED   8587L
#endif
#ifndef ERROR_DS_NOT_CLOSEST
#define ERROR_DS_NOT_CLOSEST             8588L
#endif
#ifndef ERROR_DS_CANT_DERIVE_SPN_WITHOUT_SERVER_REF
#define ERROR_DS_CANT_DERIVE_SPN_WITHOUT_SERVER_REF 8589L
#endif
#ifndef ERROR_DS_SINGLE_USER_MODE_FAILED
#define ERROR_DS_SINGLE_USER_MODE_FAILED 8590L
#endif
#ifndef ERROR_DS_NTDSCRIPT_SYNTAX_ERROR
#define ERROR_DS_NTDSCRIPT_SYNTAX_ERROR  8591L
#endif
#ifndef ERROR_DS_NTDSCRIPT_PROCESS_ERROR
#define ERROR_DS_NTDSCRIPT_PROCESS_ERROR 8592L
#endif
#ifndef ERROR_DS_DIFFERENT_REPL_EPOCHS
#define ERROR_DS_DIFFERENT_REPL_EPOCHS   8593L
#endif
#ifndef ERROR_DS_DRS_EXTENSIONS_CHANGED
#define ERROR_DS_DRS_EXTENSIONS_CHANGED  8594L
#endif
#ifndef ERROR_DS_REPLICA_SET_CHANGE_NOT_ALLOWED_ON_DISABLED_CR
#define ERROR_DS_REPLICA_SET_CHANGE_NOT_ALLOWED_ON_DISABLED_CR 8595L
#endif
#ifndef ERROR_DS_NO_MSDS_INTID
#define ERROR_DS_NO_MSDS_INTID           8596L
#endif
#ifndef ERROR_DS_DUP_MSDS_INTID
#define ERROR_DS_DUP_MSDS_INTID          8597L
#endif
#ifndef ERROR_DS_EXISTS_IN_RDNATTID
#define ERROR_DS_EXISTS_IN_RDNATTID      8598L
#endif
#ifndef ERROR_DS_AUTHORIZATION_FAILED
#define ERROR_DS_AUTHORIZATION_FAILED    8599L
#endif
#ifndef ERROR_DS_INVALID_SCRIPT
#define ERROR_DS_INVALID_SCRIPT          8600L
#endif
#ifndef ERROR_DS_REMOTE_CROSSREF_OP_FAILED
#define ERROR_DS_REMOTE_CROSSREF_OP_FAILED 8601L
#endif
#ifndef ERROR_DS_CROSS_REF_BUSY
#define ERROR_DS_CROSS_REF_BUSY          8602L
#endif
#ifndef ERROR_DS_CANT_DERIVE_SPN_FOR_DELETED_DOMAIN
#define ERROR_DS_CANT_DERIVE_SPN_FOR_DELETED_DOMAIN 8603L
#endif
#ifndef ERROR_DS_CANT_DEMOTE_WITH_WRITEABLE_NC
#define ERROR_DS_CANT_DEMOTE_WITH_WRITEABLE_NC 8604L
#endif
#ifndef ERROR_DS_DUPLICATE_ID_FOUND
#define ERROR_DS_DUPLICATE_ID_FOUND      8605L
#endif
#ifndef ERROR_DS_INSUFFICIENT_ATTR_TO_CREATE_OBJECT
#define ERROR_DS_INSUFFICIENT_ATTR_TO_CREATE_OBJECT 8606L
#endif
#ifndef ERROR_DS_GROUP_CONVERSION_ERROR
#define ERROR_DS_GROUP_CONVERSION_ERROR  8607L
#endif
#ifndef ERROR_DS_CANT_MOVE_APP_BASIC_GROUP
#define ERROR_DS_CANT_MOVE_APP_BASIC_GROUP 8608L
#endif
#ifndef ERROR_DS_CANT_MOVE_APP_QUERY_GROUP
#define ERROR_DS_CANT_MOVE_APP_QUERY_GROUP 8609L
#endif
#ifndef ERROR_DS_ROLE_NOT_VERIFIED
#define ERROR_DS_ROLE_NOT_VERIFIED       8610L
#endif
#ifndef ERROR_DS_WKO_CONTAINER_CANNOT_BE_SPECIAL
#define ERROR_DS_WKO_CONTAINER_CANNOT_BE_SPECIAL 8611L
#endif
#ifndef ERROR_DS_DOMAIN_RENAME_IN_PROGRESS
#define ERROR_DS_DOMAIN_RENAME_IN_PROGRESS 8612L
#endif
#ifndef ERROR_DS_EXISTING_AD_CHILD_NC
#define ERROR_DS_EXISTING_AD_CHILD_NC    8613L
#endif
#ifndef ERROR_DS_REPL_LIFETIME_EXCEEDED
#define ERROR_DS_REPL_LIFETIME_EXCEEDED  8614L
#endif
#ifndef ERROR_DS_DISALLOWED_IN_SYSTEM_CONTAINER
#define ERROR_DS_DISALLOWED_IN_SYSTEM_CONTAINER 8615L
#endif
#ifndef ERROR_DS_LDAP_SEND_QUEUE_FULL
#define ERROR_DS_LDAP_SEND_QUEUE_FULL    8616L
#endif
#ifndef ERROR_DS_DRA_OUT_SCHEDULE_WINDOW
#define ERROR_DS_DRA_OUT_SCHEDULE_WINDOW 8617L
#endif
#ifndef DNS_ERROR_RCODE_FORMAT_ERROR
#define DNS_ERROR_RCODE_FORMAT_ERROR     9001L
#endif
#ifndef DNS_ERROR_RCODE_SERVER_FAILURE
#define DNS_ERROR_RCODE_SERVER_FAILURE   9002L
#endif
#ifndef DNS_ERROR_RCODE_NAME_ERROR
#define DNS_ERROR_RCODE_NAME_ERROR       9003L
#endif
#ifndef DNS_ERROR_RCODE_NOT_IMPLEMENTED
#define DNS_ERROR_RCODE_NOT_IMPLEMENTED  9004L
#endif
#ifndef DNS_ERROR_RCODE_REFUSED
#define DNS_ERROR_RCODE_REFUSED          9005L
#endif
#ifndef DNS_ERROR_RCODE_YXDOMAIN
#define DNS_ERROR_RCODE_YXDOMAIN         9006L
#endif
#ifndef DNS_ERROR_RCODE_YXRRSET
#define DNS_ERROR_RCODE_YXRRSET          9007L
#endif
#ifndef DNS_ERROR_RCODE_NXRRSET
#define DNS_ERROR_RCODE_NXRRSET          9008L
#endif
#ifndef DNS_ERROR_RCODE_NOTAUTH
#define DNS_ERROR_RCODE_NOTAUTH          9009L
#endif
#ifndef DNS_ERROR_RCODE_NOTZONE
#define DNS_ERROR_RCODE_NOTZONE          9010L
#endif
#ifndef DNS_ERROR_RCODE_BADSIG
#define DNS_ERROR_RCODE_BADSIG           9016L
#endif
#ifndef DNS_ERROR_RCODE_BADKEY
#define DNS_ERROR_RCODE_BADKEY           9017L
#endif
#ifndef DNS_ERROR_RCODE_BADTIME
#define DNS_ERROR_RCODE_BADTIME          9018L
#endif
#ifndef DNS_INFO_NO_RECORDS
#define DNS_INFO_NO_RECORDS              9501L
#endif
#ifndef DNS_ERROR_BAD_PACKET
#define DNS_ERROR_BAD_PACKET             9502L
#endif
#ifndef DNS_ERROR_NO_PACKET
#define DNS_ERROR_NO_PACKET              9503L
#endif
#ifndef DNS_ERROR_RCODE
#define DNS_ERROR_RCODE                  9504L
#endif
#ifndef DNS_ERROR_UNSECURE_PACKET
#define DNS_ERROR_UNSECURE_PACKET        9505L
#endif
#ifndef DNS_ERROR_INVALID_TYPE
#define DNS_ERROR_INVALID_TYPE           9551L
#endif
#ifndef DNS_ERROR_INVALID_IP_ADDRESS
#define DNS_ERROR_INVALID_IP_ADDRESS     9552L
#endif
#ifndef DNS_ERROR_INVALID_PROPERTY
#define DNS_ERROR_INVALID_PROPERTY       9553L
#endif
#ifndef DNS_ERROR_TRY_AGAIN_LATER
#define DNS_ERROR_TRY_AGAIN_LATER        9554L
#endif
#ifndef DNS_ERROR_NOT_UNIQUE
#define DNS_ERROR_NOT_UNIQUE             9555L
#endif
#ifndef DNS_ERROR_NON_RFC_NAME
#define DNS_ERROR_NON_RFC_NAME           9556L
#endif
#ifndef DNS_STATUS_FQDN
#define DNS_STATUS_FQDN                  9557L
#endif
#ifndef DNS_STATUS_DOTTED_NAME
#define DNS_STATUS_DOTTED_NAME           9558L
#endif
#ifndef DNS_STATUS_SINGLE_PART_NAME
#define DNS_STATUS_SINGLE_PART_NAME      9559L
#endif
#ifndef DNS_ERROR_INVALID_NAME_CHAR
#define DNS_ERROR_INVALID_NAME_CHAR      9560L
#endif
#ifndef DNS_ERROR_NUMERIC_NAME
#define DNS_ERROR_NUMERIC_NAME           9561L
#endif
#ifndef DNS_ERROR_NOT_ALLOWED_ON_ROOT_SERVER
#define DNS_ERROR_NOT_ALLOWED_ON_ROOT_SERVER 9562L
#endif
#ifndef DNS_ERROR_NOT_ALLOWED_UNDER_DELEGATION
#define DNS_ERROR_NOT_ALLOWED_UNDER_DELEGATION 9563L
#endif
#ifndef DNS_ERROR_CANNOT_FIND_ROOT_HINTS
#define DNS_ERROR_CANNOT_FIND_ROOT_HINTS 9564L
#endif
#ifndef DNS_ERROR_INCONSISTENT_ROOT_HINTS
#define DNS_ERROR_INCONSISTENT_ROOT_HINTS 9565L
#endif
#ifndef DNS_ERROR_ZONE_DOES_NOT_EXIST
#define DNS_ERROR_ZONE_DOES_NOT_EXIST    9601L
#endif
#ifndef DNS_ERROR_NO_ZONE_INFO
#define DNS_ERROR_NO_ZONE_INFO           9602L
#endif
#ifndef DNS_ERROR_INVALID_ZONE_OPERATION
#define DNS_ERROR_INVALID_ZONE_OPERATION 9603L
#endif
#ifndef DNS_ERROR_ZONE_CONFIGURATION_ERROR
#define DNS_ERROR_ZONE_CONFIGURATION_ERROR 9604L
#endif
#ifndef DNS_ERROR_ZONE_HAS_NO_SOA_RECORD
#define DNS_ERROR_ZONE_HAS_NO_SOA_RECORD 9605L
#endif
#ifndef DNS_ERROR_ZONE_HAS_NO_NS_RECORDS
#define DNS_ERROR_ZONE_HAS_NO_NS_RECORDS 9606L
#endif
#ifndef DNS_ERROR_ZONE_LOCKED
#define DNS_ERROR_ZONE_LOCKED            9607L
#endif
#ifndef DNS_ERROR_ZONE_CREATION_FAILED
#define DNS_ERROR_ZONE_CREATION_FAILED   9608L
#endif
#ifndef DNS_ERROR_ZONE_ALREADY_EXISTS
#define DNS_ERROR_ZONE_ALREADY_EXISTS    9609L
#endif
#ifndef DNS_ERROR_AUTOZONE_ALREADY_EXISTS
#define DNS_ERROR_AUTOZONE_ALREADY_EXISTS 9610L
#endif
#ifndef DNS_ERROR_INVALID_ZONE_TYPE
#define DNS_ERROR_INVALID_ZONE_TYPE      9611L
#endif
#ifndef DNS_ERROR_SECONDARY_REQUIRES_MASTER_IP
#define DNS_ERROR_SECONDARY_REQUIRES_MASTER_IP 9612L
#endif
#ifndef DNS_ERROR_ZONE_NOT_SECONDARY
#define DNS_ERROR_ZONE_NOT_SECONDARY     9613L
#endif
#ifndef DNS_ERROR_NEED_SECONDARY_ADDRESSES
#define DNS_ERROR_NEED_SECONDARY_ADDRESSES 9614L
#endif
#ifndef DNS_ERROR_WINS_INIT_FAILED
#define DNS_ERROR_WINS_INIT_FAILED       9615L
#endif
#ifndef DNS_ERROR_NEED_WINS_SERVERS
#define DNS_ERROR_NEED_WINS_SERVERS      9616L
#endif
#ifndef DNS_ERROR_NBSTAT_INIT_FAILED
#define DNS_ERROR_NBSTAT_INIT_FAILED     9617L
#endif
#ifndef DNS_ERROR_SOA_DELETE_INVALID
#define DNS_ERROR_SOA_DELETE_INVALID     9618L
#endif
#ifndef DNS_ERROR_FORWARDER_ALREADY_EXISTS
#define DNS_ERROR_FORWARDER_ALREADY_EXISTS 9619L
#endif
#ifndef DNS_ERROR_ZONE_REQUIRES_MASTER_IP
#define DNS_ERROR_ZONE_REQUIRES_MASTER_IP 9620L
#endif
#ifndef DNS_ERROR_ZONE_IS_SHUTDOWN
#define DNS_ERROR_ZONE_IS_SHUTDOWN       9621L
#endif
#ifndef DNS_ERROR_PRIMARY_REQUIRES_DATAFILE
#define DNS_ERROR_PRIMARY_REQUIRES_DATAFILE 9651L
#endif
#ifndef DNS_ERROR_INVALID_DATAFILE_NAME
#define DNS_ERROR_INVALID_DATAFILE_NAME  9652L
#endif
#ifndef DNS_ERROR_DATAFILE_OPEN_FAILURE
#define DNS_ERROR_DATAFILE_OPEN_FAILURE  9653L
#endif
#ifndef DNS_ERROR_FILE_WRITEBACK_FAILED
#define DNS_ERROR_FILE_WRITEBACK_FAILED  9654L
#endif
#ifndef DNS_ERROR_DATAFILE_PARSING
#define DNS_ERROR_DATAFILE_PARSING       9655L
#endif
#ifndef DNS_ERROR_RECORD_DOES_NOT_EXIST
#define DNS_ERROR_RECORD_DOES_NOT_EXIST  9701L
#endif
#ifndef DNS_ERROR_RECORD_FORMAT
#define DNS_ERROR_RECORD_FORMAT          9702L
#endif
#ifndef DNS_ERROR_NODE_CREATION_FAILED
#define DNS_ERROR_NODE_CREATION_FAILED   9703L
#endif
#ifndef DNS_ERROR_UNKNOWN_RECORD_TYPE
#define DNS_ERROR_UNKNOWN_RECORD_TYPE    9704L
#endif
#ifndef DNS_ERROR_RECORD_TIMED_OUT
#define DNS_ERROR_RECORD_TIMED_OUT       9705L
#endif
#ifndef DNS_ERROR_NAME_NOT_IN_ZONE
#define DNS_ERROR_NAME_NOT_IN_ZONE       9706L
#endif
#ifndef DNS_ERROR_CNAME_LOOP
#define DNS_ERROR_CNAME_LOOP             9707L
#endif
#ifndef DNS_ERROR_NODE_IS_CNAME
#define DNS_ERROR_NODE_IS_CNAME          9708L
#endif
#ifndef DNS_ERROR_CNAME_COLLISION
#define DNS_ERROR_CNAME_COLLISION        9709L
#endif
#ifndef DNS_ERROR_RECORD_ONLY_AT_ZONE_ROOT
#define DNS_ERROR_RECORD_ONLY_AT_ZONE_ROOT 9710L
#endif
#ifndef DNS_ERROR_RECORD_ALREADY_EXISTS
#define DNS_ERROR_RECORD_ALREADY_EXISTS  9711L
#endif
#ifndef DNS_ERROR_SECONDARY_DATA
#define DNS_ERROR_SECONDARY_DATA         9712L
#endif
#ifndef DNS_ERROR_NO_CREATE_CACHE_DATA
#define DNS_ERROR_NO_CREATE_CACHE_DATA   9713L
#endif
#ifndef DNS_ERROR_NAME_DOES_NOT_EXIST
#define DNS_ERROR_NAME_DOES_NOT_EXIST    9714L
#endif
#ifndef DNS_WARNING_PTR_CREATE_FAILED
#define DNS_WARNING_PTR_CREATE_FAILED    9715L
#endif
#ifndef DNS_WARNING_DOMAIN_UNDELETED
#define DNS_WARNING_DOMAIN_UNDELETED     9716L
#endif
#ifndef DNS_ERROR_DS_UNAVAILABLE
#define DNS_ERROR_DS_UNAVAILABLE         9717L
#endif
#ifndef DNS_ERROR_DS_ZONE_ALREADY_EXISTS
#define DNS_ERROR_DS_ZONE_ALREADY_EXISTS 9718L
#endif
#ifndef DNS_ERROR_NO_BOOTFILE_IF_DS_ZONE
#define DNS_ERROR_NO_BOOTFILE_IF_DS_ZONE 9719L
#endif
#ifndef DNS_INFO_AXFR_COMPLETE
#define DNS_INFO_AXFR_COMPLETE           9751L
#endif
#ifndef DNS_ERROR_AXFR
#define DNS_ERROR_AXFR                   9752L
#endif
#ifndef DNS_INFO_ADDED_LOCAL_WINS
#define DNS_INFO_ADDED_LOCAL_WINS        9753L
#endif
#ifndef DNS_STATUS_CONTINUE_NEEDED
#define DNS_STATUS_CONTINUE_NEEDED       9801L
#endif
#ifndef DNS_ERROR_NO_TCPIP
#define DNS_ERROR_NO_TCPIP               9851L
#endif
#ifndef DNS_ERROR_NO_DNS_SERVERS
#define DNS_ERROR_NO_DNS_SERVERS         9852L
#endif
#ifndef DNS_ERROR_DP_DOES_NOT_EXIST
#define DNS_ERROR_DP_DOES_NOT_EXIST      9901L
#endif
#ifndef DNS_ERROR_DP_ALREADY_EXISTS
#define DNS_ERROR_DP_ALREADY_EXISTS      9902L
#endif
#ifndef DNS_ERROR_DP_NOT_ENLISTED
#define DNS_ERROR_DP_NOT_ENLISTED        9903L
#endif
#ifndef DNS_ERROR_DP_ALREADY_ENLISTED
#define DNS_ERROR_DP_ALREADY_ENLISTED    9904L
#endif
#ifndef DNS_ERROR_DP_NOT_AVAILABLE
#define DNS_ERROR_DP_NOT_AVAILABLE       9905L
#endif
#ifndef DNS_ERROR_DP_FSMO_ERROR
#define DNS_ERROR_DP_FSMO_ERROR          9906L
#endif
#ifndef WSA_QOS_RECEIVERS
#define WSA_QOS_RECEIVERS                11005L
#endif
#ifndef WSA_QOS_SENDERS
#define WSA_QOS_SENDERS                  11006L
#endif
#ifndef WSA_QOS_NO_SENDERS
#define WSA_QOS_NO_SENDERS               11007L
#endif
#ifndef WSA_QOS_NO_RECEIVERS
#define WSA_QOS_NO_RECEIVERS             11008L
#endif
#ifndef WSA_QOS_REQUEST_CONFIRMED
#define WSA_QOS_REQUEST_CONFIRMED        11009L
#endif
#ifndef WSA_QOS_ADMISSION_FAILURE
#define WSA_QOS_ADMISSION_FAILURE        11010L
#endif
#ifndef WSA_QOS_POLICY_FAILURE
#define WSA_QOS_POLICY_FAILURE           11011L
#endif
#ifndef WSA_QOS_BAD_STYLE
#define WSA_QOS_BAD_STYLE                11012L
#endif
#ifndef WSA_QOS_BAD_OBJECT
#define WSA_QOS_BAD_OBJECT               11013L
#endif
#ifndef WSA_QOS_TRAFFIC_CTRL_ERROR
#define WSA_QOS_TRAFFIC_CTRL_ERROR       11014L
#endif
#ifndef WSA_QOS_GENERIC_ERROR
#define WSA_QOS_GENERIC_ERROR            11015L
#endif
#ifndef WSA_QOS_ESERVICETYPE
#define WSA_QOS_ESERVICETYPE             11016L
#endif
#ifndef WSA_QOS_EFLOWSPEC
#define WSA_QOS_EFLOWSPEC                11017L
#endif
#ifndef WSA_QOS_EPROVSPECBUF
#define WSA_QOS_EPROVSPECBUF             11018L
#endif
#ifndef WSA_QOS_EFILTERSTYLE
#define WSA_QOS_EFILTERSTYLE             11019L
#endif
#ifndef WSA_QOS_EFILTERTYPE
#define WSA_QOS_EFILTERTYPE              11020L
#endif
#ifndef WSA_QOS_EFILTERCOUNT
#define WSA_QOS_EFILTERCOUNT             11021L
#endif
#ifndef WSA_QOS_EOBJLENGTH
#define WSA_QOS_EOBJLENGTH               11022L
#endif
#ifndef WSA_QOS_EFLOWCOUNT
#define WSA_QOS_EFLOWCOUNT               11023L
#endif
#ifndef WSA_QOS_EUNKOWNPSOBJ
#define WSA_QOS_EUNKOWNPSOBJ             11024L
#endif
#ifndef WSA_QOS_EPOLICYOBJ
#define WSA_QOS_EPOLICYOBJ               11025L
#endif
#ifndef WSA_QOS_EFLOWDESC
#define WSA_QOS_EFLOWDESC                11026L
#endif
#ifndef WSA_QOS_EPSFLOWSPEC
#define WSA_QOS_EPSFLOWSPEC              11027L
#endif
#ifndef WSA_QOS_EPSFILTERSPEC
#define WSA_QOS_EPSFILTERSPEC            11028L
#endif
#ifndef WSA_QOS_ESDMODEOBJ
#define WSA_QOS_ESDMODEOBJ               11029L
#endif
#ifndef WSA_QOS_ESHAPERATEOBJ
#define WSA_QOS_ESHAPERATEOBJ            11030L
#endif
#ifndef WSA_QOS_RESERVED_PETYPE
#define WSA_QOS_RESERVED_PETYPE          11031L
#endif
#ifndef ERROR_SXS_SECTION_NOT_FOUND
#define ERROR_SXS_SECTION_NOT_FOUND      14000L
#endif
#ifndef ERROR_SXS_CANT_GEN_ACTCTX
#define ERROR_SXS_CANT_GEN_ACTCTX        14001L
#endif
#ifndef ERROR_SXS_INVALID_ACTCTXDATA_FORMAT
#define ERROR_SXS_INVALID_ACTCTXDATA_FORMAT 14002L
#endif
#ifndef ERROR_SXS_ASSEMBLY_NOT_FOUND
#define ERROR_SXS_ASSEMBLY_NOT_FOUND     14003L
#endif
#ifndef ERROR_SXS_MANIFEST_FORMAT_ERROR
#define ERROR_SXS_MANIFEST_FORMAT_ERROR  14004L
#endif
#ifndef ERROR_SXS_MANIFEST_PARSE_ERROR
#define ERROR_SXS_MANIFEST_PARSE_ERROR   14005L
#endif
#ifndef ERROR_SXS_ACTIVATION_CONTEXT_DISABLED
#define ERROR_SXS_ACTIVATION_CONTEXT_DISABLED 14006L
#endif
#ifndef ERROR_SXS_KEY_NOT_FOUND
#define ERROR_SXS_KEY_NOT_FOUND          14007L
#endif
#ifndef ERROR_SXS_VERSION_CONFLICT
#define ERROR_SXS_VERSION_CONFLICT       14008L
#endif
#ifndef ERROR_SXS_WRONG_SECTION_TYPE
#define ERROR_SXS_WRONG_SECTION_TYPE     14009L
#endif
#ifndef ERROR_SXS_THREAD_QUERIES_DISABLED
#define ERROR_SXS_THREAD_QUERIES_DISABLED 14010L
#endif
#ifndef ERROR_SXS_PROCESS_DEFAULT_ALREADY_SET
#define ERROR_SXS_PROCESS_DEFAULT_ALREADY_SET 14011L
#endif
#ifndef ERROR_SXS_UNKNOWN_ENCODING_GROUP
#define ERROR_SXS_UNKNOWN_ENCODING_GROUP 14012L
#endif
#ifndef ERROR_SXS_UNKNOWN_ENCODING
#define ERROR_SXS_UNKNOWN_ENCODING       14013L
#endif
#ifndef ERROR_SXS_INVALID_XML_NAMESPACE_URI
#define ERROR_SXS_INVALID_XML_NAMESPACE_URI 14014L
#endif
#ifndef ERROR_SXS_ROOT_MANIFEST_DEPENDENCY_NOT_INSTALLED
#define ERROR_SXS_ROOT_MANIFEST_DEPENDENCY_NOT_INSTALLED 14015L
#endif
#ifndef ERROR_SXS_LEAF_MANIFEST_DEPENDENCY_NOT_INSTALLED
#define ERROR_SXS_LEAF_MANIFEST_DEPENDENCY_NOT_INSTALLED 14016L
#endif
#ifndef ERROR_SXS_INVALID_ASSEMBLY_IDENTITY_ATTRIBUTE
#define ERROR_SXS_INVALID_ASSEMBLY_IDENTITY_ATTRIBUTE 14017L
#endif
#ifndef ERROR_SXS_MANIFEST_MISSING_REQUIRED_DEFAULT_NAMESPACE
#define ERROR_SXS_MANIFEST_MISSING_REQUIRED_DEFAULT_NAMESPACE 14018L
#endif
#ifndef ERROR_SXS_MANIFEST_INVALID_REQUIRED_DEFAULT_NAMESPACE
#define ERROR_SXS_MANIFEST_INVALID_REQUIRED_DEFAULT_NAMESPACE 14019L
#endif
#ifndef ERROR_SXS_PRIVATE_MANIFEST_CROSS_PATH_WITH_REPARSE_POINT
#define ERROR_SXS_PRIVATE_MANIFEST_CROSS_PATH_WITH_REPARSE_POINT 14020L
#endif
#ifndef ERROR_SXS_DUPLICATE_DLL_NAME
#define ERROR_SXS_DUPLICATE_DLL_NAME     14021L
#endif
#ifndef ERROR_SXS_DUPLICATE_WINDOWCLASS_NAME
#define ERROR_SXS_DUPLICATE_WINDOWCLASS_NAME 14022L
#endif
#ifndef ERROR_SXS_DUPLICATE_CLSID
#define ERROR_SXS_DUPLICATE_CLSID        14023L
#endif
#ifndef ERROR_SXS_DUPLICATE_IID
#define ERROR_SXS_DUPLICATE_IID          14024L
#endif
#ifndef ERROR_SXS_DUPLICATE_TLBID
#define ERROR_SXS_DUPLICATE_TLBID        14025L
#endif
#ifndef ERROR_SXS_DUPLICATE_PROGID
#define ERROR_SXS_DUPLICATE_PROGID       14026L
#endif
#ifndef ERROR_SXS_DUPLICATE_ASSEMBLY_NAME
#define ERROR_SXS_DUPLICATE_ASSEMBLY_NAME 14027L
#endif
#ifndef ERROR_SXS_FILE_HASH_MISMATCH
#define ERROR_SXS_FILE_HASH_MISMATCH     14028L
#endif
#ifndef ERROR_SXS_POLICY_PARSE_ERROR
#define ERROR_SXS_POLICY_PARSE_ERROR     14029L
#endif
#ifndef ERROR_SXS_XML_E_MISSINGQUOTE
#define ERROR_SXS_XML_E_MISSINGQUOTE     14030L
#endif
#ifndef ERROR_SXS_XML_E_COMMENTSYNTAX
#define ERROR_SXS_XML_E_COMMENTSYNTAX    14031L
#endif
#ifndef ERROR_SXS_XML_E_BADSTARTNAMECHAR
#define ERROR_SXS_XML_E_BADSTARTNAMECHAR 14032L
#endif
#ifndef ERROR_SXS_XML_E_BADNAMECHAR
#define ERROR_SXS_XML_E_BADNAMECHAR      14033L
#endif
#ifndef ERROR_SXS_XML_E_BADCHARINSTRING
#define ERROR_SXS_XML_E_BADCHARINSTRING  14034L
#endif
#ifndef ERROR_SXS_XML_E_XMLDECLSYNTAX
#define ERROR_SXS_XML_E_XMLDECLSYNTAX    14035L
#endif
#ifndef ERROR_SXS_XML_E_BADCHARDATA
#define ERROR_SXS_XML_E_BADCHARDATA      14036L
#endif
#ifndef ERROR_SXS_XML_E_MISSINGWHITESPACE
#define ERROR_SXS_XML_E_MISSINGWHITESPACE 14037L
#endif
#ifndef ERROR_SXS_XML_E_EXPECTINGTAGEND
#define ERROR_SXS_XML_E_EXPECTINGTAGEND  14038L
#endif
#ifndef ERROR_SXS_XML_E_MISSINGSEMICOLON
#define ERROR_SXS_XML_E_MISSINGSEMICOLON 14039L
#endif
#ifndef ERROR_SXS_XML_E_UNBALANCEDPAREN
#define ERROR_SXS_XML_E_UNBALANCEDPAREN  14040L
#endif
#ifndef ERROR_SXS_XML_E_INTERNALERROR
#define ERROR_SXS_XML_E_INTERNALERROR    14041L
#endif
#ifndef ERROR_SXS_XML_E_UNEXPECTED_WHITESPACE
#define ERROR_SXS_XML_E_UNEXPECTED_WHITESPACE 14042L
#endif
#ifndef ERROR_SXS_XML_E_INCOMPLETE_ENCODING
#define ERROR_SXS_XML_E_INCOMPLETE_ENCODING 14043L
#endif
#ifndef ERROR_SXS_XML_E_MISSING_PAREN
#define ERROR_SXS_XML_E_MISSING_PAREN    14044L
#endif
#ifndef ERROR_SXS_XML_E_EXPECTINGCLOSEQUOTE
#define ERROR_SXS_XML_E_EXPECTINGCLOSEQUOTE 14045L
#endif
#ifndef ERROR_SXS_XML_E_MULTIPLE_COLONS
#define ERROR_SXS_XML_E_MULTIPLE_COLONS  14046L
#endif
#ifndef ERROR_SXS_XML_E_INVALID_DECIMAL
#define ERROR_SXS_XML_E_INVALID_DECIMAL  14047L
#endif
#ifndef ERROR_SXS_XML_E_INVALID_HEXIDECIMAL
#define ERROR_SXS_XML_E_INVALID_HEXIDECIMAL 14048L
#endif
#ifndef ERROR_SXS_XML_E_INVALID_UNICODE
#define ERROR_SXS_XML_E_INVALID_UNICODE  14049L
#endif
#ifndef ERROR_SXS_XML_E_WHITESPACEORQUESTIONMARK
#define ERROR_SXS_XML_E_WHITESPACEORQUESTIONMARK 14050L
#endif
#ifndef ERROR_SXS_XML_E_UNEXPECTEDENDTAG
#define ERROR_SXS_XML_E_UNEXPECTEDENDTAG 14051L
#endif
#ifndef ERROR_SXS_XML_E_UNCLOSEDTAG
#define ERROR_SXS_XML_E_UNCLOSEDTAG      14052L
#endif
#ifndef ERROR_SXS_XML_E_DUPLICATEATTRIBUTE
#define ERROR_SXS_XML_E_DUPLICATEATTRIBUTE 14053L
#endif
#ifndef ERROR_SXS_XML_E_MULTIPLEROOTS
#define ERROR_SXS_XML_E_MULTIPLEROOTS    14054L
#endif
#ifndef ERROR_SXS_XML_E_INVALIDATROOTLEVEL
#define ERROR_SXS_XML_E_INVALIDATROOTLEVEL 14055L
#endif
#ifndef ERROR_SXS_XML_E_BADXMLDECL
#define ERROR_SXS_XML_E_BADXMLDECL       14056L
#endif
#ifndef ERROR_SXS_XML_E_MISSINGROOT
#define ERROR_SXS_XML_E_MISSINGROOT      14057L
#endif
#ifndef ERROR_SXS_XML_E_UNEXPECTEDEOF
#define ERROR_SXS_XML_E_UNEXPECTEDEOF    14058L
#endif
#ifndef ERROR_SXS_XML_E_BADPEREFINSUBSET
#define ERROR_SXS_XML_E_BADPEREFINSUBSET 14059L
#endif
#ifndef ERROR_SXS_XML_E_UNCLOSEDSTARTTAG
#define ERROR_SXS_XML_E_UNCLOSEDSTARTTAG 14060L
#endif
#ifndef ERROR_SXS_XML_E_UNCLOSEDENDTAG
#define ERROR_SXS_XML_E_UNCLOSEDENDTAG   14061L
#endif
#ifndef ERROR_SXS_XML_E_UNCLOSEDSTRING
#define ERROR_SXS_XML_E_UNCLOSEDSTRING   14062L
#endif
#ifndef ERROR_SXS_XML_E_UNCLOSEDCOMMENT
#define ERROR_SXS_XML_E_UNCLOSEDCOMMENT  14063L
#endif
#ifndef ERROR_SXS_XML_E_UNCLOSEDDECL
#define ERROR_SXS_XML_E_UNCLOSEDDECL     14064L
#endif
#ifndef ERROR_SXS_XML_E_UNCLOSEDCDATA
#define ERROR_SXS_XML_E_UNCLOSEDCDATA    14065L
#endif
#ifndef ERROR_SXS_XML_E_RESERVEDNAMESPACE
#define ERROR_SXS_XML_E_RESERVEDNAMESPACE 14066L
#endif
#ifndef ERROR_SXS_XML_E_INVALIDENCODING
#define ERROR_SXS_XML_E_INVALIDENCODING  14067L
#endif
#ifndef ERROR_SXS_XML_E_INVALIDSWITCH
#define ERROR_SXS_XML_E_INVALIDSWITCH    14068L
#endif
#ifndef ERROR_SXS_XML_E_BADXMLCASE
#define ERROR_SXS_XML_E_BADXMLCASE       14069L
#endif
#ifndef ERROR_SXS_XML_E_INVALID_STANDALONE
#define ERROR_SXS_XML_E_INVALID_STANDALONE 14070L
#endif
#ifndef ERROR_SXS_XML_E_UNEXPECTED_STANDALONE
#define ERROR_SXS_XML_E_UNEXPECTED_STANDALONE 14071L
#endif
#ifndef ERROR_SXS_XML_E_INVALID_VERSION
#define ERROR_SXS_XML_E_INVALID_VERSION  14072L
#endif
#ifndef ERROR_SXS_XML_E_MISSINGEQUALS
#define ERROR_SXS_XML_E_MISSINGEQUALS    14073L
#endif
#ifndef ERROR_SXS_PROTECTION_RECOVERY_FAILED
#define ERROR_SXS_PROTECTION_RECOVERY_FAILED 14074L
#endif
#ifndef ERROR_SXS_PROTECTION_PUBLIC_KEY_TOO_SHORT
#define ERROR_SXS_PROTECTION_PUBLIC_KEY_TOO_SHORT 14075L
#endif
#ifndef ERROR_SXS_PROTECTION_CATALOG_NOT_VALID
#define ERROR_SXS_PROTECTION_CATALOG_NOT_VALID 14076L
#endif
#ifndef ERROR_SXS_UNTRANSLATABLE_HRESULT
#define ERROR_SXS_UNTRANSLATABLE_HRESULT 14077L
#endif
#ifndef ERROR_SXS_PROTECTION_CATALOG_FILE_MISSING
#define ERROR_SXS_PROTECTION_CATALOG_FILE_MISSING 14078L
#endif
#ifndef ERROR_SXS_MISSING_ASSEMBLY_IDENTITY_ATTRIBUTE
#define ERROR_SXS_MISSING_ASSEMBLY_IDENTITY_ATTRIBUTE 14079L
#endif
#ifndef ERROR_SXS_INVALID_ASSEMBLY_IDENTITY_ATTRIBUTE_NAME
#define ERROR_SXS_INVALID_ASSEMBLY_IDENTITY_ATTRIBUTE_NAME 14080L
#endif
#ifndef ERROR_IPSEC_QM_POLICY_EXISTS
#define ERROR_IPSEC_QM_POLICY_EXISTS     13000L
#endif
#ifndef ERROR_IPSEC_QM_POLICY_NOT_FOUND
#define ERROR_IPSEC_QM_POLICY_NOT_FOUND  13001L
#endif
#ifndef ERROR_IPSEC_QM_POLICY_IN_USE
#define ERROR_IPSEC_QM_POLICY_IN_USE     13002L
#endif
#ifndef ERROR_IPSEC_MM_POLICY_EXISTS
#define ERROR_IPSEC_MM_POLICY_EXISTS     13003L
#endif
#ifndef ERROR_IPSEC_MM_POLICY_NOT_FOUND
#define ERROR_IPSEC_MM_POLICY_NOT_FOUND  13004L
#endif
#ifndef ERROR_IPSEC_MM_POLICY_IN_USE
#define ERROR_IPSEC_MM_POLICY_IN_USE     13005L
#endif
#ifndef ERROR_IPSEC_MM_FILTER_EXISTS
#define ERROR_IPSEC_MM_FILTER_EXISTS     13006L
#endif
#ifndef ERROR_IPSEC_MM_FILTER_NOT_FOUND
#define ERROR_IPSEC_MM_FILTER_NOT_FOUND  13007L
#endif
#ifndef ERROR_IPSEC_TRANSPORT_FILTER_EXISTS
#define ERROR_IPSEC_TRANSPORT_FILTER_EXISTS 13008L
#endif
#ifndef ERROR_IPSEC_TRANSPORT_FILTER_NOT_FOUND
#define ERROR_IPSEC_TRANSPORT_FILTER_NOT_FOUND 13009L
#endif
#ifndef ERROR_IPSEC_MM_AUTH_EXISTS
#define ERROR_IPSEC_MM_AUTH_EXISTS       13010L
#endif
#ifndef ERROR_IPSEC_MM_AUTH_NOT_FOUND
#define ERROR_IPSEC_MM_AUTH_NOT_FOUND    13011L
#endif
#ifndef ERROR_IPSEC_MM_AUTH_IN_USE
#define ERROR_IPSEC_MM_AUTH_IN_USE       13012L
#endif
#ifndef ERROR_IPSEC_DEFAULT_MM_POLICY_NOT_FOUND
#define ERROR_IPSEC_DEFAULT_MM_POLICY_NOT_FOUND 13013L
#endif
#ifndef ERROR_IPSEC_DEFAULT_MM_AUTH_NOT_FOUND
#define ERROR_IPSEC_DEFAULT_MM_AUTH_NOT_FOUND 13014L
#endif
#ifndef ERROR_IPSEC_DEFAULT_QM_POLICY_NOT_FOUND
#define ERROR_IPSEC_DEFAULT_QM_POLICY_NOT_FOUND 13015L
#endif
#ifndef ERROR_IPSEC_TUNNEL_FILTER_EXISTS
#define ERROR_IPSEC_TUNNEL_FILTER_EXISTS 13016L
#endif
#ifndef ERROR_IPSEC_TUNNEL_FILTER_NOT_FOUND
#define ERROR_IPSEC_TUNNEL_FILTER_NOT_FOUND 13017L
#endif
#ifndef ERROR_IPSEC_MM_FILTER_PENDING_DELETION
#define ERROR_IPSEC_MM_FILTER_PENDING_DELETION 13018L
#endif
#ifndef ERROR_IPSEC_TRANSPORT_FILTER_PENDING_DELETION
#define ERROR_IPSEC_TRANSPORT_FILTER_PENDING_DELETION 13019L
#endif
#ifndef ERROR_IPSEC_TUNNEL_FILTER_PENDING_DELETION
#define ERROR_IPSEC_TUNNEL_FILTER_PENDING_DELETION 13020L
#endif
#ifndef ERROR_IPSEC_MM_POLICY_PENDING_DELETION
#define ERROR_IPSEC_MM_POLICY_PENDING_DELETION 13021L
#endif
#ifndef ERROR_IPSEC_MM_AUTH_PENDING_DELETION
#define ERROR_IPSEC_MM_AUTH_PENDING_DELETION 13022L
#endif
#ifndef ERROR_IPSEC_QM_POLICY_PENDING_DELETION
#define ERROR_IPSEC_QM_POLICY_PENDING_DELETION 13023L
#endif
#ifndef WARNING_IPSEC_MM_POLICY_PRUNED
#define WARNING_IPSEC_MM_POLICY_PRUNED   13024L
#endif
#ifndef WARNING_IPSEC_QM_POLICY_PRUNED
#define WARNING_IPSEC_QM_POLICY_PRUNED   13025L
#endif
#ifndef ERROR_IPSEC_IKE_NEG_STATUS_BEGIN
#define ERROR_IPSEC_IKE_NEG_STATUS_BEGIN 13800L
#endif
#ifndef ERROR_IPSEC_IKE_AUTH_FAIL
#define ERROR_IPSEC_IKE_AUTH_FAIL        13801L
#endif
#ifndef ERROR_IPSEC_IKE_ATTRIB_FAIL
#define ERROR_IPSEC_IKE_ATTRIB_FAIL      13802L
#endif
#ifndef ERROR_IPSEC_IKE_NEGOTIATION_PENDING
#define ERROR_IPSEC_IKE_NEGOTIATION_PENDING 13803L
#endif
#ifndef ERROR_IPSEC_IKE_GENERAL_PROCESSING_ERROR
#define ERROR_IPSEC_IKE_GENERAL_PROCESSING_ERROR 13804L
#endif
#ifndef ERROR_IPSEC_IKE_TIMED_OUT
#define ERROR_IPSEC_IKE_TIMED_OUT        13805L
#endif
#ifndef ERROR_IPSEC_IKE_NO_CERT
#define ERROR_IPSEC_IKE_NO_CERT          13806L
#endif
#ifndef ERROR_IPSEC_IKE_SA_DELETED
#define ERROR_IPSEC_IKE_SA_DELETED       13807L
#endif
#ifndef ERROR_IPSEC_IKE_SA_REAPED
#define ERROR_IPSEC_IKE_SA_REAPED        13808L
#endif
#ifndef ERROR_IPSEC_IKE_MM_ACQUIRE_DROP
#define ERROR_IPSEC_IKE_MM_ACQUIRE_DROP  13809L
#endif
#ifndef ERROR_IPSEC_IKE_QM_ACQUIRE_DROP
#define ERROR_IPSEC_IKE_QM_ACQUIRE_DROP  13810L
#endif
#ifndef ERROR_IPSEC_IKE_QUEUE_DROP_MM
#define ERROR_IPSEC_IKE_QUEUE_DROP_MM    13811L
#endif
#ifndef ERROR_IPSEC_IKE_QUEUE_DROP_NO_MM
#define ERROR_IPSEC_IKE_QUEUE_DROP_NO_MM 13812L
#endif
#ifndef ERROR_IPSEC_IKE_DROP_NO_RESPONSE
#define ERROR_IPSEC_IKE_DROP_NO_RESPONSE 13813L
#endif
#ifndef ERROR_IPSEC_IKE_MM_DELAY_DROP
#define ERROR_IPSEC_IKE_MM_DELAY_DROP    13814L
#endif
#ifndef ERROR_IPSEC_IKE_QM_DELAY_DROP
#define ERROR_IPSEC_IKE_QM_DELAY_DROP    13815L
#endif
#ifndef ERROR_IPSEC_IKE_ERROR
#define ERROR_IPSEC_IKE_ERROR            13816L
#endif
#ifndef ERROR_IPSEC_IKE_CRL_FAILED
#define ERROR_IPSEC_IKE_CRL_FAILED       13817L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_KEY_USAGE
#define ERROR_IPSEC_IKE_INVALID_KEY_USAGE 13818L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_CERT_TYPE
#define ERROR_IPSEC_IKE_INVALID_CERT_TYPE 13819L
#endif
#ifndef ERROR_IPSEC_IKE_NO_PRIVATE_KEY
#define ERROR_IPSEC_IKE_NO_PRIVATE_KEY   13820L
#endif
#ifndef ERROR_IPSEC_IKE_DH_FAIL
#define ERROR_IPSEC_IKE_DH_FAIL          13822L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_HEADER
#define ERROR_IPSEC_IKE_INVALID_HEADER   13824L
#endif
#ifndef ERROR_IPSEC_IKE_NO_POLICY
#define ERROR_IPSEC_IKE_NO_POLICY        13825L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_SIGNATURE
#define ERROR_IPSEC_IKE_INVALID_SIGNATURE 13826L
#endif
#ifndef ERROR_IPSEC_IKE_KERBEROS_ERROR
#define ERROR_IPSEC_IKE_KERBEROS_ERROR   13827L
#endif
#ifndef ERROR_IPSEC_IKE_NO_PUBLIC_KEY
#define ERROR_IPSEC_IKE_NO_PUBLIC_KEY    13828L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR
#define ERROR_IPSEC_IKE_PROCESS_ERR      13829L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_SA
#define ERROR_IPSEC_IKE_PROCESS_ERR_SA   13830L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_PROP
#define ERROR_IPSEC_IKE_PROCESS_ERR_PROP 13831L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_TRANS
#define ERROR_IPSEC_IKE_PROCESS_ERR_TRANS 13832L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_KE
#define ERROR_IPSEC_IKE_PROCESS_ERR_KE   13833L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_ID
#define ERROR_IPSEC_IKE_PROCESS_ERR_ID   13834L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_CERT
#define ERROR_IPSEC_IKE_PROCESS_ERR_CERT 13835L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_CERT_REQ
#define ERROR_IPSEC_IKE_PROCESS_ERR_CERT_REQ 13836L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_HASH
#define ERROR_IPSEC_IKE_PROCESS_ERR_HASH 13837L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_SIG
#define ERROR_IPSEC_IKE_PROCESS_ERR_SIG  13838L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_NONCE
#define ERROR_IPSEC_IKE_PROCESS_ERR_NONCE 13839L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_NOTIFY
#define ERROR_IPSEC_IKE_PROCESS_ERR_NOTIFY 13840L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_DELETE
#define ERROR_IPSEC_IKE_PROCESS_ERR_DELETE 13841L
#endif
#ifndef ERROR_IPSEC_IKE_PROCESS_ERR_VENDOR
#define ERROR_IPSEC_IKE_PROCESS_ERR_VENDOR 13842L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_PAYLOAD
#define ERROR_IPSEC_IKE_INVALID_PAYLOAD  13843L
#endif
#ifndef ERROR_IPSEC_IKE_LOAD_SOFT_SA
#define ERROR_IPSEC_IKE_LOAD_SOFT_SA     13844L
#endif
#ifndef ERROR_IPSEC_IKE_SOFT_SA_TORN_DOWN
#define ERROR_IPSEC_IKE_SOFT_SA_TORN_DOWN 13845L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_COOKIE
#define ERROR_IPSEC_IKE_INVALID_COOKIE   13846L
#endif
#ifndef ERROR_IPSEC_IKE_NO_PEER_CERT
#define ERROR_IPSEC_IKE_NO_PEER_CERT     13847L
#endif
#ifndef ERROR_IPSEC_IKE_PEER_CRL_FAILED
#define ERROR_IPSEC_IKE_PEER_CRL_FAILED  13848L
#endif
#ifndef ERROR_IPSEC_IKE_POLICY_CHANGE
#define ERROR_IPSEC_IKE_POLICY_CHANGE    13849L
#endif
#ifndef ERROR_IPSEC_IKE_NO_MM_POLICY
#define ERROR_IPSEC_IKE_NO_MM_POLICY     13850L
#endif
#ifndef ERROR_IPSEC_IKE_NOTCBPRIV
#define ERROR_IPSEC_IKE_NOTCBPRIV        13851L
#endif
#ifndef ERROR_IPSEC_IKE_SECLOADFAIL
#define ERROR_IPSEC_IKE_SECLOADFAIL      13852L
#endif
#ifndef ERROR_IPSEC_IKE_FAILSSPINIT
#define ERROR_IPSEC_IKE_FAILSSPINIT      13853L
#endif
#ifndef ERROR_IPSEC_IKE_FAILQUERYSSP
#define ERROR_IPSEC_IKE_FAILQUERYSSP     13854L
#endif
#ifndef ERROR_IPSEC_IKE_SRVACQFAIL
#define ERROR_IPSEC_IKE_SRVACQFAIL       13855L
#endif
#ifndef ERROR_IPSEC_IKE_SRVQUERYCRED
#define ERROR_IPSEC_IKE_SRVQUERYCRED     13856L
#endif
#ifndef ERROR_IPSEC_IKE_GETSPIFAIL
#define ERROR_IPSEC_IKE_GETSPIFAIL       13857L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_FILTER
#define ERROR_IPSEC_IKE_INVALID_FILTER   13858L
#endif
#ifndef ERROR_IPSEC_IKE_OUT_OF_MEMORY
#define ERROR_IPSEC_IKE_OUT_OF_MEMORY    13859L
#endif
#ifndef ERROR_IPSEC_IKE_ADD_UPDATE_KEY_FAILED
#define ERROR_IPSEC_IKE_ADD_UPDATE_KEY_FAILED 13860L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_POLICY
#define ERROR_IPSEC_IKE_INVALID_POLICY   13861L
#endif
#ifndef ERROR_IPSEC_IKE_UNKNOWN_DOI
#define ERROR_IPSEC_IKE_UNKNOWN_DOI      13862L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_SITUATION
#define ERROR_IPSEC_IKE_INVALID_SITUATION 13863L
#endif
#ifndef ERROR_IPSEC_IKE_DH_FAILURE
#define ERROR_IPSEC_IKE_DH_FAILURE       13864L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_GROUP
#define ERROR_IPSEC_IKE_INVALID_GROUP    13865L
#endif
#ifndef ERROR_IPSEC_IKE_ENCRYPT
#define ERROR_IPSEC_IKE_ENCRYPT          13866L
#endif
#ifndef ERROR_IPSEC_IKE_DECRYPT
#define ERROR_IPSEC_IKE_DECRYPT          13867L
#endif
#ifndef ERROR_IPSEC_IKE_POLICY_MATCH
#define ERROR_IPSEC_IKE_POLICY_MATCH     13868L
#endif
#ifndef ERROR_IPSEC_IKE_UNSUPPORTED_ID
#define ERROR_IPSEC_IKE_UNSUPPORTED_ID   13869L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_HASH
#define ERROR_IPSEC_IKE_INVALID_HASH     13870L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_HASH_ALG
#define ERROR_IPSEC_IKE_INVALID_HASH_ALG 13871L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_HASH_SIZE
#define ERROR_IPSEC_IKE_INVALID_HASH_SIZE 13872L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_ENCRYPT_ALG
#define ERROR_IPSEC_IKE_INVALID_ENCRYPT_ALG 13873L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_AUTH_ALG
#define ERROR_IPSEC_IKE_INVALID_AUTH_ALG 13874L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_SIG
#define ERROR_IPSEC_IKE_INVALID_SIG      13875L
#endif
#ifndef ERROR_IPSEC_IKE_LOAD_FAILED
#define ERROR_IPSEC_IKE_LOAD_FAILED      13876L
#endif
#ifndef ERROR_IPSEC_IKE_RPC_DELETE
#define ERROR_IPSEC_IKE_RPC_DELETE       13877L
#endif
#ifndef ERROR_IPSEC_IKE_BENIGN_REINIT
#define ERROR_IPSEC_IKE_BENIGN_REINIT    13878L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_RESPONDER_LIFETIME_NOTIFY
#define ERROR_IPSEC_IKE_INVALID_RESPONDER_LIFETIME_NOTIFY 13879L
#endif
#ifndef ERROR_IPSEC_IKE_INVALID_CERT_KEYLEN
#define ERROR_IPSEC_IKE_INVALID_CERT_KEYLEN 13881L
#endif
#ifndef ERROR_IPSEC_IKE_MM_LIMIT
#define ERROR_IPSEC_IKE_MM_LIMIT         13882L
#endif
#ifndef ERROR_IPSEC_IKE_NEGOTIATION_DISABLED
#define ERROR_IPSEC_IKE_NEGOTIATION_DISABLED 13883L
#endif
#ifndef ERROR_IPSEC_IKE_NEG_STATUS_END
#define ERROR_IPSEC_IKE_NEG_STATUS_END   13884L
#endif

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WinErrId --
 *
 *	Same as Tcl_ErrnoId(), but for windows error codes.
 *
 * Results:
 *      The symbolic name of the error code.
 *    
 * Comments:
 *	Current complete list from <winerror.h> as of the Platform
 *	SDK October 2002 release.  If I'm going to export this function,
 *	I may as well make it complete :)
 *
 * Side Effects:
 *	Causes large static string storage in our DLL.
 *
 *----------------------------------------------------------------------
 */

const char *
Tcl_WinErrId (unsigned int errorCode)
{
    /* Use special macro magic to simplify this mess */
#   define CASE(i) case i: return #i;

    switch (errorCode) {
	CASE(ERROR_SUCCESS)
	CASE(ERROR_INVALID_FUNCTION)
	CASE(ERROR_FILE_NOT_FOUND)
	CASE(ERROR_PATH_NOT_FOUND)
	CASE(ERROR_TOO_MANY_OPEN_FILES)
	CASE(ERROR_ACCESS_DENIED)
	CASE(ERROR_INVALID_HANDLE)
	CASE(ERROR_ARENA_TRASHED)
	CASE(ERROR_NOT_ENOUGH_MEMORY)
	CASE(ERROR_INVALID_BLOCK)
	CASE(ERROR_BAD_ENVIRONMENT)
	CASE(ERROR_BAD_FORMAT)
	CASE(ERROR_INVALID_ACCESS)
	CASE(ERROR_INVALID_DATA)
	CASE(ERROR_OUTOFMEMORY)
	CASE(ERROR_INVALID_DRIVE)
	CASE(ERROR_CURRENT_DIRECTORY)
	CASE(ERROR_NOT_SAME_DEVICE)
	CASE(ERROR_NO_MORE_FILES)
	CASE(ERROR_WRITE_PROTECT)
	CASE(ERROR_BAD_UNIT)
	CASE(ERROR_NOT_READY)
	CASE(ERROR_BAD_COMMAND)
	CASE(ERROR_CRC)
	CASE(ERROR_BAD_LENGTH)
	CASE(ERROR_SEEK)
	CASE(ERROR_NOT_DOS_DISK)
	CASE(ERROR_SECTOR_NOT_FOUND)
	CASE(ERROR_OUT_OF_PAPER)
	CASE(ERROR_WRITE_FAULT)
	CASE(ERROR_READ_FAULT)
	CASE(ERROR_GEN_FAILURE)
	CASE(ERROR_SHARING_VIOLATION)
	CASE(ERROR_LOCK_VIOLATION)
	CASE(ERROR_WRONG_DISK)
	CASE(ERROR_SHARING_BUFFER_EXCEEDED)
	CASE(ERROR_HANDLE_EOF)
	CASE(ERROR_HANDLE_DISK_FULL)
	CASE(ERROR_NOT_SUPPORTED)
	CASE(ERROR_REM_NOT_LIST)
	CASE(ERROR_DUP_NAME)
	CASE(ERROR_BAD_NETPATH)
	CASE(ERROR_NETWORK_BUSY)
	CASE(ERROR_DEV_NOT_EXIST)
	CASE(ERROR_TOO_MANY_CMDS)
	CASE(ERROR_ADAP_HDW_ERR)
	CASE(ERROR_BAD_NET_RESP)
	CASE(ERROR_UNEXP_NET_ERR)
	CASE(ERROR_BAD_REM_ADAP)
	CASE(ERROR_PRINTQ_FULL)
	CASE(ERROR_NO_SPOOL_SPACE)
	CASE(ERROR_PRINT_CANCELLED)
	CASE(ERROR_NETNAME_DELETED)
	CASE(ERROR_NETWORK_ACCESS_DENIED)
	CASE(ERROR_BAD_DEV_TYPE)
	CASE(ERROR_BAD_NET_NAME)
	CASE(ERROR_TOO_MANY_NAMES)
	CASE(ERROR_TOO_MANY_SESS)
	CASE(ERROR_SHARING_PAUSED)
	CASE(ERROR_REQ_NOT_ACCEP)
	CASE(ERROR_REDIR_PAUSED)
	CASE(ERROR_FILE_EXISTS)
	CASE(ERROR_CANNOT_MAKE)
	CASE(ERROR_FAIL_I24)
	CASE(ERROR_OUT_OF_STRUCTURES)
	CASE(ERROR_ALREADY_ASSIGNED)
	CASE(ERROR_INVALID_PASSWORD)
	CASE(ERROR_INVALID_PARAMETER)
	CASE(ERROR_NET_WRITE_FAULT)
	CASE(ERROR_NO_PROC_SLOTS)
	CASE(ERROR_TOO_MANY_SEMAPHORES)
	CASE(ERROR_EXCL_SEM_ALREADY_OWNED)
	CASE(ERROR_SEM_IS_SET)
	CASE(ERROR_TOO_MANY_SEM_REQUESTS)
	CASE(ERROR_INVALID_AT_INTERRUPT_TIME)
	CASE(ERROR_SEM_OWNER_DIED)
	CASE(ERROR_SEM_USER_LIMIT)
	CASE(ERROR_DISK_CHANGE)
	CASE(ERROR_DRIVE_LOCKED)
	CASE(ERROR_BROKEN_PIPE)
	CASE(ERROR_OPEN_FAILED)
	CASE(ERROR_BUFFER_OVERFLOW)
	CASE(ERROR_DISK_FULL)
	CASE(ERROR_NO_MORE_SEARCH_HANDLES)
	CASE(ERROR_INVALID_TARGET_HANDLE)
	CASE(ERROR_INVALID_CATEGORY)
	CASE(ERROR_INVALID_VERIFY_SWITCH)
	CASE(ERROR_BAD_DRIVER_LEVEL)
	CASE(ERROR_CALL_NOT_IMPLEMENTED)
	CASE(ERROR_SEM_TIMEOUT)
	CASE(ERROR_INSUFFICIENT_BUFFER)
	CASE(ERROR_INVALID_NAME)
	CASE(ERROR_INVALID_LEVEL)
	CASE(ERROR_NO_VOLUME_LABEL)
	CASE(ERROR_MOD_NOT_FOUND)
	CASE(ERROR_PROC_NOT_FOUND)
	CASE(ERROR_WAIT_NO_CHILDREN)
	CASE(ERROR_CHILD_NOT_COMPLETE)
	CASE(ERROR_DIRECT_ACCESS_HANDLE)
	CASE(ERROR_NEGATIVE_SEEK)
	CASE(ERROR_SEEK_ON_DEVICE)
	CASE(ERROR_IS_JOIN_TARGET)
	CASE(ERROR_IS_JOINED)
	CASE(ERROR_IS_SUBSTED)
	CASE(ERROR_NOT_JOINED)
	CASE(ERROR_NOT_SUBSTED)
	CASE(ERROR_JOIN_TO_JOIN)
	CASE(ERROR_SUBST_TO_SUBST)
	CASE(ERROR_JOIN_TO_SUBST)
	CASE(ERROR_SUBST_TO_JOIN)
	CASE(ERROR_BUSY_DRIVE)
	CASE(ERROR_SAME_DRIVE)
	CASE(ERROR_DIR_NOT_ROOT)
	CASE(ERROR_DIR_NOT_EMPTY)
	CASE(ERROR_IS_SUBST_PATH)
	CASE(ERROR_IS_JOIN_PATH)
	CASE(ERROR_PATH_BUSY)
	CASE(ERROR_IS_SUBST_TARGET)
	CASE(ERROR_SYSTEM_TRACE)
	CASE(ERROR_INVALID_EVENT_COUNT)
	CASE(ERROR_TOO_MANY_MUXWAITERS)
	CASE(ERROR_INVALID_LIST_FORMAT)
	CASE(ERROR_LABEL_TOO_LONG)
	CASE(ERROR_TOO_MANY_TCBS)
	CASE(ERROR_SIGNAL_REFUSED)
	CASE(ERROR_DISCARDED)
	CASE(ERROR_NOT_LOCKED)
	CASE(ERROR_BAD_THREADID_ADDR)
	CASE(ERROR_BAD_ARGUMENTS)
	CASE(ERROR_BAD_PATHNAME)
	CASE(ERROR_SIGNAL_PENDING)
	CASE(ERROR_MAX_THRDS_REACHED)
	CASE(ERROR_LOCK_FAILED)
	CASE(ERROR_BUSY)
	CASE(ERROR_CANCEL_VIOLATION)
	CASE(ERROR_ATOMIC_LOCKS_NOT_SUPPORTED)
	CASE(ERROR_INVALID_SEGMENT_NUMBER)
	CASE(ERROR_INVALID_ORDINAL)
	CASE(ERROR_ALREADY_EXISTS)
	CASE(ERROR_INVALID_FLAG_NUMBER)
	CASE(ERROR_SEM_NOT_FOUND)
	CASE(ERROR_INVALID_STARTING_CODESEG)
	CASE(ERROR_INVALID_STACKSEG)
	CASE(ERROR_INVALID_MODULETYPE)
	CASE(ERROR_INVALID_EXE_SIGNATURE)
	CASE(ERROR_EXE_MARKED_INVALID)
	CASE(ERROR_BAD_EXE_FORMAT)
	CASE(ERROR_ITERATED_DATA_EXCEEDS_64k)
	CASE(ERROR_INVALID_MINALLOCSIZE)
	CASE(ERROR_DYNLINK_FROM_INVALID_RING)
	CASE(ERROR_IOPL_NOT_ENABLED)
	CASE(ERROR_INVALID_SEGDPL)
	CASE(ERROR_AUTODATASEG_EXCEEDS_64k)
	CASE(ERROR_RING2SEG_MUST_BE_MOVABLE)
	CASE(ERROR_RELOC_CHAIN_XEEDS_SEGLIM)
	CASE(ERROR_INFLOOP_IN_RELOC_CHAIN)
	CASE(ERROR_ENVVAR_NOT_FOUND)
	CASE(ERROR_NO_SIGNAL_SENT)
	CASE(ERROR_FILENAME_EXCED_RANGE)
	CASE(ERROR_RING2_STACK_IN_USE)
	CASE(ERROR_META_EXPANSION_TOO_LONG)
	CASE(ERROR_INVALID_SIGNAL_NUMBER)
	CASE(ERROR_THREAD_1_INACTIVE)
	CASE(ERROR_LOCKED)
	CASE(ERROR_TOO_MANY_MODULES)
	CASE(ERROR_NESTING_NOT_ALLOWED)
	CASE(ERROR_EXE_MACHINE_TYPE_MISMATCH)
	CASE(ERROR_EXE_CANNOT_MODIFY_SIGNED_BINARY)
	CASE(ERROR_EXE_CANNOT_MODIFY_STRONG_SIGNED_BINARY)
	CASE(ERROR_BAD_PIPE)
	CASE(ERROR_PIPE_BUSY)
	CASE(ERROR_NO_DATA)
	CASE(ERROR_PIPE_NOT_CONNECTED)
	CASE(ERROR_MORE_DATA)
	CASE(ERROR_VC_DISCONNECTED)
	CASE(ERROR_INVALID_EA_NAME)
	CASE(ERROR_EA_LIST_INCONSISTENT)
	CASE(WAIT_TIMEOUT)
	CASE(ERROR_NO_MORE_ITEMS)
	CASE(ERROR_CANNOT_COPY)
	CASE(ERROR_DIRECTORY)
	CASE(ERROR_EAS_DIDNT_FIT)
	CASE(ERROR_EA_FILE_CORRUPT)
	CASE(ERROR_EA_TABLE_FULL)
	CASE(ERROR_INVALID_EA_HANDLE)
	CASE(ERROR_EAS_NOT_SUPPORTED)
	CASE(ERROR_NOT_OWNER)
	CASE(ERROR_TOO_MANY_POSTS)
	CASE(ERROR_PARTIAL_COPY)
	CASE(ERROR_OPLOCK_NOT_GRANTED)
	CASE(ERROR_INVALID_OPLOCK_PROTOCOL)
	CASE(ERROR_DISK_TOO_FRAGMENTED)
	CASE(ERROR_DELETE_PENDING)
	CASE(ERROR_MR_MID_NOT_FOUND)
	CASE(ERROR_SCOPE_NOT_FOUND)
	CASE(ERROR_INVALID_ADDRESS)
	CASE(ERROR_ARITHMETIC_OVERFLOW)
	CASE(ERROR_PIPE_CONNECTED)
	CASE(ERROR_PIPE_LISTENING)
	CASE(ERROR_EA_ACCESS_DENIED)
	CASE(ERROR_OPERATION_ABORTED)
	CASE(ERROR_IO_INCOMPLETE)
	CASE(ERROR_IO_PENDING)
	CASE(ERROR_NOACCESS)
	CASE(ERROR_SWAPERROR)
	CASE(ERROR_STACK_OVERFLOW)
	CASE(ERROR_INVALID_MESSAGE)
	CASE(ERROR_CAN_NOT_COMPLETE)
	CASE(ERROR_INVALID_FLAGS)
	CASE(ERROR_UNRECOGNIZED_VOLUME)
	CASE(ERROR_FILE_INVALID)
	CASE(ERROR_FULLSCREEN_MODE)
	CASE(ERROR_NO_TOKEN)
	CASE(ERROR_BADDB)
	CASE(ERROR_BADKEY)
	CASE(ERROR_CANTOPEN)
	CASE(ERROR_CANTREAD)
	CASE(ERROR_CANTWRITE)
	CASE(ERROR_REGISTRY_RECOVERED)
	CASE(ERROR_REGISTRY_CORRUPT)
	CASE(ERROR_REGISTRY_IO_FAILED)
	CASE(ERROR_NOT_REGISTRY_FILE)
	CASE(ERROR_KEY_DELETED)
	CASE(ERROR_NO_LOG_SPACE)
	CASE(ERROR_KEY_HAS_CHILDREN)
	CASE(ERROR_CHILD_MUST_BE_VOLATILE)
	CASE(ERROR_NOTIFY_ENUM_DIR)
	CASE(ERROR_DEPENDENT_SERVICES_RUNNING)
	CASE(ERROR_INVALID_SERVICE_CONTROL)
	CASE(ERROR_SERVICE_REQUEST_TIMEOUT)
	CASE(ERROR_SERVICE_NO_THREAD)
	CASE(ERROR_SERVICE_DATABASE_LOCKED)
	CASE(ERROR_SERVICE_ALREADY_RUNNING)
	CASE(ERROR_INVALID_SERVICE_ACCOUNT)
	CASE(ERROR_SERVICE_DISABLED)
	CASE(ERROR_CIRCULAR_DEPENDENCY)
	CASE(ERROR_SERVICE_DOES_NOT_EXIST)
	CASE(ERROR_SERVICE_CANNOT_ACCEPT_CTRL)
	CASE(ERROR_SERVICE_NOT_ACTIVE)
	CASE(ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
	CASE(ERROR_EXCEPTION_IN_SERVICE)
	CASE(ERROR_DATABASE_DOES_NOT_EXIST)
	CASE(ERROR_SERVICE_SPECIFIC_ERROR)
	CASE(ERROR_PROCESS_ABORTED)
	CASE(ERROR_SERVICE_DEPENDENCY_FAIL)
	CASE(ERROR_SERVICE_LOGON_FAILED)
	CASE(ERROR_SERVICE_START_HANG)
	CASE(ERROR_INVALID_SERVICE_LOCK)
	CASE(ERROR_SERVICE_MARKED_FOR_DELETE)
	CASE(ERROR_SERVICE_EXISTS)
	CASE(ERROR_ALREADY_RUNNING_LKG)
	CASE(ERROR_SERVICE_DEPENDENCY_DELETED)
	CASE(ERROR_BOOT_ALREADY_ACCEPTED)
	CASE(ERROR_SERVICE_NEVER_STARTED)
	CASE(ERROR_DUPLICATE_SERVICE_NAME)
	CASE(ERROR_DIFFERENT_SERVICE_ACCOUNT)
	CASE(ERROR_CANNOT_DETECT_DRIVER_FAILURE)
	CASE(ERROR_CANNOT_DETECT_PROCESS_ABORT)
	CASE(ERROR_NO_RECOVERY_PROGRAM)
	CASE(ERROR_SERVICE_NOT_IN_EXE)
	CASE(ERROR_NOT_SAFEBOOT_SERVICE)
	CASE(ERROR_END_OF_MEDIA)
	CASE(ERROR_FILEMARK_DETECTED)
	CASE(ERROR_BEGINNING_OF_MEDIA)
	CASE(ERROR_SETMARK_DETECTED)
	CASE(ERROR_NO_DATA_DETECTED)
	CASE(ERROR_PARTITION_FAILURE)
	CASE(ERROR_INVALID_BLOCK_LENGTH)
	CASE(ERROR_DEVICE_NOT_PARTITIONED)
	CASE(ERROR_UNABLE_TO_LOCK_MEDIA)
	CASE(ERROR_MEDIA_CHANGED)
	CASE(ERROR_BUS_RESET)
	CASE(ERROR_NO_MEDIA_IN_DRIVE)
	CASE(ERROR_NO_UNICODE_TRANSLATION)
	CASE(ERROR_DLL_INIT_FAILED)
	CASE(ERROR_SHUTDOWN_IN_PROGRESS)
	CASE(ERROR_NO_SHUTDOWN_IN_PROGRESS)
	CASE(ERROR_IO_DEVICE)
	CASE(ERROR_SERIAL_NO_DEVICE)
	CASE(ERROR_IRQ_BUSY)
	CASE(ERROR_MORE_WRITES)
	CASE(ERROR_COUNTER_TIMEOUT)
	CASE(ERROR_FLOPPY_ID_MARK_NOT_FOUND)
	CASE(ERROR_FLOPPY_WRONG_CYLINDER)
	CASE(ERROR_FLOPPY_UNKNOWN_ERROR)
	CASE(ERROR_FLOPPY_BAD_REGISTERS)
	CASE(ERROR_DISK_RECALIBRATE_FAILED)
	CASE(ERROR_DISK_OPERATION_FAILED)
	CASE(ERROR_DISK_RESET_FAILED)
	CASE(ERROR_EOM_OVERFLOW)
	CASE(ERROR_NOT_ENOUGH_SERVER_MEMORY)
	CASE(ERROR_POSSIBLE_DEADLOCK)
	CASE(ERROR_MAPPED_ALIGNMENT)
	CASE(ERROR_SET_POWER_STATE_VETOED)
	CASE(ERROR_SET_POWER_STATE_FAILED)
	CASE(ERROR_TOO_MANY_LINKS)
	CASE(ERROR_OLD_WIN_VERSION)
	CASE(ERROR_APP_WRONG_OS)
	CASE(ERROR_SINGLE_INSTANCE_APP)
	CASE(ERROR_RMODE_APP)
	CASE(ERROR_INVALID_DLL)
	CASE(ERROR_NO_ASSOCIATION)
	CASE(ERROR_DDE_FAIL)
	CASE(ERROR_DLL_NOT_FOUND)
	CASE(ERROR_NO_MORE_USER_HANDLES)
	CASE(ERROR_MESSAGE_SYNC_ONLY)
	CASE(ERROR_SOURCE_ELEMENT_EMPTY)
	CASE(ERROR_DESTINATION_ELEMENT_FULL)
	CASE(ERROR_ILLEGAL_ELEMENT_ADDRESS)
	CASE(ERROR_MAGAZINE_NOT_PRESENT)
	CASE(ERROR_DEVICE_REINITIALIZATION_NEEDED)
	CASE(ERROR_DEVICE_REQUIRES_CLEANING)
	CASE(ERROR_DEVICE_DOOR_OPEN)
	CASE(ERROR_DEVICE_NOT_CONNECTED)
	CASE(ERROR_NOT_FOUND)
	CASE(ERROR_NO_MATCH)
	CASE(ERROR_SET_NOT_FOUND)
	CASE(ERROR_POINT_NOT_FOUND)
	CASE(ERROR_NO_TRACKING_SERVICE)
	CASE(ERROR_NO_VOLUME_ID)
	CASE(ERROR_UNABLE_TO_REMOVE_REPLACED)
	CASE(ERROR_UNABLE_TO_MOVE_REPLACEMENT)
	CASE(ERROR_UNABLE_TO_MOVE_REPLACEMENT_2)
	CASE(ERROR_JOURNAL_DELETE_IN_PROGRESS)
	CASE(ERROR_JOURNAL_NOT_ACTIVE)
	CASE(ERROR_POTENTIAL_FILE_FOUND)
	CASE(ERROR_JOURNAL_ENTRY_DELETED)
	CASE(ERROR_BAD_DEVICE)
	CASE(ERROR_CONNECTION_UNAVAIL)
	CASE(ERROR_DEVICE_ALREADY_REMEMBERED)
	CASE(ERROR_NO_NET_OR_BAD_PATH)
	CASE(ERROR_BAD_PROVIDER)
	CASE(ERROR_CANNOT_OPEN_PROFILE)
	CASE(ERROR_NOT_CONTAINER)
	CASE(ERROR_EXTENDED_ERROR)
	CASE(ERROR_INVALID_GROUPNAME)
	CASE(ERROR_INVALID_COMPUTERNAME)
	CASE(ERROR_INVALID_EVENTNAME)
	CASE(ERROR_INVALID_DOMAINNAME)
	CASE(ERROR_INVALID_SERVICENAME)
	CASE(ERROR_INVALID_NETNAME)
	CASE(ERROR_INVALID_SHARENAME)
	CASE(ERROR_INVALID_PASSWORDNAME)
	CASE(ERROR_INVALID_MESSAGENAME)
	CASE(ERROR_INVALID_MESSAGEDEST)
	CASE(ERROR_SESSION_CREDENTIAL_CONFLICT)
	CASE(ERROR_REMOTE_SESSION_LIMIT_EXCEEDED)
	CASE(ERROR_DUP_DOMAINNAME)
	CASE(ERROR_NO_NETWORK)
	CASE(ERROR_CANCELLED)
	CASE(ERROR_USER_MAPPED_FILE)
	CASE(ERROR_CONNECTION_REFUSED)
	CASE(ERROR_GRACEFUL_DISCONNECT)
	CASE(ERROR_ADDRESS_ALREADY_ASSOCIATED)
	CASE(ERROR_ADDRESS_NOT_ASSOCIATED)
	CASE(ERROR_CONNECTION_INVALID)
	CASE(ERROR_CONNECTION_ACTIVE)
	CASE(ERROR_NETWORK_UNREACHABLE)
	CASE(ERROR_HOST_UNREACHABLE)
	CASE(ERROR_PROTOCOL_UNREACHABLE)
	CASE(ERROR_PORT_UNREACHABLE)
	CASE(ERROR_REQUEST_ABORTED)
	CASE(ERROR_CONNECTION_ABORTED)
	CASE(ERROR_RETRY)
	CASE(ERROR_CONNECTION_COUNT_LIMIT)
	CASE(ERROR_LOGIN_TIME_RESTRICTION)
	CASE(ERROR_LOGIN_WKSTA_RESTRICTION)
	CASE(ERROR_INCORRECT_ADDRESS)
	CASE(ERROR_ALREADY_REGISTERED)
	CASE(ERROR_SERVICE_NOT_FOUND)
	CASE(ERROR_NOT_AUTHENTICATED)
	CASE(ERROR_NOT_LOGGED_ON)
	CASE(ERROR_CONTINUE)
	CASE(ERROR_ALREADY_INITIALIZED)
	CASE(ERROR_NO_MORE_DEVICES)
	CASE(ERROR_NO_SUCH_SITE)
	CASE(ERROR_DOMAIN_CONTROLLER_EXISTS)
	CASE(ERROR_ONLY_IF_CONNECTED)
	CASE(ERROR_OVERRIDE_NOCHANGES)
	CASE(ERROR_BAD_USER_PROFILE)
	CASE(ERROR_NOT_SUPPORTED_ON_SBS)
	CASE(ERROR_SERVER_SHUTDOWN_IN_PROGRESS)
	CASE(ERROR_HOST_DOWN)
	CASE(ERROR_NON_ACCOUNT_SID)
	CASE(ERROR_NON_DOMAIN_SID)
	CASE(ERROR_APPHELP_BLOCK)
	CASE(ERROR_ACCESS_DISABLED_BY_POLICY)
	CASE(ERROR_REG_NAT_CONSUMPTION)
	CASE(ERROR_CSCSHARE_OFFLINE)
	CASE(ERROR_PKINIT_FAILURE)
	CASE(ERROR_SMARTCARD_SUBSYSTEM_FAILURE)
	CASE(ERROR_DOWNGRADE_DETECTED)
	CASE(ERROR_MACHINE_LOCKED)
	CASE(ERROR_CALLBACK_SUPPLIED_INVALID_DATA)
	CASE(ERROR_SYNC_FOREGROUND_REFRESH_REQUIRED)
	CASE(ERROR_DRIVER_BLOCKED)
	CASE(ERROR_INVALID_IMPORT_OF_NON_DLL)
	CASE(ERROR_ACCESS_DISABLED_WEBBLADE)
	CASE(ERROR_ACCESS_DISABLED_WEBBLADE_TAMPER)
	CASE(ERROR_RECOVERY_FAILURE)
	CASE(ERROR_ALREADY_FIBER)
	CASE(ERROR_ALREADY_THREAD)
	CASE(ERROR_STACK_BUFFER_OVERRUN)
	CASE(ERROR_PARAMETER_QUOTA_EXCEEDED)
	CASE(ERROR_DEBUGGER_INACTIVE)
	CASE(ERROR_DELAY_LOAD_FAILED)
	CASE(ERROR_VDM_DISALLOWED)
	CASE(ERROR_UNIDENTIFIED_ERROR)
	/* Security Status Codes ..... */
	CASE(ERROR_NOT_ALL_ASSIGNED)
	CASE(ERROR_SOME_NOT_MAPPED)
	CASE(ERROR_NO_QUOTAS_FOR_ACCOUNT)
	CASE(ERROR_LOCAL_USER_SESSION_KEY)
	CASE(ERROR_NULL_LM_PASSWORD)
	CASE(ERROR_UNKNOWN_REVISION)
	CASE(ERROR_REVISION_MISMATCH)
	CASE(ERROR_INVALID_OWNER)
	CASE(ERROR_INVALID_PRIMARY_GROUP)
	CASE(ERROR_NO_IMPERSONATION_TOKEN)
	CASE(ERROR_CANT_DISABLE_MANDATORY)
	CASE(ERROR_NO_LOGON_SERVERS)
	CASE(ERROR_NO_SUCH_LOGON_SESSION)
	CASE(ERROR_NO_SUCH_PRIVILEGE)
	CASE(ERROR_PRIVILEGE_NOT_HELD)
	CASE(ERROR_INVALID_ACCOUNT_NAME)
	CASE(ERROR_USER_EXISTS)
	CASE(ERROR_NO_SUCH_USER)
	CASE(ERROR_GROUP_EXISTS)
	CASE(ERROR_NO_SUCH_GROUP)
	CASE(ERROR_MEMBER_IN_GROUP)
	CASE(ERROR_LAST_ADMIN)
	CASE(ERROR_WRONG_PASSWORD)
	CASE(ERROR_ILL_FORMED_PASSWORD)
	CASE(ERROR_PASSWORD_RESTRICTION)
	CASE(ERROR_LOGON_FAILURE)
	CASE(ERROR_ACCOUNT_RESTRICTION)
	CASE(ERROR_INVALID_LOGON_HOURS)
	CASE(ERROR_INVALID_WORKSTATION)
	CASE(ERROR_PASSWORD_EXPIRED)
	CASE(ERROR_ACCOUNT_DISABLED)
	CASE(ERROR_NONE_MAPPED)
	CASE(ERROR_TOO_MANY_LUIDS_REQUESTED)
	CASE(ERROR_LUIDS_EXHAUSTED)
	CASE(ERROR_INVALID_SUB_AUTHORITY)
	CASE(ERROR_INVALID_ACL)
	CASE(ERROR_INVALID_SID)
	CASE(ERROR_INVALID_SECURITY_DESCR)
	CASE(ERROR_BAD_INHERITANCE_ACL)
	CASE(ERROR_SERVER_DISABLED)
	CASE(ERROR_SERVER_NOT_DISABLED)
	CASE(ERROR_INVALID_ID_AUTHORITY)
	CASE(ERROR_ALLOTTED_SPACE_EXCEEDED)
	CASE(ERROR_INVALID_GROUP_ATTRIBUTES)
	CASE(ERROR_BAD_IMPERSONATION_LEVEL)
	CASE(ERROR_CANT_OPEN_ANONYMOUS)
	CASE(ERROR_BAD_VALIDATION_CLASS)
	CASE(ERROR_BAD_TOKEN_TYPE)
	CASE(ERROR_NO_SECURITY_ON_OBJECT)
	CASE(ERROR_CANT_ACCESS_DOMAIN_INFO)
	CASE(ERROR_INVALID_SERVER_STATE)
	CASE(ERROR_INVALID_DOMAIN_STATE)
	CASE(ERROR_INVALID_DOMAIN_ROLE)
	CASE(ERROR_NO_SUCH_DOMAIN)
	CASE(ERROR_DOMAIN_EXISTS)
	CASE(ERROR_DOMAIN_LIMIT_EXCEEDED)
	CASE(ERROR_INTERNAL_DB_CORRUPTION)
	CASE(ERROR_INTERNAL_ERROR)
	CASE(ERROR_GENERIC_NOT_MAPPED)
	CASE(ERROR_BAD_DESCRIPTOR_FORMAT)
	CASE(ERROR_NOT_LOGON_PROCESS)
	CASE(ERROR_LOGON_SESSION_EXISTS)
	CASE(ERROR_NO_SUCH_PACKAGE)
	CASE(ERROR_BAD_LOGON_SESSION_STATE)
	CASE(ERROR_LOGON_SESSION_COLLISION)
	CASE(ERROR_INVALID_LOGON_TYPE)
	CASE(ERROR_CANNOT_IMPERSONATE)
	CASE(ERROR_RXACT_INVALID_STATE)
	CASE(ERROR_RXACT_COMMIT_FAILURE)
	CASE(ERROR_SPECIAL_ACCOUNT)
	CASE(ERROR_SPECIAL_GROUP)
	CASE(ERROR_SPECIAL_USER)
	CASE(ERROR_MEMBERS_PRIMARY_GROUP)
	CASE(ERROR_TOKEN_ALREADY_IN_USE)
	CASE(ERROR_NO_SUCH_ALIAS)
	CASE(ERROR_MEMBER_NOT_IN_ALIAS)
	CASE(ERROR_MEMBER_IN_ALIAS)
	CASE(ERROR_ALIAS_EXISTS)
	CASE(ERROR_LOGON_NOT_GRANTED)
	CASE(ERROR_TOO_MANY_SECRETS)
	CASE(ERROR_SECRET_TOO_LONG)
	CASE(ERROR_INTERNAL_DB_ERROR)
	CASE(ERROR_TOO_MANY_CONTEXT_IDS)
	CASE(ERROR_LOGON_TYPE_NOT_GRANTED)
	CASE(ERROR_NT_CROSS_ENCRYPTION_REQUIRED)
	CASE(ERROR_NO_SUCH_MEMBER)
	CASE(ERROR_INVALID_MEMBER)
	CASE(ERROR_TOO_MANY_SIDS)
	CASE(ERROR_LM_CROSS_ENCRYPTION_REQUIRED)
	CASE(ERROR_NO_INHERITANCE)
	CASE(ERROR_FILE_CORRUPT)
	CASE(ERROR_DISK_CORRUPT)
	CASE(ERROR_NO_USER_SESSION_KEY)
	CASE(ERROR_LICENSE_QUOTA_EXCEEDED)
	CASE(ERROR_WRONG_TARGET_NAME)
	CASE(ERROR_MUTUAL_AUTH_FAILED)
	CASE(ERROR_TIME_SKEW)
	CASE(ERROR_CURRENT_DOMAIN_NOT_ALLOWED)
	/* WinUser Error Codes */
	CASE(ERROR_INVALID_WINDOW_HANDLE)
	CASE(ERROR_INVALID_MENU_HANDLE)
	CASE(ERROR_INVALID_CURSOR_HANDLE)
	CASE(ERROR_INVALID_ACCEL_HANDLE)
	CASE(ERROR_INVALID_HOOK_HANDLE)
	CASE(ERROR_INVALID_DWP_HANDLE)
	CASE(ERROR_TLW_WITH_WSCHILD)
	CASE(ERROR_CANNOT_FIND_WND_CLASS)
	CASE(ERROR_WINDOW_OF_OTHER_THREAD)
	CASE(ERROR_HOTKEY_ALREADY_REGISTERED)
	CASE(ERROR_CLASS_ALREADY_EXISTS)
	CASE(ERROR_CLASS_DOES_NOT_EXIST)
	CASE(ERROR_CLASS_HAS_WINDOWS)
	CASE(ERROR_INVALID_INDEX)
	CASE(ERROR_INVALID_ICON_HANDLE)
	CASE(ERROR_PRIVATE_DIALOG_INDEX)
	CASE(ERROR_LISTBOX_ID_NOT_FOUND)
	CASE(ERROR_NO_WILDCARD_CHARACTERS)
	CASE(ERROR_CLIPBOARD_NOT_OPEN)
	CASE(ERROR_HOTKEY_NOT_REGISTERED)
	CASE(ERROR_WINDOW_NOT_DIALOG)
	CASE(ERROR_CONTROL_ID_NOT_FOUND)
	CASE(ERROR_INVALID_COMBOBOX_MESSAGE)
	CASE(ERROR_WINDOW_NOT_COMBOBOX)
	CASE(ERROR_INVALID_EDIT_HEIGHT)
	CASE(ERROR_DC_NOT_FOUND)
	CASE(ERROR_INVALID_HOOK_FILTER)
	CASE(ERROR_INVALID_FILTER_PROC)
	CASE(ERROR_HOOK_NEEDS_HMOD)
	CASE(ERROR_GLOBAL_ONLY_HOOK)
	CASE(ERROR_JOURNAL_HOOK_SET)
	CASE(ERROR_HOOK_NOT_INSTALLED)
	CASE(ERROR_INVALID_LB_MESSAGE)
	CASE(ERROR_SETCOUNT_ON_BAD_LB)
	CASE(ERROR_LB_WITHOUT_TABSTOPS)
	CASE(ERROR_DESTROY_OBJECT_OF_OTHER_THREAD)
	CASE(ERROR_CHILD_WINDOW_MENU)
	CASE(ERROR_NO_SYSTEM_MENU)
	CASE(ERROR_INVALID_MSGBOX_STYLE)
	CASE(ERROR_INVALID_SPI_VALUE)
	CASE(ERROR_SCREEN_ALREADY_LOCKED)
	CASE(ERROR_HWNDS_HAVE_DIFF_PARENT)
	CASE(ERROR_NOT_CHILD_WINDOW)
	CASE(ERROR_INVALID_GW_COMMAND)
	CASE(ERROR_INVALID_THREAD_ID)
	CASE(ERROR_NON_MDICHILD_WINDOW)
	CASE(ERROR_POPUP_ALREADY_ACTIVE)
	CASE(ERROR_NO_SCROLLBARS)
	CASE(ERROR_INVALID_SCROLLBAR_RANGE)
	CASE(ERROR_INVALID_SHOWWIN_COMMAND)
	CASE(ERROR_NO_SYSTEM_RESOURCES)
	CASE(ERROR_NONPAGED_SYSTEM_RESOURCES)
	CASE(ERROR_PAGED_SYSTEM_RESOURCES)
	CASE(ERROR_WORKING_SET_QUOTA)
	CASE(ERROR_PAGEFILE_QUOTA)
	CASE(ERROR_COMMITMENT_LIMIT)
	CASE(ERROR_MENU_ITEM_NOT_FOUND)
	CASE(ERROR_INVALID_KEYBOARD_HANDLE)
	CASE(ERROR_HOOK_TYPE_NOT_ALLOWED)
	CASE(ERROR_REQUIRES_INTERACTIVE_WINDOWSTATION)
	CASE(ERROR_TIMEOUT)
	CASE(ERROR_INVALID_MONITOR_HANDLE)
	/* Eventlog Status Codes */
	CASE(ERROR_EVENTLOG_FILE_CORRUPT)
	CASE(ERROR_EVENTLOG_CANT_START)
	CASE(ERROR_LOG_FILE_FULL)
	CASE(ERROR_EVENTLOG_FILE_CHANGED)
	/* MSI Error Codes */
	CASE(ERROR_INSTALL_SERVICE_FAILURE)
	CASE(ERROR_INSTALL_USEREXIT)
	CASE(ERROR_INSTALL_FAILURE)
	CASE(ERROR_INSTALL_SUSPEND)
	CASE(ERROR_UNKNOWN_PRODUCT)
	CASE(ERROR_UNKNOWN_FEATURE)
	CASE(ERROR_UNKNOWN_COMPONENT)
	CASE(ERROR_UNKNOWN_PROPERTY)
	CASE(ERROR_INVALID_HANDLE_STATE)
	CASE(ERROR_BAD_CONFIGURATION)
	CASE(ERROR_INDEX_ABSENT)
	CASE(ERROR_INSTALL_SOURCE_ABSENT)
	CASE(ERROR_INSTALL_PACKAGE_VERSION)
	CASE(ERROR_PRODUCT_UNINSTALLED)
	CASE(ERROR_BAD_QUERY_SYNTAX)
	CASE(ERROR_INVALID_FIELD)
	CASE(ERROR_DEVICE_REMOVED)
	CASE(ERROR_INSTALL_ALREADY_RUNNING)
	CASE(ERROR_INSTALL_PACKAGE_OPEN_FAILED)
	CASE(ERROR_INSTALL_PACKAGE_INVALID)
	CASE(ERROR_INSTALL_UI_FAILURE)
	CASE(ERROR_INSTALL_LOG_FAILURE)
	CASE(ERROR_INSTALL_LANGUAGE_UNSUPPORTED)
	CASE(ERROR_INSTALL_TRANSFORM_FAILURE)
	CASE(ERROR_INSTALL_PACKAGE_REJECTED)
	CASE(ERROR_FUNCTION_NOT_CALLED)
	CASE(ERROR_FUNCTION_FAILED)
	CASE(ERROR_INVALID_TABLE)
	CASE(ERROR_DATATYPE_MISMATCH)
	CASE(ERROR_UNSUPPORTED_TYPE)
	CASE(ERROR_CREATE_FAILED)
	CASE(ERROR_INSTALL_TEMP_UNWRITABLE)
	CASE(ERROR_INSTALL_PLATFORM_UNSUPPORTED)
	CASE(ERROR_INSTALL_NOTUSED)
	CASE(ERROR_PATCH_PACKAGE_OPEN_FAILED)
	CASE(ERROR_PATCH_PACKAGE_INVALID)
	CASE(ERROR_PATCH_PACKAGE_UNSUPPORTED)
	CASE(ERROR_PRODUCT_VERSION)
	CASE(ERROR_INVALID_COMMAND_LINE)
	CASE(ERROR_INSTALL_REMOTE_DISALLOWED)
	CASE(ERROR_SUCCESS_REBOOT_INITIATED)
	CASE(ERROR_PATCH_TARGET_NOT_FOUND)
	CASE(ERROR_PATCH_PACKAGE_REJECTED)
	CASE(ERROR_INSTALL_TRANSFORM_REJECTED)
	CASE(ERROR_INSTALL_REMOTE_PROHIBITED)
	/*   RPC Status Codes */
	CASE(RPC_S_INVALID_STRING_BINDING)
	CASE(RPC_S_WRONG_KIND_OF_BINDING)
	CASE(RPC_S_INVALID_BINDING)
	CASE(RPC_S_PROTSEQ_NOT_SUPPORTED)
	CASE(RPC_S_INVALID_RPC_PROTSEQ)
	CASE(RPC_S_INVALID_STRING_UUID)
	CASE(RPC_S_INVALID_ENDPOINT_FORMAT)
	CASE(RPC_S_INVALID_NET_ADDR)
	CASE(RPC_S_NO_ENDPOINT_FOUND)
	CASE(RPC_S_INVALID_TIMEOUT)
	CASE(RPC_S_OBJECT_NOT_FOUND)
	CASE(RPC_S_ALREADY_REGISTERED)
	CASE(RPC_S_TYPE_ALREADY_REGISTERED)
	CASE(RPC_S_ALREADY_LISTENING)
	CASE(RPC_S_NO_PROTSEQS_REGISTERED)
	CASE(RPC_S_NOT_LISTENING)
	CASE(RPC_S_UNKNOWN_MGR_TYPE)
	CASE(RPC_S_UNKNOWN_IF)
	CASE(RPC_S_NO_BINDINGS)
	CASE(RPC_S_NO_PROTSEQS)
	CASE(RPC_S_CANT_CREATE_ENDPOINT)
	CASE(RPC_S_OUT_OF_RESOURCES)
	CASE(RPC_S_SERVER_UNAVAILABLE)
	CASE(RPC_S_SERVER_TOO_BUSY)
	CASE(RPC_S_INVALID_NETWORK_OPTIONS)
	CASE(RPC_S_NO_CALL_ACTIVE)
	CASE(RPC_S_CALL_FAILED)
	CASE(RPC_S_CALL_FAILED_DNE)
	CASE(RPC_S_PROTOCOL_ERROR)
	CASE(RPC_S_UNSUPPORTED_TRANS_SYN)
	CASE(RPC_S_UNSUPPORTED_TYPE)
	CASE(RPC_S_INVALID_TAG)
	CASE(RPC_S_INVALID_BOUND)
	CASE(RPC_S_NO_ENTRY_NAME)
	CASE(RPC_S_INVALID_NAME_SYNTAX)
	CASE(RPC_S_UNSUPPORTED_NAME_SYNTAX)
	CASE(RPC_S_UUID_NO_ADDRESS)
	CASE(RPC_S_DUPLICATE_ENDPOINT)
	CASE(RPC_S_UNKNOWN_AUTHN_TYPE)
	CASE(RPC_S_MAX_CALLS_TOO_SMALL)
	CASE(RPC_S_STRING_TOO_LONG)
	CASE(RPC_S_PROTSEQ_NOT_FOUND)
	CASE(RPC_S_PROCNUM_OUT_OF_RANGE)
	CASE(RPC_S_BINDING_HAS_NO_AUTH)
	CASE(RPC_S_UNKNOWN_AUTHN_SERVICE)
	CASE(RPC_S_UNKNOWN_AUTHN_LEVEL)
	CASE(RPC_S_INVALID_AUTH_IDENTITY)
	CASE(RPC_S_UNKNOWN_AUTHZ_SERVICE)
	CASE(EPT_S_INVALID_ENTRY)
	CASE(EPT_S_CANT_PERFORM_OP)
	CASE(EPT_S_NOT_REGISTERED)
	CASE(RPC_S_NOTHING_TO_EXPORT)
	CASE(RPC_S_INCOMPLETE_NAME)
	CASE(RPC_S_INVALID_VERS_OPTION)
	CASE(RPC_S_NO_MORE_MEMBERS)
	CASE(RPC_S_NOT_ALL_OBJS_UNEXPORTED)
	CASE(RPC_S_INTERFACE_NOT_FOUND)
	CASE(RPC_S_ENTRY_ALREADY_EXISTS)
	CASE(RPC_S_ENTRY_NOT_FOUND)
	CASE(RPC_S_NAME_SERVICE_UNAVAILABLE)
	CASE(RPC_S_INVALID_NAF_ID)
	CASE(RPC_S_CANNOT_SUPPORT)
	CASE(RPC_S_NO_CONTEXT_AVAILABLE)
	CASE(RPC_S_INTERNAL_ERROR)
	CASE(RPC_S_ZERO_DIVIDE)
	CASE(RPC_S_ADDRESS_ERROR)
	CASE(RPC_S_FP_DIV_ZERO)
	CASE(RPC_S_FP_UNDERFLOW)
	CASE(RPC_S_FP_OVERFLOW)
	CASE(RPC_X_NO_MORE_ENTRIES)
	CASE(RPC_X_SS_CHAR_TRANS_OPEN_FAIL)
	CASE(RPC_X_SS_CHAR_TRANS_SHORT_FILE)
	CASE(RPC_X_SS_IN_NULL_CONTEXT)
	CASE(RPC_X_SS_CONTEXT_DAMAGED)
	CASE(RPC_X_SS_HANDLES_MISMATCH)
	CASE(RPC_X_SS_CANNOT_GET_CALL_HANDLE)
	CASE(RPC_X_NULL_REF_POINTER)
	CASE(RPC_X_ENUM_VALUE_OUT_OF_RANGE)
	CASE(RPC_X_BYTE_COUNT_TOO_SMALL)
	CASE(RPC_X_BAD_STUB_DATA)
	CASE(ERROR_INVALID_USER_BUFFER)
	CASE(ERROR_UNRECOGNIZED_MEDIA)
	CASE(ERROR_NO_TRUST_LSA_SECRET)
	CASE(ERROR_NO_TRUST_SAM_ACCOUNT)
	CASE(ERROR_TRUSTED_DOMAIN_FAILURE)
	CASE(ERROR_TRUSTED_RELATIONSHIP_FAILURE)
	CASE(ERROR_TRUST_FAILURE)
	CASE(RPC_S_CALL_IN_PROGRESS)
	CASE(ERROR_NETLOGON_NOT_STARTED)
	CASE(ERROR_ACCOUNT_EXPIRED)
	CASE(ERROR_REDIRECTOR_HAS_OPEN_HANDLES)
	CASE(ERROR_PRINTER_DRIVER_ALREADY_INSTALLED)
	CASE(ERROR_UNKNOWN_PORT)
	CASE(ERROR_UNKNOWN_PRINTER_DRIVER)
	CASE(ERROR_UNKNOWN_PRINTPROCESSOR)
	CASE(ERROR_INVALID_SEPARATOR_FILE)
	CASE(ERROR_INVALID_PRIORITY)
	CASE(ERROR_INVALID_PRINTER_NAME)
	CASE(ERROR_PRINTER_ALREADY_EXISTS)
	CASE(ERROR_INVALID_PRINTER_COMMAND)
	CASE(ERROR_INVALID_DATATYPE)
	CASE(ERROR_INVALID_ENVIRONMENT)
	CASE(RPC_S_NO_MORE_BINDINGS)
	CASE(ERROR_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT)
	CASE(ERROR_NOLOGON_WORKSTATION_TRUST_ACCOUNT)
	CASE(ERROR_NOLOGON_SERVER_TRUST_ACCOUNT)
	CASE(ERROR_DOMAIN_TRUST_INCONSISTENT)
	CASE(ERROR_SERVER_HAS_OPEN_HANDLES)
	CASE(ERROR_RESOURCE_DATA_NOT_FOUND)
	CASE(ERROR_RESOURCE_TYPE_NOT_FOUND)
	CASE(ERROR_RESOURCE_NAME_NOT_FOUND)
	CASE(ERROR_RESOURCE_LANG_NOT_FOUND)
	CASE(ERROR_NOT_ENOUGH_QUOTA)
	CASE(RPC_S_NO_INTERFACES)
	CASE(RPC_S_CALL_CANCELLED)
	CASE(RPC_S_BINDING_INCOMPLETE)
	CASE(RPC_S_COMM_FAILURE)
	CASE(RPC_S_UNSUPPORTED_AUTHN_LEVEL)
	CASE(RPC_S_NO_PRINC_NAME)
	CASE(RPC_S_NOT_RPC_ERROR)
	CASE(RPC_S_UUID_LOCAL_ONLY)
	CASE(RPC_S_SEC_PKG_ERROR)
	CASE(RPC_S_NOT_CANCELLED)
	CASE(RPC_X_INVALID_ES_ACTION)
	CASE(RPC_X_WRONG_ES_VERSION)
	CASE(RPC_X_WRONG_STUB_VERSION)
	CASE(RPC_X_INVALID_PIPE_OBJECT)
	CASE(RPC_X_WRONG_PIPE_ORDER)
	CASE(RPC_X_WRONG_PIPE_VERSION)
	CASE(RPC_S_GROUP_MEMBER_NOT_FOUND)
	CASE(EPT_S_CANT_CREATE)
	CASE(RPC_S_INVALID_OBJECT)
	CASE(ERROR_INVALID_TIME)
	CASE(ERROR_INVALID_FORM_NAME)
	CASE(ERROR_INVALID_FORM_SIZE)
	CASE(ERROR_ALREADY_WAITING)
	CASE(ERROR_PRINTER_DELETED)
	CASE(ERROR_INVALID_PRINTER_STATE)
	CASE(ERROR_PASSWORD_MUST_CHANGE)
	CASE(ERROR_DOMAIN_CONTROLLER_NOT_FOUND)
	CASE(ERROR_ACCOUNT_LOCKED_OUT)
	CASE(OR_INVALID_OXID)
	CASE(OR_INVALID_OID)
	CASE(OR_INVALID_SET)
	CASE(RPC_S_SEND_INCOMPLETE)
	CASE(RPC_S_INVALID_ASYNC_HANDLE)
	CASE(RPC_S_INVALID_ASYNC_CALL)
	CASE(RPC_X_PIPE_CLOSED)
	CASE(RPC_X_PIPE_DISCIPLINE_ERROR)
	CASE(RPC_X_PIPE_EMPTY)
	CASE(ERROR_NO_SITENAME)
	CASE(ERROR_CANT_ACCESS_FILE)
	CASE(ERROR_CANT_RESOLVE_FILENAME)
	CASE(RPC_S_ENTRY_TYPE_MISMATCH)
	CASE(RPC_S_NOT_ALL_OBJS_EXPORTED)
	CASE(RPC_S_INTERFACE_NOT_EXPORTED)
	CASE(RPC_S_PROFILE_NOT_ADDED)
	CASE(RPC_S_PRF_ELT_NOT_ADDED)
	CASE(RPC_S_PRF_ELT_NOT_REMOVED)
	CASE(RPC_S_GRP_ELT_NOT_ADDED)
	CASE(RPC_S_GRP_ELT_NOT_REMOVED)
	CASE(ERROR_KM_DRIVER_BLOCKED)
	CASE(ERROR_CONTEXT_EXPIRED)
	CASE(ERROR_PER_USER_TRUST_QUOTA_EXCEEDED)
	CASE(ERROR_ALL_USER_TRUST_QUOTA_EXCEEDED)
	CASE(ERROR_USER_DELETE_TRUST_QUOTA_EXCEEDED)
	CASE(ERROR_AUTHENTICATION_FIREWALL_FAILED)
	CASE(ERROR_REMOTE_PRINT_CONNECTIONS_BLOCKED)
	/*   OpenGL Error Code */
	CASE(ERROR_INVALID_PIXEL_FORMAT)
	CASE(ERROR_BAD_DRIVER)
	CASE(ERROR_INVALID_WINDOW_STYLE)
	CASE(ERROR_METAFILE_NOT_SUPPORTED)
	CASE(ERROR_TRANSFORM_NOT_SUPPORTED)
	CASE(ERROR_CLIPPING_NOT_SUPPORTED)
	/*   Image Color Management Error Code */
	CASE(ERROR_INVALID_CMM)
	CASE(ERROR_INVALID_PROFILE)
	CASE(ERROR_TAG_NOT_FOUND)
	CASE(ERROR_TAG_NOT_PRESENT)
	CASE(ERROR_DUPLICATE_TAG)
	CASE(ERROR_PROFILE_NOT_ASSOCIATED_WITH_DEVICE)
	CASE(ERROR_PROFILE_NOT_FOUND)
	CASE(ERROR_INVALID_COLORSPACE)
	CASE(ERROR_ICM_NOT_ENABLED)
	CASE(ERROR_DELETING_ICM_XFORM)
	CASE(ERROR_INVALID_TRANSFORM)
	CASE(ERROR_COLORSPACE_MISMATCH)
	CASE(ERROR_INVALID_COLORINDEX)
	/* Winnet32 Status Codes // */
	CASE(ERROR_CONNECTED_OTHER_PASSWORD)
	CASE(ERROR_CONNECTED_OTHER_PASSWORD_DEFAULT)
	CASE(ERROR_BAD_USERNAME)
	CASE(ERROR_NOT_CONNECTED)
	CASE(ERROR_OPEN_FILES)
	CASE(ERROR_ACTIVE_CONNECTIONS)
	CASE(ERROR_DEVICE_IN_USE)
	/* Win32 Spooler Error Codes */
	CASE(ERROR_UNKNOWN_PRINT_MONITOR)
	CASE(ERROR_PRINTER_DRIVER_IN_USE)
	CASE(ERROR_SPOOL_FILE_NOT_FOUND)
	CASE(ERROR_SPL_NO_STARTDOC)
	CASE(ERROR_SPL_NO_ADDJOB)
	CASE(ERROR_PRINT_PROCESSOR_ALREADY_INSTALLED)
	CASE(ERROR_PRINT_MONITOR_ALREADY_INSTALLED)
	CASE(ERROR_INVALID_PRINT_MONITOR)
	CASE(ERROR_PRINTER_HAS_JOBS_QUEUED)
	CASE(ERROR_SUCCESS_REBOOT_REQUIRED)
	CASE(ERROR_SUCCESS_RESTART_REQUIRED)
	CASE(ERROR_PRINTER_NOT_FOUND)
	CASE(ERROR_PRINTER_DRIVER_WARNED)
	CASE(ERROR_PRINTER_DRIVER_BLOCKED)
	/* Wins Error Codes */
	CASE(ERROR_WINS_INTERNAL)
	CASE(ERROR_CAN_NOT_DEL_LOCAL_WINS)
	CASE(ERROR_STATIC_INIT)
	CASE(ERROR_INC_BACKUP)
	CASE(ERROR_FULL_BACKUP)
	CASE(ERROR_REC_NON_EXISTENT)
	CASE(ERROR_RPL_NOT_ALLOWED)
	/* DHCP Error Codes */
	CASE(ERROR_DHCP_ADDRESS_CONFLICT)
	/* WMI Error Codes */
	CASE(ERROR_WMI_GUID_NOT_FOUND)
	CASE(ERROR_WMI_INSTANCE_NOT_FOUND)
	CASE(ERROR_WMI_CASEID_NOT_FOUND)
	CASE(ERROR_WMI_TRY_AGAIN)
	CASE(ERROR_WMI_DP_NOT_FOUND)
	CASE(ERROR_WMI_UNRESOLVED_INSTANCE_REF)
	CASE(ERROR_WMI_ALREADY_ENABLED)
	CASE(ERROR_WMI_GUID_DISCONNECTED)
	CASE(ERROR_WMI_SERVER_UNAVAILABLE)
	CASE(ERROR_WMI_DP_FAILED)
	CASE(ERROR_WMI_INVALID_MOF)
	CASE(ERROR_WMI_INVALID_REGINFO)
	CASE(ERROR_WMI_ALREADY_DISABLED)
	CASE(ERROR_WMI_READ_ONLY)
	CASE(ERROR_WMI_SET_FAILURE)
	/* NT Media Services (RSM) Error Codes */
	CASE(ERROR_INVALID_MEDIA)
	CASE(ERROR_INVALID_LIBRARY)
	CASE(ERROR_INVALID_MEDIA_POOL)
	CASE(ERROR_DRIVE_MEDIA_MISMATCH)
	CASE(ERROR_MEDIA_OFFLINE)
	CASE(ERROR_LIBRARY_OFFLINE)
	CASE(ERROR_EMPTY)
	CASE(ERROR_NOT_EMPTY)
	CASE(ERROR_MEDIA_UNAVAILABLE)
	CASE(ERROR_RESOURCE_DISABLED)
	CASE(ERROR_INVALID_CLEANER)
	CASE(ERROR_UNABLE_TO_CLEAN)
	CASE(ERROR_OBJECT_NOT_FOUND)
	CASE(ERROR_DATABASE_FAILURE)
	CASE(ERROR_DATABASE_FULL)
	CASE(ERROR_MEDIA_INCOMPATIBLE)
	CASE(ERROR_RESOURCE_NOT_PRESENT)
	CASE(ERROR_INVALID_OPERATION)
	CASE(ERROR_MEDIA_NOT_AVAILABLE)
	CASE(ERROR_DEVICE_NOT_AVAILABLE)
	CASE(ERROR_REQUEST_REFUSED)
	CASE(ERROR_INVALID_DRIVE_OBJECT)
	CASE(ERROR_LIBRARY_FULL)
	CASE(ERROR_MEDIUM_NOT_ACCESSIBLE)
	CASE(ERROR_UNABLE_TO_LOAD_MEDIUM)
	CASE(ERROR_UNABLE_TO_INVENTORY_DRIVE)
	CASE(ERROR_UNABLE_TO_INVENTORY_SLOT)
	CASE(ERROR_UNABLE_TO_INVENTORY_TRANSPORT)
	CASE(ERROR_TRANSPORT_FULL)
	CASE(ERROR_CONTROLLING_IEPORT)
	CASE(ERROR_UNABLE_TO_EJECT_MOUNTED_MEDIA)
	CASE(ERROR_CLEANER_SLOT_SET)
	CASE(ERROR_CLEANER_SLOT_NOT_SET)
	CASE(ERROR_CLEANER_CARTRIDGE_SPENT)
	CASE(ERROR_UNEXPECTED_OMID)
	CASE(ERROR_CANT_DELETE_LAST_CASE)
	CASE(ERROR_MESSAGE_EXCEEDS_MAX_SIZE)
	CASE(ERROR_VOLUME_CONTAINS_SYS_FILES)
	CASE(ERROR_INDIGENOUS_TYPE)
	CASE(ERROR_NO_SUPPORTING_DRIVES)
	CASE(ERROR_CLEANER_CARTRIDGE_INSTALLED)
	CASE(ERROR_IEPORT_FULL)
	/* NT Remote Storage Service Error Codes */
	CASE(ERROR_FILE_OFFLINE)
	CASE(ERROR_REMOTE_STORAGE_NOT_ACTIVE)
	CASE(ERROR_REMOTE_STORAGE_MEDIA_ERROR)
	/* NT Reparse Points Error Codes */
	CASE(ERROR_NOT_A_REPARSE_POINT)
	CASE(ERROR_REPARSE_ATTRIBUTE_CONFLICT)
	CASE(ERROR_INVALID_REPARSE_DATA)
	CASE(ERROR_REPARSE_TAG_INVALID)
	CASE(ERROR_REPARSE_TAG_MISMATCH)
	/* NT Single Instance Store Error Codes */
	CASE(ERROR_VOLUME_NOT_SIS_ENABLED)
	/* Cluster Error Codes */
	CASE(ERROR_DEPENDENT_RESOURCE_EXISTS)
	CASE(ERROR_DEPENDENCY_NOT_FOUND)
	CASE(ERROR_DEPENDENCY_ALREADY_EXISTS)
	CASE(ERROR_RESOURCE_NOT_ONLINE)
	CASE(ERROR_HOST_NODE_NOT_AVAILABLE)
	CASE(ERROR_RESOURCE_NOT_AVAILABLE)
	CASE(ERROR_RESOURCE_NOT_FOUND)
	CASE(ERROR_SHUTDOWN_CLUSTER)
	CASE(ERROR_CANT_EVICT_ACTIVE_NODE)
	CASE(ERROR_OBJECT_ALREADY_EXISTS)
	CASE(ERROR_OBJECT_IN_LIST)
	CASE(ERROR_GROUP_NOT_AVAILABLE)
	CASE(ERROR_GROUP_NOT_FOUND)
	CASE(ERROR_GROUP_NOT_ONLINE)
	CASE(ERROR_HOST_NODE_NOT_RESOURCE_OWNER)
	CASE(ERROR_HOST_NODE_NOT_GROUP_OWNER)
	CASE(ERROR_RESMON_CREATE_FAILED)
	CASE(ERROR_RESMON_ONLINE_FAILED)
	CASE(ERROR_RESOURCE_ONLINE)
	CASE(ERROR_QUORUM_RESOURCE)
	CASE(ERROR_NOT_QUORUM_CAPABLE)
	CASE(ERROR_CLUSTER_SHUTTING_DOWN)
	CASE(ERROR_INVALID_STATE)
	CASE(ERROR_RESOURCE_PROPERTIES_STORED)
	CASE(ERROR_NOT_QUORUM_CLASS)
	CASE(ERROR_CORE_RESOURCE)
	CASE(ERROR_QUORUM_RESOURCE_ONLINE_FAILED)
	CASE(ERROR_QUORUMLOG_OPEN_FAILED)
	CASE(ERROR_CLUSTERLOG_CORRUPT)
	CASE(ERROR_CLUSTERLOG_RECORD_EXCEEDS_MAXSIZE)
	CASE(ERROR_CLUSTERLOG_EXCEEDS_MAXSIZE)
	CASE(ERROR_CLUSTERLOG_CHKPOINT_NOT_FOUND)
	CASE(ERROR_CLUSTERLOG_NOT_ENOUGH_SPACE)
	CASE(ERROR_QUORUM_OWNER_ALIVE)
	CASE(ERROR_NETWORK_NOT_AVAILABLE)
	CASE(ERROR_NODE_NOT_AVAILABLE)
	CASE(ERROR_ALL_NODES_NOT_AVAILABLE)
	CASE(ERROR_RESOURCE_FAILED)
	CASE(ERROR_CLUSTER_INVALID_NODE)
	CASE(ERROR_CLUSTER_NODE_EXISTS)
	CASE(ERROR_CLUSTER_JOIN_IN_PROGRESS)
	CASE(ERROR_CLUSTER_NODE_NOT_FOUND)
	CASE(ERROR_CLUSTER_LOCAL_NODE_NOT_FOUND)
	CASE(ERROR_CLUSTER_NETWORK_EXISTS)
	CASE(ERROR_CLUSTER_NETWORK_NOT_FOUND)
	CASE(ERROR_CLUSTER_NETINTERFACE_EXISTS)
	CASE(ERROR_CLUSTER_NETINTERFACE_NOT_FOUND)
	CASE(ERROR_CLUSTER_INVALID_REQUEST)
	CASE(ERROR_CLUSTER_INVALID_NETWORK_PROVIDER)
	CASE(ERROR_CLUSTER_NODE_DOWN)
	CASE(ERROR_CLUSTER_NODE_UNREACHABLE)
	CASE(ERROR_CLUSTER_NODE_NOT_MEMBER)
	CASE(ERROR_CLUSTER_JOIN_NOT_IN_PROGRESS)
	CASE(ERROR_CLUSTER_INVALID_NETWORK)
	CASE(ERROR_CLUSTER_NODE_UP)
	CASE(ERROR_CLUSTER_IPADDR_IN_USE)
	CASE(ERROR_CLUSTER_NODE_NOT_PAUSED)
	CASE(ERROR_CLUSTER_NO_SECURITY_CONTEXT)
	CASE(ERROR_CLUSTER_NETWORK_NOT_INTERNAL)
	CASE(ERROR_CLUSTER_NODE_ALREADY_UP)
	CASE(ERROR_CLUSTER_NODE_ALREADY_DOWN)
	CASE(ERROR_CLUSTER_NETWORK_ALREADY_ONLINE)
	CASE(ERROR_CLUSTER_NETWORK_ALREADY_OFFLINE)
	CASE(ERROR_CLUSTER_NODE_ALREADY_MEMBER)
	CASE(ERROR_CLUSTER_LAST_INTERNAL_NETWORK)
	CASE(ERROR_CLUSTER_NETWORK_HAS_DEPENDENTS)
	CASE(ERROR_INVALID_OPERATION_ON_QUORUM)
	CASE(ERROR_DEPENDENCY_NOT_ALLOWED)
	CASE(ERROR_CLUSTER_NODE_PAUSED)
	CASE(ERROR_NODE_CANT_HOST_RESOURCE)
	CASE(ERROR_CLUSTER_NODE_NOT_READY)
	CASE(ERROR_CLUSTER_NODE_SHUTTING_DOWN)
	CASE(ERROR_CLUSTER_JOIN_ABORTED)
	CASE(ERROR_CLUSTER_INCOMPATIBLE_VERSIONS)
	CASE(ERROR_CLUSTER_MAXNUM_OF_RESOURCES_EXCEEDED)
	CASE(ERROR_CLUSTER_SYSTEM_CONFIG_CHANGED)
	CASE(ERROR_CLUSTER_RESOURCE_TYPE_NOT_FOUND)
	CASE(ERROR_CLUSTER_RESTYPE_NOT_SUPPORTED)
	CASE(ERROR_CLUSTER_RESNAME_NOT_FOUND)
	CASE(ERROR_CLUSTER_NO_RPC_PACKAGES_REGISTERED)
	CASE(ERROR_CLUSTER_OWNER_NOT_IN_PREFLIST)
	CASE(ERROR_CLUSTER_DATABASE_SEQMISMATCH)
	CASE(ERROR_RESMON_INVALID_STATE)
	CASE(ERROR_CLUSTER_GUM_NOT_LOCKER)
	CASE(ERROR_QUORUM_DISK_NOT_FOUND)
	CASE(ERROR_DATABASE_BACKUP_CORRUPT)
	CASE(ERROR_CLUSTER_NODE_ALREADY_HAS_DFS_ROOT)
	CASE(ERROR_RESOURCE_PROPERTY_UNCHANGEABLE)
	CASE(ERROR_CLUSTER_MEMBERSHIP_INVALID_STATE)
	CASE(ERROR_CLUSTER_QUORUMLOG_NOT_FOUND)
	CASE(ERROR_CLUSTER_MEMBERSHIP_HALT)
	CASE(ERROR_CLUSTER_INSTANCE_ID_MISMATCH)
	CASE(ERROR_CLUSTER_NETWORK_NOT_FOUND_FOR_IP)
	CASE(ERROR_CLUSTER_PROPERTY_DATA_TYPE_MISMATCH)
	CASE(ERROR_CLUSTER_EVICT_WITHOUT_CLEANUP)
	CASE(ERROR_CLUSTER_PARAMETER_MISMATCH)
	CASE(ERROR_NODE_CANNOT_BE_CLUSTERED)
	CASE(ERROR_CLUSTER_WRONG_OS_VERSION)
	CASE(ERROR_CLUSTER_CANT_CREATE_DUP_CLUSTER_NAME)
	CASE(ERROR_CLUSCFG_ALREADY_COMMITTED)
	CASE(ERROR_CLUSCFG_ROLLBACK_FAILED)
	CASE(ERROR_CLUSCFG_SYSTEM_DISK_DRIVE_LETTER_CONFLICT)
	CASE(ERROR_CLUSTER_OLD_VERSION)
	CASE(ERROR_CLUSTER_MISMATCHED_COMPUTER_ACCT_NAME)
	/* EFS Error Codes */
	CASE(ERROR_ENCRYPTION_FAILED)
	CASE(ERROR_DECRYPTION_FAILED)
	CASE(ERROR_FILE_ENCRYPTED)
	CASE(ERROR_NO_RECOVERY_POLICY)
	CASE(ERROR_NO_EFS)
	CASE(ERROR_WRONG_EFS)
	CASE(ERROR_NO_USER_KEYS)
	CASE(ERROR_FILE_NOT_ENCRYPTED)
	CASE(ERROR_NOT_EXPORT_FORMAT)
	CASE(ERROR_FILE_READ_ONLY)
	CASE(ERROR_DIR_EFS_DISALLOWED)
	CASE(ERROR_EFS_SERVER_NOT_TRUSTED)
	CASE(ERROR_BAD_RECOVERY_POLICY)
	CASE(ERROR_EFS_ALG_BLOB_TOO_BIG)
	CASE(ERROR_VOLUME_NOT_SUPPORT_EFS)
	CASE(ERROR_EFS_DISABLED)
	CASE(ERROR_EFS_VERSION_NOT_SUPPORT)
	CASE(ERROR_NO_BROWSER_SERVERS_FOUND)
	/* Task Scheduler Error Codes that NET START must understand */
	CASE(SCHED_E_SERVICE_NOT_LOCALSYSTEM)
	/* Terminal Server Error Codes */
	CASE(ERROR_CTX_WINSTATION_NAME_INVALID)
	CASE(ERROR_CTX_INVALID_PD)
	CASE(ERROR_CTX_PD_NOT_FOUND)
	CASE(ERROR_CTX_WD_NOT_FOUND)
	CASE(ERROR_CTX_CANNOT_MAKE_EVENTLOG_ENTRY)
	CASE(ERROR_CTX_SERVICE_NAME_COLLISION)
	CASE(ERROR_CTX_CLOSE_PENDING)
	CASE(ERROR_CTX_NO_OUTBUF)
	CASE(ERROR_CTX_MODEM_INF_NOT_FOUND)
	CASE(ERROR_CTX_INVALID_MODEMNAME)
	CASE(ERROR_CTX_MODEM_RESPONSE_ERROR)
	CASE(ERROR_CTX_MODEM_RESPONSE_TIMEOUT)
	CASE(ERROR_CTX_MODEM_RESPONSE_NO_CARRIER)
	CASE(ERROR_CTX_MODEM_RESPONSE_NO_DIALTONE)
	CASE(ERROR_CTX_MODEM_RESPONSE_BUSY)
	CASE(ERROR_CTX_MODEM_RESPONSE_VOICE)
	CASE(ERROR_CTX_TD_ERROR)
	CASE(ERROR_CTX_WINSTATION_NOT_FOUND)
	CASE(ERROR_CTX_WINSTATION_ALREADY_EXISTS)
	CASE(ERROR_CTX_WINSTATION_BUSY)
	CASE(ERROR_CTX_BAD_VIDEO_MODE)
	CASE(ERROR_CTX_GRAPHICS_INVALID)
	CASE(ERROR_CTX_LOGON_DISABLED)
	CASE(ERROR_CTX_NOT_CONSOLE)
	CASE(ERROR_CTX_CLIENT_QUERY_TIMEOUT)
	CASE(ERROR_CTX_CONSOLE_DISCONNECT)
	CASE(ERROR_CTX_CONSOLE_CONNECT)
	CASE(ERROR_CTX_SHADOW_DENIED)
	CASE(ERROR_CTX_WINSTATION_ACCESS_DENIED)
	CASE(ERROR_CTX_INVALID_WD)
	CASE(ERROR_CTX_SHADOW_INVALID)
	CASE(ERROR_CTX_SHADOW_DISABLED)
	CASE(ERROR_CTX_CLIENT_LICENSE_IN_USE)
	CASE(ERROR_CTX_CLIENT_LICENSE_NOT_SET)
	CASE(ERROR_CTX_LICENSE_NOT_AVAILABLE)
	CASE(ERROR_CTX_LICENSE_CLIENT_INVALID)
	CASE(ERROR_CTX_LICENSE_EXPIRED)
	CASE(ERROR_CTX_SHADOW_NOT_RUNNING)
	CASE(ERROR_CTX_SHADOW_ENDED_BY_MODE_CHANGE)
	CASE(ERROR_ACTIVATION_COUNT_EXCEEDED)
	/* Traffic Control Error Codes,  defined in: tcerror.h */
	/* Active Directory Error Codes */
	CASE(FRS_ERR_INVALID_API_SEQUENCE)
	CASE(FRS_ERR_STARTING_SERVICE)
	CASE(FRS_ERR_STOPPING_SERVICE)
	CASE(FRS_ERR_INTERNAL_API)
	CASE(FRS_ERR_INTERNAL)
	CASE(FRS_ERR_SERVICE_COMM)
	CASE(FRS_ERR_INSUFFICIENT_PRIV)
	CASE(FRS_ERR_AUTHENTICATION)
	CASE(FRS_ERR_PARENT_INSUFFICIENT_PRIV)
	CASE(FRS_ERR_PARENT_AUTHENTICATION)
	CASE(FRS_ERR_CHILD_TO_PARENT_COMM)
	CASE(FRS_ERR_PARENT_TO_CHILD_COMM)
	CASE(FRS_ERR_SYSVOL_POPULATE)
	CASE(FRS_ERR_SYSVOL_POPULATE_TIMEOUT)
	CASE(FRS_ERR_SYSVOL_IS_BUSY)
	CASE(FRS_ERR_SYSVOL_DEMOTE)
	CASE(FRS_ERR_INVALID_SERVICE_PARAMETER)
	CASE(ERROR_DS_NOT_INSTALLED)
	CASE(ERROR_DS_MEMBERSHIP_EVALUATED_LOCALLY)
	CASE(ERROR_DS_NO_ATTRIBUTE_OR_VALUE)
	CASE(ERROR_DS_INVALID_ATTRIBUTE_SYNTAX)
	CASE(ERROR_DS_ATTRIBUTE_TYPE_UNDEFINED)
	CASE(ERROR_DS_ATTRIBUTE_OR_VALUE_EXISTS)
	CASE(ERROR_DS_BUSY)
	CASE(ERROR_DS_UNAVAILABLE)
	CASE(ERROR_DS_NO_RIDS_ALLOCATED)
	CASE(ERROR_DS_NO_MORE_RIDS)
	CASE(ERROR_DS_INCORRECT_ROLE_OWNER)
	CASE(ERROR_DS_RIDMGR_INIT_ERROR)
	CASE(ERROR_DS_OBJ_CLASS_VIOLATION)
	CASE(ERROR_DS_CANT_ON_NON_LEAF)
	CASE(ERROR_DS_CANT_ON_RDN)
	CASE(ERROR_DS_CANT_MOD_OBJ_CLASS)
	CASE(ERROR_DS_CROSS_DOM_MOVE_ERROR)
	CASE(ERROR_DS_GC_NOT_AVAILABLE)
	CASE(ERROR_SHARED_POLICY)
	CASE(ERROR_POLICY_OBJECT_NOT_FOUND)
	CASE(ERROR_POLICY_ONLY_IN_DS)
	CASE(ERROR_PROMOTION_ACTIVE)
	CASE(ERROR_NO_PROMOTION_ACTIVE)
	CASE(ERROR_DS_OPERATIONS_ERROR)
	CASE(ERROR_DS_PROTOCOL_ERROR)
	CASE(ERROR_DS_TIMELIMIT_EXCEEDED)
	CASE(ERROR_DS_SIZELIMIT_EXCEEDED)
	CASE(ERROR_DS_ADMIN_LIMIT_EXCEEDED)
	CASE(ERROR_DS_COMPARE_FALSE)
	CASE(ERROR_DS_COMPARE_TRUE)
	CASE(ERROR_DS_AUTH_METHOD_NOT_SUPPORTED)
	CASE(ERROR_DS_STRONG_AUTH_REQUIRED)
	CASE(ERROR_DS_INAPPROPRIATE_AUTH)
	CASE(ERROR_DS_AUTH_UNKNOWN)
	CASE(ERROR_DS_REFERRAL)
	CASE(ERROR_DS_UNAVAILABLE_CRIT_EXTENSION)
	CASE(ERROR_DS_CONFIDENTIALITY_REQUIRED)
	CASE(ERROR_DS_INAPPROPRIATE_MATCHING)
	CASE(ERROR_DS_CONSTRAINT_VIOLATION)
	CASE(ERROR_DS_NO_SUCH_OBJECT)
	CASE(ERROR_DS_ALIAS_PROBLEM)
	CASE(ERROR_DS_INVALID_DN_SYNTAX)
	CASE(ERROR_DS_IS_LEAF)
	CASE(ERROR_DS_ALIAS_DEREF_PROBLEM)
	CASE(ERROR_DS_UNWILLING_TO_PERFORM)
	CASE(ERROR_DS_LOOP_DETECT)
	CASE(ERROR_DS_NAMING_VIOLATION)
	CASE(ERROR_DS_OBJECT_RESULTS_TOO_LARGE)
	CASE(ERROR_DS_AFFECTS_MULTIPLE_DSAS)
	CASE(ERROR_DS_SERVER_DOWN)
	CASE(ERROR_DS_LOCAL_ERROR)
	CASE(ERROR_DS_ENCODING_ERROR)
	CASE(ERROR_DS_DECODING_ERROR)
	CASE(ERROR_DS_FILTER_UNKNOWN)
	CASE(ERROR_DS_PARAM_ERROR)
	CASE(ERROR_DS_NOT_SUPPORTED)
	CASE(ERROR_DS_NO_RESULTS_RETURNED)
	CASE(ERROR_DS_CONTROL_NOT_FOUND)
	CASE(ERROR_DS_CLIENT_LOOP)
	CASE(ERROR_DS_REFERRAL_LIMIT_EXCEEDED)
	CASE(ERROR_DS_SORT_CONTROL_MISSING)
	CASE(ERROR_DS_OFFSET_RANGE_ERROR)
	CASE(ERROR_DS_ROOT_MUST_BE_NC)
	CASE(ERROR_DS_ADD_REPLICA_INHIBITED)
	CASE(ERROR_DS_ATT_NOT_DEF_IN_SCHEMA)
	CASE(ERROR_DS_MAX_OBJ_SIZE_EXCEEDED)
	CASE(ERROR_DS_OBJ_STRING_NAME_EXISTS)
	CASE(ERROR_DS_NO_RDN_DEFINED_IN_SCHEMA)
	CASE(ERROR_DS_RDN_DOESNT_MATCH_SCHEMA)
	CASE(ERROR_DS_NO_REQUESTED_ATTS_FOUND)
	CASE(ERROR_DS_USER_BUFFER_TO_SMALL)
	CASE(ERROR_DS_ATT_IS_NOT_ON_OBJ)
	CASE(ERROR_DS_ILLEGAL_MOD_OPERATION)
	CASE(ERROR_DS_OBJ_TOO_LARGE)
	CASE(ERROR_DS_BAD_INSTANCE_TYPE)
	CASE(ERROR_DS_MASTERDSA_REQUIRED)
	CASE(ERROR_DS_OBJECT_CLASS_REQUIRED)
	CASE(ERROR_DS_MISSING_REQUIRED_ATT)
	CASE(ERROR_DS_ATT_NOT_DEF_FOR_CLASS)
	CASE(ERROR_DS_ATT_ALREADY_EXISTS)
	CASE(ERROR_DS_CANT_ADD_ATT_VALUES)
	CASE(ERROR_DS_SINGLE_VALUE_CONSTRAINT)
	CASE(ERROR_DS_RANGE_CONSTRAINT)
	CASE(ERROR_DS_ATT_VAL_ALREADY_EXISTS)
	CASE(ERROR_DS_CANT_REM_MISSING_ATT)
	CASE(ERROR_DS_CANT_REM_MISSING_ATT_VAL)
	CASE(ERROR_DS_ROOT_CANT_BE_SUBREF)
	CASE(ERROR_DS_NO_CHAINING)
	CASE(ERROR_DS_NO_CHAINED_EVAL)
	CASE(ERROR_DS_NO_PARENT_OBJECT)
	CASE(ERROR_DS_PARENT_IS_AN_ALIAS)
	CASE(ERROR_DS_CANT_MIX_MASTER_AND_REPS)
	CASE(ERROR_DS_CHILDREN_EXIST)
	CASE(ERROR_DS_OBJ_NOT_FOUND)
	CASE(ERROR_DS_ALIASED_OBJ_MISSING)
	CASE(ERROR_DS_BAD_NAME_SYNTAX)
	CASE(ERROR_DS_ALIAS_POINTS_TO_ALIAS)
	CASE(ERROR_DS_CANT_DEREF_ALIAS)
	CASE(ERROR_DS_OUT_OF_SCOPE)
	CASE(ERROR_DS_OBJECT_BEING_REMOVED)
	CASE(ERROR_DS_CANT_DELETE_DSA_OBJ)
	CASE(ERROR_DS_GENERIC_ERROR)
	CASE(ERROR_DS_DSA_MUST_BE_INT_MASTER)
	CASE(ERROR_DS_CLASS_NOT_DSA)
	CASE(ERROR_DS_INSUFF_ACCESS_RIGHTS)
	CASE(ERROR_DS_ILLEGAL_SUPERIOR)
	CASE(ERROR_DS_ATTRIBUTE_OWNED_BY_SAM)
	CASE(ERROR_DS_NAME_TOO_MANY_PARTS)
	CASE(ERROR_DS_NAME_TOO_LONG)
	CASE(ERROR_DS_NAME_VALUE_TOO_LONG)
	CASE(ERROR_DS_NAME_UNPARSEABLE)
	CASE(ERROR_DS_NAME_TYPE_UNKNOWN)
	CASE(ERROR_DS_NOT_AN_OBJECT)
	CASE(ERROR_DS_SEC_DESC_TOO_SHORT)
	CASE(ERROR_DS_SEC_DESC_INVALID)
	CASE(ERROR_DS_NO_DELETED_NAME)
	CASE(ERROR_DS_SUBREF_MUST_HAVE_PARENT)
	CASE(ERROR_DS_NCNAME_MUST_BE_NC)
	CASE(ERROR_DS_CANT_ADD_SYSTEM_ONLY)
	CASE(ERROR_DS_CLASS_MUST_BE_CONCRETE)
	CASE(ERROR_DS_INVALID_DMD)
	CASE(ERROR_DS_OBJ_GUID_EXISTS)
	CASE(ERROR_DS_NOT_ON_BACKLINK)
	CASE(ERROR_DS_NO_CROSSREF_FOR_NC)
	CASE(ERROR_DS_SHUTTING_DOWN)
	CASE(ERROR_DS_UNKNOWN_OPERATION)
	CASE(ERROR_DS_INVALID_ROLE_OWNER)
	CASE(ERROR_DS_COULDNT_CONTACT_FSMO)
	CASE(ERROR_DS_CROSS_NC_DN_RENAME)
	CASE(ERROR_DS_CANT_MOD_SYSTEM_ONLY)
	CASE(ERROR_DS_REPLICATOR_ONLY)
	CASE(ERROR_DS_OBJ_CLASS_NOT_DEFINED)
	CASE(ERROR_DS_OBJ_CLASS_NOT_SUBCLASS)
	CASE(ERROR_DS_NAME_REFERENCE_INVALID)
	CASE(ERROR_DS_CROSS_REF_EXISTS)
	CASE(ERROR_DS_CANT_DEL_MASTER_CROSSREF)
	CASE(ERROR_DS_SUBTREE_NOTIFY_NOT_NC_HEAD)
	CASE(ERROR_DS_NOTIFY_FILTER_TOO_COMPLEX)
	CASE(ERROR_DS_DUP_RDN)
	CASE(ERROR_DS_DUP_OID)
	CASE(ERROR_DS_DUP_MAPI_ID)
	CASE(ERROR_DS_DUP_SCHEMA_ID_GUID)
	CASE(ERROR_DS_DUP_LDAP_DISPLAY_NAME)
	CASE(ERROR_DS_SEMANTIC_ATT_TEST)
	CASE(ERROR_DS_SYNTAX_MISMATCH)
	CASE(ERROR_DS_EXISTS_IN_MUST_HAVE)
	CASE(ERROR_DS_EXISTS_IN_MAY_HAVE)
	CASE(ERROR_DS_NONEXISTENT_MAY_HAVE)
	CASE(ERROR_DS_NONEXISTENT_MUST_HAVE)
	CASE(ERROR_DS_AUX_CLS_TEST_FAIL)
	CASE(ERROR_DS_NONEXISTENT_POSS_SUP)
	CASE(ERROR_DS_SUB_CLS_TEST_FAIL)
	CASE(ERROR_DS_BAD_RDN_ATT_ID_SYNTAX)
	CASE(ERROR_DS_EXISTS_IN_AUX_CLS)
	CASE(ERROR_DS_EXISTS_IN_SUB_CLS)
	CASE(ERROR_DS_EXISTS_IN_POSS_SUP)
	CASE(ERROR_DS_RECALCSCHEMA_FAILED)
	CASE(ERROR_DS_TREE_DELETE_NOT_FINISHED)
	CASE(ERROR_DS_CANT_DELETE)
	CASE(ERROR_DS_ATT_SCHEMA_REQ_ID)
	CASE(ERROR_DS_BAD_ATT_SCHEMA_SYNTAX)
	CASE(ERROR_DS_CANT_CACHE_ATT)
	CASE(ERROR_DS_CANT_CACHE_CLASS)
	CASE(ERROR_DS_CANT_REMOVE_ATT_CACHE)
	CASE(ERROR_DS_CANT_REMOVE_CLASS_CACHE)
	CASE(ERROR_DS_CANT_RETRIEVE_DN)
	CASE(ERROR_DS_MISSING_SUPREF)
	CASE(ERROR_DS_CANT_RETRIEVE_INSTANCE)
	CASE(ERROR_DS_CODE_INCONSISTENCY)
	CASE(ERROR_DS_DATABASE_ERROR)
	CASE(ERROR_DS_GOVERNSID_MISSING)
	CASE(ERROR_DS_MISSING_EXPECTED_ATT)
	CASE(ERROR_DS_NCNAME_MISSING_CR_REF)
	CASE(ERROR_DS_SECURITY_CHECKING_ERROR)
	CASE(ERROR_DS_SCHEMA_NOT_LOADED)
	CASE(ERROR_DS_SCHEMA_ALLOC_FAILED)
	CASE(ERROR_DS_ATT_SCHEMA_REQ_SYNTAX)
	CASE(ERROR_DS_GCVERIFY_ERROR)
	CASE(ERROR_DS_DRA_SCHEMA_MISMATCH)
	CASE(ERROR_DS_CANT_FIND_DSA_OBJ)
	CASE(ERROR_DS_CANT_FIND_EXPECTED_NC)
	CASE(ERROR_DS_CANT_FIND_NC_IN_CACHE)
	CASE(ERROR_DS_CANT_RETRIEVE_CHILD)
	CASE(ERROR_DS_SECURITY_ILLEGAL_MODIFY)
	CASE(ERROR_DS_CANT_REPLACE_HIDDEN_REC)
	CASE(ERROR_DS_BAD_HIERARCHY_FILE)
	CASE(ERROR_DS_BUILD_HIERARCHY_TABLE_FAILED)
	CASE(ERROR_DS_CONFIG_PARAM_MISSING)
	CASE(ERROR_DS_COUNTING_AB_INDICES_FAILED)
	CASE(ERROR_DS_HIERARCHY_TABLE_MALLOC_FAILED)
	CASE(ERROR_DS_INTERNAL_FAILURE)
	CASE(ERROR_DS_UNKNOWN_ERROR)
	CASE(ERROR_DS_ROOT_REQUIRES_CLASS_TOP)
	CASE(ERROR_DS_REFUSING_FSMO_ROLES)
	CASE(ERROR_DS_MISSING_FSMO_SETTINGS)
	CASE(ERROR_DS_UNABLE_TO_SURRENDER_ROLES)
	CASE(ERROR_DS_DRA_GENERIC)
	CASE(ERROR_DS_DRA_INVALID_PARAMETER)
	CASE(ERROR_DS_DRA_BUSY)
	CASE(ERROR_DS_DRA_BAD_DN)
	CASE(ERROR_DS_DRA_BAD_NC)
	CASE(ERROR_DS_DRA_DN_EXISTS)
	CASE(ERROR_DS_DRA_INTERNAL_ERROR)
	CASE(ERROR_DS_DRA_INCONSISTENT_DIT)
	CASE(ERROR_DS_DRA_CONNECTION_FAILED)
	CASE(ERROR_DS_DRA_BAD_INSTANCE_TYPE)
	CASE(ERROR_DS_DRA_OUT_OF_MEM)
	CASE(ERROR_DS_DRA_MAIL_PROBLEM)
	CASE(ERROR_DS_DRA_REF_ALREADY_EXISTS)
	CASE(ERROR_DS_DRA_REF_NOT_FOUND)
	CASE(ERROR_DS_DRA_OBJ_IS_REP_SOURCE)
	CASE(ERROR_DS_DRA_DB_ERROR)
	CASE(ERROR_DS_DRA_NO_REPLICA)
	CASE(ERROR_DS_DRA_ACCESS_DENIED)
	CASE(ERROR_DS_DRA_NOT_SUPPORTED)
	CASE(ERROR_DS_DRA_RPC_CANCELLED)
	CASE(ERROR_DS_DRA_SOURCE_DISABLED)
	CASE(ERROR_DS_DRA_SINK_DISABLED)
	CASE(ERROR_DS_DRA_NAME_COLLISION)
	CASE(ERROR_DS_DRA_SOURCE_REINSTALLED)
	CASE(ERROR_DS_DRA_MISSING_PARENT)
	CASE(ERROR_DS_DRA_PREEMPTED)
	CASE(ERROR_DS_DRA_ABANDON_SYNC)
	CASE(ERROR_DS_DRA_SHUTDOWN)
	CASE(ERROR_DS_DRA_INCOMPATIBLE_PARTIAL_SET)
	CASE(ERROR_DS_DRA_SOURCE_IS_PARTIAL_REPLICA)
	CASE(ERROR_DS_DRA_EXTN_CONNECTION_FAILED)
	CASE(ERROR_DS_INSTALL_SCHEMA_MISMATCH)
	CASE(ERROR_DS_DUP_LINK_ID)
	CASE(ERROR_DS_NAME_ERROR_RESOLVING)
	CASE(ERROR_DS_NAME_ERROR_NOT_FOUND)
	CASE(ERROR_DS_NAME_ERROR_NOT_UNIQUE)
	CASE(ERROR_DS_NAME_ERROR_NO_MAPPING)
	CASE(ERROR_DS_NAME_ERROR_DOMAIN_ONLY)
	CASE(ERROR_DS_NAME_ERROR_NO_SYNTACTICAL_MAPPING)
	CASE(ERROR_DS_CONSTRUCTED_ATT_MOD)
	CASE(ERROR_DS_WRONG_OM_OBJ_CLASS)
	CASE(ERROR_DS_DRA_REPL_PENDING)
	CASE(ERROR_DS_DS_REQUIRED)
	CASE(ERROR_DS_INVALID_LDAP_DISPLAY_NAME)
	CASE(ERROR_DS_NON_BASE_SEARCH)
	CASE(ERROR_DS_CANT_RETRIEVE_ATTS)
	CASE(ERROR_DS_BACKLINK_WITHOUT_LINK)
	CASE(ERROR_DS_EPOCH_MISMATCH)
	CASE(ERROR_DS_SRC_NAME_MISMATCH)
	CASE(ERROR_DS_SRC_AND_DST_NC_IDENTICAL)
	CASE(ERROR_DS_DST_NC_MISMATCH)
	CASE(ERROR_DS_NOT_AUTHORITIVE_FOR_DST_NC)
	CASE(ERROR_DS_SRC_GUID_MISMATCH)
	CASE(ERROR_DS_CANT_MOVE_DELETED_OBJECT)
	CASE(ERROR_DS_PDC_OPERATION_IN_PROGRESS)
	CASE(ERROR_DS_CROSS_DOMAIN_CLEANUP_REQD)
	CASE(ERROR_DS_ILLEGAL_XDOM_MOVE_OPERATION)
	CASE(ERROR_DS_CANT_WITH_ACCT_GROUP_MEMBERSHPS)
	CASE(ERROR_DS_NC_MUST_HAVE_NC_PARENT)
	CASE(ERROR_DS_CR_IMPOSSIBLE_TO_VALIDATE)
	CASE(ERROR_DS_DST_DOMAIN_NOT_NATIVE)
	CASE(ERROR_DS_MISSING_INFRASTRUCTURE_CONTAINER)
	CASE(ERROR_DS_CANT_MOVE_ACCOUNT_GROUP)
	CASE(ERROR_DS_CANT_MOVE_RESOURCE_GROUP)
	CASE(ERROR_DS_INVALID_SEARCH_FLAG)
	CASE(ERROR_DS_NO_TREE_DELETE_ABOVE_NC)
	CASE(ERROR_DS_COULDNT_LOCK_TREE_FOR_DELETE)
	CASE(ERROR_DS_COULDNT_IDENTIFY_OBJECTS_FOR_TREE_DELETE)
	CASE(ERROR_DS_SAM_INIT_FAILURE)
	CASE(ERROR_DS_SENSITIVE_GROUP_VIOLATION)
	CASE(ERROR_DS_CANT_MOD_PRIMARYGROUPID)
	CASE(ERROR_DS_ILLEGAL_BASE_SCHEMA_MOD)
	CASE(ERROR_DS_NONSAFE_SCHEMA_CHANGE)
	CASE(ERROR_DS_SCHEMA_UPDATE_DISALLOWED)
	CASE(ERROR_DS_CANT_CREATE_UNDER_SCHEMA)
	CASE(ERROR_DS_INSTALL_NO_SRC_SCH_VERSION)
	CASE(ERROR_DS_INSTALL_NO_SCH_VERSION_IN_INIFILE)
	CASE(ERROR_DS_INVALID_GROUP_TYPE)
	CASE(ERROR_DS_NO_NEST_GLOBALGROUP_IN_MIXEDDOMAIN)
	CASE(ERROR_DS_NO_NEST_LOCALGROUP_IN_MIXEDDOMAIN)
	CASE(ERROR_DS_GLOBAL_CANT_HAVE_LOCAL_MEMBER)
	CASE(ERROR_DS_GLOBAL_CANT_HAVE_UNIVERSAL_MEMBER)
	CASE(ERROR_DS_UNIVERSAL_CANT_HAVE_LOCAL_MEMBER)
	CASE(ERROR_DS_GLOBAL_CANT_HAVE_CROSSDOMAIN_MEMBER)
	CASE(ERROR_DS_LOCAL_CANT_HAVE_CROSSDOMAIN_LOCAL_MEMBER)
	CASE(ERROR_DS_HAVE_PRIMARY_MEMBERS)
	CASE(ERROR_DS_STRING_SD_CONVERSION_FAILED)
	CASE(ERROR_DS_NAMING_MASTER_GC)
	CASE(ERROR_DS_DNS_LOOKUP_FAILURE)
	CASE(ERROR_DS_COULDNT_UPDATE_SPNS)
	CASE(ERROR_DS_CANT_RETRIEVE_SD)
	CASE(ERROR_DS_KEY_NOT_UNIQUE)
	CASE(ERROR_DS_WRONG_LINKED_ATT_SYNTAX)
	CASE(ERROR_DS_SAM_NEED_BOOTKEY_PASSWORD)
	CASE(ERROR_DS_SAM_NEED_BOOTKEY_FLOPPY)
	CASE(ERROR_DS_CANT_START)
	CASE(ERROR_DS_INIT_FAILURE)
	CASE(ERROR_DS_SAM_INIT_FAILURE_CONSOLE)
	CASE(ERROR_DS_FOREST_VERSION_TOO_HIGH)
	CASE(ERROR_DS_DOMAIN_VERSION_TOO_HIGH)
	CASE(ERROR_DS_FOREST_VERSION_TOO_LOW)
	CASE(ERROR_DS_DOMAIN_VERSION_TOO_LOW)
	CASE(ERROR_DS_INCOMPATIBLE_VERSION)
	CASE(ERROR_DS_LOW_DSA_VERSION)
	CASE(ERROR_DS_NO_BEHAVIOR_VERSION_IN_MIXEDDOMAIN)
	CASE(ERROR_DS_NOT_SUPPORTED_SORT_ORDER)
	CASE(ERROR_DS_NAME_NOT_UNIQUE)
	CASE(ERROR_DS_MACHINE_ACCOUNT_CREATED_PRENT4)
	CASE(ERROR_DS_OUT_OF_VERSION_STORE)
	CASE(ERROR_DS_INCOMPATIBLE_CONTROLS_USED)
	CASE(ERROR_DS_NO_REF_DOMAIN)
	CASE(ERROR_DS_RESERVED_LINK_ID)
	CASE(ERROR_DS_LINK_ID_NOT_AVAILABLE)
	CASE(ERROR_DS_AG_CANT_HAVE_UNIVERSAL_MEMBER)
	CASE(ERROR_DS_MODIFYDN_DISALLOWED_BY_INSTANCE_TYPE)
	CASE(ERROR_DS_NO_OBJECT_MOVE_IN_SCHEMA_NC)
	CASE(ERROR_DS_MODIFYDN_DISALLOWED_BY_FLAG)
	CASE(ERROR_DS_MODIFYDN_WRONG_GRANDPARENT)
	CASE(ERROR_DS_NAME_ERROR_TRUST_REFERRAL)
	CASE(ERROR_NOT_SUPPORTED_ON_STANDARD_SERVER)
	CASE(ERROR_DS_CANT_ACCESS_REMOTE_PART_OF_AD)
	CASE(ERROR_DS_CR_IMPOSSIBLE_TO_VALIDATE_V2)
	CASE(ERROR_DS_THREAD_LIMIT_EXCEEDED)
	CASE(ERROR_DS_NOT_CLOSEST)
	CASE(ERROR_DS_CANT_DERIVE_SPN_WITHOUT_SERVER_REF)
	CASE(ERROR_DS_SINGLE_USER_MODE_FAILED)
	CASE(ERROR_DS_NTDSCRIPT_SYNTAX_ERROR)
	CASE(ERROR_DS_NTDSCRIPT_PROCESS_ERROR)
	CASE(ERROR_DS_DIFFERENT_REPL_EPOCHS)
	CASE(ERROR_DS_DRS_EXTENSIONS_CHANGED)
	CASE(ERROR_DS_REPLICA_SET_CHANGE_NOT_ALLOWED_ON_DISABLED_CR)
	CASE(ERROR_DS_NO_MSDS_INTID)
	CASE(ERROR_DS_DUP_MSDS_INTID)
	CASE(ERROR_DS_EXISTS_IN_RDNATTID)
	CASE(ERROR_DS_AUTHORIZATION_FAILED)
	CASE(ERROR_DS_INVALID_SCRIPT)
	CASE(ERROR_DS_REMOTE_CROSSREF_OP_FAILED)
	CASE(ERROR_DS_CROSS_REF_BUSY)
	CASE(ERROR_DS_CANT_DERIVE_SPN_FOR_DELETED_DOMAIN)
	CASE(ERROR_DS_CANT_DEMOTE_WITH_WRITEABLE_NC)
	CASE(ERROR_DS_DUPLICATE_ID_FOUND)
	CASE(ERROR_DS_INSUFFICIENT_ATTR_TO_CREATE_OBJECT)
	CASE(ERROR_DS_GROUP_CONVERSION_ERROR)
	CASE(ERROR_DS_CANT_MOVE_APP_BASIC_GROUP)
	CASE(ERROR_DS_CANT_MOVE_APP_QUERY_GROUP)
	CASE(ERROR_DS_ROLE_NOT_VERIFIED)
	CASE(ERROR_DS_WKO_CONTAINER_CANNOT_BE_SPECIAL)
	CASE(ERROR_DS_DOMAIN_RENAME_IN_PROGRESS)
	CASE(ERROR_DS_EXISTING_AD_CHILD_NC)
	CASE(ERROR_DS_REPL_LIFETIME_EXCEEDED)
	CASE(ERROR_DS_DISALLOWED_IN_SYSTEM_CONTAINER)
	CASE(ERROR_DS_LDAP_SEND_QUEUE_FULL)
	CASE(ERROR_DS_DRA_OUT_SCHEDULE_WINDOW)
	/* DNS Error Codes */
	CASE(DNS_ERROR_RCODE_FORMAT_ERROR)
	CASE(DNS_ERROR_RCODE_SERVER_FAILURE)
	CASE(DNS_ERROR_RCODE_NAME_ERROR)
	CASE(DNS_ERROR_RCODE_NOT_IMPLEMENTED)
	CASE(DNS_ERROR_RCODE_REFUSED)
	CASE(DNS_ERROR_RCODE_YXDOMAIN)
	CASE(DNS_ERROR_RCODE_YXRRSET)
	CASE(DNS_ERROR_RCODE_NXRRSET)
	CASE(DNS_ERROR_RCODE_NOTAUTH)
	CASE(DNS_ERROR_RCODE_NOTZONE)
	CASE(DNS_ERROR_RCODE_BADSIG)
	CASE(DNS_ERROR_RCODE_BADKEY)
	CASE(DNS_ERROR_RCODE_BADTIME)
	CASE(DNS_INFO_NO_RECORDS)
	CASE(DNS_ERROR_BAD_PACKET)
	CASE(DNS_ERROR_NO_PACKET)
	CASE(DNS_ERROR_RCODE)
	CASE(DNS_ERROR_UNSECURE_PACKET)
	CASE(DNS_ERROR_INVALID_TYPE)
	CASE(DNS_ERROR_INVALID_IP_ADDRESS)
	CASE(DNS_ERROR_INVALID_PROPERTY)
	CASE(DNS_ERROR_TRY_AGAIN_LATER)
	CASE(DNS_ERROR_NOT_UNIQUE)
	CASE(DNS_ERROR_NON_RFC_NAME)
	CASE(DNS_STATUS_FQDN)
	CASE(DNS_STATUS_DOTTED_NAME)
	CASE(DNS_STATUS_SINGLE_PART_NAME)
	CASE(DNS_ERROR_INVALID_NAME_CHAR)
	CASE(DNS_ERROR_NUMERIC_NAME)
	CASE(DNS_ERROR_NOT_ALLOWED_ON_ROOT_SERVER)
	CASE(DNS_ERROR_NOT_ALLOWED_UNDER_DELEGATION)
	CASE(DNS_ERROR_CANNOT_FIND_ROOT_HINTS)
	CASE(DNS_ERROR_INCONSISTENT_ROOT_HINTS)
	CASE(DNS_ERROR_ZONE_DOES_NOT_EXIST)
	CASE(DNS_ERROR_NO_ZONE_INFO)
	CASE(DNS_ERROR_INVALID_ZONE_OPERATION)
	CASE(DNS_ERROR_ZONE_CONFIGURATION_ERROR)
	CASE(DNS_ERROR_ZONE_HAS_NO_SOA_RECORD)
	CASE(DNS_ERROR_ZONE_HAS_NO_NS_RECORDS)
	CASE(DNS_ERROR_ZONE_LOCKED)
	CASE(DNS_ERROR_ZONE_CREATION_FAILED)
	CASE(DNS_ERROR_ZONE_ALREADY_EXISTS)
	CASE(DNS_ERROR_AUTOZONE_ALREADY_EXISTS)
	CASE(DNS_ERROR_INVALID_ZONE_TYPE)
	CASE(DNS_ERROR_SECONDARY_REQUIRES_MASTER_IP)
	CASE(DNS_ERROR_ZONE_NOT_SECONDARY)
	CASE(DNS_ERROR_NEED_SECONDARY_ADDRESSES)
	CASE(DNS_ERROR_WINS_INIT_FAILED)
	CASE(DNS_ERROR_NEED_WINS_SERVERS)
	CASE(DNS_ERROR_NBSTAT_INIT_FAILED)
	CASE(DNS_ERROR_SOA_DELETE_INVALID)
	CASE(DNS_ERROR_FORWARDER_ALREADY_EXISTS)
	CASE(DNS_ERROR_ZONE_REQUIRES_MASTER_IP)
	CASE(DNS_ERROR_ZONE_IS_SHUTDOWN)
	CASE(DNS_ERROR_PRIMARY_REQUIRES_DATAFILE)
	CASE(DNS_ERROR_INVALID_DATAFILE_NAME)
	CASE(DNS_ERROR_DATAFILE_OPEN_FAILURE)
	CASE(DNS_ERROR_FILE_WRITEBACK_FAILED)
	CASE(DNS_ERROR_DATAFILE_PARSING)
	CASE(DNS_ERROR_RECORD_DOES_NOT_EXIST)
	CASE(DNS_ERROR_RECORD_FORMAT)
	CASE(DNS_ERROR_NODE_CREATION_FAILED)
	CASE(DNS_ERROR_UNKNOWN_RECORD_TYPE)
	CASE(DNS_ERROR_RECORD_TIMED_OUT)
	CASE(DNS_ERROR_NAME_NOT_IN_ZONE)
	CASE(DNS_ERROR_CNAME_LOOP)
	CASE(DNS_ERROR_NODE_IS_CNAME)
	CASE(DNS_ERROR_CNAME_COLLISION)
	CASE(DNS_ERROR_RECORD_ONLY_AT_ZONE_ROOT)
	CASE(DNS_ERROR_RECORD_ALREADY_EXISTS)
	CASE(DNS_ERROR_SECONDARY_DATA)
	CASE(DNS_ERROR_NO_CREATE_CACHE_DATA)
	CASE(DNS_ERROR_NAME_DOES_NOT_EXIST)
	CASE(DNS_WARNING_PTR_CREATE_FAILED)
	CASE(DNS_WARNING_DOMAIN_UNDELETED)
	CASE(DNS_ERROR_DS_UNAVAILABLE)
	CASE(DNS_ERROR_DS_ZONE_ALREADY_EXISTS)
	CASE(DNS_ERROR_NO_BOOTFILE_IF_DS_ZONE)
	CASE(DNS_INFO_AXFR_COMPLETE)
	CASE(DNS_ERROR_AXFR)
	CASE(DNS_INFO_ADDED_LOCAL_WINS)
	CASE(DNS_STATUS_CONTINUE_NEEDED)
	CASE(DNS_ERROR_NO_TCPIP)
	CASE(DNS_ERROR_NO_DNS_SERVERS)
	CASE(DNS_ERROR_DP_DOES_NOT_EXIST)
	CASE(DNS_ERROR_DP_ALREADY_EXISTS)
	CASE(DNS_ERROR_DP_NOT_ENLISTED)
	CASE(DNS_ERROR_DP_ALREADY_ENLISTED)
	CASE(DNS_ERROR_DP_NOT_AVAILABLE)
	CASE(DNS_ERROR_DP_FSMO_ERROR)
	/* WinSock Error Codes */
	CASE(WSAEINTR)
	CASE(WSAEBADF)
	CASE(WSAEACCES)
	CASE(WSAEFAULT)
	CASE(WSAEINVAL)
	CASE(WSAEMFILE)
	CASE(WSAEWOULDBLOCK)
	CASE(WSAEINPROGRESS)
	CASE(WSAEALREADY)
	CASE(WSAENOTSOCK)
	CASE(WSAEDESTADDRREQ)
	CASE(WSAEMSGSIZE)
	CASE(WSAEPROTOTYPE)
	CASE(WSAENOPROTOOPT)
	CASE(WSAEPROTONOSUPPORT)
	CASE(WSAESOCKTNOSUPPORT)
	CASE(WSAEOPNOTSUPP)
	CASE(WSAEPFNOSUPPORT)
	CASE(WSAEAFNOSUPPORT)
	CASE(WSAEADDRINUSE)
	CASE(WSAEADDRNOTAVAIL)
	CASE(WSAENETDOWN)
	CASE(WSAENETUNREACH)
	CASE(WSAENETRESET)
	CASE(WSAECONNABORTED)
	CASE(WSAECONNRESET)
	CASE(WSAENOBUFS)
	CASE(WSAEISCONN)
	CASE(WSAENOTCONN)
	CASE(WSAESHUTDOWN)
	CASE(WSAETOOMANYREFS)
	CASE(WSAETIMEDOUT)
	CASE(WSAECONNREFUSED)
	CASE(WSAELOOP)
	CASE(WSAENAMETOOLONG)
	CASE(WSAEHOSTDOWN)
	CASE(WSAEHOSTUNREACH)
	CASE(WSAENOTEMPTY)
	CASE(WSAEPROCLIM)
	CASE(WSAEUSERS)
	CASE(WSAEDQUOT)
	CASE(WSAESTALE)
	CASE(WSAEREMOTE)
	CASE(WSASYSNOTREADY)
	CASE(WSAVERNOTSUPPORTED)
	CASE(WSANOTINITIALISED)
	CASE(WSAEDISCON)
	CASE(WSAENOMORE)
	CASE(WSAECANCELLED)
	CASE(WSAEINVALIDPROCTABLE)
	CASE(WSAEINVALIDPROVIDER)
	CASE(WSAEPROVIDERFAILEDINIT)
	CASE(WSASYSCALLFAILURE)
	CASE(WSASERVICE_NOT_FOUND)
	CASE(WSATYPE_NOT_FOUND)
	CASE(WSA_E_NO_MORE)
	CASE(WSA_E_CANCELLED)
	CASE(WSAEREFUSED)
	CASE(WSAHOST_NOT_FOUND)
	CASE(WSATRY_AGAIN)
	CASE(WSANO_RECOVERY)
	CASE(WSANO_DATA)
	CASE(WSA_QOS_RECEIVERS)
	CASE(WSA_QOS_SENDERS)
	CASE(WSA_QOS_NO_SENDERS)
	CASE(WSA_QOS_NO_RECEIVERS)
	CASE(WSA_QOS_REQUEST_CONFIRMED)
	CASE(WSA_QOS_ADMISSION_FAILURE)
	CASE(WSA_QOS_POLICY_FAILURE)
	CASE(WSA_QOS_BAD_STYLE)
	CASE(WSA_QOS_BAD_OBJECT)
	CASE(WSA_QOS_TRAFFIC_CTRL_ERROR)
	CASE(WSA_QOS_GENERIC_ERROR)
	CASE(WSA_QOS_ESERVICETYPE)
	CASE(WSA_QOS_EFLOWSPEC)
	CASE(WSA_QOS_EPROVSPECBUF)
	CASE(WSA_QOS_EFILTERSTYLE)
	CASE(WSA_QOS_EFILTERTYPE)
	CASE(WSA_QOS_EFILTERCOUNT)
	CASE(WSA_QOS_EOBJLENGTH)
	CASE(WSA_QOS_EFLOWCOUNT)
	CASE(WSA_QOS_EUNKOWNPSOBJ)
	CASE(WSA_QOS_EPOLICYOBJ)
	CASE(WSA_QOS_EFLOWDESC)
	CASE(WSA_QOS_EPSFLOWSPEC)
	CASE(WSA_QOS_EPSFILTERSPEC)
	CASE(WSA_QOS_ESDMODEOBJ)
	CASE(WSA_QOS_ESHAPERATEOBJ)
	CASE(WSA_QOS_RESERVED_PETYPE)
	/* Side By Side Error Codes */
	CASE(ERROR_SXS_SECTION_NOT_FOUND)
	CASE(ERROR_SXS_CANT_GEN_ACTCTX)
	CASE(ERROR_SXS_INVALID_ACTCTXDATA_FORMAT)
	CASE(ERROR_SXS_ASSEMBLY_NOT_FOUND)
	CASE(ERROR_SXS_MANIFEST_FORMAT_ERROR)
	CASE(ERROR_SXS_MANIFEST_PARSE_ERROR)
	CASE(ERROR_SXS_ACTIVATION_CONTEXT_DISABLED)
	CASE(ERROR_SXS_KEY_NOT_FOUND)
	CASE(ERROR_SXS_VERSION_CONFLICT)
	CASE(ERROR_SXS_WRONG_SECTION_TYPE)
	CASE(ERROR_SXS_THREAD_QUERIES_DISABLED)
	CASE(ERROR_SXS_PROCESS_DEFAULT_ALREADY_SET)
	CASE(ERROR_SXS_UNKNOWN_ENCODING_GROUP)
	CASE(ERROR_SXS_UNKNOWN_ENCODING)
	CASE(ERROR_SXS_INVALID_XML_NAMESPACE_URI)
	CASE(ERROR_SXS_ROOT_MANIFEST_DEPENDENCY_NOT_INSTALLED)
	CASE(ERROR_SXS_LEAF_MANIFEST_DEPENDENCY_NOT_INSTALLED)
	CASE(ERROR_SXS_INVALID_ASSEMBLY_IDENTITY_ATTRIBUTE)
	CASE(ERROR_SXS_MANIFEST_MISSING_REQUIRED_DEFAULT_NAMESPACE)
	CASE(ERROR_SXS_MANIFEST_INVALID_REQUIRED_DEFAULT_NAMESPACE)
	CASE(ERROR_SXS_PRIVATE_MANIFEST_CROSS_PATH_WITH_REPARSE_POINT)
	CASE(ERROR_SXS_DUPLICATE_DLL_NAME)
	CASE(ERROR_SXS_DUPLICATE_WINDOWCLASS_NAME)
	CASE(ERROR_SXS_DUPLICATE_CLSID)
	CASE(ERROR_SXS_DUPLICATE_IID)
	CASE(ERROR_SXS_DUPLICATE_TLBID)
	CASE(ERROR_SXS_DUPLICATE_PROGID)
	CASE(ERROR_SXS_DUPLICATE_ASSEMBLY_NAME)
	CASE(ERROR_SXS_FILE_HASH_MISMATCH)
	CASE(ERROR_SXS_POLICY_PARSE_ERROR)
	CASE(ERROR_SXS_XML_E_MISSINGQUOTE)
	CASE(ERROR_SXS_XML_E_COMMENTSYNTAX)
	CASE(ERROR_SXS_XML_E_BADSTARTNAMECHAR)
	CASE(ERROR_SXS_XML_E_BADNAMECHAR)
	CASE(ERROR_SXS_XML_E_BADCHARINSTRING)
	CASE(ERROR_SXS_XML_E_XMLDECLSYNTAX)
	CASE(ERROR_SXS_XML_E_BADCHARDATA)
	CASE(ERROR_SXS_XML_E_MISSINGWHITESPACE)
	CASE(ERROR_SXS_XML_E_EXPECTINGTAGEND)
	CASE(ERROR_SXS_XML_E_MISSINGSEMICOLON)
	CASE(ERROR_SXS_XML_E_UNBALANCEDPAREN)
	CASE(ERROR_SXS_XML_E_INTERNALERROR)
	CASE(ERROR_SXS_XML_E_UNEXPECTED_WHITESPACE)
	CASE(ERROR_SXS_XML_E_INCOMPLETE_ENCODING)
	CASE(ERROR_SXS_XML_E_MISSING_PAREN)
	CASE(ERROR_SXS_XML_E_EXPECTINGCLOSEQUOTE)
	CASE(ERROR_SXS_XML_E_MULTIPLE_COLONS)
	CASE(ERROR_SXS_XML_E_INVALID_DECIMAL)
	CASE(ERROR_SXS_XML_E_INVALID_HEXIDECIMAL)
	CASE(ERROR_SXS_XML_E_INVALID_UNICODE)
	CASE(ERROR_SXS_XML_E_WHITESPACEORQUESTIONMARK)
	CASE(ERROR_SXS_XML_E_UNEXPECTEDENDTAG)
	CASE(ERROR_SXS_XML_E_UNCLOSEDTAG)
	CASE(ERROR_SXS_XML_E_DUPLICATEATTRIBUTE)
	CASE(ERROR_SXS_XML_E_MULTIPLEROOTS)
	CASE(ERROR_SXS_XML_E_INVALIDATROOTLEVEL)
	CASE(ERROR_SXS_XML_E_BADXMLDECL)
	CASE(ERROR_SXS_XML_E_MISSINGROOT)
	CASE(ERROR_SXS_XML_E_UNEXPECTEDEOF)
	CASE(ERROR_SXS_XML_E_BADPEREFINSUBSET)
	CASE(ERROR_SXS_XML_E_UNCLOSEDSTARTTAG)
	CASE(ERROR_SXS_XML_E_UNCLOSEDENDTAG)
	CASE(ERROR_SXS_XML_E_UNCLOSEDSTRING)
	CASE(ERROR_SXS_XML_E_UNCLOSEDCOMMENT)
	CASE(ERROR_SXS_XML_E_UNCLOSEDDECL)
	CASE(ERROR_SXS_XML_E_UNCLOSEDCDATA)
	CASE(ERROR_SXS_XML_E_RESERVEDNAMESPACE)
	CASE(ERROR_SXS_XML_E_INVALIDENCODING)
	CASE(ERROR_SXS_XML_E_INVALIDSWITCH)
	CASE(ERROR_SXS_XML_E_BADXMLCASE)
	CASE(ERROR_SXS_XML_E_INVALID_STANDALONE)
	CASE(ERROR_SXS_XML_E_UNEXPECTED_STANDALONE)
	CASE(ERROR_SXS_XML_E_INVALID_VERSION)
	CASE(ERROR_SXS_XML_E_MISSINGEQUALS)
	CASE(ERROR_SXS_PROTECTION_RECOVERY_FAILED)
	CASE(ERROR_SXS_PROTECTION_PUBLIC_KEY_TOO_SHORT)
	CASE(ERROR_SXS_PROTECTION_CATALOG_NOT_VALID)
	CASE(ERROR_SXS_UNTRANSLATABLE_HRESULT)
	CASE(ERROR_SXS_PROTECTION_CATALOG_FILE_MISSING)
	CASE(ERROR_SXS_MISSING_ASSEMBLY_IDENTITY_ATTRIBUTE)
	CASE(ERROR_SXS_INVALID_ASSEMBLY_IDENTITY_ATTRIBUTE_NAME)
	/* IPSec Error codes */
	CASE(ERROR_IPSEC_QM_POLICY_EXISTS)
	CASE(ERROR_IPSEC_QM_POLICY_NOT_FOUND)
	CASE(ERROR_IPSEC_QM_POLICY_IN_USE)
	CASE(ERROR_IPSEC_MM_POLICY_EXISTS)
	CASE(ERROR_IPSEC_MM_POLICY_NOT_FOUND)
	CASE(ERROR_IPSEC_MM_POLICY_IN_USE)
	CASE(ERROR_IPSEC_MM_FILTER_EXISTS)
	CASE(ERROR_IPSEC_MM_FILTER_NOT_FOUND)
	CASE(ERROR_IPSEC_TRANSPORT_FILTER_EXISTS)
	CASE(ERROR_IPSEC_TRANSPORT_FILTER_NOT_FOUND)
	CASE(ERROR_IPSEC_MM_AUTH_EXISTS)
	CASE(ERROR_IPSEC_MM_AUTH_NOT_FOUND)
	CASE(ERROR_IPSEC_MM_AUTH_IN_USE)
	CASE(ERROR_IPSEC_DEFAULT_MM_POLICY_NOT_FOUND)
	CASE(ERROR_IPSEC_DEFAULT_MM_AUTH_NOT_FOUND)
	CASE(ERROR_IPSEC_DEFAULT_QM_POLICY_NOT_FOUND)
	CASE(ERROR_IPSEC_TUNNEL_FILTER_EXISTS)
	CASE(ERROR_IPSEC_TUNNEL_FILTER_NOT_FOUND)
	CASE(ERROR_IPSEC_MM_FILTER_PENDING_DELETION)
	CASE(ERROR_IPSEC_TRANSPORT_FILTER_PENDING_DELETION)
	CASE(ERROR_IPSEC_TUNNEL_FILTER_PENDING_DELETION)
	CASE(ERROR_IPSEC_MM_POLICY_PENDING_DELETION)
	CASE(ERROR_IPSEC_MM_AUTH_PENDING_DELETION)
	CASE(ERROR_IPSEC_QM_POLICY_PENDING_DELETION)
	CASE(WARNING_IPSEC_MM_POLICY_PRUNED)
	CASE(WARNING_IPSEC_QM_POLICY_PRUNED)
	CASE(ERROR_IPSEC_IKE_NEG_STATUS_BEGIN)
	CASE(ERROR_IPSEC_IKE_AUTH_FAIL)
	CASE(ERROR_IPSEC_IKE_ATTRIB_FAIL)
	CASE(ERROR_IPSEC_IKE_NEGOTIATION_PENDING)
	CASE(ERROR_IPSEC_IKE_GENERAL_PROCESSING_ERROR)
	CASE(ERROR_IPSEC_IKE_TIMED_OUT)
	CASE(ERROR_IPSEC_IKE_NO_CERT)
	CASE(ERROR_IPSEC_IKE_SA_DELETED)
	CASE(ERROR_IPSEC_IKE_SA_REAPED)
	CASE(ERROR_IPSEC_IKE_MM_ACQUIRE_DROP)
	CASE(ERROR_IPSEC_IKE_QM_ACQUIRE_DROP)
	CASE(ERROR_IPSEC_IKE_QUEUE_DROP_MM)
	CASE(ERROR_IPSEC_IKE_QUEUE_DROP_NO_MM)
	CASE(ERROR_IPSEC_IKE_DROP_NO_RESPONSE)
	CASE(ERROR_IPSEC_IKE_MM_DELAY_DROP)
	CASE(ERROR_IPSEC_IKE_QM_DELAY_DROP)
	CASE(ERROR_IPSEC_IKE_ERROR)
	CASE(ERROR_IPSEC_IKE_CRL_FAILED)
	CASE(ERROR_IPSEC_IKE_INVALID_KEY_USAGE)
	CASE(ERROR_IPSEC_IKE_INVALID_CERT_TYPE)
	CASE(ERROR_IPSEC_IKE_NO_PRIVATE_KEY)
	CASE(ERROR_IPSEC_IKE_DH_FAIL)
	CASE(ERROR_IPSEC_IKE_INVALID_HEADER)
	CASE(ERROR_IPSEC_IKE_NO_POLICY)
	CASE(ERROR_IPSEC_IKE_INVALID_SIGNATURE)
	CASE(ERROR_IPSEC_IKE_KERBEROS_ERROR)
	CASE(ERROR_IPSEC_IKE_NO_PUBLIC_KEY)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_SA)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_PROP)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_TRANS)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_KE)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_ID)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_CERT)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_CERT_REQ)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_HASH)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_SIG)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_NONCE)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_NOTIFY)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_DELETE)
	CASE(ERROR_IPSEC_IKE_PROCESS_ERR_VENDOR)
	CASE(ERROR_IPSEC_IKE_INVALID_PAYLOAD)
	CASE(ERROR_IPSEC_IKE_LOAD_SOFT_SA)
	CASE(ERROR_IPSEC_IKE_SOFT_SA_TORN_DOWN)
	CASE(ERROR_IPSEC_IKE_INVALID_COOKIE)
	CASE(ERROR_IPSEC_IKE_NO_PEER_CERT)
	CASE(ERROR_IPSEC_IKE_PEER_CRL_FAILED)
	CASE(ERROR_IPSEC_IKE_POLICY_CHANGE)
	CASE(ERROR_IPSEC_IKE_NO_MM_POLICY)
	CASE(ERROR_IPSEC_IKE_NOTCBPRIV)
	CASE(ERROR_IPSEC_IKE_SECLOADFAIL)
	CASE(ERROR_IPSEC_IKE_FAILSSPINIT)
	CASE(ERROR_IPSEC_IKE_FAILQUERYSSP)
	CASE(ERROR_IPSEC_IKE_SRVACQFAIL)
	CASE(ERROR_IPSEC_IKE_SRVQUERYCRED)
	CASE(ERROR_IPSEC_IKE_GETSPIFAIL)
	CASE(ERROR_IPSEC_IKE_INVALID_FILTER)
	CASE(ERROR_IPSEC_IKE_OUT_OF_MEMORY)
	CASE(ERROR_IPSEC_IKE_ADD_UPDATE_KEY_FAILED)
	CASE(ERROR_IPSEC_IKE_INVALID_POLICY)
	CASE(ERROR_IPSEC_IKE_UNKNOWN_DOI)
	CASE(ERROR_IPSEC_IKE_INVALID_SITUATION)
	CASE(ERROR_IPSEC_IKE_DH_FAILURE)
	CASE(ERROR_IPSEC_IKE_INVALID_GROUP)
	CASE(ERROR_IPSEC_IKE_ENCRYPT)
	CASE(ERROR_IPSEC_IKE_DECRYPT)
	CASE(ERROR_IPSEC_IKE_POLICY_MATCH)
	CASE(ERROR_IPSEC_IKE_UNSUPPORTED_ID)
	CASE(ERROR_IPSEC_IKE_INVALID_HASH)
	CASE(ERROR_IPSEC_IKE_INVALID_HASH_ALG)
	CASE(ERROR_IPSEC_IKE_INVALID_HASH_SIZE)
	CASE(ERROR_IPSEC_IKE_INVALID_ENCRYPT_ALG)
	CASE(ERROR_IPSEC_IKE_INVALID_AUTH_ALG)
	CASE(ERROR_IPSEC_IKE_INVALID_SIG)
	CASE(ERROR_IPSEC_IKE_LOAD_FAILED)
	CASE(ERROR_IPSEC_IKE_RPC_DELETE)
	CASE(ERROR_IPSEC_IKE_BENIGN_REINIT)
	CASE(ERROR_IPSEC_IKE_INVALID_RESPONDER_LIFETIME_NOTIFY)
	CASE(ERROR_IPSEC_IKE_INVALID_CERT_KEYLEN)
	CASE(ERROR_IPSEC_IKE_MM_LIMIT)
	CASE(ERROR_IPSEC_IKE_NEGOTIATION_DISABLED)
	CASE(ERROR_IPSEC_IKE_NEG_STATUS_END)
	default: return "UNKNOWN";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WinErrMsg --
 *
 *	Same as Tcl_ErrnoMsg(), but for windows error codes.
 *
 * Results:
 *      The message that is associated with the error code.
 *    
 * Side Effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

const char *
Tcl_WinErrMsg (unsigned int errorCode, va_list *extra)
{
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    DWORD result;

    /*
     * If the "customer" bit is set, this function was called
     * by mistake.
     */

    if (errorCode & (0x1 << 29)) {
	return NULL;
    }

    result = FormatMessage (
	    FORMAT_MESSAGE_FROM_SYSTEM |
	    FORMAT_MESSAGE_MAX_WIDTH_MASK,
	    0L,
	    errorCode,
	    0,		    /* use best guess localization */
	    tsdPtr->sysMsgSpace,
	    ERR_BUF_SIZE,
	    extra);

    return (result ? tsdPtr->sysMsgSpace : NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WinError --
 *
 *	Same as Tcl_PosixError(), but for windows error codes.
 *
 * Results:
 *      The class, ID and message that is associated with the error
 *	code.
 *    
 * Side Effects:
 *	sets $errorCode in the specified interpreter.
 *
 *----------------------------------------------------------------------
 */

const char *
Tcl_WinError (Tcl_Interp *interp, unsigned int errorCode, va_list *extra)
{
    CONST char *id, *msg;
    char num[TCL_INTEGER_SPACE];

    id = Tcl_WinErrId(errorCode);
    msg = Tcl_WinErrMsg(errorCode, extra);
    snprintf(num, TCL_INTEGER_SPACE, "%lu", errorCode);
    Tcl_SetErrorCode(interp, "WINDOWS", num, id, msg, NULL);
    return msg;
}
