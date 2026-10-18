/*
 * cstruct.c --
 *
 * FR : Passer un PRODUIT TYPÉ Tcl à une STRUCTURE C, et inversement.
 *      Application : traiter des lots de mesures de capteurs en C.
 *
 *        Tcl                                  C
 *        ---------------------------------    -------------------------------
 *        type def Sample {                    typedef struct {
 *            {u32le}id; {f64le}value;             uint32_t id;
 *            {u8}flags}                           double   value;
 *                                                 uint8_t  flags;   // + 7 de remplissage
 *                                             } Sample;               // 24 octets
 *        type def Batch {                     typedef struct {
 *            {u32le}n;                            uint32_t n;       // + 4 de remplissage
 *            {list($n).Sample}items}              Sample   items[];
 *                                             } Batch;                // 8 + 24 n
 *        type def Summary {                   typedef struct {
 *            {u32le}count; {f64le}min;            uint32_t count;
 *            {f64le}max;  {f64le}mean}            double min, max, mean;
 *                                             } Summary;              // 32 octets
 *
 *      « -align natural » donne au produit Tcl la disposition d'un compilateur C :
 *      champs alignés sur leur taille, taille arrondie à l'alignement. Les deux
 *      côtés décrivent donc les mêmes octets (cs::layout permet de le vérifier).
 *
 *      Deux façons de passer le produit à C, sur la même donnée :
 *
 *        cs::stats_raw  OCTETS   le produit est ENCODÉ par `type binary encode
 *                                -align natural` ; C reçoit les octets et les lit
 *                                à travers un (const Batch *) : zéro copie champ
 *                                par champ, c'est la voie rapide.
 *        cs::stats_obj  BATCH    C reçoit la VALEUR ANNOTÉE elle-même (le produit
 *                                Tcl, non encodé), vérifie son type, lit ses champs
 *                                avec l'API publique (liste) et remplit ses
 *                                structures. Plus lent, mais sans encodage et avec
 *                                contrôle du type.
 *
 *      Et dans l'autre sens :
 *
 *        cs::generate N SEED     C fabrique un Batch (struct) et le rend en octets ;
 *                                Tcl le relit en produit annoté avec
 *                                `type binary decode Batch ... -align natural`.
 *
 *      La statistique : count / min / max / moyenne des valeurs, en ignorant les
 *      mesures dont le bit 0 de flags est levé (capteur invalide).
 *
 * EN : Passing a Tcl TYPED PRODUCT to a C STRUCT, and back. Application:
 *      processing batches of sensor readings in C. `-align natural` gives the Tcl
 *      product the layout of a C compiler, so both sides describe the same bytes.
 *      Two ways to hand the product to C: encode it and read the bytes through a
 *      (const Batch *) (fast), or pass the annotated value itself and let C check
 *      its type and read its fields through the public list API.
 *
 * HYPOTHÈSE / ASSUMPTION : hôte little-endian (x86-64, arm64) ; Tcl_Size = 8.
 */
#include <tcl.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef struct { uint32_t id; double value; uint8_t flags; } Sample;
typedef struct { uint32_t n; Sample items[]; } Batch;
typedef struct { uint32_t count; double min, max, mean; } Summary;

/* ------------------------------------------------------------------ */
/* Le calcul, écrit une fois, sur de vraies structures C.             */
/* The computation, written once, on real C structs.                  */
/* ------------------------------------------------------------------ */
static void
Compute(const Sample *s, uint32_t n, Summary *out)
{
    uint32_t i, count = 0;
    double sum = 0, mn = 0, mx = 0;

    for (i = 0; i < n; i++) {
	if (s[i].flags & 1) {
	    continue;			/* capteur invalide / invalid sensor */
	}
	if (count == 0 || s[i].value < mn) mn = s[i].value;
	if (count == 0 || s[i].value > mx) mx = s[i].value;
	sum += s[i].value;
	count++;
    }
    out->count = count;
    out->min = mn;
    out->max = mx;
    out->mean = count ? sum / count : 0.0;
}

static Tcl_Obj *
SummaryBytes(const Summary *sm)
{
    return Tcl_NewByteArrayObj((const unsigned char *) sm, sizeof(*sm));
}

/* cs::layout : tailles et décalages vus par le compilateur C. */
static int
LayoutCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *r = Tcl_NewListObj(0, NULL);
    (void) cd;
    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, "");
	return TCL_ERROR;
    }
#define ADD(name, val) \
    Tcl_ListObjAppendElement(NULL, r, Tcl_NewStringObj(name, -1)); \
    Tcl_ListObjAppendElement(NULL, r, Tcl_NewWideIntObj((Tcl_WideInt)(val)))
    ADD("sizeof(Sample)", sizeof(Sample));
    ADD("Sample.id", offsetof(Sample, id));
    ADD("Sample.value", offsetof(Sample, value));
    ADD("Sample.flags", offsetof(Sample, flags));
    ADD("Batch.n", offsetof(Batch, n));
    ADD("Batch.items", offsetof(Batch, items));
    ADD("sizeof(Summary)", sizeof(Summary));
    ADD("Summary.count", offsetof(Summary, count));
    ADD("Summary.min", offsetof(Summary, min));
    ADD("Summary.max", offsetof(Summary, max));
    ADD("Summary.mean", offsetof(Summary, mean));
    Tcl_SetObjResult(interp, r);
    return TCL_OK;
}

/* cs::stats_raw OCTETS : la voie rapide, lecture directe d'un (const Batch *). */
static int
StatsRawCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_Size len;
    const unsigned char *p;
    const Batch *b;
    Summary sm;
    (void) cd;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "bytes");
	return TCL_ERROR;
    }
    p = Tcl_GetBytesFromObj(interp, objv[1], &len);
    if (p == NULL) {
	return TCL_ERROR;
    }
    /* FR : le contrat binaire est vérifié, jamais supposé.
     * EN : the binary contract is checked, never assumed. */
    if ((size_t) len < sizeof(Batch)
	    || ((uintptr_t) p % _Alignof(Batch)) != 0) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"cs::stats_raw: not a Batch (too short or misaligned)", -1));
	return TCL_ERROR;
    }
    b = (const Batch *) p;
    if ((size_t) len != sizeof(Batch) + (size_t) b->n * sizeof(Sample)) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"cs::stats_raw: %" TCL_SIZE_MODIFIER "d bytes for n=%u "
		"(expected %zu): was the Batch encoded with -align natural?",
		len, (unsigned) b->n, sizeof(Batch) + (size_t) b->n * sizeof(Sample)));
	return TCL_ERROR;
    }
    Compute(b->items, b->n, &sm);
    Tcl_SetObjResult(interp, SummaryBytes(&sm));
    return TCL_OK;
}

/*
 * Lecture d'un champ d'un produit annoté : l'objet annoté se comporte, pour
 * l'API publique, comme la liste de ses champs, dans l'ordre du type.
 * Reading a field of an annotated product: through the public API the object
 * behaves as the list of its fields, in the type's order.
 */
static int
TypeIs(Tcl_Interp *interp, Tcl_Obj *obj, const char *want)
{
    /*
     * FR : AUJOURD'HUI le type d'une valeur annotée se demande à Tcl (`type get`).
     *      Avec une fonction de stub Tcl_GetAnnotation(obj) -> Tcl_Obj*, ce serait
     *      un simple appel C, sans évaluation de script.
     * EN : TODAY a value's type is asked of Tcl (`type get`). With a stub
     *      Tcl_GetAnnotation(obj) -> Tcl_Obj*, this would be a plain C call.
     */
    Tcl_Obj *cmd[3];
    int ok;
    cmd[0] = Tcl_NewStringObj("type", -1);
    cmd[1] = Tcl_NewStringObj("get", -1);
    cmd[2] = obj;
    Tcl_IncrRefCount(cmd[0]); Tcl_IncrRefCount(cmd[1]);
    ok = (Tcl_EvalObjv(interp, 3, cmd, 0) == TCL_OK
	    && strcmp(Tcl_GetString(Tcl_GetObjResult(interp)), want) == 0);
    Tcl_DecrRefCount(cmd[0]); Tcl_DecrRefCount(cmd[1]);
    return ok;
}

/* cs::stats_obj BATCH : la valeur annotée elle-même. */
static int
StatsObjCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_Size nf, ni, i, nsf;
    Tcl_Obj **fields, **items, **sf;
    Tcl_WideInt n;
    Sample *s;
    Summary sm;
    (void) cd;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "batch");
	return TCL_ERROR;
    }
    if (!TypeIs(interp, objv[1], "Batch")) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"cs::stats_obj: argument is not a Batch", -1));
	return TCL_ERROR;
    }
    /* Batch = { n, items } */
    if (Tcl_ListObjGetElements(interp, objv[1], &nf, &fields) != TCL_OK
	    || nf != 2
	    || Tcl_GetWideIntFromObj(interp, fields[0], &n) != TCL_OK
	    || Tcl_ListObjGetElements(interp, fields[1], &ni, &items) != TCL_OK) {
	return TCL_ERROR;
    }
    if (n != ni) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"cs::stats_obj: n does not match the number of items", -1));
	return TCL_ERROR;
    }
    s = (Sample *) Tcl_Alloc((size_t)(ni ? ni : 1) * sizeof(Sample));
    for (i = 0; i < ni; i++) {
	Tcl_WideInt id, flags;
	double v;
	/* Sample = { id, value, flags } */
	if (Tcl_ListObjGetElements(interp, items[i], &nsf, &sf) != TCL_OK
		|| nsf != 3
		|| Tcl_GetWideIntFromObj(interp, sf[0], &id) != TCL_OK
		|| Tcl_GetDoubleFromObj(interp, sf[1], &v) != TCL_OK
		|| Tcl_GetWideIntFromObj(interp, sf[2], &flags) != TCL_OK) {
	    Tcl_Free(s);
	    return TCL_ERROR;
	}
	s[i].id = (uint32_t) id;
	s[i].value = v;
	s[i].flags = (uint8_t) flags;
    }
    Compute(s, (uint32_t) ni, &sm);
    Tcl_Free(s);
    Tcl_SetObjResult(interp, SummaryBytes(&sm));
    return TCL_OK;
}

/* cs::generate N SEED : C fabrique un Batch (struct) et le rend en octets. */
static int
GenerateCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    Tcl_WideInt n, seed;
    uint32_t i;
    Batch *b;
    size_t size;
    uint64_t x;
    (void) cd;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "n seed");
	return TCL_ERROR;
    }
    if (Tcl_GetWideIntFromObj(interp, objv[1], &n) != TCL_OK
	    || Tcl_GetWideIntFromObj(interp, objv[2], &seed) != TCL_OK) {
	return TCL_ERROR;
    }
    if (n < 0 || n > 100000) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("n must be in 0..100000", -1));
	return TCL_ERROR;
    }
    size = sizeof(Batch) + (size_t) n * sizeof(Sample);
    b = (Batch *) Tcl_Alloc(size);
    memset(b, 0, size);			/* remplissage à zéro, comme type binary */
    b->n = (uint32_t) n;
    x = (uint64_t) seed * 6364136223846793005ULL + 1442695040888963407ULL;
    for (i = 0; i < b->n; i++) {
	x = x * 6364136223846793005ULL + 1442695040888963407ULL;
	b->items[i].id = i + 1;
	b->items[i].value = (double) ((x >> 40) % 10000) / 100.0 - 50.0;
	b->items[i].flags = (uint8_t) (((x >> 20) % 10) == 0);   /* ~10 % invalides */
    }
    Tcl_SetObjResult(interp, Tcl_NewByteArrayObj((unsigned char *) b, size));
    Tcl_Free(b);
    return TCL_OK;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int
Cstruct_Init(Tcl_Interp *interp)
{
    if (Tcl_InitStubs(interp, "9.0", 0) == NULL) {
	return TCL_ERROR;
    }
    Tcl_CreateObjCommand2(interp, "::cs::layout", LayoutCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::cs::stats_raw", StatsRawCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::cs::stats_obj", StatsObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::cs::generate", GenerateCmd, NULL, NULL);
    return Tcl_PkgProvide(interp, "cstruct", "1.0");
}
