@echo off
rem RCS: @(#) $Id: mkd.bat,v 1.5 1999/12/22 00:00:16 hobbs Exp $

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
