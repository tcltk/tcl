@echo off

::  This is an example batchfile for building everything. Please
::  edit this (or make your own) for your needs and wants using
::  the instructions for calling makefile.vc found in makefile.vc
::
::  RCS: @(#) $Id: buildall.vc.bat,v 1.3 2002/02/20 19:06:53 davygrvy Exp $

echo Sit back and have a couple cups of coffee while this grinds through ;)
echo You asked for *everything*, remember?
echo.

if "%MSVCDir%" == "" call C:\progra~1\micros~4\vc98\bin\vcvars32.bat
set INSTALLDIR=C:\progra~1\tcl

nmake -nologo -f makefile.vc release winhelp OPTS=none
if errorlevel 1 goto error
nmake -nologo -f makefile.vc release OPTS=static
if errorlevel 1 goto error
nmake -nologo -f makefile.vc core dlls OPTS=static,msvcrt
if errorlevel 1 goto error
nmake -nologo -f makefile.vc core OPTS=static,threads
if errorlevel 1 goto error
nmake -nologo -f makefile.vc dlls OPTS=static,msvcrt,threads
if errorlevel 1 goto error
nmake -nologo -f makefile.vc shell OPTS=threads
if errorlevel 1 goto error
goto end

:error
echo **BOOM!**

:end
pause
