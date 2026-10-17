/*
 * Mock de l'API Tcl utilisée par les sources : objets-chaînes, listes et
 * dictionnaires (avec une vraie analyse de listes), tables de hachage,
 * Tcl_DString, analyseur de mots minimal, enregistrement de commandes.
 * Sert à exécuter le VRAI tclTypes.c, tclTypeParse.c, tclAnnotObj.c,
 * tclTypedVar.c et tclTypePath.c sans Tcl.
 */
#include "tclTypesInt.h"
#include <errno.h>

struct Tcl_Interp { Tcl_Obj *result; char errorCode[160]; };
long mock_live_objs = 0;

/* ---- mémoire ---------------------------------------------------------- */
char *Tcl_Alloc(size_t n) { return malloc(n ? n : 1); }
char *Tcl_Realloc(void *p, size_t n) { return realloc(p, n ? n : 1); }
char *Tcl_AttemptAlloc(size_t n) { return malloc(n ? n : 1); }
char *Tcl_AttemptRealloc(void *p, size_t n) { return realloc(p, n ? n : 1); }
void Tcl_Free(void *p) { free(p); }

/* ---- objets ----------------------------------------------------------- */
Tcl_Obj *Tcl_NewObj(void) {
    Tcl_Obj *o = calloc(1, sizeof(*o));
    o->bytes = strdup("");
    mock_live_objs++;
    return o;
}
static void Invalidate(Tcl_Obj *o) { free(o->bytes); o->bytes = NULL; o->length = 0; }
Tcl_Obj *Tcl_NewStringObj(const char *s, Tcl_Size len) {
    Tcl_Obj *o = Tcl_NewObj();
    if (len < 0) len = strlen(s);
    free(o->bytes);
    o->bytes = malloc(len + 1); memcpy(o->bytes, s, len); o->bytes[len] = 0;
    o->length = len;
    return o;
}
void Tcl_SetStringObj(Tcl_Obj *o, const char *s, Tcl_Size len) {
    if (len < 0) len = strlen(s);
    for (Tcl_Size i = 0; i < o->nelems; i++) Tcl_DecrRefCount(o->elems[i]);
    free(o->elems); o->elems = NULL; o->nelems = o->capelems = 0; o->listRep = 0;
    if (o->typePtr && o->typePtr->freeIntRepProc) o->typePtr->freeIntRepProc(o);
    o->typePtr = NULL;
    char *b = malloc(len + 1); memcpy(b, s, len); b[len] = 0;
    free(o->bytes); o->bytes = b; o->length = len;
}
void Tcl_DecrRefCount(Tcl_Obj *o) {
    if (--o->refCount <= 0) {
        if (o->typePtr && o->typePtr->freeIntRepProc) o->typePtr->freeIntRepProc(o);
        for (Tcl_Size i = 0; i < o->nelems; i++) Tcl_DecrRefCount(o->elems[i]);
        free(o->elems); free(o->bytes); free(o);
        mock_live_objs--;
    }
}
void TclInvalidateStringRep(Tcl_Obj *o) { Invalidate(o); }
void Tcl_InitStringRep(Tcl_Obj *o, const char *s, Tcl_Size len) {
    free(o->bytes); o->bytes = malloc(len + 1); memcpy(o->bytes, s, len);
    o->bytes[len] = 0; o->length = len;
}
Tcl_ObjInternalRep *Tcl_FetchInternalRep(Tcl_Obj *o, const Tcl_ObjType *t) {
    return (o->typePtr == t) ? &o->internalRep : NULL;
}
void Tcl_StoreInternalRep(Tcl_Obj *o, const Tcl_ObjType *t, const Tcl_ObjInternalRep *ir) {
    if (o->typePtr && o->typePtr->freeIntRepProc) o->typePtr->freeIntRepProc(o);
    o->typePtr = t; o->internalRep = *ir;
}

/* ---- listes : forme chaîne <-> forme liste ----------------------------- */
static int NeedsBraces(const char *s) {
    if (!*s) return 1;
    for (; *s; s++) if (isspace((unsigned char) *s) || strchr("{}\"\\[]$;", *s)) return 1;
    return 0;
}
static void ListToString(Tcl_Obj *o) {
    size_t cap = 64, l = 0; char *b = malloc(cap);
    for (Tcl_Size i = 0; i < o->nelems; i++) {
        const char *s = Tcl_GetString(o->elems[i]);
        int br = NeedsBraces(s);
        size_t need = strlen(s) + 4;
        while (l + need >= cap) { cap *= 2; b = realloc(b, cap); }
        if (i) b[l++] = ' ';
        if (br) b[l++] = '{';
        memcpy(b + l, s, strlen(s)); l += strlen(s);
        if (br) b[l++] = '}';
    }
    b[l] = 0; free(o->bytes); o->bytes = b; o->length = l;
}
const char *Tcl_GetString(Tcl_Obj *o) {
    if (o->bytes == NULL) {
        if (o->typePtr && o->typePtr->updateStringProc) o->typePtr->updateStringProc(o);
        else if (o->listRep) ListToString(o);
    }
    return o->bytes ? o->bytes : "";
}
const char *Tcl_GetStringFromObj(Tcl_Obj *o, Tcl_Size *l) {
    const char *s = Tcl_GetString(o); if (l) *l = strlen(s); return s;
}
Tcl_Obj *Tcl_NewListObj(Tcl_Size n, Tcl_Obj *const *v) {
    Tcl_Obj *o = Tcl_NewObj(); o->listRep = 1; Invalidate(o);
    for (Tcl_Size i = 0; i < n; i++) Tcl_ListObjAppendElement(NULL, o, v[i]);
    return o;
}
Tcl_Obj *Tcl_NewDictObj(void) { return Tcl_NewListObj(0, NULL); }
static void Push(Tcl_Obj *l, Tcl_Obj *e) {
    if (l->nelems == l->capelems) {
        l->capelems = l->capelems ? 2 * l->capelems : 8;
        l->elems = realloc(l->elems, l->capelems * sizeof(Tcl_Obj *));
    }
    e->refCount++; l->elems[l->nelems++] = e;
}
static int EnsureList(Tcl_Interp *in, Tcl_Obj *o) {
    if (o->listRep) return TCL_OK;
    const char *s = Tcl_GetString(o), *p = s;
    while (*p) {
        while (isspace((unsigned char) *p)) p++;
        if (!*p) break;
        const char *a; size_t n;
        if (*p == '{') {
            int d = 1; a = ++p;
            while (*p && d) { if (*p == '{') d++; else if (*p == '}') d--; if (d) p++; }
            if (d) { if (in) Tcl_SetObjResult(in, Tcl_ObjPrintf("unmatched open brace in list")); return TCL_ERROR; }
            n = p - a; p++;
        } else if (*p == '"') {
            a = ++p; while (*p && *p != '"') p++;
            n = p - a; if (*p) p++;
        } else { a = p; while (*p && !isspace((unsigned char) *p)) p++; n = p - a; }
        Push(o, Tcl_NewStringObj(a, n));          /* Push : la liste prend une référence */
    }
    o->listRep = 1;
    return TCL_OK;
}
int Tcl_ListObjAppendElement(Tcl_Interp *in, Tcl_Obj *l, Tcl_Obj *e) {
    if (EnsureList(in, l) != TCL_OK) return TCL_ERROR;
    Invalidate(l); Push(l, e); return TCL_OK;
}
int Tcl_ListObjGetElements(Tcl_Interp *in, Tcl_Obj *l, Tcl_Size *n, Tcl_Obj ***v) {
    if (EnsureList(in, l) != TCL_OK) return TCL_ERROR;
    *n = l->nelems; *v = l->elems; return TCL_OK;
}
int Tcl_ListObjLength(Tcl_Interp *in, Tcl_Obj *l, Tcl_Size *n) {
    if (EnsureList(in, l) != TCL_OK) return TCL_ERROR;
    *n = l->nelems; return TCL_OK;
}
int Tcl_ListObjIndex(Tcl_Interp *in, Tcl_Obj *l, Tcl_Size i, Tcl_Obj **o) {
    if (EnsureList(in, l) != TCL_OK) return TCL_ERROR;
    *o = (i >= 0 && i < l->nelems) ? l->elems[i] : NULL; return TCL_OK;
}
int Tcl_ListObjReplace(Tcl_Interp *in, Tcl_Obj *l, Tcl_Size first, Tcl_Size count,
        Tcl_Size objc, Tcl_Obj *const objv[]) {
    if (EnsureList(in, l) != TCL_OK) return TCL_ERROR;
    if (first > l->nelems) first = l->nelems;
    if (first + count > l->nelems) count = l->nelems - first;
    Tcl_Size nn = l->nelems - count + objc;
    Tcl_Obj **nv = malloc((nn ? nn : 1) * sizeof(Tcl_Obj *)); Tcl_Size k = 0;
    for (Tcl_Size i = 0; i < first; i++) nv[k++] = l->elems[i];
    for (Tcl_Size i = 0; i < objc; i++) { objv[i]->refCount++; nv[k++] = objv[i]; }
    for (Tcl_Size i = first + count; i < l->nelems; i++) nv[k++] = l->elems[i];
    for (Tcl_Size i = first; i < first + count; i++) Tcl_DecrRefCount(l->elems[i]);
    free(l->elems); l->elems = nv; l->nelems = l->capelems = nn;
    Invalidate(l); return TCL_OK;
}
Tcl_Obj *Tcl_DuplicateObj(Tcl_Obj *src) {
    Tcl_Obj *d = Tcl_NewObj();
    free(d->bytes); d->bytes = src->bytes ? strdup(src->bytes) : NULL; d->length = src->length;
    if (src->typePtr) {
        if (src->typePtr->dupIntRepProc) src->typePtr->dupIntRepProc(src, d);
        else { d->typePtr = src->typePtr; d->internalRep = src->internalRep; }
    }
    d->listRep = src->listRep;
    for (Tcl_Size i = 0; i < src->nelems; i++) Push(d, src->elems[i]);
    return d;
}
void Tcl_AppendObjToObj(Tcl_Obj *o, Tcl_Obj *app) {
    const char *a = Tcl_GetString(o), *b = Tcl_GetString(app);
    char *n = malloc(strlen(a) + strlen(b) + 1); strcpy(n, a); strcat(n, b);
    free(o->bytes); o->bytes = n; o->length = strlen(n);
}

/* ---- dictionnaires : liste de paires ----------------------------------- */
static int EnsureDict(Tcl_Interp *in, Tcl_Obj *d) {
    if (EnsureList(in, d) != TCL_OK) return TCL_ERROR;
    if (d->nelems % 2) {
        if (in) Tcl_SetObjResult(in, Tcl_ObjPrintf("missing value to go with key"));
        return TCL_ERROR;
    }
    return TCL_OK;
}
int Tcl_DictObjGet(Tcl_Interp *in, Tcl_Obj *d, Tcl_Obj *k, Tcl_Obj **v) {
    if (EnsureDict(in, d) != TCL_OK) return TCL_ERROR;
    *v = NULL;
    for (Tcl_Size i = 0; i < d->nelems; i += 2)
        if (!strcmp(Tcl_GetString(d->elems[i]), Tcl_GetString(k))) { *v = d->elems[i + 1]; break; }
    return TCL_OK;
}
int Tcl_DictObjPut(Tcl_Interp *in, Tcl_Obj *d, Tcl_Obj *k, Tcl_Obj *v) {
    if (EnsureDict(in, d) != TCL_OK) return TCL_ERROR;
    for (Tcl_Size i = 0; i < d->nelems; i += 2)
        if (!strcmp(Tcl_GetString(d->elems[i]), Tcl_GetString(k))) {
            v->refCount++; Tcl_DecrRefCount(d->elems[i + 1]); d->elems[i + 1] = v; Invalidate(d);
            return TCL_OK;
        }
    Invalidate(d); Push(d, k); Push(d, v); return TCL_OK;
}
int Tcl_DictObjFirst(Tcl_Interp *in, Tcl_Obj *d, Tcl_DictSearch *s, Tcl_Obj **k, Tcl_Obj **v, int *done) {
    if (EnsureDict(in, d) != TCL_OK) return TCL_ERROR;
    s->dict = d; s->index = 0;
    *done = (d->nelems == 0);
    if (!*done) { *k = d->elems[0]; *v = d->elems[1]; }
    return TCL_OK;
}
void Tcl_DictObjNext(Tcl_DictSearch *s, Tcl_Obj **k, Tcl_Obj **v, int *done) {
    s->index += 2;
    *done = (s->index >= s->dict->nelems);
    if (!*done) { *k = s->dict->elems[s->index]; *v = s->dict->elems[s->index + 1]; }
}
void Tcl_DictObjDone(Tcl_DictSearch *s) { }

/* ---- résultat, erreurs, impression -------------------------------------- */
Tcl_Obj *Tcl_ObjPrintf(const char *fmt, ...) {
    char buf[4096]; va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    return Tcl_NewStringObj(buf, -1);
}
void Tcl_AppendPrintfToObj(Tcl_Obj *o, const char *fmt, ...) {
    char buf[4096]; va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    const char *a = Tcl_GetString(o);
    char *n = malloc(strlen(a) + strlen(buf) + 1); strcpy(n, a); strcat(n, buf);
    free(o->bytes); o->bytes = n; o->length = strlen(n);
}
void Tcl_SetObjResult(Tcl_Interp *in, Tcl_Obj *o) {
    o->refCount++; if (in->result) Tcl_DecrRefCount(in->result); in->result = o;
}
Tcl_Obj *Tcl_GetObjResult(Tcl_Interp *in) {
    if (!in->result) { in->result = Tcl_NewObj(); in->result->refCount = 1; }
    return in->result;
}
void Tcl_ResetResult(Tcl_Interp *in) { if (in && in->result) { Tcl_DecrRefCount(in->result); in->result = NULL; } }
void Tcl_SetErrorCode(Tcl_Interp *in, ...) {
    va_list ap; va_start(ap, in); in->errorCode[0] = 0;
    for (const char *s; (s = va_arg(ap, const char *)) != NULL; ) {
        if (in->errorCode[0]) strcat(in->errorCode, " ");
        strncat(in->errorCode, s, sizeof(in->errorCode) - strlen(in->errorCode) - 2);
    }
    va_end(ap);
}
void Tcl_WrongNumArgs(Tcl_Interp *in, Tcl_Size objc, Tcl_Obj *const objv[], const char *msg) {
    Tcl_SetObjResult(in, Tcl_ObjPrintf("wrong # args: should be \"%s %s\"", Tcl_GetString(objv[0]), msg));
}
void TclDiscard(Tcl_Obj *o);   /* défini par tclAnnotObj.c */

/* ---- nombres ---------------------------------------------------------------- */
int Tcl_GetNumberFromObj(Tcl_Interp *in, Tcl_Obj *o, void **cd, int *type) {
    const char *s = Tcl_GetString(o); char *end;
    while (isspace((unsigned char) *s)) s++;
    errno = 0; (void) strtoll(s, &end, 10);
    if (end != s && !*end) { *type = errno ? TCL_NUMBER_BIG : TCL_NUMBER_INT; return TCL_OK; }
    (void) strtod(s, &end);
    if (end != s && !*end) { *type = TCL_NUMBER_DOUBLE; return TCL_OK; }
    if (in) Tcl_SetObjResult(in, Tcl_ObjPrintf("expected number but got \"%s\"", Tcl_GetString(o)));
    return TCL_ERROR;
}
int Tcl_GetDoubleFromObj(Tcl_Interp *in, Tcl_Obj *o, double *d) {
    const char *s = Tcl_GetString(o); char *end;
    while (isspace((unsigned char) *s)) s++;
    *d = strtod(s, &end);
    if (end != s && !*end) return TCL_OK;
    if (in) Tcl_SetObjResult(in, Tcl_ObjPrintf("expected floating-point number but got \"%s\"", Tcl_GetString(o)));
    return TCL_ERROR;
}
Tcl_Obj *Tcl_NewWideIntObj(Tcl_WideInt v) { char b[40]; snprintf(b, sizeof b, "%lld", v); return Tcl_NewStringObj(b, -1); }
Tcl_Obj *Tcl_NewDoubleObj(double v) {
    char b[48]; snprintf(b, sizeof b, "%.17g", v);
    if (!strpbrk(b, ".eEnN")) strcat(b, ".0");
    return Tcl_NewStringObj(b, -1);
}
int Tcl_InitBignumFromDouble(Tcl_Interp *in, double d, mp_int *b) { b->dummy = 1; return TCL_OK; }
Tcl_Obj *Tcl_NewBignumObj(mp_int *b) { return Tcl_NewStringObj("BIGNUM", -1); }
int TclGetIntForIndexM(Tcl_Interp *in, Tcl_Obj *o, Tcl_Size end, Tcl_Size *idx) {
    const char *s = Tcl_GetString(o); char *e;
    if (!strncmp(s, "end", 3)) {
        long off = 0;
        if (s[3] == '+' || s[3] == '-') { off = strtol(s + 3, &e, 10); if (*e) goto bad; }
        else if (s[3]) goto bad;
        *idx = end + off; return TCL_OK;
    }
    long v = strtol(s, &e, 10);
    if (e == s || *e) goto bad;
    *idx = v; return TCL_OK;
  bad:
    if (in) Tcl_SetObjResult(in, Tcl_ObjPrintf("bad index \"%s\": must be integer?[+-]integer? or end?[+-]integer?", s));
    return TCL_ERROR;
}

/* ---- Tcl_DString ------------------------------------------------------------ */
void Tcl_DStringInit(Tcl_DString *d) { d->string = malloc(1); d->string[0] = 0; d->length = 0; d->spaceAvl = 1; }
void Tcl_DStringAppend(Tcl_DString *d, const char *s, Tcl_Size len) {
    if (len < 0) len = strlen(s);
    d->string = realloc(d->string, d->length + len + 1);
    memcpy(d->string + d->length, s, len); d->length += len; d->string[d->length] = 0;
}
void Tcl_DStringFree(Tcl_DString *d) { free(d->string); d->string = NULL; d->length = 0; }

/* ---- table de hachage (clés chaînes) ---------------------------------------- */
struct Tcl_HashEntry { char *key; void *value; struct Tcl_HashEntry *next; int dead; };
void Tcl_InitHashTable(Tcl_HashTable *t, int k) { t->opaque = NULL; }
Tcl_HashEntry *Tcl_FindHashEntry(Tcl_HashTable *t, const void *key) {
    for (Tcl_HashEntry *e = t->opaque; e; e = e->next) if (!e->dead && !strcmp(e->key, key)) return e;
    return NULL;
}
Tcl_HashEntry *Tcl_CreateHashEntry(Tcl_HashTable *t, const void *key, int *isNew) {
    Tcl_HashEntry *e = Tcl_FindHashEntry(t, key);
    if ((*isNew = (e == NULL))) { e = calloc(1, sizeof *e); e->key = strdup(key); e->next = t->opaque; t->opaque = e; }
    return e;
}
void *Tcl_GetHashValue(Tcl_HashEntry *e) { return e->value; }
void Tcl_SetHashValue(Tcl_HashEntry *e, void *v) { e->value = v; }
Tcl_HashEntry *Tcl_FirstHashEntry(Tcl_HashTable *t, Tcl_HashSearch *s) { s->opaque = t->opaque; return Tcl_NextHashEntry(s); }
Tcl_HashEntry *Tcl_NextHashEntry(Tcl_HashSearch *s) {
    Tcl_HashEntry *e = s->opaque;
    while (e && e->dead) e = e->next;
    s->opaque = e ? e->next : NULL;
    return e;
}
void Tcl_DeleteHashTable(Tcl_HashTable *t) { }
const char *Tcl_GetHashKey(Tcl_HashTable *t, Tcl_HashEntry *e) { return e->key; }
/* Détruit l'entrée : marquée morte (jamais libérée, pour que les
 * recherches en cours restent valides). */
void Tcl_DeleteHashEntry(Tcl_HashEntry *e) { e->dead = 1; }

/* ---- données associées, commandes ------------------------------------------- */
static void *assoc_data;
void *Tcl_GetAssocData(Tcl_Interp *in, const char *k, void *p) { return assoc_data; }
void Tcl_SetAssocData(Tcl_Interp *in, const char *k, void (*p)(void *, Tcl_Interp *), void *c) { assoc_data = c; }
static Tcl_ObjCmdProc2 *cmd_proc; static void *cmd_data;
void *Tcl_CreateObjCommand2(Tcl_Interp *in, const char *name, Tcl_ObjCmdProc2 *proc, void *cd, void (*del)(void *)) {
    if (strcmp(name, "type") == 0) { cmd_proc = proc; cmd_data = cd; }   /* seule la commande « type » est pilotée */
    return NULL;
}
int mock_type_command(Tcl_Interp *in, Tcl_Size objc, Tcl_Obj *const objv[]) { return cmd_proc(cmd_data, in, objc, objv); }
int Tcl_EvalObjv(Tcl_Interp *in, Tcl_Size n, Tcl_Obj *const v[], int f) { abort(); }

/* ---- analyseur de mots de Tcl (version minimale) ----------------------------- */
void TclParseInit(Tcl_Interp *in, const char *s, Tcl_Size n, Tcl_Parse *p) {
    memset(p, 0, sizeof(*p)); p->string = s; p->end = s + n;
    p->tokensAvailable = 4; p->tokenPtr = malloc(p->tokensAvailable * sizeof(Tcl_Token));
}
void TclGrowParseTokenArray(Tcl_Parse *p, Tcl_Size app) {
    if (p->numTokens + app > p->tokensAvailable) {
        p->tokensAvailable = 2 * (p->numTokens + app);
        p->tokenPtr = realloc(p->tokenPtr, p->tokensAvailable * sizeof(Tcl_Token));
    }
}
void Tcl_FreeParse(Tcl_Parse *p) { free(p->tokenPtr); p->tokenPtr = NULL; }
Tcl_Size TclParseAllWhiteSpace(const char *s, Tcl_Size n) {
    Tcl_Size i = 0;
    while (i < n) {
        if (isspace((unsigned char) s[i])) i++;
        else if (s[i] == '\\' && i + 1 < n && s[i + 1] == '\n') i += 2;
        else break;
    }
    return i;
}
static int ParseDelimited(const char *s, Tcl_Size n, Tcl_Parse *p, const char **term, char open, char close) {
    Tcl_Size i = 1; int depth = 1;
    if (n < 1 || s[0] != open) return TCL_ERROR;
    for (; i < n; i++) {
        if (s[i] == '\\') { i++; continue; }
        if (open == '{') { if (s[i] == '{') depth++; else if (s[i] == '}' && --depth == 0) break; }
        else if (s[i] == close) break;
    }
    if (i >= n) { p->incomplete = 1; return TCL_ERROR; }
    TclGrowParseTokenArray(p, 2);
    Tcl_Token *t = p->tokenPtr + p->numTokens++;
    t->type = TCL_TOKEN_TEXT; t->start = s + 1; t->size = i - 1; t->numComponents = 0;
    if (open == '"') for (Tcl_Size k = 1; k < i; k++)
        if (s[k] == '$' || s[k] == '[') {
            Tcl_Token *v = p->tokenPtr + p->numTokens++;
            v->type = TCL_TOKEN_VARIABLE; v->start = s + k; v->size = 1; v->numComponents = 0; break;
        }
    *term = s + i + 1;
    return TCL_OK;
}
int Tcl_ParseQuotedString(Tcl_Interp *in, const char *s, Tcl_Size n, Tcl_Parse *p, int app, const char **term) { return ParseDelimited(s, n, p, term, '"', '"'); }
int Tcl_ParseBraces(Tcl_Interp *in, const char *s, Tcl_Size n, Tcl_Parse *p, int app, const char **term) { return ParseDelimited(s, n, p, term, '{', '}'); }
int TclWordKnownAtCompileTime(Tcl_Token *w, Tcl_Obj *val) {
    char *buf = malloc(w->size + 2); size_t len = 0;
    for (Tcl_Size i = 1; i <= w->numComponents; i++) {
        Tcl_Token *t = w + i;
        if (t->type != TCL_TOKEN_TEXT && t->type != TCL_TOKEN_BS) { free(buf); return 0; }
        for (Tcl_Size k = 0; k < t->size; k++) {
            char c = t->start[k];
            if (c == '\\' && k + 1 < t->size) { c = t->start[++k]; if (c == 'n') c = '\n'; }
            buf[len++] = c;
        }
    }
    free(val->bytes); val->bytes = malloc(len + 1); memcpy(val->bytes, buf, len); val->bytes[len] = 0; free(buf);
    return 1;
}


/* ---- Tcl_ParseCommand / Tcl_ParseVarName : versions minimales ---------------- */
int Tcl_ParseCommand(Tcl_Interp *in, const char *s, Tcl_Size n, int nested, Tcl_Parse *p) {
    Tcl_Size i = 0; int depth;
    TclParseInit(in, s, n, p);
    p->commandStart = s;
    while (i < n) {
        char c = s[i];
        if (c == '\\') { i += 2; continue; }
        if (c == '{') {
            depth = 1; i++;
            while (i < n && depth > 0) { if (s[i] == '\\') i++; else if (s[i] == '{') depth++; else if (s[i] == '}') depth--; i++; }
            if (depth > 0) { p->incomplete = 1; return TCL_ERROR; }
            continue;
        }
        if (c == '"') {
            i++;
            while (i < n && s[i] != '"') { if (s[i] == '\\') i++; i++; }
            if (i >= n) { p->incomplete = 1; return TCL_ERROR; }
            i++; continue;
        }
        if (c == '[') {
            depth = 1; i++;
            while (i < n && depth > 0) { if (s[i] == '\\') i++; else if (s[i] == '[') depth++; else if (s[i] == ']') depth--; i++; }
            if (depth > 0) { p->incomplete = 1; return TCL_ERROR; }
            continue;
        }
        if (c == ';' || c == '\n') { p->term = s + i; p->commandSize = i + 1; return TCL_OK; }
        if (c == ']' && nested) { p->term = s + i; p->commandSize = i + 1; return TCL_OK; }
        i++;
    }
    p->term = s + n; p->commandSize = n;
    return TCL_OK;
}
int Tcl_ParseVarName(Tcl_Interp *in, const char *s, Tcl_Size n, Tcl_Parse *p, int app) {
    Tcl_Size i = 1, ns, ne; Tcl_Token *t;
    TclParseInit(in, s, n, p);
    TclGrowParseTokenArray(p, 4);
    if (n < 2 || s[0] != '$') return TCL_ERROR;
    if (s[1] == '{') {
        ns = 2; for (i = 2; i < n && s[i] != '}'; i++) {}
        if (i >= n) return TCL_ERROR;
        ne = i; i++;
    } else {
        ns = 1;
        while (i < n && (isalnum((unsigned char) s[i]) || s[i] == '_' || (s[i] == ':' && i + 1 < n && s[i + 1] == ':') ||
               (s[i] == ':' && i > 0 && s[i - 1] == ':'))) i++;
        ne = i;
        if (ne == ns) { t = p->tokenPtr + p->numTokens++; t->type = TCL_TOKEN_TEXT; t->start = s; t->size = 1; t->numComponents = 0; return TCL_OK; }
    }
    t = p->tokenPtr + p->numTokens++; t->type = TCL_TOKEN_VARIABLE; t->start = s; t->numComponents = 1;
    t = p->tokenPtr + p->numTokens++; t->type = TCL_TOKEN_TEXT; t->start = s + ns; t->size = ne - ns; t->numComponents = 0;
    if (s[1] != '{' && i < n && s[i] == '(') {
        Tcl_Size j = i + 1; int d = 1;
        while (j < n && d > 0) { if (s[j] == '(') d++; else if (s[j] == ')') d--; j++; }
        if (d == 0) {
            t = p->tokenPtr + p->numTokens++; t->type = TCL_TOKEN_TEXT; t->start = s + i + 1; t->size = j - i - 2; t->numComponents = 0;
            p->tokenPtr[0].numComponents = 2; i = j;
        }
    }
    p->tokenPtr[0].size = i;
    return TCL_OK;
}

/* ---- évaluateur de scripts minimal : un mot = une commande enregistrée -------- */
typedef struct { char *name; MockCmdProc *proc; } MockCmd;
static MockCmd mock_cmds[32]; static int mock_ncmds;
void mock_register_command(const char *name, MockCmdProc *proc) {
    for (int i = 0; i < mock_ncmds; i++) if (strcmp(mock_cmds[i].name, name) == 0) { mock_cmds[i].proc = proc; return; }
    mock_cmds[mock_ncmds].name = strdup(name); mock_cmds[mock_ncmds++].proc = proc;
}
int Tcl_EvalObjEx(Tcl_Interp *in, Tcl_Obj *script, int flags) {
    Tcl_Size argc, i; Tcl_Obj **argo; const char *argv[16]; Tcl_Obj *res = NULL;
    if (Tcl_ListObjGetElements(in, script, &argc, &argo) != TCL_OK || argc == 0 || argc > 16) {
        Tcl_SetObjResult(in, Tcl_NewStringObj("empty or malformed script", -1)); return TCL_ERROR;
    }
    for (i = 0; i < argc; i++) argv[i] = Tcl_GetString(argo[i]);
    for (int k = 0; k < mock_ncmds; k++) if (strcmp(mock_cmds[k].name, argv[0]) == 0) {
        int rc = mock_cmds[k].proc(in, argc, argv, &res);
        Tcl_SetObjResult(in, res ? res : Tcl_NewStringObj("", -1));
        return rc;
    }
    Tcl_SetObjResult(in, Tcl_ObjPrintf("invalid command name \"%s\"", argv[0]));
    return TCL_ERROR;
}

/* ---- variables : jamais appelées par les tests (liaison seulement) ---------- */
Var *TclObjLookupVarEx(Tcl_Interp *in, Tcl_Obj *p1, Tcl_Obj *p2, int f, const char *m, int c1, int c2, Var **a) { abort(); }
Tcl_Obj *Tcl_ObjSetVar2(Tcl_Interp *in, Tcl_Obj *p1, Tcl_Obj *p2, Tcl_Obj *v, int f) { abort(); }
int Tcl_SetObjCmd(void *cd, Tcl_Interp *in, Tcl_Size objc, Tcl_Obj *const objv[]) { abort(); }

/* ---- aides de test ---------------------------------------------------------- */
Tcl_Interp *mock_interp(void) { return calloc(1, sizeof(struct Tcl_Interp)); }
const char *mock_result(Tcl_Interp *in) { return in->result ? Tcl_GetString(in->result) : "(aucun message)"; }
const char *mock_errorcode(Tcl_Interp *in) { return in->errorCode; }
void mock_reset(Tcl_Interp *in) { Tcl_ResetResult(in); in->errorCode[0] = 0; }

/* ---- Tcl_SubstObj / Tcl_ExprObj : versions minimales pour les tests de chemins ---- */
Tcl_Obj *Tcl_SubstObj(Tcl_Interp *in, Tcl_Obj *o, int flags) {
    const char *t = Tcl_GetString(o);
    Tcl_Obj *v;
    if (t[0] == '$' && TclVarRefGet(in, t + 1, &v) == TCL_OK) return Tcl_DuplicateObj(v);
    return Tcl_NewStringObj(t, -1);
}
int Tcl_ExprObj(Tcl_Interp *in, Tcl_Obj *o, Tcl_Obj **res) {
    long a, b; char op; const char *t = Tcl_GetString(o);
    if (sscanf(t, "(%ld%c%ld)", &a, &op, &b) == 3) {
        long r = op == '+' ? a + b : op == '-' ? a - b : op == '*' ? a * b : 0;
        *res = Tcl_NewWideIntObj(r);
    } else if (sscanf(t, "(%ld)", &a) == 1) {
        *res = Tcl_NewWideIntObj(a);
    } else { Tcl_SetObjResult(in, Tcl_NewStringObj("expr: unsupported in mock", -1)); return TCL_ERROR; }
    Tcl_IncrRefCount(*res);
    return TCL_OK;
}
Tcl_Obj *TclPtrSetVarIdx(Tcl_Interp *in, Var *v, Var *a, Tcl_Obj *p1, Tcl_Obj *p2, Tcl_Obj *n, int f, int i) { abort(); }
