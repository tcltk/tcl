/*
 * Banc de test des défauts calculés : [script], $variable, paramètres {cmd}
 * construits à chaque valeur, jamais à la définition du type.
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

/* ---- commandes de script du banc de test ------------------------------------ */
static int ticks = 99;

static int CmdTick(Tcl_Interp *i, Tcl_Size argc, const char *argv[], Tcl_Obj **res)
{ (void) i; (void) argc; (void) argv; *res = Tcl_ObjPrintf("%d", ++ticks); return TCL_OK; }
static int CmdWord(Tcl_Interp *i, Tcl_Size argc, const char *argv[], Tcl_Obj **res)
{ (void) i; (void) argc; (void) argv; *res = S("abc"); return TCL_OK; }
static int CmdBoom(Tcl_Interp *i, Tcl_Size argc, const char *argv[], Tcl_Obj **res)
{ (void) i; (void) argc; (void) argv; *res = S("kaboom"); return TCL_ERROR; }
/* ::subst -nocommands -nobackslashes {$::nom} : lit la variable globale */
static int CmdSubst(Tcl_Interp *i, Tcl_Size argc, const char *argv[], Tcl_Obj **res)
{
    char canon[200]; Tcl_Obj *v, *c; const char *t = argv[argc - 1];
    (void) i;
    if (t[0] != '$') { *res = S(t); return TCL_OK; }
    snprintf(canon, sizeof canon, "%s%s", strncmp(t + 1, "::", 2) ? "::" : "", t + 1);
    if (TclVarRefGet(NULL, canon, &v) != TCL_OK) {
        *res = Tcl_ObjPrintf("can't read \"%s\": no such variable", t + 1); return TCL_ERROR; }
    *res = S(Tcl_GetString(TclAnnotInfo(v, &c, NULL, NULL, NULL) == 1 ? c : v));
    return TCL_OK;
}

static void DefErr(const char *group, const char *name, const char *text, const char *expect)
{
    Tcl_Obj *v[4] = {S("type"), S("def"), S(name), S(text)};
    int rc;
    for (int i = 0; i < 4; i++) v[i]->refCount++;
    rc = mock_type_command(in, 4, v);
    for (int i = 0; i < 4; i++) Tcl_DecrRefCount(v[i]);
    Report(rc != TCL_OK && strstr(mock_result(in), expect) != NULL, group, name, expect, mock_result(in));
    mock_reset(in);
}

static void Parse(const char *group, const char *text, const char *expect)
{
    Canon(group, text, expect);
}

int main(void)
{
    in = mock_interp();
    TclTypesInit(in);
    mock_register_command("tick", CmdTick);
    mock_register_command("::ns::tick", CmdTick);
    mock_register_command("word", CmdWord);
    mock_register_command("boom", CmdBoom);
    mock_register_command("::subst", CmdSubst);
    Global("G", NULL, "42");

    /* ===== syntaxe ===== */
    Parse("syntaxe", "{int}t = [tick]", "OK");
    Parse("syntaxe", "{int}t = [tick 1 [word]]", "OK");
    Parse("syntaxe", "{int}t = [tick; word]", "OK");
    Parse("syntaxe", "{int}t = [tick", "ERR missing close-bracket");
    Parse("syntaxe", "{int}t = [tick] x", "ERR missing operator");
    Parse("syntaxe", "{int}t = $::G", "OK");
    Parse("syntaxe", "{int}t = $G", "OK");
    Parse("syntaxe", "{int}t = ${G}", "OK");
    Parse("syntaxe", "{int}t = $::a(k)", "OK");
    Parse("syntaxe", "{string}t = $5", "OK");              /* "$5" reste un texte */
    Parse("syntaxe", "{int}t = $", "ERR");                 /* "$" n'est pas un entier */
    Parse("syntaxe", "{string}t = {[tick]}", "OK");        /* entre accolades : littéral */
    Parse("syntaxe", "{int}a = [tick]; {int}b = 3; {int}c = $G", "OK");
    Parse("syntaxe", "{int}a = [tick], {double}d", "ERR");  /* une variante n'a pas de défaut */
    Parse("syntaxe", "{int}t = [tick].x", "ERR");
    Parse("syntaxe", "{dict(a, s?=[tick]).string}d", "OK");
    Parse("syntaxe", "dict(a, s?=[tick, 1]).int", "OK");
    Parse("syntaxe", "dict(a, s?=$::G).int", "OK");
    Parse("syntaxe", "dict(a, s?=[tick).int", "ERR");

    /* ===== défaut par commande : calculé à chaque construction ===== */
    Def("stamped", "\n    {int}epoch = [tick];\n    {string}name;\n    {string}note = x\n");
    Make("calcul", "stamped", "OK 100 hello x", 2, "name", "hello");
    Make("calcul", "stamped", "OK 101 hello x", 2, "name", "hello");   /* PAS 100 : une fois par valeur */
    Make("calcul", "stamped", "OK 7 hi x", 4, "epoch", "7", "name", "hi");
    Check("calcul", "valeur fournie : la commande ne tourne pas", ticks == 101 ? "oui" : "non", "oui");
    Make("calcul", "stamped", "OK 102 yo x", 2, "name", "yo");
    Check("calcul", "compteur", ticks == 102 ? "oui" : "non", "oui");

    /* le résultat est validé contre le type du champ, l'erreur nomme le champ */
    Def("bad1", "{int}e = [word]");
    Make("echec", "bad1", "ERR expected integer", 0);
    Def("bad2", "{int}e = [boom]");
    Make("echec", "bad2", "ERR cannot compute the default of field \"e\" of bad2: kaboom", 0);
    Def("bad3", "{int}e = [nosuchcommand]");
    Make("echec", "bad3", "ERR invalid command name", 0);

    /* ===== défaut par variable : lue à chaque construction ===== */
    Def("lecture", "{int}v = $::G; {string}w = $G");
    Make("variable", "lecture", "OK 42 42", 0);
    Global("G", NULL, "43");
    Make("variable", "lecture", "OK 43 43", 0);
    Global("G", NULL, "oups");
    Make("variable", "lecture", "ERR expected integer", 0);
    Global("G", NULL, "42");
    Def("absente", "{int}v = $::nowhere");
    Make("variable", "absente", "ERR no such variable", 0);

    /* ===== clés de dictionnaire ===== */
    Def("conf", "dict(a, stamp?=[tick], g?=$::G).string");
    Make("dict", "conf", "OK a 1 stamp 103 g 42", 2, "a", "1");
    Make("dict", "conf", "OK a 1 stamp 104 g 42", 2, "a", "1");
    Make("dict", "conf", "OK a 1 stamp 5 g 42", 4, "a", "1", "stamp", "5");
    Def("confbad", "dict(a, s?=[word]).int");
    Make("dict", "confbad", "ERR expected integer", 2, "a", "1");

    /* ===== paramètre {cmd} ===== */
    Def("stamped2({type}T, {cmd}now)", "\n    {int}epoch = $now;\n    {$T}value;\n    {string}note = x\n");
    {
        TclType *a = TclTypeParse(in, "stamped2(string, ::ns::tick)", 28);
        TclType *b = TclTypeParse(in, "stamped2(string,::ns::tick)", 27);
        TclType *c = TclTypeParse(in, "stamped2(int, ::ns::tick)", 25);
        TclType *e = TclTypeParse(in, "stamped2(string, ::ns::tick2)", 29);
        Check("cmd", "nom de l'instance", a ? a->name : "<NULL>", "stamped2(string,::ns::tick)");
        Check("cmd", "mêmes arguments : même type", a == b ? "oui" : "non", "oui");
        Check("cmd", "autre type : autre instance", a != c ? "oui" : "non", "oui");
        Check("cmd", "autre commande : autre instance", (e != NULL && e != a) ? "oui" : "non", "oui");
        mock_reset(in);
    }
    Make("cmd", "stamped2(string, ::ns::tick)", "OK 105 hi x", 2, "value", "hi");
    Make("cmd", "stamped2(string, ::ns::tick)", "OK 106 hi x", 2, "value", "hi");
    Make("cmd", "stamped2(int, ::ns::tick)", "OK 107 8 x", 2, "value", "8");
    Make("cmd", "stamped2(int, ::ns::tick)", "ERR expected integer", 2, "value", "x");
    Canon("cmd", "stamped2(string, tick)", "ERR fully qualified");
    Canon("cmd", "stamped2(string, {::a b})", "ERR fully qualified");
    Canon("cmd", "stamped2(string)", "ERR missing argument");
    Canon("cmd", "stamped2(string, ::ns::nosuch)", "OK");               /* l'existence se vérifie à la construction */
    Make("cmd", "stamped2(string, ::ns::nosuch)", "ERR invalid command name", 2, "value", "x");

    /* défaut de paramètre {cmd} */
    Def("clocked({type}T, {cmd}now = ::ns::tick)", "{int}at = $now; {$T}v; {string}note = x");
    Make("cmd", "clocked(string)", "OK 108 q x", 2, "v", "q");
    Canon("cmd", "clocked(string)", "OK clocked(string,::ns::tick)");
    DefErr("cmd", "clocked2({cmd}now = tick)", "{int}at = $now", "not a fully qualified command name");

    /* un paramètre {cmd} transmis à un autre type */
    Def("wrap({cmd}c)", "{stamped2(string, $c)}inner");
    Canon("cmd", "wrap(::ns::tick)", "OK");
    {
        TclType *w = TclTypeParse(in, "wrap(::ns::tick)", 16);
        Check("cmd", "type du champ", (w && w->nfields == 1) ? w->fields[0].type->name : "<?>", "stamped2(string,::ns::tick)");
        mock_reset(in);
    }

    /* paramètres de valeur et de type comme défaut */
    Def("lim({int}n)", "{int}max = $n; {int}cur = 0");
    Make("param", "lim(5)", "OK 5 0", 0);
    Def("lim2({int}n)", "{int}max = $zz; {int}k = $n");
    Canon("param", "lim2(5)", "ERR unknown parameter \"$zz\"");
    Def("lim3({int}n)", "{int}max = $::G; {int}k = $n");
    Make("param", "lim3(5)", "OK 42 5", 0);
    Def("lim4({type}T)", "{int}max = $T; {$T}k");
    Canon("param", "lim4(int)", "ERR is a type");
    Def("lim5({string}s)", "{int}max = $s");
    Canon("param", "lim5(abc)", "ERR");                                  /* "abc" n'est pas un entier */
    Canon("param", "lim5(12)", "OK");

    printf("\n%d vérifications, %d écart(s)\n", total, fails);
    return fails ? 1 : 0;
}
