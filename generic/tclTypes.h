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
#define TCL_TYPES_VERSION "v18e"

#ifndef VAR_TYPED
#define VAR_TYPED	0x400000
#endif

/* --------------------------------------------------------------------
 * Descripteurs de types
 * -------------------------------------------------------------------- */

typedef struct TclType TclType;

/*
 * 23/11 : opérations spécialisées d'un type (voir CONCEPTION-ops.md). Une
 * bibliothèque, intégrée ou chargée, déclare par TclTypesRegisterOps une
 * fonction « match » qui reconnaît les types visés et la table de leurs
 * opérations ; le type retient la meilleure (priorité la plus haute). Une
 * opération absente (NULL), ou qui rend NULL / 0, laisse le chemin général.
 * CONTENT est le contenu de la valeur typée (sans son annotation).
 */
typedef struct TclTypeOps {
    int version;		/* TCL_TYPE_OPS_VERSION */
    const char *name;		/* pour type info ops */
    /* l'élément I (vue liste) : une valeur typée neuve (compte 0) */
    Tcl_Obj *(*index)(const TclType *tp, Tcl_Obj *content, Tcl_Size i);
    /* le champ F (rang) de l'élément I d'une liste de produits */
    Tcl_Obj *(*elemField)(const TclType *tp, Tcl_Obj *content, Tcl_Size i,
	    Tcl_Size f);
    /* l'élément I en double, sans objet : 1 si lu */
    int (*indexDouble)(const TclType *tp, Tcl_Obj *content, Tcl_Size i,
	    double *outPtr);
} TclTypeOps;

typedef struct TclTypeSpec {
    int version;		/* TCL_TYPE_OPS_VERSION */
    const char *name;
    int priority;		/* la plus haute l'emporte ; intégrées : 0 */
    int (*match)(Tcl_Interp *interp, const TclType *tp, void *clientData);
    const TclTypeOps *ops;
    void *clientData;
} TclTypeSpec;

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
    int metaMask;		/* v17e : bare, typed, noted, typednoted, any
				 * (TCL_META_*, leur union) ; 0 sinon */
    TclType *under;		/* type nominal scalaire : type sous-jacent */
    TclType *key;		/* DICT : type des clés */
    TclType *elem;		/* LIST : éléments ; DICT : valeurs */
    Tcl_Size arity;		/* LIST : nombre d'éléments, list(2) ; -1 : liste
				 * extensible (list sans parenthèses) */
    int lenRange;		/* v17g : LIST extensible bornée, list(1:Inf),
				 * list(2:5) : lenMin et lenMax valent */
    Tcl_Size lenMin;		/* au moins lenMin éléments */
    Tcl_Size lenMax;		/* au plus lenMax ; -1 : sans borne (Inf) */
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
    struct TclType *binAtBody;	/* at(EXPR).T : T (codec C) */
    struct TclType *listOf;	/* list.T de ce type, déjà interné (cache de
				 * TclTypeListOf : $P(*)) */
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
    int sumMode;		/* SUM (v16w) : TCLT_SUM_TAGGED, une somme
				 * (",") dont la valeur écrit sa variante
				 * ("variante charge") ; TCLT_SUM_UNION, une
				 * union ("|"), disjointe, dont la valeur est
				 * la charge seule */
    struct TclType *formOf;	/* 24/11 : forme (disposition) d'un type,
				 * type def T DEF {-compact}F : F->formOf = T */
    const TclTypeOps *ops;	/* 23/11 : opérations spécialisées, ou NULL */
    size_t opsGen;		/* génération du registre où ops a été choisi */
    struct TypeData *owner;	/* v16u : la table de types qui l'a créé */
    Tcl_Obj *pred;		/* v17u : prédicat int(($%2==0)), texte de
				 * l'expression (avec ses parenthèses), ou NULL */
    struct TclType *predBase;	/* v17u : le type restreint (int, int(u,8)...) */
    Tcl_Interp *predInterp;	/* v17u : où l'évaluer (NULL : interp supprimé) */
    void *predProg;		/* v17u : forme numérique compilée (MProg), ou
				 * NULL ; (void *) 1 : pas de forme numérique */
    struct TclType *variantOf;	/* v18b : type de variante S/v : la somme S
				 * (la racine, jamais un type de variante) ;
				 * NULL sinon. Le type est une somme (mêmes
				 * variantes) dont seule variantIdx est
				 * permise ; sa charge peut être restreinte
				 * (S/v/w : fields[variantIdx].type est W/w) */
    Tcl_Size variantIdx;
    struct TclType **variants;	/* v18b : S/v déjà internés, par variante
				 * (charge non restreinte), ou NULL */
};
#define TCLT_SUM_TAGGED	0
#define TCLT_SUM_UNION	1
/* v16w : une somme dont la valeur écrit sa variante (pas une union, pas une
 * somme S($champ) dont la variante est dans un champ voisin) */
#define TclSumTagged(tp) (0 && (tp)->kind == TCLT_SUM \
	&& (tp)->sumMode == TCLT_SUM_TAGGED && (tp)->binTag == NULL)
/* v17z : la variante est une métadonnée (TclSumTagged ne vaut plus : la
 * valeur est la charge seule). Une somme (pas une union, pas une somme
 * binaire S($champ)) ne devine jamais entre deux variantes de même rang ;
 * l'étiquette la désigne. */
#define TclSumStrict(tp) ((tp)->kind == TCLT_SUM \
	&& (tp)->sumMode == TCLT_SUM_TAGGED && (tp)->binTag == NULL)
/* une somme dont la variante s'étiquette (somme ou union) */
#define TclSumLabeled(tp) ((tp)->kind == TCLT_SUM && (tp)->binTag == NULL)

/*
 * v18a : une étiquette s'écrit {/nom} (champ d'un produit, variante d'une
 * somme) ; le nom suit la barre. L'ancienne écriture {nom:} est refusée
 * (TclLabelIsOld) avec un message qui donne la nouvelle.
 */
/* v18b : le type de variante S/v (voir TclType.variantOf) */
#define TclTypeIsVariant(tp)	((tp) != NULL && (tp)->variantOf != NULL)
/* la somme d'un type de variante, ou le type lui-même */
#define TclTypeSumRoot(tp)	((tp)->variantOf != NULL ? (tp)->variantOf : (tp))

#define TclLabelIs(t, l)	((l) > 1 && (t)[0] == '/')
#define TclLabelIsOld(t, l)	((l) > 1 && (t)[0] != '/' && (t)[0] != '-' \
	&& (t)[0] != '@' && (t)[(l) - 1] == ':' && (t)[(l) - 2] != ':')

/*
 * v16u : les descripteurs appartiennent à la table de types d'un
 * interpréteur ; une valeur qui en garde un (une référence de type, un
 * tampon de cases) garde la table en vie : elle survit à l'interpréteur
 * (interp delete) jusqu'à la libération de la dernière valeur.
 */
void	TclTypeHold(const TclType *tp);
void	TclTypeRelease(const TclType *tp);

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
Tcl_Obj	*TclAnnotNoteList(Tcl_Interp *interp, Tcl_Obj *objPtr);	/* v17c */
Tcl_Obj	*TclAnnotNormalize(Tcl_Interp *interp, Tcl_Obj *objPtr);
int	TclAnnotIsNoteOnly(Tcl_Obj *objPtr);
int	TclAnnotIsSourceRaw(Tcl_Obj *objPtr);
Tcl_Obj	*TclAnnotDisplay(Tcl_Interp *interp, Tcl_Obj *objPtr, int withNotes);
int	TclAnnotSuffixIsLiteral(struct Tcl_Token *tokenPtr, Tcl_Size count);
void	TclAnnotRegisterFresh(Tcl_Interp *interp, Tcl_Obj *objPtr);
Tcl_Size TclAnnotFreshMark(Tcl_Interp *interp);
void	TclAnnotExpireFrom(Tcl_Interp *interp, Tcl_Size mark);
void	TclAnnotExpire(Tcl_Interp *interp, Tcl_Obj *objPtr);
int	TclAnnotHasFresh(Tcl_Obj *objPtr);
int	TclAnnotCastText(Tcl_Interp *interp, Tcl_Obj *typeText, Tcl_Obj *value,
	    Tcl_Obj **resultPtr);
Tcl_Obj *TclAnnotKeepExpired(Tcl_Obj *res, Tcl_Obj *src);
int	TclAnnotSettleValueArg(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    Tcl_Obj **resPtr);
int	TclAnnotCastWord(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    Tcl_Obj **resultPtr);
Tcl_Size TclAnnotLabelIndex(const TclType *tp, const char *text);
int	TclAnnotOldLabel(Tcl_Interp *interp, const TclType *tp, const char *text);
Tcl_Obj *TclAnnotLabelOf(Tcl_Obj *objPtr);
int	TclAnnotCheckOldLabel(Tcl_Interp *interp, Tcl_Obj *objPtr);
void	TclAppendPayloadChain(Tcl_Obj *res, const TclType *tp, int variant,
	    Tcl_Obj *content);
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
Tcl_Obj	*TclAnnotRawText(Tcl_Obj *objPtr);
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
Tcl_Obj *	TclAnnotCarryNotes(Tcl_Obj *from, Tcl_Obj *to);
Tcl_Obj *	TclTypeNormalizeContent(TclType *tp, Tcl_Obj *v);
int		TclTypeIsNumUnion(const TclType *et);
int		TclTypeListLenCheck(Tcl_Interp *interp, const TclType *tp,
			    Tcl_Size n);			/* v17g */

/*
 * v17e : les méta-types. L'état d'une valeur sur deux axes (un type
 * résolu ? des notes ?) ; any est leur union. Bits, pour les unions.
 */
typedef enum {
    TCL_META_BARE = 1,		/* nue : aucun type, aucune note */
    TCL_META_TYPED = 2,		/* typée : un type résolu */
    TCL_META_NOTED = 4,		/* notée : des notes, pas de type */
    TCL_META_TYPED_NOTED = 8	/* typée et notée */
} TclMeta;
#define TCL_META_ANY 15
TclMeta		TclAnnotMeta(Tcl_Obj *objPtr);
int		TclTypeMetaMask(const TclType *tp);
int		TclTypeMetaCheck(Tcl_Interp *interp, const TclType *tp,
			    Tcl_Obj *objPtr);
const char *	TclMetaName(TclMeta m);
Tcl_Obj *	TclAnnotAddNote(Tcl_Obj *objPtr, Tcl_Obj *note);
int		TclTypeCompleteBare(Tcl_Interp *interp, const TclType *tp,
			    Tcl_Obj *content, int variantHint, int *variantOut);	/* v17d */	/* v17b : {int}i | {double}d */

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
Tcl_Obj *	TclTypesNames(Tcl_Interp *interp);
/* Représentation compacte (tclAnnotObj.c) */
typedef struct TclCompactLayout TclCompactLayout;
int TclAnnotIsSettled(Tcl_Obj *objPtr);
int TclAnnotIsPlainTyped(Tcl_Obj *objPtr);
int TclTypePlainScalar(const TclType *tp);
int TclTypeBareElem(const TclType *tp);
#define TclTypeFamily(t) ((t)->formOf != NULL ? (t)->formOf : (t))
Tcl_Obj *TclAnnotWrapType(Tcl_Obj *c, const TclType *tp);
int TclCompactScalarList(Tcl_Obj *v);
int TclTypeHasBareRule(Tcl_Interp *interp, const TclType *tp);
struct Var;
int TclTypedVarLappendElems(Tcl_Interp *interp, struct Var *varPtr,
	Tcl_Size objc, Tcl_Obj *const objv[], int index, Tcl_Obj **resultPtr);
int TclTypesRegisterOps(Tcl_Interp *interp, const TclTypeSpec *spec);
const TclTypeOps *TclTypeOpsOf(const TclType *tp);
enum { TCL_TYPE_OPS_ON = 1, TCL_TYPE_OPS_OFF, TCL_TYPE_OPS_CHECK };
int TclTypeOpsMode(void);
int TclTypeOpsSame(const char *where, Tcl_Obj *ops, Tcl_Obj *gen);
Tcl_Obj *TclTypedElemFieldFast(Tcl_Obj *a, Tcl_Obj *idx, Tcl_Obj *lit);
const char *TclTypeOpsName(const TclType *tp);
#define TCL_TYPES_INTERNAL
#include "tclTypesOps.h"
extern const TclTypeSpec tclBuiltinTypeSpecs[];
extern const int tclBuiltinTypeSpecCount;
int TclCompactIndexInto(Tcl_Obj *v, Tcl_Size index, Tcl_Obj *cur);
int TclAnnotFastIndex(Tcl_Obj *aPtr, int encodedIndex, Tcl_Obj **elemPtr);
int TclAnnotExpireFromRet(Tcl_Interp *interp, Tcl_Size mark, int code);
int TclTypeDebugCmd(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]);
int TclSumDecode(const TclType *tp, Tcl_Obj *v, Tcl_Obj **payloadPtr);
Tcl_Obj *TclSumMake(const TclType *tp, int v, Tcl_Obj *payload);
Tcl_Obj *TclAnnotNumericContent(Tcl_Obj *objPtr);
Tcl_Obj *TclAnnotIterList(Tcl_Obj *aPtr);
Tcl_Obj *TclCompactFrom(TclType *listTp, Tcl_Obj *content);
int	TclCompactHolds(Tcl_Obj *v, const TclType *elemTp, Tcl_Size *nPtr);
int	TclIsCompact(Tcl_Obj *v);
int	TclIsCompactElem(Tcl_Obj *v);
int	TclCompactFlat(TclType *et, size_t *stridePtr, int max, size_t off[],
	    int size[], int isFloat[]);
const unsigned char *TclCompactData(Tcl_Obj *v, const TclType *et,
	    Tcl_Size *nPtr);
Tcl_Obj *TclCompactNew(TclType *ltp, Tcl_Size n, unsigned char **dataPtr);
Tcl_Obj *TclCompactNewAlways(TclType *ltp, Tcl_Size n,
	unsigned char **dataPtr);
Tcl_Obj *TclCompactFieldColumn(Tcl_Obj *v, Tcl_Size f, TclType *lt);
Tcl_Obj *TclCompactViewNew(Tcl_Obj *v, Tcl_Size first, Tcl_Size step,
	Tcl_Size count);
int TclIsCompactView(Tcl_Obj *v);
Tcl_Obj *TclObjViewNew(Tcl_Obj *v, Tcl_Size first, Tcl_Size step,
	Tcl_Size count);
int TclObjViewInfo(Tcl_Obj *v, Tcl_Obj **basePtr, Tcl_Obj *const **elemsPtr,
	Tcl_Size *nPtr, Tcl_Size *stridePtr);
int TclIsObjView(Tcl_Obj *v);
int TclCompactScalarWritable(Tcl_Obj *v);
/* 24/11 (v17r) : les boucles vectorisables (foreach, lmap) */
typedef int (TclVecEachloopProc)(Tcl_Interp *interp, int isLmap,
	Tcl_Size objc, Tcl_Obj *const objv[]);
int TclVecRegister(const char *name, TclVecEachloopProc *proc, int priority);
void TclTypePredTrust(int delta);
Tcl_Obj *TclCompactClone(Tcl_Obj *v);
Tcl_Obj *TclCompactCloneEx(Tcl_Obj *v, int copy);
void *TclTypePathCellsBegin(Tcl_Interp *interp, Tcl_Obj *nameObj,
	const char *path, Tcl_Size K, Tcl_Size count, unsigned char **dst,
	size_t *dstep, const char **whyPtr, int *copiedPtr);
void TclTypePathCellsEnd(void *h, int commit);
int TclTypeIntToDoubleCheck(Tcl_Interp *interp, const TclType *tp,
	const TclType *from, Tcl_Obj *content);
int TclTypePathWriteCellColumns(Tcl_Interp *interp, Tcl_Obj *nameObj,
	const char *path, Tcl_Size K, Tcl_Obj *const cols[]);
Tcl_Obj *TclCompactRepeat(TclType *ltp, Tcl_Obj *value, Tcl_Size n);
int TclCompactFillFrom(Tcl_Obj *v, Tcl_Size idx, Tcl_Size first,
	Tcl_Size count, Tcl_Size step);
int TclVecCompilable(Tcl_Interp *interp, int isLmap, const char *body,
	Tcl_Size len);
int TclVecEachloop(Tcl_Interp *interp, int isLmap, Tcl_Size objc,
	Tcl_Obj *const objv[]);
int TclCompactScalarInfo(Tcl_Obj *v, Tcl_Size *nPtr,
	const unsigned char **dataPtr, size_t *stridePtr, int *kindPtr,
	int *sizePtr);
Tcl_Obj *TclCompactCast(TclType *target, Tcl_Obj *content);
Tcl_Obj *TclCompactElement(Tcl_Obj *v, Tcl_Size i);
Tcl_Obj *TclCompactFieldNumber(Tcl_Obj *o, Tcl_Size idx);
Tcl_Obj *TclCompactCache(Tcl_Obj *v);
Tcl_Obj *TclCompactReadPath(Tcl_Obj *v, Tcl_Size idx, Tcl_Size nnames,
	    const char *const names[], TclType **tpPtr);
Tcl_Obj *TclCompactViewReadPath(Tcl_Obj *view, TclType *vt, Tcl_Size nnames,
	    const char *const names[], TclType **tpPtr);
int	TclCompactWriteScalar(Tcl_Obj *v, Tcl_Size idx, Tcl_Size nnames,
	    const char *const names[], Tcl_Obj *value);
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
int	TclTypeCastImplicit(Tcl_Interp *interp, Tcl_Obj *src,
			    TclType *target, Tcl_Obj **resultPtr);	/* v17d */
int	TclTypeCast(Tcl_Interp *interp, Tcl_Obj *src, TclType *target,
	    Tcl_Obj **resultPtr);
Tcl_Size TclTypeFieldIndex(const TclType *tp, const char *name);
Tcl_Size TclTypeMemberIndex(const TclType *tp, const char *name, Tcl_Size len);
TclType *TclTypeVariantOf(TclType *sum, Tcl_Size idx, TclType *payload);
void	TclTypeAppendMemberName(Tcl_DString *ds, const char *name);
TclType *TclTypeMember(Tcl_Interp *interp, struct TypeData *d, TclType *tp,
	    const char *p, Tcl_Size len);
/* Évalue le script d'un défaut (au niveau global) et rend son résultat dans
 * *resultPtr (objet neuf, refCount 0). what décrit le champ pour le message
 * d'erreur. Le résultat n'est PAS validé : l'appelant le conforme au type. */
int	TclTypeRunDefault(Tcl_Interp *interp, const char *what,
	    Tcl_Obj *script, Tcl_Obj **resultPtr);
int	TclTypeIsPlainString(const TclType *tp);
int	TclTypeIsAny(const TclType *tp);
Tcl_Obj *TclTypeSumOverlaps(const TclType *tp);
void	TclTypeResultRename(Tcl_Interp *interp, const char *from,
	    const char *to);
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
int	TclBitsSetInPlace(Tcl_Obj *v, Tcl_Size lo, int bit,
	    Tcl_Size widthDigits);
Tcl_Obj *	TclBitsNewWorkObj(Tcl_Obj *value, Tcl_Size widthDigits);
int	TclBitsGather(Tcl_Interp *interp, Tcl_Obj *value, Tcl_Size lo,
	    Tcl_Size step, Tcl_Size count, int reverse, Tcl_Obj **resultPtr);
int	TclBitsPutStride(Tcl_Interp *interp, Tcl_Obj *value, Tcl_Size lo,
	    Tcl_Size step, Tcl_Size count, int reverse, Tcl_Obj *pattern,
	    int fill, Tcl_Size widthDigits, Tcl_Obj **resultPtr);
int	TclBitsFillStride(Tcl_Obj *v, Tcl_Size lo, Tcl_Size step,
	    Tcl_Size count, int bit, Tcl_Size widthDigits);
int	TclAnnotSwapContent(Tcl_Obj *a, Tcl_Obj *v);
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
	    Tcl_Obj **contentPtr, int *variantPtr);
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
MODULE_SCOPE Tcl_Obj *TclTypedVarDeclareVar(Tcl_Interp *interp, Var *varPtr, Var *arrayPtr,
			    Tcl_Obj *nameObj, Tcl_Obj *typed);
MODULE_SCOPE Tcl_Obj *TclTypedFieldNumber(Var *varPtr, Tcl_Obj *part2Ptr);
MODULE_SCOPE Tcl_Obj *TclTupleAssign(Tcl_Interp *interp, Tcl_Obj *names,
			    Tcl_Obj *value, Tcl_Obj *paths);
MODULE_SCOPE Tcl_Obj *TclTypedDeclareLocal(Tcl_Interp *interp, Var *varPtr,
			    Tcl_Obj *nameObj, Tcl_Obj *typeObj, Tcl_Obj *valueObj);
MODULE_SCOPE int TclTypedVarRedeclare(Tcl_Interp *interp, Var *varPtr, Tcl_Obj *nameObj);
/* v16p : procédures typées (tclTypes.c) ; la signature est rangée dans
 * Proc.typedSig. */
MODULE_SCOPE int TclTypedProcSig(Tcl_Interp *interp, Tcl_Obj *nameObj,
			    Tcl_Obj *argsObj, void **sigPtr, int *typedArgsPtr);
/* v17k : l'espace des types donné (NULL : celui du nom) ; et la création
 * d'une procédure, typée ou non, pour tous ses créateurs (proc, apply,
 * méthodes TclOO) : tclProc.c. */
MODULE_SCOPE int TclTypedProcSigNs(Tcl_Interp *interp, Tcl_Namespace *nsPtr,
			    Tcl_Obj *nameObj, Tcl_Obj *argsObj, void **sigPtr,
			    int *typedArgsPtr);
MODULE_SCOPE int TclCreateTypedProc(Tcl_Interp *interp, Tcl_Namespace *nsPtr,
			    Tcl_Obj *nameObj, const char *procName,
			    Tcl_Obj *argsObj, Tcl_Obj *bodyObj,
			    Proc **procPtrPtr);
MODULE_SCOPE int TclTypedProcBind(Tcl_Interp *interp, void *sig, Tcl_Size skip);
/* v16z : apply {T}{args corps} : le type de retour d'une lambda. */
MODULE_SCOPE Tcl_Obj *TclTypedLambdaFor(Tcl_Interp *interp, Tcl_Obj *lambdaObj);
MODULE_SCOPE int TclTypedLambdaRet(Tcl_Interp *interp, Tcl_Obj *lambdaObj,
			    Tcl_Namespace *nsPtr, void **sigPtr);
MODULE_SCOPE int TclTypedProcReturn(Tcl_Interp *interp, void *sig, int result);
MODULE_SCOPE void TclTypedSigRetain(void *sig);
MODULE_SCOPE void TclTypedSigRelease(void *sig);
MODULE_SCOPE Tcl_Obj *TclTypedSigArgNames(void *sig);
MODULE_SCOPE Tcl_Size TclTypedSigLocals(void *sig, Tcl_Obj ***namesPtr);
MODULE_SCOPE int TclTypedSigTakesWords(void *sig);
MODULE_SCOPE int TclTypedSigDefault(void *sig, const char *name,
			    Tcl_Obj **defPtr);
#endif /* TCLTYPES_H */
