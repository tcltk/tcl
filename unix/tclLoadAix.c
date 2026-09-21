/*
 * tclLoadAix.c --
 *
 *	This file implements the dlopen and dlsym APIs under the AIX operating
 *	system, to enable the Tcl "load" command to work. This code was
 *	provided by Jens-Uwe Mager.
 *
 *	This file is subject to the following copyright notice, which is
 *	different from the notice used elsewhere in Tcl. The file has been
 *	modified to incorporate the file dlfcn.h in-line.
 *
 *	Copyright © 1992,1993,1995,1996, Jens-Uwe Mager, Helios Software GmbH
 *	Not derived from licensed software.
 *
 *	Permission is granted to freely use, copy, modify, and redistribute
 *	this software, provided that the author is not construed to be liable
 *	for any results of using the software, alterations are clearly marked
 *	as such, and this notice is not modified.
 *
 * Note: this file has been altered from the original in a few ways in order
 * to work properly with Tcl.
 */

/*
 * @(#)dlfcn.c	1.7 revision of 95/08/14  19:08:38
 * This is an unpublished work copyright © 1992 HELIOS Software GmbH
 * 30159 Hannover, Germany
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ldr.h>
#include <a.out.h>
#include <ldfcn.h>
#include "../compat/dlfcn.h"

/*
 * We simulate dlopen() et al. through a call to load. Because AIX has no call
 * to find an exported symbol we read the loader section of the loaded module
 * and build a list of exported symbols and their virtual address.
 */

typedef struct {
    char *name;			/* The symbols's name. */
    void *addr;			/* Its relocated virtual address. */
} Export;

/*
 * xlC uses the following structure to list its constructors and destructors.
 * This is gleaned from the output of munch.
 */

typedef struct {
    void (*init)(void);		/* call static constructors */
    void (*term)(void);		/* call static destructors */
} Cdtor;

/*
 * The void * handle returned from dlopen is actually a pointer to a Module.
 */

typedef struct Module {
    struct Module *next;
    char *name;			/* module name for refcounting */
    int refCnt;			/* the number of references */
    void *entry;		/* entry point from load */
    struct dl_info *info;	/* optional init/terminate functions */
    Cdtor *cdtors;		/* optional C++ constructors */
    int nExports;		/* the number of exports found */
    Export *exports;		/* the array of exports */
} Module;

/*
 * We keep a list of all loaded modules to be able to call the fini handlers
 * and destructors at atexit() time.
 */

static Module *modList;

/*
 * The last error from one of the dl* routines is kept in static variables
 * here. Each error is returned only once to the caller.
 */

static char errBuf[BUFSIZ];
static int errValid;

static void	AppendErrorDescription(char *);
static int	ReadXcoffExports(Module *);
static void	TerminateHandler(void);
static void *	FindMainModule(void);

void *
dlopen(
    const char *path,
    int mode)
{
    Module *mp;
    static void *mainModule;

    /*
     * Upon the first call register a terminate handler that will close all
     * libraries. Also get a reference to the main module for use with
     * loadbind.
     */

    if (!mainModule) {
	mainModule = FindMainModule();
	if (mainModule == NULL) {
	    return NULL;
	}
	atexit(TerminateHandler);
    }

    /*
     * Scan the list of modules if we have the module already loaded.
     */

    for (mp = modList; mp; mp = mp->next) {
	if (strcmp(mp->name, path) == 0) {
	    mp->refCnt++;
	    return (void *)mp;
	}
    }

    mp = (Module *) calloc(1, sizeof(*mp));
    if (mp == NULL) {
	errValid = 1;
	strcpy(errBuf, "calloc: ");
	strcat(errBuf, strerror(errno));
	return NULL;
    }

    mp->name = malloc(strlen(path) + 1);
    strcpy(mp->name, path);

    /*
     * load should be declared load(const char *...). Thus we cast the path to
     * a normal char *. Ugly.
     */

    mp->entry = (void *)load((char *)path, L_NOAUTODEFER, NULL);
    if (mp->entry == NULL) {
	free(mp->name);
	free(mp);
	errValid = 1;
	strcpy(errBuf, "dlopen: ");
	strcat(errBuf, path);
	strcat(errBuf, ": ");

	/*
	 * If AIX says the file is not executable, the error can be further
	 * described by querying the loader about the last error.
	 */

	if (errno == ENOEXEC) {
	    char *tmp[BUFSIZ/sizeof(char *)], **p;

	    if (loadquery(L_GETMESSAGES, tmp, sizeof(tmp)) == -1) {
		strcpy(errBuf, strerror(errno));
	    } else {
		for (p=tmp ; *p ; p++) {
		    AppendErrorDescription(*p);
		}
	    }
	} else {
	    strcat(errBuf, strerror(errno));
	}
	return NULL;
    }

    mp->refCnt = 1;
    mp->next = modList;
    modList = mp;

    if (loadbind(0, mainModule, mp->entry) == -1) {
    loadbindFailure:
	dlclose(mp);
	errValid = 1;
	strcpy(errBuf, "loadbind: ");
	strcat(errBuf, strerror(errno));
	return NULL;
    }

    /*
     * If the user wants global binding, loadbind against all other loaded
     * modules.
     */

    if (mode & RTLD_GLOBAL) {
	Module *mp1;

	for (mp1 = mp->next; mp1; mp1 = mp1->next) {
	    if (loadbind(0, mp1->entry, mp->entry) == -1) {
		goto loadbindFailure;
	    }
	}
    }

    if (ReadXcoffExports(mp) == -1) {
	dlclose(mp);
	return NULL;
    }

    /*
     * If there is a dl_info structure, call the init function.
     */

    if ((mp->info = (struct dl_info *) dlsym(mp, "dl_info"))) {
	if (mp->info->init) {
	    mp->info->init();
	}
    } else {
	errValid = 0;
    }

    /*
     * If the shared object was compiled using xlC we will need to call static
     * constructors (and later on dlclose destructors).
     */

    if ((mp->cdtors = (Cdtor *) dlsym(mp, "__cdtors"))) {
	while (mp->cdtors->init) {
	    mp->cdtors->init();
	    mp->cdtors++;
	}
    } else {
	errValid = 0;
    }

    return (void *)mp;
}

/*
 *----------------------------------------------------------------------
 *
 * AppendErrorDescription --
 *
 *	Attempt to decipher an AIX loader error message and append it to our
 *	static error message buffer.
 *
 * Returns:
 *	None
 *
 * Side effects:
 *	Updates the errBuf global static variable.
 *
 *----------------------------------------------------------------------
 */
static void
AppendErrorDescription(
    char *s)
{
    char *p = s;

    while (*p >= '0' && *p <= '9') {
	p++;
    }
    switch (atoi(s)) {		/* INTL: "C", UTF safe. */
    case L_ERROR_TOOMANY:
	strcat(errBuf, "to many errors");
	break;
    case L_ERROR_NOLIB:
	strcat(errBuf, "cannot load library");
	strcat(errBuf, p);
	break;
    case L_ERROR_UNDEF:
	strcat(errBuf, "cannot find symbol");
	strcat(errBuf, p);
	break;
    case L_ERROR_RLDBAD:
	strcat(errBuf, "bad RLD");
	strcat(errBuf, p);
	break;
    case L_ERROR_FORMAT:
	strcat(errBuf, "bad exec format in");
	strcat(errBuf, p);
	break;
    case L_ERROR_ERRNO:
	strcat(errBuf, strerror(atoi(++p)));	/* INTL: "C", UTF safe. */
	break;
    default:
	strcat(errBuf, s);
	break;
    }
}

void *
dlsym(
    void *handle,
    const char *symbol)
{
    Module *mp = (Module *)handle;
    Export *ep;
    int i;

    /*
     * Could speed up the search, but I assume that one assigns the result to
     * function pointers anyways.
     */

    for (ep = mp->exports, i = mp->nExports; i; i--, ep++) {
	if (strcmp(ep->name, symbol) == 0) {
	    return ep->addr;
	}
    }

    errValid = 1;
    strcpy(errBuf, "dlsym: undefined symbol ");
    strcat(errBuf, symbol);
    return NULL;
}

char *
dlerror(void)
{
    if (errValid) {
	errValid = 0;
	return errBuf;
    }
    return NULL;
}

int
dlclose(
    void *handle)
{
    Module *mp = (Module *)handle;
    int result;
    Module *mp1;

    if (--mp->refCnt > 0) {
	return 0;
    }

    if (mp->info && mp->info->fini) {
	mp->info->fini();
    }

    if (mp->cdtors) {
	while (mp->cdtors->term) {
	    mp->cdtors->term();
	    mp->cdtors++;
	}
    }

    result = unload(mp->entry);
    if (result == -1) {
	errValid = 1;
	strcpy(errBuf, strerror(errno));
    }

    if (mp->exports) {
	Export *ep;
	int i;
	for (ep = mp->exports, i = mp->nExports; i; i--, ep++) {
	    if (ep->name) {
		free(ep->name);
	    }
	}
	free(mp->exports);
    }

    if (mp == modList) {
	modList = mp->next;
    } else {
	for (mp1 = modList; mp1; mp1 = mp1->next) {
	    if (mp1->next == mp) {
		mp1->next = mp->next;
		break;
	    }
	}
    }

    free(mp->name);
    free(mp);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TerminateHandler --
 *
 *	Callback at exit to clean things up.
 *
 * Returns:
 *	None.
 *
 * Side effects:
 *	Closes attached modules.
 *
 *----------------------------------------------------------------------
 */
static void
TerminateHandler(void)
{
    while (modList) {
	dlclose(modList);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ReadXcoffExports --
 *
 *	Build the export table from the XCOFF .loader section.
 *
 * Returns:
 *	0 on success, -1 on failure.
 *
 * Side effects:
 *	Updates the module descriptor with the exports it has found.
 *
 *----------------------------------------------------------------------
 */
static int
ReadXcoffExports(
    Module *mp)
{
    LDFILE *ldp = NULL;
    SCNHDR sh, shdata;
    LDHDR *lhp;
    char *ldbuf;
    LDSYM *ls;
    int i;
    Export *ep;
    const char *errMsg;

#define Error(msg) \
    do {			\
	errMsg = (msg);		\
	goto error;		\
    } while (0)
#define SysErr() Error(strerror(errno))

    ldp = ldopen(mp->name, ldp);
    if (ldp == NULL) {
	struct ld_info *lp;
	char *buf;
	int size = 0;

	if (errno != ENOENT) {
	    SysErr();
	}

	/*
	 * The module might be loaded due to the LIBPATH environment variable.
	 * Search for the loaded module using L_GETINFO.
	 */

	while (1) {
	    size += 4 * 1024;
	    buf = malloc(size);
	    if (buf == NULL) {
		SysErr();
	    }

	    i = loadquery(L_GETINFO, buf, size);

	    if (i != -1) {
		break;
	    }
	    free(buf);
	    if (errno != ENOMEM) {
		SysErr();
	    }
	}

	/*
	 * Traverse the list of loaded modules. The entry point returned by
	 * load() does actually point to the data segment origin.
	 */

	lp = (struct ld_info *) buf;
	while (lp) {
	    if (lp->ldinfo_dataorg == mp->entry) {
		ldp = ldopen(lp->ldinfo_filename, ldp);
		break;
	    }
	    if (lp->ldinfo_next == 0) {
		lp = NULL;
	    } else {
		lp = (struct ld_info *)((char *)lp + lp->ldinfo_next);
	    }
	}

	free(buf);

	if (!ldp) {
	    SysErr();
	}
    }

    if (TYPE(ldp) != U802TOCMAGIC) {
	Error("bad magic");
    }

    /*
     * Get the padding for the data section. This is needed for AIX 4.1
     * compilers. This is used when building the final function pointer to the
     * exported symbol.
     */

    if (ldnshread(ldp, _DATA, &shdata) != SUCCESS) {
	Error("cannot read data section header");
    }

    if (ldnshread(ldp, _LOADER, &sh) != SUCCESS) {
	Error("cannot read loader section header");
    }

    /*
     * We read the complete loader section in one chunk, this makes finding
     * long symbol names residing in the string table easier.
     */

    ldbuf = (char *) malloc(sh.s_size);
    if (ldbuf == NULL) {
	SysErr();
    }

    if (FSEEK(ldp, sh.s_scnptr, BEGINNING) != OKFSEEK) {
	free(ldbuf);
	Error("cannot seek to loader section");
    }

    if (FREAD(ldbuf, sh.s_size, 1, ldp) != 1) {
	free(ldbuf);
	Error("cannot read loader section");
    }

    lhp = (LDHDR *) ldbuf;
    ls = (LDSYM *)(ldbuf + LDHDRSZ);

    /*
     * Count the number of exports to include in our export table.
     */

    for (i = lhp->l_nsyms; i; i--, ls++) {
	if (!LDR_EXPORT(*ls)) {
	    continue;
	}
	mp->nExports++;
    }

    mp->exports = (Export *) calloc(mp->nExports, sizeof(*mp->exports));
    if (mp->exports == NULL) {
	free(ldbuf);
	SysErr();
    }

    /*
     * Fill in the export table. All entries are relative to the entry point
     * we got from load.
     */

    ep = mp->exports;
    ls = (LDSYM *)(ldbuf + LDHDRSZ);
    for (i=lhp->l_nsyms ; i!=0 ; i--,ls++) {
	char *symname;
	char tmpsym[SYMNMLEN+1];

	if (!LDR_EXPORT(*ls)) {
	    continue;
	}

	if (ls->l_zeroes == 0) {
	    symname = ls->l_offset + lhp->l_stoff + ldbuf;
	} else {
	    /*
	     * The l_name member is not zero terminated, we must copy the
	     * first SYMNMLEN chars and make sure we have a zero byte at the
	     * end.
	     */

	    strncpy(tmpsym, ls->l_name, SYMNMLEN);
	    tmpsym[SYMNMLEN] = '\0';
	    symname = tmpsym;
	}
	ep->name = malloc(strlen(symname) + 1);
	strcpy(ep->name, symname);
	ep->addr = (void *)((unsigned long)
		mp->entry + ls->l_value - shdata.s_vaddr);
	ep++;
    }
    free(ldbuf);
    while (ldclose(ldp) == FAILURE) {
	/* Empty body */
    }
    return 0;

    /*
     * This is a factoring out of the error-handling code to make the rest of
     * the function much simpler to read.
     */

  error:
    errValid = 1;
    strcpy(errBuf, "ReadXcoffExports: ");
    strcat(errBuf, errMsg);

    if (ldp != NULL) {
	while (ldclose(ldp) == FAILURE) {
	    /* Empty body */
	}
    }
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * FindMainModule --
 *
 *	Find the main modules entry point. This is used as export pointer for
 *	loadbind() to be able to resolve references to the main part.
 *
 * Returns:
 *	Address of main entry point, or NULL if it can't be located.
 *
 *----------------------------------------------------------------------
 */
static void *
FindMainModule(void)
{
    struct ld_info *lp;
    char *buf;
    int size = 4*1024;
    int i;
    void *ret;

    buf = malloc(size);
    if (buf == NULL) {
	goto error;
    }

    while ((i = loadquery(L_GETINFO, buf, size)) == -1 && errno == ENOMEM) {
	free(buf);
	size += 4*1024;
	buf = malloc(size);
	if (buf == NULL) {
	    goto error;
	}
    }

    if (i == -1) {
	free(buf);
	goto error;
    }

    /*
     * The first entry is the main module. The entry point returned by load()
     * does actually point to the data segment origin.
     */

    lp = (struct ld_info *) buf;
    ret = lp->ldinfo_dataorg;
    free(buf);
    return ret;

  error:
    errValid = 1;
    strcpy(errBuf, "FindMainModule: ");
    strcat(errBuf, strerror(errno));
    return NULL;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
