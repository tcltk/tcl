/*
 * mockvars.c --
 *
 *	Variables Tcl pour le banc de test, et implémentation de référence de
 *	l'interface TclVarRef* (voir tclTypes.h) : une table de variables
 *	globales et une pile d'appels dont chaque cadre a un numéro unique.
 *	Une référence à une variable locale ("frame:UID:nom") cesse d'être
 *	valable quand son cadre disparaît ; le numéro n'est jamais réutilisé,
 *	donc une référence périmée échoue au lieu de viser une autre variable.
 */

#include "tclInt.h"
#include "tclTypesInt.h"
#include <stdlib.h>

typedef struct MVar {
    char *name;
    Tcl_Obj *val;
    struct MVar *next;
} MVar;

typedef struct MFrame {
    int uid, alive;
    MVar *vars;
    struct MFrame *up, *all;
} MFrame;

static MVar *globals;
static MFrame *cur, *allFrames;
static int nextUid = 1;

void mock_vars_reset(void)
{
    globals = NULL;
    cur = NULL;
}

void mock_push_frame(void)
{
    MFrame *f = calloc(1, sizeof(*f));

    f->uid = nextUid++;
    f->alive = 1;
    f->up = cur;
    f->all = allFrames;
    allFrames = f;
    cur = f;
}

void mock_pop_frame(void)
{
    cur->alive = 0;
    cur = cur->up;
}

static MVar *Find(MVar *list, const char *name)
{
    for (; list; list = list->next) {
	if (strcmp(list->name, name) == 0) {
	    return list;
	}
    }
    return NULL;
}

static void Put(MVar **list, const char *name, Tcl_Obj *v)
{
    MVar *mv = Find(*list, name);

    if (mv == NULL) {
	mv = calloc(1, sizeof(*mv));
	mv->name = strdup(name);
	mv->next = *list;
	*list = mv;
    }
    Tcl_IncrRefCount(v);
    if (mv->val) {
	Tcl_DecrRefCount(mv->val);
    }
    mv->val = v;
}

void mock_set_global(const char *name, Tcl_Obj *v) { Put(&globals, name, v); }
void mock_set_local(const char *name, Tcl_Obj *v) { Put(&cur->vars, name, v); }

/* Résout "::x" ou "frame:UID:x" vers la liste et le nom. */
static int Locate(Tcl_Interp *interp, const char *canon, MVar ***listPtr,
	const char **namePtr, int *deadPtr)
{
    *deadPtr = 0;
    if (strncmp(canon, "::", 2) == 0) {
	*listPtr = &globals;
	*namePtr = canon + 2;
	return 1;
    }
    if (strncmp(canon, "frame:", 6) == 0) {
	char *e;
	long uid = strtol(canon + 6, &e, 10);
	MFrame *f;

	if (*e != ':') {
	    return 0;
	}
	for (f = allFrames; f; f = f->all) {
	    if (f->uid == uid) {
		*listPtr = &f->vars;
		*namePtr = e + 1;
		*deadPtr = !f->alive;
		return 1;
	    }
	}
    }
    return 0;
}

int TclVarRefCanon(Tcl_Interp *interp, Tcl_Obj *name, Tcl_Obj **canonPtr)
{
    const char *s = Tcl_GetString(name);
    char buf[512];

    if (strncmp(s, "::", 2) == 0 || strncmp(s, "frame:", 6) == 0) {
	*canonPtr = Tcl_NewStringObj(s, -1);
    } else if (cur != NULL && Find(cur->vars, s) != NULL) {
	snprintf(buf, sizeof buf, "frame:%d:%s", cur->uid, s);
	*canonPtr = Tcl_NewStringObj(buf, -1);
    } else {
	snprintf(buf, sizeof buf, "::%s", s);
	*canonPtr = Tcl_NewStringObj(buf, -1);
    }
    return TCL_OK;
}

int TclVarRefGet(Tcl_Interp *interp, const char *canon, Tcl_Obj **valuePtr)
{
    MVar **list, *mv;
    const char *name;
    int dead;

    if (!Locate(interp, canon, &list, &name, &dead)) {
	if (interp) Tcl_SetObjResult(interp, Tcl_ObjPrintf("bad variable link \"%s\"",
		canon));
	return TCL_ERROR;
    }
    if (dead) {
	if (interp) Tcl_SetObjResult(interp, Tcl_ObjPrintf("can't read \"%s\": the "
		"procedure call that owned this variable has returned", canon));
	return TCL_ERROR;
    }
    mv = Find(*list, name);
    if (mv == NULL) {
	if (interp) Tcl_SetObjResult(interp, Tcl_ObjPrintf("can't read \"%s\": no such "
		"variable", canon));
	return TCL_ERROR;
    }
    *valuePtr = mv->val;
    return TCL_OK;
}

TclType *TclVarRefType(Tcl_Interp *interp, const char *canon)
{
    Tcl_Obj *v;
    TclType *tp;

    if (TclVarRefGet(NULL, canon, &v) != TCL_OK) {
	return NULL;
    }
    return (TclAnnotInfo(v, NULL, &tp, NULL, NULL) == 1) ? tp : NULL;
}

/* Affectation : la contrainte de type de la variable, si elle est typée. */
int TclVarRefSet(Tcl_Interp *interp, const char *canon, Tcl_Obj *value)
{
    MVar **list;
    const char *name;
    int dead, variant = -1;
    Tcl_Obj *old, *c, *nv;
    TclType *tp;

    if (!Locate(interp, canon, &list, &name, &dead) || dead) {
	if (interp) Tcl_SetObjResult(interp, Tcl_ObjPrintf("can't set \"%s\": no such "
		"variable scope", canon));
	return TCL_ERROR;
    }
    if (Find(*list, name) != NULL && (old = Find(*list, name)->val,
	    TclAnnotInfo(old, NULL, &tp, NULL, NULL) == 1)) {
	if (TclAnnotInfo(value, &c, NULL, NULL, NULL) != 1) {
	    c = value;
	}
	c = Tcl_DuplicateObj(c);
	if (TclTypeComplete(interp, tp, c, -1, &variant) != TCL_OK) {
	    TclDiscard(c);
	    return TCL_ERROR;
	}
	nv = TclNewAnnotated(interp, tp, variant, c);
	TclDiscard(c);
    } else {
	nv = Tcl_DuplicateObj(value);
    }
    Put(list, name, nv);
    return TCL_OK;
}
