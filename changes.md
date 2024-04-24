# Tcl/Tk 9.0b2 Release Announcement
April ??, 2024

The Tcl Core Team is pleased to announce the 9.0b2 releases of the Tcl
dynamic language and the Tk graphical interface package.  These are the
second beta releases of Tcl 9.0 and Tk 9.0.  More details can be found below.

We would like to express our gratitude to all those who submit bug
reports and patches.  This information is invaluable in enabling us
to identify and eliminate problems in the core. Such reports can be
submitted here.

        https://core.tcl-lang.org/tcl/ticket
        https://core.tcl-lang.org/tk/ticket

We ask that you log in (anonymous if you wish) to create tickets.
This deters abuse of the ticketing system.

## Contents
 1. [Where to get the new releases](#wheretoget)
 2. [General Summary](#summary)
 3. [Some of the most noteworthy changes](#changes)
 4. [Tcl Improvement Proposals (TIPs)](#tips)
 5. [Additional support resources](#support)
 6. [For additional information](#info)

## <a id="wheretoget">1.</a> Where to get the new releases

Tcl/Tk 9.0b2 sources are freely available as open source from the Tcl
SourceForge project's file distribution area:

        https://sourceforge.net/projects/tcl/files/

This distribution is source code only.  We keep links to some third
parties offering pre-built binaries for various systems here:

        https://www.tcl-lang.org/software/tcltk/bindist.html

## <a id="summary">2.</a> General Summary

These are new major versions of both Tcl and Tk.  There are new features
to be enjoyed.  There are incompatibilities to be considered. The list
of both is long and detailed and not fully included here.  We believe many
scripts written for Tcl 8 will run unchanged in Tcl 9.  We believe many more
can be modified in small and simple ways to produce a new script that runs
in both Tcl 8 and Tcl 9.  We expect that extensions and applications using
the public C APIs of Tcl and Tk will involve more effort, but that it is
still within reasonable reach to produce source code supporting both Tcl 8
and Tcl 9 while both releases remain in widespread use.

These are beta releases.  The developers believe the new feature set is
complete enough and the code quality is high enough that it is time for
a larger audience of Tcl/Tk users to give them a try and report back
to the developers what difficulties need resolution before stable
releases of Tcl/Tk 9.0.0.

The experiences of Tcl/Tk 8 users adapting their code to the beta releases
of Tcl/Tk 9 will shape the final interfaces of Tcl/Tk 9.0.0, and will
determine the need for possible Tcl/Tk 8.7 releases that might supply
additional lifecycle and migration support.

It is not recommended to deploy these beta releases directly to mission
critical use without significant testing and review.

## <a id="changes">3.</a> Some of the most noteworthy changes

Tcl 9:

  * 64-bit capacity: Data values larger than 2Gb

  * Internationalization of text
    - Full Unicode range of codepoints
    - New encodings: utf-16/utf-32/ucs-2(le|be), CESU-8, etc.
    - [encoding] options -profile, -failindex manage encoding of I/O.
    - [msgcat] supports custom locale search list
    - [source] defaults to -encoding utf-8

  * Zip filesystems and attached archives.

  * Unix notifiers available using epoll() or kqueue()
    - relieves limits on file descriptors imposed by legacy select()

  * Notable incompatibilities
    - Unqualified varnames resolved in current namespace, not global.
    - No --disable-threads build option.  Always thread-enabled.
    - I/O malencoding default response: raise error (-profile strict)
    - Windows platform needs Windows 7 or Windows Server 2008 R2 or later
    - Ended interpretation of ~ as home directory in pathnames
    - Removed the "identity" encoding
    - $::tcl_precision no longer controls string generation of doubles
    - Removed Tcl 7 legacies: [case], [puts] [read] variant syntaxes
    - Removed subcommands [trace variable|vdelete|vinfo]
    - No -eofchar option for channels anymore for writing.
    - On Windows 10+ (Version 1903 or higher), system encoding is always utf-8.

  * Incompatibilities in C public interface
    - Many arguments expanded type from int to Tcl_Size
    - Ended support for Tcl_ChannelTypeVersion less than 5
    - Introduced versioning of the Tcl_ObjType struct
    - Removed macros CONST*: Tcl 9 support means dropping Tcl 8.3 support
    - Removed routines:
        Tcl_Backslash(), Tcl_*VA(), Tcl_*MathFunc*(), Tcl_MakeSafe(),
        Tcl_(Save|Restore|Discard|Free)Result(), Tcl_EvalTokens(),
        Tcl_(Get|Set)DefaultEncodingDir(),
        Tcl_UniCharN(case)cmp(), Tcl_UniCharCaseMatch()

  * New commands
    - [array default], [array for]
    - [coroinject], [coroprobe]
    - [clock add weekdays]
    - [const], [info const*]
    - [dict getdefault]
    - [file tempdir], [file home], [file tildeexpand]
    - [info commandtype]
    - [ledit]
    - [lpop]
    - [lremove]
    - [lseq]
    - [package files]
    - [string insert], [string is dict]
    - [tcl::process]
    - [*::build-info]

  * New command options
    - [regsub ... -command ...]
    - [lsearch ... -stride ...]
    - [clock scan ... -validate ...]
    - [socket ... -nodelay ... -keepalive ...]
    - [vwait] controlled by several new options

  * Numbers
    - 0NNN format is no longer octal interpretation. Use 0oNNN.
    - 0dNNNN format to compel decimal interpretation.
    - NN_NNN_NNN, underscores in numbers for optional readability
    - Functions: isinf() isnan() isnormal() issubnormal() isunordered()
    - [fpclassify]
    - Function int() no longer truncates to word size

  * tcl::oo facilities
    - private variable and methods
    - [method -export], [method -unexport]

Tk 9:

  * Many improvements to use of platform features and conventions.
    - Built-in widgets and themes are scaling-aware.
    - Improved support of two-finger gestures, where available
    - The [tk windowingsystem] "aqua" needs macOS 10.10 or later

  * New commands and options
    - [tk sysnotify]: access to the OS notifications system
    - [tk systray]: access to the OS tray facility
    - [tk print]: access to the OS printing facility

  * Widget options
    - New ttk::progressbar option: -text
    - [$frame ... -backgroundimage $img -tile $bool]
    - [$menu id], [$menu add|insert ... ?$id? ...]
    - [$image get ... -withalpha ...]
    - All indices now accept the forms "end", "end-int", "int+|-int"

  * Improved widget appearance
    - ttk::notebook with nondefault tab positions

  * Images
    - Partial SVG support
    - Read/write access to photo image metadata

## <a id="tips">4.</a> Tcl Improvement Proposals (TIPs)

Each new user-visible feature in Tcl or Tk should find its origins in
a Tcl Improvement Proposal (TIP).  TIPs are published, edited, considered
and voted in public, and should contain valuable information about how
a feature came to be the way it is.  See the full collection here:

    https://tip.tcl-lang.org/

## <a id="support">5.</a> Additional support resources

See the following links for an accumulation of migration advice:

https://core.tcl-lang.org/tcl/wiki?name=Migrating+C+extensions+to+Tcl+9
https://core.tcl-lang.org/tcl/wiki?name=Migrating+scripts+to+Tcl+9

There has been much progress already porting many known applications,
extensions, and packages in the Tcl world to compatibility with Tcl/Tk 9:

https://wiki.tcl-lang.org/page/Apps+confirmed+to+work+with+Tcl+9
https://wiki.tcl-lang.org/page/Porting+extensions+to+Tcl+9

## <a id="info">6.</a> For additional information:

Please visit the Tcl Developer Xchange web site:

        https://www.tcl-lang.org/

This site contains a variety of information about Tcl/Tk in general, the
core Tcl and Tk distributions, Tcl development tools, and much more.

--
Tcl Core Team and Maintainers
Don Porter, Tcl Core Release Manager