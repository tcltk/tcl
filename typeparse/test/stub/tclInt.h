/*
 * tclInt.h FACTICE : uniquement pour vérifier la syntaxe et exécuter le
 * banc de test. Ne contient que les noms que les sources utilisent.
 */
#ifndef STUB_TCLINT_H
#define STUB_TCLINT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

typedef ptrdiff_t Tcl_Size;
#define TCL_SIZE_MODIFIER "t"
#define TCL_AUTO_LENGTH ((Tcl_Size) -1)
typedef long long Tcl_WideInt;

#define TCL_OK		0
#define TCL_ERROR	1
#define TCL_CONTINUE	4
#define TCL_LEAVE_ERR_MSG 0x200
#define TCL_APPEND_VALUE 0x4
#define TCL_LIST_ELEMENT 0x8
#define TCL_STRING_KEYS	0
#define TCL_OBJTYPE_V0	0
#define TCL_NUMBER_INT	1
#define TCL_NUMBER_BIG	2
#define TCL_NUMBER_DOUBLE 3

typedef struct Tcl_Interp Tcl_Interp;
typedef struct Tcl_Obj Tcl_Obj;
typedef struct Tcl_ObjType Tcl_ObjType;
typedef union Tcl_ObjInternalRep {
    struct { void *ptr1; void *ptr2; } twoPtrValue;
} Tcl_ObjInternalRep;
struct Tcl_Obj {
    Tcl_Size refCount;
    char *bytes;
    Tcl_Size length;
    const Tcl_ObjType *typePtr;
    Tcl_ObjInternalRep internalRep;
    Tcl_Obj **elems;			/* (banc de test : listes) */
    Tcl_Size nelems, capelems;
    int listRep;			/* (banc de test) 1 : a une forme liste */
};

typedef void (Tcl_FreeInternalRepProc)(Tcl_Obj *objPtr);
typedef void (Tcl_DupInternalRepProc)(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr);
typedef void (Tcl_UpdateStringProc)(Tcl_Obj *objPtr);
typedef int (Tcl_SetFromAnyProc)(Tcl_Interp *interp, Tcl_Obj *objPtr);
struct Tcl_ObjType {
    const char *name;
    Tcl_FreeInternalRepProc *freeIntRepProc;
    Tcl_DupInternalRepProc *dupIntRepProc;
    Tcl_UpdateStringProc *updateStringProc;
    Tcl_SetFromAnyProc *setFromAnyProc;
    size_t version;
};
Tcl_ObjInternalRep *Tcl_FetchInternalRep(Tcl_Obj *objPtr, const Tcl_ObjType *t);
void	TclInvalidateStringRep(Tcl_Obj *objPtr);
void	Tcl_ResetResult(Tcl_Interp *interp);

#define Tcl_IncrRefCount(o)	((o)->refCount++)
void	Tcl_DecrRefCount(Tcl_Obj *objPtr);
Tcl_Obj *Tcl_NewObj(void);
#define TclNewObj(o)		((o) = Tcl_NewObj())
Tcl_Obj *Tcl_NewStringObj(const char *s, Tcl_Size len);
Tcl_Obj *Tcl_NewListObj(Tcl_Size n, Tcl_Obj *const *objv);
Tcl_Obj *Tcl_NewDictObj(void);
Tcl_Obj *Tcl_NewWideIntObj(Tcl_WideInt v);
Tcl_Obj *Tcl_NewDoubleObj(double v);
Tcl_Obj *Tcl_DuplicateObj(Tcl_Obj *objPtr);
void	Tcl_SetStringObj(Tcl_Obj *objPtr, const char *bytes, Tcl_Size length);
const char *Tcl_GetString(Tcl_Obj *objPtr);
const char *Tcl_GetStringFromObj(Tcl_Obj *objPtr, Tcl_Size *lenPtr);
void	Tcl_InitStringRep(Tcl_Obj *objPtr, const char *s, Tcl_Size len);
void	Tcl_StoreInternalRep(Tcl_Obj *objPtr, const Tcl_ObjType *t,
	    const Tcl_ObjInternalRep *ir);
int	Tcl_ListObjAppendElement(Tcl_Interp *interp, Tcl_Obj *list,
	    Tcl_Obj *elem);
int	Tcl_ListObjGetElements(Tcl_Interp *interp, Tcl_Obj *list,
	    Tcl_Size *objcPtr, Tcl_Obj ***objvPtr);
void	Tcl_AppendObjToObj(Tcl_Obj *objPtr, Tcl_Obj *app);
Tcl_Obj *Tcl_ObjPrintf(const char *fmt, ...);
void	Tcl_AppendPrintfToObj(Tcl_Obj *objPtr, const char *fmt, ...);
void	Tcl_SetObjResult(Tcl_Interp *interp, Tcl_Obj *objPtr);
Tcl_Obj *Tcl_GetObjResult(Tcl_Interp *interp);
void	Tcl_SetErrorCode(Tcl_Interp *interp, ...);
void	Tcl_WrongNumArgs(Tcl_Interp *interp, Tcl_Size objc,
	    Tcl_Obj *const objv[], const char *message);

char	*Tcl_Alloc(size_t n);
char	*Tcl_Realloc(void *p, size_t n);
char	*Tcl_AttemptAlloc(size_t n);
char	*Tcl_AttemptRealloc(void *p, size_t n);
void	Tcl_Free(void *p);

typedef struct Tcl_HashEntry Tcl_HashEntry;
typedef struct Tcl_HashTable { void *opaque; } Tcl_HashTable;
typedef struct Tcl_HashSearch { void *opaque; } Tcl_HashSearch;
void	Tcl_InitHashTable(Tcl_HashTable *t, int keyType);
void	Tcl_DeleteHashTable(Tcl_HashTable *t);
Tcl_HashEntry *Tcl_CreateHashEntry(Tcl_HashTable *t, const void *key,
	    int *newPtr);
Tcl_HashEntry *Tcl_FindHashEntry(Tcl_HashTable *t, const void *key);
Tcl_HashEntry *Tcl_FirstHashEntry(Tcl_HashTable *t, Tcl_HashSearch *s);
Tcl_HashEntry *Tcl_NextHashEntry(Tcl_HashSearch *s);
void	*Tcl_GetHashValue(Tcl_HashEntry *h);
void	Tcl_DeleteHashEntry(Tcl_HashEntry *h);
const char *Tcl_GetHashKey(Tcl_HashTable *t, Tcl_HashEntry *h);
void	Tcl_SetHashValue(Tcl_HashEntry *h, void *value);

typedef struct Tcl_DString {
    char *string;
    Tcl_Size length;
    Tcl_Size spaceAvl;
} Tcl_DString;
void	Tcl_DStringInit(Tcl_DString *ds);
void	Tcl_DStringAppend(Tcl_DString *ds, const char *s, Tcl_Size len);
void	Tcl_DStringFree(Tcl_DString *ds);
#define Tcl_DStringValue(ds)	((ds)->string)
#define Tcl_DStringLength(ds)	((ds)->length)

typedef struct mp_int { int dummy; } mp_int;
int	Tcl_InitBignumFromDouble(Tcl_Interp *interp, double d, mp_int *b);
Tcl_Obj *Tcl_NewBignumObj(mp_int *b);
int	Tcl_GetNumberFromObj(Tcl_Interp *interp, Tcl_Obj *o, void **cd,
	    int *typePtr);
int	Tcl_GetDoubleFromObj(Tcl_Interp *interp, Tcl_Obj *o, double *dPtr);

typedef struct Tcl_DictSearch { Tcl_Obj *dict; Tcl_Size index; } Tcl_DictSearch;
int	Tcl_DictObjFirst(Tcl_Interp *interp, Tcl_Obj *dict,
	    Tcl_DictSearch *s, Tcl_Obj **k, Tcl_Obj **v, int *done);
void	Tcl_DictObjNext(Tcl_DictSearch *s, Tcl_Obj **k, Tcl_Obj **v,
	    int *done);
void	Tcl_DictObjDone(Tcl_DictSearch *s);
int	Tcl_DictObjPut(Tcl_Interp *interp, Tcl_Obj *dict, Tcl_Obj *k,
	    Tcl_Obj *v);
int	Tcl_DictObjGet(Tcl_Interp *interp, Tcl_Obj *dict, Tcl_Obj *k,
	    Tcl_Obj **valuePtr);
int	Tcl_ListObjLength(Tcl_Interp *interp, Tcl_Obj *list, Tcl_Size *lenPtr);
int	Tcl_ListObjIndex(Tcl_Interp *interp, Tcl_Obj *list, Tcl_Size index,
	    Tcl_Obj **objPtr);
int	Tcl_ListObjReplace(Tcl_Interp *interp, Tcl_Obj *list, Tcl_Size first,
	    Tcl_Size count, Tcl_Size objc, Tcl_Obj *const objv[]);

int	Tcl_EvalObjEx(Tcl_Interp *interp, Tcl_Obj *objPtr, int flags);
#define TCL_SUBST_VARIABLES 4
#define TCL_SUBST_ALL 7
Tcl_Obj *Tcl_SubstObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int flags);
int	Tcl_ExprObj(Tcl_Interp *interp, Tcl_Obj *objPtr, Tcl_Obj **resultPtrPtr);

#ifndef TCL_EVAL_GLOBAL
#define TCL_EVAL_GLOBAL 0x20000
#endif
int	Tcl_EvalObjv(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[],
	    int flags);
void	*Tcl_GetAssocData(Tcl_Interp *interp, const char *key, void *procPtr);
void	Tcl_SetAssocData(Tcl_Interp *interp, const char *key,
	    void (*proc)(void *, Tcl_Interp *), void *clientData);
typedef int (Tcl_ObjCmdProc2)(void *clientData, Tcl_Interp *interp,
	    Tcl_Size objc, Tcl_Obj *const objv[]);
void	*Tcl_CreateObjCommand2(Tcl_Interp *interp, const char *name,
	    Tcl_ObjCmdProc2 *proc, void *clientData, void (*del)(void *));

/* ---- l'analyseur de Tcl (tclParse.c) -------------------------------- */

#define TCL_TOKEN_WORD		1
#define TCL_TOKEN_TEXT		2
#define TCL_TOKEN_BS		4
#define TCL_TOKEN_COMMAND	8
#define TCL_TOKEN_VARIABLE	16
#define TCL_TOKEN_ANNOT		512

typedef struct Tcl_Token {
    int type;
    const char *start;
    Tcl_Size size;
    Tcl_Size numComponents;
} Tcl_Token;

typedef struct Tcl_Parse {
    const char *term;
    int incomplete;
    int errorType;
    Tcl_Token *tokenPtr;
    Tcl_Size numTokens;
    Tcl_Size tokensAvailable;
    const char *string;
    const char *end;
    const char *commandStart;
    Tcl_Size commandSize;
    Tcl_Size numWords;
} Tcl_Parse;

void	TclParseInit(Tcl_Interp *interp, const char *start, Tcl_Size numBytes,
	    Tcl_Parse *parsePtr);
void	TclGrowParseTokenArray(Tcl_Parse *parsePtr, Tcl_Size append);
Tcl_Size TclParseAllWhiteSpace(const char *src, Tcl_Size numBytes);
int	Tcl_ParseQuotedString(Tcl_Interp *interp, const char *start,
	    Tcl_Size numBytes, Tcl_Parse *parsePtr, int append,
	    const char **termPtr);
int	Tcl_ParseBraces(Tcl_Interp *interp, const char *start,
	    Tcl_Size numBytes, Tcl_Parse *parsePtr, int append,
	    const char **termPtr);
int	TclWordKnownAtCompileTime(Tcl_Token *tokenPtr, Tcl_Obj *valuePtr);
void	Tcl_FreeParse(Tcl_Parse *parsePtr);
int	Tcl_ParseCommand(Tcl_Interp *interp, const char *start,
	    Tcl_Size numBytes, int nested, Tcl_Parse *parsePtr);
int	Tcl_ParseVarName(Tcl_Interp *interp, const char *start,
	    Tcl_Size numBytes, Tcl_Parse *parsePtr, int append);
/* banc de test : commandes de script enregistrées par les tests */
typedef int (MockCmdProc)(Tcl_Interp *interp, Tcl_Size argc, const char *argv[],
	    Tcl_Obj **resultPtr);
void	mock_register_command(const char *name, MockCmdProc *proc);

/* ---- variables (tclTypedVar.c seulement) ---------------------------- */

typedef struct Var {
    int flags;
    union { Tcl_Obj *objPtr; struct Var *linkPtr; void *ptr; } value;
} Var;
#define TclIsVarLink(v)			((v)->flags & 0x1)
#define TclIsVarArray(v)		((v)->flags & 0x2)
#define TclIsVarArrayElement(v)		((v)->flags & 0x4)
#define TclIsVarDirectReadable(v)	1
#define TclIsVarDirect(v)		1
#define TclIsVarDirectWritable(v)	1
#define TclIsVarDirectModifyable(v)	1
#define TclIsVarDirectUnsettable(v)	1
Tcl_Obj *TclPtrSetVarIdx(Tcl_Interp *interp, Var *varPtr, Var *arrayPtr,
	    Tcl_Obj *part1Ptr, Tcl_Obj *part2Ptr, Tcl_Obj *newValuePtr,
	    int flags, int index);
Var	*TclObjLookupVarEx(Tcl_Interp *interp, Tcl_Obj *part1Ptr,
	    Tcl_Obj *part2Ptr, int flags, const char *msg, int createPart1,
	    int createPart2, Var **arrayPtrPtr);
Tcl_Obj	*Tcl_ObjSetVar2(Tcl_Interp *interp, Tcl_Obj *part1Ptr,
	    Tcl_Obj *part2Ptr, Tcl_Obj *newValuePtr, int flags);
int	Tcl_SetObjCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc,
	    Tcl_Obj *const objv[]);

int	TclGetIntForIndexM(Tcl_Interp *interp, Tcl_Obj *objPtr, Tcl_Size endValue,
	    Tcl_Size *idxPtr);

#endif
