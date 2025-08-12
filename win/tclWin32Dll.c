/*
 * tclWin32Dll.c --
 *
 *	This file contains the DLL entry point and other low-level bit bashing
 *	code that needs inline assembly.
 *
 * Copyright © 1995-1996 Sun Microsystems, Inc.
 * Copyright © 1998-2000 Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclWinInt.h"
#if defined(HAVE_CPUID_H)
#   include <cpuid.h>
#elif defined(_MSC_VER)
#   include <intrin.h>
#endif

/*
 * The following variables keep track of information about this DLL on a
 * per-instance basis. Each time this DLL is loaded, it gets its own new data
 * segment with its own copy of all static and global information.
 */

static HINSTANCE hInstance;	/* HINSTANCE of this DLL. */

/*
 * The following declaration is for the VC++ DLL entry point.
 */

BOOL APIENTRY		DllMain(HINSTANCE hInst, DWORD reason,
			    LPVOID reserved);

/*
 * The following structure and linked list is to allow us to map between
 * volume mount points and drive letters on the fly (no Win API exists for
 * this).
 */

typedef struct MountPointMap {
    WCHAR *volumeName;		/* Native wide string volume name. */
    WCHAR driveLetter;		/* Drive letter corresponding to the volume
				 * name. */
    struct MountPointMap *nextPtr;
				/* Pointer to next structure in list, or
				 * NULL. */
} MountPointMap;

/*
 * This is the head of the linked list, which is protected by the mutex which
 * follows, for thread-enabled builds.
 */

MountPointMap *driveLetterLookup = NULL;
TCL_DECLARE_MUTEX(mountPointMap)

/*
 * We will need this below.
 */

#ifdef _WIN32
#ifndef STATIC_BUILD

/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This wrapper function is used by Borland to invoke the initialization
 *	code for Tcl. It simply calls the DllMain routine.
 *
 * Results:
 *	See DllMain.
 *
 * Side effects:
 *	See DllMain.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY
DllEntryPoint(
    HINSTANCE hInst,		/* Library instance handle. */
    DWORD reason,		/* Reason this function is being called. */
    LPVOID reserved)
{
    return DllMain(hInst, reason, reserved);
}

/*
 *----------------------------------------------------------------------
 *
 * DllMain --
 *
 *	This routine is called by the VC++ C run time library init code, or
 *	the DllEntryPoint routine. It is responsible for initializing various
 *	dynamically loaded libraries.
 *
 * Results:
 *	TRUE on sucess, FALSE on failure.
 *
 * Side effects:
 *	Initializes most rudimentary Windows bits.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY
DllMain(
    HINSTANCE hInst,		/* Library instance handle. */
    DWORD reason,		/* Reason this function is being called. */
    TCL_UNUSED(LPVOID))
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
	DisableThreadLibraryCalls(hInst);
	TclWinInit(hInst);
	return TRUE;

	/*
	 * DLL_PROCESS_DETACH is unnecessary as the user should call
	 * Tcl_Finalize explicitly before unloading Tcl.
	 */
    }

    return TRUE;
}
#endif /* !STATIC_BUILD */
#endif /* _WIN32 */

/*
 *----------------------------------------------------------------------
 *
 * TclWinGetTclInstance --
 *
 *	Retrieves the global library instance handle.
 *
 * Results:
 *	Returns the global library instance handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void *
TclWinGetTclInstance(void)
{
    return hInstance;
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinInit --
 *
 *	This function initializes the internal state of the tcl library.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Initializes the tclPlatformId variable.
 *
 *----------------------------------------------------------------------
 */

void
TclWinInit(
    HINSTANCE hInst)		/* Library instance handle. */
{
    OSVERSIONINFOW os;

    hInstance = hInst;
    os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
    GetVersionExW(&os);

    /*
     * We no longer support Win32s or Win9x or Windows CE or Windows XP, so just
     * in case someone manages to get a runtime there, make sure they know that.
     */

    if (os.dwPlatformId != VER_PLATFORM_WIN32_NT) {
	Tcl_Panic("Windows 7 is the minimum supported platform");
    }
}

/*
 *-------------------------------------------------------------------------
 *
 * TclWinNoBackslash --
 *
 *	We're always iterating through a string in Windows, changing the
 *	backslashes to slashes for use in Tcl.
 *
 * Results:
 *	All backslashes in given string are changed to slashes.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

char *
TclWinNoBackslash(
    char *path)			/* String to change. */
{
    for (char *p = path; *p != '\0'; p++) {
	if (*p == '\\') {
	    *p = '/';
	}
    }
    return path;
}

/*
 *---------------------------------------------------------------------------
 *
 * TclWinEncodingsCleanup --
 *
 *	Called during finalization to clean up any memory allocated in our
 *	mount point map which is used to follow certain kinds of symlinks.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

void
TclWinEncodingsCleanup(void)
{
    MountPointMap *dlIter, *dlIter2;

    /*
     * Clean up the mount point map.
     */

    Tcl_MutexLock(&mountPointMap);
    dlIter = driveLetterLookup;
    while (dlIter != NULL) {
	dlIter2 = dlIter->nextPtr;
	Tcl_Free(dlIter->volumeName);
	Tcl_Free(dlIter);
	dlIter = dlIter2;
    }
    Tcl_MutexUnlock(&mountPointMap);
}

/*
 *--------------------------------------------------------------------
 *
 * TclWinDriveLetterForVolMountPoint
 *
 *	Unfortunately, Windows provides no easy way at all to get hold of the
 *	drive letter for a volume mount point, but we need that information to
 *	understand paths correctly. So, we have to build an associated array
 *	to find these correctly, and allow quick and easy lookup from volume
 *	mount points to drive letters.
 *
 *	We assume here that we are running on a system for which the wide
 *	character interfaces are used, which is valid for Win 2000 and WinXP
 *	which are the only systems on which this function will ever be called.
 *
 * Result:
 *	The drive letter, or -1 if no drive letter corresponds to the given
 *	mount point.
 *
 *--------------------------------------------------------------------
 */

char
TclWinDriveLetterForVolMountPoint(
    const WCHAR *mountPoint)
{
    MountPointMap *dlIter, *dlPtr2;
    WCHAR Target[55];		/* Target of mount at mount point */
    WCHAR drive[4] = L"A:\\";

    /*
     * Detect the volume mounted there. Unfortunately, there is no simple way
     * to map a unique volume name to a DOS drive letter. So, we have to build
     * an associative array.
     */

    Tcl_MutexLock(&mountPointMap);
    dlIter = driveLetterLookup;
    while (dlIter != NULL) {
	if (wcscmp(dlIter->volumeName, mountPoint) == 0) {
	    /*
	     * We need to check whether this information is still valid, since
	     * either the user or various programs could have adjusted the
	     * mount points on the fly.
	     */

	    drive[0] = (WCHAR) dlIter->driveLetter;

	    /*
	     * Try to read the volume mount point and see where it points.
	     */

	    if (GetVolumeNameForVolumeMountPointW(drive,
		    Target, 55) != 0) {
		if (wcscmp(dlIter->volumeName, Target) == 0) {
		    /*
		     * Nothing has changed.
		     */

		    Tcl_MutexUnlock(&mountPointMap);
		    return (char) dlIter->driveLetter;
		}
	    }

	    /*
	     * If we reach here, unfortunately, this mount point is no longer
	     * valid at all.
	     */

	    if (driveLetterLookup == dlIter) {
		dlPtr2 = dlIter;
		driveLetterLookup = dlIter->nextPtr;
	    } else {
		for (dlPtr2 = driveLetterLookup;
			dlPtr2 != NULL; dlPtr2 = dlPtr2->nextPtr) {
		    if (dlPtr2->nextPtr == dlIter) {
			dlPtr2->nextPtr = dlIter->nextPtr;
			dlPtr2 = dlIter;
			break;
		    }
		}
	    }

	    /*
	     * Now dlPtr2 points to the structure to free.
	     */

	    Tcl_Free(dlPtr2->volumeName);
	    Tcl_Free(dlPtr2);

	    /*
	     * Restart the loop - we could try to be clever and continue half
	     * way through, but the logic is a bit messy, so it's cleanest
	     * just to restart.
	     */

	    dlIter = driveLetterLookup;
	    continue;
	}
	dlIter = dlIter->nextPtr;
    }

    /*
     * We couldn't find it, so we must iterate over the letters.
     */

    for (drive[0] = 'A'; drive[0] <= 'Z'; drive[0]++) {
	/*
	 * Try to read the volume mount point and see where it points.
	 */

	if (GetVolumeNameForVolumeMountPointW(drive,
		Target, 55) != 0) {
	    bool alreadyStored = false;

	    for (dlIter = driveLetterLookup; dlIter != NULL;
		    dlIter = dlIter->nextPtr) {
		if (wcscmp(dlIter->volumeName, Target) == 0) {
		    alreadyStored = true;
		    break;
		}
	    }
	    if (!alreadyStored) {
		dlPtr2 = (MountPointMap *)Tcl_Alloc(sizeof(MountPointMap));
		dlPtr2->volumeName = (WCHAR *)TclNativeDupInternalRep(Target);
		dlPtr2->driveLetter = (WCHAR) drive[0];
		dlPtr2->nextPtr = driveLetterLookup;
		driveLetterLookup = dlPtr2;
	    }
	}
    }

    /*
     * Try again.
     */

    for (dlIter = driveLetterLookup; dlIter != NULL;
	    dlIter = dlIter->nextPtr) {
	if (wcscmp(dlIter->volumeName, mountPoint) == 0) {
	    Tcl_MutexUnlock(&mountPointMap);
	    return (char) dlIter->driveLetter;
	}
    }

    /*
     * The volume doesn't appear to correspond to a drive letter - we remember
     * that fact and store '-1' so we don't have to look it up each time.
     */

    dlPtr2 = (MountPointMap *)Tcl_Alloc(sizeof(MountPointMap));
    dlPtr2->volumeName = (WCHAR *)TclNativeDupInternalRep((void *)mountPoint);
    dlPtr2->driveLetter = (WCHAR)-1;
    dlPtr2->nextPtr = driveLetterLookup;
    driveLetterLookup = dlPtr2;
    Tcl_MutexUnlock(&mountPointMap);
    return -1;
}

/*
 *------------------------------------------------------------------------
 *
 * TclWinCPUID --
 *
 *	Get CPU ID information on an Intel box under Windows
 *
 * Results:
 *	Returns TCL_OK if successful, TCL_ERROR if CPUID is not supported or
 *	fails.
 *
 * Side effects:
 *	If successful, stores EAX, EBX, ECX and EDX registers after the CPUID
 *	instruction in the four integers designated by 'regsPtr'
 *
 *----------------------------------------------------------------------
 */

int
TclWinCPUID(
    int index,			/* Which CPUID value to retrieve. */
    int *regsPtr)		/* Registers after the CPUID. */
{
    int status = TCL_ERROR;

#if defined(HAVE_CPUID_H)

    unsigned int *regs = (unsigned int *)regsPtr;
    __get_cpuid(index, &regs[0], &regs[1], &regs[2], &regs[3]);
    status = TCL_OK;

#elif defined(_MSC_VER) && defined(_WIN64) && defined(HAVE_CPUID)

    __cpuid((int *)regsPtr, index);
    status = TCL_OK;

#elif defined (_M_IX86)
    /*
     * Define a structure in the stack frame to hold the registers.
     */

    struct {
	DWORD dw0;
	DWORD dw1;
	DWORD dw2;
	DWORD dw3;
    } regs;
    regs.dw0 = index;

    /*
     * Execute the CPUID instruction and save regs in the stack frame.
     */

    _try {
	_asm {
	    push    ebx
	    push    ecx
	    push    edx
	    mov	    eax, regs.dw0
	    cpuid
	    mov	    regs.dw0, eax
	    mov	    regs.dw1, ebx
	    mov	    regs.dw2, ecx
	    mov	    regs.dw3, edx
	    pop	    edx
	    pop	    ecx
	    pop	    ebx
	}

	/*
	 * Copy regs back out to the caller.
	 */

	regsPtr[0] = regs.dw0;
	regsPtr[1] = regs.dw1;
	regsPtr[2] = regs.dw2;
	regsPtr[3] = regs.dw3;

	status = TCL_OK;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
	/* do nothing */
    }

#else
    (void)index;
    (void)regsPtr;
    /*
     * Don't know how to do assembly code for this compiler and/or
     * architecture.
     */
#endif
    return status;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
