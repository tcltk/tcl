@echo off
rem SCCS: %Z% $Id: rmd.bat,v 1.2 1998/07/01 18:08:34 escoffon Exp $ 

if not exist %1 goto end

if %OS% == Windows_NT goto winnt

echo Add support for Win 95 please
goto end

goto success

:winnt
rmdir %1 /s /q
if errorlevel 1 goto end

:success
echo deleted directory %1

:end
