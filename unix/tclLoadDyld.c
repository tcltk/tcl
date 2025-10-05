/*
 * tclLoadDyld.c --
 *
 *	This procedure provides a version of the TclLoadFile that works with
 *	Apple's dyld dynamic loading.
 *	Original version of his file (superseded long ago) provided by
 *	Wilfredo Sanchez (wsanchez@apple.com).
 *
 * Copyright © 1995 Apple Computer, Inc.
 * Copyright © 2001-2007 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

#ifndef MODULE_SCOPE
#   define MODULE_SCOPE extern
#endif

/*
 * Use includes for the API we're using.
 */

#include <dlfcn.h>

#if defined(TCL_LOAD_FROM_MEMORY)
#if defined (__clang__) || ((__GNUC__)  && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))))
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <mach-o/dyld.h>
#include <mach-o/fat.h>
#include <mach-o/swap.h>
#include <mach-o/arch.h>
#include <libkern/OSByteOrder.h>
#include <mach/mach.h>

typedef struct Tcl_DyldModuleHandle {
    struct Tcl_DyldModuleHandle *nextPtr;
    NSModule module;
} Tcl_DyldModuleHandle;
#endif /* TCL_LOAD_FROM_MEMORY */

typedef struct {
    void *dlHandle;
#if defined(TCL_LOAD_FROM_MEMORY)
    const struct mach_header *dyldLibHeader;
    Tcl_DyldModuleHandle *modulePtr;
#endif
} Tcl_DyldLoadHandle;

/*
 * Static functions defined in this file.
 */

static void *		FindSymbol(Tcl_Interp *interp,
			    Tcl_LoadHandle loadHandle, const char *symbol);
static void		UnloadFile(Tcl_LoadHandle handle);

/*
 *----------------------------------------------------------------------
 *
 * TclpDlopen --
 *
 *	Dynamically loads a binary code file into memory and returns a handle
 *	to the new code.
 *
 * Results:
 *	A standard Tcl completion code. If an error occurs, an error message
 *	is left in the interpreter's result.
 *
 * Side effects:
 *	New code suddenly appears in memory.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE int
TclpDlopen(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Obj *pathPtr,		/* Name of the file containing the desired
				 * code (UTF-8). */
    Tcl_LoadHandle *loadHandle,	/* Filled with token for dynamically loaded
				 * file which will be passed back to
				 * (*unloadProcPtr)() to unload the file. */
    Tcl_FSUnloadFileProc **unloadProcPtr,
				/* Filled with address of Tcl_FSUnloadFileProc
				 * function which should be used for this
				 * file. */
    int flags)
{
    Tcl_DyldLoadHandle *dyldLoadHandle;
    Tcl_LoadHandle newHandle;
    void *dlHandle = NULL;
#if defined(TCL_LOAD_FROM_MEMORY)
    const struct mach_header *dyldLibHeader = NULL;
    Tcl_DyldModuleHandle *modulePtr = NULL;
#endif
    const char *errMsg = NULL;
    int result;
    Tcl_DString ds;
    const char *nativePath, *nativeFileName = NULL;
    int dlopenflags = 0;

    /*
     * First try the full path the user gave us. This is particularly
     * important if the cwd is inside a vfs, and we are trying to load using a
     * relative path.
     */

    nativePath = (const char *)Tcl_FSGetNativePath(pathPtr);
    if (Tcl_UtfToExternalDStringEx(interp, NULL, TclGetString(pathPtr),
	    TCL_INDEX_NONE, 0, &ds, NULL) != TCL_OK) {
	Tcl_DStringFree(&ds);
	return TCL_ERROR;
    }
    nativeFileName = Tcl_DStringValue(&ds);

    /*
     * Use (RTLD_NOW|RTLD_LOCAL) as default, see [Bug #3216070]
     */

    if (flags & TCL_LOAD_GLOBAL) {
	dlopenflags |= RTLD_GLOBAL;
    } else {
	dlopenflags |= RTLD_LOCAL;
    }
    if (flags & TCL_LOAD_LAZY) {
	dlopenflags |= RTLD_LAZY;
    } else {
	dlopenflags |= RTLD_NOW;
    }
    dlHandle = dlopen(nativePath, dlopenflags);
    if (!dlHandle) {
	/*
	 * Let the OS loader examine the binary search path for whatever string
	 * the user gave us which hopefully refers to a file on the binary
	 * path.
	 */

	dlHandle = dlopen(nativeFileName, dlopenflags);
	if (!dlHandle) {
	    errMsg = dlerror();
	}
    }

    if (dlHandle) {
	dyldLoadHandle = (Tcl_DyldLoadHandle *)Tcl_Alloc(sizeof(Tcl_DyldLoadHandle));
	dyldLoadHandle->dlHandle = dlHandle;
#if defined(TCL_LOAD_FROM_MEMORY)
	dyldLoadHandle->dyldLibHeader = dyldLibHeader;
	dyldLoadHandle->modulePtr = modulePtr;
#endif /* TCL_LOAD_FROM_MEMORY */
	newHandle = (Tcl_LoadHandle)Tcl_Alloc(sizeof(*newHandle));
	newHandle->clientData = dyldLoadHandle;
	newHandle->findSymbolProcPtr = &FindSymbol;
	newHandle->unloadFileProcPtr = &UnloadFile;
	*unloadProcPtr = &UnloadFile;
	*loadHandle = newHandle;
	result = TCL_OK;
    } else {
	Tcl_Obj *errObj;

	TclNewObj(errObj);
	if (errMsg != NULL) {
	    Tcl_AppendToObj(errObj, errMsg, TCL_INDEX_NONE);
	}
	Tcl_SetObjResult(interp, errObj);
	result = TCL_ERROR;
    }

    Tcl_DStringFree(&ds);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * FindSymbol --
 *
 *	Looks up a symbol, by name, through a handle associated with a
 *	previously loaded piece of code (shared library).
 *
 * Results:
 *	Returns a pointer to the function associated with 'symbol' if it is
 *	found. Otherwise returns NULL and may leave an error message in the
 *	interp's result.
 *
 *----------------------------------------------------------------------
 */

static void *
FindSymbol(
    Tcl_Interp *interp,		/* For error reporting. */
    Tcl_LoadHandle loadHandle,	/* Handle from TclpDlopen. */
    const char *symbol)		/* Symbol name to look up. */
{
    Tcl_DyldLoadHandle *dyldLoadHandle = (Tcl_DyldLoadHandle *)loadHandle->clientData;
    Tcl_LibraryInitProc *proc = NULL;
    const char *errMsg = NULL;
    Tcl_DString ds;
    const char *native;

    if (Tcl_UtfToExternalDStringEx(interp, NULL, symbol, TCL_INDEX_NONE, 0, &ds, NULL) != TCL_OK) {
	Tcl_DStringFree(&ds);
	return NULL;
    }
    native = Tcl_DStringValue(&ds);
    if (dyldLoadHandle->dlHandle) {
	proc = (Tcl_LibraryInitProc *)dlsym(dyldLoadHandle->dlHandle, native);
	if (!proc) {
	    errMsg = dlerror();
	}
    } else {
#if defined(TCL_LOAD_FROM_MEMORY)
	NSSymbol nsSymbol = NULL;
	Tcl_DString newName;

	/*
	 * dyld adds an underscore to the beginning of symbol names.
	 */

	Tcl_DStringInit(&newName);
	TclDStringAppendLiteral(&newName, "_");
	native = Tcl_DStringAppend(&newName, native, TCL_INDEX_NONE);
	if (dyldLoadHandle->dyldLibHeader) {
	    nsSymbol = NSLookupSymbolInImage(dyldLoadHandle->dyldLibHeader,
		    native, NSLOOKUPSYMBOLINIMAGE_OPTION_BIND_NOW |
		    NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
	    if (nsSymbol) {
		/*
		 * Until dyld supports unloading of MY_DYLIB binaries, the
		 * following is not needed.
		 */

#ifdef DYLD_SUPPORTS_DYLIB_UNLOADING
		NSModule module = NSModuleForSymbol(nsSymbol);
		Tcl_DyldModuleHandle *modulePtr = dyldLoadHandle->modulePtr;

		while (modulePtr != NULL) {
		    if (module == modulePtr->module) {
			break;
		    }
		    modulePtr = modulePtr->nextPtr;
		}
		if (modulePtr == NULL) {
		    modulePtr = (Tcl_DyldModuleHandle *)Tcl_Alloc(sizeof(Tcl_DyldModuleHandle));
		    modulePtr->module = module;
		    modulePtr->nextPtr = dyldLoadHandle->modulePtr;
		    dyldLoadHandle->modulePtr = modulePtr;
		}
#endif /* DYLD_SUPPORTS_DYLIB_UNLOADING */
	    } else {
		NSLinkEditErrors editError;
		int errorNumber;
		const char *errorName;

		NSLinkEditError(&editError, &errorNumber, &errorName, &errMsg);
	    }
	} else if (dyldLoadHandle->modulePtr) {
	    nsSymbol = NSLookupSymbolInModule(
		    dyldLoadHandle->modulePtr->module, native);
	}
	if (nsSymbol) {
	    proc = (Tcl_LibraryInitProc *)NSAddressOfSymbol(nsSymbol);
	}
	Tcl_DStringFree(&newName);
#endif /* TCL_LOAD_FROM_MEMORY */
    }
    Tcl_DStringFree(&ds);
    if (errMsg && (interp != NULL)) {
	TclPrintfResult(interp, "cannot find symbol \"%s\": %s",
		symbol, errMsg);
	Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "LOAD_SYMBOL", symbol,
		(char *)NULL);
    }
    return (void *)proc;
}

/*
 *----------------------------------------------------------------------
 *
 * UnloadFile --
 *
 *	Unloads a dynamically loaded binary code file from memory. Code
 *	pointers in the formerly loaded file are no longer valid after calling
 *	this function.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Code dissapears from memory. Note that dyld currently only supports
 *	unloading of binaries of type MH_BUNDLE loaded with NSLinkModule() in
 *	TclpDlopen() above.
 *
 *----------------------------------------------------------------------
 */

static void
UnloadFile(
    Tcl_LoadHandle loadHandle)	/* loadHandle returned by a previous call to
				 * TclpDlopen(). The loadHandle is a token
				 * that represents the loaded file. */
{
    Tcl_DyldLoadHandle *dyldLoadHandle = (Tcl_DyldLoadHandle *)loadHandle->clientData;

    if (dyldLoadHandle->dlHandle) {
	(void) dlclose(dyldLoadHandle->dlHandle);
    } else {
#if defined(TCL_LOAD_FROM_MEMORY)
	Tcl_DyldModuleHandle *modulePtr = dyldLoadHandle->modulePtr;

	while (modulePtr != NULL) {
	    void *ptr = modulePtr;

	    (void) NSUnLinkModule(modulePtr->module,
		    NSUNLINKMODULE_OPTION_RESET_LAZY_REFERENCES);
	    modulePtr = modulePtr->nextPtr;
	    Tcl_Free(ptr);
	}
#endif /* TCL_LOAD_FROM_MEMORY */
    }
    Tcl_Free(dyldLoadHandle);
    Tcl_Free(loadHandle);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpLoadMemoryGetBuffer --
 *
 *	Allocate a buffer that can be used with TclpLoadMemory() below.
 *
 * Results:
 *	Pointer to allocated buffer or NULL if an error occurs.
 *
 * Side effects:
 *	Buffer is allocated.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_LOAD_FROM_MEMORY
MODULE_SCOPE void *
TclpLoadMemoryGetBuffer(
    size_t size)			/* Size of desired buffer. */
{
    void *buffer = NULL;

    /*
     * We must allocate the buffer using vm_allocate, because
     * NSCreateObjectFileImageFromMemory will dispose of it using
     * vm_deallocate.
     */

    if (vm_allocate(mach_task_self(), (vm_address_t *) &buffer, size, 1)) {
	buffer = NULL;
    }
    return buffer;
}
#endif /* TCL_LOAD_FROM_MEMORY */

/*
 *----------------------------------------------------------------------
 *
 * TclpLoadMemory --
 *
 *	Dynamically loads binary code file from memory and returns a handle to
 *	the new code.
 *
 * Results:
 *	A standard Tcl completion code. If an error occurs, an error message
 *	is left in the interpreter's result.
 *
 * Side effects:
 *	New code is loaded from memory.
 *
 *----------------------------------------------------------------------
 */

#ifdef TCL_LOAD_FROM_MEMORY
MODULE_SCOPE int
TclpLoadMemory(
    void *buffer,		/* Buffer containing the desired code
				 * (allocated with TclpLoadMemoryGetBuffer). */
    size_t size,		/* Allocation size of buffer. */
    Tcl_Size codeSize,		/* Size of code data read into buffer or -1 if
				 * an error occurred and the buffer should
				 * just be freed. */
    const char *path,
    Tcl_LoadHandle *loadHandle,	/* Filled with token for dynamically loaded
				 * file which will be passed back to
				 * (*unloadProcPtr)() to unload the file. */
    Tcl_FSUnloadFileProc **unloadProcPtr,
				/* Filled with address of Tcl_FSUnloadFileProc
				 * function which should be used for this
				 * file. */
    int flags)
{
    Tcl_LoadHandle newHandle;
    Tcl_DyldLoadHandle *dyldLoadHandle;
    NSObjectFileImage dyldObjFileImage = NULL;
    Tcl_DyldModuleHandle *modulePtr;
    NSModule module;
    int nsflags = NSLINKMODULE_OPTION_RETURN_ON_ERROR;

    /*
     * Try to create an object file image that we can load from.
     */

    if (codeSize >= 0) {
	NSObjectFileImageReturnCode err = NSObjectFileImageSuccess;
	const struct fat_header *fh = (const struct fat_header *)buffer;
	uint32_t ms = 0;
#ifndef __LP64__
	const struct mach_header *mh = NULL;
#	define mh_size  sizeof(struct mach_header)
#	define mh_magic MH_MAGIC
#	define arch_abi 0
#else
	const struct mach_header_64 *mh = NULL;
#	define mh_size  sizeof(struct mach_header_64)
#	define mh_magic MH_MAGIC_64
#	define arch_abi CPU_ARCH_ABI64
#endif /* __LP64__ */

	if ((size_t)codeSize >= sizeof(struct fat_header)
		&& fh->magic == OSSwapHostToBigInt32(FAT_MAGIC)) {
	    uint32_t fh_nfat_arch = OSSwapBigToHostInt32(fh->nfat_arch);

	    /*
	     * Fat binary, try to find mach_header for our architecture
	     */

	    if ((size_t)codeSize >= sizeof(struct fat_header) +
		    fh_nfat_arch * sizeof(struct fat_arch)) {
		void *fatarchs = (char *)buffer + sizeof(struct fat_header);
		const NXArchInfo *arch = NXGetLocalArchInfo();
		struct fat_arch *fa;

		if (fh->magic != FAT_MAGIC) {
		    swap_fat_arch((struct fat_arch *)fatarchs, fh_nfat_arch, arch->byteorder);
		}
		fa = NXFindBestFatArch(arch->cputype | arch_abi,
			arch->cpusubtype, (struct fat_arch *)fatarchs, fh_nfat_arch);
		if (fa) {
		    mh = (const struct mach_header_64 *)((char *) buffer + fa->offset);
		    ms = fa->size;
		} else {
		    err = NSObjectFileImageInappropriateFile;
		}
		if (fh->magic != FAT_MAGIC) {
		    swap_fat_arch((struct fat_arch *)fatarchs, fh_nfat_arch, arch->byteorder);
		}
	    } else {
		err = NSObjectFileImageInappropriateFile;
	    }
	} else {
	    /*
	     * Thin binary
	     */

	    mh = (const struct mach_header_64 *)buffer;
	    ms = codeSize;
	}
	if (ms && !(ms >= mh_size && mh->magic == mh_magic &&
		mh->filetype == MH_BUNDLE)) {
	    err = NSObjectFileImageInappropriateFile;
	}
	if (err == NSObjectFileImageSuccess) {
	    err = NSCreateObjectFileImageFromMemory(buffer, codeSize,
		    &dyldObjFileImage);
	}
    }

    /*
     * If it went wrong (or we were asked to just deallocate), get rid of the
     * memory block.
     */

    if (dyldObjFileImage == NULL) {
	vm_deallocate(mach_task_self(), (vm_address_t) buffer, size);
	return TCL_ERROR;
    }

    /*
     * Extract the module we want from the image of the object file.
     */

    if (!(flags & 1)) {
	nsflags |= NSLINKMODULE_OPTION_PRIVATE;
    }
    if (!(flags & 2)) {
	nsflags |= NSLINKMODULE_OPTION_BINDNOW;
    }
    module = NSLinkModule(dyldObjFileImage, (path ? path : "[Memory Based Bundle]"), nsflags);
    NSDestroyObjectFileImage(dyldObjFileImage);
    if (!module) {
	NSLinkEditErrors editError;
	int errorNumber;
	const char *errorName, *errMsg;

	NSLinkEditError(&editError, &errorNumber, &errorName, &errMsg);
	return TCL_ERROR;
    }

    /*
     * Stash the module reference within the load handle we create and return.
     */

    modulePtr = (Tcl_DyldModuleHandle *)Tcl_Alloc(sizeof(Tcl_DyldModuleHandle));
    modulePtr->module = module;
    modulePtr->nextPtr = NULL;
    dyldLoadHandle = (Tcl_DyldLoadHandle *)Tcl_Alloc(sizeof(Tcl_DyldLoadHandle));
    dyldLoadHandle->dlHandle = NULL;
    dyldLoadHandle->dyldLibHeader = NULL;
    dyldLoadHandle->modulePtr = modulePtr;
    newHandle = (Tcl_LoadHandle)Tcl_Alloc(sizeof(*newHandle));
    newHandle->clientData = dyldLoadHandle;
    newHandle->findSymbolProcPtr = &FindSymbol;
    newHandle->unloadFileProcPtr = &UnloadFile;
    *loadHandle = newHandle;
    *unloadProcPtr = &UnloadFile;
    return TCL_OK;
}
#endif /* TCL_LOAD_FROM_MEMORY */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 79
 * End:
 */
