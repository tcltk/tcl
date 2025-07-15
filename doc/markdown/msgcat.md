---
CommandName: msgcat
ManualSection: n
Version: 1.5
TclPart: msgcat
TclDescription: Tcl Bundled Packages
Links:
 - format(n)
 - scan(n)
 - namespace(n)
 - package(n)
 - oo::class(n)
 - oo::object
Keywords:
 - internationalization
 - i18n
 - localization
 - l10n
 - message
 - text
 - translation
 - class
 - object
Copyright:
 - Copyright (c) 1998 Mark Harrison.
---

# Name

msgcat - Tcl message catalog

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcl]{.lit} [9.0]{.lit}
[package]{.cmd} [require]{.sub} [msgcat]{.lit} [1.7]{.lit}

[::msgcat::mc]{.cmd} [src-string]{.arg} [arg]{.optdot}
[::msgcat::mcmax]{.cmd} [src-string]{.optdot}
[::msgcat::mcexists]{.cmd} [-exactnamespace]{.optlit} [-exactlocale]{.optlit} [src-string]{.arg}
[::msgcat::mcpackagenamespaceget]{.cmd}
[::msgcat::mclocale]{.cmd} [newLocale]{.optarg}
[::msgcat::mcpreferences]{.cmd} [locale preference]{.optdot}
[::msgcat::mcloadedlocales]{.cmd} [subcommand]{.sub}
[::msgcat::mcload]{.cmd} [dirname]{.arg}
[::msgcat::mcset]{.cmd} [locale]{.arg} [src-string]{.arg} [translate-string]{.optarg}
[::msgcat::mcmset]{.cmd} [locale]{.arg} [src-trans-list]{.arg}
[::msgcat::mcflset]{.cmd} [src-string]{.arg} [translate-string]{.optarg}
[::msgcat::mcflmset]{.cmd} [src-trans-list]{.arg}
[::msgcat::mcunknown]{.cmd} [locale]{.arg} [src-string]{.arg} [arg arg]{.optdot}
[::msgcat::mcpackagelocale]{.cmd} [subcommand]{.sub} [locale]{.optarg}
[::msgcat::mcpackageconfig]{.cmd} [subcommand]{.sub} [option]{.arg} [value]{.optarg}
[::msgcat::mcforgetpackage]{.cmd}
[::msgcat::mcutil]{.cmd} [subcommand]{.sub} [locale]{.optarg}
:::

# Description

The \fBmsgcat\fR package provides a set of functions that can be used to manage multi-lingual user interfaces. Text strings are defined in a "message catalog" which is independent from the application, and which can be edited or localized without modifying the application source code.  New languages or locales may be provided by adding a new file to the message catalog.

**msgcat** distinguishes packages by its namespace. Each package has its own message catalog and configuration settings in \fBmsgcat\fR.

A \fIlocale\fR is a specification string describing a user language like \fBde_ch\fR for Swiss German. In \fBmsgcat\fR, there is a global locale initialized by the system locale of the current system. Each package may decide to use the global locale or to use a package specific locale.

The global locale may be changed on demand, for example by a user initiated language change or within a multi user application like a web server.

::: {.info -version="TIP490"}
Object oriented programming is supported by the use of a package namespace.
:::

# Commands

**::msgcat::mc** \fIsrc-string\fR ?\fIarg ...\fR?
: Returns a translation of \fIsrc-string\fR according to the current locale.  If additional arguments past \fIsrc-string\fR are given, the \fBformat\fR command is used to substitute the additional arguments in the translation of \fIsrc-string\fR.
    **::msgcat::mc** will search the messages defined in the current namespace for a translation of \fIsrc-string\fR; if none is found, it will search in the parent of the current namespace, and so on until it reaches the global namespace.  If no translation string exists, \fB::msgcat::mcunknown\fR is called and the string returned from \fB::msgcat::mcunknown\fR is returned.
    **::msgcat::mc** is the main function used to localize an application.  Instead of using an English string directly, an application can pass the English string through \fB::msgcat::mc\fR and use the result.  If an application is written for a single language in this fashion, then it is easy to add support for additional languages later simply by defining new message catalog entries.

**::msgcat::mcn** \fInamespace src-string\fR ?\fIarg arg ...\fR?
: Like \fB::msgcat::mc\fR, but with the message namespace specified as first argument.


**mcn** may be used for cases where the package namespace is not the namespace of the caller. An example is shown within the description of the command \fB::msgcat::mcpackagenamespaceget\fR below.

**::msgcat::mcmax** ?\fIsrc-string ...\fR?
: Given several source strings, \fB::msgcat::mcmax\fR returns the length of the longest translated string.  This is useful when designing localized GUIs, which may require that all buttons, for example, be a fixed width (which will be the width of the widest button).

**::msgcat::mcexists** ?\fB-exactnamespace\fR? ?\fB-exactlocale\fR? ?\fB-namespace\fR \fInamespace\fR? \fIsrc-string\fR
: Return true, if there is a translation for the given \fIsrc-string\fR.


The search may be limited by the option \fB-exactnamespace\fR to only check the current namespace and not any parent namespaces.

It may also be limited by the option \fB-exactlocale\fR to only check the first prefered locale (e.g. first element returned by \fB::msgcat::mcpreferences\fR if global locale is used).

::: {.info -version="TIP490"}
An explicit package namespace may be specified by the option \fB-namespace\fR. The namespace of the caller is used if not explicitly specified.
:::

**::msgcat::mcpackagenamespaceget**
: Return the package namespace of the caller. This command handles all cases described in section \fBOBJECT ORIENTED PROGRAMMING\fR.


Example usage is a tooltip package, which saves the caller package namespace to update the translation each time the tooltip is shown:

```
proc ::tooltip::tooltip {widget message} {
    ...
    set messagenamespace [uplevel 1 {::msgcat::mcpackagenamespaceget}]
    ...
    bind $widget  [list ::tooltip::show $widget $messagenamespace $message]
}

proc ::tooltip::show {widget messagenamespace message} {
    ...
    set message [::msgcat::mcn $messagenamespace $message]
    ...
}
```

**::msgcat::mclocale** ?\fInewLocale\fR?
: If \fInewLocale\fR is omitted, the current locale is returned, otherwise the current locale is set to \fInewLocale\fR.


If the new locale is set to \fInewLocale\fR, the corresponding preferences are calculated and set. For example, if the current locale is en_US_funky, then \fB::msgcat::mcpreferences\fR returns \fB{en_us_funky en_us en {}}\fR.

The same result may be achieved by \fB::msgcat::mcpreferences\fR {*}[\fB::msgcat::mcutil getpreferences\fR \fInewLocale\fR].

The current locale is always the first element of the list returned by \fBmcpreferences\fR.

msgcat stores and compares the locale in a case-insensitive manner, and returns locales in lowercase. The initial locale is determined by the locale specified in the user's environment.  See \fBLOCALE SPECIFICATION\fR below for a description of the locale string format.

If the locale is set, the preference list of locales is evaluated. Locales in this list are loaded now, if not jet loaded.

**::msgcat::mcpreferences** ?\fIlocale preference ...\fR?
: Without arguments, returns an ordered list of the locales preferred by the user. The list is ordered from most specific to least preference.


::: {.info -version="TIP499"}
A set of locale preferences may be given to set the list of locale preferences. The current locale is also set, which is the first element of the locale preferences list.
:::

Locale preferences are loaded now, if not jet loaded.

As an example, the user may prefer French or English text. This may be configured by:

```
::msgcat::mcpreferences fr en {}
```

**::msgcat::mcloadedlocales subcommand**
: This group of commands manage the list of loaded locales for packages not setting a package locale.


The subcommand \fBloaded\fR returns the list of currently loaded locales.

The subcommand \fBclear\fR removes all locales and their data, which are not in the current preference list.

**::msgcat::mcload** \fIdirname\fR
: Searches the specified directory for files that match the language specifications returned by \fB::msgcat::mcloadedlocales loaded\fR (or \fBmsgcat::mcpackagelocale preferences\fR if a package locale is set) (note that these are all lowercase), extended by the file extension ".msg". Each matching file is read in order, assuming a UTF-8 encoding.  The file contents are then evaluated as a Tcl script.  This means that Unicode characters may be present in the message file either directly in their UTF-8 encoded form, or by use of the backslash-u quoting recognized by Tcl evaluation.  The number of message files which matched the specification and were loaded is returned.
    In addition, the given folder is stored in the \fBmsgcat\fR package configuration option \fImcfolder\fR to eventually load message catalog files required by a locale change.

**::msgcat::mcset** \fIlocale src-string\fR ?\fItranslate-string\fR?
: Sets the translation for \fIsrc-string\fR to \fItranslate-string\fR in the specified \fIlocale\fR and the current namespace.  If \fItranslate-string\fR is not specified, \fIsrc-string\fR is used for both.  The function returns \fItranslate-string\fR.

**::msgcat::mcmset** \fIlocale src-trans-list\fR
: Sets the translation for multiple source strings in \fIsrc-trans-list\fR in the specified \fIlocale\fR and the current namespace. \fIsrc-trans-list\fR must have an even number of elements and is in the form {\fIsrc-string translate-string\fR ?\fIsrc-string translate-string ...\fR?} \fB::msgcat::mcmset\fR can be significantly faster than multiple invocations of \fB::msgcat::mcset\fR. The function returns the number of translations set.

**::msgcat::mcflset** \fIsrc-string\fR ?\fItranslate-string\fR?
: Sets the translation for \fIsrc-string\fR to \fItranslate-string\fR in the current namespace for the locale implied by the name of the message catalog being loaded via \fB::msgcat::mcload\fR.  If \fItranslate-string\fR is not specified, \fIsrc-string\fR is used for both.  The function returns \fItranslate-string\fR.

**::msgcat::mcflmset** \fIsrc-trans-list\fR
: Sets the translation for multiple source strings in \fIsrc-trans-list\fR in the current namespace for the locale implied by the name of the message catalog being loaded via \fB::msgcat::mcload\fR. \fIsrc-trans-list\fR must have an even number of elements and is in the form {\fIsrc-string translate-string\fR ?\fIsrc-string translate-string ...\fR?} \fB::msgcat::mcflmset\fR can be significantly faster than multiple invocations of \fB::msgcat::mcflset\fR. The function returns the number of translations set.

**::msgcat::mcunknown** \fIlocale src-string\fR ?\fIarg arg ...\fR?
: This routine is called by \fB::msgcat::mc\fR in the case when a translation for \fIsrc-string\fR is not defined in the current locale.  The default action is to return \fIsrc-string\fR passed by format if there are any arguments.  This procedure can be redefined by the application, for example to log error messages for each unknown string.  The \fB::msgcat::mcunknown\fR procedure is invoked at the same stack context as the call to \fB::msgcat::mc\fR.  The return value of \fB::msgcat::mcunknown\fR is used as the return value for the call to \fB::msgcat::mc\fR.
    Note that this routine is only called if the concerned package did not set a package locale unknown command name.

**::msgcat::mcforgetpackage**
: The calling package clears all its state within the \fBmsgcat\fR package including all settings and translations.


**::msgcat::mcutil getpreferences** \fIlocale\fR
: Return the preferences list of the given locale as described in the section \fBLOCALE SPECIFICATION\fR. An example is the composition of a preference list for the bilingual region "Biel/Bienne" as a concatenation of swiss german and swiss french:

    ```
    % concat [lrange [msgcat::mcutil getpreferences fr_CH] 0 end-1] [msgcat::mcutil getpreferences de_CH]
    fr_ch fr de_ch de {}
    ```

**::msgcat::mcutil getsystemlocale**
: The system locale is returned as described by the section \fBLOCALE SPECIFICATION\fR.


# Locale specification

The locale is specified to \fBmsgcat\fR by a locale string passed to \fB::msgcat::mclocale\fR. The locale string consists of a language code, an optional country code, and an optional system-specific code, each separated by "_". The country and language codes are specified in standards ISO-639 and ISO-3166. For example, the locale "en" specifies English and "en_US" specifies U.S. English.

When the msgcat package is first loaded, the locale is initialized according to the user's environment.  The variables \fBenv(LC_ALL)\fR, \fBenv(LC_MESSAGES)\fR, and \fBenv(LANG)\fR are examined in order. The first of them to have a non-empty value is used to determine the initial locale.  The value is parsed according to the XPG4 pattern

```
language[_country][.codeset][@modifier]
```

to extract its parts.  The initial locale is then set by calling \fB::msgcat::mclocale\fR with the argument

```
language[_country][_modifier]
```

On Windows and Cygwin, if none of those environment variables is set, msgcat will attempt to extract locale information from the registry. The RFC4747 locale name "lang-script-country-options" is transformed to the locale as "lang_country_script" (Example: sr-Latn-CS -> sr_cs_latin). If all these attempts to discover an initial locale from the user's environment fail, msgcat defaults to an initial locale of "C".

When a locale is specified by the user, a "best match" search is performed during string translation.  For example, if a user specifies en_GB_Funky, the locales "en_gb_funky", "en_gb", "en" and .MT (the empty string) are searched in order until a matching translation string is found.  If no translation string is available, then the unknown handler is called.

# Namespaces and message catalogs

Strings stored in the message catalog are stored relative to the namespace from which they were added.  This allows multiple packages to use the same strings without fear of collisions with other packages.  It also allows the source string to be shorter and less prone to typographical error.

For example, executing the code

```
::msgcat::mcset en hello "hello from ::"
namespace eval foo {
    ::msgcat::mcset en hello "hello from ::foo"
}
puts [::msgcat::mc hello]
namespace eval foo {puts [::msgcat::mc hello]}
```

will print

```
hello from ::
hello from ::foo
```

When searching for a translation of a message, the message catalog will search first the current namespace, then the parent of the current namespace, and so on until the global namespace is reached.  This allows child namespaces to "inherit" messages from their parent namespace.

For example, executing (in the "en" locale) the code

```
::msgcat::mcset en m1 ":: message1"
::msgcat::mcset en m2 ":: message2"
::msgcat::mcset en m3 ":: message3"
namespace eval ::foo {
    ::msgcat::mcset en m2 "::foo message2"
    ::msgcat::mcset en m3 "::foo message3"
}
namespace eval ::foo::bar {
    ::msgcat::mcset en m3 "::foo::bar message3"
}
namespace import ::msgcat::mc
puts "[mc m1]; [mc m2]; [mc m3]"
namespace eval ::foo {puts "[mc m1]; [mc m2]; [mc m3]"}
namespace eval ::foo::bar {puts "[mc m1]; [mc m2]; [mc m3]"}
```

will print

```
:: message1; :: message2; :: message3
:: message1; ::foo message2; ::foo message3
:: message1; ::foo message2; ::foo::bar message3
```

# Location and format of message files

Message files can be located in any directory, subject to the following conditions:

1. All message files for a package are in the same directory.

2. The message file name is a msgcat locale specifier (all lowercase) followed by ".msg". For example:


```
es.msg    \(em spanish
en_gb.msg \(em United Kingdom English
```

*Exception:* The message file for the root locale .MT is called "**ROOT.msg**". This exception is made so as not to cause peculiar behavior, such as marking the message file as "hidden" on Unix file systems.

1. The file contains a series of calls to \fBmcflset\fR and \fBmcflmset\fR, setting the necessary translation strings for the language, likely enclosed in a \fBnamespace eval\fR so that all source strings are tied to the namespace of the package. For example, a short \fBes.msg\fR might contain:


```
namespace eval ::mypackage {
    ::msgcat::mcflset "Free Beer" "Cerveza Gratis"
}
```

# Recommended message setup for packages

If a package is installed into a subdirectory of the \fBtcl_pkgPath\fR and loaded via \fBpackage require\fR, the following procedure is recommended.

1. During package installation, create a subdirectory \fBmsgs\fR under your package directory.

2. Copy your *.msg files into that directory.

3. Add the following command to your package initialization script:


```
# load language files, stored in msgs subdirectory
::msgcat::mcload [file join [file dirname [info script]] msgs]
```

# Positional codes for format and scan commands

It is possible that a message string used as an argument to \fBformat\fR might have positionally dependent parameters that might need to be repositioned.  For example, it might be syntactically desirable to rearrange the sentence structure while translating.

```
format "We produced %d units in location %s" $num $city
format "In location %s we produced %d units" $city $num
```

This can be handled by using the positional parameters:

```
format "We produced %1\$d units in location %2\$s" $num $city
format "In location %2\$s we produced %1\$d units" $num $city
```

Similarly, positional parameters can be used with \fBscan\fR to extract values from internationalized strings. Note that it is not necessary to pass the output of \fB::msgcat::mc\fR to \fBformat\fR directly; by passing the values to substitute in as arguments, the formatting substitution is done directly.

```
msgcat::mc {Produced %1$d at %2$s} $num $city
# ... where that key is mapped to one of the
# human-oriented versions by msgcat::mcset
```

# Package private locale

A package using \fBmsgcat\fR may choose to use its own package private locale and its own set of loaded locales, independent to the global locale set by \fB::msgcat::mclocale\fR.

This allows a package to change its locale without causing any locales load or removal in other packages and not to invoke the global locale change callback (see below).

This action is controled by the following ensemble:

**::msgcat::mcpackagelocale set** ?\fIlocale\fR?
: Set or change a package private locale. The package private locale is set to the given \fIlocale\fR if the \fIlocale\fR is given. If the option \fIlocale\fR is not given, the package is set to package private locale mode, but no locale is changed (e.g. if the global locale was valid for the package before, it is copied to the package private locale).


This command may cause the load of locales.

**::msgcat::mcpackagelocale get**
: Return the package private locale or the global locale, if no package private locale is set.

**::msgcat::mcpackagelocale preferences** ?\fIlocale preference\fR? ...
: With no parameters, return the package private preferences or the global preferences, if no package private locale is set. The package locale state (set or not) is not changed (in contrast to the command \fB::msgcat::mcpackagelocale set\fR).


::: {.info -version="TIP499"}
If a set of locale preferences is given, it is set as package locale preference list. The package locale is set to the first element of the preference list. A package locale is activated, if it was not set so far.
:::

Locale preferences are loaded now for the package, if not yet loaded.

**::msgcat::mcpackagelocale loaded**
: Return the list of locales loaded for this package.

**::msgcat::mcpackagelocale isset**
: Returns true, if a package private locale is set.

**::msgcat::mcpackagelocale unset**
: Unset the package private locale and use the global locale. Load and remove locales to adjust the list of loaded locales for the package to the global loaded locales list.

**::msgcat::mcpackagelocale present** \fIlocale\fR
: Returns true, if the given locale is loaded for the package.

**::msgcat::mcpackagelocale clear**
: Clear any loaded locales of the package not present in the package preferences.


# Changing package options

Each package using msgcat has a set of options within \fBmsgcat\fR. The package options are described in the next sectionPackage options. Each package option may be set or unset individually using the following ensemble:

**::msgcat::mcpackageconfig get** \fIoption\fR
: Return the current value of the given \fIoption\fR. This call returns an error if the option is not set for the package.

**::msgcat::mcpackageconfig isset** \fIoption\fR
: Returns 1, if the given \fIoption\fR is set for the package, 0 otherwise.

**::msgcat::mcpackageconfig set** \fIoption value\fR
: Set the given \fIoption\fR to the given \fIvalue\fR. This may invoke additional actions in dependency of the \fIoption\fR. The return value is 0 or the number of loaded packages for the option \fBmcfolder\fR.

**::msgcat::mcpackageconfig unset** \fIoption\fR
: Unsets the given \fIoption\fR for the package. No action is taken if the \fIoption\fR is not set for the package. The empty string is returned.


## Package options

The following package options are available for each package:

**mcfolder**
: This is the message folder of the package. This option is set by mcload and by the subcommand set. Both are identical and both return the number of loaded message catalog files.
    Setting or changing this value will load all locales contained in the preferences valid for the package. This implies also to invoke any set loadcmd (see below).
    Unsetting this value will disable message file load for the package.

**loadcmd**
: This callback is invoked before a set of message catalog files are loaded for the package which has this property set.


This callback may be used to do any preparation work for message file load or to get the message data from another source like a data base. In this case, no message files are used (mcfolder is unset).

See section \fBcallback invocation\fR below. The parameter list appended to this callback is the list of locales to load.

If this callback is changed, it is called with the preferences valid for the package.

**changecmd**
: This callback is invoked when a default local change was performed. Its purpose is to allow a package to update any dependency on the default locale like showing the GUI in another language.


See the callback invocation section below. The parameter list appended to this callback is \fBmcpreferences\fR. The registered callbacks are invoked in no particular order.

**unknowncmd**
: Use a package locale mcunknown procedure instead of the standard version supplied by the msgcat package (\fBmsgcat::mcunknown\fR).


The called procedure must return the formatted message which will finally be returned by \fBmsgcat::mc\fR.

A generic unknown handler is used if set to the empty string. This consists of returning the key if no arguments are given. With given arguments, the \fBformat\fR command is used to process the arguments.

See section \fBcallback invocation\fR below. The appended arguments are identical to \fBmsgcat::mcunknown\fR.

# Callback invocation

A package may decide to register one or multiple callbacks, as described above.

Callbacks are invoked, if:

1. the callback command is set,

2. the command is not the empty string,

3. the registering namespace exists.

If a called routine fails with an error, the \fBbgerror\fR routine for the interpreter is invoked after command completion. Only exception is the callback \fBunknowncmd\fR, where an error causes the invoking \fBmc\fR-command to fail with that error.

# Object oriented programming

**msgcat** supports packages implemented by object oriented programming. Objects and classes should be defined within a package namespace.

There are 3 supported cases where package namespace sensitive commands of msgcat (\fBmc\fR, \fBmcexists\fR, \fBmcpackagelocale\fR, \fBmcforgetpackage\fR, \fBmcpackagenamespaceget\fR, \fBmcpackageconfig\fR, \fBmcset\fR and \fBmcmset\fR) may be called:

**1) In class definition script**
: **msgcat** command is called within a class definition script.

    ```
    namespace eval ::N2 {
        mcload $dir/msgs
        oo::class create C1 {puts [mc Hi!]}
    }
    ```


**2) method defined in a class**
: **msgcat** command is called from a method in an object and the method is defined in a class.

    ```
    namespace eval ::N3Class {
        mcload $dir/msgs
        oo::class create C1
        oo::define C1 method m1 {
            puts [mc Hi!]
        }
    }
    ```


**3) method defined in a classless object**
: **msgcat** command is called from a method of a classless object.

    ```
    namespace eval ::N4 {
        mcload $dir/msgs
        oo::object create O1
        oo::objdefine O1 method m1 {} {
            puts [mc Hi!]
        }
    }
    ```


# Examples

Packages which display a GUI may update their widgets when the global locale changes. To register to a callback, use:

```
namespace eval gui {
    msgcat::mcpackageconfig changecmd updateGUI

    proc updateGUI args {
        puts "New locale is '[lindex $args 0]'."
    }
}
% msgcat::mclocale fr
fr
% New locale is 'fr'.
```

If locales (or additional locales) are contained in another source like a database, a package may use the load callback and not \fBmcload\fR:

```
namespace eval db {
    msgcat::mcpackageconfig loadcmd loadMessages

    proc loadMessages args {
        foreach locale $args {
            if {[LocaleInDB $locale]} {
                msgcat::mcmset $locale [GetLocaleList $locale]
            }
        }
    }
}
```

The \fBclock\fR command implementation uses \fBmsgcat\fR with a package locale to implement the command line parameter \fB-locale\fR. Here are some sketches of the implementation:

First, a package locale is initialized and the generic unknown function is deactivated:

```
msgcat::mcpackagelocale set
msgcat::mcpackageconfig unknowncmd ""
```

As an example, the user requires the week day in a certain locale as follows:

```
clock format [clock seconds] -format %A -locale fr
```

**clock** sets the package locale to \fBfr\fR and looks for the day name as follows:

```
msgcat::mcpackagelocale set $locale
return [lindex [msgcat::mc DAYS_OF_WEEK_FULL] $day]
### Returns "mercredi"
```

Within \fBclock\fR, some message-catalog items are heavy in computation and thus are dynamically cached using:

```
proc ::tcl::clock::LocalizeFormat { locale format } {
    set key FORMAT_$format
    if { [::msgcat::mcexists -exactlocale -exactnamespace $key] } {
        return [mc $key]
    }
    #...expensive computation of format clipped...
    mcset $locale $key $format
    return $format
}
```

# Credits

The message catalog code was developed by Mark Harrison.

