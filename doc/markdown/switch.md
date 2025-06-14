---
CommandName: switch
ManualSection: n
Version: 8.5
TclPart: Tcl
TclDescription: Tcl Built-In Commands
Links:
 - for(n)
 - if(n)
 - regexp(n)
Keywords:
 - switch
 - match
 - regular expression
Copyright:
 - Copyright (c) 1993 The Regents of the University of California.
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
---

# Name

switch - Evaluate one of several scripts, depending on a given value

# Synopsis

::: {.synopsis} :::
[switch]{.cmd} [options=?+ string pattern body= ?+pattern body]{.optdot}
[switch]{.cmd} [options=?+ string= +pattern body= ?+pattern body ...]{.optarg}
:::

# Description

The **switch** command matches its *string* argument against each of the *pattern* arguments in order. As soon as it finds a *pattern* that matches *string* it evaluates the following *body* argument by passing it recursively to the Tcl interpreter and returns the result of that evaluation. If the last *pattern* argument is **default** then it matches anything. If no *pattern* argument matches *string* and no default is given, then the **switch** command returns an empty string.

If the initial arguments to **switch** start with **-** then they are treated as options unless there are exactly two arguments to **switch** (in which case the first must the *string* and the second must be the *pattern*/*body* list). The following options are currently supported:

**-exact**
: Use exact matching when comparing *string* to a pattern.  This is the default.

**-glob**
: When matching *string* to the patterns, use glob-style matching (i.e. the same as implemented by the **string match** command).

**-regexp**
: When matching *string* to the patterns, use regular expression matching (as described in the **re_syntax** reference page).

**-nocase**
: Causes comparisons to be handled in a case-insensitive manner.

**-matchvar** *varName*
: This option (only legal when **-regexp** is also specified) specifies the name of a variable into which the list of matches found by the regular expression engine will be written.  The first element of the list written will be the overall substring of the input string (i.e. the *string* argument to **switch**) matched, the second element of the list will be the substring matched by the first capturing parenthesis in the regular expression that matched, and so on.  When a **default** branch is taken, the variable will have the empty list written to it.  This option may be specified at the same time as the **-indexvar** option.

**-indexvar** *varName*
: This option (only legal when **-regexp** is also specified) specifies the name of a variable into which the list of indices referring to matching substrings found by the regular expression engine will be written.  The first element of the list written will be a two-element list specifying the index of the start and index of the first character after the end of the overall substring of the input string (i.e. the *string* argument to **switch**) matched, in a similar way to the **-indices** option to the **regexp** can obtain.  Similarly, the second element of the list refers to the first capturing parenthesis in the regular expression that matched, and so on.  When a **default** branch is taken, the variable will have the empty list written to it.  This option may be specified at the same time as the **-matchvar** option.

**-\|-**
: Marks the end of options.  The argument following this one will be treated as *string* even if it starts with a **-**. This is not required when the matching patterns and bodies are grouped together in a single argument.


Two syntaxes are provided for the *pattern* and *body* arguments. The first uses a separate argument for each of the patterns and commands; this form is convenient if substitutions are desired on some of the patterns or commands. The second form places all of the patterns and commands together into a single argument; the argument must have proper list structure, with the elements of the list being the patterns and commands. The second form makes it easy to construct multi-line switch commands, since the braces around the whole list make it unnecessary to include a backslash at the end of each line. Since the *pattern* arguments are in braces in the second form, no command or variable substitutions are performed on them;  this makes the behavior of the second form different than the first form in some cases.

If a *body* is specified as "**-**" it means that the *body* for the next pattern should also be used as the body for this pattern (if the next pattern also has a body of "**-**" then the body after that is used, and so on). This feature makes it possible to share a single *body* among several patterns.

Beware of how you place comments in **switch** commands.  Comments should only be placed **inside** the execution body of one of the patterns, and not intermingled with the patterns.

# Examples

The **switch** command can match against variables and not just literals, as shown here (the result is *2*):

```
set foo "abc"
switch abc a - b {expr {1}} $foo {expr {2}} default {expr {3}}
```

Using glob matching and the fall-through body is an alternative to writing regular expressions with alternations, as can be seen here (this returns *1*):

```
switch -glob aaab {
    a*b     -
    b       {expr {1}}
    a*      {expr {2}}
    default {expr {3}}
}
```

Whenever nothing matches, the **default** clause (which must be last) is taken.  This example has a result of *3*:

```
switch xyz {
    a -
    b {
        # Correct Comment Placement
        expr {1}
    }
    c {
        expr {2}
    }
    default {
        expr {3}
    }
}
```

When matching against regular expressions, information about what exactly matched is easily obtained using the **-matchvar** option:

```
switch -regexp -matchvar foo -- $bar {
    a(b*)c {
        puts "Found [string length [lindex $foo 1]] 'b's"
    }
    d(e*)f(g*)h {
        puts "Found [string length [lindex $foo 1]] 'e's and\
                [string length [lindex $foo 2]] 'g's"
    }
}
```

