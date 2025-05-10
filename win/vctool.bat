@echo off
REM Pass /? as argument for usage.

IF DEFINED VCINSTALLDIR goto setup

echo "Not in a Visual Studio command prompt."
exit /B 1

:setup
setlocal

set "tool=%0"

REM Get the current directory
set "currentDir=%CD%"

REM Use FOR command to get the parent directory
for %%I in ("%currentDir%") do set "parentDir=%%~dpI"

REM Remove the trailing backslash
set "parentDir=%parentDir:~0,-1%"

REM Use FOR command again to get the parent of the parent directory
for %%J in ("%parentDir%") do set "grandParentDir=%%~dpJ"

REM Remove the trailing backslash
set "grandParentDir=%grandParentDir:~0,-1%"

REM Use FOR command to extract the last component
for %%I in ("%grandParentDir%") do set "grandParentTail=%%~nxI"

REM Extract the drive letter
for %%I in ("%currentDir%") do set "driveLetter=%%~dI"

set ARCH=%VSCMD_ARG_TGT_ARCH%
if "%TCLINSTALLROOT%" == "" (
    set INSTROOT=%driveLetter%\Tcl\%grandParentTail%\%ARCH%
) else (
    set INSTROOT=%TCLINSTALLROOT%\%grandParentTail%\%ARCH%
)

REM Parse options
:options
if "%1" == "" goto dobuilds
if "%1" == "/?" goto help
if "%1" == "-?" goto help
if /i "%1" == "/help" goto help
if "%1" == "all" goto all
if "%1" == "shared" goto shared
if "%1" == "static" goto static
if "%1" == "shared_noembed" goto shared_noembed
if "%1" == "static_noembed" goto static_noembed
if "%1" == "compile" goto compile
if "%1" == "test" goto targets
if "%1" == "install" goto targets
if "%1" == "runshell" goto targets
if "%1" == "debug" goto debug
goto help

:debug
set debug=1
shift
goto options

:shared
set shared=1
shift
goto options

:static
set static=1
shift
goto options

:shared_noembed
set shared_noembed=1
shift
goto options

:static_noembed
set static_noembed=1
shift
goto options

:all
set shared=1
set static=1
set shared_noembed=1
set static_noembed=1
shift
goto options

:targets
set TARGETS=%TARGETS% %1
shift
goto options

REM The makefile.vc compilation target is called "release"
:compile
set TARGETS=%TARGETS% release
shift
goto options

:dobuilds
if "%shared%%static%%shared_noembed%%static_noembed%" == "" (
    echo At least one of shared, static, shared_noembed, static_noembed, all must be specified.
    echo For more help, type "%0 help"
    goto error
)

if DEFINED shared (
    call :runmake shared
)
if DEFINED shared_noembed (
call :runmake shared-noembed noembed
)
if DEFINED static (
call :runmake static static
)
if DEFINED static_noembed (
call :runmake static-noembed "static,noembed"
)

:done
endlocal
exit /b 0

:error
endlocal
exit /b 1

:: call :runmake dir opts
:runmake
if "%debug%" == "" (
    nmake /s /f makefile.vc OUT_DIR=%currentDir%\vc-%ARCH%-%1 TMP_DIR=%currentDir%\vc-%ARCH%-%1\objs OPTS=pdbs,%2 INSTALLDIR=%INSTROOT%-%1 %TARGETS% && goto error
) else (
    nmake /s /f makefile.vc OUT_DIR=%currentDir%\vc-%ARCH%-%1-debug TMP_DIR=%currentDir%\vc-%ARCH%-%1-debug\objs OPTS=pdbs,%2 cdebug="-Zi -Od" INSTALLDIR=%INSTROOT%-%1-debug %TARGETS% && goto error
)
goto eof

:help
echo.
echo Usage: %0 arg ...
echo.
echo where each arg may be either a build config or a target or "debug".
echo.
echo Configs: shared, static, shared_noembed, static_noembed, all
echo Targets: compile (default), test, install
echo.
echo Multiple configs and targets may be specified and intermixed.
echo At least one config must be specified. If no targets specified,
echo default is compile. If multiple targets are present, they
echo are built in specified order.
echo.
echo If "debug" is supplied as an argument, the build has optimizations
echo disabled and full debug information.
echo.
echo If environment variable TCLINSTALLROOT is defined, install target
echo will be subdirectory under it named after the grandparent of the
echo current directory. TCLINSTALLROOT defaults to the X:\Tcl where
echo X is the current drive.
echo.
echo For example, if the current directory C:\src\core-8-branch\tcl\win,
echo install directories will be echo under c:\Tcl\core-8-branch\ as
echo x64-shared, x64-static etc. or x64-shared-debug etc. if "debug" passed.
echo.
echo Examples:
echo    %tool% shared                       (Builds default "shared" config)
echo    %tool% shared test                  (Tests shared build)
echo    %tool% static compile test          (Builds and tests static build)
echo    %tool% all debug                    (Build debug versions of all configs)
echo    %tool% all compile install debug    (Builds and installs all configs)
echo    %tool% shared static shared_noembed (Builds three configs)
goto done
