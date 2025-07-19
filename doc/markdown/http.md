---
CommandName: http
ManualSection: n
Version: 2.10
TclPart: http
TclDescription: Tcl Bundled Packages
Links:
 - safe(n)
 - socket(n)
 - safesock(n)
Keywords:
 - internet
 - security policy
 - socket
 - www
Copyright:
 - Copyright (c) 1995-1997 Sun Microsystems, Inc.
 - Copyright (c) 1998-2000 Ajuba Solutions.
 - Copyright (c) 2004 ActiveState Corporation.
---

# Name

http - Client-side implementation of the HTTP/1.1 protocol

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [http]{.lit} [2.10]{.optlit}

[::http::config]{.cmd} [-option value]{.optdot}
[::http::geturl]{.cmd} [url]{.arg} [-option value]{.optdot}
[::http::formatQuery]{.cmd} [key]{.arg} [value]{.arg} [key value]{.optdot}
[::http::quoteString]{.cmd} [value]{.arg}
[::http::reset]{.cmd} [token]{.arg} [why]{.optarg}
[::http::wait]{.cmd} [token]{.arg}
[::http::status]{.cmd} [token]{.arg}
[::http::size]{.cmd} [token]{.arg}
[::http::error]{.cmd} [token]{.arg}
[::http::postError]{.cmd} [token]{.arg}
[::http::cleanup]{.cmd} [token]{.arg}
[::http::requestLine]{.cmd} [token]{.arg}
[::http::requestHeaders]{.cmd} [token]{.arg} [headerName]{.optarg}
[::http::requestHeaderValue]{.cmd} [token]{.arg} [headerName]{.arg}
[::http::responseLine]{.cmd} [token]{.arg}
[::http::responseCode]{.cmd} [token]{.arg}
[::http::reasonPhrase]{.cmd} [code]{.arg}
[::http::responseHeaders]{.cmd} [token]{.arg} [headerName]{.optarg}
[::http::responseHeaderValue]{.cmd} [token]{.arg} [headerName]{.arg}
[::http::responseInfo]{.cmd} [token]{.arg}
[::http::responseBody]{.cmd} [token]{.arg}
[::http::register]{.cmd} [proto]{.arg} [port]{.arg} [command]{.arg} [socketCmdVarName]{.optarg} [useSockThread]{.optarg} [endToEndProxy]{.optarg}
[::http::registerError]{.cmd} [sock]{.arg} [message]{.optarg}
[::http::unregister]{.cmd} [proto]{.arg}
[::http::code]{.cmd} [token]{.arg}
[::http::data]{.cmd} [token]{.arg}
[::http::meta]{.cmd} [token]{.arg} [headerName]{.optarg}
[::http::metaValue]{.cmd} [token]{.arg} [headerName]{.arg}
[::http::ncode]{.cmd} [token]{.arg}
:::

# Exported commands

Namespace **http** exports the commands **config**, **formatQuery**, **geturl**, **postError**, **quoteString**, **reasonPhrase**, **register**, **registerError**, **requestHeaders**, **requestHeaderValue**, **requestLine**, **responseBody**, **responseCode**, **responseHeaders**, **responseHeaderValue**, **responseInfo**, **responseLine**, **reset**, **unregister**, and **wait**.

It does not export the commands **cleanup**, **code**, **data**, **error**, **meta**, **metaValue**, **ncode**, **size**, or **status**.

# Description

The **http** package provides the client side of the HTTP/1.1 protocol, as defined in RFC 9110 to 9112, which supersede RFC 7230 to RFC 7235, which in turn supersede RFC 2616. The package implements the GET, POST, and HEAD operations of HTTP/1.1.  It allows configuration of a proxy host to get through firewalls.  The package is compatible with the **Safesock** security policy, so it can be used by untrusted applets to do URL fetching from a restricted set of hosts. This package can be extended to support additional HTTP transport protocols, such as HTTPS, by providing a custom **socket** command, via **::http::register**.

The **::http::geturl** procedure does a HTTP transaction. Its *options*  determine whether a GET, POST, or HEAD transaction is performed. The return value of **::http::geturl** is a token for the transaction. The token can be supplied as an argument to other commands, to manage the transaction and examine its results.

If the **-command** option is specified, then the HTTP operation is done in the background. **::http::geturl** returns immediately after generating the HTTP request and the **-command** callback is invoked when the transaction completes.  For this to work, the Tcl event loop must be active.  In Tk applications this is always true.  For pure-Tcl applications, the caller can use **::http::wait** after calling **::http::geturl** to start the event loop.

**Note:** The event queue is even used without the **-command** option. As a side effect, arbitrary commands may be processed while **http::geturl** is running.

When the HTTP server has replied to the request, call the command **::http::responseInfo**, which returns a **dict** of metadata that is essential for identifying a successful transaction and making use of the response.  See section **METADATA** for details of the information returned. The response itself is returned by command **::http::responseBody**, unless it has been redirected to a file by the *-channel* option of **::http::geturl**.

# Commands

**::http::config** ?*options*?
: The **::http::config** command is used to set and query the name of the proxy server and port, and the User-Agent name used in the HTTP requests.  If no options are specified, then the current configuration is returned.  If a single argument is specified, then it should be one of the flags described below.  In this case the current value of that setting is returned.  Otherwise, the options should be a set of flags and values that define the configuration:

**-accept** *mimetypes*
: The Accept header of the request.  The default is */*, which means that all types of documents are accepted.  Otherwise you can supply a comma-separated list of mime type patterns that you are willing to receive.  For example, "image/gif, image/jpeg, text/*".

**-cookiejar** *command*
: The cookie store for the package to use to manage HTTP cookies. *command* is a command prefix list; if the empty list (the default value) is used, no cookies will be sent by requests or stored from responses. The command indicated by *command*, if supplied, must obey the **COOKIE JAR PROTOCOL** described below.

**-pipeline** *boolean*
: Specifies whether HTTP/1.1 transactions on a persistent socket will be pipelined.  See the **PERSISTENT SOCKETS** section for details. The default is 1.

**-postfresh** *boolean*
: Specifies whether requests that use the **POST** method will always use a fresh socket, overriding the **-keepalive** option of command **http::geturl**.  See the **PERSISTENT SOCKETS** section for details. The default is 0.

**-proxyauth** *string*
: If non-empty, the string is supplied to the proxy server as the value of the request header Proxy-Authorization.  This option can be used for HTTP Basic Authentication.  If the proxy server requires authentication by another technique, e.g. Digest Authentication, the **-proxyauth** option is not useful.  In that case the caller must expect a 407 response from the proxy, compute the authentication value to be supplied, and use the **-headers** option to supply it as the value of the Proxy-Authorization header.

**-proxyfilter** *command*
: The command is a callback that is made during **::http::geturl** to determine if a proxy is required for a given host.  One argument, a host name, is added to *command* when it is invoked.  If a proxy is required, the callback should return a two-element list containing the proxy server and proxy port.  Otherwise the filter command should return an empty list.

    The default value of **-proxyfilter** is **http::ProxyRequired**, and this command returns the values of the **-proxyhost** and **-proxyport** settings if they are non-empty.  The options **-proxyhost**, **-proxyport**, and **-proxynot** are used only by **http::ProxyRequired**, and nowhere else in **::http::geturl**. A user-supplied **-proxyfilter** command may use these options, or alternatively it may obtain values from elsewhere in the calling script. In the latter case, any values provided for **-proxyhost**, **-proxyport**, and **-proxynot** are unused.
    The **::http::geturl** command runs the **-proxyfilter** callback inside a **catch** command.  Therefore an error in the callback command does not call the **bgerror** handler.  See the **ERRORS** section for details.

**-proxyhost** *hostname*
: The host name or IP address of the proxy server, if any.  If this value is the empty string, the URL host is contacted directly.  See **-proxyfilter** for how the value is used.

**-proxynot** *list*
: A Tcl list of domain names and IP addresses that should be accessed directly, not through the proxy server.  The target hostname is compared with each list element using a case-insensitive **string match**.  It is often convenient to use the wildcard "*" at the start of a domain name (e.g. *.example.com) or at the end of an IP address (e.g. 192.168.0.*).  See **-proxyfilter** for how the value is used.

**-proxyport** *number*
: The port number of the proxy server.  See **-proxyfilter** for how the value is used.

**-repost** *boolean*
: Specifies what to do if a POST request over a persistent connection fails because the server has half-closed the connection.  If boolean **true**, the request will be automatically retried; if boolean **false** it will not, and the application that uses **http::geturl** is expected to seek user confirmation before retrying the POST.  The value **true** should be used only under certain conditions. See the **PERSISTENT SOCKETS** section for details. The default is 0.

**-threadlevel** *level*
: Specifies whether and how to use the **Thread** package.  Possible values of *level* are 0, 1 or 2.

**0**
: (the default) do not use Thread

**1**
: use Thread if it is available, do not use it if it is unavailable

**2**
: use Thread if it is available, raise an error if it is unavailable

    The Tcl **socket -async** command can block in adverse cases (e.g. a slow DNS lookup).  Using the Thread package works around this problem, for both HTTP and HTTPS transactions.  Values of *level* other than 0 are available only to the main interpreter in each thread.  See section **THREADS** for more information.

**-urlencoding** *encoding*
: The *encoding* used for creating the x-url-encoded URLs with **::http::formatQuery** and **::http::quoteString**. The default is **utf-8**, as specified by RFC 2718.

**-useragent** *string*
: The value of the User-Agent header in the HTTP request.  In an unsafe interpreter, the default value depends upon the operating system, and the version numbers of **http** and **Tcl**, and is (for example) "**Mozilla/5.0 (Windows; U; Windows NT 10.0) http/2.10.0 Tcl/9.0.0**". A safe interpreter cannot determine its operating system, and so the default in a safe interpreter is to use a Windows 10 value with the current version numbers of **http** and **Tcl**.

**-zip** *boolean*
: If the value is boolean **true**, then by default requests will send a header "**Accept-Encoding: gzip,deflate**". If the value is boolean **false**, then by default requests will send a header "**Accept-Encoding: identity**". In either case the default can be overridden for an individual request by supplying a custom **Accept-Encoding** header in the **-headers** option of **http::geturl**. The default value is 1.

**::http::geturl** *url* ?*options*?
: The **::http::geturl** command is the main procedure in the package. The **-query** or **-querychannel** option causes a POST operation and the **-validate** option causes a HEAD operation; otherwise, a GET operation is performed.  The **::http::geturl** command returns a *token* value that can be passed as an argument to other commands to get information about the transaction.  See the **METADATA** and **ERRORS** section for details.  The **::http::geturl** command blocks until the operation completes, unless the **-command** option specifies a callback that is invoked when the HTTP transaction completes. **::http::geturl** takes several options:

**-binary** *boolean*
: Specifies whether to force interpreting the URL data as binary.  Normally this is auto-detected (anything not beginning with a **text** content type or whose content encoding is **gzip** or **deflate** is considered binary data).

**-blocksize** *size*
: The block size used when reading the URL. At most *size* bytes are read at once.  After each block, a call to the **-progress** callback is made (if that option is specified).

**-channel** *name*
: Copy the URL contents to channel *name* instead of saving it in a Tcl variable for retrieval by **::http::responseBody**.

**-command** *callback*
: The presence of this option causes **::http::geturl** to return immediately. After the HTTP transaction completes, the value of *callback* is expanded, an additional argument is added, and the resulting command is evaluated. The additional argument is the *token* returned from **::http::geturl**. This token is the name of an array that is described in the **STATE ARRAY** section.  Here is a template for the callback:


    ```
    proc httpCallback {token} {
        upvar 0 $token state
        # Access state as a Tcl array defined in this proc
        ...
        return
    }
    ```
    The **::http::geturl** command runs the **-command** callback inside a **catch** command.  Therefore an error in the callback command does not call the **bgerror** handler.  See the **ERRORS** section for details.

**-guesstype** *boolean*
: Attempt to guess the **Content-Type** and character set when a misconfigured server provides no information.  The default value is *false* (do nothing).  If boolean *true* then, if the server does not send a **Content-Type** header, or if it sends the value "application/octet-stream", **http::geturl** will attempt to guess appropriate values.  This is not intended to become a general-purpose tool, and currently it is limited to detecting XML documents that begin with an XML declaration.  In this case the **Content-Type** is changed to "application/xml", the binary flag state(binary) is changed to 0, and the character set is changed to the one specified by the "encoding" tag of the XML line, or to utf-8 if no encoding is specified.  Not used if a **-channel** is specified.

**-handler** *callback*
: If this option is absent, **http::geturl** processes incoming data itself, either appending it to the state(body) variable or writing it to the -channel. But if the **-handler** option is present, **http::geturl** does not do this processing and instead calls *callback*. Whenever HTTP data is available, the value of *callback* is expanded, an additional two arguments are added, and the resulting command is evaluated. The two additional arguments are: the socket for the HTTP data and the *token* returned from **::http::geturl**.  The token is the name of a global array that is described in the **STATE ARRAY** section.  The procedure is expected to return the number of bytes read from the socket.  Here is a template for the callback:

    ```
    proc httpHandlerCallback {socket token} {
        upvar 0 $token state
        # Access socket, and state as a Tcl array defined in this proc
        # For example...
        ...
        set data [read $socket 1000]
        set nbytes [string length $data]
        ...
        return $nbytes
    }
    ```
    The **http::geturl** code for the **-handler** option is not compatible with either compression or chunked transfer-encoding.  If **-handler** is specified, then to work around these issues **http::geturl** will reduce the HTTP protocol to 1.0, and override the **-zip** option (i.e. it will send the header **Accept-Encoding: identity** instead of **Accept-Encoding: gzip,deflate**).
    If options **-handler** and **-channel** are used together, the handler is responsible for copying the data from the HTTP socket to the specified channel.  The name of the channel is available to the handler as element **-channel** of the token array.
    The **::http::geturl** command runs the **-handler** callback inside a **catch** command.  Therefore an error in the callback command does not call the **bgerror** handler.  See the **ERRORS** section for details.

**-headers** *keyvaluelist*
: This option is used to add headers not already specified by **::http::config** to the HTTP request.  The *keyvaluelist* argument must be a list with an even number of elements that alternate between keys and values.  The keys become header field names.  Newlines are stripped from the values so the header cannot be corrupted.  For example, if *keyvaluelist* is **Pragma no-cache** then the following header is included in the HTTP request:

    ```
    Pragma: no-cache
    ```

**-keepalive** *boolean*
: If boolean **true**, attempt to keep the connection open for servicing multiple requests.  Default is 0.

**-method** *type*
: Force the HTTP request method to *type*. **::http::geturl** will auto-select GET, POST or HEAD based on other options, but this option overrides that selection and enables choices like PUT and DELETE for WebDAV support.
    It is the caller's responsibility to ensure that the headers and request body (if any) conform to the requirements of the request method.  For example, if using **-method** *POST* to send a POST with an empty request body, the caller must also supply the option

    ```
    \-headers {Content-Length 0}
    ```

**-myaddr** *address*
: Pass an specific local address to the underlying **socket** call in case multiple interfaces are available.

**-progress** *callback*
: If the **-progress** option is present, then the *callback* is made after each transfer of data from the URL. The value of *callback* is expanded, an additional three arguments are added, and the resulting command is evaluated. The three additional arguments are: the *token* returned from **::http::geturl**, the expected total size of the contents from the **Content-Length** response header, and the current number of bytes transferred so far.  The token is the name of a global array that is described in the **STATE ARRAY** section.  The expected total size may be unknown, in which case zero is passed to the callback.  Here is a template for the progress callback:

    ```
    proc httpProgress {token total current} {
        upvar 0 $token state
        # Access state as a Tcl array defined in this proc
        ...
        return
    }
    ```

**-protocol** *version*
: Select the HTTP protocol version to use. This should be 1.0 or 1.1 (the default). Should only be necessary for servers that do not understand or otherwise complain about HTTP/1.1.

**-query** *query*
: This flag (if the value is non-empty) causes **::http::geturl** to do a POST request that passes the string *query* verbatim to the server as the request payload. The content format (and encoding) of *query* is announced by the request header **Content-Type** which is set by the option **-type**.  Any value of **-type** is permitted, and it is the responsibility of the caller to supply *query* in the correct format.
    If **-type** is not specified, it defaults to *application/x-www-form-urlencoded*, which requires *query* to be an x-url-encoding formatted query-string (this **-type** and query format are used in a POST submitted from an html form).  The **::http::formatQuery** procedure can be used to do the formatting.

**-queryblocksize** *size*
: The block size used when posting query data to the URL. At most *size* bytes are written at once.  After each block, a call to the **-queryprogress** callback is made (if that option is specified).

**-querychannel** *channelID*
: This flag causes **::http::geturl** to do a POST request that passes the data contained in *channelID* to the server. The data contained in *channelID* must be an x-url-encoding formatted query unless the **-type** option below is used. If a **Content-Length** header is not specified via the **-headers** options, **::http::geturl** attempts to determine the size of the post data in order to create that header.  If it is unable to determine the size, it returns an error.

**-queryprogress** *callback*
: If the **-queryprogress** option is present, then the *callback* is made after each transfer of data to the URL in a POST request (i.e. a call to **::http::geturl** with option **-query** or **-querychannel**) and acts exactly like the **-progress** option (the callback format is the same).

**-strict** *boolean*
: If true then the command will test that the URL complies with RFC 3986, i.e. that it has no characters that should be "x-url-encoded" (e.g. a space should be encoded to "%20").  Default value is 1.

**-timeout** *milliseconds*
: If *milliseconds* is non-zero, then **::http::geturl** sets up a timeout to occur after the specified number of milliseconds. A timeout results in a call to **::http::reset** and to the **-command** callback, if specified. The return value of **::http::status** (and the value of the *status* key in the dictionary returned by **::http::responseInfo**) is **timeout** after a timeout has occurred.

**-type** *mime-type*
: Use *mime-type* as the **Content-Type** value, instead of the default value (**application/x-www-form-urlencoded**) during a POST operation.

**-validate** *boolean*
: If *boolean* is non-zero, then **::http::geturl** does an HTTP HEAD request.  This server returns the same status line and response headers as it would for a HTTP GET request, but omits the response entity (the URL "contents").  The response headers are available after the transaction using command **::http::responseHeaders** or, for selected information, **::http::responseInfo**.

**::http::formatQuery** *key value* ?*key value* ...?
: This procedure does x-url-encoding of query data.  It takes an even number of arguments that are the keys and values of the query.  It encodes the keys and values, and generates one string that has the proper & and = separators.  The result is suitable for the **-query** value passed to **::http::geturl**.

**::http::quoteString** *value*
: This procedure does x-url-encoding of string.  It takes a single argument and encodes it.

**::http::reset** *token* ?*why*?
: This command resets the HTTP transaction identified by *token*, if any. This sets the **state(status)** value to *why*, which defaults to **reset**, and then calls the registered **-command** callback.

**::http::wait** *token*
: This command blocks and waits for the transaction to complete.  This only works in trusted code because it uses **vwait**.  Also, it is not useful for the case where **::http::geturl** is called *without* the **-command** option because in this case the **::http::geturl** call does not return until the HTTP transaction is complete, and thus there is nothing to wait for.

**::http::status** *token*
: This command returns a description of the status of the HTTP transaction. The return value is the empty string until the HTTP transaction is completed; after completion it has one of the values ok, eof, error, timeout, and reset.  The meaning of these values is described in the section **ERRORS** (below).


The name "status" is not related to the terms "status line" and "status code" that are defined for a HTTP response.

**::http::size** *token*
: This command returns the number of bytes received so far from the URL in the **::http::geturl** call.

**::http::error** *token*
: This command returns the error information if the HTTP transaction failed, or the empty string if there was no error.  The information is a Tcl list of the error message, stack trace, and error code.

**::http::postError** *token*
: A POST request is a call to **::http::geturl** with either the **-query** or **-querychannel** option. The **::http::postError** command returns the error information generated when a HTTP POST request sends its request-body to the server; or the empty string if there was no error.  The information is a Tcl list of the error message, stack trace, and error code.  When this type of error occurs, the **::http::geturl** command continues the transaction and attempts to receive a response from the server.

**::http::cleanup** *token*
: This procedure cleans up the state associated with the connection identified by *token*.  After this call, the procedures like **::http::responseBody** cannot be used to get information about the operation.  It is *strongly* recommended that you call this function after you are done with a given HTTP request.  Not doing so will result in memory not being freed, and if your app calls **::http::geturl** enough times, the memory leak could cause a performance hit...or worse.

**::http::requestLine** *token*
: This command returns the "request line" sent to the server. The "request line" is the first line of a HTTP client request, and has three elements separated by spaces: the HTTP method, the URL relative to the server, and the HTTP version. Examples:


GET / HTTP/1.1 GET /introduction.html?subject=plumbing HTTP/1.1 POST /forms/order.html HTTP/1.1

**::http::requestHeaders** *token* ?*headerName*?
: This command returns the HTTP request header names and values, in the order that they were sent to the server, as a Tcl list of the form ?name value ...?  Header names are case-insensitive and are converted to lower case.  The return value is not a **dict** because some header names may occur more than once.  If one argument is supplied, all request headers are returned.  If two arguments are supplied, the second provides the value of a header name.  Only headers with the requested name (converted to lower case) are returned.  If no such headers are found, an empty list is returned.

**::http::requestHeaderValue** *token headerName*
: This command returns the value of the HTTP request header named *headerName*.  Header names are case-insensitive and are converted to lower case.  If no such header exists, the return value is the empty string. If there are multiple headers named *headerName*, the result is obtained by joining the individual values with the string ", " (comma and space), preserving their order.

**::http::responseLine** *token*
: This command returns the first line of the server response: the HTTP "status line".  The "status line" has three elements separated by spaces: the HTTP version, a three-digit numerical "status code", and a "reason phrase".  Only the reason phrase may contain spaces.  Examples:


HTTP/1.1 200 OK HTTP/1.0 404 Not Found The "status code" is a three-digit number in the range 100 to 599. A value of 200 is the normal return from a GET request, and its matching "reason phrase" is "OK".  Codes beginning with 4 or 5 indicate errors. Codes beginning with 3 are redirection errors.  In this case the **Location** response header specifies a new URL that contains the requested information.

The "reason phrase" is a textual description of the "status code": it may vary from server to server, and can be changed without affecting the HTTP protocol.  The recommended values (RFC 7231 and IANA assignments) for each code are provided by the command **::http::reasonPhrase**.

**::http::responseCode** *token*
: This command returns the "status code" (200, 404, etc.) of the server "status line".  If a three-digit code cannot be found, the full status line is returned.  See command **::http::responseLine** for more information on the "status line".

**::http::reasonPhrase** *code*
: This command returns the IANA recommended "reason phrase" for a particular "status code" returned by a HTTP server.  The argument *code* is a valid status code, and therefore is an integer in the range 100 to 599 inclusive. For numbers in this range with no assigned meaning, the command returns the value "Unassigned".  Several status codes are used only in response to the methods defined by HTTP extensions such as WebDAV, and not in response to a HEAD, GET, or POST request method.


The "reason phrase" returned by a HTTP server may differ from the recommended value, without affecting the HTTP protocol.  The value returned by **::http::geturl** can be obtained by calling either command **::http::responseLine** (which returns the full status line) or command **::http::responseInfo** (which returns a dictionary, with the "reason phrase" stored in key *reasonPhrase*).

A registry of valid status codes is maintained at https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml

**::http::responseHeaders** *token* ?*headerName*?
: The response from a HTTP server includes metadata headers that describe the response body and the transaction itself. This command returns the HTTP response header names and values, in the order that they were received from the server, as a Tcl list of the form ?name value ...?  Header names are case-insensitive and are converted to lower case.  The return value is not a **dict** because some header names may occur more than once, notably **Set-Cookie**.  If the second argument is not supplied, all response headers are returned.  If the second argument is supplied, it provides the value of a header name.  Only headers with the requested name (converted to lower case) are returned.  If no such headers are found, an empty list is returned.  See section **METADATA** for more information.

**::http::responseHeaderValue** *token headerName*
: This command returns the value of the HTTP response header named *headerName*.  Header names are case-insensitive and are converted to lower case.  If no such header exists, the return value is the empty string. If there are multiple headers named *headerName*, the result is obtained by joining the individual values with the string ", " (comma and space), preserving their order.  Multiple headers with the same name may be processed in this manner, except **Set-Cookie** which does not conform to the comma-separated-list syntax and cannot be combined into a single value. Each **Set-Cookie** header must be treated individually, e.g. by processing the return value of **::http::responseHeaders** *token* **Set-Cookie**.

**::http::responseInfo** *token*
: This command returns a **dict** of selected response metadata that are essential for identifying a successful transaction and making use of the response, along with other metadata that are informational.  The keys of the **dict** are *stage*, *status*, *responseCode*, *reasonPhrase*, *contentType*, *binary*, *redirection*, *upgrade*, *error*, *postError*, *method*, *charset*, *compression*, *httpRequest*, *httpResponse*, *url*, *connectionRequest*, *connectionResponse*, *connectionActual*, *transferEncoding*, *totalPost*, *currentPost*, *totalSize*, and *currentSize*.  The meaning of these keys is described in the section **METADATA** below.
    It is always worth checking the value of *binary* after a HTTP transaction, to determine whether a misconfigured server has caused http to interpret a text resource as a binary, or vice versa.
    After a POST transaction, check the value of *postError* to verify that the request body was uploaded without error.

**::http::responseBody** *token*
: This command returns the entity sent by the HTTP server (unless *-channel* was used, in which case the entity was delivered to the channel, and the command returns the empty string).
    Other terms for "entity", with varying precision, include "representation of resource", "resource", "response body after decoding", "payload", "message body after decoding", "content(s)", and "file".

**::http::register** *proto port command* ?*socketCmdVarName*? ?*useSockThread*? ?*endToEndProxy*?
: This procedure allows one to provide custom HTTP transport types such as HTTPS, by registering a prefix, the default port, and the command to execute to create the Tcl **channel**. The optional arguments configure how **http** uses the custom transport, and have default values that are compatible with older versions of **http** in which **::http::register** has no optional arguments.
    Argument *socketCmdVarName* is the name of a variable provided by the transport, whose value is the command used by the transport to open a socket.  Its default value is set by the transport and is "::socket", but if the name of the variable is supplied to **::http::register**, then **http** will set a new value in order to make optional facilities available.  These facilities are enabled by the optional arguments *useSockThread*, *endToEndProxy*, which take boolean values with default value *false*.
    Iff argument *useSockThread* is supplied and is boolean *true*, then iff permitted by the value [**http::config** *-threadlevel*] and by the availability of package **Thread**, sockets created for the transport will be opened in a different thread so that a slow DNS lookup will not cause the script to block.
    Iff argument *endToEndProxy* is supplied and is boolean *true*, then when **http::geturl** accesses a server via a proxy, it will open a channel by sending a CONNECT request to the proxy, and it will then make its request over this channel.  This allows end-to-end encryption for HTTPS requests made through a proxy.
    For example,

    ```
    package require http
    package require tls
    
    ::http::register https 443 ::tls::socket ::tls::socketCmd 1 1
    
    set token [::http::geturl https://my.secure.site/]
    ```

**::http::registerError** *sock* ?*message*?
: This procedure allows a registered protocol handler to deliver an error message for use by **http**.  Calling this command does not raise an error. The command is useful when a registered protocol detects an problem (for example, an invalid TLS certificate) that will cause an error to propagate to **http**.  The command allows **http** to provide a precise error message rather than a general one.  The command returns the value provided by the last call with argument *message*, or the empty string if no such call has been made.

**::http::unregister** *proto*
: This procedure unregisters a protocol handler that was previously registered via **::http::register**, returning a six-item list of the values that were previously supplied to **::http::register** if there was such a handler, and an error if there was no such handler.

**::http::code** *token*
: An alternative name for the command **::http::responseLine**

**::http::data** *token*
: An alternative name for the command **::http::responseBody**.

**::http::meta** *token* ?*headerName*?
: An alternative name for the command **::http::responseHeaders**

**::http::ncode** *token*
: An alternative name for the command **::http::responseCode**


# Errors

The **::http::geturl** procedure will raise errors in the following cases: invalid command line options, or an invalid URL. These errors mean that it cannot even start the network transaction. For synchronous **::http::geturl** calls (where **-command** is not specified), it will raise an error if the URL is on a non-existent host or at a bad port on an existing host. It will also raise an error for any I/O errors while writing out the HTTP request line and headers, or reading the HTTP reply headers or data.  Because **::http::geturl** does not return a token in these cases, it does all the required cleanup and there is no issue of your app having to call **::http::cleanup**.

For asynchronous **::http::geturl** calls, all of the above error situations apply, except that if there is any error while reading the HTTP reply headers or data, no exception is thrown.  This is because after writing the HTTP headers, **::http::geturl** returns, and the rest of the HTTP transaction occurs in the background.  The command callback can check if any error occurred during the read by calling **::http::responseInfo** to check the transaction status.

Alternatively, if the main program flow reaches a point where it needs to know the result of the asynchronous HTTP request, it can call **::http::wait** and then check status and error, just as the synchronous call does.

The **::http::geturl** command runs the **-command**, **-handler**, and **-proxyfilter** callbacks inside a **catch** command.  Therefore an error in the callback command does not call the **bgerror** handler. When debugging one of these callbacks, it may be convenient to report errors by using a **catch** command within the callback command itself, e.g. to write an error message to stdout.

In any case, you must still call **::http::cleanup** to delete the state array when you are done.

There are other possible results of the HTTP transaction determined by examining the status from **::http::status** (or the value of the *status* key in the dictionary returned by **::http::responseInfo**). These are described below.

**ok**
: If the HTTP transaction completes entirely, then status will be **ok**. However, you should still check the **::http::responseLine** value to get the HTTP status.  The **::http::responseCode** procedure provides just the numeric error (e.g., 200, 404 or 500) while the **::http::responseLine** procedure returns a value like "HTTP 404 File not found".

**eof**
: If the server closes the socket without replying, then no error is raised, but the status of the transaction will be **eof**.

**error**
: The error message, stack trace, and error code are accessible via **::http::error**.  The error message is also provided by the value of the *error* key in the dictionary returned by **::http::responseInfo**.

**timeout**
: A timeout occurred before the transaction could complete.

**reset**
: The user has called **::http::reset**.

**""**
: (empty string) The transaction has not yet finished.


Another error possibility is that **::http::geturl** failed to write the whole of the POST request body (**-query** or **-querychannel** data) to the server.  **::http::geturl** stores the error message for later retrieval by the **::http::postError** or **::http::responseInfo** commands, and then attempts to complete the transaction. If it can read the server's response the status will be **ok**, but it is important to call **::http::postError** or **::http::responseInfo** after every POST to check that the data was sent in full. If the server has closed the connection the status will be **eof**.

# Metadata

## Most useful metadata

When a HTTP server responds to a request, it supplies not only the entity requested, but also metadata.  This is provided by the first line (the "status line") of the response, and by a number of HTTP headers.  Further metadata relates to how **::http::geturl** has processed the response from the server.

The most important metadata can be accessed with the command **::http::responseInfo**. This command returns a **dict** of metadata that are essential for identifying a successful transaction and making use of the response, along with other metadata that are informational.  The keys of the **dict** are:

**===== Essential Values =====**

**stage**
: This value, set by **::http::geturl**, describes the stage that the transaction has reached. Values, in order of the transaction lifecycle, are: "created", "connecting", "header", "body", and "complete".  The other **dict** keys will not be available until the value of **stage** is "body" or "complete".  The key **currentSize** has its final value only when **stage** is "complete".

**status**
: This value, set by **::http::geturl**, is "ok" for a successful transaction; "eof", "error", "timeout", or "reset" for an unsuccessful transaction; or "" if the transaction is still in progress.  The value is the same as that returned by command **::http::status**. The meaning of these values is described in the section **ERRORS** (above).

**responseCode**
: The "HTTP status code" sent by the server in the first line (the "status line") of the response.  If the value cannot be extracted from the status line, the full status line is returned.

**reasonPhrase**
: The "reason phrase" sent by the server as a description of the HTTP status code. If the value cannot be extracted from the status line, the full status line is returned.

**contentType**
: The value of the **Content-Type** response header or, if the header was not supplied, the default value "application/octet-stream".

**binary**
: This boolean value, set by **::http::geturl**, describes how the command has interpreted the entity returned by the server (after decoding any compression specified by the **Content-Encoding** response header). This decoded entity is accessible as the return value of the command **::http::responseBody**.


The value is **true** if http has interpreted the decoded entity as binary. The value returned by **::http::responseBody** is a Tcl binary string. This is a suitable format for image data, zip files, etc. **::http::geturl** chooses this value if the user has requested a binary interpretation by passing the option **-binary** to the command, or if the server has supplied a binary content type in a **Content-Type** response header, or if the server has not supplied any **Content-Type** header.

The value is **false** in other cases, and this means that http has interpreted the decoded entity as text. The text has been converted, from the character set notified by the server, into Tcl's internal Unicode format; the value returned by **::http::responseBody** is an ordinary Tcl string.

It is always worth checking the value of "binary" after a HTTP transaction, to determine whether a misconfigured server has caused http to interpret a text resource as a binary, or vice versa.

**redirection**
: The URL that is the redirection target. The value is that of the **Location** response header.  This header is sent when a response has status code 3XX (redirection).

**upgrade**
: If not empty, the value indicates the protocol(s) to which the server will switch after completion of this transaction, while continuing to use the same connection.  When the server intends to switch protocols, it will also send the value "101" as the status code (the **responseCode** key), and the word "upgrade" as an element of the **Connection** response header (the **connectionResponse** key), and it will not send a response body. See the section **PROTOCOL UPGRADES** for more information.

**error**
: The error message, if there is one.  Further information, including a stack trace and error code, are available from command **::http::error**.

**postError**
: The error message (if any) generated when a HTTP POST request sends its request-body to the server.  Further information, including a stack trace and error code, are available from command **::http::postError**.  A POST transaction may appear complete, according to the keys **stage**, **status**, and **responseCode**, but it is important to check this **postError** key in case an error occurred when uploading the request-body.


**===== Informational Values =====**

**method**
: The HTTP method used in the request.

**charset**
: The value of the charset attribute of the **Content-Type** response header. The charset value is used only for a text resource.  If the server did not specify a charset, the value defaults to that of the variable **::http::defaultCharset**, which unless it has been deliberately modified by the caller is **iso8859-1**.  Incoming text data is automatically converted from the character set defined by **charset** to Tcl's internal Unicode representation, i.e. to a Tcl string.

**compression**
: A copy of the **Content-Encoding** response-header value.

**httpRequest**
: The version of HTTP specified in the request (i.e. sent in the request line). The value is that of the option **-protocol** supplied to **::http::geturl** (default value "1.1"), unless the command reduced the value to "1.0" because it was passed the **-handler** option.

**httpResponse**
: The version of HTTP used by the server (obtained from the response "status line").  The server uses this version of HTTP in its response, but ensures that this response is compatible with the HTTP version specified in the client's request.  If the value cannot be extracted from the status line, the full status line is returned.

**url**
: The requested URL, typically the URL supplied as an argument to **::http::geturl** but without its "fragment" (the final part of the URL beginning with "#").

**connectionRequest**
: The value, if any, sent to the server in **Connection** request header(s).

**connectionResponse**
: The value, if any, received from the server in **Connection** response header(s).

**connectionActual**
: This value, set by **::http::geturl**, reports whether the connection was closed after the transaction (value "close"), or left open (value "keep-alive").

**transferEncoding**
: The value of the Transfer-Encoding response header, if it is present. The value is either "chunked" (indicating HTTP/1.1 "chunked encoding") or the empty string.

**totalPost**
: The total length of the request body in a POST request.

**currentPost**
: The number of bytes of the POST request body sent to the server so far. The value is the same as that returned by command **::http::size**.

**totalSize**
: A copy of the **Content-Length** response-header value. The number of bytes specified in a **Content-Length** header, if one was sent.  If none was sent, the value is 0.  A correctly configured server omits this header if the transfer-encoding is "chunked", or (for older servers) if the server closes the connection when it reaches the end of the resource.

**currentSize**
: The number of bytes fetched from the server so far.


## More metadata

The dictionary returned by **::http::responseInfo** is the most useful subset of the available metadata.  Other metadata include:

1. The full "status line" of the response, available as the return value of command **::http::responseLine**.

2. The full response headers, available as the return value of command **::http::responseHeaders**.  This return value is a list of the response-header names and values, in the order that they were received from the server.

The return value is not a **dict** because some header names may occur more than once, notably **Set-Cookie**. If the value is read into a **dict** or into an array (using array set), only the last header with each name will be preserved.

Some of the header names (metadata keys) are listed below, but the HTTP standard defines several more, and servers are free to add their own. When a dictionary key is mentioned below, this refers to the **dict** value returned by command **::http::responseInfo**.

**Content-Type**
: The content type of the URL contents.  Examples include **text/html**, **image/gif,** **application/postscript** and **application/x-tcl**.  Text values typically specify a character set, e.g. **text/html; charset=UTF-8**.  Dictionary key *contentType*.

**Content-Length**
: The advertised size in bytes of the contents, available as dictionary key *totalSize*.  The actual number of bytes read by **::http::geturl** so far is available as dictionary key **currentSize**.

**Content-Encoding**
: The compression algorithm used for the contents. Examples include **gzip**, **deflate**. Dictionary key *content*.

**Location**
: This header is sent when a response has status code 3XX (redirection). It provides the URL that is the redirection target. Dictionary key *redirection*.

**Set-Cookie**
: This header is sent to offer a cookie to the client.  Cookie management is done by the **::http::config** option **-cookiejar**, and so the **Set-Cookie** headers need not be parsed by user scripts. See section **COOKIE JAR PROTOCOL**.

**Connection**
: The value can be supplied as a comma-separated list, or by multiple headers. The list often has only one element, either "close" or "keep-alive". The value "upgrade" indicates a successful upgrade request and is typically combined with the status code 101, an **Upgrade** response header, and no response body.  Dictionary key *connectionResponse*.

**Upgrade**
: The value indicates the protocol(s) to which the server will switch immediately after the empty line that terminates the 101 response headers. Dictionary key *upgrade*.


## Even more metadata

1.
: Details of the HTTP request.  The request is determined by the options supplied to **::http::geturl** and **::http::config**.  However, it is sometimes helpful to examine what **::http::geturl** actually sent to the server, and this information is available through commands **::http::requestHeaders** and **::http::requestLine**.

2.
: The state array: the internal variables of **::http::geturl**. It may sometimes be helpful to examine this array. Details are given in the next section.


# State array

The **::http::geturl** procedure returns a *token* that can be used as an argument to other **::http::*** commands, which examine and manage the state of the HTTP transaction.  For most purposes these commands are sufficient.  The *token* can also be used to access the internal state of the transaction, which is stored in a Tcl array. This facility is most useful when writing callback commands for the options **-command**, **-handler**, **-progress**, or **-queryprogress**. Use the following command inside the proc to define an easy-to-use array *state* as a local variable within the proc

```
upvar 0 $token state
```

Once the data associated with the URL is no longer needed, the state array should be unset to free up storage. The **::http::cleanup** procedure is provided for that purpose.

The following elements of the array are supported, and are the origin of the values returned by commands as described below.  When a dictionary key is mentioned below, this refers to the **dict** value returned by command **::http::responseInfo**.

**binary**
: For dictionary key *binary*.

**body**
: For command **::http::responseBody**.

**charset**
: For dictionary key *charset*.

**coding**
: For dictionary key *compression*.

**connection**
: For dictionary key *connectionActual*.

**currentsize**
: For command **::http::size**; and for dictionary key *currentSize*.

**error**
: For command **::http::error**; part is used in dictionary key *error*.

**http**
: For command **::http::responseLine**.

**httpResponse**
: For dictionary key *httpResponse*.

**meta**
: For command **::http::responseHeaders**. Further discussion above in the section **MORE METADATA**.

**method**
: For dictionary key *method*.

**posterror**
: For dictionary key *postError*.

**postErrorFull**
: For command **::http::postError**.

**-protocol**
: For dictionary key *httpRequest*.

**querylength**
: For dictionary key *totalPost*.

**queryoffset**
: For dictionary key *currentPost*.

**reasonPhrase**
: For dictionary key *reasonPhrase*.

**requestHeaders**
: For command **::http::requestHeaders**.

**requestLine**
: For command **::http::requestLine**.

**responseCode**
: For dictionary key *responseCode*.

**state**
: For dictionary key *stage*.

**status**
: For command **::http::status**; and for dictionary key *status*.

**totalsize**
: For dictionary key *totalSize*.

**transfer**
: For dictionary key *transferEncoding*.

**type**
: For dictionary key *contentType*.

**upgrade**
: For dictionary key *upgrade*.

**url**
: For dictionary key *url*.


# Persistent connections

## Basics

See RFC 7230 Sec 6, which supersedes RFC 2616 Sec 8.1.

A persistent connection allows multiple HTTP/1.1 transactions to be carried over the same TCP connection.  Pipelining allows a client to make multiple requests over a persistent connection without waiting for each response.  The server sends responses in the same order that the requests were received.

If a POST request fails to complete, typically user confirmation is needed before sending the request again.  The user may wish to verify whether the server was modified by the failed POST request, before sending the same request again.

A HTTP request will use a persistent socket if the call to **http::geturl** has the option **-keepalive true**. It will use pipelining where permitted if the **http::config** option **-pipeline** is boolean **true** (its default value).

The http package maintains no more than one persistent connection to each server (i.e. each value of "domain:port"). If **http::geturl** is called to make a request over a persistent connection while the connection is busy with another request, the new request will be held in a queue until the connection is free.

The http package does not support HTTP/1.0 persistent connections controlled by the **Keep-Alive** header.

## Special cases

This subsection discusses issues related to closure of the persistent connection by the server, automatic retry of failed requests, the special treatment necessary for POST requests, and the options for dealing with these cases.

In accordance with RFC 7230, **http::geturl** does not pipeline requests that use the POST method.  If a POST uses a persistent connection and is not the first request on that connection, **http::geturl** waits until it has received the response for the previous request; or (if **http::config** option **-postfresh** is boolean **true**) it uses a new connection for each POST.

If the server is processing a number of pipelined requests, and sends a response header "**Connection: close**" with one of the responses (other than the last), then subsequent responses are unfulfilled. **http::geturl** will send the unfulfilled requests again over a new connection.

A difficulty arises when a HTTP client sends a request over a persistent connection that has been idle for a while.  The HTTP server may half-close an apparently idle connection while the client is sending a request, but before the request arrives at the server: in this case (an "asynchronous close event") the request will fail.  The difficulty arises because the client cannot be certain whether the POST modified the state of the server.  For HEAD or GET requests, **http::geturl** opens another connection and retransmits the failed request. However, if the request was a POST, RFC 7230 forbids automatic retry by default, suggesting either user confirmation, or confirmation by user-agent software that has semantic understanding of the application.  The **http::config** option **-repost** allows for either possibility.

Asynchronous close events can occur only in a short interval of time.  The **http** package monitors each persistent connection for closure by the server.  Upon detection, the connection is also closed at the client end, and subsequent requests will use a fresh connection.

If the **http::geturl** command is called with option **-keepalive true**, then it will both try to use an existing persistent connection (if one is available), and it will send the server a "**Connection: keep-alive**" request header asking to keep the connection open for future requests.

The **http::config** options **-pipeline**, **-postfresh**, and **-repost** relate to persistent connections.

Option **-pipeline**, if boolean **true**, will pipeline GET and HEAD requests made over a persistent connection.  POST requests will not be pipelined - if the POST is not the first transaction on the connection, its request will not be sent until the previous response has finished.  GET and HEAD requests made after a POST will not be sent until the POST response has been delivered, and will not be sent if the POST fails.

Option **-postfresh**, if boolean **true**, will override the **http::geturl** option **-keepalive**, and always open a fresh connection for a POST request.

Option **-repost**, if **true**, permits automatic retry of a POST request that fails because it uses a persistent connection that the server has half-closed (an "asynchronous close event"). Subsequent GET and HEAD requests in a failed pipeline will also be retried. *The* **-repost** *option should be used only if the application understands that the retry is appropriate* - specifically, the application must know that if the failed POST successfully modified the state of the server, a repeat POST would have no adverse effect.

# Cookie jar protocol

Cookies are short key-value pairs used to implement sessions within the otherwise-stateless HTTP protocol. (See RFC 6265 for details; Tcl does not implement the Cookie2 protocol as that is rarely seen in the wild.)

Cookie storage management commands \(em "cookie jars" \(em must support these subcommands which form the HTTP cookie storage management protocol. Note that *cookieJar* below does not have to be a command name; it is properly a command prefix (a Tcl list of words that will be expanded in place) and admits many possible implementations.

Though not formally part of the protocol, it is expected that particular values of *cookieJar* will correspond to sessions; it is up to the caller of **::http::config** to decide what session applies and to manage the deletion of said sessions when they are no longer desired (which should be when they not configured as the current cookie jar).

*cookieJar* **getCookies** *protocol host requestPath*
: This command asks the cookie jar what cookies should be supplied for a particular request. It should take the *protocol* (typically **http** or **https**), *host* name and *requestPath* (parsed from the *url* argument to **::http::geturl**) and return a list of cookie keys and values that describe the cookies to supply to the remote host. The list must have an even number of elements.
    There should only ever be at most one cookie with a particular key for any request (typically the one with the most specific *host*/domain match and most specific *requestPath*/path match), but there may be many cookies with different names in any request.

*cookieJar* **storeCookie** *cookieDictionary*
: This command asks the cookie jar to store a particular cookie that was returned by a request; the result of this command is ignored. The cookie (which will have been parsed by the http package) is described by a dictionary, *cookieDictionary*, that may have the following keys:

**domain**
: This is always present. Its value describes the domain hostname *or prefix* that the cookie should be returned for.  The checking of the domain against the origin (below) should be careful since sites that issue cookies should only do so for domains related to themselves. Cookies that do not obey a relevant origin matching rule should be ignored.

**expires**
: This is optional. If present, the cookie is intended to be a persistent cookie and the value of the option is the Tcl timestamp (in seconds from the same base as **clock seconds**) of when the cookie expires (which may be in the past, which should result in the cookie being deleted immediately). If absent, the cookie is intended to be a session cookie that should be not persisted beyond the lifetime of the cookie jar.

**hostonly**
: This is always present. Its value is a boolean that describes whether the cookie is a single host cookie (true) or a domain-level cookie (false).

**httponly**
: This is always present. Its value is a boolean that is true when the site wishes the cookie to only ever be used with HTTP (or HTTPS) traffic.

**key**
: This is always present. Its value is the *key* of the cookie, which is part of the information that must be return when sending this cookie back in a future request.

**origin**
: This is always present. Its value describes where the http package believes it received the cookie from, which may be useful for checking whether the cookie's domain is valid.

**path**
: This is always present. Its value describes the path prefix of requests to the cookie domain where the cookie should be returned.

**secure**
: This is always present. Its value is a boolean that is true when the cookie should only used on requests sent over secure channels (typically HTTPS).

**value**
: This is always present. Its value is the value of the cookie, which is part of the information that must be return when sending this cookie back in a future request.

    Other keys may always be ignored; they have no meaning in this protocol.


# Protocol upgrades

The HTTP/1.1 **Connection** and **Upgrade** request headers inform the server that the client wishes to change the protocol used over the existing connection (RFC 7230). This mechanism can be used to request a WebSocket (RFC 6455), a higher version of the HTTP protocol (HTTP 2), or TLS encryption.  If the server accepts the upgrade request, its response code will be 101.

To request a protocol upgrade when calling **http::geturl**, the **-headers** option must supply appropriate values for **Connection** and **Upgrade**, and the **-command** option must supply a command that implements the requested protocol and can also handle the server response if the server refuses the protocol upgrade.  For upgrade requests **http::geturl** ignores the value of option **-keepalive**, and always uses the value **0** so that the upgrade request is not made over a connection that is intended for multiple HTTP requests.

The Tcllib library **websocket** implements WebSockets, and makes the necessary calls to commands in the **http** package.

There is currently no native Tcl client library for HTTP/2.

The **Upgrade** mechanism is not used to request TLS in web browsers, because **http** and **https** are served over different ports.  It is used by protocols such as Internet Printing Protocol (IPP) that are built on top of **http(s)** and use the same TCP port number for both secure and insecure traffic.

In browsers, opportunistic encryption is instead implemented by the **Upgrade-Insecure-Requests** client header.  If a secure service is available, the server response code is a 307 redirect, and the response header **Location** specifies the target URL.  The browser must call **http::geturl** again in order to fetch this URL. See https://w3c.github.io/webappsec-upgrade-insecure-requests/

# Threads

## Purpose

Command **::http::geturl** uses the Tcl **::socket** command with the **-async** option to connect to a remote server, but the return from this command can be delayed in adverse cases (e.g. a slow DNS lookup), preventing the event loop from processing other events. This delay is avoided if the **::socket** command is evaluated in another thread.  The Thread package is not part of Tcl but is provided in "Batteries Included" distributions.  Instead of the **::socket** command, the http package uses **::http::socket** which makes connections in the manner specified by the value of **-threadlevel** and the availability of package Thread.

## With tls (https)

The same **-threadlevel** configuration applies to both HTTP and HTTPS connections. HTTPS is enabled by using the **http::register** command, typically by specifying the **::tls::socket** command of the tls package to handle TLS cryptography.  The **::tls::socket** command connects to the remote server by using the command specified by the value of variable **::tls::socketCmd**, and this value defaults to "::socket".  If http::geturl finds that **::tls::socketCmd** has this value, it replaces it with the value "::http::socket".  If **::tls::socketCmd** has a value other than "::socket", i.e. if the script or the Tcl installation has replaced the value "::socket" with the name of a different command, then http does not change the value. The script or installation that modified **::tls::socketCmd** is responsible for integrating **::http::socket** into its own replacement command.

## With a child interpreter

The peer thread can transfer the socket only to the main interpreter of the script's thread.  Therefore the thread-based **::http::socket** works with non-zero **-threadlevel** values only if the script runs in the main interpreter.  A child interpreter must use **-threadlevel 0** unless the parent interpreter has provided alternative facilities.  The main parent interpreter may grant full **-threadlevel** facilities to a child interpreter, for example by aliasing, to **::http::socket** in the child, a command that runs **http::socket** in the parent, and then transfers the socket to the child.

# Example

This example creates a procedure to copy a URL to a file while printing a progress meter, and prints the response headers associated with the URL.

```
proc httpcopy { url file {chunk 4096} } {
    set out [open $file w]
    set token [::http::geturl $url -channel $out \
            -progress httpCopyProgress -blocksize $chunk]
    close $out

    # This ends the line started by httpCopyProgress
    puts stderr ""

    upvar 0 $token state
    set max 0
    foreach {name value} $state(meta) {
        if {[string length $name] > $max} {
            set max [string length $name]
        }
        if {[regexp -nocase ^location$ $name]} {
            # Handle URL redirects
            puts stderr "Location:$value"
            return [httpcopy [string trim $value] $file $chunk]
        }
    }
    incr max
    foreach {name value} $state(meta) {
        puts [format "%-*s %s" $max $name: $value]
    }

    return $token
}
proc httpCopyProgress {args} {
    puts -nonewline stderr .
    flush stderr
}
```

