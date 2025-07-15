---
CommandName: tcltest
ManualSection: n
Version: 2.5
TclPart: tcltest
TclDescription: Tcl Bundled Packages
Keywords:
 - test
 - test harness
 - test suite
Copyright:
 - Copyright (c) 1990-1994 The Regents of the University of California
 - Copyright (c) 1994-1997 Sun Microsystems, Inc.
 - Copyright (c) 1998-1999 Scriptics Corporation
 - Copyright (c) 2000 Ajuba Solutions
---

# Name

tcltest - Test harness support code and utilities

# Synopsis

::: {.synopsis} :::
[package]{.cmd} [require]{.sub} [tcltest]{.lit} [2.5]{.optlit}

[tcltest::test]{.cmd} [name]{.arg} [description]{.arg} [-option value]{.optdot}
[tcltest::test]{.cmd} [name]{.arg} [description]{.arg} [constraints]{.optarg} [body]{.arg} [result]{.arg}

[tcltest::loadTestedCommands]{.cmd}
[tcltest::makeDirectory]{.cmd} [name]{.arg} [directory]{.optarg}
[tcltest::removeDirectory]{.cmd} [name]{.arg} [directory]{.optarg}
[tcltest::makeFile]{.cmd} [contents]{.arg} [name]{.arg} [directory]{.optarg}
[tcltest::removeFile]{.cmd} [name]{.arg} [directory]{.optarg}
[tcltest::viewFile]{.cmd} [name]{.arg} [directory]{.optarg}
[tcltest::cleanupTests]{.cmd} [runningMultipleTests]{.optarg}
[tcltest::runAllTests]{.cmd}

[tcltest::configure]{.cmd}
[tcltest::configure]{.cmd} [-option]{.arg}
[tcltest::configure]{.cmd} [-option]{.arg} [value]{.arg} [-option value]{.optdot}
[tcltest::customMatch]{.cmd} [mode]{.arg} [command]{.arg}
[tcltest::testConstraint]{.cmd} [constraint]{.arg} [value]{.optarg}
[tcltest::outputChannel]{.cmd} [channelID]{.optarg}
[tcltest::errorChannel]{.cmd} [channelID]{.optarg}
[tcltest::interpreter]{.cmd} [interp]{.optarg}

[tcltest::debug]{.cmd} [level]{.optarg}
[tcltest::errorFile]{.cmd} [filename]{.optarg}
[tcltest::limitConstraints]{.cmd} [boolean]{.optarg}
[tcltest::loadFile]{.cmd} [filename]{.optarg}
[tcltest::loadScript]{.cmd} [script]{.optarg}
[tcltest::match]{.cmd} [patternList]{.optarg}
[tcltest::matchDirectories]{.cmd} [patternList]{.optarg}
[tcltest::matchFiles]{.cmd} [patternList]{.optarg}
[tcltest::outputFile]{.cmd} [filename]{.optarg}
[tcltest::preserveCore]{.cmd} [level]{.optarg}
[tcltest::singleProcess]{.cmd} [boolean]{.optarg}
[tcltest::skip]{.cmd} [patternList]{.optarg}
[tcltest::skipDirectories]{.cmd} [patternList]{.optarg}
[tcltest::skipFiles]{.cmd} [patternList]{.optarg}
[tcltest::temporaryDirectory]{.cmd} [directory]{.optarg}
[tcltest::testsDirectory]{.cmd} [directory]{.optarg}
[tcltest::verbose]{.cmd} [level]{.optarg}

[tcltest::test]{.cmd} [name]{.arg} [description]{.arg} [optionList]{.arg}
[tcltest::normalizeMsg]{.cmd} [msg]{.arg}
[tcltest::normalizePath]{.cmd} [pathVar]{.arg}
[tcltest::workingDirectory]{.cmd} [dir]{.optarg}
:::

# Description

The **tcltest** package provides several utility commands useful in the construction of test suites for code instrumented to be run by evaluation of Tcl commands.  Notably the built-in commands of the Tcl library itself are tested by a test suite using the tcltest package.

All the commands provided by the **tcltest** package are defined in and exported from the **::tcltest** namespace, as indicated in the **SYNOPSIS** above.  In the following sections, all commands will be described by their simple names, in the interest of brevity.

The central command of **tcltest** is **test** that defines and runs a test.  Testing with **test** involves evaluation of a Tcl script and comparing the result to an expected result, as configured and controlled by a number of options.  Several other commands provided by **tcltest** govern the configuration of **test** and the collection of many **test** commands into test suites.

See **CREATING TEST SUITES WITH TCLTEST** below for an extended example of how to use the commands of **tcltest** to produce test suites for your Tcl-enabled code.

# Commands

**test** *name description* ?*-option value ...*?
: Defines and possibly runs a test with the name *name* and description *description*.  The name and description of a test are used in messages reported by **test** during the test, as configured by the options of **tcltest**.  The remaining *option value* arguments to **test** define the test, including the scripts to run, the conditions under which to run them, the expected result, and the means by which the expected and actual results should be compared. See **TESTS** below for a complete description of the valid options and how they define a test.  The **test** command returns an empty string.

**test** *name description* ?*constraints*? *body result*
: This form of **test** is provided to support test suites written for version 1 of the **tcltest** package, and also a simpler interface for a common usage.  It is the same as "**test** *name description* **-constraints** *constraints* **-body** *body* **-result** *result*". All other options to **test** take their default values.  When *constraints* is omitted, this form of **test** can be distinguished from the first because all *option*s begin with "-".

**loadTestedCommands**
: Evaluates in the caller's context the script specified by **configure -load** or **configure -loadfile**. Returns the result of that script evaluation, including any error raised by the script.  Use this command and the related configuration options to provide the commands to be tested to the interpreter running the test suite.

**makeFile** *contents name* ?*directory*?
: Creates a file named *name* relative to directory *directory* and write *contents* to that file using the encoding **encoding system**. If *contents* does not end with a newline, a newline will be appended so that the file named *name* does end with a newline.  Because the system encoding is used, this command is only suitable for making text files. The file will be removed by the next evaluation of **cleanupTests**, unless it is removed by **removeFile** first.  The default value of *directory* is the directory **configure -tmpdir**. Returns the full path of the file created.  Use this command to create any text file required by a test with contents as needed.

**removeFile** *name* ?*directory*?
: Forces the file referenced by *name* to be removed.  This file name should be relative to *directory*.   The default value of *directory* is the directory **configure -tmpdir**. Returns an empty string.  Use this command to delete files created by **makeFile**.

**makeDirectory** *name* ?*directory*?
: Creates a directory named *name* relative to directory *directory*. The directory will be removed by the next evaluation of **cleanupTests**, unless it is removed by **removeDirectory** first. The default value of *directory* is the directory **configure -tmpdir**. Returns the full path of the directory created.  Use this command to create any directories that are required to exist by a test.

**removeDirectory** *name* ?*directory*?
: Forces the directory referenced by *name* to be removed. This directory should be relative to *directory*. The default value of *directory* is the directory **configure -tmpdir**. Returns an empty string.  Use this command to delete any directories created by **makeDirectory**.

**viewFile** *file* ?*directory*?
: Returns the contents of *file*, except for any final newline, just as **read -nonewline** would return. This file name should be relative to *directory*. The default value of *directory* is the directory **configure -tmpdir**.  Use this command as a convenient way to turn the contents of a file generated by a test into the result of that test for matching against an expected result.  The contents of the file are read using the system encoding, so its usefulness is limited to text files.

**cleanupTests**
: Intended to clean up and summarize after several tests have been run.  Typically called once per test file, at the end of the file after all tests have been completed.  For best effectiveness, be sure that the **cleanupTests** is evaluated even if an error occurs earlier in the test file evaluation.
    Prints statistics about the tests run and removes files that were created by **makeDirectory** and **makeFile** since the last **cleanupTests**.  Names of files and directories in the directory **configure -tmpdir** created since the last **cleanupTests**, but not created by **makeFile** or **makeDirectory** are printed to **outputChannel**.  This command also restores the original shell environment, as described by the global **env** array. Returns an empty string.

**runAllTests**
: This is a main command meant to run an entire suite of tests, spanning multiple files and/or directories, as governed by the configurable options of **tcltest**.  See **RUNNING ALL TESTS** below for a complete description of the many variations possible with **runAllTests**.


## Configuration commands

**configure**
: Returns the list of configurable options supported by **tcltest**. See **CONFIGURABLE OPTIONS** below for the full list of options, their valid values, and their effect on **tcltest** operations.

**configure** *option*
: Returns the current value of the supported configurable option *option*. Raises an error if *option* is not a supported configurable option.

**configure** *option value* ?*-option value ...*?
: Sets the value of each configurable option *option* to the corresponding value *value*, in order.  Raises an error if an *option* is not a supported configurable option, or if *value* is not a valid value for the corresponding *option*, or if a *value* is not provided.  When an error is raised, the operation of **configure** is halted, and subsequent *option value* arguments are not processed.
    If the environment variable **::env(TCLTEST_OPTIONS)** exists when the **tcltest** package is loaded (by **package require** **tcltest**) then its value is taken as a list of arguments to pass to **configure**. This allows the default values of the configuration options to be set by the environment.

**customMatch** *mode script*
: Registers *mode* as a new legal value of the **-match** option to **test**.  When the **-match** *mode* option is passed to **test**, the script *script* will be evaluated to compare the actual result of evaluating the body of the test to the expected result. To perform the match, the *script* is completed with two additional words, the expected result, and the actual result, and the completed script is evaluated in the global namespace. The completed script is expected to return a boolean value indicating whether or not the results match.  The built-in matching modes of **test** are **exact**, **glob**, and **regexp**.

**testConstraint** *constraint* ?*boolean*?
: Sets or returns the boolean value associated with the named *constraint*. See **TEST CONSTRAINTS** below for more information.

**interpreter** ?*executableName*?
: Sets or returns the name of the executable to be **exec**ed by **runAllTests** to run each test file when **configure -singleproc** is false. The default value for **interpreter** is the name of the currently running program as returned by **info nameofexecutable**.

**outputChannel** ?*channelID*?
: Sets or returns the output channel ID.  This defaults to **stdout**. Any test that prints test related output should send that output to **outputChannel** rather than letting that output default to **stdout**.

**errorChannel** ?*channelID*?
: Sets or returns the error channel ID.  This defaults to **stderr**. Any test that prints error messages should send that output to **errorChannel** rather than printing directly to **stderr**.


## Shortcut configuration commands

**debug** ?*level*?
: Same as "**configure -debug** ?*level*?".

**errorFile** ?*filename*?
: Same as "**configure -errfile** ?*filename*?".

**limitConstraints** ?*boolean*?
: Same as "**configure -limitconstraints** ?*boolean*?".

**loadFile** ?*filename*?
: Same as "**configure -loadfile** ?*filename*?".

**loadScript** ?*script*?
: Same as "**configure -load** ?*script*?".

**match** ?*patternList*?
: Same as "**configure -match** ?*patternList*?".

**matchDirectories** ?*patternList*?
: Same as "**configure -relateddir** ?*patternList*?".

**matchFiles** ?*patternList*?
: Same as "**configure -file** ?*patternList*?".

**outputFile** ?*filename*?
: Same as "**configure -outfile** ?*filename*?".

**preserveCore** ?*level*?
: Same as "**configure -preservecore** ?*level*?".

**singleProcess** ?*boolean*?
: Same as "**configure -singleproc** ?*boolean*?".

**skip** ?*patternList*?
: Same as "**configure -skip** ?*patternList*?".

**skipDirectories** ?*patternList*?
: Same as "**configure -asidefromdir** ?*patternList*?".

**skipFiles** ?*patternList*?
: Same as "**configure -notfile** ?*patternList*?".

**temporaryDirectory** ?*directory*?
: Same as "**configure -tmpdir** ?*directory*?".

**testsDirectory** ?*directory*?
: Same as "**configure -testdir** ?*directory*?".

**verbose** ?*level*?
: Same as "**configure -verbose** ?*level*?".


## Other commands

The remaining commands provided by **tcltest** have better alternatives provided by **tcltest** or **Tcl** itself.  They are retained to support existing test suites, but should be avoided in new code.

**test** *name description optionList*
: This form of **test** was provided to enable passing many options spanning several lines to **test** as a single argument quoted by braces, rather than needing to backslash quote the newlines between arguments to **test**.  The *optionList* argument is expected to be a list with an even number of elements representing *option* and *value* arguments to pass to **test**.  However, these values are not passed directly, as in the alternate forms of **switch**.  Instead, this form makes an unfortunate attempt to overthrow Tcl's substitution rules by performing substitutions on some of the list elements as an attempt to implement a "do what I mean" interpretation of a brace-enclosed "block". The result is nearly impossible to document clearly, and for that reason this form is not recommended.  See the examples in **CREATING TEST SUITES WITH TCLTEST** below to see that this form is really not necessary to avoid backslash-quoted newlines. If you insist on using this form, examine the source code of **tcltest** if you want to know the substitution details, or just enclose the third through last argument to **test** in braces and hope for the best.

**workingDirectory** ?*directoryName*?
: Sets or returns the current working directory when the test suite is running.  The default value for workingDirectory is the directory in which the test suite was launched.  The Tcl commands **cd** and **pwd** are sufficient replacements.

**normalizeMsg** *msg*
: Returns the result of removing the "extra" newlines from *msg*, where "extra" is rather imprecise.  Tcl offers plenty of string processing commands to modify strings as you wish, and **customMatch** allows flexible matching of actual and expected results.

**normalizePath** *pathVar*
: Resolves symlinks in a path, thus creating a path without internal redirection.  It is assumed that *pathVar* is absolute. *pathVar* is modified in place.  The Tcl command **file normalize** is a sufficient replacement.


# Tests

The **test** command is the heart of the **tcltest** package. Its essential function is to evaluate a Tcl script and compare the result with an expected result.  The options of **test** define the test script, the environment in which to evaluate it, the expected result, and how the compare the actual result to the expected result.  Some configuration options of **tcltest** also influence how **test** operates.

The valid options for **test** are summarized:

```
test name description
        ?\-constraints keywordList|expression?
        ?\-setup setupScript?
        ?\-body testScript?
        ?\-cleanup cleanupScript?
        ?\-result expectedAnswer?
        ?\-output expectedOutput?
        ?\-errorOutput expectedError?
        ?\-returnCodes codeList?
        ?\-errorCode expectedErrorCode?
        ?\-match mode?
```

The *name* may be any string.  It is conventional to choose a *name* according to the pattern:

```
target-majorNum.minorNum
```

For white-box (regression) tests, the target should be the name of the C function or Tcl procedure being tested.  For black-box tests, the target should be the name of the feature being tested.  Some conventions call for the names of black-box tests to have the suffix **_bb**. Related tests should share a major number.  As a test suite evolves, it is best to have the same test name continue to correspond to the same test, so that it remains meaningful to say things like "Test foo-1.3 passed in all releases up to 3.4, but began failing in release 3.5."

During evaluation of **test**, the *name* will be compared to the lists of string matching patterns returned by **configure -match**, and **configure -skip**.  The test will be run only if *name* matches any of the patterns from **configure -match** and matches none of the patterns from **configure -skip**.

The *description* should be a short textual description of the test.  The *description* is included in output produced by the test, typically test failure messages.  Good *description* values should briefly explain the purpose of the test to users of a test suite. The name of a Tcl or C function being tested should be included in the description for regression tests.  If the test case exists to reproduce a bug, include the bug ID in the description.

Valid attributes and associated values are:

**-constraints** *keywordList*|*expression*
: The optional **-constraints** attribute can be list of one or more keywords or an expression.  If the **-constraints** value is a list of keywords, each of these keywords should be the name of a constraint defined by a call to **testConstraint**.  If any of the listed constraints is false or does not exist, the test is skipped.  If the **-constraints** value is an expression, that expression is evaluated. If the expression evaluates to true, then the test is run.
    Note that the expression form of **-constraints** may interfere with the operation of **configure -constraints** and **configure -limitconstraints**, and is not recommended.
    Appropriate constraints should be added to any tests that should not always be run.  That is, conditional evaluation of a test should be accomplished by the **-constraints** option, not by conditional evaluation of **test**.  In that way, the same number of tests are always reported by the test suite, though the number skipped may change based on the testing environment. The default value is an empty list. See **TEST CONSTRAINTS** below for a list of built-in constraints and information on how to add your own constraints.

**-setup** *script*
: The optional **-setup** attribute indicates a *script* that will be run before the script indicated by the **-body** attribute.  If evaluation of *script* raises an error, the test will fail.  The default value is an empty script.

**-body** *script*
: The **-body** attribute indicates the *script* to run to carry out the test, which must return a result that can be checked for correctness. If evaluation of *script* raises an error, the test will fail (unless the **-returnCodes** option is used to state that an error is expected). The default value is an empty script.

**-cleanup** *script*
: The optional **-cleanup** attribute indicates a *script* that will be run after the script indicated by the **-body** attribute. If evaluation of *script* raises an error, the test will fail. The default value is an empty script.

**-match** *mode*
: The **-match** attribute determines how expected answers supplied by **-result**, **-output**, and **-errorOutput** are compared.  Valid values for *mode* are **regexp**, **glob**, **exact**, and any value registered by a prior call to **customMatch**.  The default value is **exact**.

**-result** *expectedValue*
: The **-result** attribute supplies the *expectedValue* against which the return value from script will be compared. The default value is an empty string.

**-output** *expectedValue*
: The **-output** attribute supplies the *expectedValue* against which any output sent to **stdout** or **outputChannel** during evaluation of the script(s) will be compared.  Note that only output printed using the global **puts** command is used for comparison.  If **-output** is not specified, output sent to **stdout** and **outputChannel** is not processed for comparison.

**-errorOutput** *expectedValue*
: The **-errorOutput** attribute supplies the *expectedValue* against which any output sent to **stderr** or **errorChannel** during evaluation of the script(s) will be compared. Note that only output printed using the global **puts** command is used for comparison.  If **-errorOutput** is not specified, output sent to **stderr** and **errorChannel** is not processed for comparison.

**-returnCodes** *expectedCodeList*
: The optional **-returnCodes** attribute supplies *expectedCodeList*, a list of return codes that may be accepted from evaluation of the **-body** script.  If evaluation of the **-body** script returns a code not in the *expectedCodeList*, the test fails.  All return codes known to **return**, in both numeric and symbolic form, including extended return codes, are acceptable elements in the *expectedCodeList*.  Default value is "**ok return**".

**-errorCode** *expectedErrorCode*
: The optional **-errorCode** attribute supplies *expectedErrorCode*, a glob pattern that should match the error code reported from evaluation of the **-body** script.  If evaluation of the **-body** script returns a code not matching *expectedErrorCode*, the test fails.  Default value is "*****". If **-returnCodes** does not include **error** it is set to **error**.


To pass, a test must successfully evaluate its **-setup**, **-body**, and **-cleanup** scripts.  The return code of the **-body** script and its result must match expected values, and if specified, output and error data from the test must match expected **-output** and **-errorOutput** values.  If any of these conditions are not met, then the test fails. Note that all scripts are evaluated in the context of the caller of **test**.

As long as **test** is called with valid syntax and legal values for all attributes, it will not raise an error.  Test failures are instead reported as output written to **outputChannel**. In default operation, a successful test produces no output.  The output messages produced by **test** are controlled by the **configure -verbose** option as described in **CONFIGURABLE OPTIONS** below.  Any output produced by the test scripts themselves should be produced using **puts** to **outputChannel** or **errorChannel**, so that users of the test suite may easily capture output with the **configure -outfile** and **configure -errfile** options, and so that the **-output** and **-errorOutput** attributes work properly.

## Test constraints

Constraints are used to determine whether or not a test should be skipped. Each constraint has a name, which may be any string, and a boolean value.  Each **test** has a **-constraints** value which is a list of constraint names.  There are two modes of constraint control. Most frequently, the default mode is used, indicated by a setting of **configure -limitconstraints** to false.  The test will run only if all constraints in the list are true-valued.  Thus, the **-constraints** option of **test** is a convenient, symbolic way to define any conditions required for the test to be possible or meaningful.  For example, a **test** with **-constraints unix** will only be run if the constraint **unix** is true, which indicates the test suite is being run on a Unix platform.

Each **test** should include whatever **-constraints** are required to constrain it to run only where appropriate.  Several constraints are predefined in the **tcltest** package, listed below.  The registration of user-defined constraints is performed by the **testConstraint** command.  User-defined constraints may appear within a test file, or within the script specified by the **configure -load** or **configure -loadfile** options.

The following is a list of constraints predefined by the **tcltest** package itself:

*singleTestInterp*
: This test can only be run if all test files are sourced into a single interpreter.

*unix*
: This test can only be run on any Unix platform.

*win*
: This test can only be run on any Windows platform.

*nt*
: This test can only be run on any Windows NT platform.

*mac*
: This test can only be run on any Mac platform.

*unixOrWin*
: This test can only be run on a Unix or Windows platform.

*macOrWin*
: This test can only be run on a Mac or Windows platform.

*macOrUnix*
: This test can only be run on a Mac or Unix platform.

*tempNotWin*
: This test can not be run on Windows.  This flag is used to temporarily disable a test.

*tempNotMac*
: This test can not be run on a Mac.  This flag is used to temporarily disable a test.

*unixCrash*
: This test crashes if it is run on Unix.  This flag is used to temporarily disable a test.

*winCrash*
: This test crashes if it is run on Windows.  This flag is used to temporarily disable a test.

*macCrash*
: This test crashes if it is run on a Mac.  This flag is used to temporarily disable a test.

*emptyTest*
: This test is empty, and so not worth running, but it remains as a place-holder for a test to be written in the future.  This constraint has value false to cause tests to be skipped unless the user specifies otherwise.

*knownBug*
: This test is known to fail and the bug is not yet fixed.  This constraint has value false to cause tests to be skipped unless the user specifies otherwise.

*nonPortable*
: This test can only be run in some known development environment. Some tests are inherently non-portable because they depend on things like word length, file system configuration, window manager, etc. This constraint has value false to cause tests to be skipped unless the user specifies otherwise.

*userInteraction*
: This test requires interaction from the user.  This constraint has value false to causes tests to be skipped unless the user specifies otherwise.

*interactive*
: This test can only be run in if the interpreter is in interactive mode (when the global **::tcl_interactive** variable is set to 1).

*nonBlockFiles*
: This test can only be run if platform supports setting files into nonblocking mode.

*asyncPipeClose*
: This test can only be run if platform supports async flush and async close on a pipe.

*unixExecs*
: This test can only be run if this machine has Unix-style commands **cat**, **echo**, **sh**, **wc**, **rm**, **sleep**, **fgrep**, **ps**, **chmod**, and **mkdir** available.

*hasIsoLocale*
: This test can only be run if can switch to an ISO locale.

*root*
: This test can only run if Unix user is root.

*notRoot*
: This test can only run if Unix user is not root.

*eformat*
: This test can only run if app has a working version of sprintf with respect to the "e" format of floating-point numbers.

*stdio*
: This test can only be run if **interpreter** can be **open**ed as a pipe.


The alternative mode of constraint control is enabled by setting **configure -limitconstraints** to true.  With that configuration setting, all existing constraints other than those in the constraint list returned by **configure -constraints** are set to false. When the value of **configure -constraints** is set, all those constraints are set to true.  The effect is that when both options **configure -constraints** and **configure -limitconstraints** are in use, only those tests including only constraints from the **configure -constraints** list are run; all others are skipped.  For example, one might set up a configuration with

```
configure -constraints knownBug \
          -limitconstraints true \
          -verbose pass
```

to run exactly those tests that exercise known bugs, and discover whether any of them pass, indicating the bug had been fixed.

## Running all tests

The single command **runAllTests** is evaluated to run an entire test suite, spanning many files and directories.  The configuration options of **tcltest** control the precise operations.  The **runAllTests** command begins by printing a summary of its configuration to **outputChannel**.

Test files to be evaluated are sought in the directory **configure -testdir**.  The list of files in that directory that match any of the patterns in **configure -file** and match none of the patterns in **configure -notfile** is generated and sorted.  Then each file will be evaluated in turn.  If **configure -singleproc** is true, then each file will be **source**d in the caller's context.  If it is false, then a copy of **interpreter** will be **exec**'d to evaluate each file.  The multi-process operation is useful when testing can cause errors so severe that a process terminates.  Although such an error may terminate a child process evaluating one file, the main process can continue with the rest of the test suite.  In multi-process operation, the configuration of **tcltest** in the main process is passed to the child processes as command line arguments, with the exception of **configure -outfile**.  The **runAllTests** command in the main process collects all output from the child processes and collates their results into one main report.  Any reports of individual test failures, or messages requested by a **configure -verbose** setting are passed directly on to **outputChannel** by the main process.

After evaluating all selected test files, a summary of the results is printed to **outputChannel**.  The summary includes the total number of **test**s evaluated, broken down into those skipped, those passed, and those failed. The summary also notes the number of files evaluated, and the names of any files with failing tests or errors.  A list of the constraints that caused tests to be skipped, and the number of tests skipped for each is also printed.  Also, messages are printed if it appears that evaluation of a test file has caused any temporary files to be left behind in **configure -tmpdir**.

Having completed and summarized all selected test files, **runAllTests** then recursively acts on subdirectories of **configure -testdir**.  All subdirectories that match any of the patterns in **configure -relateddir** and do not match any of the patterns in **configure -asidefromdir** are examined.  If a file named **all.tcl** is found in such a directory, it will be **source**d in the caller's context. Whether or not an examined directory contains an **all.tcl** file, its subdirectories are also scanned against the **configure -relateddir** and **configure -asidefromdir** patterns.  In this way, many directories in a directory tree can have all their test files evaluated by a single **runAllTests** command.

# Configurable options

The **configure** command is used to set and query the configurable options of **tcltest**.  The valid options are:

**-singleproc** *boolean*
: Controls whether or not **runAllTests** spawns a child process for each test file.  No spawning when *boolean* is true.  Default value is false.

**-debug** *level*
: Sets the debug level to *level*, an integer value indicating how much debugging information should be printed to **stdout**.  Note that debug messages always go to **stdout**, independent of the value of **configure -outfile**.  Default value is 0.  Levels are defined as:

0
: Do not display any debug information.

1
: Display information regarding whether a test is skipped because it does not match any of the tests that were specified using by **configure -match** (userSpecifiedNonMatch) or matches any of the tests specified by **configure -skip** (userSpecifiedSkip).  Also print warnings about possible lack of cleanup or balance in test files. Also print warnings about any re-use of test names.

2
: Display the flag array parsed by the command line processor, the contents of the global **env** array, and all user-defined variables that exist in the current namespace as they are used.

3
: Display information regarding what individual procs in the test harness are doing.


**-verbose** *level*
: Sets the type of output verbosity desired to *level*, a list of zero or more of the elements **body**, **pass**, **skip**, **start**, **error**, **line**, **msec** and **usec**. Default value is "**body error**". Levels are defined as:

body (**b**)
: Display the body of failed tests

pass (**p**)
: Print output when a test passes

skip (**s**)
: Print output when a test is skipped

start (**t**)
: Print output whenever a test starts

error (**e**)
: Print errorInfo and errorCode, if they exist, when a test return code does not match its expected return code

line (**l**)
: Print source file line information of failed tests

msec (**m**)
: Print each test's execution time in milliseconds

usec (**u**)
: Print each test's execution time in microseconds

    Note that the **msec** and **usec** verbosity levels are provided as indicative measures only. They do not tackle the problem of repeatability which should be considered in performance tests or benchmarks. To use these verbosity levels to thoroughly track performance degradations, consider wrapping your test bodies with **time** commands.
    The single letter abbreviations noted above are also recognized so that "**configure -verbose pt**" is the same as "**configure -verbose {pass start}**".

**-preservecore** *level*
: Sets the core preservation level to *level*.  This level determines how stringent checks for core files are.  Default value is 0.  Levels are defined as:

0
: No checking \(em do not check for core files at the end of each test command, but do check for them in **runAllTests** after all test files have been evaluated.

1
: Also check for core files at the end of each **test** command.

2
: Check for core files at all times described above, and save a copy of each core file produced in **configure -tmpdir**.


**-limitconstraints** *boolean*
: Sets the mode by which **test** honors constraints as described in **TESTS** above.  Default value is false.

**-constraints** *list*
: Sets all the constraints in *list* to true.  Also used in combination with **configure -limitconstraints true** to control an alternative constraint mode as described in **TESTS** above. Default value is an empty list.

**-tmpdir** *directory*
: Sets the temporary directory to be used by **makeFile**, **makeDirectory**, **viewFile**, **removeFile**, and **removeDirectory** as the default directory where temporary files and directories created by test files should be created.  Default value is **workingDirectory**.

**-testdir** *directory*
: Sets the directory searched by **runAllTests** for test files and subdirectories.  Default value is **workingDirectory**.

**-file** *patternList*
: Sets the list of patterns used by **runAllTests** to determine what test files to evaluate.  Default value is "***.test**".

**-notfile** *patternList*
: Sets the list of patterns used by **runAllTests** to determine what test files to skip.  Default value is "**l.*.test**", so that any SCCS lock files are skipped.

**-relateddir** *patternList*
: Sets the list of patterns used by **runAllTests** to determine what subdirectories to search for an **all.tcl** file.  Default value is "*****".

**-asidefromdir** *patternList*
: Sets the list of patterns used by **runAllTests** to determine what subdirectories to skip when searching for an **all.tcl** file. Default value is an empty list.

**-match** *patternList*
: Set the list of patterns used by **test** to determine whether a test should be run.  Default value is "*****".

**-skip** *patternList*
: Set the list of patterns used by **test** to determine whether a test should be skipped.  Default value is an empty list.

**-load** *script*
: Sets a script to be evaluated by **loadTestedCommands**. Default value is an empty script.

**-loadfile** *filename*
: Sets the filename from which to read a script to be evaluated by **loadTestedCommands**.  This is an alternative to **-load**.  They cannot be used together.

**-outfile** *filename*
: Sets the file to which all output produced by tcltest should be written.  A file named *filename* will be **open**ed for writing, and the resulting channel will be set as the value of **outputChannel**.

**-errfile** *filename*
: Sets the file to which all error output produced by tcltest should be written.  A file named *filename* will be **open**ed for writing, and the resulting channel will be set as the value of **errorChannel**.


# Creating test suites with tcltest

The fundamental element of a test suite is the individual **test** command.  We begin with several examples.

1. Test of a script that returns normally.

```
test example-1.0 {normal return} {
    format %s value
} value
```

2. Test of a script that requires context setup and cleanup.  Note the bracing and indenting style that avoids any need for line continuation.

```
test example-1.1 {test file existence} -setup {
    set file [makeFile {} test]
} -body {
    file exists $file
} -cleanup {
    removeFile test
} -result 1
```

3. Test of a script that raises an error.

```
test example-1.2 {error return} -body {
    error message
} -returnCodes error -result message
```

4. Test with a constraint.

```
test example-1.3 {user owns created files} -constraints {
    unix
} -setup {
    set file [makeFile {} test]
} -body {
    file attributes $file -owner
} -cleanup {
    removeFile test
} -result $::tcl_platform(user)
```


At the next higher layer of organization, several **test** commands are gathered together into a single test file.  Test files should have names with the "**.test**" extension, because that is the default pattern used by **runAllTests** to find test files.  It is a good rule of thumb to have one test file for each source code file of your project. It is good practice to edit the test file and the source code file together, keeping tests synchronized with code changes.

Most of the code in the test file should be the **test** commands. Use constraints to skip tests, rather than conditional evaluation of **test**.

1. Recommended system for writing conditional tests, using constraints to guard:

```
testConstraint X [expr $myRequirement]
test goodConditionalTest {} X {
    # body
} result
```

2. Discouraged system for writing conditional tests, using **if** to guard:

```
if $myRequirement {
    test badConditionalTest {} {
        #body
    } result
}
```


Use the **-setup** and **-cleanup** options to establish and release all context requirements of the test body.  Do not make tests depend on prior tests in the file.  Those prior tests might be skipped.  If several consecutive tests require the same context, the appropriate setup and cleanup scripts may be stored in variable for passing to each tests **-setup** and **-cleanup** options.  This is a better solution than performing setup outside of **test** commands, because the setup will only be done if necessary, and any errors during setup will be reported, and not cause the test file to abort.

A test file should be able to be combined with other test files and not interfere with them, even when **configure -singleproc 1** causes all files to be evaluated in a common interpreter.  A simple way to achieve this is to have your tests define all their commands and variables in a namespace that is deleted when the test file evaluation is complete. A good namespace to use is a child namespace **test** of the namespace of the module you are testing.

A test file should also be able to be evaluated directly as a script, not depending on being called by a main **runAllTests**.  This means that each test file should process command line arguments to give the tester all the configuration control that **tcltest** provides.

After all **test**s in a test file, the command **cleanupTests** should be called.

1. Here is a sketch of a sample test file illustrating those points:

```
package require tcltest 2.5
eval ::tcltest::configure $argv
package require example
namespace eval ::example::test {
    namespace import ::tcltest::*
    testConstraint X [expr {...}]
    variable SETUP {#common setup code}
    variable CLEANUP {#common cleanup code}
    test example-1 {} -setup $SETUP -body {
        # First test
    } -cleanup $CLEANUP -result {...}
    test example-2 {} -constraints X -setup $SETUP -body {
        # Second test; constrained
    } -cleanup $CLEANUP -result {...}
    test example-3 {} {
        # Third test; no context required
    } {...}
    cleanupTests
}
namespace delete ::example::test
```


The next level of organization is a full test suite, made up of several test files.  One script is used to control the entire suite.  The basic function of this script is to call **runAllTests** after doing any necessary setup.  This script is usually named **all.tcl** because that is the default name used by **runAllTests** when combining multiple test suites into one testing run.

1. Here is a sketch of a sample test suite main script:

```
package require tcltest 2.5
package require example
::tcltest::configure -testdir \
        [file dirname [file normalize [info script]]]
eval ::tcltest::configure $argv
::tcltest::runAllTests
```


# Compatibility

A number of commands and variables in the **::tcltest** namespace provided by earlier releases of **tcltest** have not been documented here.  They are no longer part of the supported public interface of **tcltest** and should not be used in new test suites.  However, to continue to support existing test suites written to the older interface specifications, many of those deprecated commands and variables still work as before.  For example, in many circumstances, **configure** will be automatically called shortly after **package require** **tcltest 2.1** succeeds with arguments from the variable **::argv**.  This is to support test suites that depend on the old behavior that **tcltest** was automatically configured from command line arguments.  New test files should not depend on this, but should explicitly include

```
eval ::tcltest::configure $::argv
```

or

```
::tcltest::configure {*}$::argv
```

to establish a configuration from command line arguments.

# Known issues

There are two known issues related to nested evaluations of **test**. The first issue relates to the stack level in which test scripts are executed.  Tests nested within other tests may be executed at the same stack level as the outermost test.  For example, in the following code:

```
test level-1.1 {level 1} {
    -body {
        test level-2.1 {level 2} {
        }
    }
}
```

any script executed in level-2.1 may be executed at the same stack level as the script defined for level-1.1.

In addition, while two **test**s have been run, results will only be reported by **cleanupTests** for tests at the same level as test level-1.1.  However, test results for all tests run prior to level-1.1 will be available when test level-2.1 runs.  What this means is that if you try to access the test results for test level-2.1, it will may say that "m" tests have run, "n" tests have been skipped, "o" tests have passed and "p" tests have failed, where "m", "n", "o", and "p" refer to tests that were run at the same test level as test level-1.1.

Implementation of output and error comparison in the test command depends on usage of **puts** in your application code.  Output is intercepted by redefining the global **puts** command while the defined test script is being run.  Errors thrown by C procedures or printed directly from C applications will not be caught by the **test** command. Therefore, usage of the **-output** and **-errorOutput** options to **test** is useful only for pure Tcl applications that use **puts** to produce output.

