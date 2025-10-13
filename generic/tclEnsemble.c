/*
 * tclEnsemble.c --
 *
 *	Contains support for ensembles (see TIP#112), which provide simple
 *	mechanism for creating composite commands on top of namespaces.
 *
 * Copyright © 2005-2013 Donal K. Fellows.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"
#include "tclCompile.h"

/*
 * Declarations for functions local to this file:
 */

static Tcl_Command	InitEnsembleFromOptions(Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		ReadOneEnsembleOption(Tcl_Interp *interp,
			    Tcl_Command token, Tcl_Obj *optionObj);
static int		ReadAllEnsembleOptions(Tcl_Interp *interp,
			    Tcl_Command token);
static int		SetEnsembleConfigOptions(Tcl_Interp *interp,
			    Tcl_Command token, Tcl_Size objc,
			    Tcl_Obj *const objv[]);
static inline int	EnsembleUnknownCallback(Tcl_Interp *interp,
			    EnsembleConfig *ensemblePtr, int objc,
			    Tcl_Obj *const objv[], Tcl_Obj **prefixObjPtr);
static int		NsEnsembleImplementationCmdNR(void *clientData,
			    Tcl_Interp *interp,int objc,Tcl_Obj *const objv[]);
static void		BuildEnsembleConfig(EnsembleConfig *ensemblePtr);
static int		NsEnsembleStringOrder(const void *strPtr1,
			    const void *strPtr2);
static void		DeleteEnsembleConfig(void *clientData);
static void		MakeCachedEnsembleCommand(Tcl_Obj *objPtr,
			    EnsembleConfig *ensemblePtr, Tcl_HashEntry *hPtr,
			    Tcl_Obj *fix);
static void		FreeEnsembleCmdRep(Tcl_Obj *objPtr);
static void		DupEnsembleCmdRep(Tcl_Obj *objPtr, Tcl_Obj *copyPtr);
static void		CompileToInvokedCommand(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, Tcl_Obj *replacements,
			    Command *cmdPtr, CompileEnv *envPtr);
static int		CompileBasicNArgCommand(Tcl_Interp *interp,
			    Tcl_Parse *parsePtr, Command *cmdPtr,
			    CompileEnv *envPtr);

static Tcl_NRPostProc	FreeER;

/*
 * The lists of subcommands and options for the [namespace ensemble] command.
 */

static const char *const ensembleSubcommands[] = {
    "configure", "create", "exists", NULL
};
enum EnsSubcmds {
    ENS_CONFIG, ENS_CREATE, ENS_EXISTS
};

static const char *const ensembleCreateOptions[] = {
    "-command", "-map", "-parameters", "-prefixes", "-subcommands",
    "-unknown", NULL
};
enum EnsCreateOpts {
    CRT_CMD, CRT_MAP, CRT_PARAM, CRT_PREFIX, CRT_SUBCMDS, CRT_UNKNOWN
};

static const char *const ensembleConfigOptions[] = {
    "-map", "-namespace", "-parameters", "-prefixes", "-subcommands",
    "-unknown", NULL
};
enum EnsConfigOpts {
    CONF_MAP, CONF_NAMESPACE, CONF_PARAM, CONF_PREFIX, CONF_SUBCMDS,
    CONF_UNKNOWN
};

/*
 * ensembleCmdType is a Tcl object type that contains a reference to an
 * ensemble subcommand, e.g. the "length" in [string length ab]. It is used
 * to cache the mapping between the subcommand itself and the real command
 * that implements it.
 */

static const Tcl_ObjType ensembleCmdType = {
    "ensembleCommand",		/* the type's name */
    FreeEnsembleCmdRep,		/* freeIntRepProc */
    DupEnsembleCmdRep,		/* dupIntRepProc */
    NULL,			/* updateStringProc */
    NULL,			/* setFromAnyProc */
    TCL_OBJTYPE_V0
};

#define ECRSetInternalRep(objPtr, ecRepPtr) \
    do {								\
	Tcl_ObjInternalRep ir;						\
	ir.twoPtrValue.ptr1 = (ecRepPtr);				\
	ir.twoPtrValue.ptr2 = NULL;					\
	Tcl_StoreInternalRep((objPtr), &ensembleCmdType, &ir);		\
    } while (0)

#define ECRGetInternalRep(objPtr, ecRepPtr) \
    do {								\
	const Tcl_ObjInternalRep *irPtr;				\
	irPtr = TclFetchInternalRep((objPtr), &ensembleCmdType);	\
	(ecRepPtr) = irPtr ? (EnsembleCmdRep *)				\
		irPtr->twoPtrValue.ptr1 : NULL;				\
    } while (0)

/*
 * The internal rep for caching ensemble subcommand lookups and spelling
 * corrections.
 */

typedef struct {
    Tcl_Size epoch;		/* Used to confirm when the data in this
				 * really structure matches up with the
				 * ensemble. */
    Command *token;		/* Reference to the command for which this
				 * structure is a cache of the resolution. */
    Tcl_Obj *fix;		/* Corrected spelling, if needed. */
    Tcl_HashEntry *hPtr;	/* Direct link to entry in the subcommand hash
				 * table. */
} EnsembleCmdRep;

/*
 *----------------------------------------------------------------------
 *
 * TclNamespaceEnsembleCmd --
 *
 *	Invoked to implement the "namespace ensemble" command that creates and
 *	manipulates ensembles built on top of namespaces. Handles the
 *	following syntax:
 *
 *	    namespace ensemble name ?dictionary?
 *
 * Results:
 *	Returns TCL_OK if successful, and TCL_ERROR if anything goes wrong.
 *
 * Side effects:
 *	Creates the ensemble for the namespace if one did not previously
 *	exist. Alternatively, alters the way that the ensemble's subcommand =>
 *	implementation prefix is configured.
 *
 *----------------------------------------------------------------------
 */

int
TclNamespaceEnsembleCmd(
    TCL_UNUSED(void *),
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Namespace *nsPtr = (Namespace *) TclGetCurrentNamespace(interp);
    Tcl_Command token;		/* The ensemble command. */
    enum EnsSubcmds index;

    if (nsPtr == NULL || nsPtr->flags & NS_DEAD) {
	if (!Tcl_InterpDeleted(interp)) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "tried to manipulate ensemble of deleted namespace",
		    TCL_AUTO_LENGTH));
	    Tcl_SetErrorCode(interp, "TCL", "ENSEMBLE", "DEAD", (char *)NULL);
	}
	return TCL_ERROR;
    }

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?arg ...?");
	return TCL_ERROR;
    } else if (Tcl_GetIndexFromObj(interp, objv[1], ensembleSubcommands,
	    "subcommand", 0, &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
    case ENS_CREATE:
	/*
	 * Check that we've got option-value pairs... [Bug 1558654]
	 */

	if (objc & 1) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?option value ...?");
	    return TCL_ERROR;
	}
	token = InitEnsembleFromOptions(interp, objc - 2, objv + 2);
	if (token == NULL) {
	    return TCL_ERROR;
	}

	/*
	 * Tricky! Must ensure that the result is not shared (command delete
	 * traces could have corrupted the pristine object that we started
	 * with). [Snit test rename-1.5]
	 */

	Tcl_ResetResult(interp);
	Tcl_GetCommandFullName(interp, token, Tcl_GetObjResult(interp));
	return TCL_OK;

    case ENS_EXISTS:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "cmdname");
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(
		Tcl_FindEnsemble(interp, objv[2], 0) != NULL));
	return TCL_OK;

    case ENS_CONFIG:
	if (objc < 3 || (objc != 4 && !(objc & 1))) {
	    Tcl_WrongNumArgs(interp, 2, objv,
		    "cmdname ?-option value ...? ?arg ...?");
	    return TCL_ERROR;
	}
	token = Tcl_FindEnsemble(interp, objv[2], TCL_LEAVE_ERR_MSG);
	if (token == NULL) {
	    return TCL_ERROR;
	}

	if (objc == 4) {
	    return ReadOneEnsembleOption(interp, token, objv[3]);
	} else if (objc == 3) {
	    return ReadAllEnsembleOptions(interp, token);
	} else {
	    return SetEnsembleConfigOptions(interp, token, objc - 3, objv + 3);
	}

    default:
	TCL_UNREACHABLE();
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InitEnsembleFromOptions --
 *
 *	Core of implementation of "namespace ensemble create".
 *
 * Results:
 *	Returns created ensemble's command token if successful, and NULL if
 *	anything goes wrong.
 *
 * Side effects:
 *	Creates the ensemble for the namespace if one did not previously
 *	exist.
 *
 * Note:
 *	Can't use SetEnsembleConfigOptions() here. Different (but overlapping)
 *	options are supported.
 *
 *----------------------------------------------------------------------
 */
static Tcl_Command
InitEnsembleFromOptions(
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    Namespace *nsPtr = (Namespace *) TclGetCurrentNamespace(interp);
    Namespace *cxtPtr = nsPtr->parentPtr;
    Namespace *altFoundNsPtr, *actualCxtPtr;
    const char *name = nsPtr->name;
    Tcl_Size len;
    int allocatedMapFlag = 0;
    enum EnsCreateOpts index;
    Tcl_Command token;		/* The created ensemble command. */
    Namespace *foundNsPtr;
    const char *simpleName;
    /*
     * Defaults
     */
    Tcl_Obj *subcmdObj = NULL;
    Tcl_Obj *mapObj = NULL;
    int permitPrefix = 1;
    Tcl_Obj *unknownObj = NULL;
    Tcl_Obj *paramObj = NULL;

    /*
     * Parse the option list, applying type checks as we go. Note that we are
     * not incrementing any reference counts in the objects at this stage, so
     * the presence of an option multiple times won't cause any memory leaks.
     */

    for (; objc>1 ; objc-=2,objv+=2) {
	if (Tcl_GetIndexFromObj(interp, objv[0], ensembleCreateOptions,
		"option", 0, &index) != TCL_OK) {
	    goto error;
	}
	switch (index) {
	case CRT_CMD:
	    name = TclGetString(objv[1]);
	    cxtPtr = nsPtr;
	    continue;
	case CRT_SUBCMDS:
	    if (TclListObjLength(interp, objv[1], &len) != TCL_OK) {
		goto error;
	    }
	    subcmdObj = (len > 0 ? objv[1] : NULL);
	    continue;
	case CRT_PARAM:
	    if (TclListObjLength(interp, objv[1], &len) != TCL_OK) {
		goto error;
	    }
	    paramObj = (len > 0 ? objv[1] : NULL);
	    continue;
	case CRT_MAP: {
	    Tcl_Obj *patchedDict = NULL, *subcmdWordsObj, *listObj;
	    Tcl_DictSearch search;
	    int done;

	    /*
	     * Verify that the map is sensible.
	     */

	    if (Tcl_DictObjFirst(interp, objv[1], &search,
		    &subcmdWordsObj, &listObj, &done) != TCL_OK) {
		goto error;
	    } else if (done) {
		mapObj = NULL;
		continue;
	    }
	    do {
		Tcl_Obj **listv;
		const char *cmd;

		if (TclListObjGetElements(interp, listObj, &len,
			&listv) != TCL_OK) {
		    goto mapError;
		}
		if (len < 1) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "ensemble subcommand implementations "
			    "must be non-empty lists", TCL_AUTO_LENGTH));
		    Tcl_SetErrorCode(interp, "TCL", "ENSEMBLE",
			    "EMPTY_TARGET", (char *)NULL);
		    goto mapError;
		}
		cmd = TclGetString(listv[0]);
		if (!(cmd[0] == ':' && cmd[1] == ':')) {
		    Tcl_Obj *newList = Tcl_NewListObj(len, listv);
		    Tcl_Obj *newCmd = TclNewNamespaceObj(
			    (Tcl_Namespace *) nsPtr);

		    if (nsPtr->parentPtr) {
			Tcl_AppendStringsToObj(newCmd, "::", (char *)NULL);
		    }
		    Tcl_AppendObjToObj(newCmd, listv[0]);
		    Tcl_ListObjReplace(NULL, newList, 0, 1, 1, &newCmd);
		    if (patchedDict == NULL) {
			patchedDict = Tcl_DuplicateObj(objv[1]);
		    }
		    Tcl_DictObjPut(NULL, patchedDict, subcmdWordsObj, newList);
		}
		Tcl_DictObjNext(&search, &subcmdWordsObj, &listObj, &done);
	    } while (!done);

	    if (allocatedMapFlag) {
		Tcl_DecrRefCount(mapObj);
	    }
	    mapObj = (patchedDict ? patchedDict : objv[1]);
	    if (patchedDict) {
		allocatedMapFlag = 1;
	    }
	    continue;
	mapError:
	    Tcl_DictObjDone(&search);
	    if (patchedDict) {
		Tcl_DecrRefCount(patchedDict);
	    }
	    goto error;
	}
	case CRT_PREFIX:
	    if (Tcl_GetBooleanFromObj(interp, objv[1],
		    &permitPrefix) != TCL_OK) {
		goto error;
	    }
	    continue;
	case CRT_UNKNOWN:
	    if (TclListObjLength(interp, objv[1], &len) != TCL_OK) {
		goto error;
	    }
	    unknownObj = (len > 0 ? objv[1] : NULL);
	    continue;
	default:
	    TCL_UNREACHABLE();
	}
    }

    TclGetNamespaceForQualName(interp, name, cxtPtr,
	    TCL_CREATE_NS_IF_UNKNOWN, &foundNsPtr, &altFoundNsPtr,
	    &actualCxtPtr, &simpleName);

    /*
     * Create the ensemble. Note that this might delete another ensemble
     * linked to the same namespace, so we must be careful. However, we
     * should be OK because we only link the namespace into the list once
     * we've created it (and after any deletions have occurred.)
     */

    token = TclCreateEnsembleInNs(interp, simpleName,
	    (Tcl_Namespace *) foundNsPtr, (Tcl_Namespace *) nsPtr,
	    (permitPrefix ? TCL_ENSEMBLE_PREFIX : 0));
    Tcl_SetEnsembleSubcommandList(interp, token, subcmdObj);
    Tcl_SetEnsembleMappingDict(interp, token, mapObj);
    Tcl_SetEnsembleUnknownHandler(interp, token, unknownObj);
    Tcl_SetEnsembleParameterList(interp, token, paramObj);
    return token;

  error:
    if (allocatedMapFlag) {
	Tcl_DecrRefCount(mapObj);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ReadOneEnsembleOption --
 *
 *	Core of implementation of "namespace ensemble configure" with just a
 *	single option name.
 *
 * Results:
 *	Tcl result code. Modifies the interpreter result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static int
ReadOneEnsembleOption(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble to read from. */
    Tcl_Obj *optionObj)		/* The name of the option to read. */
{
    Tcl_Obj *resultObj = NULL;			/* silence gcc 4 warning */
    enum EnsConfigOpts index;

    if (Tcl_GetIndexFromObj(interp, optionObj, ensembleConfigOptions,
	    "option", 0, &index) != TCL_OK) {
	return TCL_ERROR;
    }
    switch (index) {
    case CONF_SUBCMDS:
	Tcl_GetEnsembleSubcommandList(NULL, token, &resultObj);
	if (resultObj != NULL) {
	    Tcl_SetObjResult(interp, resultObj);
	}
	break;
    case CONF_PARAM:
	Tcl_GetEnsembleParameterList(NULL, token, &resultObj);
	if (resultObj != NULL) {
	    Tcl_SetObjResult(interp, resultObj);
	}
	break;
    case CONF_MAP:
	Tcl_GetEnsembleMappingDict(NULL, token, &resultObj);
	if (resultObj != NULL) {
	    Tcl_SetObjResult(interp, resultObj);
	}
	break;
    case CONF_NAMESPACE: {
	Tcl_Namespace *namespacePtr = NULL;	/* silence gcc 4 warning */
	Tcl_GetEnsembleNamespace(NULL, token, &namespacePtr);
	Tcl_SetObjResult(interp, TclNewNamespaceObj(namespacePtr));
	break;
    }
    case CONF_PREFIX: {
	int flags = 0;				/* silence gcc 4 warning */

	Tcl_GetEnsembleFlags(NULL, token, &flags);
	Tcl_SetObjResult(interp,
		Tcl_NewBooleanObj(flags & TCL_ENSEMBLE_PREFIX));
	break;
    }
    case CONF_UNKNOWN:
	Tcl_GetEnsembleUnknownHandler(NULL, token, &resultObj);
	if (resultObj != NULL) {
	    Tcl_SetObjResult(interp, resultObj);
	}
	break;
    default:
	TCL_UNREACHABLE();
    }
    return TCL_OK;
}
/*
 *----------------------------------------------------------------------
 *
 * ReadAllEnsembleOptions --
 *
 *	Core of implementation of "namespace ensemble configure" without
 *	option names.
 *
 * Results:
 *	Tcl result code. Modifies the interpreter result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static int
ReadAllEnsembleOptions(
    Tcl_Interp *interp,
    Tcl_Command token)		/* The ensemble to read from. */
{
    Tcl_Obj *resultObj, *tmpObj = NULL;	/* silence gcc 4 warning */
    int flags = 0;			/* silence gcc 4 warning */
    Tcl_Namespace *namespacePtr = NULL;	/* silence gcc 4 warning */

    TclNewObj(resultObj);

    /* -map option */
    Tcl_ListObjAppendElement(NULL, resultObj,
	    Tcl_NewStringObj(ensembleConfigOptions[CONF_MAP],
		    TCL_AUTO_LENGTH));
    Tcl_GetEnsembleMappingDict(NULL, token, &tmpObj);
    Tcl_ListObjAppendElement(NULL, resultObj,
	    (tmpObj != NULL) ? tmpObj : Tcl_NewObj());

    /* -namespace option */
    Tcl_ListObjAppendElement(NULL, resultObj,
	    Tcl_NewStringObj(ensembleConfigOptions[CONF_NAMESPACE],
		    TCL_AUTO_LENGTH));
    Tcl_GetEnsembleNamespace(NULL, token, &namespacePtr);
    Tcl_ListObjAppendElement(NULL, resultObj, TclNewNamespaceObj(namespacePtr));

    /* -parameters option */
    Tcl_ListObjAppendElement(NULL, resultObj,
	    Tcl_NewStringObj(ensembleConfigOptions[CONF_PARAM],
		    TCL_AUTO_LENGTH));
    Tcl_GetEnsembleParameterList(NULL, token, &tmpObj);
    Tcl_ListObjAppendElement(NULL, resultObj,
	    (tmpObj != NULL) ? tmpObj : Tcl_NewObj());

    /* -prefix option */
    Tcl_ListObjAppendElement(NULL, resultObj,
	    Tcl_NewStringObj(ensembleConfigOptions[CONF_PREFIX],
		    TCL_AUTO_LENGTH));
    Tcl_GetEnsembleFlags(NULL, token, &flags);
    Tcl_ListObjAppendElement(NULL, resultObj,
	    Tcl_NewBooleanObj(flags & TCL_ENSEMBLE_PREFIX));

    /* -subcommands option */
    Tcl_ListObjAppendElement(NULL, resultObj,
	    Tcl_NewStringObj(ensembleConfigOptions[CONF_SUBCMDS],
		    TCL_AUTO_LENGTH));
    Tcl_GetEnsembleSubcommandList(NULL, token, &tmpObj);
    Tcl_ListObjAppendElement(NULL, resultObj,
	    (tmpObj != NULL) ? tmpObj : Tcl_NewObj());

    /* -unknown option */
    Tcl_ListObjAppendElement(NULL, resultObj,
	    Tcl_NewStringObj(ensembleConfigOptions[CONF_UNKNOWN],
		    TCL_AUTO_LENGTH));
    Tcl_GetEnsembleUnknownHandler(NULL, token, &tmpObj);
    Tcl_ListObjAppendElement(NULL, resultObj,
	    (tmpObj != NULL) ? tmpObj : Tcl_NewObj());

    Tcl_SetObjResult(interp, resultObj);
    return TCL_OK;
}
/*
 *----------------------------------------------------------------------
 *
 * SetEnsembleConfigOptions --
 *
 *	Core of implementation of "namespace ensemble configure" with even
 *	number of arguments (where there is at least one pair).
 *
 * Results:
 *	Tcl result code. Modifies the interpreter result.
 *
 * Side effects:
 *	Modifies the ensemble's configuration.
 *
 *----------------------------------------------------------------------
 */
static int
SetEnsembleConfigOptions(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble to configure. */
    Tcl_Size objc,			/* The count of option-related arguments. */
    Tcl_Obj *const objv[])	/* Option-related arguments. */
{
    Tcl_Size len;
    int allocatedMapFlag = 0;
    Tcl_Obj *subcmdObj = NULL, *mapObj = NULL, *paramObj = NULL,
	    *unknownObj = NULL;	/* Defaults, silence gcc 4 warnings */
    Tcl_Obj *listObj;
    Tcl_DictSearch search;
    int permitPrefix, flags = 0;	/* silence gcc 4 warning */
    enum EnsConfigOpts index;
    int done;

    Tcl_GetEnsembleSubcommandList(NULL, token, &subcmdObj);
    Tcl_GetEnsembleMappingDict(NULL, token, &mapObj);
    Tcl_GetEnsembleParameterList(NULL, token, &paramObj);
    Tcl_GetEnsembleUnknownHandler(NULL, token, &unknownObj);
    Tcl_GetEnsembleFlags(NULL, token, &flags);
    permitPrefix = (flags & TCL_ENSEMBLE_PREFIX) != 0;

    /*
     * Parse the option list, applying type checks as we go. Note that
     * we are not incrementing any reference counts in the objects at
     * this stage, so the presence of an option multiple times won't
     * cause any memory leaks.
     */

    for (; objc>0 ; objc-=2,objv+=2) {
	if (Tcl_GetIndexFromObj(interp, objv[0], ensembleConfigOptions,
		"option", 0, &index) != TCL_OK) {
	    goto freeMapAndError;
	}
	switch (index) {
	case CONF_SUBCMDS:
	    if (TclListObjLength(interp, objv[1], &len) != TCL_OK) {
		goto freeMapAndError;
	    }
	    subcmdObj = (len > 0 ? objv[1] : NULL);
	    continue;
	case CONF_PARAM:
	    if (TclListObjLength(interp, objv[1], &len) != TCL_OK) {
		goto freeMapAndError;
	    }
	    paramObj = (len > 0 ? objv[1] : NULL);
	    continue;
	case CONF_MAP: {
	    Tcl_Obj *patchedDict = NULL, *subcmdWordsObj, **listv;
	    Namespace *nsPtr = (Namespace *) TclGetCurrentNamespace(interp);
	    const char *cmd;

	    /*
	     * Verify that the map is sensible.
	     */

	    if (Tcl_DictObjFirst(interp, objv[1], &search,
		    &subcmdWordsObj, &listObj, &done) != TCL_OK) {
		goto freeMapAndError;
	    } else if (done) {
		mapObj = NULL;
		continue;
	    }

	    do {
		if (TclListObjLength(interp, listObj, &len) != TCL_OK) {
		    goto finishSearchAndError;
		}
		if (len < 1) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "ensemble subcommand implementations "
			    "must be non-empty lists", TCL_AUTO_LENGTH));
		    Tcl_SetErrorCode(interp, "TCL", "ENSEMBLE",
			    "EMPTY_TARGET", (char *)NULL);
		    goto finishSearchAndError;
		}
		if (TclListObjGetElements(interp, listObj, &len,
			&listv) != TCL_OK) {
		    goto finishSearchAndError;
		}
		cmd = TclGetString(listv[0]);
		if (!(cmd[0] == ':' && cmd[1] == ':')) {
		    Tcl_Obj *newList = Tcl_DuplicateObj(listObj);
		    Tcl_Obj *newCmd = TclNewNamespaceObj(
			    (Tcl_Namespace*) nsPtr);

		    if (nsPtr->parentPtr) {
			Tcl_AppendStringsToObj(newCmd, "::", (char *)NULL);
		    }
		    Tcl_AppendObjToObj(newCmd, listv[0]);
		    Tcl_ListObjReplace(NULL, newList, 0, 1, 1, &newCmd);
		    if (patchedDict == NULL) {
			patchedDict = Tcl_DuplicateObj(objv[1]);
		    }
		    Tcl_DictObjPut(NULL, patchedDict, subcmdWordsObj, newList);
		}
		Tcl_DictObjNext(&search, &subcmdWordsObj, &listObj, &done);
	    } while (!done);
	    if (allocatedMapFlag) {
		Tcl_DecrRefCount(mapObj);
	    }
	    mapObj = (patchedDict ? patchedDict : objv[1]);
	    if (patchedDict) {
		allocatedMapFlag = 1;
	    }
	    continue;

	finishSearchAndError:
	    Tcl_DictObjDone(&search);
	    if (patchedDict) {
		Tcl_DecrRefCount(patchedDict);
	    }
	    goto freeMapAndError;
	}
	case CONF_NAMESPACE:
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "option -namespace is read-only", TCL_AUTO_LENGTH));
	    Tcl_SetErrorCode(interp, "TCL", "ENSEMBLE", "READ_ONLY",
		    (char *)NULL);
	    goto freeMapAndError;
	case CONF_PREFIX:
	    if (Tcl_GetBooleanFromObj(interp, objv[1],
		    &permitPrefix) != TCL_OK) {
		goto freeMapAndError;
	    }
	    continue;
	case CONF_UNKNOWN:
	    if (TclListObjLength(interp, objv[1], &len) != TCL_OK) {
		goto freeMapAndError;
	    }
	    unknownObj = (len > 0 ? objv[1] : NULL);
	    continue;
	default:
	    TCL_UNREACHABLE();
	}
    }

    /*
     * Update the namespace now that we've finished the parsing stage.
     */

    flags = (permitPrefix ? flags | TCL_ENSEMBLE_PREFIX
	    : flags & ~TCL_ENSEMBLE_PREFIX);
    Tcl_SetEnsembleSubcommandList(interp, token, subcmdObj);
    Tcl_SetEnsembleMappingDict(interp, token, mapObj);
    Tcl_SetEnsembleParameterList(interp, token, paramObj);
    Tcl_SetEnsembleUnknownHandler(interp, token, unknownObj);
    Tcl_SetEnsembleFlags(interp, token, flags);
    return TCL_OK;

  freeMapAndError:
    if (allocatedMapFlag) {
	Tcl_DecrRefCount(mapObj);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateEnsembleInNs --
 *
 *	Like Tcl_CreateEnsemble, but additionally accepts as an argument the
 *	name of the namespace to create the command in.
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
TclCreateEnsembleInNs(
    Tcl_Interp *interp,
    const char *name,		/* Simple name of command to create (no
				 * namespace components). */
    Tcl_Namespace *nameNsPtr,	/* Name of namespace to create the command
				 * in. */
    Tcl_Namespace *ensembleNsPtr,
				/* Name of the namespace for the ensemble. */
    int flags)			/* Whether we need exact matching and whether
				 * we bytecode-compile the ensemble's uses. */
{
    Namespace *nsPtr = (Namespace *) ensembleNsPtr;
    EnsembleConfig *ensemblePtr;
    Tcl_Command token;

    ensemblePtr = (EnsembleConfig *) Tcl_Alloc(sizeof(EnsembleConfig));
    token = TclNRCreateCommandInNs(interp, name,
	    (Tcl_Namespace *) nameNsPtr, TclEnsembleImplementationCmd,
	    NsEnsembleImplementationCmdNR, ensemblePtr, DeleteEnsembleConfig);
    if (token == NULL) {
	Tcl_Free(ensemblePtr);
	return NULL;
    }

    ensemblePtr->nsPtr = nsPtr;
    ensemblePtr->epoch = 0;
    Tcl_InitHashTable(&ensemblePtr->subcommandTable, TCL_STRING_KEYS);
    ensemblePtr->subcommandArrayPtr = NULL;
    ensemblePtr->subcmdList = NULL;
    ensemblePtr->subcommandDict = NULL;
    ensemblePtr->flags = flags;
    ensemblePtr->numParameters = 0;
    ensemblePtr->parameterList = NULL;
    ensemblePtr->unknownHandler = NULL;
    ensemblePtr->token = token;
    ensemblePtr->next = (EnsembleConfig *) nsPtr->ensembles;
    nsPtr->ensembles = (Tcl_Ensemble *) ensemblePtr;

    /*
     * Trigger an eventual recomputation of the ensemble command set. Note
     * that this is slightly tricky, as it means that we are not actually
     * counting the number of namespace export actions, but it is the simplest
     * way to go!
     */

    nsPtr->exportLookupEpoch++;

    if (flags & ENSEMBLE_COMPILE) {
	((Command *) ensemblePtr->token)->compileProc = TclCompileEnsemble;
    }

    return ensemblePtr->token;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateEnsemble
 *
 *	Create a simple ensemble attached to the given namespace. Deprecated
 *	(internally) by TclCreateEnsembleInNs.
 *
 * Value
 *
 *	The token for the command created.
 *
 * Effect
 *	The ensemble is created and marked for compilation.
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
Tcl_CreateEnsemble(
    Tcl_Interp *interp,
    const char *name,		/* The ensemble name. */
    Tcl_Namespace *namespacePtr,/* Context namespace. */
    int flags)			/* Whether we need exact matching and whether
				 * we bytecode-compile the ensemble's uses. */
{
    Namespace *nsPtr = (Namespace *) namespacePtr, *foundNsPtr, *altNsPtr,
	    *actualNsPtr;
    const char * simpleName;

    if (nsPtr == NULL) {
	nsPtr = (Namespace *) TclGetCurrentNamespace(interp);
    }

    TclGetNamespaceForQualName(interp, name, nsPtr, TCL_CREATE_NS_IF_UNKNOWN,
	    &foundNsPtr, &altNsPtr, &actualNsPtr, &simpleName);
    return TclCreateEnsembleInNs(interp, simpleName,
	    (Tcl_Namespace *) foundNsPtr, (Tcl_Namespace *) nsPtr, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * GetEnsembleFromCommand --
 *
 *	Standard check to see if a command is an ensemble.
 *
 * Results:
 *	The ensemble implementation if the command is an ensemble. NULL if it
 *	isn't.
 *
 * Side effects:
 *	Reports an error in the interpreter (if non-NULL) if the command is
 *	not an ensemble.
 *
 *----------------------------------------------------------------------
 */
static inline EnsembleConfig *
GetEnsembleFromCommand(
    Tcl_Interp *interp,		/* Where to report an error. May be NULL. */
    Tcl_Command token)		/* What to check for ensemble-ness. */
{
    Command *cmdPtr = (Command *) token;

    if (cmdPtr->objProc != TclEnsembleImplementationCmd) {
	if (interp != NULL) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "command is not an ensemble", TCL_AUTO_LENGTH));
	    Tcl_SetErrorCode(interp,
		    "TCL", "ENSEMBLE", "NOT_ENSEMBLE", (char *)NULL);
	}
	return NULL;
    }
    return (EnsembleConfig *) cmdPtr->objClientData;
}

/*
 *----------------------------------------------------------------------
 *
 * BumpEpochIfNecessary --
 *
 *	Increments the compilation epoch if the (ensemble) command is one where
 *	changes would be seen by the compiler in some cases.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May trigger later bytecode recompilations.
 *
 *----------------------------------------------------------------------
 */
static inline void
BumpEpochIfNecessary(
    Tcl_Interp *interp,
    Tcl_Command token)		/* The ensemble command to check. */
{
    /*
     * Special hack to make compiling of [info exists] work when the
     * dictionary is modified.
     */

    if (((Command *) token)->compileProc != NULL) {
	((Interp *) interp)->compileEpoch++;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetEnsembleSubcommandList --
 *
 *	Set the subcommand list for a particular ensemble.
 *
 * Results:
 *	Tcl result code (error if command token does not indicate an ensemble
 *	or the subcommand list - if non-NULL - is not a list).
 *
 * Side effects:
 *	The ensemble is updated and marked for recompilation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_SetEnsembleSubcommandList(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble command to write to. */
    Tcl_Obj *subcmdList)
{
    EnsembleConfig *ensemblePtr = GetEnsembleFromCommand(interp, token);
    Tcl_Obj *oldList;

    if (ensemblePtr == NULL) {
	return TCL_ERROR;
    }
    if (subcmdList != NULL) {
	Tcl_Size length;

	if (TclListObjLength(interp, subcmdList, &length) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (length < 1) {
	    subcmdList = NULL;
	}
    }

    oldList = ensemblePtr->subcmdList;
    ensemblePtr->subcmdList = subcmdList;
    if (subcmdList != NULL) {
	Tcl_IncrRefCount(subcmdList);
    }
    if (oldList != NULL) {
	TclDecrRefCount(oldList);
    }

    /*
     * Trigger an eventual recomputation of the ensemble command set. Note
     * that this is slightly tricky, as it means that we are not actually
     * counting the number of namespace export actions, but it is the simplest
     * way to go!
     */

    ensemblePtr->nsPtr->exportLookupEpoch++;
    BumpEpochIfNecessary(interp, token);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetEnsembleParameterList --
 *
 *	Set the parameter list for a particular ensemble.
 *
 * Results:
 *	Tcl result code (error if command token does not indicate an ensemble
 *	or the parameter list - if non-NULL - is not a list).
 *
 * Side effects:
 *	The ensemble is updated and marked for recompilation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_SetEnsembleParameterList(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble command to write to. */
    Tcl_Obj *paramList)
{
    EnsembleConfig *ensemblePtr = GetEnsembleFromCommand(interp, token);
    Tcl_Obj *oldList;
    Tcl_Size length;

    if (ensemblePtr == NULL) {
	return TCL_ERROR;
    }
    if (paramList == NULL) {
	length = 0;
    } else {
	if (TclListObjLength(interp, paramList, &length) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (length < 1) {
	    paramList = NULL;
	}
    }

    oldList = ensemblePtr->parameterList;
    ensemblePtr->parameterList = paramList;
    if (paramList != NULL) {
	Tcl_IncrRefCount(paramList);
    }
    if (oldList != NULL) {
	TclDecrRefCount(oldList);
    }
    ensemblePtr->numParameters = length;

    /*
     * Trigger an eventual recomputation of the ensemble command set. Note
     * that this is slightly tricky, as it means that we are not actually
     * counting the number of namespace export actions, but it is the simplest
     * way to go!
     */

    ensemblePtr->nsPtr->exportLookupEpoch++;
    BumpEpochIfNecessary(interp, token);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetEnsembleMappingDict --
 *
 *	Set the mapping dictionary for a particular ensemble.
 *
 * Results:
 *	Tcl result code (error if command token does not indicate an ensemble
 *	or the mapping - if non-NULL - is not a dict).
 *
 * Side effects:
 *	The ensemble is updated and marked for recompilation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_SetEnsembleMappingDict(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble command to write to. */
    Tcl_Obj *mapDict)
{
    EnsembleConfig *ensemblePtr = GetEnsembleFromCommand(interp, token);
    Tcl_Obj *oldDict;

    if (ensemblePtr == NULL) {
	return TCL_ERROR;
    }
    if (mapDict != NULL) {
	Tcl_Size size;
	int done;
	Tcl_DictSearch search;
	Tcl_Obj *valuePtr;

	if (Tcl_DictObjSize(interp, mapDict, &size) != TCL_OK) {
	    return TCL_ERROR;
	}

	for (Tcl_DictObjFirst(NULL, mapDict, &search, NULL, &valuePtr, &done);
		!done; Tcl_DictObjNext(&search, NULL, &valuePtr, &done)) {
	    Tcl_Obj *cmdObjPtr;
	    const char *bytes;

	    if (Tcl_ListObjIndex(interp, valuePtr, 0, &cmdObjPtr) != TCL_OK) {
		Tcl_DictObjDone(&search);
		return TCL_ERROR;
	    }
	    bytes = TclGetString(cmdObjPtr);
	    if (bytes[0] != ':' || bytes[1] != ':') {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"ensemble target is not a fully-qualified command",
			TCL_AUTO_LENGTH));
		Tcl_SetErrorCode(interp, "TCL", "ENSEMBLE",
			"UNQUALIFIED_TARGET", (char *)NULL);
		Tcl_DictObjDone(&search);
		return TCL_ERROR;
	    }
	}

	if (size < 1) {
	    mapDict = NULL;
	}
    }

    oldDict = ensemblePtr->subcommandDict;
    ensemblePtr->subcommandDict = mapDict;
    if (mapDict != NULL) {
	Tcl_IncrRefCount(mapDict);
    }
    if (oldDict != NULL) {
	TclDecrRefCount(oldDict);
    }

    /*
     * Trigger an eventual recomputation of the ensemble command set. Note
     * that this is slightly tricky, as it means that we are not actually
     * counting the number of namespace export actions, but it is the simplest
     * way to go!
     */

    ensemblePtr->nsPtr->exportLookupEpoch++;
    BumpEpochIfNecessary(interp, token);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetEnsembleUnknownHandler --
 *
 *	Set the unknown handler for a particular ensemble.
 *
 * Results:
 *	Tcl result code (error if command token does not indicate an ensemble
 *	or the unknown handler - if non-NULL - is not a list).
 *
 * Side effects:
 *	The ensemble is updated and marked for recompilation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_SetEnsembleUnknownHandler(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble command to write to. */
    Tcl_Obj *unknownList)
{
    EnsembleConfig *ensemblePtr = GetEnsembleFromCommand(interp, token);
    Tcl_Obj *oldList;

    if (ensemblePtr == NULL) {
	return TCL_ERROR;
    }
    if (unknownList != NULL) {
	Tcl_Size length;

	if (TclListObjLength(interp, unknownList, &length) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (length < 1) {
	    unknownList = NULL;
	}
    }

    oldList = ensemblePtr->unknownHandler;
    ensemblePtr->unknownHandler = unknownList;
    if (unknownList != NULL) {
	Tcl_IncrRefCount(unknownList);
    }
    if (oldList != NULL) {
	TclDecrRefCount(oldList);
    }

    /*
     * Trigger an eventual recomputation of the ensemble command set. Note
     * that this is slightly tricky, as it means that we are not actually
     * counting the number of namespace export actions, but it is the simplest
     * way to go!
     */

    ensemblePtr->nsPtr->exportLookupEpoch++;

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetEnsembleFlags --
 *
 *	Set the flags for a particular ensemble.
 *
 * Results:
 *	Tcl result code (error if command token does not indicate an
 *	ensemble).
 *
 * Side effects:
 *	The ensemble is updated and marked for recompilation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_SetEnsembleFlags(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble command to write to. */
    int flags)
{
    EnsembleConfig *ensemblePtr = GetEnsembleFromCommand(interp, token);
    int changedFlags = flags ^ ensemblePtr->flags;

    if (ensemblePtr == NULL) {
	return TCL_ERROR;
    }

    /*
     * This API refuses to set the ENSEMBLE_DEAD flag...
     */

    ensemblePtr->flags &= ENSEMBLE_DEAD;
    ensemblePtr->flags |= flags & ~ENSEMBLE_DEAD;

    /*
     * Trigger an eventual recomputation of the ensemble command set. Note
     * that this is slightly tricky, as it means that we are not actually
     * counting the number of namespace export actions, but it is the simplest
     * way to go!
     */

    ensemblePtr->nsPtr->exportLookupEpoch++;

    /*
     * If the ENSEMBLE_COMPILE flag status was changed, install or remove the
     * compiler function and bump the interpreter's compilation epoch so that
     * bytecode gets regenerated.
     */

    if (changedFlags & ENSEMBLE_COMPILE) {
	((Command*) ensemblePtr->token)->compileProc =
		((flags & ENSEMBLE_COMPILE) ? TclCompileEnsemble : NULL);
	((Interp *) interp)->compileEpoch++;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetEnsembleSubcommandList --
 *
 *	Get the list of subcommands associated with a particular ensemble.
 *
 * Results:
 *	Tcl result code (error if command token does not indicate an
 *	ensemble). The list of subcommands is returned by updating the
 *	variable pointed to by the last parameter (NULL if this is to be
 *	derived from the mapping dictionary or the associated namespace's
 *	exported commands).
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetEnsembleSubcommandList(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble command to read from. */
    Tcl_Obj **subcmdListPtr)
{
    EnsembleConfig *ensemblePtr = GetEnsembleFromCommand(interp, token);

    if (ensemblePtr == NULL) {
	return TCL_ERROR;
    }
    *subcmdListPtr = ensemblePtr->subcmdList;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetEnsembleParameterList --
 *
 *	Get the list of parameters associated with a particular ensemble.
 *
 * Results:
 *	Tcl result code (error if command token does not indicate an
 *	ensemble). The list of parameters is returned by updating the
 *	variable pointed to by the last parameter (NULL if there are
 *	no parameters).
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetEnsembleParameterList(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble command to read from. */
    Tcl_Obj **paramListPtr)
{
    EnsembleConfig *ensemblePtr = GetEnsembleFromCommand(interp, token);

    if (ensemblePtr == NULL) {
	return TCL_ERROR;
    }
    *paramListPtr = ensemblePtr->parameterList;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetEnsembleMappingDict --
 *
 *	Get the command mapping dictionary associated with a particular
 *	ensemble.
 *
 * Results:
 *	Tcl result code (error if command token does not indicate an
 *	ensemble). The mapping dict is returned by updating the variable
 *	pointed to by the last parameter (NULL if none is installed).
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetEnsembleMappingDict(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble command to read from. */
    Tcl_Obj **mapDictPtr)
{
    EnsembleConfig *ensemblePtr = GetEnsembleFromCommand(interp, token);

    if (ensemblePtr == NULL) {
	return TCL_ERROR;
    }
    *mapDictPtr = ensemblePtr->subcommandDict;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetEnsembleUnknownHandler --
 *
 *	Get the unknown handler associated with a particular ensemble.
 *
 * Results:
 *	Tcl result code (error if command token does not indicate an
 *	ensemble). The unknown handler is returned by updating the variable
 *	pointed to by the last parameter (NULL if no handler is installed).
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetEnsembleUnknownHandler(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble command to read from. */
    Tcl_Obj **unknownListPtr)
{
    EnsembleConfig *ensemblePtr = GetEnsembleFromCommand(interp, token);

    if (ensemblePtr == NULL) {
	return TCL_ERROR;
    }
    *unknownListPtr = ensemblePtr->unknownHandler;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetEnsembleFlags --
 *
 *	Get the flags for a particular ensemble.
 *
 * Results:
 *	Tcl result code (error if command token does not indicate an
 *	ensemble). The flags are returned by updating the variable pointed to
 *	by the last parameter.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetEnsembleFlags(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble command to read from. */
    int *flagsPtr)
{
    EnsembleConfig *ensemblePtr = GetEnsembleFromCommand(interp, token);

    if (ensemblePtr == NULL) {
	return TCL_ERROR;
    }
    *flagsPtr = ensemblePtr->flags;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetEnsembleNamespace --
 *
 *	Get the namespace associated with a particular ensemble.
 *
 * Results:
 *	Tcl result code (error if command token does not indicate an
 *	ensemble). Namespace is returned by updating the variable pointed to
 *	by the last parameter.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetEnsembleNamespace(
    Tcl_Interp *interp,
    Tcl_Command token,		/* The ensemble command to read from. */
    Tcl_Namespace **namespacePtrPtr)
{
    EnsembleConfig *ensemblePtr = GetEnsembleFromCommand(interp, token);

    if (ensemblePtr == NULL) {
	return TCL_ERROR;
    }
    *namespacePtrPtr = (Tcl_Namespace *) ensemblePtr->nsPtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FindEnsemble --
 *
 *	Given a command name, get the ensemble token for it, allowing for
 *	[namespace import]s. [Bug 1017022]
 *
 * Results:
 *	The token for the ensemble command with the given name, or NULL if the
 *	command either does not exist or is not an ensemble (when an error
 *	message will be written into the interp if thats non-NULL).
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
Tcl_FindEnsemble(
    Tcl_Interp *interp,		/* Where to do the lookup, and where to write
				 * the errors if TCL_LEAVE_ERR_MSG is set in
				 * the flags. */
    Tcl_Obj *cmdNameObj,	/* Name of command to look up. */
    int flags)			/* Either 0 or TCL_LEAVE_ERR_MSG; other flags
				 * are probably not useful. */
{
    Tcl_Command token;

    token = Tcl_FindCommand(interp, TclGetString(cmdNameObj), NULL, flags);
    if (token == NULL) {
	return NULL;
    }

    if (((Command *) token)->objProc != TclEnsembleImplementationCmd) {
	/*
	 * Reuse existing infrastructure for following import link chains
	 * rather than duplicating it.
	 */

	token = TclGetOriginalCommand(token);

	if (token == NULL ||
		((Command *) token)->objProc != TclEnsembleImplementationCmd) {
	    if (flags & TCL_LEAVE_ERR_MSG) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"\"%s\" is not an ensemble command",
			TclGetString(cmdNameObj)));
		Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "ENSEMBLE",
			TclGetString(cmdNameObj), (char *)NULL);
	    }
	    return NULL;
	}
    }

    return token;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_IsEnsemble --
 *
 *	Simple test for ensemble-hood that takes into account imported
 *	ensemble commands as well.
 *
 * Results:
 *	Boolean value
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

int
Tcl_IsEnsemble(
    Tcl_Command token)		/* The command to check. */
{
    Command *cmdPtr = (Command *) token;

    if (cmdPtr->objProc == TclEnsembleImplementationCmd) {
	return 1;
    }
    cmdPtr = (Command *) TclGetOriginalCommand((Tcl_Command) cmdPtr);
    if (cmdPtr == NULL || cmdPtr->objProc != TclEnsembleImplementationCmd) {
	return 0;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclMakeEnsemble --
 *
 *	Create an ensemble from a table of implementation commands. The
 *	ensemble will be subject to (limited) compilation if any of the
 *	implementation commands are compilable.
 *
 *	The 'name' parameter may be a single command name or a list if
 *	creating an ensemble subcommand (see the binary implementation).
 *
 *	Currently, the TCL_ENSEMBLE_PREFIX ensemble flag is only used on
 *	top-level ensemble commands.
 *
 *	This code is not safe to run in Safe interpreter after user code has
 *	executed. That's OK right now because it's just used to set up Tcl,
 *	but it means we mustn't expose it at all, not even to Tk (until we can
 *	hide commands in namespaces directly).
 *
 * Results:
 *	Handle for the new ensemble, or NULL on failure.
 *
 * Side effects:
 *	May advance the bytecode compilation epoch.
 *
 *----------------------------------------------------------------------
 */

Tcl_Command
TclMakeEnsemble(
    Tcl_Interp *interp,
    const char *name,		/* The ensemble name (as explained above) */
    const EnsembleImplMap map[])/* The subcommands to create */
{
    Tcl_Command ensemble;
    Tcl_Namespace *ns;
    Tcl_DString buf, hiddenBuf;
    const char **nameParts = NULL;
    const char *cmdName = NULL;
    Tcl_Size i, nameCount = 0;
    int ensembleFlags = 0;
    Tcl_Size hiddenLen;

    /*
     * Construct the path for the ensemble namespace and create it.
     */

    Tcl_DStringInit(&buf);
    Tcl_DStringInit(&hiddenBuf);
    TclDStringAppendLiteral(&hiddenBuf, "tcl:");
    Tcl_DStringAppend(&hiddenBuf, name, TCL_AUTO_LENGTH);
    TclDStringAppendLiteral(&hiddenBuf, ":");
    hiddenLen = Tcl_DStringLength(&hiddenBuf);
    if (name[0] == ':' && name[1] == ':') {
	/*
	 * An absolute name, so use it directly.
	 */

	cmdName = name;
	Tcl_DStringAppend(&buf, name, TCL_AUTO_LENGTH);
	ensembleFlags = TCL_ENSEMBLE_PREFIX;
    } else {
	/*
	 * Not an absolute name, so do munging of it. Note that this treats a
	 * multi-word list differently to a single word.
	 */

	TclDStringAppendLiteral(&buf, "::tcl");

	if (Tcl_SplitList(NULL, name, &nameCount, &nameParts) != TCL_OK) {
	    Tcl_Panic("invalid ensemble name '%s'", name);
	}

	for (i = 0; i < nameCount; ++i) {
	    TclDStringAppendLiteral(&buf, "::");
	    Tcl_DStringAppend(&buf, nameParts[i], TCL_AUTO_LENGTH);
	}
    }

    ns = Tcl_FindNamespace(interp, Tcl_DStringValue(&buf), NULL,
	    TCL_CREATE_NS_IF_UNKNOWN);
    if (!ns) {
	Tcl_Panic("unable to find or create %s namespace!",
		Tcl_DStringValue(&buf));
    }

    /*
     * Create the named ensemble in the correct namespace
     */

    if (cmdName == NULL) {
	if (nameCount == 1) {
	    ensembleFlags = TCL_ENSEMBLE_PREFIX;
	    cmdName = Tcl_DStringValue(&buf) + 5;
	} else {
	    ns = ns->parentPtr;
	    cmdName = nameParts[nameCount - 1];
	}
    }

    /*
     * Switch on compilation always for core ensembles now that we can do
     * nice bytecode things with them.  Do it now.  Waiting until later will
     * just cause pointless epoch bumps.
     */

    ensembleFlags |= ENSEMBLE_COMPILE;
    ensemble = Tcl_CreateEnsemble(interp, cmdName, ns, ensembleFlags);

    /*
     * Create the ensemble mapping dictionary and the ensemble command procs.
     */

    if (ensemble != NULL) {
	Tcl_Obj *mapDict, *toObj;
	Command *cmdPtr;

	TclDStringAppendLiteral(&buf, "::");
	TclNewObj(mapDict);
	for (i=0 ; map[i].name != NULL ; i++) {
	    TclNewStringObj(toObj, Tcl_DStringValue(&buf),
		    Tcl_DStringLength(&buf));
	    Tcl_AppendToObj(toObj, map[i].name, TCL_AUTO_LENGTH);
	    TclDictPut(NULL, mapDict, map[i].name, toObj);

	    if (map[i].proc || map[i].nreProc) {
		/*
		 * If the command is unsafe, hide it when we're in a safe
		 * interpreter. The code to do this is really hokey! It also
		 * doesn't work properly yet; this function is always
		 * currently called before the safe-interp flag is set so the
		 * Tcl_IsSafe check fails.
		 */

		if (map[i].unsafe && Tcl_IsSafe(interp)) {
		    cmdPtr = (Command *)
			    Tcl_NRCreateCommand(interp, "___tmp", map[i].proc,
			    map[i].nreProc, map[i].clientData, NULL);
		    Tcl_DStringSetLength(&hiddenBuf, hiddenLen);
		    if (Tcl_HideCommand(interp, "___tmp",
			    Tcl_DStringAppend(&hiddenBuf, map[i].name,
				    TCL_AUTO_LENGTH))) {
			Tcl_Panic("%s", Tcl_GetStringResult(interp));
		    }
		    /* don't compile unsafe subcommands in safe interp */
		    cmdPtr->compileProc = NULL;
		} else {
		    /*
		     * Not hidden, so just create it. Yay!
		     */

		    cmdPtr = (Command *)
			    Tcl_NRCreateCommand(interp, TclGetString(toObj),
			    map[i].proc, map[i].nreProc, map[i].clientData,
			    NULL);
		    cmdPtr->compileProc = map[i].compileProc;
		}
	    }
	}
	Tcl_SetEnsembleMappingDict(interp, ensemble, mapDict);
    }

    Tcl_DStringFree(&buf);
    Tcl_DStringFree(&hiddenBuf);
    if (nameParts != NULL) {
	Tcl_Free((void *)nameParts);
    }
    return ensemble;
}

/*
 *----------------------------------------------------------------------
 *
 * TclEnsembleImplementationCmd --
 *
 *	Implements an ensemble of commands (being those exported by a
 *	namespace other than the global namespace) as a command with the same
 *	(short) name as the namespace in the parent namespace.
 *
 * Results:
 *	A standard Tcl result code. Will be TCL_ERROR if the command is not an
 *	unambiguous prefix of any command exported by the ensemble's
 *	namespace.
 *
 * Side effects:
 *	Depends on the command within the namespace that gets executed. If the
 *	ensemble itself returns TCL_ERROR, a descriptive error message will be
 *	placed in the interpreter's result.
 *
 *----------------------------------------------------------------------
 */

int
TclEnsembleImplementationCmd(
    void *clientData,
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    return Tcl_NRCallObjProc(interp, NsEnsembleImplementationCmdNR,
	    clientData, objc, objv);
}

static int
NsEnsembleImplementationCmdNR(
    void *clientData,		/* The ensemble this is the impl. of. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *const objv[])
{
    EnsembleConfig *ensemblePtr = (EnsembleConfig *) clientData;
				/* The ensemble itself. */
    Tcl_Obj *prefixObj;		/* An object containing the prefix words of
				 * the command that implements the
				 * subcommand. */
    Tcl_HashEntry *hPtr;	/* Used for efficient lookup of fully
				 * specified but not yet cached command
				 * names. */
    int reparseCount = 0;	/* Number of reparses. */
    Tcl_Obj *errorObj;		/* Used for building error messages. */
    Tcl_Obj *subObj;
    Tcl_Size subIdx;

    /*
     * Must recheck objc since numParameters might have changed. See test
     * namespace-53.9.
     */

  restartEnsembleParse:
    subIdx = 1 + ensemblePtr->numParameters;
    if (objc < subIdx + 1) {
	/*
	 * No subcommand argument. Make error message.
	 */

	Tcl_DString buf;	/* Message being built */

	Tcl_DStringInit(&buf);
	if (ensemblePtr->parameterList) {
	    TclDStringAppendObj(&buf, ensemblePtr->parameterList);
	    TclDStringAppendLiteral(&buf, " ");
	}
	TclDStringAppendLiteral(&buf, "subcommand ?arg ...?");
	Tcl_WrongNumArgs(interp, 1, objv, Tcl_DStringValue(&buf));
	Tcl_DStringFree(&buf);

	return TCL_ERROR;
    }

    if (ensemblePtr->nsPtr->flags & NS_DEAD) {
	/*
	 * Don't know how we got here, but make things give up quickly.
	 */

	if (!Tcl_InterpDeleted(interp)) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "ensemble activated for deleted namespace",
		    TCL_AUTO_LENGTH));
	    Tcl_SetErrorCode(interp, "TCL", "ENSEMBLE", "DEAD", (char *)NULL);
	}
	return TCL_ERROR;
    }

    /*
     * If the table of subcommands is valid just lookup up the command there
     * and go to dispatch.
     */

    subObj = objv[subIdx];

    if (ensemblePtr->epoch == ensemblePtr->nsPtr->exportLookupEpoch) {
	/*
	 * Table of subcommands is still valid so if the internal representtion
	 * is an ensembleCmd, just call it.
	 */
	EnsembleCmdRep *ensembleCmd;

	ECRGetInternalRep(subObj, ensembleCmd);
	if (ensembleCmd) {
	    if (ensembleCmd->epoch == ensemblePtr->epoch &&
		    ensembleCmd->token == (Command *) ensemblePtr->token) {
		prefixObj = (Tcl_Obj *) Tcl_GetHashValue(ensembleCmd->hPtr);
		Tcl_IncrRefCount(prefixObj);
		if (ensembleCmd->fix) {
		    TclSpellFix(interp, objv, objc, subIdx, subObj, ensembleCmd->fix);
		}
		goto runResultingSubcommand;
	    }
	}
    } else {
	BuildEnsembleConfig(ensemblePtr);
	ensemblePtr->epoch = ensemblePtr->nsPtr->exportLookupEpoch;
    }

    /*
     * Look in the hashtable for the named subcommand.  This is the fastest
     * path if there is no cache in operation.
     */

    hPtr = Tcl_FindHashEntry(&ensemblePtr->subcommandTable,
	    TclGetString(subObj));
    if (hPtr != NULL) {
	/*
	 * Cache ensemble in the subcommand object for later.
	 */

	MakeCachedEnsembleCommand(subObj, ensemblePtr, hPtr, NULL);
    } else if (!(ensemblePtr->flags & TCL_ENSEMBLE_PREFIX)) {
	/*
	 * Could not map.  No prefixing.  Go to unknown/error handling.
	 */

	goto unknownOrAmbiguousSubcommand;
    } else {
	/*
	 * If the command isn't yet confirmed with the hash as part of building
	 * the export table, scan the sorted array for matches.
	 */

	const char *subcmdName; /* Name of the subcommand or unique prefix of
				 * it (a non-unique prefix produces an error). */
	char *fullName = NULL;	/* Full name of the subcommand. */
	Tcl_Size stringLength, i;
	Tcl_Size tableLength = ensemblePtr->subcommandTable.numEntries;
	Tcl_Obj *fix;

	subcmdName = TclGetStringFromObj(subObj, &stringLength);
	for (i=0 ; i<tableLength ; i++) {
	    int cmp = strncmp(subcmdName,
		    ensemblePtr->subcommandArrayPtr[i],
		    stringLength);

	    if (cmp == 0) {
		if (fullName != NULL) {
		    /*
		     * Hash search filters out the exact-match case, so getting
		     * here indicates that the subcommand is an ambiguous
		     * prefix of at least two exported subcommands, which is an
		     * error case.
		     */

		    goto unknownOrAmbiguousSubcommand;
		}
		fullName = ensemblePtr->subcommandArrayPtr[i];
	    } else if (cmp < 0) {
		/*
		 * The table is sorted so stop searching because a match would
		 * have been found already.
		 */

		break;
	    }
	}
	if (fullName == NULL) {
	    /*
	     * The subcommand is not a prefix of anything. Bail out!
	     */

	    goto unknownOrAmbiguousSubcommand;
	}
	hPtr = Tcl_FindHashEntry(&ensemblePtr->subcommandTable, fullName);
	if (hPtr == NULL) {
	    Tcl_Panic("full name %s not found in supposedly synchronized hash",
		    fullName);
	}

	/*
	 * Record the spelling correction for usage message.
	 */

	fix = Tcl_NewStringObj(fullName, TCL_AUTO_LENGTH);

	/*
	 * Cache for later in the subcommand object.
	 */

	MakeCachedEnsembleCommand(subObj, ensemblePtr, hPtr, fix);
	TclSpellFix(interp, objv, objc, subIdx, subObj, fix);
    }

    prefixObj = (Tcl_Obj *) Tcl_GetHashValue(hPtr);
    Tcl_IncrRefCount(prefixObj);
  runResultingSubcommand:

    /*
     * Execute the subcommand by populating an array of objects, which might
     * not be the same length as the number of arguments to this ensemble
     * command, and then handing it to the main command-lookup engine. In
     * theory, the command could be looked up right here using the namespace in
     * which it is guaranteed to exist,
     *
     *   ((Q: That's not true if the -map option is used, is it?))
     *
     * but don't do that because caching of the command object should help.
     */

    {
	Tcl_Obj *copyPtr;	/* The list of words to dispatch on.
				 * Will be freed by the dispatch engine. */
	Tcl_Obj **copyObjv;
	Tcl_Size copyObjc, prefixObjc;

	TclListObjLength(NULL, prefixObj, &prefixObjc);

	if (objc == 2) {
	    copyPtr = TclListObjCopy(NULL, prefixObj);
	} else {
	    copyPtr = Tcl_NewListObj(objc - 2 + prefixObjc, NULL);
	    Tcl_ListObjAppendList(NULL, copyPtr, prefixObj);
	    Tcl_ListObjReplace(NULL, copyPtr, LIST_MAX, 0,
		    ensemblePtr->numParameters, objv + 1);
	    Tcl_ListObjReplace(NULL, copyPtr, LIST_MAX, 0,
		    objc - 2 - ensemblePtr->numParameters,
		    objv + 2 + ensemblePtr->numParameters);
	}
	Tcl_IncrRefCount(copyPtr);
	TclNRAddCallback(interp, TclNRReleaseValues, copyPtr);
	TclDecrRefCount(prefixObj);

	/*
	 * Record the words of the command as given so that routines like
	 * Tcl_WrongNumArgs can produce the correct error message. Parameters
	 * count both as inserted and removed arguments.
	 */

	if (TclInitRewriteEnsemble(interp, 2 + ensemblePtr->numParameters,
		prefixObjc + ensemblePtr->numParameters, objv)) {
	    TclNRAddCallback(interp, TclClearRootEnsemble);
	}

	/*
	 * Hand off to the target command.
	 */

	TclSkipTailcall(interp);
	TclListObjGetElements(NULL, copyPtr, &copyObjc, &copyObjv);
	((Interp *) interp)->lookupNsPtr = ensemblePtr->nsPtr;
	return TclNREvalObjv(interp, copyObjc, copyObjv, TCL_EVAL_INVOKE, NULL);
    }

  unknownOrAmbiguousSubcommand:
    /*
     * The named subcommand did not match any exported command. If there is a
     * handler registered unknown subcommands, call it, but not more than once
     * for this call.
     */

    if (ensemblePtr->unknownHandler != NULL && reparseCount++ < 1) {
	switch (EnsembleUnknownCallback(interp, ensemblePtr, objc, objv,
		&prefixObj)) {
	case TCL_OK:
	    goto runResultingSubcommand;
	case TCL_ERROR:
	    return TCL_ERROR;
	case TCL_CONTINUE:
	    goto restartEnsembleParse;
	}
    }

    /*
     * Could not find a routine for the named subcommand so generate a standard
     * failure message.  The one odd case compared with a standard
     * ensemble-like command is where a namespace has no exported commands at
     * all...
     */

    Tcl_ResetResult(interp);
    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "SUBCOMMAND",
	    TclGetString(subObj), (char *)NULL);
    if (ensemblePtr->subcommandTable.numEntries == 0) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"unknown subcommand \"%s\": namespace %s does not"
		" export any commands", TclGetString(subObj),
		ensemblePtr->nsPtr->fullName));
	return TCL_ERROR;
    }
    errorObj = Tcl_ObjPrintf("unknown%s subcommand \"%s\": must be ",
	    (ensemblePtr->flags & TCL_ENSEMBLE_PREFIX ? " or ambiguous" : ""),
	    TclGetString(subObj));
    if (ensemblePtr->subcommandTable.numEntries == 1) {
	Tcl_AppendToObj(errorObj, ensemblePtr->subcommandArrayPtr[0],
		TCL_AUTO_LENGTH);
    } else {
	Tcl_Size i;

	for (i=0 ; i<ensemblePtr->subcommandTable.numEntries-1 ; i++) {
	    Tcl_AppendToObj(errorObj, ensemblePtr->subcommandArrayPtr[i],
		    TCL_AUTO_LENGTH);
	    Tcl_AppendToObj(errorObj, ", ", 2);
	}
	Tcl_AppendPrintfToObj(errorObj, "or %s",
		ensemblePtr->subcommandArrayPtr[i]);
    }
    Tcl_SetObjResult(interp, errorObj);
    return TCL_ERROR;
}

int
TclClearRootEnsemble(
    TCL_UNUSED(void **),
    Tcl_Interp *interp,
    int result)
{
    TclResetRewriteEnsemble(interp, 1);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitRewriteEnsemble --
 *
 *	Applies a rewrite of arguments so that an ensemble subcommand
 *	correctly reports any error messages for the overall command.
 *
 * Results:
 *	Whether this is the first rewrite applied, a value which must be
 *	passed to TclResetRewriteEnsemble when undoing this command's
 *	behaviour.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclInitRewriteEnsemble(
    Tcl_Interp *interp,
    Tcl_Size numRemoved,
    Tcl_Size numInserted,
    Tcl_Obj *const *objv)
{
    Interp *iPtr = (Interp *) interp;

    int isRootEnsemble = (iPtr->ensembleRewrite.sourceObjs == NULL);

    if (isRootEnsemble) {
	iPtr->ensembleRewrite.sourceObjs = objv;
	iPtr->ensembleRewrite.numRemovedObjs = numRemoved;
	iPtr->ensembleRewrite.numInsertedObjs = numInserted;
    } else {
	Tcl_Size numIns = iPtr->ensembleRewrite.numInsertedObjs;

	if (numIns < numRemoved) {
	    iPtr->ensembleRewrite.numRemovedObjs += numRemoved - numIns;
	    iPtr->ensembleRewrite.numInsertedObjs = numInserted;
	} else {
	    iPtr->ensembleRewrite.numInsertedObjs += numInserted - numRemoved;
	}
    }
    return isRootEnsemble;
}

/*
 *----------------------------------------------------------------------
 *
 * TclResetRewriteEnsemble --
 *
 *	Removes any rewrites applied to support proper reporting of error
 *	messages used in ensembles. Should be paired with
 *	TclInitRewriteEnsemble.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclResetRewriteEnsemble(
    Tcl_Interp *interp,
    int isRootEnsemble)
{
    Interp *iPtr = (Interp *) interp;

    if (isRootEnsemble) {
	iPtr->ensembleRewrite.sourceObjs = NULL;
	iPtr->ensembleRewrite.numRemovedObjs = 0;
	iPtr->ensembleRewrite.numInsertedObjs = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclSpellFix --
 *
 *	Records a spelling correction that needs making in the generation of
 *	the WrongNumArgs usage message.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Can create an alternative ensemble rewrite structure.
 *
 *----------------------------------------------------------------------
 */

static int
FreeER(
    void *data[],
    TCL_UNUSED(Tcl_Interp *),
    int result)
{
    Tcl_Obj **tmp = (Tcl_Obj **) data[0];
    Tcl_Obj **store = (Tcl_Obj **) data[1];

    Tcl_Free(store);
    Tcl_Free(tmp);
    return result;
}

void
TclSpellFix(
    Tcl_Interp *interp,
    Tcl_Obj *const *objv,
    Tcl_Size objc,
    Tcl_Size badIdx,
    Tcl_Obj *bad,
    Tcl_Obj *fix)
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Obj *const *search;
    Tcl_Obj **store;
    Tcl_Size idx;
    Tcl_Size size;

    if (iPtr->ensembleRewrite.sourceObjs == NULL) {
	iPtr->ensembleRewrite.sourceObjs = objv;
	iPtr->ensembleRewrite.numRemovedObjs = 0;
	iPtr->ensembleRewrite.numInsertedObjs = 0;
    }

    /*
     * Compute the valid length of the ensemble root.
     */

    size = iPtr->ensembleRewrite.numRemovedObjs + objc
	    - iPtr->ensembleRewrite.numInsertedObjs;

    search = iPtr->ensembleRewrite.sourceObjs;
    if (search[0] == NULL) {
	/*
	 * Awful casting abuse here!
	 */

	search = (Tcl_Obj *const *) search[1];
    }

    if (badIdx < iPtr->ensembleRewrite.numInsertedObjs) {
	/*
	 * Misspelled value was inserted. Cannot directly jump to the bad
	 * value.  Must search.
	 */

	idx = 1;
	while (idx < size) {
	    if (search[idx] == bad) {
		break;
	    }
	    idx++;
	}
	if (idx == size) {
	    return;
	}
    } else {
	/*
	 * Jump to the misspelled value.
	 */

	idx = iPtr->ensembleRewrite.numRemovedObjs + badIdx
		- iPtr->ensembleRewrite.numInsertedObjs;

	/* Verify */
	if (search[idx] != bad) {
	    Tcl_Panic("SpellFix: programming error");
	}
    }

    search = iPtr->ensembleRewrite.sourceObjs;
    if (search[0] == NULL) {
	store = (Tcl_Obj **) search[2];
    }  else {
	Tcl_Obj **tmp = (Tcl_Obj **) Tcl_Alloc(3 * sizeof(Tcl_Obj *));

	store = (Tcl_Obj **) Tcl_Alloc(size * sizeof(Tcl_Obj *));
	memcpy(store, iPtr->ensembleRewrite.sourceObjs,
		size * sizeof(Tcl_Obj *));

	/*
	 * Awful casting abuse here! Note that the NULL in the first element
	 * indicates that the initial objects are a raw array in the second
	 * element and the rewritten ones are a raw array in the third.
	 */

	tmp[0] = NULL;
	tmp[1] = (Tcl_Obj *) iPtr->ensembleRewrite.sourceObjs;
	tmp[2] = (Tcl_Obj *) store;
	iPtr->ensembleRewrite.sourceObjs = (Tcl_Obj *const *) tmp;

	TclNRAddCallback(interp, FreeER, tmp, store);
    }

    store[idx] = fix;
    Tcl_IncrRefCount(fix);
    TclNRAddCallback(interp, TclNRReleaseValues, fix);
}

/*
 *----------------------------------------------------------------------
 *
 * TclEnsembleGetRewriteValues --
 *
 *	Get the original arguments to the current command before any rewrite
 *	rules (from aliases, ensembles, and method forwards) were applied.
 *
 *----------------------------------------------------------------------
 */
Tcl_Obj *const *
TclEnsembleGetRewriteValues(
    Tcl_Interp *interp)		/* Current interpreter. */
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Obj *const *origObjv = iPtr->ensembleRewrite.sourceObjs;

    if (origObjv[0] == NULL) {
	origObjv = (Tcl_Obj *const *) origObjv[2];
    }
    return origObjv;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFetchEnsembleRoot --
 *
 *	Returns the root of ensemble rewriting, if any.
 *	If no root exists, returns objv instead.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
Tcl_Obj *const *
TclFetchEnsembleRoot(
    Tcl_Interp *interp,
    Tcl_Obj *const *objv,
    Tcl_Size objc,
    Tcl_Size *objcPtr)
{
    Tcl_Obj *const *sourceObjs;
    Interp *iPtr = (Interp *) interp;

    if (iPtr->ensembleRewrite.sourceObjs) {
	*objcPtr = objc + iPtr->ensembleRewrite.numRemovedObjs
		- iPtr->ensembleRewrite.numInsertedObjs;
	if (iPtr->ensembleRewrite.sourceObjs[0] == NULL) {
	    sourceObjs = (Tcl_Obj *const *) iPtr->ensembleRewrite.sourceObjs[1];
	} else {
	    sourceObjs = iPtr->ensembleRewrite.sourceObjs;
	}
	return sourceObjs;
    }
    *objcPtr = objc;
    return objv;
}

/*
 * ----------------------------------------------------------------------
 *
 * EnsembleUnknownCallback --
 *
 *	Helper for the ensemble engine.  Calls the routine registered for
 *	"ensemble unknown" case.  See the user documentation of the
 *	ensemble unknown handler for details.  Only called when such a
 *	function is defined, and is only called once per ensemble dispatch.
 *	I.e. even if a reparse still fails, this isn't called again.
 *
 * Results:
 *	TCL_OK -	*prefixObjPtr contains the command words to dispatch
 *			to.
 *	TCL_CONTINUE -	Need to reparse, i.e. *prefixObjPtr is invalid
 *	TCL_ERROR -	Something went wrong. Error message in interpreter.
 *
 * Side effects:
 *	Arbitrary, due to evaluation of script provided by client.
 *
 * ----------------------------------------------------------------------
 */

static inline int
EnsembleUnknownCallback(
    Tcl_Interp *interp,
    EnsembleConfig *ensemblePtr,/* The ensemble structure. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[],	/* Actual arguments. */
    Tcl_Obj **prefixObjPtr)	/* Where to write the prefix suggested by the
				 * unknown callback. Must not be NULL. Only has
				 * a meaningful value on TCL_OK. */
{
    Tcl_Size paramc;
    int result;
    Tcl_Size i, prefixObjc;
    Tcl_Obj **paramv, *unknownCmd, *ensObj;

    /*
     * Create the "unknown" command callback to determine what to do.
     */

    unknownCmd = Tcl_DuplicateObj(ensemblePtr->unknownHandler);
    TclNewObj(ensObj);
    Tcl_GetCommandFullName(interp, ensemblePtr->token, ensObj);
    Tcl_ListObjAppendElement(NULL, unknownCmd, ensObj);
    for (i = 1 ; i < objc ; i++) {
	Tcl_ListObjAppendElement(NULL, unknownCmd, objv[i]);
    }
    TclListObjGetElements(NULL, unknownCmd, &paramc, &paramv);
    Tcl_IncrRefCount(unknownCmd);

    /*
     * Call the "unknown" handler.  No attempt to NRE-enable this as deep
     * recursion through unknown handlers is perverse. It is always an error
     * for an unknown handler to delete its ensemble. Don't do that.
     */

    Tcl_Preserve(ensemblePtr);
    TclSkipTailcall(interp);
    result = Tcl_EvalObjv(interp, paramc, paramv, 0);
    if ((result == TCL_OK) && (ensemblePtr->flags & ENSEMBLE_DEAD)) {
	if (!Tcl_InterpDeleted(interp)) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "unknown subcommand handler deleted its ensemble",
		    TCL_AUTO_LENGTH));
	    Tcl_SetErrorCode(interp, "TCL", "ENSEMBLE", "UNKNOWN_DELETED",
		    (char *)NULL);
	}
	result = TCL_ERROR;
    }
    Tcl_Release(ensemblePtr);

    /*
     * On success the result is a list of words that form the command to be
     * executed.  If the list is empty, the ensemble should have been updated,
     * so ask the ensemble engine to reparse the original command.
     */

    if (result == TCL_OK) {
	*prefixObjPtr = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(*prefixObjPtr);
	TclDecrRefCount(unknownCmd);
	Tcl_ResetResult(interp);

	/* A non-empty list is the replacement command. */

	if (TclListObjLength(interp, *prefixObjPtr, &prefixObjc) != TCL_OK) {
	    TclDecrRefCount(*prefixObjPtr);
	    Tcl_AddErrorInfo(interp, "\n    while parsing result of "
		    "ensemble unknown subcommand handler");
	    return TCL_ERROR;
	}
	if (prefixObjc > 0) {
	    return TCL_OK;
	}

	/*
	 * Empty result => reparse.
	 */

	TclDecrRefCount(*prefixObjPtr);
	return TCL_CONTINUE;
    }

    /*
     * Convert exceptional result to an error.
     */

    if (!Tcl_InterpDeleted(interp)) {
	if (result != TCL_ERROR) {
	    Tcl_ResetResult(interp);
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "unknown subcommand handler returned bad code: ",
		    TCL_AUTO_LENGTH));
	    switch (result) {
	    case TCL_RETURN:
		Tcl_AppendToObj(Tcl_GetObjResult(interp), "return",
			TCL_AUTO_LENGTH);
		break;
	    case TCL_BREAK:
		Tcl_AppendToObj(Tcl_GetObjResult(interp), "break",
			TCL_AUTO_LENGTH);
		break;
	    case TCL_CONTINUE:
		Tcl_AppendToObj(Tcl_GetObjResult(interp), "continue",
			TCL_AUTO_LENGTH);
		break;
	    default:
		Tcl_AppendPrintfToObj(Tcl_GetObjResult(interp), "%d", result);
	    }
	    Tcl_AddErrorInfo(interp, "\n    result of "
		    "ensemble unknown subcommand handler: ");
	    Tcl_AppendObjToErrorInfo(interp, unknownCmd);
	    Tcl_SetErrorCode(interp, "TCL", "ENSEMBLE", "UNKNOWN_RESULT",
		    (char *)NULL);
	} else {
	    Tcl_AddErrorInfo(interp,
		    "\n    (ensemble unknown subcommand handler)");
	}
    }
    TclDecrRefCount(unknownCmd);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeCachedEnsembleCommand --
 *
 *	Caches what has been computed so far to minimize string copying.
 *	Starts by deleting any existing representation but reusing the existing
 *	structure if it is an ensembleCmd.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Converts the internal representation of the given object to an
 *	ensembleCmd.
 *
 *----------------------------------------------------------------------
 */

static void
MakeCachedEnsembleCommand(
    Tcl_Obj *objPtr,		/* Object to cache in. */
    EnsembleConfig *ensemblePtr,/* Ensemble implementation. */
    Tcl_HashEntry *hPtr,	/* What to cache; what the object maps to. */
    Tcl_Obj *fix)		/* Spelling correction for later error, or NULL
				 * if no correction. */
{
    EnsembleCmdRep *ensembleCmd;

    ECRGetInternalRep(objPtr, ensembleCmd);
    if (ensembleCmd) {
	TclCleanupCommandMacro(ensembleCmd->token);
	if (ensembleCmd->fix) {
	    Tcl_DecrRefCount(ensembleCmd->fix);
	}
    } else {
	/*
	 * Replace any old internal representation with a new one.
	 */

	ensembleCmd = (EnsembleCmdRep *) Tcl_Alloc(sizeof(EnsembleCmdRep));
	ECRSetInternalRep(objPtr, ensembleCmd);
    }

    /*
     * Populate the internal rep.
     */

    ensembleCmd->epoch = ensemblePtr->epoch;
    ensembleCmd->token = (Command *) ensemblePtr->token;
    ensembleCmd->token->refCount++;
    if (fix) {
	Tcl_IncrRefCount(fix);
    }
    ensembleCmd->fix = fix;
    ensembleCmd->hPtr = hPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteEnsembleConfig --
 *
 *	Destroys the data structure used to represent an ensemble.  Called when
 *	the procedure for the ensemble is deleted, which happens automatically
 *	if the namespace for the ensemble is deleted.  Deleting the procedure
 *	for an ensemble is the right way to initiate cleanup.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is eventually deallocated.
 *
 *----------------------------------------------------------------------
 */

static void
ClearTable(
    EnsembleConfig *ensemblePtr)/* Ensemble to clear table of. */
{
    Tcl_HashTable *hash = &ensemblePtr->subcommandTable;

    if (hash->numEntries != 0) {
	Tcl_HashSearch search;
	Tcl_HashEntry *hPtr = Tcl_FirstHashEntry(hash, &search);

	while (hPtr != NULL) {
	    Tcl_Obj *prefixObj = (Tcl_Obj *) Tcl_GetHashValue(hPtr);
	    Tcl_DecrRefCount(prefixObj);
	    hPtr = Tcl_NextHashEntry(&search);
	}
	Tcl_Free(ensemblePtr->subcommandArrayPtr);
    }
    Tcl_DeleteHashTable(hash);
}

static void
DeleteEnsembleConfig(
    void *clientData)		/* Ensemble to delete. */
{
    EnsembleConfig *ensemblePtr = (EnsembleConfig *) clientData;
    Namespace *nsPtr = ensemblePtr->nsPtr;

    /* Unlink from the ensemble chain if it not already marked as unlinked. */

    if (ensemblePtr->next != ensemblePtr) {
	EnsembleConfig *ensPtr = (EnsembleConfig *) nsPtr->ensembles;

	if (ensPtr == ensemblePtr) {
	    nsPtr->ensembles = (Tcl_Ensemble *) ensemblePtr->next;
	} else {
	    while (ensPtr != NULL) {
		if (ensPtr->next == ensemblePtr) {
		    ensPtr->next = ensemblePtr->next;
		    break;
		}
		ensPtr = ensPtr->next;
	    }
	}
    }

    /*
     * Mark the namespace as dead so code that uses Tcl_Preserve() can tell
     * whether disaster happened anyway.
     */

    ensemblePtr->flags |= ENSEMBLE_DEAD;

    /*
     * Release the fields that contain pointers.
     */

    ClearTable(ensemblePtr);
    if (ensemblePtr->subcmdList != NULL) {
	Tcl_DecrRefCount(ensemblePtr->subcmdList);
    }
    if (ensemblePtr->parameterList != NULL) {
	Tcl_DecrRefCount(ensemblePtr->parameterList);
    }
    if (ensemblePtr->subcommandDict != NULL) {
	Tcl_DecrRefCount(ensemblePtr->subcommandDict);
    }
    if (ensemblePtr->unknownHandler != NULL) {
	Tcl_DecrRefCount(ensemblePtr->unknownHandler);
    }

    /*
     * Arrange for the structure to be reclaimed. This is complex because it is
     * necessary to react sensibly when an ensemble is deleted during its
     * initialisation, particularly in the case of an unknown callback.
     */

    Tcl_EventuallyFree(ensemblePtr, TCL_DYNAMIC);
}

/*
 *----------------------------------------------------------------------
 *
 * BuildEnsembleConfig --
 *
 *	Creates the internal data structures that describe how an ensemble
 *	looks.  The structures are a hash map from the full command name to the
 *	Tcl list that describes the implementation prefix words, and a sorted
 *	array of all the full command names to allow for reasonably efficient
 *	handling of an unambiguous prefix.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Reallocates and rebuilds the hash table and array stored at the
 *	ensemblePtr argument. For large ensembles or large namespaces, this is
 *	may be an expensive operation.
 *
 *----------------------------------------------------------------------
 */

static void
BuildEnsembleConfig(
    EnsembleConfig *ensemblePtr)/* Ensemble to set up. */
{
    Tcl_HashSearch search;	/* Used for scanning the commands in
				 * the namespace for this ensemble. */
    Tcl_Size i, j;
    int isNew;
    Tcl_HashTable *hash = &ensemblePtr->subcommandTable;
    Tcl_HashEntry *hPtr;
    Tcl_Obj *mapDict = ensemblePtr->subcommandDict;
    Tcl_Obj *subList = ensemblePtr->subcmdList;

    ClearTable(ensemblePtr);
    Tcl_InitHashTable(hash, TCL_STRING_KEYS);

    if (subList) {
	Tcl_Size subc;
	Tcl_Obj **subv, *target, *cmdObj, *cmdPrefixObj;
	const char *name;

	/*
	 * There is a list of exactly what subcommands go in the table.
	 * Determine the target for each.
	 */

	TclListObjGetElements(NULL, subList, &subc, &subv);
	if (subList == mapDict) {
	    /*
	     * Unusual case where explicit list of subcommands is same value
	     * as the dict mapping to targets.
	     */

	    for (i = 0; i < subc; i += 2) {
		name = TclGetString(subv[i]);
		hPtr = Tcl_CreateHashEntry(hash, name, &isNew);
		if (!isNew) {
		    cmdObj = (Tcl_Obj *) Tcl_GetHashValue(hPtr);
		    Tcl_DecrRefCount(cmdObj);
		}
		Tcl_SetHashValue(hPtr, subv[i + 1]);
		Tcl_IncrRefCount(subv[i + 1]);

		name = TclGetString(subv[i + 1]);
		hPtr = Tcl_CreateHashEntry(hash, name, &isNew);
		if (isNew) {
		    cmdObj = Tcl_NewStringObj(name, TCL_AUTO_LENGTH);
		    cmdPrefixObj = Tcl_NewListObj(1, &cmdObj);
		    Tcl_SetHashValue(hPtr, cmdPrefixObj);
		    Tcl_IncrRefCount(cmdPrefixObj);
		}
	    }
	} else {
	    /*
	     * Usual case where we can freely act on the list and dict.
	     */

	    for (i = 0; i < subc; i++) {
		name = TclGetString(subv[i]);
		hPtr = Tcl_CreateHashEntry(hash, name, &isNew);
		if (!isNew) {
		    continue;
		}

		/*
		 * Lookup target in the dictionary.
		 */

		if (mapDict) {
		    Tcl_DictObjGet(NULL, mapDict, subv[i], &target);
		    if (target) {
			Tcl_SetHashValue(hPtr, target);
			Tcl_IncrRefCount(target);
			continue;
		    }
		}

		/*
		 * Target was not in the dictionary.  Map onto the namespace.
		 * In this case there is no guarantee that the command is
		 * actually there.  It is the responsibility of the programmer
		 * (or [::unknown] of course) to provide the procedure.
		 */

		cmdObj = Tcl_NewStringObj(name, TCL_AUTO_LENGTH);
		cmdPrefixObj = Tcl_NewListObj(1, &cmdObj);
		Tcl_SetHashValue(hPtr, cmdPrefixObj);
		Tcl_IncrRefCount(cmdPrefixObj);
	    }
	}
    } else if (mapDict) {
	/*
	 * No subcmd list, but there is a mapping dictionary, so use
	 * the keys of that. Convert the contents of the dictionary into the
	 * form required for the internal hashtable of the ensemble.
	 */

	Tcl_DictSearch dictSearch;
	Tcl_Obj *keyObj, *valueObj;
	int done;

	Tcl_DictObjFirst(NULL, ensemblePtr->subcommandDict, &dictSearch,
		&keyObj, &valueObj, &done);
	while (!done) {
	    const char *name = TclGetString(keyObj);

	    hPtr = Tcl_CreateHashEntry(hash, name, NULL);
	    Tcl_SetHashValue(hPtr, valueObj);
	    Tcl_IncrRefCount(valueObj);
	    Tcl_DictObjNext(&dictSearch, &keyObj, &valueObj, &done);
	}
    } else {
	/*
	 * Use the array of patterns and the hash table whose keys are the
	 * commands exported by the namespace.  The corresponding values do not
	 * matter here.  Filter the commands in the namespace against the
	 * patterns in the export list to find out what commands are actually
	 * exported. Use an intermediate hash table to make memory management
	 * easier and to make exact matching much easier.
	 *
	 * Suggestion for future enhancement: Compute the unique prefixes and
	 * place them in the hash too for even faster matching.
	 */

	hPtr = Tcl_FirstHashEntry(&ensemblePtr->nsPtr->cmdTable, &search);
	for (; hPtr!= NULL ; hPtr=Tcl_NextHashEntry(&search)) {
	    char *nsCmdName = (char *)	/* Name of command in namespace. */
		    Tcl_GetHashKey(&ensemblePtr->nsPtr->cmdTable, hPtr);

	    for (i=0 ; i<ensemblePtr->nsPtr->numExportPatterns ; i++) {
		if (Tcl_StringMatch(nsCmdName,
			ensemblePtr->nsPtr->exportArrayPtr[i])) {
		    hPtr = Tcl_CreateHashEntry(hash, nsCmdName, &isNew);

		    /*
		     * Remember, hash entries have a full reference to the
		     * substituted part of the command (as a list) as their
		     * content!
		     */

		    if (isNew) {
			Tcl_Obj *cmdObj, *cmdPrefixObj;

			TclNewObj(cmdObj);
			Tcl_AppendStringsToObj(cmdObj,
				ensemblePtr->nsPtr->fullName,
				(ensemblePtr->nsPtr->parentPtr ? "::" : ""),
				nsCmdName, (char *)NULL);
			cmdPrefixObj = Tcl_NewListObj(1, &cmdObj);
			Tcl_SetHashValue(hPtr, cmdPrefixObj);
			Tcl_IncrRefCount(cmdPrefixObj);
		    }
		    break;
		}
	    }
	}
    }

    if (hash->numEntries == 0) {
	ensemblePtr->subcommandArrayPtr = NULL;
	return;
    }

    /*
     * Create a sorted array of all subcommands in the ensemble.  Hash tables
     * are all very well for a quick look for an exact match, but they can't
     * determine things like whether a string is a prefix of another, at least
     * not without a lot of preparation, and they're not useful for generating
     * the error message either.
     *
     * Do this by filling an array with the names:  Use the hash keys
     * directly to save a copy since any time we change the array we change
     * the hash too, and vice versa, and run quicksort over the array.
     */

    ensemblePtr->subcommandArrayPtr = (char **)
	    Tcl_Alloc(sizeof(char *) * hash->numEntries);

    /*
     * Fill the array from both ends as this reduces the likelihood of
     * performance problems in qsort(). This makes this code much more opaque,
     * but the naive alternatve:
     *
     * for (hPtr=Tcl_FirstHashEntry(hash,&search),i=0 ;
     *	       hPtr!=NULL ; hPtr=Tcl_NextHashEntry(&search),i++) {
     *     ensemblePtr->subcommandArrayPtr[i] = Tcl_GetHashKey(hash, &hPtr);
     * }
     *
     * can produce long runs of precisely ordered table entries when the
     * commands in the namespace are declared in a sorted fashion,  which is an
     * ordering some people like, and the hashing functions or the command
     * names themselves are fairly unfortunate. Filling from both ends means
     * that it requires active malice, and probably a debugger, to get qsort()
     * to have awful runtime behaviour.
     */

    i = 0;
    j = hash->numEntries;
    hPtr = Tcl_FirstHashEntry(hash, &search);
    while (hPtr != NULL) {
	ensemblePtr->subcommandArrayPtr[i++] = (char *)
		Tcl_GetHashKey(hash, hPtr);
	hPtr = Tcl_NextHashEntry(&search);
	if (hPtr == NULL) {
	    break;
	}
	ensemblePtr->subcommandArrayPtr[--j] = (char *)
		Tcl_GetHashKey(hash, hPtr);
	hPtr = Tcl_NextHashEntry(&search);
    }
    if (hash->numEntries > 1) {
	qsort(ensemblePtr->subcommandArrayPtr, hash->numEntries,
		sizeof(char *), NsEnsembleStringOrder);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * NsEnsembleStringOrder --
 *
 *	Helper to for use with qsort() that compares two array entries that
 *	contain string pointers.
 *
 * Results:
 *	-1 if the first string is smaller, 1 if the second string is smaller,
 *	and 0 if they are equal.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
NsEnsembleStringOrder(
    const void *strPtr1,	/* Points to first array entry */
    const void *strPtr2)	/* Points to second array entry */
{
    return strcmp(*(const char **)strPtr1, *(const char **)strPtr2);
}

/*
 *----------------------------------------------------------------------
 *
 * FreeEnsembleCmdRep --
 *
 *	Destroys the internal representation of a Tcl_Obj that has been
 *	holding information about a command in an ensemble.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is deallocated. If this held the last reference to a
 *	namespace's main structure, that main structure will also be
 *	destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
FreeEnsembleCmdRep(
    Tcl_Obj *objPtr)
{
    EnsembleCmdRep *ensembleCmd;

    ECRGetInternalRep(objPtr, ensembleCmd);
    TclCleanupCommandMacro(ensembleCmd->token);
    if (ensembleCmd->fix) {
	Tcl_DecrRefCount(ensembleCmd->fix);
    }
    Tcl_Free(ensembleCmd);
}

/*
 *----------------------------------------------------------------------
 *
 * DupEnsembleCmdRep --
 *
 *	Makes one Tcl_Obj into a copy of another that is a subcommand of an
 *	ensemble.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is allocated, and the namespace that the ensemble is built on
 *	top of gains another reference.
 *
 *----------------------------------------------------------------------
 */

static void
DupEnsembleCmdRep(
    Tcl_Obj *objPtr,
    Tcl_Obj *copyPtr)
{
    EnsembleCmdRep *ensembleCmd;
    EnsembleCmdRep *ensembleCopy = (EnsembleCmdRep *)
	    Tcl_Alloc(sizeof(EnsembleCmdRep));

    ECRGetInternalRep(objPtr, ensembleCmd);
    ECRSetInternalRep(copyPtr, ensembleCopy);

    ensembleCopy->epoch = ensembleCmd->epoch;
    ensembleCopy->token = ensembleCmd->token;
    ensembleCopy->token->refCount++;
    ensembleCopy->fix = ensembleCmd->fix;
    if (ensembleCopy->fix) {
	Tcl_IncrRefCount(ensembleCopy->fix);
    }
    ensembleCopy->hPtr = ensembleCmd->hPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCompileEnsemble --
 *
 *	Procedure called to compile an ensemble command. Note that most
 *	ensembles are not compiled, since modifying a compiled ensemble causes
 *	a invalidation of all existing bytecode (expensive!) which is not
 *	normally warranted.
 *
 * Results:
 *	Returns TCL_OK for a successful compile. Returns TCL_ERROR to defer
 *	evaluation to runtime.
 *
 * Side effects:
 *	Instructions are added to envPtr to execute the subcommands of the
 *	ensemble at runtime if a compile-time mapping is possible.
 *
 *----------------------------------------------------------------------
 */

int
TclCompileEnsemble(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;
    Tcl_Token *tokenPtr = TokenAfter(parsePtr->tokenPtr);
    Tcl_Obj *mapObj, *subcmdObj, *targetCmdObj, *listObj, **elems;
    Tcl_Obj *replaced, *replacement;
    Tcl_Command ensemble = (Tcl_Command) cmdPtr;
    Command *oldCmdPtr = cmdPtr, *newCmdPtr;
    int result, flags = 0, depth = 1, invokeAnyway = 0;
    int ourResult = TCL_ERROR;
    Tcl_Size i, len, numBytes;
    const char *word;

    TclNewObj(replaced);
    Tcl_IncrRefCount(replaced);
    if (parsePtr->numWords <= depth) {
	goto tryCompileToInv;
    }
    if (tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	/*
	 * Too hard.
	 */

	goto tryCompileToInv;
    }

    /*
     * This is where we return to if we are parsing multiple nested compiled
     * ensembles. [info object] is such a beast.
     */

  checkNextWord:
    word = tokenPtr[1].start;
    numBytes = tokenPtr[1].size;

    /*
     * There's a sporting chance we'll be able to compile this. But now we
     * must check properly. To do that, check that we're compiling an ensemble
     * that has a compilable command as its appropriate subcommand.
     */

    if (Tcl_GetEnsembleMappingDict(NULL, ensemble, &mapObj) != TCL_OK
	    || mapObj == NULL) {
	/*
	 * Either not an ensemble or a mapping isn't installed. Crud. Too hard
	 * to proceed.
	 */

	goto tryCompileToInv;
    }

    /*
     * Also refuse to compile anything that uses a formal parameter list for
     * now, on the grounds that it is too complex.
     */

    if (Tcl_GetEnsembleParameterList(NULL, ensemble, &listObj) != TCL_OK
	    || listObj != NULL) {
	/*
	 * Figuring out how to compile this has become too much. Bail out.
	 */

	goto tryCompileToInv;
    }

    /*
     * Next, get the flags. We need them on several code paths so that we can
     * know whether we're to do prefix matching.
     */

    (void) Tcl_GetEnsembleFlags(NULL, ensemble, &flags);

    /*
     * Check to see if there's also a subcommand list; must check to see if
     * the subcommand we are calling is in that list if it exists, since that
     * list filters the entries in the map.
     */

    (void) Tcl_GetEnsembleSubcommandList(NULL, ensemble, &listObj);
    if (listObj != NULL) {
	Tcl_Size sclen;
	const char *str;
	Tcl_Obj *matchObj = NULL;

	if (TclListObjGetElements(NULL, listObj, &len, &elems) != TCL_OK) {
	    goto tryCompileToInv;
	}
	for (i=0 ; i<len ; i++) {
	    str = TclGetStringFromObj(elems[i], &sclen);
	    if ((sclen == numBytes) && !memcmp(word, str, numBytes)) {
		/*
		 * Exact match! Excellent!
		 */

		result = Tcl_DictObjGet(NULL, mapObj,elems[i], &targetCmdObj);
		if (result != TCL_OK || targetCmdObj == NULL) {
		    goto tryCompileToInv;
		}
		replacement = elems[i];
		goto doneMapLookup;
	    }

	    /*
	     * Check to see if we've got a prefix match. A single prefix match
	     * is fine, and allows us to refine our dictionary lookup, but
	     * multiple prefix matches is a Bad Thing and will prevent us from
	     * making progress. Note that we cannot do the lookup immediately
	     * in the prefix case; might be another entry later in the list
	     * that causes things to fail.
	     */

	    if ((flags & TCL_ENSEMBLE_PREFIX)
		    && strncmp(word, str, numBytes) == 0) {
		if (matchObj != NULL) {
		    goto tryCompileToInv;
		}
		matchObj = elems[i];
	    }
	}
	if (matchObj == NULL) {
	    goto tryCompileToInv;
	}
	result = Tcl_DictObjGet(NULL, mapObj, matchObj, &targetCmdObj);
	if (result != TCL_OK || targetCmdObj == NULL) {
	    goto tryCompileToInv;
	}
	replacement = matchObj;
    } else {
	Tcl_DictSearch s;
	int done, matched;
	Tcl_Obj *tmpObj;

	/*
	 * No map, so check the dictionary directly.
	 */

	TclNewStringObj(subcmdObj, word, numBytes);
	result = Tcl_DictObjGet(NULL, mapObj, subcmdObj, &targetCmdObj);
	if (result == TCL_OK && targetCmdObj != NULL) {
	    /*
	     * Got it. Skip the fiddling around with prefixes.
	     */

	    replacement = subcmdObj;
	    goto doneMapLookup;
	}
	TclDecrRefCount(subcmdObj);

	/*
	 * We've not literally got a valid subcommand. But maybe we have a
	 * prefix. Check if prefix matches are allowed.
	 */

	if (!(flags & TCL_ENSEMBLE_PREFIX)) {
	    goto tryCompileToInv;
	}

	/*
	 * Iterate over the keys in the dictionary, checking to see if we're a
	 * prefix.
	 */

	Tcl_DictObjFirst(NULL, mapObj, &s, &subcmdObj, &tmpObj, &done);
	matched = 0;
	replacement = NULL;		/* Silence, fool compiler! */
	while (!done) {
	    if (strncmp(TclGetString(subcmdObj), word, numBytes) == 0) {
		if (matched++) {
		    /*
		     * Must have matched twice! Not unique, so no point
		     * looking further.
		     */

		    break;
		}
		replacement = subcmdObj;
		targetCmdObj = tmpObj;
	    }
	    Tcl_DictObjNext(&s, &subcmdObj, &tmpObj, &done);
	}
	Tcl_DictObjDone(&s);

	/*
	 * If we have anything other than a single match, we've failed the
	 * unique prefix check.
	 */

	if (matched != 1) {
	    invokeAnyway = 1;
	    goto tryCompileToInv;
	}
    }

    /*
     * OK, we definitely map to something. But what?
     *
     * The command we map to is the first word out of the map element. Note
     * that we also reject dealing with multi-element rewrites if we are in a
     * safe interpreter, as there is otherwise a (highly gnarly!) way to make
     * Tcl crash open to exploit.
     */

  doneMapLookup:
    Tcl_ListObjAppendElement(NULL, replaced, replacement);
    if (TclListObjGetElements(NULL, targetCmdObj, &len, &elems) != TCL_OK) {
	goto tryCompileToInv;
    } else if (len != 1) {
	/*
	 * Note that at this point we know we can't issue any special
	 * instruction sequence as the mapping isn't one that we support at
	 * the compiled level.
	 */

	goto cleanup;
    }
    targetCmdObj = elems[0];

    oldCmdPtr = cmdPtr;
    Tcl_IncrRefCount(targetCmdObj);
    newCmdPtr = (Command *) Tcl_GetCommandFromObj(interp, targetCmdObj);
    TclDecrRefCount(targetCmdObj);
    if (newCmdPtr == NULL || (Tcl_IsSafe(interp) && !cmdPtr->compileProc)
	    || newCmdPtr->nsPtr->flags & NS_SUPPRESS_COMPILATION
	    || newCmdPtr->flags & CMD_HAS_EXEC_TRACES
	    || ((Interp *) interp)->flags & DONT_COMPILE_CMDS_INLINE) {
	/*
	 * Maps to an undefined command or a command without a compiler.
	 * Cannot compile.
	 */
	goto cleanup;
    }
    cmdPtr = newCmdPtr;
    depth++;

    /*
     * See whether we have a nested ensemble. If we do, we can go round the
     * mulberry bush again, consuming the next word.
     */

    if (cmdPtr->compileProc == TclCompileEnsemble) {
	tokenPtr = TokenAfter(tokenPtr);
	if ((int)parsePtr->numWords < depth + 1
		|| tokenPtr->type != TCL_TOKEN_SIMPLE_WORD) {
	    /*
	     * Too hard because the user has done something unpleasant like
	     * omitting the sub-ensemble's command name or used a non-constant
	     * name for a sub-ensemble's command name; we respond by bailing
	     * out completely (this is a rare case). [Bug 6d2f249a01]
	     */

	    goto cleanup;
	}
	ensemble = (Tcl_Command) cmdPtr;
	goto checkNextWord;
    }

    /*
     * Now that the mapping process is done we actually try to compile.
     * If there is a subcommand compiler and that successfully produces code,
     * we'll use that. Otherwise, we fall back to generating opcodes to do the
     * invoke at runtime.
     */

    invokeAnyway = 1;
    if (TCL_OK == TclAttemptCompileProc(interp, parsePtr, depth, cmdPtr,
	    envPtr)) {
	ourResult = TCL_OK;
	goto cleanup;
    }

    /*
     * Throw out any line information generated by the failed compile attempt.
     */

    ClearFailedCompile(envPtr);

    /*
     * Failed to do a full compile for some reason. Try to do a direct invoke
     * instead of going through the ensemble lookup process again.
     */

  tryCompileToInv:
    if (depth < 250) {
	if (depth > 1) {
	    if (!invokeAnyway) {
		cmdPtr = oldCmdPtr;
		depth--;
	    }
	}
	/*
	 * The length of the "replaced" list must be depth-1.  Trim back
	 * any extra elements that might have been appended by failing
	 * pathways above.
	 */
	(void) Tcl_ListObjReplace(NULL, replaced, depth-1, LIST_MAX, 0, NULL);

	/*
	 * TODO: Reconsider whether we ought to call CompileToInvokedCommand()
	 * when depth==1.  In that case we are choosing to emit the
	 * INST_INVOKE_REPLACE bytecode when there is in fact no replacing
	 * to be done.  It would be equally functional and presumably more
	 * performant to fall through to cleanup below, return TCL_ERROR,
	 * and let the compiler harness emit the INST_INVOKE_STK
	 * implementation for us.
	 */

	CompileToInvokedCommand(interp, parsePtr, replaced, cmdPtr, envPtr);
	ourResult = TCL_OK;
    }

    /*
     * Release the memory we allocated. If we've got here, we've either done
     * something useful or we're in a case that we can't compile at all and
     * we're just giving up.
     */

  cleanup:
    Tcl_DecrRefCount(replaced);
    return ourResult;
}

int
TclAttemptCompileProc(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Tcl_Size depth,
    Command *cmdPtr,
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;
    int result;
    Tcl_Size i;
    Tcl_Token *saveTokenPtr = parsePtr->tokenPtr;
    Tcl_Size savedStackDepth = envPtr->currStackDepth;
    Tcl_Size savedCodeNext = CurrentOffset(envPtr);
    Tcl_Size savedAuxDataArrayNext = envPtr->auxDataArrayNext;
    Tcl_Size savedExceptArrayNext = envPtr->exceptArrayNext;
#ifdef TCL_COMPILE_DEBUG
    Tcl_Size savedExceptDepth = envPtr->exceptDepth;
#endif

    if (cmdPtr->compileProc == NULL) {
	return TCL_ERROR;
    }

    /*
     * Advance parsePtr->tokenPtr so that it points at the last subcommand.
     * This will be wrong but it will not matter, and it will put the
     * tokens for the arguments in the right place without the need to
     * allocate a synthetic Tcl_Parse struct or copy tokens around.
     */

    for (i = 0; i < depth - 1; i++) {
	parsePtr->tokenPtr = TokenAfter(parsePtr->tokenPtr);
    }
    parsePtr->numWords -= (depth - 1);

    /*
     * Shift the line information arrays to account for different word
     * index values.
     */

    ExtCmdLocation.line += (depth - 1);
    ExtCmdLocation.next += (depth - 1);

    /*
     * Hand off compilation to the subcommand compiler. At last!
     */

    result = cmdPtr->compileProc(interp, parsePtr, cmdPtr, envPtr);

    /*
     * Undo the shift.
     */

    ExtCmdLocation.line -= (depth - 1);
    ExtCmdLocation.next -= (depth - 1);

    parsePtr->numWords += (depth - 1);
    parsePtr->tokenPtr = saveTokenPtr;

    /*
     * If our target failed to compile, revert any data from failed partial
     * compiles.  Note that envPtr->numCommands need not be checked because
     * we avoid compiling subcommands that recursively call TclCompileScript().
     */

#ifdef TCL_COMPILE_DEBUG
    if (envPtr->exceptDepth != savedExceptDepth) {
	Tcl_Panic("ExceptionRange Starts and Ends do not balance");
    }
#endif

    if (result != TCL_OK) {
	ExceptionAux *auxPtr = envPtr->exceptAuxArrayPtr;

	for (i = 0; i < savedExceptArrayNext; i++) {
	    while (auxPtr->numBreakTargets > 0
		    && (Tcl_Size) auxPtr->breakTargets[auxPtr->numBreakTargets - 1]
		    >= savedCodeNext) {
		auxPtr->numBreakTargets--;
	    }
	    while (auxPtr->numContinueTargets > 0
		    && (Tcl_Size) auxPtr->continueTargets[auxPtr->numContinueTargets - 1]
		    >= savedCodeNext) {
		auxPtr->numContinueTargets--;
	    }
	    auxPtr++;
	}
	envPtr->exceptArrayNext = savedExceptArrayNext;

	if (savedAuxDataArrayNext != envPtr->auxDataArrayNext) {
	    AuxData *auxDataPtr = envPtr->auxDataArrayPtr;
	    AuxData *auxDataEnd = auxDataPtr;

	    auxDataPtr += savedAuxDataArrayNext;
	    auxDataEnd += envPtr->auxDataArrayNext;

	    while (auxDataPtr < auxDataEnd) {
		if (auxDataPtr->type->freeProc != NULL) {
		    auxDataPtr->type->freeProc(auxDataPtr->clientData);
		}
		auxDataPtr++;
	    }
	    envPtr->auxDataArrayNext = savedAuxDataArrayNext;
	}
	envPtr->currStackDepth = savedStackDepth;
	envPtr->codeNext = envPtr->codeStart + savedCodeNext;
#ifdef TCL_COMPILE_DEBUG
    } else {
	/*
	 * Confirm that the command compiler generated a single value on
	 * the stack as its result. This is only done in debugging mode,
	 * as it *should* be correct and normal users have no reasonable
	 * way to fix it anyway.
	 */

	int diff = envPtr->currStackDepth - savedStackDepth;

	if (diff != 1) {
	    Tcl_Panic("bad stack adjustment when compiling"
		    " %.*s (was %d instead of 1)", (int)parsePtr->tokenPtr->size,
		    parsePtr->tokenPtr->start, diff);
	}
#endif
    }

    return result;
}

/*
 * How to compile a subcommand to a _replacing_ invoke of its implementation
 * command.
 */

static void
CompileToInvokedCommand(
    Tcl_Interp *interp,
    Tcl_Parse *parsePtr,
    Tcl_Obj *replacements,
    Command *cmdPtr,
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    DefineLineInformation;
    Tcl_Token *tokPtr;
    Tcl_Obj *objPtr, **words;
    int cmdLit, extraLiteralFlags = LITERAL_CMD_NAME;
    Tcl_Size i, numWords;

    /*
     * Push the words of the command. Take care; the command words may be
     * scripts that have backslashes in them, and [info frame 0] can see the
     * difference. Hence the call to TclContinuationsEnterDerived...
     */

    TclListObjGetElements(NULL, replacements, &numWords, &words);
    for (i = 0, tokPtr = parsePtr->tokenPtr; i < parsePtr->numWords;
	    i++, tokPtr = TokenAfter(tokPtr)) {
	if (i > 0 && i <= numWords) {
	    PUSH_OBJ(		words[i - 1]);
	    continue;
	}

	SetLineInformation(i);
	if (tokPtr->type == TCL_TOKEN_SIMPLE_WORD) {
	    int literal = PUSH_SIMPLE_TOKEN(tokPtr);

	    if (envPtr->clNext) {
		TclContinuationsEnterDerived(
			TclFetchLiteral(envPtr, literal),
			tokPtr[1].start - envPtr->source,
			envPtr->clNext);
	    }
	} else {
	    CompileTokens(envPtr, tokPtr, interp);
	}
    }

    /*
     * Push the name of the command we're actually dispatching to as part of
     * the implementation.
     */

    TclNewObj(objPtr);
    Tcl_GetCommandFullName(interp, (Tcl_Command) cmdPtr, objPtr);
    if ((cmdPtr != NULL) && (cmdPtr->flags & CMD_VIA_RESOLVER)) {
	extraLiteralFlags |= LITERAL_UNSHARED;
    }
    cmdLit = PUSH_OBJ_FLAGS(objPtr, extraLiteralFlags);
    TclSetCmdNameObj(interp, TclFetchLiteral(envPtr, cmdLit), cmdPtr);

    /*
     * Do the replacing dispatch.
     */

    INVOKE41(			INVOKE_REPLACE, parsePtr->numWords, numWords+1);
}

/*
 * Helpers that do issuing of instructions for commands that "don't have
 * compilers" (well, they do; these). They all work by just generating base
 * code to invoke the command; they're intended for ensemble subcommands so
 * that the costs of INST_INVOKE_REPLACE can be avoided where we can work out
 * that they're not needed.
 *
 * Note that these are NOT suitable for commands where there's an argument
 * that is a script, as an [info level] or [info frame] in the inner context
 * can see the difference.
 */

static int
CompileBasicNArgCommand(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    Tcl_Obj *objPtr;

    TclNewObj(objPtr);
    Tcl_IncrRefCount(objPtr);
    Tcl_GetCommandFullName(interp, (Tcl_Command) cmdPtr, objPtr);
    TclCompileInvocation(interp, parsePtr->tokenPtr, objPtr,
	    parsePtr->numWords, envPtr);
    Tcl_DecrRefCount(objPtr);
    return TCL_OK;
}

int
TclCompileBasic0ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 1) {
	return TCL_ERROR;
    }

    return CompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic1ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    return CompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic2ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    return CompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic3ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 4) {
	return TCL_ERROR;
    }

    return CompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic0Or1ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 1 && parsePtr->numWords != 2) {
	return TCL_ERROR;
    }

    return CompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic1Or2ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 2 && parsePtr->numWords != 3) {
	return TCL_ERROR;
    }

    return CompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic2Or3ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords != 3 && parsePtr->numWords != 4) {
	return TCL_ERROR;
    }

    return CompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic0To2ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords < 1 || parsePtr->numWords > 3) {
	return TCL_ERROR;
    }

    return CompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasic1To3ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if (parsePtr->numWords < 2 || parsePtr->numWords > 4) {
	return TCL_ERROR;
    }

    return CompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasicMin0ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if ((int)parsePtr->numWords < 1) {
	return TCL_ERROR;
    }

    return CompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasicMin1ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if ((int)parsePtr->numWords < 2) {
	return TCL_ERROR;
    }

    return CompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

int
TclCompileBasicMin2ArgCmd(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Tcl_Parse *parsePtr,	/* Points to a parse structure for the command
				 * created by Tcl_ParseCommand. */
    Command *cmdPtr,		/* Points to definition of command being
				 * compiled. */
    CompileEnv *envPtr)		/* Holds resulting instructions. */
{
    /*
     * Verify that the number of arguments is correct; that's the only case
     * that we know will avoid the call to Tcl_WrongNumArgs() at invoke time,
     * which is the only code that sees the shenanigans of ensemble dispatch.
     */

    if ((int)parsePtr->numWords < 3) {
	return TCL_ERROR;
    }

    return CompileBasicNArgCommand(interp, parsePtr, cmdPtr, envPtr);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
