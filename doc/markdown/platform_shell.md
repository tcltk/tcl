---
CommandName: platform::shell
ManualSection: n
Version: 1.1.4
TclPart: platform::shell
TclDescription: Tcl Bundled Packages
Keywords:
 - operating system
 - cpu architecture
 - platform
 - architecture
Copyright:
 - Copyright (c) 2006-2008 ActiveState Software Inc
---

# Name

platform::shell - System identification support code and utilities

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [platform::shell]{.lit} [1.1.4]{.optlit}

[platform::shell::generic]{.cmd} [shell]{.arg}
[platform::shell::identify]{.cmd} [shell]{.arg}
[platform::shell::platform]{.cmd} [shell]{.arg}
:::

# Description

The \fBplatform::shell\fR package provides several utility commands useful for the identification of the architecture of a specific Tcl shell.

This package allows the identification of the architecture of a specific Tcl shell different from the shell running the package. The only requirement is that the other shell (identified by its path), is actually executable on the current machine.

While for most platform this means that the architecture of the interrogated shell is identical to the architecture of the running shell this is not generally true. A counter example are all platforms which have 32 and 64 bit variants and where a 64bit system is able to run 32bit code. For these running and interrogated shell may have different 32/64 bit settings and thus different identifiers.

For applications like a code repository it is important to identify the architecture of the shell which will actually run the installed packages, versus the architecture of the shell running the repository software.

# Commands

**platform::shell::identify** \fIshell\fR
: This command does the same identification as \fBplatform::identify\fR, for the specified Tcl shell, in contrast to the running shell.

**platform::shell::generic** \fIshell\fR
: This command does the same identification as \fBplatform::generic\fR, for the specified Tcl shell, in contrast to the running shell.

**platform::shell::platform** \fIshell\fR
: This command returns the contents of \fBtcl_platform(platform)\fR for the specified Tcl shell.


