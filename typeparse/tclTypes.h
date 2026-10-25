/*
 * tclTypes.h --
 *
 *	Déclarations communes des valeurs annotées et des variables typées
 *	(Tcl 9.0), d'après la spécification « Valeurs annotées et variables
 *	typées pour Tcl 9.0 », révision 4 (syntaxe de types à opérateurs :
 *	, ; . =  voir tclTypeParse.c).
 *
 *	Squelette NON COMPILÉ contre le vrai Tcl 9.0 : les noms du noyau
 *	(TclObjLookupVarEx, TclInvalidateStringRep, TclIsVarLink, ...) et les
 *	signatures sont cités de mémoire. Voir la liste de vérification à la
 *	fin de tclTypedVar.c.
 */

#ifndef TCLTYPES_H
#define TCLTYPES_H

#include "tclInt.h"

/* Bit de Var.flags : à choisir parmi les bits LIBRES (vérifier tous les
 * VAR_* de tclInt.h). La valeur ci-dessous est un PLACEHOLDER. */
/* Version du prototype TIP 764 (typeparse/VERSION, maj.sh, et
 * tcl::unsupported::typesversion). */
#define TCL_TYPES_VERSION "v16b"

#ifndef VAR_TYPED
#define VAR_TYPED	0x400000
#endif

/* --------------------------------------------------------------------
 * Descripteurs de types
 * -------------------------------------------------------------------- */

typedef struct TclType TclType;

/* Une clé admise d'un dictionnaire contraint : dict(x, y?, z?=0). */
typedef struct TclDictKey {
    Tcl_Obj *name;		/* la clé (toujours du texte) */
    int optional;		/* 1 : peut manquer ("z?" ou "z?=D") */
    Tcl_Obj *defval;		/* valeur par défaut, ajoutée si la clé manque
				 * ("z?=D"), sinon NULL */
    Tcl_Obj *defcmd;		/* sinon : script dont le résultat est le défaut
				 * ("z?=[clock seconds]"), évalué à chaque
				 * construction ; NULL le plus souvent */
} TclDictKey;

typedef enum {
    TCLT_INT, TCLT_DOUBLE, TCLT_STRING,
    TCLT_LIST, TCLT_DICT, TCLT_PRODUCT, TCLT_SUM, TCLT_VAR
} TclTypeKind;

/* Représentation binaire d'un type (int(u|s,n), double(n), string(enc,N|z),
 * bitarray(n)) : ce que `type binary` décrit, lit et écrit. */
enum {
    TCLBIN_NONE = 0, TCLBIN_UINT, TCLBIN_SINT, TCLBIN_FLOAT, TCLBIN_BYTES,
    TCLBIN_STR, TCLBIN_CSTRING
};
/* Boutisme d'un type binaire (TclType.binEnd). */
enum {
    TCLBIN_AUTO = 0,		/* celui de l'appel (option -endian) */
    TCLBIN_LE, TCLBIN_BE
};

/* Taille, nombre ou position qui dépend de la valeur lue ailleurs (niveau 2 de
 * `type binary`) : un préfixe, ou une expression Tcl avec des $champ. */
enum {
    TCLDYN_PREFIX = 1,		/* string(utf-8,int(u,16,be)), list(int(u,8)).T */
    TCLDYN_EXPR			/* string(raw,$len), list($n).T, at($off).T */
};

typedef struct TclBinDyn {
    int kind;			/* TCLDYN_PREFIX ou TCLDYN_EXPR */
    struct TclType *pre;	/* PREFIX : type entier non signé du préfixe */
    char *expr;			/* EXPR : expression Tcl (texte) */
} TclBinDyn;

/* *(ORIGINE, REPR ?, options?).T : un lien, c'est-à-dire un entier REPR qui
 * désigne, depuis ORIGINE, la valeur de type T (voir CONCEPTION-lien.md). */
typedef struct TclBinLink {
    char *origin;		/* "//" (début des données), "./" (début de la
				 * structure englobante), "../", "../../"...,
				 * "." (le champ lui-même) */
    struct TclType *repr;	/* type entier de l'adresse stockée */
    char *nullVal;		/* texte de la valeur nulle, ou NULL */
    int opaque;			/* réservé : la valeur reste l'entier */
    struct TclType *ref;	/* type de l'adresse de frontière (-linkdepth) :
				 * un entier nominal "&" + nom du lien */
    struct TclType *target;	/* le type désigné (peut être lui-même un lien) */
} TclBinLink;

typedef struct TclTypeField {
    Tcl_Obj *name;		/* champ ou variante ; NULL si sans nom */
    TclType *type;		/* NULL pour une variante littérale */
    Tcl_Obj *literal;		/* valeur d'une variante littérale, sinon NULL */
    Tcl_Obj *defval;		/* valeur par défaut (opérateur "="), sinon NULL */
    Tcl_Obj *defcmd;		/* ou script dont le résultat est le défaut,
				 * {int}t = [clock seconds] : évalué à chaque
				 * construction d'une valeur (jamais ensemble
				 * avec defval) */
} TclTypeField;

struct TclType {
    TclTypeKind kind;
    char *name;			/* "int", "pixel", ou nom canonique anonyme */
    int nominal;		/* défini par "type def" */
    int reserved;		/* intégré : ne peut être ni masqué ni redéfini */
    TclType *under;		/* type nominal scalaire : type sous-jacent */
    TclType *key;		/* DICT : type des clés */
    TclType *elem;		/* LIST : éléments ; DICT : valeurs */
    Tcl_Size arity;		/* LIST : nombre d'éléments, list(2) ; -1 : liste
				 * extensible (list sans parenthèses) */
    int keyed;			/* DICT : clés contraintes, dict(x, y) */
    Tcl_Size nkeys;
    TclDictKey *keys;		/* DICT : clés admises, dans l'ordre écrit */
    Tcl_Size nfields;		/* PRODUCT : champs ; SUM : variantes (triées) */
    TclTypeField *fields;
    int varTyped;		/* VAR : la variable visée doit être typée,
				 * var(typed).T ; elem est T (string si omis) */
    Tcl_Size bitWidth;		/* INT : bitarray(n, ordre) a n > 0 bits, sinon
				 * 0 */
    int bitMsb0;		/* bitarray : 1 si l'indice 0 est le bit de
				 * gauche (msb0), 0 si de droite (lsb0) */
    int binKind;		/* TCLBIN_* : représentation binaire, ou 0 */
    Tcl_Size binSize;		/* octets : entier, flottant, chaîne de taille
				 * fixe */
    const char *binEnc;		/* chaîne : nom d'encodage Tcl (statique), NULL
				 * pour des octets bruts (raw) */
    int binEnd;			/* TCLBIN_AUTO, TCLBIN_LE ou TCLBIN_BE */
    TclBinDyn *binDyn;		/* chaîne ou liste : taille / nombre dynamique */
    char *binAt;		/* at(EXPR).T : position de la valeur (texte) */
    TclBinLink *binLink;	/* *(...).T : la valeur est désignée par un entier */
    struct TclType *refOf;	/* type adresse de frontière : le lien dont il est
				 * l'adresse (position absolue de la cible) */
    char *binTag;		/* S($champ) : champ dont la valeur choisit la
				 * variante de la somme S */
    struct TclType *binBase;	/* type sans ces décorations (at, $champ, liste
				 * dynamique), pour TclTypeAssignable */
    int refine;			/* 1 : raffinement d'int ou de double (instance
				 * de int(s,n), double(n), bitarray(n)) : voir
				 * TclTypeAssignable */
    struct TypeGeneric *family;	/* instance d'un type def paramétré (grandeur(L,
				 * 0.001)), ou nominal défini sur elle : la
				 * famille, pour les règles « type convert
				 * famille famille » ; NULL sinon */
    Tcl_Obj *familyArgs;	/* liste des valeurs liées des paramètres, dans
				 * l'ordre de la déclaration (défauts compris) */
    Tcl_Obj *refCache[8];	/* typeref (ce type, variante i-1), empruntés à
				 * la table de l'interpréteur : évite la clé
				 * snprintf et le hachage à chaque valeur */
    struct TclCompactLayout *compact;
				/* disposition des cases quand ce type est
				 * l'élément d'une liste compacte, ou NULL */
    int compactState;		/* 0 : pas encore calculée ; 1 : compact ;
				 * -1 : pas de forme compacte */
    int compactWanted;		/* type def {-compact}T : les valeurs de ce type
				 * (une liste) sont toujours en cases, nombres
				 * normalisés à la construction ; sur un
				 * produit : toute liste de ce produit */
    int compactFields;		/* PRODUCT : un champ (ou un sous-champ) est une
				 * liste qui peut être en cases ; 0 : pas
				 * encore calculé, 1 : oui, -1 : non */
};

/* Les descripteurs vivent autant que l'interpréteur (pas de comptage). */

/* --------------------------------------------------------------------
 * Objets annotés
 * -------------------------------------------------------------------- */

/* Statut d'un argument après TclAnnotResolve. */
enum {
    TCLA_PLAIN = 0,		/* pas une annotation */
    TCLA_TYPED,			/* valeur typée, contenu déjà valide */
    TCLA_RAW,			/* annotation du parseur, type résolu, contenu
				 * NON validé */
    TCLA_EMPTY			/* annotation du type vide {} */
};

/* Ce qu'une fonction de conversion du noyau demande à l'objet. */
enum {
    TCLWANT_NUMBER, TCLWANT_INT, TCLWANT_DOUBLE, TCLWANT_BOOL,
    TCLWANT_LIST, TCLWANT_DICT, TCLWANT_STRING
};

/* Petit vecteur d'objets, qui tient une référence sur chacun. */
typedef struct TclObjVec {
    Tcl_Obj **v;
    Tcl_Size n, cap;
} TclObjVec;

/* tclAnnotObj.c */
Tcl_Obj *TclNewAnnotated(Tcl_Interp *interp, TclType *tp, int variant,
	Tcl_Obj *content);
Tcl_Obj *TclNewRawAnnotation(Tcl_Obj *typeText, Tcl_Obj *content);
Tcl_Obj *TclTypeRefFor(Tcl_Interp *interp, TclType *tp, int variant);
Tcl_Obj *TclNewAnnotatedOwn(Tcl_Obj *content, Tcl_Obj *typeRef);
Tcl_Obj *TclNewAnnotatedShared(Tcl_Interp *interp, TclType *tp, int variant,
	    Tcl_Obj *content);
int	TclIsAnnotated(Tcl_Obj *objPtr);
int	TclAnnotInfo(Tcl_Obj *objPtr, Tcl_Obj **contentPtr, TclType **tpPtr,
	    int *variantPtr, Tcl_Obj **rawTypePtr);
int	TclAnnotResolve(Tcl_Interp *interp, Tcl_Obj *objPtr, const TclType *ctx,
	    int *statusPtr, TclType **tpPtr, int *variantPtr,
	    Tcl_Obj **contentPtr);
int	TclAnnotUnwrap(Tcl_Interp *interp, Tcl_Obj *objPtr, int want,
	    Tcl_Obj **contentPtr);
void	TclAnnotInit(Tcl_Interp *interp);
/*
 * Déballage dans les fonctions de conversion du noyau (tclObj.c,
 * tclStringObj.c, tclBinary.c, tclExecute.c). TclAnnotatedObjTypePtr est le
 * type "annotated" : le test est une comparaison de pointeurs.
 *
 *   TCL_ANNOT_UNWRAP : contrôle strict (TclAnnotUnwrap) puis travail sur le
 *       contenu V, jamais sur l'annotation, qui n'est plus détruite par
 *       shimmering. À utiliser dans une fonction qui renvoie un int TCL_*.
 *   TCL_ANNOT_CONTENT : la vue chaîne est toujours permise ; on prend V.
 */
extern const Tcl_ObjType *const TclAnnotatedObjTypePtr;
#define TCL_ANNOT_P(o)	((o)->typePtr == TclAnnotatedObjTypePtr)
#define TCL_ANNOT_CONTENT(o) \
	(TCL_ANNOT_P(o) ? (Tcl_Obj *) (o)->internalRep.twoPtrValue.ptr1 : (o))
/* Le contenu à CONVERTIR (chaîne, octets) : pour une forme compacte, une
 * copie en chaîne gardée à côté des cases, pour que la conversion ne la
 * défasse pas. */
Tcl_Obj *TclCompactConv(Tcl_Obj *v);
#define TCL_ANNOT_CONV(o) \
	(TCL_ANNOT_P(o) ? TclCompactConv((Tcl_Obj *) \
		(o)->internalRep.twoPtrValue.ptr1) : (o))
#define TCL_ANNOT_UNWRAP(interp, o, want) \
	do { \
	    if (TCL_ANNOT_P(o)) { \
		Tcl_Obj *annotContent_; \
		if (TclAnnotUnwrap((interp), (o), (want), &annotContent_) \
			!= TCL_OK) { \
		    return TCL_ERROR; \
		} \
		(o) = annotContent_; \
	    } \
	} while (0)
/* Modification en place d'une valeur annotée (revalidée) : tclAnnotObj.c */
int	TclAnnotSetContent(Tcl_Interp *interp, Tcl_Obj *aPtr, Tcl_Obj *wPtr);

struct Tcl_Token;
Tcl_Obj	*TclAnnotNotes(Tcl_Obj *objPtr);
Tcl_Obj	*TclAnnotNormalize(Tcl_Interp *interp, Tcl_Obj *objPtr);
int	TclAnnotIsNoteOnly(Tcl_Obj *objPtr);
int	TclAnnotIsSourceRaw(Tcl_Obj *objPtr);
Tcl_Obj	*TclAnnotDisplay(Tcl_Interp *interp, Tcl_Obj *objPtr, int withNotes);
int	TclAnnotSuffixIsLiteral(struct Tcl_Token *tokenPtr, Tcl_Size count);
int	TclAnnotCollectionCast(Tcl_Interp *interp, Tcl_Obj *typeText,
	    Tcl_Obj *value, Tcl_Obj **resultPtr);
int	TclAnnotateValue(Tcl_Interp *interp, Tcl_Obj *typeText, Tcl_Obj *value,
	    Tcl_Obj **resultPtr);
/*
 * Le résolveur des constructeurs (patch cast-constructeur). Hors d'un
 * constructeur, {A}$x ne fait qu'empiler A (TclAnnotPush) ; un constructeur
 * appelle TclAnnotSettle sur la valeur qu'il range : les annotations sont
 * dépilées depuis la plus récente tant qu'elles se résolvent (type connu),
 * la première qui ne se résout pas arrête tout, celles dépilées doivent
 * être cohérentes, et la plus récente donne le cast. ctx : le contrat de la
 * cible (une somme : ses noms de variantes restent à la cible). Le
 * résultat (*resultPtr, sans référence propre, éventuellement objPtr
 * lui-même) a un type résolu ou aucun, et la pile restante.
 * TCLA_SETTLE_KEEP : ne rien résoudre (set {}y $x), seulement retirer le
 * type résolu et les annotations locales.
 */
#define TCLA_SETTLE_KEEP	1
Tcl_Obj	*TclAnnotPush(Tcl_Obj *value, Tcl_Obj *typeText);
int	TclAnnotSettle(Tcl_Interp *interp, Tcl_Obj *objPtr, const TclType *ctx,
	    int flags, Tcl_Obj **resultPtr);
/* La pile d'annotations non résolues (liste neuve, de la plus ancienne à la
 * plus récente), vide pour une valeur dont le type a été effacé par {}. */
Tcl_Obj	*TclAnnotPending(Tcl_Obj *objPtr);
Tcl_Obj	*TclAnnotPopTop(Tcl_Obj *src, Tcl_Obj **topPtr);
int	TclAnnotIsUnknown(Tcl_Interp *interp, Tcl_Obj *textObj);
Tcl_Obj	*TclAnnotWithPending(Tcl_Obj *res, Tcl_Obj *pending);
/* Vrai si la valeur porte la marque {} (type vide) posée hors constructeur. */
int	TclAnnotIsCleared(Tcl_Obj *objPtr);
int	TclAnnotErrorCodeIs(Tcl_Interp *interp, const char *what);

/* tclCompAnnot.c */
struct Tcl_Parse;
int	TclHasAnnotatedWord(Tcl_Parse *parsePtr);
Tcl_Obj *TclAnnotLocalWordText(Tcl_Interp *interp, struct Tcl_Token *wordTokenPtr);
void	TclVecAdd(TclObjVec *vec, Tcl_Obj *objPtr);
void	TclVecFree(TclObjVec *vec);
void	TclDiscard(Tcl_Obj *objPtr);

/*
 * tclVarRef.c (noyau) : résolution des liens var. Un lien est le NOM
 * CANONIQUE d'une variable : "::ns::x" ou "::a(k)" pour une variable
 * d'espace de noms (ou globale), "frame:UID:x" pour une variable locale d'un
 * appel de procédure (valable tant que cet appel, ou cette coroutine, vit).
 * Le nom canonique est idempotent : canoniser un nom canonique le rend tel
 * quel.
 *
 *   Canon : nom -> nom canonique (la variable n'a pas à exister).
 *   Get   : valeur courante de la variable (annotée si elle est typée), ou
 *           TCL_ERROR (variable absente, appel terminé).
 *   Set   : affecte la variable (ses propres contraintes de type jouent).
 *   Type  : type déclaré de la variable, NULL si elle n'est pas typée ou
 *           n'existe pas.
 */
int	TclVarRefCanon(Tcl_Interp *interp, Tcl_Obj *name, Tcl_Obj **canonPtr);
int	TclVarRefGet(Tcl_Interp *interp, const char *canon, Tcl_Obj **valuePtr);
int	TclVarRefSet(Tcl_Interp *interp, const char *canon, Tcl_Obj *value);
TclType *TclVarRefType(Tcl_Interp *interp, const char *canon);
/* Appelé par Tcl_PopCallFrame : l'appel se termine, ses liens deviennent
 * périmés (registre des variables locales, tclVarRef.c). */
struct CallFrame;
void	TclVarRefFrameGone(Tcl_Interp *interp, struct CallFrame *framePtr);

/* tclTypes.c */
void	TclTypesInit(Tcl_Interp *interp);
TclType *TclTypeParse(Tcl_Interp *interp, const char *text, Tcl_Size len);
int	TclTypeTagVariantFrom(const TclType *prod, Tcl_Size idx,
	    Tcl_Obj *const *vals, Tcl_Size nvals);
int	TclTypeTagVariant(const TclType *prod, Tcl_Size idx, Tcl_Obj *content);
int	TclTypeAbiAlign(int isFloat, int bits);
TclType *TclAnnotTypeOf(Tcl_Interp *interp, Tcl_Obj *textObj);
int	TclTypesGeneration(Tcl_Interp *interp, size_t *uidPtr, size_t *genPtr);
/* Représentation compacte (tclAnnotObj.c) */
typedef struct TclCompactLayout TclCompactLayout;
int TclAnnotIsSettled(Tcl_Obj *objPtr);
Tcl_Obj *TclAnnotNumericContent(Tcl_Obj *objPtr);
Tcl_Obj *TclAnnotIterList(Tcl_Obj *aPtr);
Tcl_Obj *TclCompactFrom(TclType *listTp, Tcl_Obj *content);
int	TclCompactHolds(Tcl_Obj *v, const TclType *elemTp, Tcl_Size *nPtr);
int	TclIsCompact(Tcl_Obj *v);
int	TclCompactFlat(TclType *et, size_t *stridePtr, int max, size_t off[],
	    int size[], int isFloat[]);
const unsigned char *TclCompactData(Tcl_Obj *v, const TclType *et,
	    Tcl_Size *nPtr);
Tcl_Obj *TclCompactNew(TclType *ltp, Tcl_Size n, unsigned char **dataPtr);
Tcl_Obj *TclCompactCast(TclType *target, Tcl_Obj *content);
Tcl_Obj *TclCompactElement(Tcl_Obj *v, Tcl_Size i);
Tcl_Obj *TclCompactCache(Tcl_Obj *v);
Tcl_Obj *TclCompactReadPath(Tcl_Obj *v, Tcl_Size idx, Tcl_Size nnames,
	    const char *const names[], TclType **tpPtr);
TclCompactLayout *TclCompactLayoutOf(TclType *elemTp);
void	TclCompactLayoutRelease(TclCompactLayout *lay);
void	TclCompactInit(Tcl_Interp *interp);
int	TclTypeVariantConvert(Tcl_Interp *interp, TclType *sum, int from,
	    Tcl_Obj *content, int to, Tcl_Obj **resPtr);
int	TclTypeSumCurrent(const TclType *tp, Tcl_Obj *part);
int	TclTypeSumNeedsTag(const TclType *tp, Tcl_Obj *content, int v);
int	TclTypeValidate(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *value,
	    int variantHint, int *variantOut);
int	TclTypeIncludes(const TclType *a, const TclType *b);
/* Une valeur de type got peut-elle être rangée dans une variable (ou un
 * champ) de type expected ? Même type, ou élargissement automatique : un
 * raffinement (int(u,8), int(s,16,be), bitarray(12)) entre dans int, double
 * ou un raffinement plus large. L'inverse (rétrécissement) reste refusé. */
int	TclTypeAssignable(const TclType *expected, const TclType *got);
int	TclTypeSame(const TclType *a, const TclType *b);
/* Générique d'un nominal scalaire (chaîne des bases), et règle du contrat :
 * got entre dans expected s'il y est assignable, ou si un type de sa chaîne
 * de bases l'est ; deux nominaux sans lien ne sont jamais équivalents. */
const TclType *TclTypeGenericOf(const TclType *tp);
int	TclTypeEnters(const TclType *expected, const TclType *got);
TclType *TclTypeListOf(Tcl_Interp *interp, TclType *elem);
TclType *TclTypeCommon(Tcl_Interp *interp, TclType *a, TclType *b);
int	TclTypeCast(Tcl_Interp *interp, Tcl_Obj *src, TclType *target,
	    Tcl_Obj **resultPtr);
Tcl_Size TclTypeFieldIndex(const TclType *tp, const char *name);
/* Évalue le script d'un défaut (au niveau global) et rend son résultat dans
 * *resultPtr (objet neuf, refCount 0). what décrit le champ pour le message
 * d'erreur. Le résultat n'est PAS validé : l'appelant le conforme au type. */
int	TclTypeRunDefault(Tcl_Interp *interp, const char *what,
	    Tcl_Obj *script, Tcl_Obj **resultPtr);
int	TclTypeIsPlainString(const TclType *tp);
int	TclTypeIsAny(const TclType *tp);
/* type deref : TclTypeHasLink dit si un lien var se trouve dans tp ;
 * TclTypeDerefType rend le type où chaque var.T est remplacé par T (NULL et
 * message si impossible). TclValueDeref remplace chaque lien d'une valeur
 * typée par la valeur actuelle de la variable visée. */
int	TclTypeHasLink(const TclType *tp);
TclType *TclTypeDerefType(Tcl_Interp *interp, TclType *tp);
int	TclValueDeref(Tcl_Interp *interp, Tcl_Obj *value,
	    Tcl_Obj **resultPtr);
/* bitarray(n, ordre) : entier de n bits dont on lit/écrit les bits.
 * Les positions (lo, len) sont toujours celles du poids faible (0 = bit de
 * droite) ; la conversion depuis l'indice msb0 est faite par l'appelant. */
int	TclBitsGet(Tcl_Interp *interp, Tcl_Obj *value, Tcl_Size lo,
	    Tcl_Size len, Tcl_Obj **resultPtr);
int	TclBitsPut(Tcl_Interp *interp, Tcl_Obj *value, Tcl_Size lo,
	    Tcl_Size len, Tcl_Obj *bits, Tcl_Obj **resultPtr);
int	TclTypeDictKeyAllowed(const TclType *tp, const char *key);
/* Valide content puis ajoute les valeurs par défaut des clés manquantes
 * (dict(x?=0)). content est un objet PRIVÉ, modifié en place. */
int	TclTypeComplete(Tcl_Interp *interp, const TclType *tp, Tcl_Obj *content,
	    int variantHint, int *variantOut);

/* tclTypedVar.c */
int	TclTypedVarPrepareWrite(Tcl_Interp *interp, Var *varPtr,
	    Tcl_Obj **valuePtrPtr, int *flagsPtr, Tcl_Obj **tmpPtrPtr);
void	TclTypedVarAfterStore(Var *varPtr);
int	TclTypedVarBindArg(Tcl_Interp *interp, Var *varPtr);
void	TclTypedVarForget(Var *varPtr);
void	TclTypedVarForgetByName(Tcl_Interp *interp, Tcl_Obj *nameObj);
Tcl_Obj *TclTypedConform(Tcl_Interp *interp, TclType *tp, Tcl_Obj *arg);
int	TclTypeTagCheck(Tcl_Interp *interp, const TclType *prod, Tcl_Size idx,
	    Tcl_Obj *arg, Tcl_Obj *const *vals, Tcl_Size nvals);
Tcl_Obj *TclTypedConformHint(Tcl_Interp *interp, TclType *tp, Tcl_Obj *arg,
	    int hint);
int	TclSetAnnotatedCmd(Tcl_Interp *interp, Tcl_Size objc,
	    Tcl_Obj *const objv[]);

/* tclTypePath.c */
int	TclTypePathRead(Tcl_Interp *interp, Tcl_Obj *containerPtr,
	    const char *path, Tcl_Obj **resultPtr);
int	TclTypedVarGetElementByName(Tcl_Interp *interp, Tcl_Obj *nameObj,
	    Tcl_Obj **resultPtr);
Tcl_Obj *TclPathIndexNew(Tcl_Obj *const parts[], Tcl_Size n,
	    const char *subst);
int	TclTypePathConstruct(Tcl_Interp *interp, TclType *tp,
	    const char *path, Tcl_Size nvals, Tcl_Obj *const vals[],
	    Tcl_Obj **contentPtr);
int	TclBuildProductNamed(Tcl_Interp *interp, TclType *tp, Tcl_Size n,
	    Tcl_Obj *const a[], Tcl_Obj **contentPtr);
int	TclTypePathWrite(Tcl_Interp *interp, Tcl_Obj *containerPtr,
	    const char *path, Tcl_Size nvals, Tcl_Obj *const vals[],
	    Tcl_Obj **newPtr);
int	TclSetPathCmd(Tcl_Interp *interp, Tcl_Size objc,
	    Tcl_Obj *const objv[]);
int	TclTypedVarGetElement(Tcl_Interp *interp, Tcl_Obj *part1Ptr,
	    Tcl_Obj *part2Ptr, Tcl_Obj **resultPtr);
int	TclTypedVarGetElementVar(Tcl_Interp *interp, Var *varPtr,
	    Tcl_Obj *part2Ptr, Tcl_Obj **resultPtr);
int	TclTypedVarSetElementVar(Tcl_Interp *interp, Var *varPtr,
	    Tcl_Obj *part1Ptr, int index, Tcl_Obj *part2Ptr,
	    Tcl_Obj *valuePtr, Tcl_Obj **resultPtr);
/* Écritures qui MODIFIENT une partie (incr, append, lappend) par chemin. */
#define TCLTV_APPEND		1	/* argPtr : texte ajouté */
#define TCLTV_LAPPEND		2	/* argPtr : un élément ajouté */
#define TCLTV_LAPPEND_LIST	3	/* argPtr : liste d'éléments ajoutés */
#define TCLTV_INCR		4	/* argPtr : incrément, ou NULL pour 1 */
int	TclTypedVarModifyElementVar(Tcl_Interp *interp, Var *varPtr,
	    Tcl_Obj *part1Ptr, int index, Tcl_Obj *part2Ptr, int op,
	    Tcl_Obj *argPtr, Tcl_Obj **resultPtr);
int	TclTypedVarModifyElement(Tcl_Interp *interp, Tcl_Obj *part1Ptr,
	    Tcl_Obj *part2Ptr, int op, Tcl_Obj *argPtr, Tcl_Obj **resultPtr);
int	TclTypedVarFlagsOp(int flags);
int	TclTypedVarSetElement(Tcl_Interp *interp, Tcl_Obj *part1Ptr,
	    Tcl_Obj *part2Ptr, Tcl_Obj *valuePtr, Tcl_Obj **resultPtr);

MODULE_SCOPE Tcl_Obj *TclTypedVarDeclare(Tcl_Interp *interp, Tcl_Obj *nameObj, Tcl_Obj *typed);
MODULE_SCOPE int TclTypedVarRedeclare(Tcl_Interp *interp, Var *varPtr, Tcl_Obj *nameObj);
#endif /* TCLTYPES_H */
