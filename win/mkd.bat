@echo off
rem RCS: @(#) $Id: mkd.bat,v 1.1.2.2 1998/10/06 03:09:26 stanton Exp $

if exist %1\tag.txt goto end

if "%OS%" == "Windows_NT" goto winnt

md %1
if errorlevel 1 goto end

goto success

:winnt
md %1
if errorlevel 1 goto end

:success
echo TAG >%1\tag.txt
echo created directory %1

:end
