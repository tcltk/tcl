---
CommandName: idna
ManualSection: n
Version: 0.1
TclPart: http
TclDescription: Tcl Bundled Packages
Links:
 - http(n)
 - cookiejar(n)
Keywords:
 - internet
 - www
Copyright:
 - Copyright (c) 2014-2018 Donal K. Fellows.
---

# Name

tcl::idna - Support for normalization of Internationalized Domain Names

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl::idna]{.lit} [1.0]{.lit}

[tcl::idna]{.cmd} [decode]{.sub} [hostname]{.arg}
[tcl::idna]{.cmd} [encode]{.sub} [hostname]{.arg}
[tcl::idna]{.cmd} [puny]{.sub} [decode]{.lit} [string]{.arg} [case]{.optarg}
[tcl::idna]{.cmd} [puny]{.sub} [encode]{.lit} [string]{.arg} [case]{.optarg}
[tcl::idna]{.cmd} [version]{.sub}
:::

# Description

This package provides an implementation of the punycode scheme used in Internationalised Domain Names, and some access commands. (See RFC 3492 for a description of punycode.)

**tcl::idna decode** *hostname*
: This command takes the name of a host that potentially contains punycode-encoded character sequences, *hostname*, and returns the hostname as might be displayed to the user. Note that there are often UNICODE characters that have extremely similar glyphs, so care should be taken with displaying hostnames to users.

**tcl::idna encode** *hostname*
: This command takes the name of a host as might be displayed to the user, *hostname*, and returns the version of the hostname with characters not permitted in basic hostnames encoded with punycode.

**tcl::idna puny** *subcommand ...*
: This command provides direct access to the basic punycode encoder and decoder. It supports two *subcommand*s:

**tcl::idna puny decode** *string* ?*case*?
: This command decodes the punycode-encoded string, *string*, and returns the result. If *case* is provided, it is a boolean to make the case be folded to upper case (if *case* is true) or lower case (if *case* is false) during the decoding process; if omitted, no case transformation is applied.

**tcl::idna puny encode** *string* ?*case*?
: This command encodes the string, *string*, and returns the punycode-encoded version of the string. If *case* is provided, it is a boolean to make the case be folded to upper case (if *case* is true) or lower case (if *case* is false) during the encoding process; if omitted, no case transformation is applied.


**tcl::idna version**
: This returns the version of the **tcl::idna** package.


# Example

This is an example of how punycoding of a string works:

```
package require tcl::idna

puts [tcl::idna puny encode "abc\(->def"]
#    prints: abcdef-kn2c
puts [tcl::idna puny decode "abcdef-kn2c"]
#    prints: abc\(->def
```

