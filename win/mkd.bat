@echo off
rem RCS: @(#) $Id: mkd.bat,v 1.7 2001/09/08 23:52:02 davygrvy Exp $

if exist %1\. goto end

if "%OS%" == "Windows_NT" goto winnt

md %1
if errorlevel 1 goto end

goto success

:winnt
md %1
if errorlevel 1 goto end

:success
echo created directory %1

:end


