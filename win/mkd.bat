@echo off
rem RCS: @(#) $Id: mkd.bat,v 1.1.2.1 1998/09/24 23:59:49 stanton Exp $

if exist %1 goto end

if %OS% == Windows_NT goto winnt

echo Add support for Win 95 please
goto end

goto success

:winnt
md %1
if errorlevel 1 goto end

:success
echo created directory %1

:end
