@echo off
::  This is an example batchfile for building everything. Please
::  edit this (or make your own) for your needs and wants using
::  the instructions for calling makefile.vc found in makefile.vc
::
::  RCS: @(#) $Id: buildall.vc.bat,v 1.6.4.2 2004/03/26 22:28:30 dgp Exp $

if "%1" == "/?" goto help
if /i "%1" == "/help" goto help

:: reset errorlevel
cd > nul

:: We need to run the development environment batch script that comes
:: with developer studio (v4,5,6,7,etc...)  All have it.  These paths
:: might not be correct.  You may need to edit these.
::
if not defined MSDevDir (
    call "C:\Program Files\Microsoft Developer Studio\vc98\bin\vcvars32.bat"
    ::call "C:\Program Files\Microsoft Developer Studio\vc\bin\vcvars32.bat"
    ::call c:\dev\devstudio60\vc98\bin\vcvars32.bat
    if errorlevel 1 goto no_vcvars
)


echo.
echo Sit back and have a cup of coffee while this grinds through ;)
echo You asked for *everything*, remember?
echo.
title Building Tcl, please wait...


:: makefile.vc uses this for its default anyways, but show its use here
:: just to be explicit and convey understanding to the user.  Setting
:: the INSTALLDIR envar prior to running this batchfile affects all builds.
::
if "%INSTALLDIR%" == "" set INSTALLDIR=C:\Program Files\Tcl


:: Build the normal stuff along with the help file.
::
nmake -nologo -f makefile.vc release winhelp OPTS=none %1
if errorlevel 1 goto error

:: Build the static core, dlls and shell.
::
nmake -nologo -f makefile.vc release OPTS=static %1
if errorlevel 1 goto error

:: Build the special static libraries that use the dynamic runtime.
::
nmake -nologo -f makefile.vc core dlls OPTS=static,msvcrt %1
if errorlevel 1 goto error

:: Build the core and shell for thread support.
::
nmake -nologo -f makefile.vc shell OPTS=threads %1
if errorlevel 1 goto error

:: Build a static, thread support core library with a shell.
::
nmake -nologo -f makefile.vc shell OPTS=static,threads %1
if errorlevel 1 goto error

:: Build the special static libraries that use the dynamic runtime,
:: but now with thread support.
::
nmake -nologo -f makefile.vc core dlls OPTS=static,msvcrt,threads %1
if errorlevel 1 goto error

goto end

:error
echo *** BOOM! ***
goto end

:no_vcvars
echo vcvars32.bat not found.  You'll need to edit this batch script.
goto out

:help
title buildall.vc.bat help message
echo usage:
echo   %0           : builds Tcl for all build types (do this first)
echo   %0 install   : installs all the builds        (do this second)
echo.
goto out

:end
title Building Tcl, please wait... DONE!
echo DONE!
goto out

:out
pause
title Command Prompt
