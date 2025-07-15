---
CommandName: cookiejar
ManualSection: n
Version: 0.1
TclPart: http
TclDescription: Tcl Bundled Packages
Links:
 - http(n)
 - oo::class(n)
 - sqlite3(n)
Keywords:
 - cookie
 - internet
 - security policy
 - www
Copyright:
 - Copyright (c) 2014-2018 Donal K. Fellows.
---

# Name

cookiejar - Implementation of the Tcl http package cookie jar protocol

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [cookiejar]{.lit} [0.1]{.optlit}

[::http::cookiejar]{.cmd} [configure]{.sub} [optionName]{.optarg} [optionValue]{.optarg}
[::http::cookiejar]{.cmd} [create]{.sub} [name]{.arg} [filename]{.optarg}
[::http::cookiejar]{.cmd} [new]{.sub} [filename]{.optarg}

[cookiejar]{.ins} [destroy]{.sub}
[cookiejar]{.ins} [forceLoadDomainData]{.sub}
[cookiejar]{.ins} [getCookies]{.sub} [protocol]{.arg} [host]{.arg} [path]{.arg}
[cookiejar]{.ins} [storeCookie]{.sub} [options]{.arg}
[cookiejar]{.ins} [lookup]{.sub} [host]{.optarg} [key]{.optarg}
:::

# Description

The cookiejar package provides an implementation of the http package's cookie jar protocol using an SQLite database. It provides one main command, \fB::http::cookiejar\fR, which is a TclOO class that should be instantiated to create a cookie jar that manages a particular HTTP session.

The database management policy can be controlled at the package level by the \fBconfigure\fR method on the \fB::http::cookiejar\fR class object:

**::http::cookiejar configure** ?\fIoptionName\fR? ?\fIoptionValue\fR?
: If neither \fIoptionName\fR nor \fIoptionValue\fR are supplied, this returns a copy of the configuration as a Tcl dictionary. If just \fIoptionName\fR is supplied, just the value of the named option is returned. If both \fIoptionName\fR and \fIoptionValue\fR are given, the named option is changed to be the given value.
    Supported options are:

**-domainfile** \fIfilename\fR
: A file (defaulting to within the cookiejar package) with a description of the list of top-level domains (e.g., \fB.com\fR or \fB.co.jp\fR). Such domains \fImust not\fR accept cookies set upon them. Note that the list of such domains is both security-sensitive and \fInot\fR constant and should be periodically refetched. Cookie jars maintain their own cache of the domain list.

**-domainlist** \fIurl\fR
: A URL to fetch the list of top-level domains (e.g., \fB.com\fR or \fB.co.jp\fR) from.  Such domains \fImust not\fR accept cookies set upon them. Note that the list of such domains is both security-sensitive and \fInot\fR constant and should be periodically refetched. Cookie jars maintain their own cache of the domain list.

**-domainrefresh** \fIintervalMilliseconds\fR
: The number of milliseconds between checks of the \fB-domainlist\fR for new domains.

**-loglevel** \fIlevel\fR
: The logging level of this package. The logging level must be (in order of decreasing verbosity) one of \fBdebug\fR, \fBinfo\fR, \fBwarn\fR, or \fBerror\fR.

**-offline** \fIflag\fR
: Allows the cookie management engine to be placed into offline mode. In offline mode, the list of domains is read immediately from the file configured in the \fB-domainfile\fR option, and the \fB-domainlist\fR option is not used; it also makes the \fB-domainrefresh\fR option be effectively ignored.

**-purgeold** \fIintervalMilliseconds\fR
: The number of milliseconds between checks of the database for expired cookies; expired cookies are deleted.

**-retain** \fIcookieCount\fR
: The maximum number of cookies to retain in the database.

**-vacuumtrigger** \fIdeletionCount\fR
: A count of the number of persistent cookie deletions to go between vacuuming the database.



Cookie jar instances may be made with any of the standard TclOO instance creation methods (\fBcreate\fR or \fBnew\fR).

**::http::cookiejar new** ?\fIfilename\fR?
: If a \fIfilename\fR argument is provided, it is the name of a file containing an SQLite database that will contain the persistent cookies maintained by the cookie jar; the database will be created if the file does not already exist. If \fIfilename\fR is not supplied, the database will be held entirely within memory, which effectively forces all cookies within it to be session cookies.


## Instance methods

The following methods are supported on the instances:

*cookiejar* \fBdestroy\fR
: This is the standard TclOO destruction method. It does \fInot\fR delete the SQLite database if it is written to disk. Callers are responsible for ensuring that the cookie jar is not in use by the http package at the time of destruction.

*cookiejar* \fBforceLoadDomainData\fR
: This method causes the cookie jar to immediately load (and cache) the domain list data. The domain list will be loaded from the \fB-domainlist\fR configured a the package level if that is enabled, and otherwise will be obtained from the \fB-domainfile\fR configured at the package level.

*cookiejar* \fBgetCookies\fR \fIprotocol host path\fR
: This method obtains the cookies for a particular HTTP request. \fIThis implements the http cookie jar protocol.\fR

*cookiejar* \fBpolicyAllow\fR \fIoperation domain path\fR
: This method is called by the \fBstoreCookie\fR method to get a decision on whether to allow \fIoperation\fR to be performed for the \fIdomain\fR and \fIpath\fR. This is checked immediately before the database is updated but after the built-in security checks are done, and should return a boolean value; if the value is false, the operation is rejected and the database is not modified. The supported \fIoperation\fRs are:

**delete**
: The \fIdomain\fR is seeking to delete a cookie.

**session**
: The \fIdomain\fR is seeking to create or update a session cookie.

**set**
: The \fIdomain\fR is seeking to create or update a persistent cookie (with a defined lifetime).

    The default implementation of this method just returns true, but subclasses of this class may impose their own rules.

*cookiejar* \fBstoreCookie\fR \fIoptions\fR
: This method stores a single cookie from a particular HTTP response. Cookies that fail security checks are ignored. \fIThis implements the http cookie jar protocol.\fR

*cookiejar* \fBlookup\fR ?\fIhost\fR? ?\fIkey\fR?
: This method looks a cookie by exact host (or domain) matching. If neither \fIhost\fR nor \fIkey\fR are supplied, the list of hosts for which a cookie is stored is returned. If just \fIhost\fR (which may be a hostname or a domain name) is supplied, the list of cookie keys stored for that host is returned. If both \fIhost\fR and \fIkey\fR are supplied, the value for that key is returned; it is an error if no such host or key match exactly.


# Examples

The simplest way of using a cookie jar is to just permanently configure it at the start of the application.

```
package require http
package require cookiejar

set cookiedb [file join [file home] cookiejar]
http::config -cookiejar [http::cookiejar new $cookiedb]

# No further explicit steps are required to use cookies
set tok [http::geturl http://core.tcl-lang.org/]
```

To only allow a particular domain to use cookies, perhaps because you only want to enable a particular host to create and manipulate sessions, create a subclass that imposes that policy.

```
package require http
package require cookiejar

oo::class create MyCookieJar {
    superclass http::cookiejar

    method policyAllow {operation domain path} {
        return [expr {$domain eq "my.example.com"}]
    }
}

set cookiedb [file join [file home] cookiejar]
http::configure -cookiejar [MyCookieJar new $cookiedb]

# No further explicit steps are required to use cookies
set tok [http::geturl http://core.tcl-lang.org/]
```

