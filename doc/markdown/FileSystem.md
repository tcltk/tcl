---
CommandName: Filesystem
ManualSection: 3
Version: 8.4
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - cd(n)
 - file(n)
 - filename(n)
 - load(n)
 - open(n)
 - pwd(n)
 - source(n)
 - unload(n)
Keywords:
 - stat
 - access
 - filesystem
 - vfs
 - virtual filesystem
Copyright:
 - Copyright (c) 2001 Vincent Darley
 - Copyright (c) 2008-2010 Donal K. Fellows
---

# Name

Tcl_FSRegister, Tcl_FSUnregister, Tcl_FSData, Tcl_FSMountsChanged, Tcl_FSGetFileSystemForPath, Tcl_FSGetPathType, Tcl_FSCopyFile, Tcl_FSCopyDirectory, Tcl_FSCreateDirectory, Tcl_FSDeleteFile, Tcl_FSRemoveDirectory, Tcl_FSRenameFile, Tcl_FSListVolumes, Tcl_FSEvalFile, Tcl_FSEvalFileEx, Tcl_FSLoadFile, Tcl_FSUnloadFile, Tcl_FSMatchInDirectory, Tcl_FSLink, Tcl_FSLstat, Tcl_FSUtime, Tcl_FSFileAttrsGet, Tcl_FSFileAttrsSet, Tcl_FSFileAttrStrings, Tcl_FSStat, Tcl_FSAccess, Tcl_FSOpenFileChannel, Tcl_FSGetCwd, Tcl_FSChdir, Tcl_FSPathSeparator, Tcl_FSJoinPath, Tcl_FSSplitPath, Tcl_FSEqualPaths, Tcl_FSGetNormalizedPath, Tcl_FSJoinToPath, Tcl_FSConvertToPathType, Tcl_FSGetInternalRep, Tcl_FSGetTranslatedPath, Tcl_FSGetTranslatedStringPath, Tcl_FSNewNativePath, Tcl_FSGetNativePath, Tcl_FSFileSystemInfo, Tcl_GetAccessTimeFromStat, Tcl_GetBlockSizeFromStat, Tcl_GetBlocksFromStat, Tcl_GetChangeTimeFromStat, Tcl_GetDeviceTypeFromStat, Tcl_GetFSDeviceFromStat, Tcl_GetFSInodeFromStat, Tcl_GetGroupIdFromStat, Tcl_GetLinkCountFromStat, Tcl_GetModeFromStat, Tcl_GetModificationTimeFromStat, Tcl_GetSizeFromStat, Tcl_GetUserIdFromStat, Tcl_AllocStatBuf, Tcl_FSTildeExpand - procedures to interact with any filesystem

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl_FSRegister]{.ccmd}[clientData, fsPtr]{.cargs}
[int]{.ret} [Tcl_FSUnregister]{.ccmd}[fsPtr]{.cargs}
[void *]{.ret} [Tcl_FSData]{.ccmd}[fsPtr]{.cargs}
[Tcl_FSMountsChanged]{.ccmd}[fsPtr]{.cargs}
[const Tcl_Filesystem *]{.ret} [Tcl_FSGetFileSystemForPath]{.ccmd}[pathPtr]{.cargs}
[Tcl_PathType]{.ret} [Tcl_FSGetPathType]{.ccmd}[pathPtr]{.cargs}
[int]{.ret} [Tcl_FSCopyFile]{.ccmd}[srcPathPtr, destPathPtr]{.cargs}
[int]{.ret} [Tcl_FSCopyDirectory]{.ccmd}[srcPathPtr, destPathPtr, errorPtr]{.cargs}
[int]{.ret} [Tcl_FSCreateDirectory]{.ccmd}[pathPtr]{.cargs}
[int]{.ret} [Tcl_FSDeleteFile]{.ccmd}[pathPtr]{.cargs}
[int]{.ret} [Tcl_FSRemoveDirectory]{.ccmd}[pathPtr, recursive, errorPtr]{.cargs}
[int]{.ret} [Tcl_FSRenameFile]{.ccmd}[srcPathPtr, destPathPtr]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_FSListVolumes]{.ccmd}[void]{.cargs}
[int]{.ret} [Tcl_FSEvalFileEx]{.ccmd}[interp, pathPtr, encodingName]{.cargs}
[int]{.ret} [Tcl_FSEvalFile]{.ccmd}[interp, pathPtr]{.cargs}
[int]{.ret} [Tcl_FSLoadFile]{.ccmd}[interp, pathPtr, sym1, sym2, proc1Ptr, proc2Ptr, loadHandlePtr, unloadProcPtr]{.cargs}
[int]{.ret} [Tcl_FSUnloadFile]{.ccmd}[interp, loadHandle]{.cargs}
[int]{.ret} [Tcl_FSMatchInDirectory]{.ccmd}[interp, resultPtr, pathPtr, pattern, types]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_FSLink]{.ccmd}[linkNamePtr, toPtr, linkAction]{.cargs}
[int]{.ret} [Tcl_FSLstat]{.ccmd}[pathPtr, statPtr]{.cargs}
[int]{.ret} [Tcl_FSUtime]{.ccmd}[pathPtr, tval]{.cargs}
[int]{.ret} [Tcl_FSFileAttrsGet]{.ccmd}[interp, index, pathPtr, objPtrRef]{.cargs}
[int]{.ret} [Tcl_FSFileAttrsSet]{.ccmd}[interp, index, pathPtr, objPtr]{.cargs}
[const char *const *]{.ret} [Tcl_FSFileAttrStrings]{.ccmd}[pathPtr, objPtrRef]{.cargs}
[int]{.ret} [Tcl_FSStat]{.ccmd}[pathPtr, statPtr]{.cargs}
[int]{.ret} [Tcl_FSAccess]{.ccmd}[pathPtr, mode]{.cargs}
[Tcl_Channel]{.ret} [Tcl_FSOpenFileChannel]{.ccmd}[interp, pathPtr, modeString, permissions]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_FSGetCwd]{.ccmd}[interp]{.cargs}
[int]{.ret} [Tcl_FSChdir]{.ccmd}[pathPtr]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_FSPathSeparator]{.ccmd}[pathPtr]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_FSJoinPath]{.ccmd}[listObj, elements]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_FSSplitPath]{.ccmd}[pathPtr, lenPtr]{.cargs}
[int]{.ret} [Tcl_FSEqualPaths]{.ccmd}[firstPtr, secondPtr]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_FSGetNormalizedPath]{.ccmd}[interp, pathPtr]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_FSJoinToPath]{.ccmd}[basePtr, objc, objv]{.cargs}
[int]{.ret} [Tcl_FSConvertToPathType]{.ccmd}[interp, pathPtr]{.cargs}
[void *]{.ret} [Tcl_FSGetInternalRep]{.ccmd}[pathPtr, fsPtr]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_FSGetTranslatedPath]{.ccmd}[interp, pathPtr]{.cargs}
[const char *]{.ret} [Tcl_FSGetTranslatedStringPath]{.ccmd}[interp, pathPtr]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_FSNewNativePath]{.ccmd}[fsPtr, clientData]{.cargs}
[const void *]{.ret} [Tcl_FSGetNativePath]{.ccmd}[pathPtr]{.cargs}
[Tcl_Obj *]{.ret} [Tcl_FSFileSystemInfo]{.ccmd}[pathPtr]{.cargs}
[int]{.ret} [Tcl_FSTildeExpand]{.ccmd}[interp, pathStr, dsPtr]{.cargs}
[Tcl_StatBuf *]{.ret} [Tcl_AllocStatBuf]{.ccmd}[]{.cargs}
[long long]{.ret} [Tcl_GetAccessTimeFromStat]{.ccmd}[statPtr]{.cargs}
[unsigned]{.ret} [Tcl_GetBlockSizeFromStat]{.ccmd}[statPtr]{.cargs}
[unsigned long long]{.ret} [Tcl_GetBlocksFromStat]{.ccmd}[statPtr]{.cargs}
[long long]{.ret} [Tcl_GetChangeTimeFromStat]{.ccmd}[statPtr]{.cargs}
[int]{.ret} [Tcl_GetDeviceTypeFromStat]{.ccmd}[statPtr]{.cargs}
[unsigned]{.ret} [Tcl_GetFSDeviceFromStat]{.ccmd}[statPtr]{.cargs}
[unsigned]{.ret} [Tcl_GetFSInodeFromStat]{.ccmd}[statPtr]{.cargs}
[int]{.ret} [Tcl_GetGroupIdFromStat]{.ccmd}[statPtr]{.cargs}
[int]{.ret} [Tcl_GetLinkCountFromStat]{.ccmd}[statPtr]{.cargs}
[unsigned]{.ret} [Tcl_GetModeFromStat]{.ccmd}[statPtr]{.cargs}
[long long]{.ret} [Tcl_GetModificationTimeFromStat]{.ccmd}[statPtr]{.cargs}
[unsigned long long]{.ret} [Tcl_GetSizeFromStat]{.ccmd}[statPtr]{.cargs}
[int]{.ret} [Tcl_GetUserIdFromStat]{.ccmd}[statPtr]{.cargs}
:::

# Arguments

.AP "const Tcl_Filesystem" *fsPtr in Points to a structure containing the addresses of procedures that can be called to perform the various filesystem operations. .AP "const char" *pathStr in Pointer to a NUL terminated string representing a file system path. .AP Tcl_Obj *pathPtr in The path represented by this value is used for the operation in question. If the value does not already have an internal **path** representation, it will be converted to have one. .AP Tcl_Obj *srcPathPtr in As for *pathPtr*, but used for the source file for a copy or rename operation. .AP Tcl_Obj *destPathPtr in As for *pathPtr*, but used for the destination filename for a copy or rename operation. .AP int recursive in Whether to remove subdirectories and their contents as well. .AP "const char" *encodingName in The encoding of the data stored in the file identified by *pathPtr* and to be evaluated. .AP "const char" *pattern in Only files or directories matching this pattern will be returned. .AP Tcl_GlobTypeData *types in Only files or directories matching the type descriptions contained in this structure will be returned. This parameter may be NULL. .AP Tcl_Interp *interp in Interpreter to use either for results, evaluation, or reporting error messages. .AP void *clientData in The native description of the path value to create. .AP Tcl_Obj *firstPtr in The first of two path values to compare. The value may be converted to **path** type. .AP Tcl_Obj *secondPtr in The second of two path values to compare. The value may be converted to **path** type. .AP Tcl_Obj *listObj in The list of path elements to operate on with a **join** operation. .AP Tcl_Size elements in The number of elements in the *listObj* which should be joined together. If negative, then all elements are joined. .AP Tcl_Obj **errorPtr out In the case of an error, filled with a value containing the name of the file which caused an error in the various copy/rename operations. .AP int index in The index of the attribute in question. .AP Tcl_Obj *objPtr in The value to set in the operation. .AP Tcl_Obj **objPtrRef out Filled with a value containing the result of the operation. .AP Tcl_Obj *resultPtr out Preallocated value in which to store (using **Tcl_ListObjAppendElement**) the list of files or directories which are successfully matched. .AP int mode in Mask consisting of one or more of R_OK, W_OK, X_OK and F_OK. R_OK, W_OK and X_OK request checking whether the file exists and  has  read, write and  execute  permissions, respectively. F_OK just requests checking for the existence of the file. .AP Tcl_StatBuf *statPtr out The structure that contains the result of a stat or lstat operation. .AP "const char" *sym1 in Name of a procedure to look up in the file's symbol table .AP "const char" *sym2 in Name of a procedure to look up in the file's symbol table .AP Tcl_LibraryInitProc **proc1Ptr out Filled with the init function for this code. .AP Tcl_LibraryInitProc **proc2Ptr out Filled with the safe-init function for this code. .AP void **clientDataPtr out Filled with the clientData value to pass to this code's unload function when it is called. .AP Tcl_LoadHandle *loadHandlePtr out Filled with an abstract token representing the loaded file. .AP Tcl_FSUnloadFileProc **unloadProcPtr out Filled with the function to use to unload this piece of code. .AP Tcl_LoadHandle loadHandle in Handle to the loaded library to be unloaded. .AP utimbuf *tval in The access and modification times in this structure are read and used to set those values for a given file. .AP "const char" *modeString in Specifies how the file is to be accessed. May have any of the values allowed for the *mode* argument to the Tcl **open** command. .AP int permissions in POSIX-style permission flags such as 0644. If a new file is created, these permissions will be set on the created file. .AP "Tcl_Size \&| int" *lenPtr out Filled with the number of elements in the split path. May be (Tcl_Size *)NULL when not used. If it points to a variable which type is not **Tcl_Size**, a compiler warning will be generated. If your extensions is compiled with -DTCL_8_API, this function will return NULL for paths having more than INT_MAX elements (which should trigger proper error-handling), otherwise expect it to crash. .AP Tcl_Obj *basePtr in The base path on to which to join the given elements. May be NULL. .AP Tcl_Size objc in The number of elements in *objv*. .AP "Tcl_Obj *const" objv[] in The elements to join to the given base path. .AP Tcl_Obj *linkNamePtr in The name of the link to be created or read. .AP Tcl_Obj *toPtr in What the link called *linkNamePtr* should be linked to, or NULL if the symbolic link specified by *linkNamePtr* is to be read. .AP int linkAction in OR-ed combination of flags indicating what kind of link should be created (will be ignored if *toPtr* is NULL). Valid bits to set are **TCL_CREATE_SYMBOLIC_LINK** and **TCL_CREATE_HARD_LINK**. When both flags are set and the underlying filesystem can do either, symbolic links are preferred. .AP Tcl_DString *dsPtr out Pointer to a **Tcl_DString** to hold an output string result.

# Description

There are several reasons for calling the **Tcl_FS** API functions (e.g.\ **Tcl_FSAccess** and **Tcl_FSStat**) rather than calling system level functions like **access** and **stat** directly. First, they will work cross-platform, so an extension which calls them should work unmodified on Unix and Windows. Second, the Windows implementation of some of these functions fixes some bugs in the system level calls. Third, these function calls deal with any "Utf to platform-native" path conversions which may be required (and may cache the results of such conversions for greater efficiency on subsequent calls). Fourth, and perhaps most importantly, all of these functions are "virtual filesystem aware". Any virtual filesystem (VFS for short) which has been registered (through **Tcl_FSRegister**) may reroute file access to alternative media or access methods. This means that all of these functions (and therefore the corresponding **file**, **glob**, **pwd**, **cd**, **open**, etc.\ Tcl commands) may be operate on "files" which are not native files in the native filesystem. This also means that any Tcl extension which accesses the filesystem (FS for short) through this API is automatically "virtual filesystem aware". Of course, if an extension accesses the native filesystem directly (through platform-specific APIs, for example), then Tcl cannot intercept such calls.

If appropriate VFSes have been registered, the "files" may, to give two examples, be remote (e.g.\ situated on a remote ftp server) or archived (e.g.\ lying inside a .zip archive). Such registered filesystems provide a lookup table of functions to implement all or some of the functionality listed here. Finally, the **Tcl_FSStat** and **Tcl_FSLstat** calls abstract away from what the "struct stat" buffer is actually declared to be, allowing the same code to be used both on systems with and systems without support for files larger than 2GB in size.

The **Tcl_FS** API is **Tcl_Obj**-ified and may cache internal representations and other path-related strings (e.g.\ the current working directory). One side-effect of this is that one must not pass in values with a reference count of zero to any of these functions. If such calls were handled, they might result in memory leaks (under some circumstances, the filesystem code may wish to retain a reference to the passed in value, and so one must not assume that after any of these calls return, the value still has a reference count of zero - it may have been incremented) or in a direct segmentation fault (or other memory access error) due to the value being freed part way through the complex value manipulation required to ensure that the path is fully normalized and absolute for filesystem determination. The practical lesson to learn from this is that

```
Tcl_Obj *path = Tcl_NewStringObj(...);
Tcl_FSWhatever(path);
Tcl_DecrRefCount(path);
```

is wrong, and may cause memory errors. The *path* must have its reference count incremented before passing it in, or decrementing it. For this reason, values with a reference count of zero are considered not to be valid filesystem paths and calling any Tcl_FS API function with such a value will result in no action being taken.

## Fs api functions

**Tcl_FSCopyFile** attempts to copy the file given by *srcPathPtr* to the path name given by *destPathPtr*. If the two paths given lie in the same filesystem (according to **Tcl_FSGetFileSystemForPath**) then that filesystem's "copy file" function is called (if it is non-NULL). Otherwise the function returns -1 and sets the **errno** global C variable to the "EXDEV" POSIX error code (which signifies a "cross-domain link").

**Tcl_FSCopyDirectory** attempts to copy the directory given by *srcPathPtr* to the path name given by *destPathPtr*. If the two paths given lie in the same filesystem (according to **Tcl_FSGetFileSystemForPath**) then that filesystem's "copy file" function is called (if it is non-NULL). Otherwise the function returns -1 and sets the **errno** global C variable to the "EXDEV" POSIX error code (which signifies a "cross-domain link").

**Tcl_FSCreateDirectory** attempts to create the directory given by *pathPtr* by calling the owning filesystem's "create directory" function.

**Tcl_FSDeleteFile** attempts to delete the file given by *pathPtr* by calling the owning filesystem's "delete file" function.

**Tcl_FSRemoveDirectory** attempts to remove the directory given by *pathPtr* by calling the owning filesystem's "remove directory" function.

**Tcl_FSRenameFile** attempts to rename the file or directory given by *srcPathPtr* to the path name given by *destPathPtr*. If the two paths given lie in the same filesystem (according to **Tcl_FSGetFileSystemForPath**) then that filesystem's "rename file" function is called (if it is non-NULL). Otherwise the function returns -1 and sets the **errno** global C variable to the "EXDEV" POSIX error code (which signifies a "cross-domain link").

**Tcl_FSListVolumes** calls each filesystem which has a non-NULL "list volumes" function and asks them to return their list of root volumes. It accumulates the return values in a list which is returned to the caller (with a reference count of 0).

**Tcl_FSEvalFileEx** reads the file given by *pathPtr* using the encoding identified by *encodingName* and evaluates its contents as a Tcl script. It returns the same information as **Tcl_EvalObjEx**. If *encodingName* is NULL, the utf-8 encoding is used for reading the file contents. If the file could not be read then a Tcl error is returned to describe why the file could not be read. The eofchar for files is "\x1A" (^Z) for all platforms. If you require a "^Z" in code for string comparison, you can use "\x1A", which will be safely substituted by the Tcl interpreter into "^Z". **Tcl_FSEvalFile** is a simpler version of **Tcl_FSEvalFileEx** that always uses the utf-8 encoding when reading the file.

**Tcl_FSLoadFile** dynamically loads a binary code file into memory and returns the addresses of two procedures within that file, if they are defined. The appropriate function for the filesystem to which *pathPtr* belongs will be called. If that filesystem does not implement this function (most virtual filesystems will not, because of OS limitations in dynamically loading binary code), Tcl will attempt to copy the file to a temporary directory and load that temporary file. **Tcl_FSUnloadFile** reverses the operation, asking for the library indicated by the *loadHandle* to be removed from the process. Note that, unlike with the **unload** command, this does not give the library any opportunity to clean up.

Both the above functions return a standard Tcl completion code. If an error occurs, an error message is left in the *interp*'s result.

The token provided via the variable indicated by *loadHandlePtr* may be used with **Tcl_FindSymbol**.

**Tcl_FSMatchInDirectory** is used by the globbing code to search a directory for all files which match a given pattern. The appropriate function for the filesystem to which *pathPtr* belongs will be called.

The return value is a standard Tcl result indicating whether an error occurred in globbing. Error messages are placed in interp (unless interp is NULL, which is allowed), but good results are placed in the resultPtr given.

Note that the **glob** code implements recursive patterns internally, so this function will only ever be passed simple patterns, which can be matched using the logic of **string match**. To handle recursion, Tcl will call this function frequently asking only for directories to be returned. A special case of being called with a NULL pattern indicates that the path needs to be checked only for the correct type.

**Tcl_FSLink** replaces the library version of **readlink**, and extends it to support the creation of links. The appropriate function for the filesystem to which *linkNamePtr* belongs will be called.

If the *toPtr* is NULL, a "read link" action is performed. The result is a Tcl_Obj specifying the contents of the symbolic link given by *linkNamePtr*, or NULL if the link could not be read. The result is owned by the caller, which should call **Tcl_DecrRefCount** when the result is no longer needed. If the *toPtr* is not NULL, Tcl should create a link of one of the types passed in in the *linkAction* flag. This flag is an OR'ed combination of **TCL_CREATE_SYMBOLIC_LINK** and **TCL_CREATE_HARD_LINK**. Where a choice exists (i.e.\ more than one flag is passed in), the Tcl convention is to prefer symbolic links. When a link is successfully created, the return value should be *toPtr* (which is therefore already owned by the caller). If unsuccessful, NULL is returned.

**Tcl_FSLstat** fills the *Tcl_StatBuf* structure *statPtr* with information about the specified file. You do not need any access rights to the file to get this information but you need search rights to all directories named in the path leading to the file. The *Tcl_StatBuf* structure includes info regarding device, inode (always 0 on Windows), privilege mode, nlink (always 1 on Windows), user id (always 0 on Windows), group id (always 0 on Windows), rdev (same as device on Windows), size, last access time, last modification time, and last metadata change time. See **PORTABLE STAT RESULT API** for a description of how to write portable code to allocate and access the *Tcl_StatBuf* structure.

If *path* exists, **Tcl_FSLstat** returns 0 and the stat structure is filled with data. Otherwise, -1 is returned, and no stat info is given.

**Tcl_FSUtime** replaces the library version of utime.

This returns 0 on success and -1 on error (as per the **utime** documentation). If successful, the function will update the "atime" and "mtime" values of the file given.

**Tcl_FSFileAttrsGet** implements read access for the hookable **file attributes** subcommand. The appropriate function for the filesystem to which *pathPtr* belongs will be called.

If the result is **TCL_OK**, then a value was placed in *objPtrRef*, which will only be temporarily valid (unless **Tcl_IncrRefCount** is called).

**Tcl_FSFileAttrsSet** implements write access for the hookable **file attributes** subcommand. The appropriate function for the filesystem to which *pathPtr* belongs will be called.

**Tcl_FSFileAttrStrings** implements part of the hookable **file attributes** subcommand. The appropriate function for the filesystem to which *pathPtr* belongs will be called.

The called procedure may either return an array of strings, or may instead return NULL and place a Tcl list into the given *objPtrRef*. Tcl will take that list and first increment its reference count before using it. On completion of that use, Tcl will decrement its reference count. Hence if the list should be disposed of by Tcl when done, it should have a reference count of zero, and if the list should not be disposed of, the filesystem should ensure it retains a reference count to the value.

**Tcl_FSAccess** checks whether the process would be allowed to read, write or test for existence of the file (or other filesystem object) whose name is *pathname*. If *pathname* is a symbolic link on Unix, then permissions of the file referred by this symbolic link are tested.

On success (all requested permissions granted), zero is returned. On error (at least one bit in mode asked for a permission that is denied, or some other error occurred), -1 is returned.

**Tcl_FSStat** fills the *Tcl_StatBuf* structure *statPtr* with information about the specified file. You do not need any access rights to the file to get this information but you need search rights to all directories named in the path leading to the file. The *Tcl_StatBuf* structure includes info regarding device, inode (always 0 on Windows), privilege mode, nlink (always 1 on Windows), user id (always 0 on Windows), group id (always 0 on Windows), rdev (same as device on Windows), size, last access time, last modification time, and last metadata change time. See **PORTABLE STAT RESULT API** for a description of how to write portable code to allocate and access the *Tcl_StatBuf* structure.

If *path* exists, **Tcl_FSStat** returns 0 and the stat structure is filled with data. Otherwise, -1 is returned, and no stat info is given.

**Tcl_FSOpenFileChannel** opens a file specified by *pathPtr* and returns a channel handle that can be used to perform input and output on the file. This API is modeled after the **fopen** procedure of the Unix standard I/O library. The syntax and meaning of all arguments is similar to those given in the Tcl **open** command when opening a file. If an error occurs while opening the channel, **Tcl_FSOpenFileChannel** returns NULL and records a POSIX error code that can be retrieved with **Tcl_GetErrno**. In addition, if *interp* is non-NULL, **Tcl_FSOpenFileChannel** leaves an error message in *interp*'s result after any error.

The newly created channel is not registered in the supplied interpreter; to register it, use **Tcl_RegisterChannel**. If one of the standard channels, **stdin**, **stdout** or **stderr** was previously closed, the act of creating the new channel also assigns it as a replacement for the standard channel.

**Tcl_FSGetCwd** replaces the library version of **getcwd**.

It returns the Tcl library's current working directory. This may be different to the native platform's working directory, which happens when the current working directory is not in the native filesystem.

The result is a pointer to a Tcl_Obj specifying the current directory, or NULL if the current directory could not be determined. If NULL is returned, an error message is left in the *interp*'s result.

The result already has its reference count incremented for the caller. When it is no longer needed, that reference count should be decremented. This is needed for thread-safety purposes, to allow multiple threads to access this and related functions, while ensuring the results are always valid.

**Tcl_FSChdir** replaces the library version of **chdir**. The path is normalized and then passed to the filesystem which claims it. If that filesystem does not implement this function, Tcl will fallback to a combination of **stat** and **access** to check whether the directory exists and has appropriate permissions.

For results, see **chdir** documentation. If successful, we keep a record of the successful path in *cwdPathPtr* for subsequent calls to **Tcl_FSGetCwd**.

**Tcl_FSPathSeparator** returns the separator character to be used for most specific element of the path specified by *pathPtr* (i.e.\ the last part of the path).

The separator is returned as a Tcl_Obj containing a string of length 1. If the path is invalid, NULL is returned.

**Tcl_FSJoinPath** takes the given Tcl_Obj, which must be a valid list (which is allowed to have a reference count of zero), and returns the path value given by considering the first *elements* elements as valid path segments (each path segment may be a complete path, a partial path or just a single possible directory or file name). If any path segment is actually an absolute path, then all prior path segments are discarded. If *elements* is less than 0, we use the entire list.

It is possible that the returned value is actually an element of the given list, so the caller should be careful to increment the reference count of the result before freeing the list.

The returned value, typically with a reference count of zero (but it could be shared under some conditions), contains the joined path. The caller must add a reference count to the value before using it. In particular, the returned value could be an element of the given list, so freeing the list might free the value prematurely if no reference count has been taken. If the number of elements is zero, then the returned value will be an empty-string Tcl_Obj.

**Tcl_FSSplitPath** takes the given Tcl_Obj, which should be a valid path, and returns a Tcl list value containing each segment of that path as an element. It returns a list value with a reference count of zero. If the passed in *lenPtr* is non-NULL, the variable it points to will be updated to contain the number of elements in the returned list.

**Tcl_FSEqualPaths** tests whether the two paths given represent the same filesystem object. It returns 1 if the paths are equal, and 0 if they are different. If either path is NULL, 0 is always returned.

**Tcl_FSGetNormalizedPath** attempts to extract from the given Tcl_Obj a unique normalized path representation, whose string value can be used as a unique identifier for the file.

It returns the normalized path value, owned by Tcl, or NULL if the path was invalid or could otherwise not be successfully converted. Extraction of absolute, normalized paths is very efficient (because the filesystem operates on these representations internally), although the result when the filesystem contains numerous symbolic links may not be the most user-friendly version of a path. The return value is owned by Tcl and has a lifetime equivalent to that of the *pathPtr* passed in (unless that is a relative path, in which case the normalized path value may be freed any time the cwd changes) - the caller can of course increment the reference count if it wishes to maintain a copy for longer.

**Tcl_FSJoinToPath** takes the given value, which should usually be a valid path or NULL, and joins onto it the array of paths segments given.

Returns a value, typically with reference count of zero (but it could be shared under some conditions), containing the joined path. The caller must add a reference count to the value before using it. If any of the values passed into this function (*pathPtr* or *path* elements) have a reference count of zero, they will be freed when this function returns.

**Tcl_FSConvertToPathType** tries to convert the given Tcl_Obj to a valid Tcl path type, taking account of the fact that the cwd may have changed even if this value is already supposedly of the correct type.

If the conversion succeeds (i.e.\ the value is a valid path in one of the current filesystems), then **TCL_OK** is returned. Otherwise **TCL_ERROR** is returned, and an error message may be left in the interpreter.

**Tcl_FSGetInternalRep** extracts the internal representation of a given path value, in the given filesystem. If the path value belongs to a different filesystem, we return NULL. If the internal representation is currently NULL, we attempt to generate it, by calling the filesystem's **Tcl_FSCreateInternalRepProc**.

Returns NULL or a valid internal path representation. This internal representation is cached, so that repeated calls to this function will not require additional conversions.

**Tcl_FSGetTranslatedPath** attempts to extract the translated path from the given Tcl_Obj.

If the translation succeeds (i.e.\ the value is a valid path), then it is returned. Otherwise NULL will be returned, and an error message may be left in the interpreter. The value returned is owned by the caller, which must store it or call **Tcl_DecrRefCount** to ensure memory is freed. This function is of little practical use, and **Tcl_FSGetNormalizedPath** or **Tcl_FSGetNativePath** are usually better functions to use for most purposes.

**Tcl_FSGetTranslatedStringPath** does the same as **Tcl_FSGetTranslatedPath**, but returns a character string or NULL. The string returned is dynamically allocated and owned by the caller, which must store it or call **Tcl_Free** to ensure it is freed. Again, **Tcl_FSGetNormalizedPath** or **Tcl_FSGetNativePath** are usually better functions to use for most purposes.

**Tcl_FSNewNativePath** performs something like the reverse of the usual obj->path->nativerep conversions. If some code retrieves a path in native form (from, e.g.\ **readlink** or a native dialog), and that path is to be used at the Tcl level, then calling this function is an efficient way of creating the appropriate path value type.

The resulting value is a pure "path" value, which will only receive a UTF-8 string representation if that is required by some Tcl code.

**Tcl_FSGetNativePath** is for use by the Win/Unix native filesystems, so that they can easily retrieve the native (char* or TCHAR*) representation of a path. This function is a convenience wrapper around **Tcl_FSGetInternalRep**. It may be desirable in the future to have non-string-based native representations (for example, on macOS, a representation using a fileSpec of FSRef structure would probably be more efficient). On Windows a full Unicode representation would allow for paths of unlimited length. Currently the representation is simply a character string which may contain either the relative path or a complete, absolute normalized path in the native encoding (complex conditions dictate which of these will be provided, so neither can be relied upon, unless the path is known to be absolute). If you need a native path which must be absolute, then you should ask for the native version of a normalized path. If for some reason a non-absolute, non-normalized version of the path is needed, that must be constructed separately (e.g.\ using **Tcl_FSGetTranslatedPath**).

The native representation is cached so that repeated calls to this function will not require additional conversions. The return value is owned by Tcl and has a lifetime equivalent to that of the *pathPtr* passed in (unless that is a relative path, in which case the native representation may be freed any time the cwd changes).

**Tcl_FSFileSystemInfo** returns a list of two elements. The first element is the name of the filesystem (e.g. "native", "vfs", "zip", or "prowrap", perhaps), and the second is the particular type of the given path within that filesystem (which is filesystem dependent). The second element may be empty if the filesystem does not provide a further categorization of files.

A valid list value is returned, unless the path value is not recognized, when NULL will be returned.

**Tcl_FSGetFileSystemForPath** returns a pointer to the **Tcl_Filesystem** which accepts this path as valid.

If no filesystem will accept the path, NULL is returned.

**Tcl_FSGetPathType** determines whether the given path is relative to the current directory, relative to the current volume, or absolute.

It returns one of **TCL_PATH_ABSOLUTE**, **TCL_PATH_RELATIVE**, or **TCL_PATH_VOLUME_RELATIVE**

**Tcl_FSTildeExpand** performs tilde substitution on the input path passed via **pathStr** as described in the documentation for the **file tildeexpand** Tcl command. On success, the function returns **TCL_OK** with the result of the substitution in **dsPtr** which must be subsequently freed by the caller. The **dsPtr** structure is initialized by the function. No guarantees are made about the form of the returned path such as the path separators used. The returned result should be passed to other Tcl C API functions such as **Tcl_FSGetNormalizedPath** or **Tcl_FSGetNativePath** if necessary. On error, the function returns **TCL_ERROR** with an error message in **interp** which may be passed as NULL if error messages are not of interest.

## Portable stat result api

**Tcl_AllocStatBuf** allocates a *Tcl_StatBuf* on the system heap (which may be deallocated by being passed to **Tcl_Free**). This allows extensions to invoke **Tcl_FSStat** and **Tcl_FSLstat** without being dependent on the size of the buffer. That in turn depends on the flags used to build Tcl.

The portable fields of a *Tcl_StatBuf* may be read using the following functions, each of which returns the value of the corresponding field listed in the table below. Note that on some platforms there may be other fields in the *Tcl_StatBuf* as it is an alias for a suitable system structure, but only the portable ones are made available here. See your system documentation for a full description of these fields. .DS *Access Function*	*Field*  **Tcl_GetFSDeviceFromStat**	 st_dev  **Tcl_GetFSInodeFromStat**	 st_ino  **Tcl_GetModeFromStat**	 st_mode  **Tcl_GetLinkCountFromStat**	 st_nlink  **Tcl_GetUserIdFromStat**	 st_uid  **Tcl_GetGroupIdFromStat**	 st_gid  **Tcl_GetDeviceTypeFromStat**	 st_rdev  **Tcl_GetAccessTimeFromStat**	 st_atime  **Tcl_GetModificationTimeFromStat**	 st_mtime  **Tcl_GetChangeTimeFromStat**	 st_ctime  **Tcl_GetSizeFromStat**	 st_size  **Tcl_GetBlocksFromStat**	 st_blocks  **Tcl_GetBlockSizeFromStat**	 st_blksize .DE

# The virtual filesystem api

A filesystem provides a **Tcl_Filesystem** structure that contains pointers to functions that implement the various operations on a filesystem; these operations are invoked as needed by the generic layer, which generally occurs through the functions listed above.

The **Tcl_Filesystem** structures are manipulated using the following methods.

**Tcl_FSRegister** takes a pointer to a filesystem structure and an optional piece of data to associated with that filesystem. On calling this function, Tcl will attach the filesystem to the list of known filesystems, and it will become fully functional immediately. Tcl does not check if the same filesystem is registered multiple times (and in general that is not a good thing to do). **TCL_OK** will be returned.

**Tcl_FSUnregister** removes the given filesystem structure from the list of known filesystems, if it is known, and returns **TCL_OK**. If the filesystem is not currently registered, **TCL_ERROR** is returned.

**Tcl_FSData** will return the clientData associated with the given filesystem, if that filesystem is registered. Otherwise it will return NULL.

**Tcl_FSMountsChanged** is used to inform the Tcl's core that the set of mount points for the given (already registered) filesystem have changed, and that cached file representations may therefore no longer be correct.

## The tcl_filesystem structure

The **Tcl_Filesystem** structure contains the following fields:

```
typedef struct {
    const char *typeName;
    Tcl_Size structureLength;
    Tcl_FSVersion version;
    Tcl_FSPathInFilesystemProc *pathInFilesystemProc;
    Tcl_FSDupInternalRepProc *dupInternalRepProc;
    Tcl_FSFreeInternalRepProc *freeInternalRepProc;
    Tcl_FSInternalToNormalizedProc *internalToNormalizedProc;
    Tcl_FSCreateInternalRepProc *createInternalRepProc;
    Tcl_FSNormalizePathProc *normalizePathProc;
    Tcl_FSFilesystemPathTypeProc *filesystemPathTypeProc;
    Tcl_FSFilesystemSeparatorProc *filesystemSeparatorProc;
    Tcl_FSStatProc *statProc;
    Tcl_FSAccessProc *accessProc;
    Tcl_FSOpenFileChannelProc *openFileChannelProc;
    Tcl_FSMatchInDirectoryProc *matchInDirectoryProc;
    Tcl_FSUtimeProc *utimeProc;
    Tcl_FSLinkProc *linkProc;
    Tcl_FSListVolumesProc *listVolumesProc;
    Tcl_FSFileAttrStringsProc *fileAttrStringsProc;
    Tcl_FSFileAttrsGetProc *fileAttrsGetProc;
    Tcl_FSFileAttrsSetProc *fileAttrsSetProc;
    Tcl_FSCreateDirectoryProc *createDirectoryProc;
    Tcl_FSRemoveDirectoryProc *removeDirectoryProc;
    Tcl_FSDeleteFileProc *deleteFileProc;
    Tcl_FSCopyFileProc *copyFileProc;
    Tcl_FSRenameFileProc *renameFileProc;
    Tcl_FSCopyDirectoryProc *copyDirectoryProc;
    Tcl_FSLstatProc *lstatProc;
    Tcl_FSLoadFileProc *loadFileProc;
    Tcl_FSGetCwdProc *getCwdProc;
    Tcl_FSChdirProc *chdirProc;
} Tcl_Filesystem;
```

Except for the first three fields in this structure which contain simple data elements, all entries contain addresses of functions called by the generic filesystem layer to perform the complete range of filesystem related actions.

The many functions in this structure are broken down into three categories: infrastructure functions (almost all of which must be implemented), operational functions (which must be implemented if a complete filesystem is provided), and efficiency functions (which need only be implemented if they can be done so efficiently, or if they have side-effects which are required by the filesystem; Tcl has less efficient emulations it can fall back on). It is important to note that, in the current version of Tcl, most of these fallbacks are only used to handle commands initiated in Tcl, not in C. What this means is, that if a **file rename** command is issued in Tcl, and the relevant filesystem(s) do not implement their *Tcl_FSRenameFileProc*, Tcl's core will instead fallback on a combination of other filesystem functions (it will use *Tcl_FSCopyFileProc* followed by *Tcl_FSDeleteFileProc*, and if *Tcl_FSCopyFileProc* is not implemented there is a further fallback). However, if a *Tcl_FSRenameFileProc* command is issued at the C level, no such fallbacks occur. This is true except for the last four entries in the filesystem table (**lstat**, **load**, **getcwd** and **chdir**) for which fallbacks do in fact occur at the C level.

Any functions which take path names in Tcl_Obj form take those names in UTF-8 form. The filesystem infrastructure API is designed to support efficient, cached conversion of these UTF-8 paths to other native representations.

## Example filesystem definition

Here is the filesystem lookup table used by the "vfs" extension which allows filesystem actions to be implemented in Tcl.

```
static Tcl_Filesystem vfsFilesystem = {
    "tclvfs",
    sizeof(Tcl_Filesystem),
    TCL_FILESYSTEM_VERSION_1,
    &VfsPathInFilesystem,
    &VfsDupInternalRep,
    &VfsFreeInternalRep,
    /* No internal to normalized, since we don't create
     * any pure 'internal' Tcl_Obj path representations */
    NULL,
    /* No create native rep function, since we don't use
     * it and don't choose to support uses of
     * Tcl_FSNewNativePath */
    NULL,
    /* Normalize path isn't needed - we assume paths only
     * have one representation */
    NULL,
    &VfsFilesystemPathType,
    &VfsFilesystemSeparator,
    &VfsStat,
    &VfsAccess,
    &VfsOpenFileChannel,
    &VfsMatchInDirectory,
    &VfsUtime,
    /* We choose not to support symbolic links inside our
     * VFS's */
    NULL,
    &VfsListVolumes,
    &VfsFileAttrStrings,
    &VfsFileAttrsGet,
    &VfsFileAttrsSet,
    &VfsCreateDirectory,
    &VfsRemoveDirectory,
    &VfsDeleteFile,
    /* No copy file; use the core fallback mechanism */
    NULL,
    /* No rename file; use the core fallback mechanism */
    NULL,
    /* No copy directory; use the core fallback mechanism */
    NULL,
    /* Core will use stat for lstat */
    NULL,
    /* No load; use the core fallback mechanism */
    NULL,
    /* We don't need a getcwd or chdir; the core's own
     * internal value is suitable */
    NULL,
    NULL
};
```

# Filesystem infrastructure

These fields contain basic information about the filesystem structure and addresses of functions which are used to associate a particular filesystem with a file path, and deal with the internal handling of path representations, for example copying and freeing such representations.

## Typename

The *typeName* field contains a null-terminated string that identifies the type of the filesystem implemented, e.g. "native", "zip" or "vfs".

## Structure length

The *structureLength* field is generally implemented as *sizeof(Tcl_Filesystem)*, and is there to allow easier binary backwards compatibility if the size of the structure changes in a future Tcl release.

## Version

The *version* field should be set to **TCL_FILESYSTEM_VERSION_1**.

## Pathinfilesystemproc

The *pathInFilesystemProc* field contains the address of a function which is called to determine whether a given path value belongs to this filesystem or not. Tcl will only call the rest of the filesystem functions with a path for which this function has returned **TCL_OK**. If the path does not belong, -1 should be returned (the behavior of Tcl for any other return value is not defined). If **TCL_OK** is returned, then the optional *clientDataPtr* output parameter can be used to return an internal (filesystem specific) representation of the path, which will be cached inside the path value, and may be retrieved efficiently by the other filesystem functions. Tcl will simultaneously cache the fact that this path belongs to this filesystem. Such caches are invalidated when filesystem structures are added or removed from Tcl's internal list of known filesystems.

```
typedef int Tcl_FSPathInFilesystemProc(
        Tcl_Obj *pathPtr,
        void **clientDataPtr);
```

## Dupinternalrepproc

This function makes a copy of a path's internal representation, and is called when Tcl needs to duplicate a path value. If NULL, Tcl will simply not copy the internal representation, which may then need to be regenerated later.

```
typedef void *Tcl_FSDupInternalRepProc(
        void *clientData);
```

## Freeinternalrepproc

Free the internal representation. This must be implemented if internal representations need freeing (i.e.\ if some memory is allocated when an internal representation is generated), but may otherwise be NULL.

```
typedef void Tcl_FSFreeInternalRepProc(
        void *clientData);
```

## Internaltonormalizedproc

Function to convert internal representation to a normalized path. Only required if the filesystem creates pure path values with no string/path representation. The return value is a Tcl value whose string representation is the normalized path.

```
typedef Tcl_Obj *Tcl_FSInternalToNormalizedProc(
        void *clientData);
```

## Createinternalrepproc

Function to take a path value, and calculate an internal representation for it, and store that native representation in the value. May be NULL if paths have no internal representation, or if the *Tcl_FSPathInFilesystemProc* for this filesystem always immediately creates an internal representation for paths it accepts.

```
typedef void *Tcl_FSCreateInternalRepProc(
        Tcl_Obj *pathPtr);
```

## Normalizepathproc

Function to normalize a path. Should be implemented for all filesystems which can have multiple string representations for the same path value. In Tcl, every "path" must have a single unique "normalized" string representation. Depending on the filesystem, there may be more than one unnormalized string representation which refers to that path (e.g.\ a relative path, a path with different character case if the filesystem is case insensitive, a path containing symbolic links, etc). If the very last component in the path is a symbolic link, it should not be converted into the value it points to (but its case or other aspects should be made unique). All other path components should be converted from symbolic links. This one exception is required to agree with Tcl's semantics with **file delete**, **file rename**, **file copy** operating on symbolic links. This function may be called with *nextCheckpoint* either at the beginning of the path (i.e.\ zero), at the end of the path, or at any intermediate file separator in the path. It will never point to any other arbitrary position in the path. In the last of the three valid cases, the implementation can assume that the path up to and including the file separator is known and normalized.

```
typedef int Tcl_FSNormalizePathProc(
        Tcl_Interp *interp,
        Tcl_Obj *pathPtr,
        int nextCheckpoint);
```

# Filesystem operations

The fields in this section of the structure contain addresses of functions which are called to carry out the basic filesystem operations. A filesystem which expects to be used with the complete standard Tcl command set must implement all of these. If some of them are not implemented, then certain Tcl commands may fail when operating on paths within that filesystem. However, in some instances this may be desirable (for example, a read-only filesystem should not implement the last four functions, and a filesystem which does not support symbolic links need not implement the **readlink** function, etc. The Tcl core expects filesystems to behave in this way).

## Filesystempathtypeproc

Function to determine the type of a path in this filesystem. May be NULL, in which case no type information will be available to users of the filesystem. The "type" is used only for informational purposes, and should be returned as the string representation of the Tcl_Obj which is returned. A typical return value might be "networked", "zip" or "ftp". The Tcl_Obj result is owned by the filesystem and so Tcl will increment the reference count of that value if it wishes to retain a reference to it.

```
typedef Tcl_Obj *Tcl_FSFilesystemPathTypeProc(
        Tcl_Obj *pathPtr);
```

## Filesystemseparatorproc

Function to return the separator character(s) for this filesystem. This need only be implemented if the filesystem wishes to use a different separator than the standard string "/". Amongst other uses, it is returned by the **file separator** command. The return value should be a value with reference count of zero.

```
typedef Tcl_Obj *Tcl_FSFilesystemSeparatorProc(
        Tcl_Obj *pathPtr);
```

## Statproc

Function to process a **Tcl_FSStat** call. Must be implemented for any reasonable filesystem, since many Tcl level commands depend crucially upon it (e.g.\ **file atime**, **file isdirectory**, **file size**, **glob**).

```
typedef int Tcl_FSStatProc(
        Tcl_Obj *pathPtr,
        Tcl_StatBuf *statPtr);
```

The **Tcl_FSStatProc** fills the stat structure *statPtr* with information about the specified file. You do not need any access rights to the file to get this information but you need search rights to all directories named in the path leading to the file. The stat structure includes info regarding device, inode (always 0 on Windows), privilege mode, nlink (always 1 on Windows), user id (always 0 on Windows), group id (always 0 on Windows), rdev (same as device on Windows), size, last access time, last modification time, and last metadata change time.

If the file represented by *pathPtr* exists, the **Tcl_FSStatProc** returns 0 and the stat structure is filled with data. Otherwise, -1 is returned, and no stat info is given.

## Accessproc

Function to process a **Tcl_FSAccess** call. Must be implemented for any reasonable filesystem, since many Tcl level commands depend crucially upon it (e.g.\ **file exists**, **file readable**).

```
typedef int Tcl_FSAccessProc(
        Tcl_Obj *pathPtr,
        int mode);
```

The **Tcl_FSAccessProc** checks whether the process would be allowed to read, write or test for existence of the file (or other filesystem object) whose name is in *pathPtr*. If the pathname refers to a symbolic link, then the permissions of the file referred by this symbolic link should be tested.

On success (all requested permissions granted), zero is returned. On error (at least one bit in mode asked for a permission that is denied, or some other  error occurred), -1 is returned.

## Openfilechannelproc

Function to process a **Tcl_FSOpenFileChannel** call. Must be implemented for any reasonable filesystem, since any operations which require open or accessing a file's contents will use it (e.g.\ **open**, **encoding**, and many Tk commands).

```
typedef Tcl_Channel Tcl_FSOpenFileChannelProc(
        Tcl_Interp *interp,
        Tcl_Obj *pathPtr,
        int mode,
        int permissions);
```

The **Tcl_FSOpenFileChannelProc** opens a file specified by *pathPtr* and returns a channel handle that can be used to perform input and output on the file. This API is modeled after the **fopen** procedure of the Unix standard I/O library. The syntax and meaning of all arguments is similar to those given in the Tcl **open** command when opening a file, where the *mode* argument is a combination of the POSIX flags O_RDONLY, O_WRONLY, etc. If an error occurs while opening the channel, the **Tcl_FSOpenFileChannelProc** returns NULL and records a POSIX error code that can be retrieved with **Tcl_GetErrno**. In addition, if *interp* is non-NULL, the **Tcl_FSOpenFileChannelProc** leaves an error message in *interp*'s result after any error.

The newly created channel must not be registered in the supplied interpreter by a **Tcl_FSOpenFileChannelProc**; that task is up to the caller of **Tcl_FSOpenFileChannel** (if necessary). If one of the standard channels, **stdin**, **stdout** or **stderr** was previously closed, the act of creating the new channel also assigns it as a replacement for the standard channel.

## Matchindirectoryproc

Function to process a **Tcl_FSMatchInDirectory** call. If not implemented, then glob and recursive copy functionality will be lacking in the filesystem (and this may impact commands like **encoding names** which use glob functionality internally).

```
typedef int Tcl_FSMatchInDirectoryProc(
        Tcl_Interp *interp,
        Tcl_Obj *resultPtr,
        Tcl_Obj *pathPtr,
        const char *pattern,
        Tcl_GlobTypeData *types);
```

The function should return all files or directories (or other filesystem objects) which match the given pattern and accord with the *types* specification given. There are two ways in which this function may be called. If *pattern* is NULL, then *pathPtr* is a full path specification of a single file or directory which should be checked for existence and correct type. Otherwise, *pathPtr* is a directory, the contents of which the function should search for files or directories which have the correct type. In either case, *pathPtr* can be assumed to be both non-NULL and non-empty. It is not currently documented whether *pathPtr* will have a file separator at its end of not, so code should be flexible to both possibilities.

The return value is a standard Tcl result indicating whether an error occurred in the matching process. Error messages are placed in *interp*, unless *interp* in NULL in which case no error message need be generated; on a **TCL_OK** result, results should be added to the *resultPtr* value given (which can be assumed to be a valid unshared Tcl list). The matches added to *resultPtr* should include any path prefix given in *pathPtr* (this usually means they will be absolute path specifications). Note that if no matches are found, that simply leads to an empty result; errors are only signaled for actual file or filesystem problems which may occur during the matching process.

The **Tcl_GlobTypeData** structure passed in the *types* parameter contains the following fields:

```
typedef struct {
    /* Corresponds to bcdpfls as in 'find -t' */
    int type;
    /* Corresponds to file permissions */
    int perm;
    /* Acceptable mac type */
    Tcl_Obj *macType;
    /* Acceptable mac creator */
    Tcl_Obj *macCreator;
} Tcl_GlobTypeData;
```

There are two specific cases which it is important to handle correctly, both when *types* is non-NULL. The two cases are when *types->types & TCL_GLOB_TYPE_DIR* or *types->types & TCL_GLOB_TYPE_MOUNT* are true (and in particular when the other flags are false). In the first of these cases, the function must list the contained directories. Tcl uses this to implement recursive globbing, so it is critical that filesystems implement directory matching correctly. In the second of these cases, with **TCL_GLOB_TYPE_MOUNT**, the filesystem must list the mount points which lie within the given *pathPtr* (and in this case, *pathPtr* need not lie within the same filesystem - different to all other cases in which this function is called). Support for this is critical if Tcl is to have seamless transitions between from one filesystem to another.

## Utimeproc

Function to process a **Tcl_FSUtime** call. Required to allow setting (not reading) of times with **file mtime**, **file atime** and the open-r/open-w/fcopy implementation of **file copy**.

```
typedef int Tcl_FSUtimeProc(
        Tcl_Obj *pathPtr,
        struct utimbuf *tval);
```

The access and modification times of the file specified by *pathPtr* should be changed to the values given in the *tval* structure.

The return value should be 0 on success and -1 on an error, as with the system **utime**.

## Linkproc

Function to process a **Tcl_FSLink** call. Should be implemented only if the filesystem supports links, and may otherwise be NULL.

```
typedef Tcl_Obj *Tcl_FSLinkProc(
        Tcl_Obj *linkNamePtr,
        Tcl_Obj *toPtr,
        int linkAction);
```

If *toPtr* is NULL, the function is being asked to read the contents of a link. The result is a Tcl_Obj specifying the contents of the link given by *linkNamePtr*, or NULL if the link could not be read. The result is owned by the caller (and should therefore have its ref count incremented before being returned). Any callers should call **Tcl_DecrRefCount** on this result when it is no longer needed. If *toPtr* is not NULL, the function should attempt to create a link. The result in this case should be *toPtr* if the link was successful and NULL otherwise. In this case the result is not owned by the caller (i.e.\ no reference count manipulations on either end are needed). See the documentation for **Tcl_FSLink** for the correct interpretation of the *linkAction* flags.

## Listvolumesproc

Function to list any filesystem volumes added by this filesystem. Should be implemented only if the filesystem adds volumes at the head of the filesystem, so that they can be returned by **file volumes**.

```
typedef Tcl_Obj *Tcl_FSListVolumesProc(void);
```

The result should be a list of volumes added by this filesystem, or NULL (or an empty list) if no volumes are provided. The result value is considered to be owned by the filesystem (not by Tcl's core), but should be given a reference count for Tcl. Tcl will use the contents of the list and then decrement that reference count. This allows filesystems to choose whether they actually want to retain a "global list" of volumes or not (if not, they generate the list on the fly and pass it to Tcl with a reference count of 1 and then forget about the list, if yes, then they simply increment the reference count of their global list and pass it to Tcl which will copy the contents and then decrement the count back to where it was).

Therefore, Tcl considers return values from this proc to be read-only.

## Fileattrstringsproc

Function to list all attribute strings which are valid for this filesystem. If not implemented the filesystem will not support the **file attributes** command. This allows arbitrary additional information to be attached to files in the filesystem. If it is not implemented, there is no need to implement the **get** and **set** methods.

```
typedef const char *const *Tcl_FSFileAttrStringsProc(
        Tcl_Obj *pathPtr,
        Tcl_Obj **objPtrRef);
```

The called function may either return an array of strings, or may instead return NULL and place a Tcl list into the given *objPtrRef*. Tcl will take that list and first increment its reference count before using it. On completion of that use, Tcl will decrement its reference count. Hence if the list should be disposed of by Tcl when done, it should have a reference count of zero, and if the list should not be disposed of, the filesystem should ensure it returns a value with a reference count of at least one.

## Fileattrsgetproc

Function to process a **Tcl_FSFileAttrsGet** call, used by **file attributes**.

```
typedef int Tcl_FSFileAttrsGetProc(
        Tcl_Interp *interp,
        int index,
        Tcl_Obj *pathPtr,
        Tcl_Obj **objPtrRef);
```

Returns a standard Tcl return code. The attribute value retrieved, which corresponds to the *index*'th element in the list returned by the **Tcl_FSFileAttrStringsProc**, is a Tcl_Obj placed in *objPtrRef* (if **TCL_OK** was returned) and is likely to have a reference count of zero. Either way we must either store it somewhere (e.g.\ the Tcl result), or Incr/Decr its reference count to ensure it is properly freed.

## Fileattrssetproc

Function to process a **Tcl_FSFileAttrsSet** call, used by **file attributes**. If the filesystem is read-only, there is no need to implement this.

```
typedef int Tcl_FSFileAttrsSetProc(
        Tcl_Interp *interp,
        int index,
        Tcl_Obj *pathPtr,
        Tcl_Obj *objPtr);
```

The attribute value of the *index*'th element in the list returned by the Tcl_FSFileAttrStringsProc should be set to the *objPtr* given.

## Createdirectoryproc

Function to process a **Tcl_FSCreateDirectory** call. Should be implemented unless the FS is read-only.

```
typedef int Tcl_FSCreateDirectoryProc(
        Tcl_Obj *pathPtr);
```

The return value is a standard Tcl result indicating whether an error occurred in the process. If successful, a new directory should have been added to the filesystem in the location specified by *pathPtr*.

## Removedirectoryproc

Function to process a **Tcl_FSRemoveDirectory** call. Should be implemented unless the FS is read-only.

```
typedef int Tcl_FSRemoveDirectoryProc(
        Tcl_Obj *pathPtr,
        int recursive,
        Tcl_Obj **errorPtr);
```

The return value is a standard Tcl result indicating whether an error occurred in the process. If successful, the directory specified by *pathPtr* should have been removed from the filesystem. If the *recursive* flag is given, then a non-empty directory should be deleted without error. If this flag is not given, then and the directory is non-empty a POSIX "EEXIST" error should be signaled. If an error does occur, the name of the file or directory which caused the error should be placed in *errorPtr*.

## Deletefileproc

Function to process a **Tcl_FSDeleteFile** call. Should be implemented unless the FS is read-only.

```
typedef int Tcl_FSDeleteFileProc(
        Tcl_Obj *pathPtr);
```

The return value is a standard Tcl result indicating whether an error occurred in the process. If successful, the file specified by *pathPtr* should have been removed from the filesystem. Note that, if the filesystem supports symbolic links, Tcl will always call this function and not Tcl_FSRemoveDirectoryProc when needed to delete them (even if they are symbolic links to directories).

# Filesystem efficiency

These functions need not be implemented for a particular filesystem because the core has a fallback implementation available. See each individual description for the consequences of leaving the field NULL.

## Lstatproc

Function to process a **Tcl_FSLstat** call. If not implemented, Tcl will attempt to use the *statProc* defined above instead. Therefore it need only be implemented if a filesystem can differentiate between **stat** and **lstat** calls.

```
typedef int Tcl_FSLstatProc(
        Tcl_Obj *pathPtr,
        Tcl_StatBuf *statPtr);
```

The behavior of this function is very similar to that of the **Tcl_FSStatProc** defined above, except that if it is applied to a symbolic link, it returns information about the link, not about the target file.

## Copyfileproc

Function to process a **Tcl_FSCopyFile** call. If not implemented Tcl will fall back on **open**-r, **open**-w and **fcopy** as a copying mechanism. Therefore it need only be implemented if the filesystem can perform that action more efficiently.

```
typedef int Tcl_FSCopyFileProc(
        Tcl_Obj *srcPathPtr,
        Tcl_Obj *destPathPtr);
```

The return value is a standard Tcl result indicating whether an error occurred in the copying process. Note that, *destPathPtr* is the name of the file which should become the copy of *srcPathPtr*. It is never the name of a directory into which *srcPathPtr* could be copied (i.e.\ the function is much simpler than the Tcl level **file copy** subcommand). Note that, if the filesystem supports symbolic links, Tcl will always call this function and not *copyDirectoryProc* when needed to copy them (even if they are symbolic links to directories). Finally, if the filesystem determines it cannot support the **file copy** action, calling **Tcl_SetErrno(EXDEV)** and returning a non-**TCL_OK** result will tell Tcl to use its standard fallback mechanisms.

## Renamefileproc

Function to process a **Tcl_FSRenameFile** call. If not implemented, Tcl will fall back on a copy and delete mechanism. Therefore it need only be implemented if the filesystem can perform that action more efficiently.

```
typedef int Tcl_FSRenameFileProc(
        Tcl_Obj *srcPathPtr,
        Tcl_Obj *destPathPtr);
```

The return value is a standard Tcl result indicating whether an error occurred in the renaming process. If the filesystem determines it cannot support the **file rename** action, calling **Tcl_SetErrno(EXDEV)** and returning a non-**TCL_OK** result will tell Tcl to use its standard fallback mechanisms.

## Copydirectoryproc

Function to process a **Tcl_FSCopyDirectory** call. If not implemented, Tcl will fall back on a recursive **file mkdir**, **file copy** mechanism. Therefore it need only be implemented if the filesystem can perform that action more efficiently.

```
typedef int Tcl_FSCopyDirectoryProc(
        Tcl_Obj *srcPathPtr,
        Tcl_Obj *destPathPtr,
        Tcl_Obj **errorPtr);
```

The return value is a standard Tcl result indicating whether an error occurred in the copying process. If an error does occur, the name of the file or directory which caused the error should be placed in *errorPtr*. Note that, *destPathPtr* is the name of the directory-name which should become the mirror-image of *srcPathPtr*. It is not the name of a directory into which *srcPathPtr* should be copied (i.e.\ the function is much simpler than the Tcl level **file copy** subcommand). Finally, if the filesystem determines it cannot support the directory copy action, calling **Tcl_SetErrno(EXDEV)** and returning a non-**TCL_OK** result will tell Tcl to use its standard fallback mechanisms.

## Loadfileproc

Function to process a **Tcl_FSLoadFile** call. If not implemented, Tcl will fall back on a copy to native-temp followed by a **Tcl_FSLoadFile** on that temporary copy. Therefore it need only be implemented if the filesystem can load code directly, or it can be implemented simply to return **TCL_ERROR** to disable load functionality in this filesystem entirely.

```
typedef int Tcl_FSLoadFileProc(
        Tcl_Interp *interp,
        Tcl_Obj *pathPtr,
        Tcl_LoadHandle *handlePtr,
        Tcl_FSUnloadFileProc *unloadProcPtr);
```

Returns a standard Tcl completion code. If an error occurs, an error message is left in the *interp*'s result. The function dynamically loads a binary code file into memory. On a successful load, the *handlePtr* should be filled with a token for the dynamically loaded file, and the *unloadProcPtr* should be filled in with the address of a procedure. The unload procedure will be called with the given **Tcl_LoadHandle** as its only parameter when Tcl needs to unload the file. For example, for the native filesystem, the **Tcl_LoadHandle** returned is currently a token which can be used in the private **TclpFindSymbol** to access functions in the new code. Each filesystem is free to define the **Tcl_LoadHandle** as it requires. Finally, if the filesystem determines it cannot support the file load action, calling **Tcl_SetErrno(EXDEV)** and returning a non-**TCL_OK** result will tell Tcl to use its standard fallback mechanisms.

## Unloadfileproc

Function to unload a previously successfully loaded file. If load was implemented, then this should also be implemented, if there is any cleanup action required.

```
typedef void Tcl_FSUnloadFileProc(
        Tcl_LoadHandle loadHandle);
```

## Getcwdproc

Function to process a **Tcl_FSGetCwd** call. Most filesystems need not implement this. It will usually only be called once, if **getcwd** is called before **chdir**. May be NULL.

```
typedef Tcl_Obj *Tcl_FSGetCwdProc(
        Tcl_Interp *interp);
```

If the filesystem supports a native notion of a current working directory (which might perhaps change independent of Tcl), this function should return that cwd as the result, or NULL if the current directory could not be determined (e.g.\ the user does not have appropriate permissions on the cwd directory). If NULL is returned, an error message is left in the *interp*'s result.

## Chdirproc

Function to process a **Tcl_FSChdir** call. If filesystems do not implement this, it will be emulated by a series of directory access checks. Otherwise, virtual filesystems which do implement it need only respond with a positive return result if the *pathPtr* is a valid, accessible directory in their filesystem. They need not remember the result, since that will be automatically remembered for use by **Tcl_FSGetCwd**. Real filesystems should carry out the correct action (i.e.\ call the correct system **chdir** API).

```
typedef int Tcl_FSChdirProc(
        Tcl_Obj *pathPtr);
```

The **Tcl_FSChdirProc** changes the applications current working directory to the value specified in *pathPtr*. The function returns -1 on error or 0 on success.

# Reference count management

## Public api calls

For all of these functions, *pathPtr* (including the *srcPathPtr* and *destPathPtr* arguments to **Tcl_FSCopyFile**, **Tcl_FSCopyDirectory**, and **Tcl_FSRenameFile**, the *firstPtr* and *secondPtr* arguments to **Tcl_FSEqualPaths**, and the *linkNamePtr* and *toPtr* arguments to **Tcl_FSLink**) must not be a zero reference count value; references may be retained in internal caches even for theoretically read-only operations.  These functions may also manipulate the interpreter result (if they take and are given a non-NULL *interp* argument); you must not count on the interpreter result to hold the reference count of any argument value over these calls and should manage your own references there. However, references held by the arguments to a Tcl command *are* suitable for reference count management purposes for the duration of the implementation of that command.

The *errorPtr* argument to **Tcl_FSCopyDirectory** and **Tcl_FSRemoveDirectory** is, when an object is set into it at all, set to an object with a non-zero reference count that should be passed to **Tcl_DecrRefCount** when no longer needed.

**Tcl_FSListVolumes** always returns a zero-reference object, much like **Tcl_NewObj**.

**Tcl_FSLink** always returns a non-zero-reference object when it is asked to read; you must call **Tcl_DecrRefCount** on the object once you no longer need it.

**Tcl_FSGetCwd** always returns a non-zero-reference object; you must call **Tcl_DecrRefCount** on the object once you no longer need it.

**Tcl_FSPathSeparator** always returns a zero-reference object, much like **Tcl_NewObj**.

**Tcl_FSJoinPath** always returns a zero-reference object, much like **Tcl_NewObj**. Its *listObj* argument can have any reference count; it is only read by this function.

**Tcl_FSSplitPath** always returns a zero-reference object, much like **Tcl_NewObj**.

**Tcl_FSGetNormalizedPath** returns an object with a non-zero reference count where Tcl is the owner. You should increment its reference count if you want to retain it, but do not need to if you are just using the value immediately.

**Tcl_FSJoinToPath** always returns a zero-reference object, much like **Tcl_NewObj**. Its *basePtr* argument follows the rules above for *pathPtr*, as do the values in the *objv* argument.

**Tcl_FSGetTranslatedPath** returns a non-zero-reference object (or NULL in the error case); you must call **Tcl_DecrRefCount** on the object once you no longer need it.

**Tcl_FSNewNativePath** always returns a zero-reference object (or NULL), much like **Tcl_NewObj**.

**Tcl_FSFileSystemInfo** always returns a zero-reference object (or NULL), much like **Tcl_NewObj**.

The *objPtr* and *objPtrRef* arguments to **Tcl_FSFileAttrsGet**, **Tcl_FSFileAttrsSet** and **Tcl_FSFileAttrStrings** are conventional Tcl values; the *objPtr* argument will be read but not retained, and the *objPtrRef* argument will have (on success) a zero-reference value written into it (as with **Tcl_NewObj**). **Tcl_FSFileAttrsGet** and **Tcl_FSFileAttrsSet** may also manipulate the interpreter result.

The *resultPtr* argument to **Tcl_FSMatchInDirectory** will not have its reference count manipulated, but it should have a reference count of no more than 1, and should not be the current interpreter result (as the function may overwrite that on error).

## Virtual filesystem interface

For all virtual filesystem implementation functions, any *pathPtr* arguments should not have their reference counts manipulated. If they take an *interp* argument, they may set an error message in that, but must not manipulate the *pathPtr* afterwards. Aside from that:

*internalToNormalizedProc*
: This should return a zero-reference count value, as if allocated with **Tcl_NewObj**.

*normalizePathProc*
: Unlike with other API implementation functions, the *pathPtr* argument here is guaranteed to be an unshared object that should be updated. Its reference count should not be modified.

*filesystemPathTypeProc*
: The return value (if non-NULL) either has a reference count of zero or needs to be maintained (on a per-thread basis) by the filesystem. Tcl will increment the reference count of the value if it wishes to retain it.

*filesystemSeparatorProc*
: The return value should be a value with reference count of zero.

*matchInDirectoryProc*
: The *resultPtr* argument should be assumed to hold a list that can be appended to (i.e., that has a reference count no greater than 1). No reference to it should be retained.

*linkProc*
: If *toPtr* is NULL, this should return a value with reference count 1 that has just been allocated and passed to **Tcl_IncrRefCount**. If *toPtr* is not NULL, it should be returned on success.

*listVolumesProc*
: The result value should be a list (if non-NULL); it will have its reference count decremented once (with **Tcl_DecrRefCount**) by Tcl once done.

*fileAttrStringsProc*
: If the result is NULL, the *objPtrRef* should have a list value written to it; that list will have its reference count both incremented (with **Tcl_IncrRefCount**) and decremented (with **Tcl_DecrRefCount**).

*fileAttrsGetProc*
: The *objPtrRef* argument should have (on non-error return) a zero reference count value written to it (allocated as if with **Tcl_NewObj**).

*fileAttrsSetProc*
: The *objPtr* argument should either just be read or its reference count incremented to retain it.

*removeDirectoryProc*
: If an error is being reported, the problem filename reported via *errorPtr* should be newly allocated (as if with **Tcl_NewObj**) and have a reference count of 1 (i.e., have been passed to **Tcl_IncrRefCount**).

*copyDirectoryProc*
: If an error is being reported, the problem filename reported via *errorPtr* should be newly allocated (as if with **Tcl_NewObj**) and have a reference count of 1 (i.e., have been passed to **Tcl_IncrRefCount**).

*getCwdProc*
: The result will be passed to **Tcl_DecrRefCount** by the implementation of **Tcl_FSGetCwd** after it has been normalized.


