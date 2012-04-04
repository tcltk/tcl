/*
 * tclIOSock.c --
 *
 *	Common routines used by all socket based channel types.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

#if defined(_WIN32) && defined(UNICODE)
/* On Windows, we always need the ASCII version. */
#   undef gai_strerror
#   define gai_strerror gai_strerrorA
#endif

/*
 *---------------------------------------------------------------------------
 *
 * TclSockGetPort --
 *
 *	Maps from a string, which could be a service name, to a port. Used by
 *	socket creation code to get port numbers and resolve registered
 *	service names to port numbers.
 *
 * Results:
 *	A standard Tcl result. On success, the port number is returned in
 *	portPtr. On failure, an error message is left in the interp's result.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

int
TclSockGetPort(
    Tcl_Interp *interp,
    const char *string, /* Integer or service name */
    const char *proto, /* "tcp" or "udp", typically */
    int *portPtr)		/* Return port number */
{
    struct servent *sp;		/* Protocol info for named services */
    Tcl_DString ds;
    const char *native;

    if (Tcl_GetInt(NULL, string, portPtr) != TCL_OK) {
	/*
	 * Don't bother translating 'proto' to native.
	 */

	native = Tcl_UtfToExternalDString(NULL, string, -1, &ds);
	sp = getservbyname(native, proto);		/* INTL: Native. */
	Tcl_DStringFree(&ds);
	if (sp != NULL) {
	    *portPtr = ntohs((unsigned short) sp->s_port);
	    return TCL_OK;
	}
    }
    if (Tcl_GetInt(interp, string, portPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (*portPtr > 0xFFFF) {
	Tcl_AppendResult(interp, "couldn't open socket: port number too high",
		NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclSockMinimumBuffers --
 *
 *	Ensure minimum buffer sizes (non zero).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Sets SO_SNDBUF and SO_RCVBUF sizes.
 *
 *----------------------------------------------------------------------
 */

#ifndef _WIN32
#   define SOCKET size_t
#endif

int
TclSockMinimumBuffers(
    void *sock,			/* Socket file descriptor */
    int size)			/* Minimum buffer size */
{
    int current;
    socklen_t len;

    len = sizeof(int);
    getsockopt((SOCKET)sock, SOL_SOCKET, SO_SNDBUF, (char *)&current, &len);
    if (current < size) {
	len = sizeof(int);
	setsockopt((SOCKET)sock, SOL_SOCKET, SO_SNDBUF, (char *)&size, len);
    }
    len = sizeof(int);
    getsockopt((SOCKET)sock, SOL_SOCKET, SO_RCVBUF, (char *)&current, &len);
    if (current < size) {
	len = sizeof(int);
	setsockopt((SOCKET)sock, SOL_SOCKET, SO_RCVBUF, (char *)&size, len);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateSocketAddress --
 *
 *	This function initializes a sockaddr structure for a host and port.
 *
 * Results:
 *	1 if the host was valid, 0 if the host could not be converted to an IP
 *	address.
 *
 * Side effects:
 *	Fills in the *sockaddrPtr structure.
 *
 *----------------------------------------------------------------------
 */

int
TclCreateSocketAddress(
    Tcl_Interp *interp,                 /* Interpreter for querying
					 * the desired socket family */
    struct addrinfo **addrlist,		/* Socket address list */
    const char *host,			/* Host. NULL implies INADDR_ANY */
    int port,				/* Port number */
    int willBind,			/* Is this an address to bind() to or
					 * to connect() to? */
    const char **errorMsgPtr)		/* Place to store the error message
					 * detail, if available. */
{
    struct addrinfo hints;
    struct addrinfo *p;
    struct addrinfo *v4head = NULL, *v4ptr = NULL;
    struct addrinfo *v6head = NULL, *v6ptr = NULL;
    char *native = NULL, portstring[TCL_INTEGER_SPACE];
    const char *family = NULL;
    Tcl_DString ds;
    int result, i;

    TclFormatInt(portstring, port);

    if (host != NULL) {
	native = Tcl_UtfToExternalDString(NULL, host, -1, &ds);
    }
    
    (void) memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = AF_UNSPEC;
    /* 
     * Magic variable to enforce a certain address family - to be superseded
     * by a TIP that adds explicit switches to [socket]
     */
    if (interp != NULL) {
        family = Tcl_GetVar(interp, "::tcl::unsupported::socketAF", 0);
        if (family != NULL) {
            if (strcmp(family, "inet") == 0) {
                hints.ai_family = AF_INET;
            } else if (strcmp(family, "inet6") == 0) {
                hints.ai_family = AF_INET6;
            }
        }
    }

    hints.ai_socktype = SOCK_STREAM;
#if 0
    /*
     * We found some problems when using AI_ADDRCONFIG, e.g. on systems that
     * have no networking besides the loopback interface and want to resolve
     * localhost. See bugs 3385024, 3382419, 3382431. As the advantage of
     * using AI_ADDRCONFIG in situations where it works, is probably low,
     * we'll leave it out for now. After all, it is just an optimisation.
     */
#if defined(AI_ADDRCONFIG) && !defined(_AIX) && !defined(__hpux)
    /*
     * Missing on: OpenBSD, NetBSD.
     * Causes failure when used on AIX 5.1 and HP-UX
     */
    hints.ai_flags |= AI_ADDRCONFIG;
#endif
#endif
    if (willBind) {
	hints.ai_flags |= AI_PASSIVE;
    } 

    result = getaddrinfo(native, portstring, &hints, addrlist);

    if (host != NULL) {
	Tcl_DStringFree(&ds);
    }

    if (result != 0) {
	goto error;
    }

    /*
     * Put IPv4 addresses before IPv6 addresses to maximize backwards
     * compatibility of [fconfigure -sockname] output.
     *
     * There might be more elegant/efficient ways to do this.
     */
    if (willBind) {
	for (p = *addrlist; p != NULL; p = p->ai_next) {
	    if (p->ai_family == AF_INET) {
		if (v4head == NULL) {
		    v4head = p;
		} else {
		    v4ptr->ai_next = p;
		}
		v4ptr = p;
	    } else {
		if (v6head == NULL) {
		    v6head = p;
		} else {
		    v6ptr->ai_next = p;
		}
		v6ptr = p;
	    }
	}
	*addrlist = NULL;
	if (v6head != NULL) {
	    *addrlist = v6head;
	    v6ptr->ai_next = NULL;
	}
	if (v4head != NULL) {
	    v4ptr->ai_next = *addrlist;
	    *addrlist = v4head;
	}
    }
    i = 0;
    for (p = *addrlist; p != NULL; p = p->ai_next) {
	i++;
    }
    
    return 1;
	
    /*
     * Ought to use gai_strerror() here...
     */

error:
    switch (result) {
    case EAI_NONAME:
    case EAI_SERVICE:
#if defined(EAI_ADDRFAMILY) && EAI_ADDRFAMILY != EAI_NONAME
    case EAI_ADDRFAMILY:
#endif
#if defined(EAI_NODATA) && EAI_NODATA != EAI_NONAME
    case EAI_NODATA:
#endif
	*errorMsgPtr = gai_strerror(result);
	errno = EHOSTUNREACH;
	return 0;
#ifdef EAI_SYSTEM
    case EAI_SYSTEM:
	return 0;
#endif
    default:
	*errorMsgPtr = gai_strerror(result);
	errno = ENXIO;
	return 0;
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
