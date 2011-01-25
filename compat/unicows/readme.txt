

                  =============================================
                                    libunicows
                      -------------------------------------
                                 Import library for
                    Microsoft Layer for Unicode (unicows.dll)
                    
                         http://libunicows.sourceforge.net
                  =============================================


 About
=======

Traditionally, win32 Unicode API was only available on Windows NT or 2000. If
you wanted to take advantage of Unicode in your application and support Windows
95/98 at the same time, your only option was to deploy two executables, one for
NT and one for 9X. 

Fortunately, this changed in 2001 when MS (finally!) released MSLU runtime that
allows Unicode applications to run under Windows 9X.

See these pages for details:

    http://www.microsoft.com/globaldev/handson/dev/mslu_announce.mspx
    http://msdn.microsoft.com/msdnmag/nettop.asp?page=/msdnmag/issues/01/10/MSLU/MSLU.asp&ad=ads.ddj.com/msdnmag/premium.htm

Less fortunately, this solution requires that you use a special statically
linked import library that decides at runtime whether to load symbols from
system libraries like kernel32.dll or user32.dll (in case of Windows NT) or
from unicows.dll (which provides Unicode emulation layer under 9X). This import
library is only available for Microsoft Visual C++ and is only part of the new
Platform SDK, which is rather huge package.

This library contains independent implementation of the import library. It can
be used with any C compiler (although it was only tested with Mingw32 and MSVC
so far).



 Installing libunicows
=======================

 Mingw32
---------

Simply copy libunicows.a to the lib subdirectory of your Mingw32 installation 
(e.g. c:\mingw32\lib).


 Microsoft Visual C++
----------------------

Copy unicows.lib to C:\Program Files\Visual Studio\VC98\Lib (assuming you
installed MSVC into C:\Program Files\Visual Studio and that you have version
6.0, the path may vary otherwise).

Note: This was tested only with MSVC++ 6.0, but should work with other versions
      as well.


 Borland C++
-------------

Copy unicows.lib to %BORLAND%\lib where %BORLAND% is where you installed BC++
(this directory should contain import32.lib).
 
 
 Watcom C/C++
-------------

Copy unicows.lib to %WATCOM%\lib386\nt where %WATCOM% is where you installed
the compiler.


 Usage
=======

1) Add the unicows import library BEFORE other win32 libraries on your command
line. For example, if your command line for Mingw32 was

    c++ foo.o bar.o -o foo -lkernel32 -luser32 -lgdi32 -lcomdlg32

change it to

    c++ foo.o bar.o -o foo -lunicows -lkernel32 -luser32 -lgdi32 -lcomdlg32

No other change is neccessary, you don't have to include any special headers in
your source files.


2) Download Unicows runtime from

    http://www.microsoft.com/downloads/release.asp?releaseid=30039
    
or
    
    http://download.microsoft.com/download/platformsdk/Redist/1.0/W9XMe/EN-US/unicows.exe
    
Extract unicows.dll from the package and distribute it with your application. 
Do *not* install it to Windows system directory, always copy the DLL to your 
application's directory! (Nobody wants any more of DLL hell...).
 
If your application uses Common Controls DLL (very likely) or Rich Edit control,
make sure the installer installs new enough versions that fully support Unicode
(Common Controls DLL version 5.80 and RichEdit 4.0).


 Compiling from sources
========================

1) Download the source package (libunicows-$version-src.tar.gz)

2) [optional step] Run generate.py to create assembler stubs. You will need
   Python (http://www.python.org) to run it. You don't have to do this unless
   you modified symbols.txt or src\template.asm, because generated stubs are
   already included in source package (in src\gen_asm).

3) Change to 'src' subdirectory and compile the library. You will need
   NASM (http://nasm.sourceforge.net).

     Mingw32: run make -f makefile.mingw32
     MSVC:    run nmake -f makefile.vc6

   If your compiler is not supported, you will have to create a makefile/project
   file for it. You can gen inspiration from existing makefiles. If you do so,
   please send the makefile (and if possible, compiled unicows.lib) to me,
   so that I can include it in the next release of libunicows. Thanks!


 Compiling and using unicows_wrapper.dll
=========================================

If precompiled version of libunicows is not available for your compiler, you
can use unicows_wrapper.dll (at the cost of marginally slower calls and 
the need to install unicows_wrapper.dll on NT/2k/XP boxes, too). Create
an import library for it in the same way you would do for any other DLL
and same the created import library as unicows.lib (or whatever name convention
your compiler uses, the point is to not name it unicows_wrapper.lib). For
example, this is how to do it with Borland C++ tools:

    implib unicows.lib unicows_wrapper.dll

That is. Use the library as with other compilers, i.e. put unicows.lib as the
first one on your command line, so that the symbols are found in unicows.lib
and not in e.g. kernel32.lib.

Unlike with the "native" import libraries, using unicows_wrapper.dll will make
your application depend on unicows_wrapper.dll even when installed on
Windows NT/2000/XP. Therefore, your installer must install following files
in addition to your application's binary and data:

9x/ME:     unicows.dll, unicows_wrapper.dll
NT/2k/XP:  unicows_wrapper.dll



 Contacting the author
=======================

I can be reached at this email address: vslavik@fastmail.fm
