---
CommandName: unicode
ManualSection: n
Version: 9.1
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - Tcl_UtfToNormalized(3)
Keywords:
 - Unicode
 - normalize
Copyright:
 - Copyright (c) 2025 Ashok P. Nadkarni.
---

# Name

unicode - Unicode character transforms

# Synopsis

::: {.synopsis} :::
[unicode]{.cmd} [function]{.arg} [options]{.optarg} [string]{.arg}
:::

# Description

The command performs one of several Unicode character transformations, depending on *function* which may take the values described below.

[unicode]{.cmd} [tonfc]{.sub} [[-profile]{.lit} [profile]{.arg}]{.optarg} [string]{.arg}
: Returns *string* normalized as per Unicode **Normalization Form C** (NFC).


[unicode]{.cmd} [tonfd]{.sub} [[-profile]{.lit} [profile]{.arg}]{.optarg} [string]{.arg}
: Returns *string* normalized as per Unicode **Normalization Form D** (NFD).


[unicode]{.cmd} [tonfkc]{.sub} [[-profile]{.lit} [profile]{.arg}]{.optarg} [string]{.arg}
: Returns *string* normalized as per Unicode **Normalization Form KC** (NFKC).


[unicode]{.cmd} [tonfkd]{.sub} [[-profile]{.lit} [profile]{.arg}]{.optarg} [string]{.arg}
: Returns *string* normalized as per Unicode **Normalization Form KD** (NFKD).


The normalization forms NFC, NFD, NFKC and NFKD referenced above are defined in **Section 3.11** of the Unicode standard (see https://www.unicode.org/versions/Unicode16.0.0/core-spec/chapter-3/).

The **-profile** option determines the command behavior in the presence of conversion errors. The passed profile must be **strict** or **replace**. See the **PROFILES** section in the documentation of the **encoding** command for details on profiles.

