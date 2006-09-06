/*
 * tclUnixCompat.c
 *
 * Written by: Zoran Vasiljevic (vasiljevic@users.sourceforge.net). 
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclUnixCompat.c,v 1.2 2006/09/06 13:25:29 vasiljevic Exp $
 *
 */

#include "tclInt.h"
#include "tclPort.h"
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <string.h>

/*
 * Mutex to lock access to MT-unsafe calls. This is just to protect 
 * our own usage. It does not protect us from others calling the
 * same functions without (or using some different) lock. 
 */

Tcl_Mutex compatLock;

#if !defined(HAVE_GETHOSTBYNAME_R) || !defined(HAVE_GETHOSTBYADDR_R) || \
    !defined(HAVE_GETPWNAM_R)      || !defined(HAVE_GETGRNAM_R)


/*
 *---------------------------------------------------------------------------
 *
 * CopyArray --
 *
 *      Copies array of strings (or fixed values) to the 
 *      private buffer, honouring the size of the buffer.
 *
 * Results:
 *      Number of bytes copied on success or -1 on error (errno = ERANGE)
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

static int 
CopyArray(char **src, int elsize, char *buf, int buflen)
{
    int i, j, len = 0;
    char *p, **new;

    if (src == NULL) {
        return 0;
    }
    for (i = 0; src[i] != NULL; i++) {
        /* Empty loop to count howmany */
    }
    if ((sizeof(char *)*(i + 1)) >  buflen) {
        return -1;
    }
    len = (sizeof(char *)*(i + 1)); /* Leave place for the array */
    new = (char **)buf;
    p = buf + (sizeof(char *)*(i + 1));
    for (j = 0; j < i; j++) {
        if (elsize < 0) {
            len += strlen(src[j]) + 1;
        } else {
            len += elsize;
        }
        if (len > buflen) {
            return -1;
        }
        if (elsize < 0) {
            strcpy(p, src[j]);
        } else {
            memcpy(p, src[j], elsize);
        }
        new[j] = p;
        p = buf + len;
    }
    new[j] = NULL;

    return len;
}


/*
 *---------------------------------------------------------------------------
 *
 * CopyString --
 *
 *      Copies a string to the private buffer, honouring the 
 *      size of the buffer
 *
 * Results:
 *      0 success or -1 on error (errno = ERANGE)
 *
 * Side effects:
 *      None
 *
 *---------------------------------------------------------------------------
 */


static int 
CopyString(char *src, char *buf, int buflen)
{
    int len = 0;

    if (src != NULL) {
        len += strlen(src) + 1;
        if (len > buflen) {
            return -1;
        }
        strcpy(buf, src);
    }

    return len;
}

#endif /* !defined(HAVE_GETHOSTBYNAME_R) || !defined(HAVE_GETHOSTBYADDR_R) || 
          !defined(HAVE_GETPWNAM_R) || !defined(HAVE_GETGRNAM_R) */


/*
 *---------------------------------------------------------------------------
 *
 * CopyHostnent --
 *
 *      Copies string fields of the hostnent structure to the 
 *      private buffer, honouring the size of the buffer.
 *
 * Results:
 *      Number of bytes copied on success or -1 on error (errno = ERANGE)
 *
 * Side effects:
 *      None
 *
 *---------------------------------------------------------------------------
 */

#if !defined(HAVE_GETHOSTBYNAME_R) || !defined(HAVE_GETHOSTBYADDR_R)
static int
CopyHostent(struct hostent *tgtPtr, char *buf, int buflen)
{
    char *p = buf;
    int copied, len = 0;

    copied = CopyString(tgtPtr->h_name, p, buflen - len);
    if (copied == -1) {
    range:
        errno = ERANGE;
        return -1;
    }
    tgtPtr->h_name = (copied > 0) ? p : NULL;
    len += copied;
    p = buf + len;

    copied = CopyArray(tgtPtr->h_aliases, -1, p, buflen - len);
    if (copied == -1) {
        goto range;
    }
    tgtPtr->h_aliases = (copied > 0) ? (char **)p : NULL;
    len += copied;
    p += len;

    copied = CopyArray(tgtPtr->h_addr_list, tgtPtr->h_length, p, buflen - len);
    if (copied == -1) {
        goto range;
    }
    tgtPtr->h_addr_list = (copied > 0) ? (char **)p : NULL;
   
    return 0;
}
#endif /* !defined(HAVE_GETHOSTBYNAME_R) || !defined(HAVE_GETHOSTBYADDR_R) */


/*
 *---------------------------------------------------------------------------
 *
 * CopyPwd --
 *
 *      Copies string fields of the passwd structure to the 
 *      private buffer, honouring the size of the buffer.
 *
 * Results:
 *      0 on success or -1 on error (errno = ERANGE)
 *
 * Side effects:
 *      We are not copying the gecos field as it may not be supported
 *      on all platforms.
 *
 *---------------------------------------------------------------------------
 */

#if !defined(HAVE_GETPWNAM_R) || !defined(HAVE_GETPWUID_R)
static int
CopyPwd(struct passwd *tgtPtr, char *buf, int buflen)
{
    char *p = buf;
    int copied, len = 0;

    copied = CopyString(tgtPtr->pw_name, p, buflen - len);
    if (copied == -1) {
    range:
        errno = ERANGE;
        return -1;
    }
    tgtPtr->pw_name = (copied > 0) ? p : NULL;
    len += copied;
    p = buf + len;

    copied = CopyString(tgtPtr->pw_passwd, p, buflen - len);
    if (copied == -1) {
        goto range;
    }
    tgtPtr->pw_passwd = (copied > 0) ? p : NULL;
    len += copied;
    p = buf + len;

    copied = CopyString(tgtPtr->pw_dir, p, buflen - len);
    if (copied == -1) {
        goto range;
    }
    tgtPtr->pw_dir = (copied > 0) ? p : NULL;
    len += copied;
    p = buf + len;

    copied = CopyString(tgtPtr->pw_shell, p, buflen - len);
    if (copied == -1) {
        goto range;
    }
    tgtPtr->pw_shell = (copied > 0) ? p : NULL;

    return 0;
}
#endif /* HAVE_GETPWNAM_R || HAVE_GETPWUID_R*/


/*
 *---------------------------------------------------------------------------
 *
 * CopyGrp --
 *
 *      Copies string fields of the group structure to the 
 *      private buffer, honouring the size of the buffer.
 *
 * Results:
 *      0 on success or -1 on error (errno = ERANGE)
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

#if !defined(HAVE_GETGRNAM_R) || !defined(HAVE_GETGRGID_R)
static int
CopyGrp(struct group *tgtPtr, char *buf, int buflen)
{
    register char *p = buf;
    register int copied, len = 0;

    /* Copy username */
    copied = CopyString(tgtPtr->gr_name, p, buflen - len);
    if (copied == -1) {
    range:
        errno = ERANGE;
        return -1;
    }
    tgtPtr->gr_name = (copied > 0) ? p : NULL;
    len += copied;
    p = buf + len;

    /* Copy password */
    copied = CopyString(tgtPtr->gr_passwd, p, buflen - len);
    if (copied == -1) {
        goto range;
    }
    tgtPtr->gr_passwd = (copied > 0) ? p : NULL;
    len += copied;
    p = buf + len;

    /* Copy group members */
    copied = CopyArray((char **)tgtPtr->gr_mem, -1, p, buflen - len);
    if (copied == -1) {
        goto range;
    }
    tgtPtr->gr_mem = (copied > 0) ? (char **)p : NULL;

    return 0;
}
#endif /* HAVE_GETGRNAM_R || HAVE_GETGRGID_R*/


/*
 *---------------------------------------------------------------------------
 *
 * TclpGetPwNam --
 *
 *      Thread-safe wrappers for getpwnam().
 *      See "man getpwnam" for more details.
 *
 * Results:
 *      0 on success or -1 on error.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

int
TclpGetPwNam(const char *name, struct passwd *pwbuf, char *buf, size_t buflen, 
             struct passwd **pwbufp)
{   
    int result = 0;

#if defined(HAVE_GETPWNAM_R_5)
    result = getpwnam_r(name, pwbuf, buf, buflen, pwbufp);
#elif defined(HAVE_GETPWNAM_R_4)
    *pwbufp = getpwnam_r(name, pwbuf, buf, buflen);
    if (*pwbufp == NULL) {
        result = -1;
    }
#else
    struct passwd *pwPtr = NULL;
    Tcl_MutexLock(&compatLock);
    pwPtr = getpwnam(name);
    if (pwPtr == NULL) {
        result = -1;
    } else {
        *pwbuf = *pwPtr;
        *pwbufp = pwbuf;
        result = CopyPwd(pwbuf, buf, buflen);
    }
    Tcl_MutexUnlock(&compatLock);
#endif
    return result;
}


/*
 *---------------------------------------------------------------------------
 *
 * TclpGetPwUid --
 *
 *      Thread-safe wrappers for getpwuid().
 *      See "man getpwuid" for more details.
 *
 * Results:
 *      0 on success or -1 on error.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

int 
TclpGetPwUid(uid_t uid, struct passwd *pwbuf, char  *buf, size_t buflen,
             struct passwd **pwbufp)
{
    int result = 0;

#if defined(HAVE_GETPWUID_R_5)
    result = getpwuid_r(uid, pwbuf, buf, buflen, pwbufp);
#elif defined(HAVE_GETPWUID_R_4)
    *pwbufp = getpwuid_r(uid, pwbuf, buf, buflen);
    if (*pwbufp == NULL) {
        result = -1;
    }
#else
    struct passwd *pwPtr = NULL;
    Tcl_MutexLock(&compatLock);
    pwPtr = getpwuid(uid);
    if (pwPtr == NULL) {
        result = -1;
    } else {
        *pwbuf = *pwPtr;
        *pwbufp = pwbuf;
        result = CopyPwd(pwbuf, buf, buflen);
    }
    Tcl_MutexUnlock(&compatLock);
#endif
    return result;
}


/*
 *---------------------------------------------------------------------------
 *
 * TclpGetGrNam --
 *
 *      Thread-safe wrappers for getgrnam().
 *      See "man getgrnam" for more details.
 *
 * Results:
 *      0 on success or -1 on error.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

int
TclpGetGrNam(const char *name, struct group *gbuf, char *buf, size_t buflen, 
             struct group **gbufp)
{
    int result = 0;

#if defined(HAVE_GETGRNAM_R_5)
    result = getgrnam_r(name, gbuf, buf, buflen, gbufp);
#elif defined(HAVE_GETGRNAM_R_4)
    *gbufp = getgrgid_r(name, gbuf, buf, buflen);
    if (*gbufp == NULL) {
        result = -1;
    }
#else
    struct group *grPtr = NULL;
    Tcl_MutexLock(&compatLock);
    grPtr = getgrnam(name);
    if (grPtr == NULL) {
        result = -1;
    } else {
        *gbuf = *grPtr;
        *gbufp = gbuf;
        result = CopyGrp(gbuf, buf, buflen);
    }
    Tcl_MutexUnlock(&compatLock);
#endif
    return result;
}


/*
 *---------------------------------------------------------------------------
 *
 * TclpGetGrGid --
 *
 *      Thread-safe wrappers for getgrgid().
 *      See "man getgrgid" for more details.
 *
 * Results:
 *      0 on success or -1 on error.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

int
TclpGetGrGid(gid_t gid, struct group *gbuf, char *buf, size_t buflen, 
             struct group **gbufp)
{
    int result = 0;

#if defined(HAVE_GETGRGID_R_5)
    result = getgrgid_r(gid, gbuf, buf, buflen, gbufp);
#elif defined(HAVE_GETGRGID_R_4)
    *gbufp =  getgrgid_r(gid, gbuf, buf, buflen);
    if (*gbufp == NULL) {
        result = -1;
    }
#else
    struct group *grPtr = NULL;
    Tcl_MutexLock(&compatLock);
    grPtr = getgrgid(gid);
    if (grPtr == NULL) {
        result = -1;
    } else {
        *gbuf = *grPtr;
        *gbufp = gbuf;
        result = CopyGrp(gbuf, buf, buflen);
    }
    Tcl_MutexUnlock(&compatLock);
#endif
    return result;
}


/*
 *---------------------------------------------------------------------------
 *
 * TclpGetHostByName --
 *
 *      Thread-safe wrappers for gethostbyname().
 *      See "man gethostbyname" for more details.
 *
 * Results:
 *      Pointer to struct hostent on success or NULL on error.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

struct hostent *
TclpGetHostByName(const char *name, struct hostent *hbuf, char *buf, 
                  size_t buflen, int *h_errnop)
{
#if defined(HAVE_GETHOSTBYNAME_R_5)
    return gethostbyname_r(name, hbuf, buf, buflen, h_errnop);
#elif defined(HAVE_GETHOSTBYNAME_R_6)
    struct hostent *hePtr;
    return (gethostbyname_r(name, hbuf, buf, buflen, &hePtr,
                            h_errnop) == 0) ? hbuf : NULL;
#elif defined(HAVE_GETHOSTBYNAME_R_3)
    struct hostent_data data;
    if (-1 == gethostbyname_r(host, hbuf, &data)) {
        *h_errnop = h_errno;
        return NULL;
    } else {
        return &hbuf;
    }
#else
    struct hostent *hePtr = NULL;
    Tcl_MutexLock(&compatLock);
    hePtr = gethostbyname(name);
    if (hePtr != NULL) {
        *hbuf = *hePtr;
        if (-1 == CopyHostent(hbuf, buf, buflen)) {
            hePtr = NULL;
        } else {
            hePtr = hbuf;
        }
    }
    Tcl_MutexUnlock(&compatLock);
    return hePtr;
#endif
    return NULL; /* Not reached */
}


/*
 *---------------------------------------------------------------------------
 *
 * TclpGetHostByAddr --
 *
 *      Thread-safe wrappers for gethostbyaddr().
 *      See "man gethostbyaddr" for more details.
 *
 * Results:
 *      Pointer to struct hostent on success or NULL on error.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

struct hostent *
TclpGetHostByAddr(const char *addr, int length, int type, struct hostent *hbuf,
                  char *buf, size_t buflen, int *h_errnop)
{
#if defined(HAVE_GETHOSTBYADDR_R_7)
    return gethostbyaddr_r(addr, length, type, hbuf, buf, buflen, h_errnop);
#elif defined(HAVE_GETHOSTBYADDR_R_8)
    struct hostent *hePtr;
    return (gethostbyaddr_r(addr, length, type, hbuf, buf, buflen,
                            &hePtr, h_errnop) == 0) ?  hbuf : NULL;
#else
    struct hostent *hePtr = NULL;
    Tcl_MutexLock(&compatLock);
    hePtr = gethostbyaddr(addr, length, type);
    if (hePtr != NULL) {
        *hbuf = *hePtr;
        if (-1 == CopyHostent(hbuf, buf, buflen)) {
            hePtr = NULL;
        } else {
            hePtr = hbuf;
        }
    }
    Tcl_MutexUnlock(&compatLock);
    return hePtr;
#endif
    return NULL; /* Not reached */
}
