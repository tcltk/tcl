@echo off

rem
rem   This is an example batchfile for building everything. Please
rem   edit this (or make your own) for your needs and wants using
rem   the instructions for calling makefile.vc found in makefile.vc
rem
rem RCS: @(#) $Id: buildall.vc.bat,v 1.2.4.1 2002/02/05 02:22:05 wolfsuit Exp $

if "%MSVCDir%" == "" call c:\progra~1\micros~4\vc98\bin\vcvars32.bat
set INSTALLDIR=d:\tclTestArea

nmake -nologo -f makefile.vc release winhelp OPTS=none
nmake -nologo -f makefile.vc release OPTS=static
nmake -nologo -f makefile.vc core dlls OPTS=static,msvcrt
nmake -nologo -f makefile.vc core OPTS=static,threads
nmake -nologo -f makefile.vc dlls OPTS=static,msvcrt,threads
nmake -nologo -f makefile.vc shell OPTS=threads
pause
