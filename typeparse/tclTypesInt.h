/*
 * tclTypesInt.h --
 *
 *	Interface INTERNE entre tclTypes.c (table de types, interning,
 *	validation, cast) et tclTypeParse.c (analyseur et compilateur des
 *	expressions de types). Rien ici n'est destiné au reste du noyau.
 */

#ifndef TCLTYPESINT_H
#define TCLTYPESINT_H

#include "tclTypes.h"

/*
 * Types paramétrés : type def matrix({int}i, {int}j = 3) {list(i).list(j).int}
 *
 * TypeParam décrit un paramètre (déclaration) et, dans une instanciation,
 * la valeur de son argument. TypeGeneric est le type paramétré lui-même ;
 * chaque instance (matrix(3,3)) est un type nominal ordinaire, interné par
 * (nom, arguments).
 */

typedef struct TypeParam {
    Tcl_Obj *name;		/* identifiant */
    int isType;			/* 1 : {type}T, l'argument est un type */
    int isCmd;			/* 1 : {cmd}c, l'argument est un nom de commande
				 * pleinement qualifié (::ns::cmd) */
    TclType *type;		/* sinon : type de l'argument (int, string...) */
    char *defText;		/* texte du défaut ("= ..."), ou NULL */
    Tcl_Obj *value;		/* instance : texte canonique de l'argument */
    TclType *typeValue;		/* instance : l'argument, si isType */
} TypeParam;

typedef struct TypeParams {
    TypeParam *p;
    Tcl_Size n;			/* nombre de paramètres déjà liés */
} TypeParams;

typedef struct TypeGeneric {
    char *name;
    Tcl_Size nparams;
    TypeParam *params;
    char *body;			/* texte du type, avec les paramètres libres */
    Tcl_Size bodyLen;
    int alias;			/* 1 : type alias paramétré : l'instance est le type
				 * du corps lui-même (structurel), pas un type
				 * nominal de plus */
    int bits;			/* 1 : bitarray, l'instance reçoit bitWidth et
				 * bitMsb0 et binEnd */
} TypeGeneric;

typedef struct TypeData {
    Tcl_HashTable names;	/* nom -> TclType* (NULL pour "array") */
    Tcl_HashTable generics;	/* nom -> TypeGeneric* */
    Tcl_HashTable instances;	/* "nom, 0x01, arg, 0x01, arg" -> TclType* */
    char **instLog;		/* clés créées par l'instanciation en cours,
				 * pour les retirer en cas d'échec */
    Tcl_Size nInstLog, capInstLog;
    int instDepth;
    Tcl_HashTable canon;	/* nom canonique -> type anonyme interné */
    TclType *linkStack[32];	/* liens en cours de développement (BinRepr) */
    int nLinkStack;
    Tcl_HashTable converts;	/* "De\1Vers" -> lambda (Tcl_Obj*, compté) */
    Tcl_HashTable derefs;	/* type deref : TclType* -> son type sans liens
				 * (produits et sommes, y compris récursifs) */
    TclType **all;
    Tcl_Size nAll, capAll;
    TclType *tInt, *tDouble, *tString, *tList, *tDict, *tVar;
    TclType *tAt;		/* constructeur at(EXPR).T (caché : pas un nom) */
    TclType *tLink;		/* constructeur *(...).T (caché : pas un nom) */
    void **blobs;		/* décorations binaires (TclBinDyn, textes),
				 * partagées par les types, libérées à la fin */
    Tcl_Size nBlobs, capBlobs;
} TypeData;

#define ERR(interp, code, ...) \
    do { \
	if ((interp) != NULL) { \
	    Tcl_SetObjResult((interp), Tcl_ObjPrintf(__VA_ARGS__)); \
	    Tcl_SetErrorCode((interp), "TCL", "TYPE", code, (char *) NULL); \
	} \
    } while (0)

/*
 * Environnement de compilation d'un type : l'analogue de CompileEnv pour
 * TclCompileExpr. Il porte le contexte (le type en cours de définition) et
 * reçoit le résultat.
 */

typedef struct TypeEnv {
    TypeData *d;		/* table de types de l'interpréteur */
    const TypeParams *params;	/* paramètres liés (corps d'un type paramétré
				 * ou arguments d'une instanciation), ou NULL */
    const char *selfName;	/* type en cours de définition, ou NULL */
    TclType *self;
    TclType *into;		/* type nominal à remplir (racine), ou NULL */
    TclType *result;		/* résultat de la compilation */
    void *stack;		/* pile d'opérandes (privée à tclTypeParse.c) */
    Tcl_Size nStack, capStack;
} TypeEnv;

/* tclTypeParse.c */
/* Un mot de valeur : "texte", {texte}, mot nu, ou $param. */
int	TclTypeParseWord(Tcl_Interp *interp, const TypeEnv *envPtr,
	    const char **pp, const char *end, int isValue, Tcl_Obj **objPtr);
int	TclCompileType(Tcl_Interp *interp, const char *script,
	    Tcl_Size numBytes, TypeEnv *envPtr);
int	TclTypeIsNameChar(int c);
int	TclTypeIsCmdName(const char *s);
/* La ")" qui ferme une "(" déjà lue, ou NULL. */
const char *TclTypeFindArgsEnd(const char *p, const char *limit);

/* tclTypeGeneric.c : types paramétrés */
const TypeParam *TclTypeFindParam(const TypeParams *ps, const char *name,
	    Tcl_Size len);
TypeGeneric *TclTypesFindGeneric(TypeData *d, const char *w, Tcl_Size len);
/* argText == NULL : aucun argument (tous les paramètres ont un défaut). */
TclType *TclTypesInstantiate(Tcl_Interp *interp, TypeData *d, TypeGeneric *g,
	    const char *argText, Tcl_Size argLen, const TypeEnv *parent);
int	TclTypesDefineGeneric(Tcl_Interp *interp, TypeData *d,
	    const char *spec, Tcl_Size specLen, const char *body,
	    Tcl_Size bodyLen, int alias);
void	TclTypesFreeGenerics(TypeData *d);
/* Marque le type paramétré NAME : 1 bitarray. */
void	TclTypesMarkBits(TypeData *d, const char *name, int code);
/* Octets du terminateur NUL de string(enc,z) pour un encodage Tcl. */
int	TclTypesNulWidth(const char *tclName);

/* tclTypes.c : services utilisés par le compilateur de types */
TclType *TclTypesNewType(TypeData *d, TclTypeKind kind, const char *name);
int	TclTypesInhabited(const TclType *tp, const TclType **stack, int depth);
void	TclTypesCopyInto(TclType *into, const TclType *res);
int	TclTypesCheckNewName(Tcl_Interp *interp, TypeData *d, const char *name);
TclType *TclTypesResolve(Tcl_Interp *interp, TypeData *d,
	    const char *selfName, TclType *self, const char *w, Tcl_Size len);
/* int(u,16,be), double(32,le), string(utf-8,8) : le type de base (int,
 * double, string) avec sa représentation binaire. */
TclType *TclTypesBinaryType(Tcl_Interp *interp, TypeData *d, const char *base,
	    Tcl_Size baseLen, const char *argText, Tcl_Size argLen,
	    const TypeParams *params);
/* Argument de taille, de nombre ou de position : N, z, un préfixe (type
 * entier non signé) ou une expression avec $champ. */
int	TclTypesParseSize(Tcl_Interp *interp, TypeData *d, const char *what,
	    const char *text, Tcl_Size len, int allowZ, const TypeParams *params,
	    Tcl_WideInt *nPtr, int *zPtr, TclBinDyn **dynPtr);
void	*TclTypesKeep(TypeData *d, void *blob);
char	*TclTypesKeepText(TypeData *d, const char *s, Tcl_Size len);
/* list(PRÉFIXE|$expr).T, at(EXPR).T, S($champ) : types décorés. */
TclType *TclTypesMakeListDyn(TypeData *d, TclType *elem, TclBinDyn *dyn);
TclType *TclTypesMakeAt(TypeData *d, TclType *elem, const char *expr);
TclType *TclTypesMakeLink(Tcl_Interp *interp, TypeData *d, TclType *elem,
	    TclBinLink *link);
TclType *TclTypesMakeTagged(Tcl_Interp *interp, TypeData *d, TclType *sum,
	    const char *field, int isConst);
/* list.T : liste de T, extensible. */
TclType *TclTypesMakeList(TypeData *d, TclType *elem);
TclType *TclTypesMakeListExplicit(TypeData *d, TclType *elem);
/* list(N).T : exactement N éléments (arity >= 0) ; arity < 0 : extensible. */
TclType *TclTypesMakeListN(TypeData *d, TclType *elem, Tcl_Size arity);
/* var.T : nom d'une variable dont la valeur est un T ; typed != 0 exige
 * que la variable soit typée. */
TclType *TclTypesMakeVar(TypeData *d, TclType *elem, int typed);
/* dict.T (clés quelconques) si keyed == 0, sinon dict(clés).T. Les clés
 * sont toujours du texte. Prend possession de keys. */
TclType *TclTypesMakeDict(TypeData *d, TclType *elem, int keyed,
	    TclDictKey *keys, Tcl_Size nkeys);
void	TclTypesFreeKeys(TclDictKey *keys, Tcl_Size n);
/* Prend possession de fields (même en cas d'échec). */
TclType *TclTypesFinish(Tcl_Interp *interp, TypeData *d, const char *selfName,
	    TclTypeKind kind, TclTypeField *fields, Tcl_Size n, TclType *into);
void	TclTypesFreeFields(TclTypeField *fields, Tcl_Size n);

#endif /* TCLTYPESINT_H */
