/*
 * Banc de test du système de types, avec le VRAI tclTypes.c, tclTypeParse.c,
 * tclAnnotObj.c, tclTypedVar.c et tclTypePath.c sur un mock de l'API Tcl.
 * tclTypedVar.c est inclus pour atteindre ses fonctions statiques
 * (BuildValue), exactement ce qu'appelle "set {T}nom args".
 */
#include "../tclTypedVar.c"

extern long mock_live_objs;
Tcl_Interp *mock_interp(void);
const char *mock_result(Tcl_Interp *);
const char *mock_errorcode(Tcl_Interp *);
void mock_reset(Tcl_Interp *);
int mock_type_command(Tcl_Interp *in, Tcl_Size objc, Tcl_Obj *const objv[]);

static Tcl_Interp *in;
static int total, fails;

static Tcl_Obj *S(const char *s) { return Tcl_NewStringObj(s, -1); }

static void Report(int ok, const char *group, const char *label, const char *expect, const char *got)
{
    total++; if (!ok) fails++;
    printf("%s  %-9s %-46.46s -> %s\n", ok ? "pass" : "FAIL", group, label, got);
    if (!ok) printf("             attendu : %s\n", expect);
}

/* "OK <texte>" : égalité exacte ; "ERR <fragment>" : échec dont le message contient le fragment. */
static int Match(const char *expect, int rc, const char *okText, const char *errText)
{
    if (expect[0] == 'O') return rc == TCL_OK && (expect[2] == '\0' || !strcmp(okText, expect + 3));
    return rc != TCL_OK && (expect[3] == '\0' || strstr(errText, expect + 4) != NULL);
}

/* type def name text */
static int Def(const char *name, const char *text)
{
    Tcl_Obj *v[4]; int rc;
    v[0] = S("type"); v[1] = S("def"); v[2] = S(name); v[3] = S(text);
    for (int i = 0; i < 4; i++) v[i]->refCount++;
    rc = mock_type_command(in, 4, v);
    for (int i = 0; i < 4; i++) Tcl_DecrRefCount(v[i]);
    return rc;
}

/* Définit un type et attend un résultat. */
static void DefCheck(const char *group, const char *name, const char *text, const char *expect)
{
    int rc = Def(name, text);
    char label[200]; snprintf(label, sizeof label, "type def %s %s", name, text);
    Report(Match(expect, rc, "", mock_result(in)), group, label, expect, rc == TCL_OK ? "défini" : mock_result(in));
    mock_reset(in);
}

/* Analyse un texte de type : nom canonique ou message. */
static void Canon(const char *group, const char *text, const char *expect)
{
    TclType *tp = TclTypeParse(in, text, strlen(text));
    char label[200]; snprintf(label, sizeof label, "%s", strchr(text, '\n') ? "(multiligne)" : text);
    Report(Match(expect, tp ? TCL_OK : TCL_ERROR, tp ? tp->name : "", mock_result(in)), group, label, expect,
           tp ? tp->name : mock_result(in));
    mock_reset(in);
}

/* TclTypeComplete : valide puis remplit les défauts. */
static void Comp(const char *group, const char *typeText, const char *value, const char *expect)
{
    TclType *tp = TclTypeParse(in, typeText, strlen(typeText));
    Tcl_Obj *o = S(value); int rc; char label[200];
    snprintf(label, sizeof label, "%s  <-  {%s}", typeText, value);
    if (!tp) { Report(0, group, label, expect, mock_result(in)); mock_reset(in); return; }
    o->refCount++;
    rc = TclTypeComplete(in, tp, o, -1, NULL);
    Report(Match(expect, rc, Tcl_GetString(o), mock_result(in)), group, label, expect,
           rc == TCL_OK ? Tcl_GetString(o) : mock_result(in));
    Tcl_DecrRefCount(o); mock_reset(in);
}

/* Deux textes de type désignent-ils le MÊME descripteur (interning) ? */
static void Same(const char *group, const char *a, const char *b, int expect)
{
    TclType *ta = TclTypeParse(in, a, strlen(a)), *tb;
    char label[200]; int ok;
    mock_reset(in);
    tb = TclTypeParse(in, b, strlen(b));
    mock_reset(in);
    snprintf(label, sizeof label, "%s  ==  %s", a, b);
    ok = ta != NULL && tb != NULL && ((ta == tb) == expect);
    Report(ok, group, label, expect ? "même type" : "types distincts", (ta == tb) ? "même type" : "types distincts");
}

static void Elem(const char *group, const char *typeText, int expect)
{
    TclType *tp = TclTypeParse(in, typeText, strlen(typeText));
    int ok = tp != NULL && tp->fields != NULL && tp->fields[0].type->elem == tp;
    Report(ok == expect, group, typeText, expect ? "auto-référence" : "autre", ok ? "auto-référence" : "autre");
    mock_reset(in);
}

static void Inc(const char *group, const char *a, const char *b, int expect)
{
    TclType *ta = TclTypeParse(in, a, strlen(a)), *tb = TclTypeParse(in, b, strlen(b));
    char label[200]; snprintf(label, sizeof label, "%s  ⊂  %s", a, b);
    int got = (ta && tb) ? TclTypeIncludes(ta, tb) : -1;
    Report(got == expect, group, label, expect ? "oui" : "non", got < 0 ? mock_result(in) : got ? "oui" : "non");
    mock_reset(in);
}

/* BuildValue : ce que fait "set {T}P args...". Puis, comme TclSetAnnotatedCmd, la validation du conteneur. */
static void Build(const char *group, const char *typeText, const char *expect, int n, ...)
{
    TclType *tp = TclTypeParse(in, typeText, strlen(typeText));
    Tcl_Obj *a[16], *c = NULL; int variant = -1, rc; va_list ap; char label[240], cmd[200];
    size_t l = 0;
    va_start(ap, n);
    for (int i = 0; i < n; i++) { a[i] = S(va_arg(ap, const char *)); a[i]->refCount++; l += snprintf(cmd + l, sizeof cmd - l, " %s", Tcl_GetString(a[i])); }
    va_end(ap);
    snprintf(label, sizeof label, "set {%s}P%s", typeText, cmd);
    rc = BuildValue(in, tp, 0, n, a, &c, &variant);
    if (rc == TCL_OK && (tp->kind == TCLT_LIST || tp->kind == TCLT_DICT))
        rc = TclTypeComplete(in, tp, c, -1, NULL);
    Report(Match(expect, rc, rc == TCL_OK ? Tcl_GetString(c) : "", mock_result(in)), group, label, expect,
           rc == TCL_OK ? Tcl_GetString(c) : mock_result(in));
    if (c) TclDiscard(c);
    for (int i = 0; i < n; i++) Tcl_DecrRefCount(a[i]);
    mock_reset(in);
}

/* Écriture par chemin dans une valeur typée. */
static void PathW(const char *group, const char *typeText, const char *content, const char *path,
        const char *value, const char *expect)
{
    TclType *tp = TclTypeParse(in, typeText, strlen(typeText));
    Tcl_Obj *cont = TclNewAnnotated(in, tp, -1, S(content)), *val = S(value), *res = NULL;
    Tcl_Obj *vals[1]; char label[240]; int rc;
    cont->refCount++; val->refCount++; vals[0] = val;
    snprintf(label, sizeof label, "%s : {%s}  P(%s) <- %s", typeText, content, path, value);
    rc = TclTypePathWrite(in, cont, path, 1, vals, &res);
    Tcl_Obj *c2 = NULL;
    if (rc == TCL_OK) TclAnnotInfo(res, &c2, NULL, NULL, NULL);
    Report(Match(expect, rc, rc == TCL_OK ? Tcl_GetString(c2) : "", mock_result(in)), group, label, expect,
           rc == TCL_OK ? Tcl_GetString(c2) : mock_result(in));
    if (res) TclDiscard(res);
    Tcl_DecrRefCount(cont); Tcl_DecrRefCount(val); mock_reset(in);
}

int main(void)
{
    in = mock_interp();
    TclTypesInit(in);

    /* ===== types de base (les commandes de l'utilisateur) ===== */
    DefCheck("def", "num", "double, int", "OK");              /* type def num {double, int} */
    DefCheck("def", "pixel", "int", "OK");
    Canon("somme", "num", "OK num");

    /* ===== grammaire : . = ; , { } ===== */
    Canon("gram", "int", "OK int");
    Canon("gram", "list", "OK list");
    Canon("gram", "list.dict", "OK list<dict>");
    Canon("gram", "list.list.int", "OK list<list<int>>");
    Canon("gram", "dict.list.int", "OK dict<list<int>>");
    Canon("gram", " list . int ", "OK list<int>");
    Canon("gram", "{list.int}", "OK list<int>");
    Canon("gram", "{double}x; {double}y", "OK prod<x:double;y:double;>");
    Canon("gram", "{double}x", "OK prod<x:double;>");
    Canon("gram", "{int}i, {double}d", "OK sum<i:int;d:double;>");
    Canon("tri", "{double}d, {int}i", "OK sum<i:int;d:double;>");           /* trié : int avant double */
    Canon("tri", "{double}d, \"none\", {int}i", "OK sum<:\"none\";i:int;d:double;>");
    Canon("tri", "double, int", "OK sum<:int;:double;>");
    Canon("gram", "\"red\", \"green\", \"blue\"", "OK sum<:\"red\";:\"green\";:\"blue\";>");
    Canon("gram", "{{int}x; {int}y}, \"none\"", "OK sum<:prod<x:int;y:int;>;:\"none\";>");
    Canon("gram", "{num}real = 0; {num}img = 0", "OK prod<real:num=\"0\";img:num=\"0\";>");
    Canon("gram", "{{num}real = 0; {num}img = 0}cart, {{num}mod = 0; {num}arg = 0}polar",
          "OK sum<cart:prod<real:num=\"0\";img:num=\"0\";>;polar:prod<mod:num=\"0\";arg:num=\"0\";>;>");
    Canon("gram", "{num}x = -1.5; {pixel}n = 3", "OK prod<x:num=\"-1.5\";n:pixel=\"3\";>");
    Canon("gram", "{string}s = {a b c}", "OK prod<s:string=\"a b c\";>");
    Canon("gram", "# un point\n{double}x;   # abscisse\n{double}y\n", "OK prod<x:double;y:double;>");
    /* erreurs */
    Canon("err", "{double}x, {double}y; {int}z", "OK");
    Canon("err", "{double}x {double}y", "ERR missing operator");
    Canon("err", "{double}x;", "OK prod<x:double;>");   /* ";" final toléré */
    Canon("err", "{double", "ERR unbalanced open brace");
    Canon("err", "double}", "ERR unbalanced close brace");
    Canon("err", "{}", "ERR empty group");
    Canon("err", "", "ERR empty type expression");
    Canon("err", "{double}x = 0 = 1", "ERR left operand of \"=\"");
    Canon("err", "int.list", "ERR can be nested");
    Canon("err", "{int}x; {int}x", "ERR duplicate field name");
    Canon("err", "{double}x = \"abc\"", "ERR expected floating-point number");
    Canon("err", "{string}s = \"a$b\"", "ERR substitutions are not allowed");
    Canon("err", "array", "ERR reserved");
    Canon("err", "foo", "ERR unknown type \"foo\"");

    /* ===== arité d'une liste : list(2), taille exacte ===== */
    Canon("liste", "list(2).int", "OK list(2)<int>");
    Canon("liste", " list( 2 ) . int", "OK list(2)<int>");
    Canon("liste", "list(2)", "OK list(2)<string>");
    Canon("liste", "list(0)", "OK list(0)<string>");
    Canon("liste", "list(2).list(3).int", "OK list(2)<list(3)<int>>");      /* matrice 2x3 */
    Canon("liste", "list.list(3).int", "OK list<list(3)<int>>");
    Canon("liste", "list()", "ERR list needs a size");                      /* décidé : interdit */
    Canon("liste", "list(1:3).int", "ERR bad list size");          /* les intervalles n'existent pas */
    Canon("liste", "list(2:)", "ERR bad list size");
    Canon("liste", "list(x)", "ERR bad list size");
    Canon("liste", "list(-1)", "ERR bad list size");
    Canon("liste", "list(2", "ERR missing close-parenthesis");
    Canon("liste", "list (2)", "ERR invalid character");                    /* la parenthèse doit être collée */
    Canon("liste", "int(2)", "ERR take arguments");
    Canon("liste", "{list(2).int}p; {list(3).double}q", "OK prod<p:list(2)<int>;q:list(3)<double>;>");

    /* ===== clés d'un dictionnaire ===== */
    Canon("dict", "dict", "OK dict");
    Canon("dict", "dict.int", "OK dict<int>");
    Canon("dict", "dict(x, y).int", "OK dict(\"x\",\"y\")<int>");
    Canon("dict", "dict(x, y?, z?).int", "OK dict(\"x\",\"y\"?,\"z\"?)<int>");
    Canon("dict", "dict(x?=0, y?=0, z?=0).int", "OK dict(\"x\"?=\"0\",\"y\"?=\"0\",\"z\"?=\"0\")<int>");
    Canon("dict", "dict(x?=\"\")", "OK dict(\"x\"?=\"\")<string>");
    Canon("dict", "dict()", "OK dict(\"\")<string>");                       /* seule la clé vide */
    Canon("dict", "dict(x,)", "OK dict(\"x\",\"\")<string>");
    Canon("dict", "dict(\"a b\", x)", "OK dict(\"a b\",\"x\")<string>");
    Canon("dict", "dict(x?=\"a,b)\")", "OK dict(\"x\"?=\"a,b)\")<string>");  /* "," et ")" dans une valeur */
    Canon("dict", "dict(x?={a b})", "OK dict(\"x\"?=\"a b\")<string>");
    Canon("dict", "dict(x?=1.5).double", "OK dict(\"x\"?=\"1.5\")<double>");
    Canon("dict", "dict(x?=1.5).num", "OK dict(\"x\"?=\"1.5\")<num>");       /* num accepte 1.5 */
    Canon("dict", "dict(x, y, z).num", "OK dict(\"x\",\"y\",\"z\")<num>");
    Canon("dict", "{dict(x,y).int}a, {dict.int}b", "OK sum<a:dict(\"x\",\"y\")<int>;b:dict<int>;>");
    Canon("dict", "dict(x, x)", "ERR duplicate key");
    Canon("dict", "dict(x=0)", "ERR not optional");
    Canon("dict", "dict(x?=abc).int", "ERR default value \"abc\" of key \"x\"");   /* validé à la définition */
    Canon("dict", "dict(x?=1.5).int", "ERR default value \"1.5\" of key \"x\"");
    Canon("dict", "dict(x y)", "ERR unexpected text");
    Canon("dict", "dict(a?={}).dict(x?=0).int", "OK dict(\"a\"?=\"\")<dict(\"x\"?=\"0\")<int>>");

    /* ===== types nominaux avec arité et clés ===== */
    DefCheck("nominal", "pointL", "list(2).int", "OK");
    DefCheck("nominal", "pointD", "dict(x, y).int", "OK");
    DefCheck("nominal", "opt", "dict(x, y?, z?).int", "OK");
    DefCheck("nominal", "defs", "dict(x?=0, y?=0, z?=0).int", "OK");
    DefCheck("nominal", "point", "dict(x,y,z).num", "OK");                  /* l'exemple de l'utilisateur */
    DefCheck("nominal", "badpoint", "dict(x?=abc).num", "ERR default value");
    DefCheck("nominal", "ppt", "{num}x; {num}y; {num}z", "OK");             /* le produit équivalent */

    /* ===== validation : arité ===== */
    Comp("arité", "pointL", "1 2", "OK 1 2");
    Comp("arité", "pointL", "1", "ERR exactly 2 elements but got 1");
    Comp("arité", "pointL", "1 2 3", "ERR exactly 2 elements but got 3");
    Comp("arité", "pointL", "1 x", "ERR");

    /* ===== validation : clés ===== */
    Comp("clés", "pointD", "x 1 y 2", "OK x 1 y 2");
    Comp("clés", "pointD", "x 1", "ERR key \"y\" is missing");
    Comp("clés", "pointD", "x 1 y 2 z 3", "ERR key \"z\" is not allowed");
    Comp("clés", "pointD", "x a y 2", "ERR");
    Comp("clés", "pointD", "1 2 3", "ERR");                                 /* nombre impair d'éléments */
    Comp("clés", "opt", "x 1", "OK x 1");
    Comp("clés", "opt", "x 1 z 5", "OK x 1 z 5");
    Comp("clés", "opt", "z 5 x 1", "OK x 1 z 5");
    Comp("clés", "pointD", "y 2 x 1", "OK x 1 y 2");
    Comp("clés", "dict.int", "b 2 a 1", "OK b 2 a 1");           /* dict sans contrainte : inchangé */
    Comp("clés", "list.dict(x, y).int", "{y 2 x 1} {x 3 y 4}", "OK {x 1 y 2} {x 3 y 4}");
    Comp("clés", "opt", "y 2", "ERR key \"x\" is missing");
    Comp("clés", "point", "x 1 y 2 z 3", "OK x 1 y 2 z 3");
    Comp("clés", "point", "x 1 y 2.5 z 3", "OK x 1 y 2.5 z 3");            /* num : entier ou flottant */
    Comp("clés", "point", "x 1 y abc z 3", "ERR");

    /* ===== valeurs par défaut des clés manquantes ===== */
    Comp("défaut", "defs", "", "OK x 0 y 0 z 0");                           /* set {point}P {} */
    Comp("défaut", "defs", "y 5", "OK x 0 y 5 z 0");
    Comp("défaut", "defs", "x 1 y 2 z 3", "OK x 1 y 2 z 3");
    Comp("défaut", "defs", "w 1", "ERR key \"w\" is not allowed");
    Comp("défaut", "defs", "x a", "ERR");
    Comp("défaut", "dict(x?=0, y?=0, z?=0).int", "", "OK x 0 y 0 z 0");
    Comp("défaut", "dict(x, y?=7).int", "x 1", "OK x 1 y 7");
    Comp("défaut", "dict(x, y?=7).int", "y 2", "ERR key \"x\" is missing");   /* x obligatoire, sans défaut */
    Comp("défaut", "dict(x, y?=7, z?).int", "x 1", "OK x 1 y 7");            /* z facultative sans défaut : absente */
    Comp("défaut", "dict(a?={}).dict(x?=0).int", "", "OK a {x 0}");          /* défaut qui contient des défauts */
    Comp("défaut", "dict(a?={}).dict(x?=0).int", "a {}", "OK a {x 0}");
    Comp("défaut", "dict(a?={}).dict(x?=0).int", "a {x 5}", "OK a {x 5}");
    Comp("défaut", "list.dict(x?=0).int", "{} {x 5}", "OK {x 0} {x 5}");   /* dans les éléments d'une liste */

    /* ===== inclusion ===== */
    Inc("inclus", "list(2).int", "list.int", 1);
    Inc("inclus", "list.int", "list(2).int", 0);
    Inc("inclus", "list(2).int", "list(3).int", 0);
    Inc("inclus", "int", "list(1).int", 1);
    Inc("inclus", "list(2).int", "list(2).double", 1);
    Inc("inclus", "int", "list(2).int", 0);
    Inc("inclus", "dict(x,y).int", "dict.int", 1);
    Inc("inclus", "dict.int", "dict(x,y).int", 0);
    Inc("inclus", "dict(x,y).int", "dict(x,y,z?).int", 1);
    Inc("inclus", "dict(x,y,z?).int", "dict(x,y).int", 0);
    Inc("inclus", "dict(x).int", "dict(x?).int", 1);
    Inc("inclus", "dict(x?).int", "dict(x).int", 0);
    Inc("inclus", "dict(x,y).int", "dict(x,y).double", 1);
    Inc("inclus", "dict(x,y).int", "list", 1);                              /* un dict est une liste de 2n éléments */
    Inc("inclus", "dict(x,y).int", "list(4).string", 1);
    Inc("inclus", "dict(x,y).int", "list(2).string", 0);

    /* ===== set {point}P ... : la question de l'utilisateur ===== */
    Build("set", "point", "OK x 1 y 2 z 3", 6, "x", "1", "y", "2", "z", "3");
    Build("set", "point", "ERR even number", 3, "1", "2", "3");             /* la forme positionnelle n'existe pas pour un dict */
    Build("set", "point", "ERR key \"1\" is not allowed", 4, "1", "2", "3", "4");
    Build("set", "point", "ERR key \"z\" is missing", 4, "x", "1", "y", "2");
    Build("set", "point", "OK x 1 y 2 z 3", 6, "z", "3", "x", "1", "y", "2");   /* ordre de la définition */
    Build("set", "defs", "OK x 0 y 0 z 0", 0);                              /* set {defs}P {} */
    Build("set", "defs", "OK x 0 y 5 z 0", 2, "y", "5");
    Build("set", "ppt", "OK 1 2 3", 3, "1", "2", "3");                      /* produit : forme positionnelle */
    Build("set", "ppt", "OK 1 2 3", 6, "x", "1", "y", "2", "z", "3");       /* produit : forme nom/valeur */
    Build("set", "ppt", "OK 1 2 3", 6, "z", "3", "x", "1", "y", "2");       /* ... dans n'importe quel ordre */
    Build("set", "pointL", "OK 1 2", 2, "1", "2");
    Build("set", "pointL", "ERR exactly 2 elements but got 3", 3, "1", "2", "3");

    /* ===== écriture par chemin ===== */
    PathW("chemin", "pointL", "1 2", "1", "9", "OK 1 9");
    PathW("chemin", "pointL", "1 2", "2", "9", "ERR cannot grow beyond 2 elements");
    PathW("chemin", "list.int", "1 2 3", "3", "9", "OK 1 2 3 9");           /* liste ordinaire : extensible */
    PathW("chemin", "pointD", "x 1 y 2", "x", "9", "OK x 9 y 2");
    PathW("chemin", "pointD", "x 1 y 2", "z", "9", "ERR key \"z\" is not allowed");
    PathW("chemin", "opt", "x 1", "z", "5", "OK x 1 z 5");
    PathW("chemin", "point", "x 1 y 2 z 3", "y", "2.5", "OK x 1 y 2.5 z 3");
    PathW("chemin", "point", "x 1 y 2 z 3", "y", "abc", "ERR");

    /* ===== types paramétrés ===== */
    DefCheck("gen", "matrix({int}i, {int}j)", "list($i).list($j).int", "OK");
    Canon("gen", "matrix(3,3)", "OK matrix(3,3)");
    Canon("gen", "matrix( 2 , 3 )", "OK matrix(2,3)");
    Canon("gen", "matrix(03,3)", "OK matrix(3,3)");
    Canon("gen", "matrix(3)", "ERR missing argument for parameter \"j\"");
    Canon("gen", "matrix(3,3,3)", "ERR too many arguments");
    Canon("gen", "matrix", "ERR takes parameters");
    Canon("gen", "square(n)", "ERR unknown type");
    DefCheck("gen", "bare({int}n)", "list(n).int", "OK");
    Canon("gen", "bare(2)", "ERR to use the parameter, write list($n)");
    DefCheck("gen", "bare2({int}n)", "list($n).list(n).int", "OK");
    Canon("gen", "bare2(2)", "ERR in the instance");
    DefCheck("gen", "bare3({int}n)", "matrix(n,$n)", "OK");
    Canon("gen", "bare3(2)", "ERR to pass a parameter, write $name");
    Canon("gen", "matrix(a,3)", "ERR bad value \"a\" for parameter \"i\"");
    Canon("gen", "matrix(-1,3)", "ERR cannot be negative");
    Same("gen", "matrix(2,2)", "matrix(2, 2)", 1);
    Same("gen", "matrix(2,2)", "matrix(02,2)", 1);
    Same("gen", "matrix(2,2)", "matrix(2,3)", 0);
    Same("gen", "list.matrix(2,2)", "list.matrix(2,2)", 1);
    Comp("gen", "matrix(2,2)", "{1 2} {3 4}", "OK {1 2} {3 4}");
    Comp("gen", "matrix(2,2)", "{1 2 3} {3 4}", "ERR exactly 2 elements");
    Comp("gen", "matrix(2,2)", "{1 2} {3 x}", "ERR expected integer");
    Comp("gen", "matrix(2,2)", "{1 2}", "ERR exactly 2 elements");
    Build("gen", "matrix(2,2)", "OK {1 2} {3 4}", 2, "1 2", "3 4");

    DefCheck("gen", "stack({type}T)", "list.$T", "OK");
    Canon("gen", "stack(int)", "OK stack(int)");
    Canon("gen", "stack(list.int)", "OK stack(list<int>)");
    Canon("gen", "stack(stack(int))", "OK stack(stack(int))");
    Canon("gen", "stack(zzz)", "ERR unknown type");
    Canon("gen", "stack(3)", "ERR unknown type");
    Same("gen", "stack(int)", "stack( int )", 1);
    Same("gen", "stack(int)", "stack(double)", 0);
    Comp("gen", "stack(int)", "1 2 3", "OK 1 2 3");
    Comp("gen", "stack(int)", "1 x", "ERR expected integer");
    Comp("gen", "stack(stack(int))", "{1 2} {3}", "OK {1 2} {3}");

    DefCheck("gen", "tree({type}T)", "{{$T}leaf, {list.tree($T)}node}", "OK");
    Canon("gen", "tree(int)", "OK tree(int)");
    Same("gen", "tree(int)", "tree(int)", 1);
    Comp("gen", "tree(int)", "3", "OK 3");
    Comp("gen", "tree(int)", "x", "ERR matches no variant");
    Comp("gen", "tree(int)", "{{x}}", "ERR matches no variant");
    DefCheck("gen", "tree2({type}T)", "{{$T}leaf, {list.tree2($T)}node}", "OK");
    Elem("gen", "tree2(int)", 0);

    /* récursion sans clôture : la validation doit se terminer */
    DefCheck("gen", "sn", "{int}, {list.sn}", "OK");
    Comp("gen", "sn", "x", "ERR matches no variant");
    Comp("gen", "sn", "3", "OK 3");
    Comp("gen", "sn", "{3 4}", "OK {3 4}");

    /* défauts */
    DefCheck("gen", "m3({int}i, {int}j = 3)", "list($i).list($j).int", "OK");
    Canon("gen", "m3(2)", "OK m3(2,3)");
    Canon("gen", "m3(2,5)", "OK m3(2,5)");
    Canon("gen", "m3", "ERR takes parameters");
    Same("gen", "m3(2)", "m3(2,3)", 1);
    DefCheck("gen", "m4({int}i = 2, {int}j = $i)", "list($i).list($j).int", "OK");
    Canon("gen", "m4", "OK m4(2,2)");
    Canon("gen", "m4()", "OK m4(2,2)");
    Canon("gen", "m4(3)", "OK m4(3,3)");
    Canon("gen", "m4(3,4)", "OK m4(3,4)");

    /* un type paramétré dans un autre, avec un paramètre transmis */
    DefCheck("gen", "square({int}n)", "matrix($n,$n)", "OK");
    Canon("gen", "square(4)", "OK square(4)");
    Comp("gen", "square(2)", "{1 2} {3 4}", "OK {1 2} {3 4}");
    Comp("gen", "square(2)", "{1 2 3} {3 4 5}", "ERR exactly 2 elements");
    DefCheck("gen", "sq({int}n = 2)", "square($n)", "OK");
    Canon("gen", "sq", "OK sq(2)");

    /* $nom : clés et valeurs par défaut d'un dictionnaire */
    DefCheck("gen", "kp({string}k, {int}d = 7)", "dict($k, z?=$d).int", "OK");
    Canon("gen", "kp(a)", "OK kp(a,7)");
    Comp("gen", "kp(a)", "a 1", "OK a 1 z 7");
    Comp("gen", "kp(a)", "a 1 z 9", "OK a 1 z 9");
    Comp("gen", "kp(a)", "b 1", "ERR not allowed");
    Comp("gen", "kp(a,8)", "a 1", "OK a 1 z 8");
    Canon("gen", "kp({a b})", "OK kp({a b},7)");
    Canon("gen", "kp(a,x)", "ERR bad value \"x\" for parameter \"d\"");

    /* plusieurs paramètres de type */
    DefCheck("gen", "pr({type}A, {type}B)", "{$A}x; {$B}y", "OK");
    Canon("gen", "pr(int,double)", "OK pr(int,double)");
    Canon("gen", "pr(int)", "ERR missing argument for parameter \"B\"");
    Canon("gen", "pr(int,)", "ERR empty argument");
    Comp("gen", "pr(int,double)", "1 2.5", "OK 1 2.5");

    /* définitions refusées */
    DefCheck("gen", "bad1({int}i, {int}j)", "list($i).int", "ERR never used");
    DefCheck("gen", "bad2({int}a, {int}a)", "list($a).int", "ERR duplicate parameter");
    DefCheck("gen", "bad3({int}a = 1, {int}b)", "list($a).list($b).int", "ERR has no default but follows");
    DefCheck("gen", "bad4(i)", "list($i).int", "ERR needs a kind");
    DefCheck("gen", "bad5()", "int", "ERR empty parameter list");
    DefCheck("gen", "bad6({int}n) x", "list($n).int", "ERR bad parameter list");
    DefCheck("gen", "bad7({int}int)", "list($int).int", "ERR already a type name");
    DefCheck("gen", "bad8({int}type)", "list($type).int", "ERR already a type name");
    DefCheck("gen", "bad9({int}bad9)", "list($bad9).int", "ERR already a type name");
    DefCheck("gen", "bad10({int}n = )", "list($n).int", "ERR no default value");
    DefCheck("gen", "type({int}n)", "list($n).int", "ERR reserved");
    DefCheck("gen", "int({int}n)", "list($n).int", "ERR reserved");
    DefCheck("gen", "matrix", "int", "ERR already defined");
    DefCheck("gen", "matrix({int}n)", "list($n).int", "ERR already defined");
    DefCheck("gen", "bad11({int}n)", "list($n).zz", "OK");   /* l'erreur ne vient qu'à l'instanciation */
    Canon("gen", "bad11(2)", "ERR in the instance bad11(2)");
    DefCheck("gen", "bad12({type}T)", "{$T}x", "OK");
    Canon("gen", "bad12(int).int", "ERR");
    DefCheck("gen", "bad13({int}n)", "{$n}x", "OK");
    Canon("gen", "bad13(2)", "ERR is a value, not a type");
    DefCheck("gen", "bad16({type}T)", "list.T", "OK");
    Canon("gen", "bad16(int)", "ERR is a parameter: write $T");
    DefCheck("gen", "bad17({type}T)", "list.$U", "ERR never used");
    DefCheck("gen", "bad18({type}T)", "{$T}a; {$U}b", "OK");
    Canon("gen", "bad18(int)", "ERR unknown parameter \"$U\"");
    DefCheck("gen", "dflt({type}T = int)", "list.$T", "OK");
    Canon("gen", "dflt", "OK dflt(int)");
    Canon("gen", "dflt(double)", "OK dflt(double)");
    DefCheck("gen", "collect({type}T)", "list.$T", "OK");
    Comp("gen", "collect(int)", "1 2 3 4", "OK 1 2 3 4");
    Comp("gen", "collect(int)", "1 x", "ERR expected integer");
    DefCheck("gen", "polyl({type}P)", "list.collect($P)", "OK");
    Canon("gen", "polyl(int)", "OK polyl(int)");
    Comp("gen", "polyl(int)", "{1 2} {3 4}", "OK {1 2} {3 4}");
    DefCheck("gen", "bad14({type}T)", "list($T).int", "OK");
    Canon("gen", "bad14(int)", "ERR");
    DefCheck("gen", "bad15({type}T)", "$T(2)", "OK");
    Canon("gen", "bad15(int)", "ERR is a parameter: it takes no arguments");

    /* échec puis retour arrière : une nouvelle tentative recommence à zéro */
    DefCheck("gen", "late({type}T)", "{list.late($T)}a; {later}b", "OK");
    Canon("gen", "late(int)", "ERR unknown type \"later\"");
    Canon("gen", "late(int)", "ERR unknown type \"later\"");
    DefCheck("gen", "later", "int", "OK");
    Canon("gen", "late(int)", "OK late(int)");
    Elem("gen", "late(int)", 1);
    Canon("gen", "stack(late(int))", "OK stack(late(int))");

    /* un type paramétré n'est pas un nom de type ordinaire */
    DefCheck("gen", "x1", "matrix", "ERR takes parameters");
    DefCheck("gen", "x2", "matrix(2,2)", "OK");
    Canon("gen", "x2", "OK x2");

    /* ===== écriture sur plusieurs lignes ===== */
    DefCheck("lignes", "tree3(\n    {type}T\n)", "\n    {$T}value;\n    {list.tree3($T)}children\n", "OK");
    Comp("lignes", "tree3(int)", "1 {{2 {}} {3 {}}}", "OK 1 {{2 {}} {3 {}}}");
    DefCheck("lignes", "badgrid(\n    {int}rows,\n    {int}cols = 3.5\n)",
             "\n    list($rows).list($cols).int\n", "ERR is not a valid int");
    DefCheck("lignes", "badgrid2({string}s = {a b})", "dict($s).int", "OK");
    DefCheck("lignes", "badgrid3({int}n = $n)", "list($n).int", "OK");
    Canon("lignes", "badgrid3", "ERR");   /* un défaut ne peut citer que les paramètres précédents */
    DefCheck("lignes", "grid(\n    {int}rows,\n    {int}cols = 3\n)",
             "\n    list($rows).list($cols).int\n", "OK");
    Canon("lignes", "grid(2)", "OK grid(2,3)");
    DefCheck("lignes", "edge3(\n    {type}W\n)", "\n    {string}to;\n    {$W}weight\n", "OK");
    DefCheck("lignes", "graph3(\n    {type}W\n)", "\n    dict.list.edge3($W)\n", "OK");
    Comp("lignes", "graph3(int)", "A {{B 4}} B {}", "OK A {{B 4}} B {}");
    DefCheck("lignes", "pt3", "\n    {double}x;\n    {double}y;\n", "OK");   /* ";" final toléré */
    Canon("lignes", "grid(\n 2,\n 4\n)", "OK grid(2,4)");

    /* ===== ";" initial, final, répété ===== */
    Canon("semi", "{int}a;{int}b", "OK prod<a:int;b:int;>");
    Canon("semi", ";{int}a;{int}b", "OK prod<a:int;b:int;>");
    Canon("semi", "{int}a;{int}b;", "OK prod<a:int;b:int;>");
    Canon("semi", "{int}a;;{int}b", "OK prod<a:int;b:int;>");
    Canon("semi", ";;{int}a;;;{int}b;;", "OK prod<a:int;b:int;>");
    Canon("semi", "{int}a;{int}b ;  \n  ", "OK prod<a:int;b:int;>");
    Canon("semi", "{int}a;{int}b; # fin\n", "OK prod<a:int;b:int;>");
    Canon("semi", "{ ;{int}a;{int}b; }", "OK");
    Canon("semi", "{{int}a;}", "OK");
    Canon("semi", "list.{{int}a;{int}b;}", "OK");
    Canon("semi", "{int}a = 1;", "OK");
    Canon("semi", ";", "ERR empty type expression");
    Canon("semi", ";;", "ERR empty type expression");
    Canon("semi", "{int}a;,{int}b", "ERR");
    Canon("semi", "{int}a,;{int}b", "ERR");
    Canon("semi", "{int}a;.int", "ERR");
    Canon("semi", "{int}a;{int}b,{int}c", "OK");

    /* ===== "," lie plus fort que ";" ===== */
    Canon("prec", "{int}a; {double}d, {int}i", "OK");
    Canon("prec", "{{int}a; {double}d, {int}i}", "OK");
    Same("prec", "{int}a; {double}d, {int}i", "{int}a; {{double}d, {int}i}", 1);   /* avec ou sans accolades */
    Same("prec", "{int}a; {double}d, {int}i", "{int}a; {double}d, {int}i", 1);
    Comp("prec", "{int}a; {double}d, {int}i", "1 2.5", "OK 1 2.5");
    Comp("prec", "{int}a; {double}d, {int}i", "1 x", "ERR");
    Comp("prec", "{int}a; {double}d, {int}i", "1", "ERR needs 2 values");
    Comp("prec", "{double}d, {int}i; {int}a", "2.5 1", "OK 2.5 1");
    Comp("prec", "{int}a; {double}d, {int}i; {string}s", "1 2.5 z", "OK 1 2.5 z");
    Comp("prec", "{int}a; {double}d, {int}i; {string}s", "1 2.5", "ERR needs 3 values");
    Canon("prec", "{int}a, {int}b; {int}c, {int}d", "OK");
    Comp("prec", "{int}a, {int}b; {int}c, {int}d", "1 2", "OK 1 2");
    Canon("prec", "{int}a; {int}b, {int}c; {int}d", "OK");
    Canon("prec", "{int}a; {int}b = 0, {int}c", "ERR cannot have a default");   /* "=" lie plus fort que "," */
    Canon("prec", "{int}a; num", "OK");   /* un type somme nommé : champ anonyme */
    Canon("prec", "{int}a; {int}", "ERR a product field must be written");
    Canon("prec", "{int}a = 1; {double}d, {int}i", "OK");
    DefCheck("prec", "opt1", "{int}a; {double}d, {int}i", "OK");
    Build("prec", "opt1", "OK 1 2.5", 2, "1", "2.5");

    /* ===== inclusion des types var ===== */
    Inc("var", "var.int", "string", 1);
    Inc("var", "var.int", "var.int", 1);
    Inc("var", "var(typed).int", "var.int", 1);
    Inc("var", "var.int", "var(typed).int", 0);
    Inc("var", "var.int", "var.double", 0);
    Inc("var", "int", "var.int", 0);
    Inc("var", "string", "var", 0);
    Inc("var", "var", "list", 0);

    printf("\n%d vérifications, %d écart(s)\n", total, fails);
    return fails != 0;
}
