@echo off

if "%MSVCDir%" == "" call c:\dev\devstudio60\vc98\bin\vcvars32.bat
set INSTALLDIR=d:\tclTestArea

nmake -nologo -f makefile.vc %1 release winhelp OPTS=none
nmake -nologo -f makefile.vc %1 release OPTS=static
nmake -nologo -f makefile.vc %1 core dlls OPTS=static,msvcrt
nmake -nologo -f makefile.vc %1 core OPTS=static,threads
nmake -nologo -f makefile.vc %1 dlls OPTS=static,msvcrt,threads
nmake -nologo -f makefile.vc %1 shell OPTS=threads
pause
