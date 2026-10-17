/*
 * Banc de test du type var : liens vers des variables, lecture et écriture
 * par chemin, variables typées, variables locales à un appel.
 */
#include "../tclTypedVar.c"

extern long mock_live_objs;
Tcl_Interp *mock_interp(void);
const char *mock_result(Tcl_Interp *);
void mock_reset(Tcl_Interp *);
int mock_type_command(Tcl_Interp *in, Tcl_Size objc, Tcl_Obj *const objv[]);
void mock_vars_reset(void);
void mock_push_frame(void);
void mock_pop_frame(void);
void mock_set_global(const char *name, Tcl_Obj *v);
void mock_set_local(const char *name, Tcl_Obj *v);

static Tcl_Interp *in;
static int total, fails;

static Tcl_Obj *S(const char *s) { Tcl_Obj *o = Tcl_NewStringObj(s, -1); return o; }

static void Report(int ok, const char *group, const char *label, const char *expect, const char *got)
{
    total++; if (!ok) fails++;
    printf("%s  %-8s %-60.60s -> %s\n", ok ? "pass" : "FAIL", group, label, got);
    if (!ok) printf("             attendu : %s\n", expect);
}

static int Match(const char *expect, int rc, const char *okText, const char *errText)
{
    if (expect[0] == 'O') return rc == TCL_OK && (expect[2] == '\0' || !strcmp(okText, expect + 3));
    return rc != TCL_OK && (expect[3] == '\0' || strstr(errText, expect + 4) != NULL);
}

static void Def(const char *name, const char *text)
{
    Tcl_Obj *v[4] = {S("type"), S("def"), S(name), S(text)};
    int rc;
    for (int i = 0; i < 4; i++) v[i]->refCount++;
    rc = mock_type_command(in, 4, v);
    for (int i = 0; i < 4; i++) Tcl_DecrRefCount(v[i]);
    if (rc != TCL_OK) { printf("DEF ECHEC %s: %s\n", name, mock_result(in)); fails++; }
    mock_reset(in);
}

/* Variable globale ordinaire ou typée ("int", "string"...). */
static void Global(const char *name, const char *type, const char *value)
{
    Tcl_Obj *v;
    if (type) {
        TclType *tp = TclTypeParse(in, type, strlen(type));
        v = TclNewAnnotated(in, tp, -1, S(value));
    } else v = S(value);
    mock_set_global(name, v);
}

static const char *GlobalValue(const char *canon)
{
    Tcl_Obj *v, *c;
    if (TclVarRefGet(NULL, canon, &v) != TCL_OK) return "<absent>";
    return Tcl_GetString(TclAnnotInfo(v, &c, NULL, NULL, NULL) == 1 ? c : v);
}

/* Construit la valeur de "set {type}x args" ; renvoie l'objet annoté (compté). */
static Tcl_Obj *Make(const char *group, const char *typeText, const char *expect, int n, ...)
{
    TclType *tp = TclTypeParse(in, typeText, strlen(typeText));
    Tcl_Obj *a[16], *c = NULL, *res = NULL; int variant = -1, rc; va_list ap; char label[240], cmd[200];
    size_t l = 0;
    va_start(ap, n);
    for (int i = 0; i < n; i++) { a[i] = S(va_arg(ap, const char *)); a[i]->refCount++; l += snprintf(cmd + l, sizeof cmd - l, " %s", Tcl_GetString(a[i])); }
    va_end(ap);
    snprintf(label, sizeof label, "set {%s}x%s", typeText, cmd);
    if (tp == NULL) { Report(Match(expect, TCL_ERROR, "", mock_result(in)), group, label, expect, mock_result(in)); mock_reset(in); return NULL; }
    rc = BuildValue(in, tp, 0, n, a, &c, &variant);
    if (rc == TCL_OK && (tp->kind == TCLT_LIST || tp->kind == TCLT_DICT))
        rc = TclTypeComplete(in, tp, c, -1, NULL);
    Report(Match(expect, rc, rc == TCL_OK ? Tcl_GetString(c) : "", mock_result(in)), group, label, expect,
           rc == TCL_OK ? Tcl_GetString(c) : mock_result(in));
    if (rc == TCL_OK) { res = TclNewAnnotated(in, tp, variant, c); Tcl_IncrRefCount(res); }
    if (c) TclDiscard(c);
    for (int i = 0; i < n; i++) Tcl_DecrRefCount(a[i]);
    mock_reset(in);
    return res;
}

/* $x(path) */
static void Read(const char *group, Tcl_Obj *x, const char *path, const char *expect)
{
    Tcl_Obj *res = NULL, *c; char label[200]; int rc;
    snprintf(label, sizeof label, "$x(%s)", path);
    rc = TclTypePathRead(in, x, path, &res);
    Report(Match(expect, rc, rc == TCL_OK ? Tcl_GetString(TclAnnotInfo(res, &c, NULL, NULL, NULL) == 1 ? c : res) : "", mock_result(in)),
           group, label, expect,
           rc == TCL_OK ? Tcl_GetString(TclAnnotInfo(res, &c, NULL, NULL, NULL) == 1 ? c : res) : mock_result(in));
    if (res) Tcl_DecrRefCount(res);
    mock_reset(in);
}

/* set x(path) value ; le conteneur doit rester inchangé. */
static void Write(const char *group, Tcl_Obj *x, const char *path, const char *value, const char *expect)
{
    Tcl_Obj *val = S(value), *res = NULL, *vals[1], *c; char label[200]; int rc;
    const char *before = strdup(Tcl_GetString(TclAnnotInfo(x, &c, NULL, NULL, NULL) == 1 ? c : x));
    val->refCount++; vals[0] = val;
    snprintf(label, sizeof label, "set x(%s) %s", path, value);
    rc = TclTypePathWrite(in, x, path, 1, vals, &res);
    Report(Match(expect, rc, "", mock_result(in)), group, label, expect, rc == TCL_OK ? "écrit" : mock_result(in));
    if (rc == TCL_OK) {
        const char *after = Tcl_GetString(TclAnnotInfo(res, &c, NULL, NULL, NULL) == 1 ? c : res);
        Report(strcmp(before, after) == 0, group, "  le conteneur n'a pas changé", before, after);
        Tcl_DecrRefCount(res);
    }
    free((void *) before);
    Tcl_DecrRefCount(val); mock_reset(in);
}

static void Check(const char *group, const char *label, const char *got, const char *expect)
{
    Report(strcmp(got, expect) == 0, group, label, expect, got);
}

static void Canon(const char *group, const char *text, const char *expect)
{
    TclType *tp = TclTypeParse(in, text, strlen(text));
    Report(Match(expect, tp ? TCL_OK : TCL_ERROR, tp ? tp->name : "", mock_result(in)), group, text, expect,
           tp ? tp->name : mock_result(in));
    mock_reset(in);
}

int main(void)
{
    Tcl_Obj *T, *L;

    in = mock_interp();
    TclTypesInit(in);

    /* ===== le type ===== */
    Canon("type", "var", "OK var");
    Canon("type", "var.string", "OK var");
    Canon("type", "var.int", "OK var<int>");
    Canon("type", "var(typed)", "OK var(typed)<string>");
    Canon("type", "var(typed).int", "OK var(typed)<int>");
    Canon("type", "list.var.int", "OK list<var<int>>");
    Canon("type", "var.list.int", "OK var<list<int>>");
    Canon("type", "var(foo)", "ERR the only option is var(typed)");
    Canon("type", "var(typed", "ERR");
    Canon("type", "int.var", "ERR");
    Canon("type", "{var.int}x; {var}y", "OK");
    Canon("type", "var.", "ERR");

    /* ===== l'exemple de l'arbre ===== */
    Def("tree({type}T)", "\n    {$T}value;\n    {list.tree($T)}children\n");
    Global("A", NULL, "1");
    Global("B", NULL, "2");
    Global("C", NULL, "3");

    T = Make("arbre", "tree(var)", "OK ::A {{::B {}} {::C {}}}", 2, "A", "\n    {B {}}\n    {C {}}\n");
    Read("arbre", T, "0", "OK 1");                     /* valeur de A */
    Read("arbre", T, "1.0.0", "OK 2");                 /* $T(1.0.0) eq $B */
    Read("arbre", T, "1.1.0", "OK 3");
    Read("arbre", T, "1.0", "OK ::B {}");              /* le noeud : les noms, pas les valeurs */
    Global("B", NULL, "4");                         /* set B 4 */
    Read("arbre", T, "1.0.0", "OK 4");                 /* toujours égal à $B */
    Check("arbre", "GlobalValue(::B)", GlobalValue("::B"), "4");
    Write("arbre", T, "1.0.0", "7", "OK");          /* set T(1.0.0) 7 modifie B */
    Check("arbre", "B après l'écriture", GlobalValue("::B"), "7");
    Write("arbre", T, "0", "10", "OK");
    Check("arbre", "A après l'écriture", GlobalValue("::A"), "10");
    Read("arbre", T, "0", "OK 10");
    Write("arbre", T, "1.5.0", "1", "ERR out of range");
    Write("arbre", T, "1.0.0 ", "8", "OK");
    Tcl_DecrRefCount(T);

    /* variable absente : permise à la construction, erreur à la lecture */
    T = Make("absente", "tree(var)", "OK ::Z {}", 2, "Z", "");
    Read("absente", T, "0", "ERR no such variable");
    Write("absente", T, "0", "1", "OK");            /* l'écriture crée la variable */
    Read("absente", T, "0", "OK 1");
    Tcl_DecrRefCount(T);

    /* ===== var.T : le contenu est vérifié ===== */
    Global("n", NULL, "12");
    Global("s", NULL, "abc");
    L = Make("var.int", "list.var.int", "OK ::n ::s", 2, "n", "s");
    Read("var.int", L, "0", "OK 12");
    Read("var.int", L, "1", "ERR expected integer");   /* s ne contient pas un entier */
    Write("var.int", L, "0", "x", "ERR expected integer");
    Check("var.int", "n inchangée", GlobalValue("::n"), "12");
    Write("var.int", L, "0", "13", "OK");
    Check("var.int", "n", GlobalValue("::n"), "13");
    Tcl_DecrRefCount(L);

    /* ===== variables typées ===== */
    Global("ti", "int", "5");
    Global("td", "double", "2.5");
    Global("ts", "string", "x");
    Global("plain", NULL, "7");
    L = Make("typée", "list.var(typed).int", "OK ::ti", 1, "ti");
    Read("typée", L, "0", "OK 5");
    Write("typée", L, "0", "6", "OK");
    Check("typée", "ti", GlobalValue("::ti"), "6");
    Write("typée", L, "0", "1.5", "ERR expected integer");   /* c'est ti elle-même qui refuse */
    Tcl_DecrRefCount(L);
    Make("typée", "list.var(typed).int", "ERR must be a typed variable", 1, "plain");
    Make("typée", "list.var(typed).int", "ERR must be a typed variable", 1, "nonexistent");
    Make("typée", "list.var(typed).int", "ERR not included in int", 1, "ts");
    Make("typée", "list.var(typed).int", "ERR not included in int", 1, "td");   /* double n'est pas dans int */
    L = Make("typée", "list.var.double", "OK ::ti", 1, "ti");                   /* int ⊂ double : lecture sûre */
    Tcl_DecrRefCount(L);
    Make("typée", "list.var.int", "ERR not included in int", 1, "td");
    L = Make("typée", "list.var.int", "OK ::plain", 1, "plain");                /* variable non typée : permis */
    Tcl_DecrRefCount(L);

    /* ===== à travers un lien vers une structure ===== */
    Def("point", "{int}x; {int}y");
    Global("P0", NULL, "1 2");
    L = Make("struct", "list.var.point", "OK ::P0", 1, "P0");
    Read("struct", L, "0.x", "OK 1");
    Read("struct", L, "0.y", "OK 2");
    Read("struct", L, "0", "OK 1 2");
    Write("struct", L, "0.y", "9", "OK");
    Check("struct", "P0", GlobalValue("::P0"), "1 9");
    Write("struct", L, "0.y", "z", "ERR expected integer");
    Check("struct", "P0 inchangée", GlobalValue("::P0"), "1 9");
    Write("struct", L, "0.q", "1", "ERR has no field");
    Tcl_DecrRefCount(L);

    /* ===== traversée d'un lien : seulement si la case est typée ===== */
    Def("collection", "list(3).int");
    Global("CB", "collection", "4 5 6");
    Global("UB", NULL, "4 5 6");

    T = Make("trav", "tree(var.collection)", "OK ::CB {}", 2, "CB", "");
    Read("trav", T, "0", "OK 4 5 6");
    Read("trav", T, "0.1", "OK 5");                    /* T(0.1) : 2e élément */
    Write("trav", T, "0.1", "50", "OK");
    Check("trav", "CB après l'écriture", GlobalValue("::CB"), "4 50 6");
    Write("trav", T, "0.7", "1", "ERR out of range");
    Write("trav", T, "0.1", "x", "ERR");               /* int attendu */
    Check("trav", "CB inchangée après échec", GlobalValue("::CB"), "4 50 6");
    Tcl_DecrRefCount(T);

    T = Make("trav", "tree(var.collection)", "OK ::UB {}", 2, "UB", "");   /* cible non typée */
    Read("trav", T, "0", "OK 4 5 6");
    Read("trav", T, "0.2", "OK 6");                    /* validée comme collection */
    Tcl_DecrRefCount(T);

    T = Make("trav", "tree(var)", "OK ::CB {}", 2, "CB", "");              /* var brut */
    Read("trav", T, "0", "OK 4 50 6");                 /* lecture finale : permise */
    Read("trav", T, "0.1", "ERR untyped link");        /* traversée : refusée */
    Write("trav", T, "0.1", "9", "ERR untyped link");
    Check("trav", "CB intacte", GlobalValue("::CB"), "4 50 6");
    Tcl_DecrRefCount(T);

    T = Make("trav", "tree(var(typed))", "OK ::CB {}", 2, "CB", "");       /* variable typée exigée */
    Read("trav", T, "0.1", "OK 50");
    Write("trav", T, "0.1", "51", "OK");
    Check("trav", "CB via var(typed)", GlobalValue("::CB"), "4 51 6");
    Tcl_DecrRefCount(T);

    /* ===== variables locales : durée de vie limitée à l'appel ===== */
    mock_push_frame();
    mock_set_local("loc", S("42"));
    L = Make("locale", "list.var.int", "OK frame:1:loc", 1, "loc");
    Read("locale", L, "0", "OK 42");
    Write("locale", L, "0", "43", "OK");
    Read("locale", L, "0", "OK 43");
    Global("loc", NULL, "G");                         /* un global de même nom ne la masque pas */
    Read("locale", L, "0", "OK 43");
    {
        Tcl_Obj *G = Make("locale", "list.var", "OK ::loc", 1, "::loc");   /* nom explicite : le global */
        Read("locale", G, "0", "OK G");
        Tcl_DecrRefCount(G);
    }
    mock_pop_frame();                                 /* l'appel se termine */
    Read("locale", L, "0", "ERR has returned");
    Write("locale", L, "0", "1", "ERR");
    Tcl_DecrRefCount(L);
    /* un nouvel appel ne récupère pas l'ancien lien */
    mock_push_frame();
    mock_set_local("loc", S("1"));
    mock_pop_frame();

    /* ===== un type nominal, un défaut ===== */
    Def("ref", "var.int");
    L = Make("nominal", "list.ref", "OK ::n", 1, "n");
    Read("nominal", L, "0", "OK 13");
    Tcl_DecrRefCount(L);
    Def("labelled", "{string}label; {var.int}target");
    L = Make("nominal", "labelled", "OK x ::n", 2, "x", "n");
    Read("nominal", L, "target", "OK 13");
    Read("nominal", L, "label", "OK x");
    Write("nominal", L, "target", "14", "OK");
    Check("nominal", "n", GlobalValue("::n"), "14");
    Tcl_DecrRefCount(L);

    /* ===== arbre profond : tous les noms sont canonisés ===== */
    {
        static char node[20000], path[400], tmp[20000];
        int i;
        Tcl_Obj *D;

        Global("B", NULL, "2");
        strcpy(node, "B {}");
        strcpy(path, "1.0");
        for (i = 1; i < 40; i++) {
            snprintf(tmp, sizeof tmp, "B {{%s}}", node);
            strcpy(node, tmp);
            strcat(path, ".1.0");
        }
        strcat(path, ".0");
        D = Make("profond", "tree(var)", "OK", 2, "B", "");   /* base : pas de fils */
        Tcl_DecrRefCount(D);
        {
            char children[20000];
            snprintf(children, sizeof children, "{%s}", node);
            D = Make("profond", "tree(var)", "OK", 2, "B", children);
        }
        Read("profond", D, path, "OK 2");                      /* 41 niveaux */
        Tcl_DecrRefCount(D);
    }


    printf("\n%d vérifications, %d écart(s)\n", total, fails);
    
    /* ===== langage d'index : ParseIndex / CompileIndexTree ===== */
    {
        Tcl_Obj *N, *SG, *D;
        Def("pt2", "{int}x; {int}y");
        Def("seg2", "{pt2}a; {pt2}b");
        Global("gi", NULL, "2");
        N = Make("index", "list.int", "OK 10 20 30 40 50", 5, "10", "20", "30", "40", "50");
        Read("index", N, "1", "OK 20");
        Read("index", N, "end", "OK 50");
        Read("index", N, "end-1", "OK 40");
        Read("index", N, "1:3", "OK 20 30 40");
        Read("index", N, "0:end-3", "OK 10 20");
        Read("index", N, "0,2", "OK 10 30");
        Read("index", N, "(0,2)", "OK 10 30");
        Read("index", N, "((0),(2,4))", "OK 10 30 50");
        Read("index", N, "0:1,3:4", "OK 10 20 40 50");
        Read("index", N, "'end'", "ERR can't be indexed with the key");
        Read("index", N, "[(1+2)]", "OK 40");
        Read("index", N, "[(2)]", "OK 30");
        Read("index", N, "$::gi", "OK 30");
        Read("index", N, "9", "ERR out of range");
        Read("index", N, "", "ERR empty index expression");
        Read("index", N, "0.", "ERR missing operand");
        Read("index", N, "(0", "ERR unbalanced open parenthesis");
        Read("index", N, "0)", "ERR unbalanced close parenthesis");
        Read("index", N, "()", "ERR empty group");
        Read("index", N, "0 1", "ERR missing operator");
        Read("index", N, "'abc", "ERR missing close-quote");
        Read("index", N, "[foo", "ERR missing close-bracket");
        Read("index", N, "0..1", "ERR missing operand");
        Read("index", N, "0:", "ERR missing operand");
        Read("index", N, "(0,1):3", "ERR single indices");
        Read("index", N, "0,\"|\",1", "OK 10 | 20");
        Read("index", N, "\"x\".0", "ERR tag");
        Write("index", N, "1:2", "5", "ERR only simple names");
        {   /* écriture directe : le nouveau conteneur change, l'ancien non */
            Tcl_Obj *val = S("7"), *res = NULL, *vals[1], *c; int rc;
            val->refCount++; vals[0] = val;
            rc = TclTypePathWrite(in, N, "[(1+1)]", 1, vals, &res);
            Check("index", "set x([(1+1)]) 7", rc == TCL_OK ? Tcl_GetString(TclAnnotInfo(res, &c, NULL, NULL, NULL) == 1 ? c : res) : mock_result(in), "10 20 7 40 50");
            if (res) Tcl_DecrRefCount(res);
            Tcl_DecrRefCount(val); mock_reset(in);
        }
        Write("index", N, "0,1", "7", "ERR wrong # args");
        SG = Make("index", "seg2", "OK {1 2} {3 4}", 2, "1 2", "3 4");
        Read("index", SG, "b.y", "OK 4");
        Read("index", SG, "(a,b).x", "OK 1 3");
        Read("index", SG, "a.(x,y)", "OK 1 2");
        Read("index", SG, "(a.x,b.y)", "OK 1 4");
        Read("index", SG, "a.(x,y).z", "ERR can't index a value");
        Read("index", SG, "0.1", "OK 2");
        Read("index", SG, "'0'.y", "ERR has no field");
        Write("index", SG, "(a,b).x", "9", "ERR wrong # args");
        D = Make("index", "dict.int", "OK end 1 x 2", 4, "end", "1", "x", "2");
        Read("index", D, "'end'", "OK 1");
        Read("index", D, "end", "ERR write 'end'");
        Read("index", D, "x", "OK 2");
        Read("index", D, "'x',x", "OK 2 2");
    }
return fails != 0;
}
