@echo off
rem RCS: @(#) $Id: mkd.bat,v 1.8 2001/11/10 10:38:47 davygrvy Exp $

if exist %1\nul goto end

md %1
if errorlevel 1 goto end

echo Created directory %1

:end


