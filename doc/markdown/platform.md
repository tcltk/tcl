---
CommandName: platform
ManualSection: n
Version: 1.0.4
TclPart: platform
TclDescription: Tcl Bundled Packages
Keywords:
 - operating system
 - cpu architecture
 - platform
 - architecture
Copyright:
 - Copyright (c) 2006 ActiveState Software Inc
---

# Name

platform - System identification support code and utilities

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [platform]{.lit} [1.0.10]{.optlit}

[platform::generic]{.cmd}
[platform::identify]{.cmd}
[platform::patterns]{.cmd} [identifier]{.arg}
:::

# Description

The **platform** package provides several utility commands useful for the identification of the architecture of a machine running Tcl.

Whilst Tcl provides the **tcl_platform** array for identifying the current architecture (in particular, the platform and machine elements) this is not always sufficient. This is because (on Unix machines) **tcl_platform** reflects the values returned by the **uname** command and these are not standardized across platforms and architectures. In addition, on at least one platform (AIX) the **tcl_platform(machine)** contains the CPU serial number.

Consequently, individual applications need to manipulate the values in **tcl_platform** (along with the output of system specific utilities) - which is both inconvenient for developers, and introduces the potential for inconsistencies in identifying architectures and in naming conventions.

The **platform** package prevents such fragmentation - i.e., it establishes a standard naming convention for architectures running Tcl and makes it more convenient for developers to identify the current architecture a Tcl program is running on.

# Commands

**platform::identify**
: This command returns an identifier describing the platform the Tcl core is running on. The returned identifier has the general format *OS*-*CPU*. The *OS* part of the identifier may contain details like kernel version, libc version, etc., and this information may contain dashes as well.  The *CPU* part will not contain dashes, making the preceding dash the last dash in the result.

**platform::generic**
: This command returns a simplified identifier describing the platform the Tcl core is running on. In contrast to **platform::identify** it leaves out details like kernel version, libc version, etc. The returned identifier has the general format *OS*-*CPU*.

**platform::patterns** *identifier*
: This command takes an identifier as returned by **platform::identify** and returns a list of identifiers describing compatible architectures.


# Example

This can be used to allow an application to be shipped with multiple builds of a shared library, so that the same package works on many versions of an operating system. For example:

```
package require platform
# Assume that app script is .../theapp/bin/theapp.tcl
set binDir [file dirname [file normalize [info script]]]
set libDir [file join $binDir .. lib]
set platLibDir [file join $libDir [platform::identify]]
load [file join $platLibDir support[info sharedlibextension]]
```

