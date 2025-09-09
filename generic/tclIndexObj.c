/*
 * tclIndexObj.c --
 *
 *	This file implements objects of type "index". This object type is used
 *	to lookup a keyword in a table of valid values and cache the index of
 *	the matching entry. Also provides table-based argv/argc processing.
 *
 * Copyright © 1990-1994 The Regents of the University of California.
 * Copyright © 1997 Sun Microsystems, Inc.
 * Copyright © 2006 Sam Bromley.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * Prototypes for functions defined later in this file:
 */

static int		GetIndexFromObjList(Tcl_Interp *interp,
			    Tcl_Obj *objPtr, Tcl_Obj *tableObjPtr,
			    const char *msg, int flags, Tcl_Size *indexPtr);
static void		UpdateStringOfIndex(Tcl_Obj *objPtr);
static void		DupIndex(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr);
static void		FreeIndex(Tcl_Obj *objPtr);
static Tcl_ObjCmdProc PrefixAllObjCmd;
static Tcl_ObjCmdProc PrefixLongestObjCmd;
static Tcl_ObjCmdProc PrefixMatchObjCmd;
static void		PrintUsage(Tcl_Interp *interp,
			    const Tcl_ArgvInfo *argTable);

/*
 * The structure below defines the index Tcl object type by means of functions
 * that can be invoked by generic object code.
 */

const Tcl_ObjType tclIndexType = {
    "index",			/* name */
    FreeIndex,			/* freeIntRepProc */
    DupIndex,			/* dupIntRepProc */
    UpdateStringOfIndex,	/* updateStringProc */
    NULL,			/* setFromAnyProc */
    TCL_OBJTYPE_V0
};

/*
 * The definition of the internal representation of the "index" object; The
 * internalRep.twoPtrValue.ptr1 field of an object of "index" type will be a
 * pointer to one of these structures.
 *
 * Keep this structure declaration in sync with tclTestObj.c
 */

typedef struct {
    void *tablePtr;		/* Pointer to the table of strings */
    Tcl_Size offset;		/* Offset between table entries */
    Tcl_Size index;		/* Selected index into table. */
} IndexRep;

/*
 * The following macros greatly simplify moving through a table...
 */

#define STRING_AT(table, offset) \
	(*((const char *const *)(((char *)(table)) + (offset))))
#define NEXT_ENTRY(table, offset) \
	(&(STRING_AT(table, offset)))
#define EXPAND_OF(indexRep) \
	(((indexRep)->index != TCL_INDEX_NONE) ?			\
		STRING_AT((indexRep)->tablePtr, (indexRep)->offset*(indexRep)->index) : \
		"")

/*
 *----------------------------------------------------------------------
 *
 * GetIndexFromObjList --
 *
 *	This procedure looks up an object's value in a table of strings and
 *	returns the index of the matching string, if any.
 *
 * Results:
 *	If the value of objPtr is identical to or a unique abbreviation for
 *	one of the entries in tableObjPtr, then the return value is TCL_OK and
 *	the index of the matching entry is stored at *indexPtr. If there isn't
 *	a proper match, then TCL_ERROR is returned and an error message is
 *	left in interp's result (unless interp is NULL). The msg argument is
 *	used in the error message; for example, if msg has the value "option"
 *	then the error message will say something flag 'bad option "foo": must
 *	be ...'
 *
 * Side effects:
 *	Removes any internal representation that the object might have. (TODO:
 *	find a way to cache the lookup.)
 *
 *----------------------------------------------------------------------
 */

int
GetIndexFromObjList(
    Tcl_Interp *interp,		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr,		/* Object containing the string to lookup. */
    Tcl_Obj *tableObjPtr,	/* List of strings to compare against the
				 * value of objPtr. */
    const char *msg,		/* Identifying word to use in error
				 * messages. */
    int flags,			/* 0 or TCL_EXACT */
    Tcl_Size *indexPtr)		/* Place to store resulting index. */
{

    Tcl_Size objc, t;
    int result;
    Tcl_Obj **objv;
    const char **tablePtr;

    /*
     * Use Tcl_GetIndexFromObjStruct to do the work to avoid duplicating most
     * of the code there. This is a bit inefficient but simpler.
     */

    result = TclListObjGetElements(interp, tableObjPtr, &objc, &objv);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Build a string table from the list.
     */

    tablePtr = (const char **)Tcl_Alloc((objc + 1) * sizeof(char *));
    for (t = 0; t < objc; t++) {
	if (objv[t] == objPtr) {
	    /*
	     * An exact match is always chosen, so we can stop here.
	     */

	    Tcl_Free(tablePtr);
	    *indexPtr = t;
	    return TCL_OK;
	}

	tablePtr[t] = TclGetString(objv[t]);
    }
    tablePtr[objc] = NULL;

    result = Tcl_GetIndexFromObjStruct(interp, objPtr, tablePtr,
	    sizeof(char *), msg, flags | TCL_INDEX_TEMP_TABLE, indexPtr);

    Tcl_Free(tablePtr);

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetIndexFromObjStruct --
 *
 *	This function looks up an object's value given a starting string and
 *	an offset for the amount of space between strings. This is useful when
 *	the strings are embedded in some other kind of array.
 *
 * Results:
 *	If the value of objPtr is identical to or a unique abbreviation for
 *	one of the entries in tablePtr, then the return value is TCL_OK and
 *	the index of the matching entry is stored at *indexPtr
 *	(unless indexPtr is NULL). If there isn't a proper match, then
 *	TCL_ERROR is returned and an error message is left in interp's
 *	result (unless interp is NULL). The msg argument is used in the
 *	error message; for example, if msg has the value "option" then
 *	the error message will say something like 'bad option "foo": must
 *	be ...'
 *
 * Side effects:
 *	The result of the lookup is cached as the internal rep of objPtr, so
 *	that repeated lookups can be done quickly.
 *
 *----------------------------------------------------------------------
 */

#undef Tcl_GetIndexFromObjStruct
int
Tcl_GetIndexFromObjStruct(
    Tcl_Interp *interp,		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr,		/* Object containing the string to lookup. */
    const void *tablePtr,	/* The first string in the table. The second
				 * string will be at this address plus the
				 * offset, the third plus the offset again,
				 * etc. The last entry must be NULL and there
				 * must not be duplicate entries. */
    Tcl_Size offset,		/* The number of bytes between entries */
    const char *msg,		/* Identifying word to use in error
				 * messages. */
    int flags,			/* 0, TCL_EXACT, TCL_NULL_OK or TCL_INDEX_TEMP_TABLE */
    void *indexPtr)		/* Place to store resulting index. */
{
    Tcl_Size index, idx, numAbbrev;
    const char *key, *p1;
    const char *p2;
    const char *const *entryPtr;
    Tcl_Obj *resultPtr;
    IndexRep *indexRep;
    const Tcl_ObjInternalRep *irPtr;

    if (offset < (Tcl_Size)sizeof(char *)) {
	if (interp) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "Invalid %s value %" TCL_SIZE_MODIFIER "d.",
		    "struct offset", offset));
	}
	return TCL_ERROR;
    }
    /*
     * See if there is a valid cached result from a previous lookup.
     */

    if (objPtr && !(flags & TCL_INDEX_TEMP_TABLE)) {
	irPtr = TclFetchInternalRep(objPtr, &tclIndexType);
	if (irPtr) {
	    indexRep = (IndexRep *)irPtr->twoPtrValue.ptr1;
	    if ((indexRep->tablePtr == tablePtr)
		    && (indexRep->offset == offset)
		    && (indexRep->index != TCL_INDEX_NONE)) {
		index = indexRep->index;
		goto uncachedDone;
	    }
	}
    }

    /*
     * Lookup the value of the object in the table. Accept unique
     * abbreviations unless TCL_EXACT is set in flags.
     */

    key = objPtr ? TclGetString(objPtr) : "";
    index = TCL_INDEX_NONE;
    numAbbrev = 0;

    if (!*key && (flags & TCL_NULL_OK)) {
	goto uncachedDone;
    }
    /*
     * Scan the table looking for one of:
     *  - An exact match (always preferred)
     *  - A single abbreviation (allowed depending on flags)
     *  - Several abbreviations (never allowed, but overridden by exact match)
     */

    for (entryPtr = (const char *const *)tablePtr, idx = 0; *entryPtr != NULL;
	    entryPtr = NEXT_ENTRY(entryPtr, offset), idx++) {
	for (p1 = key, p2 = *entryPtr; *p1 == *p2; p1++, p2++) {
	    if (*p1 == '\0') {
		index = idx;
		goto done;
	    }
	}
	if (*p1 == '\0') {
	    /*
	     * The value is an abbreviation for this entry. Continue checking
	     * other entries to make sure it's unique. If we get more than one
	     * unique abbreviation, keep searching to see if there is an exact
	     * match, but remember the number of unique abbreviations and
	     * don't allow either.
	     */

	    numAbbrev++;
	    index = idx;
	}
    }

    /*
     * Check if we were instructed to disallow abbreviations.
     */

    if ((flags & TCL_EXACT) || (key[0] == '\0') || (numAbbrev != 1)) {
	goto error;
    }

  done:
    /*
     * Cache the found representation. Note that we want to avoid allocating a
     * new internal-rep if at all possible since that is potentially a slow
     * operation.
     */

    if (objPtr && (index != TCL_INDEX_NONE) && !(flags & TCL_INDEX_TEMP_TABLE)) {
    irPtr = TclFetchInternalRep(objPtr, &tclIndexType);
    if (irPtr) {
	indexRep = (IndexRep *)irPtr->twoPtrValue.ptr1;
    } else {
	Tcl_ObjInternalRep ir;

	indexRep = (IndexRep*)Tcl_Alloc(sizeof(IndexRep));
	ir.twoPtrValue.ptr1 = indexRep;
	Tcl_StoreInternalRep(objPtr, &tclIndexType, &ir);
    }
    indexRep->tablePtr = (void *) tablePtr;
    indexRep->offset = offset;
    indexRep->index = index;
    }

  uncachedDone:
    if (indexPtr != NULL) {
	flags &= (30-(int)(sizeof(int)<<1));
	if (flags) {
	    if (flags == sizeof(uint16_t)<<1) {
		*(uint16_t *)indexPtr = (uint16_t)index;
		return TCL_OK;
	    } else if (flags == (int)(sizeof(uint8_t)<<1)) {
		*(uint8_t *)indexPtr = (uint8_t)index;
		return TCL_OK;
	    } else if (flags == (int)(sizeof(int64_t)<<1)) {
		*(int64_t *)indexPtr = index;
		return TCL_OK;
	    } else if (flags == (int)(sizeof(int32_t)<<1)) {
		*(int32_t *)indexPtr = (int32_t)index;
		return TCL_OK;
	    }
	}
	*(int *)indexPtr = (int)index;
    }
    return TCL_OK;

  error:
    if (interp != NULL) {
	/*
	 * Produce a fancy error message.
	 */

	int count = 0;

	TclNewObj(resultPtr);
	entryPtr = (const char *const *)tablePtr;
	while ((*entryPtr != NULL) && !**entryPtr) {
	    entryPtr = NEXT_ENTRY(entryPtr, offset);
	}
	Tcl_AppendStringsToObj(resultPtr,
		(numAbbrev>1 && !(flags & TCL_EXACT) ? "ambiguous " : "bad "),
		msg, " \"", key, (char *)NULL);
	if (*entryPtr == NULL) {
	    Tcl_AppendStringsToObj(resultPtr, "\": no valid options", (char *)NULL);
	} else {
	    Tcl_AppendStringsToObj(resultPtr, "\": must be ",
		    *entryPtr, (char *)NULL);
	    entryPtr = NEXT_ENTRY(entryPtr, offset);
	    while (*entryPtr != NULL) {
		if ((*NEXT_ENTRY(entryPtr, offset) == NULL) && !(flags & TCL_NULL_OK)) {
		    Tcl_AppendStringsToObj(resultPtr, (count > 0 ? "," : ""),
			    " or ", *entryPtr, (char *)NULL);
		} else if (**entryPtr) {
		    Tcl_AppendStringsToObj(resultPtr, ", ", *entryPtr, (char *)NULL);
		    count++;
		}
		entryPtr = NEXT_ENTRY(entryPtr, offset);
	    }
	    if ((flags & TCL_NULL_OK)) {
		Tcl_AppendStringsToObj(resultPtr, ", or \"\"", (char *)NULL);
	    }
	}
	Tcl_SetObjResult(interp, resultPtr);
	Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "INDEX", msg, key, (char *)NULL);
    }
    return TCL_ERROR;
}
/* #define again, needed below */
#define Tcl_GetIndexFromObjStruct(interp, objPtr, tablePtr, offset, msg, flags, indexPtr) \
	((Tcl_GetIndexFromObjStruct)((interp), (objPtr), (tablePtr), (offset), \
		(msg), (flags)|(int)(sizeof(*(indexPtr))<<1), (indexPtr)))

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfIndex --
 *
 *	This function is called to convert a Tcl object from index internal
 *	form to its string form. No abbreviation is ever generated.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The string representation of the object is updated.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfIndex(
    Tcl_Obj *objPtr)
{
    IndexRep *indexRep = (IndexRep *)
	    TclFetchInternalRep(objPtr, &tclIndexType)->twoPtrValue.ptr1;
    const char *indexStr = EXPAND_OF(indexRep);
    size_t len = strlen(indexStr);

    Tcl_InitStringRep(objPtr, indexStr, len);
}

/*
 *----------------------------------------------------------------------
 *
 * DupIndex --
 *
 *	This function is called to copy the internal rep of an index Tcl
 *	object from to another object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The internal representation of the target object is updated and the
 *	type is set.
 *
 *----------------------------------------------------------------------
 */

static void
DupIndex(
    Tcl_Obj *srcPtr,
    Tcl_Obj *dupPtr)
{
    Tcl_ObjInternalRep ir;
    IndexRep *dupIndexRep = (IndexRep *)Tcl_Alloc(sizeof(IndexRep));

    memcpy(dupIndexRep, TclFetchInternalRep(srcPtr, &tclIndexType)->twoPtrValue.ptr1,
	    sizeof(IndexRep));

    ir.twoPtrValue.ptr1 = dupIndexRep;
    Tcl_StoreInternalRep(dupPtr, &tclIndexType, &ir);
}

/*
 *----------------------------------------------------------------------
 *
 * FreeIndex --
 *
 *	This function is called to delete the internal rep of an index Tcl
 *	object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The internal representation of the target object is deleted.
 *
 *----------------------------------------------------------------------
 */

static void
FreeIndex(
    Tcl_Obj *objPtr)
{
    Tcl_Free(TclFetchInternalRep(objPtr, &tclIndexType)->twoPtrValue.ptr1);
    objPtr->typePtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitPrefixCmd --
 *
 *	This procedure creates the "prefix" Tcl command. See the user
 *	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
TclInitPrefixCmd(
    Tcl_Interp *interp)		/* Current interpreter. */
{
    static const EnsembleImplMap prefixImplMap[] = {
	{"all",	    PrefixAllObjCmd,	TclCompileBasic2ArgCmd, NULL, NULL, 0},
	{"longest", PrefixLongestObjCmd,TclCompileBasic2ArgCmd, NULL, NULL, 0},
	{"match",   PrefixMatchObjCmd,	TclCompileBasicMin2ArgCmd, NULL, NULL, 0},
	{NULL, NULL, NULL, NULL, NULL, 0}
    };
    Tcl_Command prefixCmd;

    prefixCmd = TclMakeEnsemble(interp, "::tcl::prefix", prefixImplMap);
    Tcl_Export(interp, Tcl_FindNamespace(interp, "::tcl", NULL, 0),
	    "prefix", 0);
    return prefixCmd;
}

/*----------------------------------------------------------------------
 *
 * PrefixMatchObjCmd --
 *
 *	This function implements the 'prefix match' Tcl command. Refer to the
 *	user documentation for details on what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
PrefixMatchObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int flags = 0, result;
    Tcl_Size errorLength, i;
    Tcl_Obj *errorPtr = NULL;
    const char *message = "option";
    Tcl_Obj *tablePtr, *objPtr, *resultPtr;
    static const char *const matchOptions[] = {
	"-error", "-exact", "-message", NULL
    };
    enum matchOptionsEnum {
	PRFMATCH_ERROR, PRFMATCH_EXACT, PRFMATCH_MESSAGE
    } index;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "?options? table string");
	return TCL_ERROR;
    }

    for (i = 1; i < (objc - 2); i++) {
	if (Tcl_GetIndexFromObjStruct(interp, objv[i], matchOptions,
		sizeof(char *), "option", 0, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (index) {
	case PRFMATCH_EXACT:
	    flags |= TCL_EXACT;
	    break;
	case PRFMATCH_MESSAGE:
	    if (i > objc-4) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"missing value for -message", TCL_INDEX_NONE));
		Tcl_SetErrorCode(interp, "TCL", "OPERATION", "NOARG", (char *)NULL);
		return TCL_ERROR;
	    }
	    i++;
	    message = TclGetString(objv[i]);
	    break;
	case PRFMATCH_ERROR:
	    if (i > objc-4) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"missing value for -error", TCL_INDEX_NONE));
		Tcl_SetErrorCode(interp, "TCL", "OPERATION", "NOARG", (char *)NULL);
		return TCL_ERROR;
	    }
	    i++;
	    result = TclListObjLength(interp, objv[i], &errorLength);
	    if (result != TCL_OK) {
		return TCL_ERROR;
	    }
	    if ((errorLength % 2) != 0) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"error options must have an even number of elements",
			-1));
		Tcl_SetErrorCode(interp, "TCL", "VALUE", "DICTIONARY", (char *)NULL);
		return TCL_ERROR;
	    }
	    errorPtr = objv[i];
	    break;
	}
    }

    tablePtr = objv[objc - 2];
    objPtr = objv[objc - 1];

    /*
     * Check that table is a valid list first, since we want to handle that
     * error case regardless of level.
     */

    result = TclListObjLength(interp, tablePtr, &i);
    if (result != TCL_OK) {
	return result;
    }

    result = GetIndexFromObjList(interp, objPtr, tablePtr, message, flags,
	    &i);
    if (result != TCL_OK) {
	if (errorPtr != NULL && errorLength == 0) {
	    Tcl_ResetResult(interp);
	    return TCL_OK;
	} else if (errorPtr == NULL) {
	    return TCL_ERROR;
	}

	if (Tcl_IsShared(errorPtr)) {
	    errorPtr = Tcl_DuplicateObj(errorPtr);
	}
	Tcl_ListObjAppendElement(interp, errorPtr,
		Tcl_NewStringObj("-code", 5));
	Tcl_ListObjAppendElement(interp, errorPtr, Tcl_NewWideIntObj(result));

	return Tcl_SetReturnOptions(interp, errorPtr);
    }

    result = Tcl_ListObjIndex(interp, tablePtr, i, &resultPtr);
    if (result != TCL_OK) {
	return result;
    }
    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * PrefixAllObjCmd --
 *
 *	This function implements the 'prefix all' Tcl command. Refer to the
 *	user documentation for details on what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
PrefixAllObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int result;
    Tcl_Size length, elemLength, tableObjc, t;
    const char *string, *elemString;
    Tcl_Obj **tableObjv, *resultPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "table string");
	return TCL_ERROR;
    }

    result = TclListObjGetElements(interp, objv[1], &tableObjc, &tableObjv);
    if (result != TCL_OK) {
	return result;
    }
    resultPtr = Tcl_NewListObj(0, NULL);
    string = TclGetStringFromObj(objv[2], &length);

    for (t = 0; t < tableObjc; t++) {
	elemString = TclGetStringFromObj(tableObjv[t], &elemLength);

	/*
	 * A prefix cannot match if it is longest.
	 */

	if (length <= elemLength) {
	    if (TclpUtfNcmp2(elemString, string, length) == 0) {
		Tcl_ListObjAppendElement(interp, resultPtr, tableObjv[t]);
	    }
	}
    }

    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
}

/*----------------------------------------------------------------------
 *
 * PrefixLongestObjCmd --
 *
 *	This function implements the 'prefix longest' Tcl command. Refer to
 *	the user documentation for details on what it does.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
PrefixLongestObjCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int result;
    Tcl_Size i, length, elemLength, resultLength, tableObjc, t;
    const char *string, *elemString, *resultString;
    Tcl_Obj **tableObjv;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "table string");
	return TCL_ERROR;
    }

    result = TclListObjGetElements(interp, objv[1], &tableObjc, &tableObjv);
    if (result != TCL_OK) {
	return result;
    }
    string = TclGetStringFromObj(objv[2], &length);

    resultString = NULL;
    resultLength = 0;

    for (t = 0; t < tableObjc; t++) {
	elemString = TclGetStringFromObj(tableObjv[t], &elemLength);

	/*
	 * First check if the prefix string matches the element. A prefix
	 * cannot match if it is longest.
	 */

	if ((length > elemLength) ||
		TclpUtfNcmp2(elemString, string, length) != 0) {
	    continue;
	}

	if (resultString == NULL) {
	    /*
	     * If this is the first match, the longest common substring this
	     * far is the complete string. The result is part of this string
	     * so we only need to adjust the length later.
	     */

	    resultString = elemString;
	    resultLength = elemLength;
	} else {
	    /*
	     * Longest common substring cannot be longer than shortest string.
	     */

	    if (elemLength < resultLength) {
		resultLength = elemLength;
	    }

	    /*
	     * Compare strings.
	     */

	    for (i = 0; i < resultLength; i++) {
		if (resultString[i] != elemString[i]) {
		    /*
		     * Adjust in case we stopped in the middle of a UTF char.
		     */

		    resultLength = Tcl_UtfPrev(&resultString[i+1],
			    resultString) - resultString;
		    break;
		}
	    }
	}
    }
    if (resultLength > 0) {
	Tcl_SetObjResult(interp,
		Tcl_NewStringObj(resultString, resultLength));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WrongNumArgs --
 *
 *	This function generates a "wrong # args" error message in an
 *	interpreter. It is used as a utility function by many command
 *	functions, including the function that implements procedures.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An error message is generated in interp's result object to indicate
 *	that a command was invoked with the wrong number of arguments. The
 *	message has the form
 *		wrong # args: should be "foo bar additional stuff"
 *	where "foo" and "bar" are the initial objects in objv (objc determines
 *	how many of these are printed) and "additional stuff" is the contents
 *	of the message argument.
 *
 *	The message printed is modified somewhat if the command is wrapped
 *	inside an ensemble. In that case, the error message generated is
 *	rewritten in such a way that it appears to be generated from the
 *	user-visible command and not how that command is actually implemented,
 *	giving a better overall user experience.
 *
 *	Internally, the Tcl core may set the flag INTERP_ALTERNATE_WRONG_ARGS
 *	in the interpreter to generate complex multi-part messages by calling
 *	this function repeatedly. This allows the code that knows how to
 *	handle ensemble-related error messages to be kept here while still
 *	generating suitable error messages for commands like [read] and
 *	[socket]. Ideally, this would be done through an extra flags argument,
 *	but that wouldn't be source-compatible with the existing API and it's
 *	a fairly rare requirement anyway.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_WrongNumArgs(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Size objc,		/* Number of arguments to print from objv. */
    Tcl_Obj *const objv[],	/* Initial argument objects, which should be
				 * included in the error message. */
    const char *message)	/* Error message to print after the leading
				 * objects in objv. The message may be
				 * NULL. */
{
    Tcl_Obj *objPtr;
    Tcl_Size i, len, elemLen;
    char flags;
    Interp *iPtr = (Interp *)interp;
    const char *elementStr;

    TclNewObj(objPtr);
    if (iPtr->flags & INTERP_ALTERNATE_WRONG_ARGS) {
	iPtr->flags &= ~INTERP_ALTERNATE_WRONG_ARGS;
	Tcl_AppendObjToObj(objPtr, Tcl_GetObjResult(interp));
	Tcl_AppendToObj(objPtr, " or \"", TCL_INDEX_NONE);
    } else {
	Tcl_AppendToObj(objPtr, "wrong # args: should be \"", TCL_INDEX_NONE);
    }

    /*
     * If processing an ensemble implementation, rewrite the results in
     * terms of how the ensemble was invoked.
     */

    if (iPtr->ensembleRewrite.sourceObjs != NULL) {
	Tcl_Size toSkip = iPtr->ensembleRewrite.numInsertedObjs;
	Tcl_Size toPrint = iPtr->ensembleRewrite.numRemovedObjs;
	Tcl_Obj *const *origObjv = TclEnsembleGetRewriteValues(interp);

	/*
	 * Only do rewrite the command if all the replaced objects are
	 * actually arguments (in objv) to this function. Otherwise it just
	 * gets too complicated and it's to just give a slightly
	 * confusing error message...
	 */

	if (objc < toSkip) {
	    goto addNormalArgumentsToMessage;
	}

	/*
	 * Strip out the actual arguments that the ensemble inserted.
	 */

	objv += toSkip;
	objc -= toSkip;

	/*
	 * Assume no object is of index type.
	 */

	for (i=0 ; i<toPrint ; i++) {
	    /*
	     * Add the element, quoting it if necessary.
	     */
	    const Tcl_ObjInternalRep *irPtr;

	    if ((irPtr = TclFetchInternalRep(origObjv[i], &tclIndexType))) {
		IndexRep *indexRep = (IndexRep *)irPtr->twoPtrValue.ptr1;

		elementStr = EXPAND_OF(indexRep);
		elemLen = strlen(elementStr);
	    } else {
		elementStr = TclGetStringFromObj(origObjv[i], &elemLen);
	    }
	    flags = 0;
	    len = TclScanElement(elementStr, elemLen, &flags);

	    if (len != elemLen) {
		char *quotedElementStr = (char *)TclStackAlloc(interp, len + 1);

		len = TclConvertElement(elementStr, elemLen,
			quotedElementStr, flags);
		Tcl_AppendToObj(objPtr, quotedElementStr, len);
		TclStackFree(interp, quotedElementStr);
	    } else {
		Tcl_AppendToObj(objPtr, elementStr, elemLen);
	    }

	    /*
	     * Add a space if the word is not the last one (which has a
	     * moderately complex condition here).
	     */

	    if (i + 1 < toPrint || objc!=0 || message!=NULL) {
		Tcl_AppendStringsToObj(objPtr, " ", (char *)NULL);
	    }
	}
    }

    /*
     * Now add the arguments (other than those rewritten) that the caller took
     * from its calling context.
     */

  addNormalArgumentsToMessage:
    for (i = 0; i < objc; i++) {
	/*
	 * If the object is an index type, use the index table which allows for
	 * the correct error message even if the subcommand was abbreviated.
	 * Otherwise, just use the string rep.
	 */
	const Tcl_ObjInternalRep *irPtr;

	if ((irPtr = TclFetchInternalRep(objv[i], &tclIndexType))) {
	    IndexRep *indexRep = (IndexRep *)irPtr->twoPtrValue.ptr1;

	    Tcl_AppendStringsToObj(objPtr, EXPAND_OF(indexRep), (char *)NULL);
	} else {
	    /*
	     * Quote the argument if it contains spaces (Bug 942757).
	     */

	    elementStr = TclGetStringFromObj(objv[i], &elemLen);
	    flags = 0;
	    len = TclScanElement(elementStr, elemLen, &flags);

	    if (len != elemLen) {
		char *quotedElementStr = (char *)TclStackAlloc(interp, len + 1);

		len = TclConvertElement(elementStr, elemLen,
			quotedElementStr, flags);
		Tcl_AppendToObj(objPtr, quotedElementStr, len);
		TclStackFree(interp, quotedElementStr);
	    } else {
		Tcl_AppendToObj(objPtr, elementStr, elemLen);
	    }
	}

	/*
	 * Append a space character (" ") if there is more text to follow
	 * (either another element from objv, or the message string).
	 */

	if (i + 1 < objc || message!=NULL) {
	    Tcl_AppendStringsToObj(objPtr, " ", (char *)NULL);
	}
    }

    /*
     * Add any trailing message bits and set the resulting string as the
     * interpreter result. Caller is responsible for reporting this as an
     * actual error.
     */

    if (message != NULL) {
	Tcl_AppendStringsToObj(objPtr, message, (char *)NULL);
    }
    Tcl_AppendStringsToObj(objPtr, "\"", (char *)NULL);
    Tcl_SetErrorCode(interp, "TCL", "WRONGARGS", (char *)NULL);
    Tcl_SetObjResult(interp, objPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ParseArgsObjv --
 *
 *	Process an objv array according to a table of expected command-line
 *	options. See the manual page for more details.
 *
 * Results:
 *	The return value is a standard Tcl return value. If an error occurs
 *	then an error message is left in the interp's result. Under normal
 *	conditions, both *objcPtr and *objv are modified to return the
 *	arguments that couldn't be processed here (they didn't match the
 *	option table, or followed an TCL_ARGV_REST argument).
 *
 * Side effects:
 *	Variables may be modified, or procedures may be called. It all depends
 *	on the arguments and their entries in argTable. See the user
 *	documentation for details.
 *
 *----------------------------------------------------------------------
 */

#undef Tcl_ParseArgsObjv
int
Tcl_ParseArgsObjv(
    Tcl_Interp *interp,		/* Place to store error message. */
    const Tcl_ArgvInfo *argTable,
				/* Array of option descriptions. */
    Tcl_Size *objcPtr,		/* Number of arguments in objv. Modified to
				 * hold # args left in objv at end. */
    Tcl_Obj *const *objv,	/* Array of arguments to be parsed. */
    Tcl_Obj ***remObjv)		/* Pointer to array of arguments that were not
				 * processed here. Should be NULL if no return
				 * of arguments is desired. */
{
    Tcl_Obj **leftovers;	/* Array to write back to remObjv on
				 * successful exit. Will include the name of
				 * the command. */
    Tcl_Size nrem;		/* Size of leftovers.*/
    const Tcl_ArgvInfo *infoPtr;
				/* Pointer to the current entry in the table
				 * of argument descriptions. */
    const Tcl_ArgvInfo *matchPtr;
				/* Descriptor that matches current argument */
    Tcl_Obj *curArg;		/* Current argument */
    const char *str = NULL;
    char c;			/* Second character of current arg (used for
				 * quick check for matching; use 2nd char.
				 * because first char. will almost always be
				 * '-'). */
    Tcl_Size srcIndex;		/* Location from which to read next argument
				 * from objv. */
    Tcl_Size dstIndex;		/* Used to keep track of current arguments
				 * being processed, primarily for error
				 * reporting. */
    Tcl_Size objc;		/* # arguments in objv still to process. */
    Tcl_Size length;		/* Number of characters in current argument */
    Tcl_Size gf_ret;		/* Return value from Tcl_ArgvGenFuncProc*/

    if (remObjv != NULL) {
	/*
	 * Then we should copy the name of the command (0th argument). The
	 * upper bound on the number of elements is known, and (undocumented,
	 * but historically true) there should be a NULL argument after the
	 * last result. [Bug 3413857]
	 */

	nrem = 1;
	leftovers = (Tcl_Obj **)Tcl_Alloc((1 + *objcPtr) * sizeof(Tcl_Obj *));
	leftovers[0] = objv[0];
    } else {
	nrem = 0;
	leftovers = NULL;
    }

    /*
     * OK, now start processing from the second element (1st argument).
     */

    srcIndex = dstIndex = 1;
    objc = *objcPtr-1;

    while (objc > 0) {
	curArg = objv[srcIndex];
	srcIndex++;
	objc--;
	str = TclGetStringFromObj(curArg, &length);
	if (length > 0) {
	    c = str[1];
	} else {
	    c = 0;
	}

	/*
	 * Loop through the argument descriptors searching for one with the
	 * matching key string. If found, leave a pointer to it in matchPtr.
	 */

	matchPtr = NULL;
	infoPtr = argTable;
	for (; infoPtr != NULL && infoPtr->type != TCL_ARGV_END ; infoPtr++) {
	    if (infoPtr->keyStr == NULL) {
		continue;
	    }
	    if ((infoPtr->keyStr[1] != c)
		    || (strncmp(infoPtr->keyStr, str, length) != 0)) {
		continue;
	    }
	    if (infoPtr->keyStr[length] == 0) {
		matchPtr = infoPtr;
		goto gotMatch;
	    }
	    if (matchPtr != NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"ambiguous option \"%s\"", str));
		goto error;
	    }
	    matchPtr = infoPtr;
	}
	if (matchPtr == NULL) {
	    /*
	     * Unrecognized argument. Just copy it down, unless the caller
	     * prefers an error to be registered.
	     */

	    if (remObjv == NULL) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"unrecognized argument \"%s\"", str));
		goto error;
	    }

	    dstIndex++;		/* This argument is now handled */
	    leftovers[nrem++] = curArg;
	    continue;
	}

	/*
	 * Take the appropriate action based on the option type
	 */

    gotMatch:
	infoPtr = matchPtr;
	switch (infoPtr->type) {
	case TCL_ARGV_CONSTANT:
	    *((int *)infoPtr->dstPtr) = (int)PTR2INT(infoPtr->srcPtr);
	    break;
	case TCL_ARGV_INT:
	    if (objc == 0) {
		goto missingArg;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[srcIndex],
		    (int *) infoPtr->dstPtr) == TCL_ERROR) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"expected integer argument for \"%s\" but got \"%s\"",
			infoPtr->keyStr, TclGetString(objv[srcIndex])));
		goto error;
	    }
	    srcIndex++;
	    objc--;
	    break;
	case TCL_ARGV_STRING:
	    if (objc == 0) {
		goto missingArg;
	    }
	    *((const char **) infoPtr->dstPtr) =
		    TclGetString(objv[srcIndex]);
	    srcIndex++;
	    objc--;
	    break;
	case TCL_ARGV_REST:
	    /*
	     * Only store the point where we got to if it's not to be written
	     * to NULL, so that TCL_ARGV_AUTO_REST works.
	     */

	    if (infoPtr->dstPtr != NULL) {
		*((int *)infoPtr->dstPtr) = (int)dstIndex;
	    }
	    goto argsDone;
	case TCL_ARGV_FLOAT:
	    if (objc == 0) {
		goto missingArg;
	    }
	    if (Tcl_GetDoubleFromObj(interp, objv[srcIndex],
		    (double *)infoPtr->dstPtr) == TCL_ERROR) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"expected floating-point argument for \"%s\" but got \"%s\"",
			infoPtr->keyStr, TclGetString(objv[srcIndex])));
		goto error;
	    }
	    srcIndex++;
	    objc--;
	    break;
	case TCL_ARGV_FUNC: {
	    Tcl_ArgvFuncProc *handlerProc = (Tcl_ArgvFuncProc *)
		    infoPtr->srcPtr;
	    Tcl_Obj *argObj;

	    if (objc == 0) {
		argObj = NULL;
	    } else {
		argObj = objv[srcIndex];
	    }
	    if (handlerProc(infoPtr->clientData, argObj, infoPtr->dstPtr)) {
		srcIndex++;
		objc--;
	    }
	    break;
	}
	case TCL_ARGV_GENFUNC: {

	    if (objc > INT_MAX) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"too many (%" TCL_SIZE_MODIFIER "d) arguments for TCL_ARGV_GENFUNC", objc));
		goto error;
	    }
	    Tcl_ArgvGenFuncProc *handlerProc = (Tcl_ArgvGenFuncProc *)
		    infoPtr->srcPtr;

	    gf_ret = handlerProc(infoPtr->clientData, interp, objc,
		    &objv[srcIndex], infoPtr->dstPtr);
	    if (gf_ret < 0) {
		goto error;
	    } else {
		srcIndex += gf_ret;
		objc -= gf_ret;
	    }
	    break;
	}
	case TCL_ARGV_HELP:
	    PrintUsage(interp, argTable);
	    goto error;
	default:
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "bad argument type %d in Tcl_ArgvInfo", infoPtr->type));
	    goto error;
	}
    }

    /*
     * If we broke out of the loop because of an OPT_REST argument, copy the
     * remaining arguments down. Note that there is always at least one
     * argument left over - the command name - so we always have a result if
     * our caller is willing to receive it. [Bug 3413857]
     */

  argsDone:
    if (remObjv == NULL) {
	/*
	 * Nothing to do.
	 */

	return TCL_OK;
    }

    if (objc > 0) {
	memcpy(leftovers+nrem, objv+srcIndex, objc*sizeof(Tcl_Obj *));
	nrem += objc;
    }
    leftovers[nrem] = NULL;
    *objcPtr = nrem++;
    *remObjv = (Tcl_Obj **)Tcl_Realloc(leftovers, nrem * sizeof(Tcl_Obj *));
    return TCL_OK;

    /*
     * Make sure to handle freeing any temporary space we've allocated on the
     * way to an error.
     */

  missingArg:
    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
	    "\"%s\" option requires an additional argument", str));
  error:
    if (leftovers != NULL) {
	Tcl_Free(leftovers);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * PrintUsage --
 *
 *	Generate a help string describing command-line options.
 *
 * Results:
 *	The interp's result will be modified to hold a help string describing
 *	all the options in argTable.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
PrintUsage(
    Tcl_Interp *interp,		/* Place information in this interp's result
				 * area. */
    const Tcl_ArgvInfo *argTable)
				/* Array of command-specific argument
				 * descriptions. */
{
    const Tcl_ArgvInfo *infoPtr;
    Tcl_Size width, numSpaces;
#define NUM_SPACES 20
    static const char spaces[] = "                    ";
    Tcl_Obj *msg;

    /*
     * First, compute the width of the widest option key, so that we can make
     * everything line up.
     */

    width = 4;
    for (infoPtr = argTable; infoPtr->type != TCL_ARGV_END; infoPtr++) {
	Tcl_Size length;

	if (infoPtr->keyStr == NULL) {
	    continue;
	}
	length = strlen(infoPtr->keyStr);
	if (length > width) {
	    width = length;
	}
    }

    /*
     * Now add the option information, with pretty-printing.
     */

    msg = Tcl_NewStringObj("Command-specific options:", TCL_INDEX_NONE);
    for (infoPtr = argTable; infoPtr->type != TCL_ARGV_END; infoPtr++) {
	if ((infoPtr->type == TCL_ARGV_HELP) && (infoPtr->keyStr == NULL)) {
	    Tcl_AppendPrintfToObj(msg, "\n%s", infoPtr->helpStr);
	    continue;
	}
	Tcl_AppendPrintfToObj(msg, "\n %s:", infoPtr->keyStr);
	numSpaces = width + 1 - strlen(infoPtr->keyStr);
	while (numSpaces > 0) {
	    if (numSpaces >= NUM_SPACES) {
		Tcl_AppendToObj(msg, spaces, NUM_SPACES);
	    } else {
		Tcl_AppendToObj(msg, spaces, numSpaces);
	    }
	    numSpaces -= NUM_SPACES;
	}
	Tcl_AppendToObj(msg, infoPtr->helpStr, TCL_INDEX_NONE);
	switch (infoPtr->type) {
	case TCL_ARGV_INT:
	    Tcl_AppendPrintfToObj(msg, "\n\t\tDefault value: %d",
		    *((int *) infoPtr->dstPtr));
	    break;
	case TCL_ARGV_FLOAT:
	    Tcl_AppendPrintfToObj(msg, "\n\t\tDefault value: %g",
		    *((double *) infoPtr->dstPtr));
	    break;
	case TCL_ARGV_STRING: {
	    char *string = *((char **) infoPtr->dstPtr);

	    if (string != NULL) {
		Tcl_AppendPrintfToObj(msg, "\n\t\tDefault value: \"%s\"",
			string);
	    }
	    break;
	}
	default:
	    break;
	}
    }
    Tcl_SetObjResult(interp, msg);
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetCompletionCodeFromObj --
 *
 *	Parses Completion code Code
 *
 * Results:
 *	Returns TCL_ERROR if the value is an invalid completion code.
 *	Otherwise, returns TCL_OK, and writes the completion code to the
 *	pointer provided.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGetCompletionCodeFromObj(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *value,
    int *codePtr)		/* Argument objects. */
{
    static const char *const returnCodes[] = {
	"ok", "error", "return", "break", "continue", NULL
    };

    if (!TclHasInternalRep(value, &tclIndexType)
	    && TclGetIntFromObj(NULL, value, codePtr) == TCL_OK) {
	return TCL_OK;
    }
    if (Tcl_GetIndexFromObjStruct(NULL, value, returnCodes,
	    sizeof(char *), NULL, TCL_EXACT, codePtr) == TCL_OK) {
	return TCL_OK;
    }

    /*
     * Value is not a legal completion code.
     */

    if (interp != NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"bad completion code \"%s\": must be"
		" ok, error, return, break, continue, or an integer",
		TclGetString(value)));
	Tcl_SetErrorCode(interp, "TCL", "RESULT", "ILLEGAL_CODE", (char *)NULL);
    }
    return TCL_ERROR;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
