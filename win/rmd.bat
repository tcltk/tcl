@echo off
rem RCS: @(#) $Id: rmd.bat,v 1.7.8.1 2002/02/05 02:22:05 wolfsuit Exp $

if not exist %1\nul goto end

echo Removing directory %1

if "%OS%" == "Windows_NT" goto winnt

deltree /y %1
if errorlevel 1 goto end
goto success

:winnt
rmdir /s /q %1
if errorlevel 1 goto end

:success
echo Deleted directory %1

:end
