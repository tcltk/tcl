@echo off
rem RCS: @(#) $Id: mkd.bat,v 1.3 1998/09/14 18:40:19 stanton Exp $

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
