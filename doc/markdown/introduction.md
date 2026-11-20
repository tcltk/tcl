---
title: Introduction
TclVersion: 9.1
Links:
 - Tcl
Keywords:
 - syntax
 - notation
 - introduction
---

## About this manual

This is the manual of the Tcl language and the Tk toolkit. It describes every script command and every C API function available in Tcl/Tk . The manual pages are divided into three main sections:

- "Tcl" = all script commands, functions, variables etc. of the Tcl language and the Tcl C API
- "Tk" = all Tk commands and Tk/ttk widgets and the Tk C API
- "Keywords" = a list of keywords and manual pages associated with them

The 12 basic syntax rules of Tcl ("the Dodecalogue") are defined on the page [Language syntax](Tcl/Tcl.md).

## Syntax notation

A Tcl/Tk command consists of words where each word is a text string separated by white space from other words. Newlines separate commands (not words).

The first word of a command is called the "command name". If the second word of a command is a literal word (see below), it is called the subcommand. Not all commands have subcommands. All other words are called arguments in general. Specific terms are used when an argument serves a specific purpose or is handled in a specific way.

In the manual pages, it looks like this:

::: {.synopsis} :::
[commandName]{.cmd} [subCommand]{.sub} [argument]{.arg}
[commandName]{.cmd} [argument]{.arg}
:::

Arguments come in two flavours, mandatory arguments and optional arguments:

- A mandatory argument is often just called an 'argument'. As the name says, it is mandatory and cannot be left out when calling the command. It can be a literal word, i.e. something you need to write as presented (like a fixed keyword) or it can be a placeholder for some word you need to write yourself, i.e. variable text.
- An optional argument is often just called an 'option'. Options can be left out when calling the command and typically have a pre-defined default value that applies when left out. Many optional arguments are switches starting with '-', such as `-side` or `-command`. When the optional argument one a literal word, write it as is, otherwise you can typically pick one from a list of possible choices.

[boldText]{.lit}
: Denotes a literal word, to be written in the command as is.

[italicText]{.arg}
: Denotes an argument. That is a word which is mandatory but with a variable content, i.e. things you must fill in. The text in the syntax description is just a placeholder for your input and its name often tells you what is expected here.

[option]{.optarg}
: Text enclosed in a pair of '?' denotes options. These are optional arguments, i.e. words that can be omitted in the command.

[arg]{.optdot}
: Means that the enclosed arguments may be repeated any number of times (including zero times). Every argument within the enclosed pair of '?' must be repeated in the same order, not just some of them. E.g., [item value]{.optdot} means any number of pairs (including zero) such as `item1 value1 item2 value2 item3 value3`.