@echo off
rem RCS: @(#) $Id: rmd.bat,v 1.1.2.1 1998/09/24 23:59:49 stanton Exp $

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
