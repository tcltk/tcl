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
[const char *]{.ret} [TclZipfs_AppHook]{.ccmd}[argcPtr, argvPtr]{.cargs}
[int]{.ret} [TclZipfs_Mount]{.ccmd}[interp, zipname, mountpoint, password]{.cargs}
[int]{.ret} [TclZipfs_MountBuffer]{.ccmd}[interp, data, dataLen, mountpoint, copy]{.cargs}
[int]{.ret} [TclZipfs_Unmount]{.ccmd}[interp, mountpoint]{.cargs}
:::

# Arguments

.AP "int" *argcPtr in Pointer to a variable holding the number of command line arguments from **main**(). .AP "char" ***argvPtr in Pointer to an array of strings containing the command line arguments to **main**(). .AP Tcl_Interp *interp in Interpreter in which the ZIP file system is mounted.  The interpreter's result is modified to hold the result or error message from the script. .AP "const char" *zipname in Name of a ZIP file. Must not be NULL when either mounting or unmounting a ZIP. .AP "const char" *mountpoint in Name of a mount point, which must be a legal Tcl file or directory name. May be NULL to query current mount points. .AP "const char" *password in An (optional) password. Use NULL if no password is wanted to read the file. .AP "const void" *data in A data buffer to mount. The data buffer must hold the contents of a ZIP archive, and must not be NULL. .AP size_t dataLen in The number of bytes in the supplied data buffer argument, *data*. .AP int copy in If non-zero, the ZIP archive in the data buffer will be internally copied before mounting, allowing the data buffer to be disposed once **TclZipfs_MountBuffer** returns. If zero, the caller guarantees that the buffer will be valid to read from for the duration of the mount.

# Description

**TclZipfs_AppHook** is a utility function to perform standard application initialization procedures, taking into account available ZIP archives as follows:

1. If the current application has a mountable ZIP archive, that archive is mounted under *ZIPFS_VOLUME***/app** as a read-only Tcl virtual file system (VFS). The value of *ZIPFS_VOLUME* can be retrieved using the Tcl command **zipfs root**.

2. If a file named **main.tcl** is located in the root directory of that file system (i.e., at *ZIPFS_VOLUME***/app/main.tcl** after the ZIP archive is mounted as described above) it is treated as the startup script for the process.

3. If the file *ZIPFS_VOLUME***/app/tcl_library/init.tcl** is present, the **tcl_library** global variable in the initial Tcl interpreter is set to *ZIPFS_VOLUME***/app/tcl_library**.

4. If the directory **tcl_library** was not found in the main application mount, the system will then search for it as either a VFS attached to the application dynamic library, or as a zip archive named **libtcl_***major***_***minor***_***patchlevel***.zip** either in the present working directory or in the standard Tcl install location. (For example, the Tcl 9.0.2 release would be searched for in a file **libtcl_9_0_2.zip**.) That archive, if located, is also mounted read-only.


On Windows, **TclZipfs_AppHook** has a slightly different signature, since it uses WCHAR instead of char. As a result, it requires the application to be compiled with the UNICODE preprocessor symbol defined (e.g., via the **-DUNICODE** compiler flag).

The result of **TclZipfs_AppHook** is the full Tcl version with build information (e.g., **9.0.0+abcdef...abcdef.gcc-1002**). The function *may* modify the variables pointed to by *argcPtr* and *argvPtr* to remove arguments; the current implementation does not do so, but callers *should not* assume that this will be true in the future.

**TclZipfs_Mount** is used to mount ZIP archives and to retrieve information about currently mounted archives. If *mountpoint* and *zipname* are both specified (i.e. non-NULL), the function mounts the ZIP archive *zipname* on the mount point given in *mountpoint*. If *password* is not NULL, it should point to the NUL terminated password protecting the archive. If not under the zipfs file system root, *mountpoint* is normalized with respect to it. For example, a mount point passed as either **mt** or **/mt** would be normalized to **//zipfs:/mt**, given that *ZIPFS_VOLUME* as returned by **zipfs root** is "//zipfs:/". An error is raised if the mount point includes a drive or UNC volume. On success, *interp*'s result is set to the normalized mount point path.

If *mountpoint* is a NULL pointer, information on all currently mounted ZIP file systems is stored in *interp*'s result as a sequence of mount points and ZIP file names.

If *mountpoint* is not NULL but *zipfile* is NULL, the path to the archive mounted at that mount point is stored as *interp*'s result. The function returns a standard Tcl result code.

**TclZipfs_MountBuffer** mounts the ZIP archive content *data* on the mount point given in *mountpoint*. Both *mountpoint* and *data* must be specified as non-NULL. The *copy* argument determines whether the buffer is internally copied before mounting or not. The ZIP archive is assumed to be not password protected. On success, *interp*'s result is set to the normalized mount point path.

**TclZipfs_Unmount** undoes the effect of **TclZipfs_Mount**, i.e., it unmounts the mounted ZIP file system that was mounted from *zipname* (at *mountpoint*). Errors are reported in the interpreter *interp*.  The result of this call is a standard Tcl result code.

**TclZipfs_AppHook** can not be used in stub-enabled extensions.

