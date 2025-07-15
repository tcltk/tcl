---
CommandName: Tclzipfs
ManualSection: 3
Version: 9.0
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - zipfs(n)
Keywords:
 - compress
 - filesystem
 - zip
Copyright:
 - Copyright (c) 2015 Jan Nijtmans <jan.nijtmans@gmail.com>
 - Copyright (c) 2015 Christian Werner <chw@ch-werner.de>
 - Copyright (c) 2017 Sean Woods <yoda@etoyoc.com>
---

# Name

TclZipfs_AppHook, TclZipfs_Mount, TclZipfs_MountBuffer, TclZipfs_Unmount - handle ZIP files as Tcl virtual filesystems

# Synopsis

::: {.synopsis} :::
[const char *]{.ret} [TclZipfs_AppHook]{.lit} [argcPtr, argvPtr]{.arg}
[int]{.ret} [TclZipfs_Mount]{.lit} [interp, zipname, mountpoint, password]{.arg}
[int]{.ret} [TclZipfs_MountBuffer]{.lit} [interp, data, dataLen, mountpoint, copy]{.arg}
[int]{.ret} [TclZipfs_Unmount]{.lit} [interp, mountpoint]{.arg}
:::

# Arguments

.AP "int" *argcPtr in Pointer to a variable holding the number of command line arguments from \fBmain\fR(). .AP "char" ***argvPtr in Pointer to an array of strings containing the command line arguments to \fBmain\fR(). .AP Tcl_Interp *interp in Interpreter in which the ZIP file system is mounted.  The interpreter's result is modified to hold the result or error message from the script. .AP "const char" *zipname in Name of a ZIP file. Must not be NULL when either mounting or unmounting a ZIP. .AP "const char" *mountpoint in Name of a mount point, which must be a legal Tcl file or directory name. May be NULL to query current mount points. .AP "const char" *password in An (optional) password. Use NULL if no password is wanted to read the file. .AP "const void" *data in A data buffer to mount. The data buffer must hold the contents of a ZIP archive, and must not be NULL. .AP size_t dataLen in The number of bytes in the supplied data buffer argument, \fIdata\fR. .AP int copy in If non-zero, the ZIP archive in the data buffer will be internally copied before mounting, allowing the data buffer to be disposed once \fBTclZipfs_MountBuffer\fR returns. If zero, the caller guarantees that the buffer will be valid to read from for the duration of the mount.

# Description

**TclZipfs_AppHook** is a utility function to perform standard application initialization procedures, taking into account available ZIP archives as follows:

1. If the current application has a mountable ZIP archive, that archive is mounted under \fIZIPFS_VOLUME\fR\fB/app\fR as a read-only Tcl virtual file system (VFS). The value of \fIZIPFS_VOLUME\fR can be retrieved using the Tcl command \fBzipfs root\fR.

2. If a file named \fBmain.tcl\fR is located in the root directory of that file system (i.e., at \fIZIPFS_VOLUME\fR\fB/app/main.tcl\fR after the ZIP archive is mounted as described above) it is treated as the startup script for the process.

3. If the file \fIZIPFS_VOLUME\fR\fB/app/tcl_library/init.tcl\fR is present, the \fBtcl_library\fR global variable in the initial Tcl interpreter is set to \fIZIPFS_VOLUME\fR\fB/app/tcl_library\fR.

4. If the directory \fBtcl_library\fR was not found in the main application mount, the system will then search for it as either a VFS attached to the application dynamic library, or as a zip archive named \fBlibtcl_\fR\fImajor\fR\fB_\fR\fIminor\fR\fB_\fR\fIpatchlevel\fR\fB.zip\fR either in the present working directory or in the standard Tcl install location. (For example, the Tcl 9.0.2 release would be searched for in a file \fBlibtcl_9_0_2.zip\fR.) That archive, if located, is also mounted read-only.


On Windows, \fBTclZipfs_AppHook\fR has a slightly different signature, since it uses WCHAR instead of char. As a result, it requires the application to be compiled with the UNICODE preprocessor symbol defined (e.g., via the \fB-DUNICODE\fR compiler flag).

The result of \fBTclZipfs_AppHook\fR is the full Tcl version with build information (e.g., \fB9.0.0+abcdef...abcdef.gcc-1002\fR). The function \fImay\fR modify the variables pointed to by \fIargcPtr\fR and \fIargvPtr\fR to remove arguments; the current implementation does not do so, but callers \fIshould not\fR assume that this will be true in the future.

**TclZipfs_Mount** is used to mount ZIP archives and to retrieve information about currently mounted archives. If \fImountpoint\fR and \fIzipname\fR are both specified (i.e. non-NULL), the function mounts the ZIP archive \fIzipname\fR on the mount point given in \fImountpoint\fR. If \fIpassword\fR is not NULL, it should point to the NUL terminated password protecting the archive. If not under the zipfs file system root, \fImountpoint\fR is normalized with respect to it. For example, a mount point passed as either \fBmt\fR or \fB/mt\fR would be normalized to \fB//zipfs:/mt\fR, given that \fIZIPFS_VOLUME\fR as returned by \fBzipfs root\fR is "//zipfs:/". An error is raised if the mount point includes a drive or UNC volume. On success, \fIinterp\fR's result is set to the normalized mount point path.

If \fImountpoint\fR is a NULL pointer, information on all currently mounted ZIP file systems is stored in \fIinterp\fR's result as a sequence of mount points and ZIP file names.

If \fImountpoint\fR is not NULL but \fIzipfile\fR is NULL, the path to the archive mounted at that mount point is stored as \fIinterp\fR's result. The function returns a standard Tcl result code.

**TclZipfs_MountBuffer** mounts the ZIP archive content \fIdata\fR on the mount point given in \fImountpoint\fR. Both \fImountpoint\fR and \fIdata\fR must be specified as non-NULL. The \fIcopy\fR argument determines whether the buffer is internally copied before mounting or not. The ZIP archive is assumed to be not password protected. On success, \fIinterp\fR's result is set to the normalized mount point path.

**TclZipfs_Unmount** undoes the effect of \fBTclZipfs_Mount\fR, i.e., it unmounts the mounted ZIP file system that was mounted from \fIzipname\fR (at \fImountpoint\fR). Errors are reported in the interpreter \fIinterp\fR.  The result of this call is a standard Tcl result code.

**TclZipfs_AppHook** can not be used in stub-enabled extensions.

