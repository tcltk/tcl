---
CommandName: Tcl_OpenTcpClient
ManualSection: 3
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_OpenFileChannel(3)
 - Tcl_RegisterChannel(3)
 - vwait(n)
Keywords:
 - channel
 - client
 - server
 - socket
 - TCP
Copyright:
 - Copyright (c) 1996-7 Sun Microsystems, Inc.
---

# Name

Tcl\_OpenTcpClient, Tcl\_MakeTcpClientChannel, Tcl\_OpenTcpServer, Tcl\_OpenTcpServerEx - procedures to open channels using TCP sockets

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>=**
[Tcl\_Channel]{.ret} [Tcl\_OpenTcpClient]{.ccmd}[interp, port, host, myaddr, myport, async]{.cargs}
[Tcl\_Channel]{.ret} [Tcl\_MakeTcpClientChannel]{.ccmd}[sock]{.cargs}
[Tcl\_Channel]{.ret} [Tcl\_OpenTcpServer]{.ccmd}[interp, port, myaddr, proc, clientData]{.cargs}
[Tcl\_Channel]{.ret} [Tcl\_OpenTcpServerEx]{.ccmd}[interp, service, myaddr, flags, backlog, proc, clientData]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp in Tcl interpreter to use for error reporting.  If non-NULL and an error occurs, an error message is left in the interpreter's result. .AP int port in A port number to connect to as a client or to listen on as a server. .AP "const char" \*service in A string specifying the port number to connect to as a client or to listen on as  a server. .AP "const char" \*host in A string specifying a host name or address for the remote end of the connection. .AP int myport in A port number for the client's end of the socket.  If 0, a port number is allocated at random. .AP "const char" \*myaddr in A string specifying the host name or address for network interface to use for the local end of the connection.  If NULL, a default interface is chosen. .AP int async in If nonzero, the client socket is connected asynchronously to the server. .AP int backlog in Length of OS listen backlog queue. Use -1 for default value. .AP "unsigned int" flags in ORed combination of **TCL\_TCPSERVER\_\*** flags that specify additional information about the socket being created. .AP void \*sock in Platform-specific handle for client TCP socket. .AP Tcl\_TcpAcceptProc \*proc in Pointer to a procedure to invoke each time a new connection is accepted via the socket. .AP void \*clientData in Arbitrary one-word value to pass to *proc*.

# Description

These functions are convenience procedures for creating channels that communicate over TCP sockets. The operations on a channel are described in the manual entry for **Tcl\_OpenFileChannel**.

## Tcl\_opentcpclient

**Tcl\_OpenTcpClient** opens a client TCP socket connected to a *port* on a specific *host*, and returns a channel that can be used to communicate with the server. The host to connect to can be specified either as a domain name style name (e.g. **www.sunlabs.com**), or as a string containing the alphanumeric representation of its four-byte address (e.g. **127.0.0.1**). Use the string **localhost** to connect to a TCP socket on the host on which the function is invoked.

The *myaddr* and *myport* arguments allow a client to specify an address for the local end of the connection.  If *myaddr* is NULL, then an interface is chosen automatically by the operating system. If *myport* is 0, then a port number is chosen at random by the operating system.

If *async* is zero, the call to **Tcl\_OpenTcpClient** returns only after the client socket has either successfully connected to the server, or the attempted connection has failed. If *async* is nonzero the socket is connected asynchronously and the returned channel may not yet be connected to the server when the call to **Tcl\_OpenTcpClient** returns. If the channel is in blocking mode and an input or output operation is done on the channel before the connection is completed or fails, that operation will wait until the connection either completes successfully or fails. If the channel is in nonblocking mode, the input or output operation will return immediately and a subsequent call to **Tcl\_InputBlocked** on the channel will return nonzero.

The returned channel is opened for reading and writing. If an error occurs in opening the socket, **Tcl\_OpenTcpClient** returns NULL and records a POSIX error code that can be retrieved with **Tcl\_GetErrno**. In addition, if *interp* is non-NULL, an error message is left in the interpreter's result.

The newly created channel is not registered in the supplied interpreter; to register it, use **Tcl\_RegisterChannel**. If one of the standard channels, **stdin**, **stdout** or **stderr** was previously closed, the act of creating the new channel also assigns it as a replacement for the standard channel.

## Tcl\_maketcpclientchannel

**Tcl\_MakeTcpClientChannel** creates a **Tcl\_Channel** around an existing, platform specific, handle for a client TCP socket.

The newly created channel is not registered in the supplied interpreter; to register it, use **Tcl\_RegisterChannel**. If one of the standard channels, **stdin**, **stdout** or **stderr** was previously closed, the act of creating the new channel also assigns it as a replacement for the standard channel.

## Tcl\_opentcpserver

**Tcl\_OpenTcpServer** opens a TCP socket on the local host on a specified *port* and uses the Tcl event mechanism to accept requests from clients to connect to it. The *myaddr* argument specifies the network interface. If *myaddr* is NULL the special address INADDR\_ANY should be used to allow connections from any network interface. Each time a client connects to this socket, Tcl creates a channel for the new connection and invokes *proc* with information about the channel. *Proc* must match the following prototype:

```
typedef void Tcl_TcpAcceptProc(
        void *clientData,
        Tcl_Channel channel,
        char *hostName,
        int port);
```

The *clientData* argument will be the same as the *clientData* argument to **Tcl\_OpenTcpServer**, *channel* will be the handle for the new channel, *hostName* points to a string containing the name of the client host making the connection, and *port* will contain the client's port number. The new channel is opened for both input and output. If *proc* raises an error, the connection is closed automatically. *Proc* has no return value, but if it wishes to reject the connection it can close *channel*.

**Tcl\_OpenTcpServer** normally returns a pointer to a channel representing the server socket. If an error occurs, **Tcl\_OpenTcpServer** returns NULL and records a POSIX error code that can be retrieved with **Tcl\_GetErrno**. In addition, if the interpreter is non-NULL, an error message is left in the interpreter's result.

The channel returned by **Tcl\_OpenTcpServer** cannot be used for either input or output. It is simply a handle for the socket used to accept connections. The caller can close the channel to shut down the server and disallow further connections from new clients.

TCP server channels operate correctly only in applications that dispatch events through **Tcl\_DoOneEvent** or through Tcl commands such as [vwait]; otherwise Tcl will never notice that a connection request from a remote client is pending.

The newly created channel is not registered in the supplied interpreter; to register it, use **Tcl\_RegisterChannel**. If one of the standard channels, **stdin**, **stdout** or **stderr** was previously closed, the act of creating the new channel also assigns it as a replacement for the standard channel.

## Tcl\_opentcpserverex

**Tcl\_OpenTcpServerEx** behaviour is identical to **Tcl\_OpenTcpServer** but gives more flexibility to the user by providing a mean to further customize some aspects of the socket via the *flags* parameter. Available flags (dependent on platform) are *TCL\_TCPSERVER\_REUSEADDR* *TCL\_TCPSERVER\_REUSEPORT*

# Platform issues

On Unix platforms, the socket handle is a Unix file descriptor as returned by the [socket] system call.  On the Windows platform, the socket handle is a **SOCKET** as defined in the WinSock API.


[socket]: socket.md
[vwait]: vwait.md

