@echo off
rem RCS: @(#) $Id: mkd.bat,v 1.7.8.1 2002/02/05 02:22:05 wolfsuit Exp $

if exist %1\nul goto end

md %1
if errorlevel 1 goto end

echo Created directory %1

:end


