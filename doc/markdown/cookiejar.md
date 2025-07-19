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

The cookiejar package provides an implementation of the http package's cookie jar protocol using an SQLite database. It provides one main command, **::http::cookiejar**, which is a TclOO class that should be instantiated to create a cookie jar that manages a particular HTTP session.

The database management policy can be controlled at the package level by the **configure** method on the **::http::cookiejar** class object:

**::http::cookiejar configure** ?*optionName*? ?*optionValue*?
: If neither *optionName* nor *optionValue* are supplied, this returns a copy of the configuration as a Tcl dictionary. If just *optionName* is supplied, just the value of the named option is returned. If both *optionName* and *optionValue* are given, the named option is changed to be the given value.
    Supported options are:

**-domainfile** *filename*
: A file (defaulting to within the cookiejar package) with a description of the list of top-level domains (e.g., **.com** or **.co.jp**). Such domains *must not* accept cookies set upon them. Note that the list of such domains is both security-sensitive and *not* constant and should be periodically refetched. Cookie jars maintain their own cache of the domain list.

**-domainlist** *url*
: A URL to fetch the list of top-level domains (e.g., **.com** or **.co.jp**) from.  Such domains *must not* accept cookies set upon them. Note that the list of such domains is both security-sensitive and *not* constant and should be periodically refetched. Cookie jars maintain their own cache of the domain list.

**-domainrefresh** *intervalMilliseconds*
: The number of milliseconds between checks of the **-domainlist** for new domains.

**-loglevel** *level*
: The logging level of this package. The logging level must be (in order of decreasing verbosity) one of **debug**, **info**, **warn**, or **error**.

**-offline** *flag*
: Allows the cookie management engine to be placed into offline mode. In offline mode, the list of domains is read immediately from the file configured in the **-domainfile** option, and the **-domainlist** option is not used; it also makes the **-domainrefresh** option be effectively ignored.

**-purgeold** *intervalMilliseconds*
: The number of milliseconds between checks of the database for expired cookies; expired cookies are deleted.

**-retain** *cookieCount*
: The maximum number of cookies to retain in the database.

**-vacuumtrigger** *deletionCount*
: A count of the number of persistent cookie deletions to go between vacuuming the database.



Cookie jar instances may be made with any of the standard TclOO instance creation methods (**create** or **new**).

**::http::cookiejar new** ?*filename*?
: If a *filename* argument is provided, it is the name of a file containing an SQLite database that will contain the persistent cookies maintained by the cookie jar; the database will be created if the file does not already exist. If *filename* is not supplied, the database will be held entirely within memory, which effectively forces all cookies within it to be session cookies.


## Instance methods

The following methods are supported on the instances:

*cookiejar* **destroy**
: This is the standard TclOO destruction method. It does *not* delete the SQLite database if it is written to disk. Callers are responsible for ensuring that the cookie jar is not in use by the http package at the time of destruction.

*cookiejar* **forceLoadDomainData**
: This method causes the cookie jar to immediately load (and cache) the domain list data. The domain list will be loaded from the **-domainlist** configured a the package level if that is enabled, and otherwise will be obtained from the **-domainfile** configured at the package level.

*cookiejar* **getCookies** *protocol host path*
: This method obtains the cookies for a particular HTTP request. *This implements the http cookie jar protocol.*

*cookiejar* **policyAllow** *operation domain path*
: This method is called by the **storeCookie** method to get a decision on whether to allow *operation* to be performed for the *domain* and *path*. This is checked immediately before the database is updated but after the built-in security checks are done, and should return a boolean value; if the value is false, the operation is rejected and the database is not modified. The supported *operation*s are:

**delete**
: The *domain* is seeking to delete a cookie.

**session**
: The *domain* is seeking to create or update a session cookie.

**set**
: The *domain* is seeking to create or update a persistent cookie (with a defined lifetime).

    The default implementation of this method just returns true, but subclasses of this class may impose their own rules.

*cookiejar* **storeCookie** *options*
: This method stores a single cookie from a particular HTTP response. Cookies that fail security checks are ignored. *This implements the http cookie jar protocol.*

*cookiejar* **lookup** ?*host*? ?*key*?
: This method looks a cookie by exact host (or domain) matching. If neither *host* nor *key* are supplied, the list of hosts for which a cookie is stored is returned. If just *host* (which may be a hostname or a domain name) is supplied, the list of cookie keys stored for that host is returned. If both *host* and *key* are supplied, the value for that key is returned; it is an error if no such host or key match exactly.


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

