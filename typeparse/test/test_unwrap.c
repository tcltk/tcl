/* TclAnnotUnwrap sur des sommes : le type "number" est défini PAR LA COMMANDE `type def`. */
#include "tclTypesInt.h"
Tcl_Interp *mock_interp(void);
const char *mock_result(Tcl_Interp *);
void mock_reset(Tcl_Interp *);
int mock_type_command(Tcl_Interp *in, Tcl_Size objc, Tcl_Obj *const objv[]);

static Tcl_Obj *S(const char *s) { return Tcl_NewStringObj(s, -1); }
static const char *wn[] = {"NUMBER", "INT", "DOUBLE", "BOOL", "LIST", "DICT", "STRING"};
static int fails, total;
static Tcl_Interp *in;

static void Def(const char *name, const char *text)
{
    Tcl_Obj *v[4] = {S("type"), S("def"), S(name), S(text)};
    for (int i = 0; i < 4; i++) v[i]->refCount++;
    if (mock_type_command(in, 4, v) != TCL_OK) { printf("échec de type def %s : %s\n", name, mock_result(in)); exit(2); }
    for (int i = 0; i < 4; i++) Tcl_DecrRefCount(v[i]);
}
static TclType *T(const char *name) { return TclTypeParse(in, name, strlen(name)); }

static void Check(const char *label, Tcl_Obj *o, int want, int expectOk)
{
    Tcl_Obj *content = NULL;
    int rc = TclAnnotUnwrap(in, o, want, &content);
    int ok = (rc == TCL_OK) == expectOk;
    total++; if (!ok) fails++;
    printf("%s  %-40s %-7s attendu %-3s -> %s\n", ok ? "pass" : "FAIL", label, wn[want],
           expectOk ? "ok" : "err", rc == TCL_OK ? "ok" : mock_result(in));
    mock_reset(in);
}

int main(void)
{
    in = mock_interp();
    TclTypesInit(in);
    Def("number", "double, int");          /* trié : int (0) avant double (1) */
    Def("color", "\"red\", \"green\"");
    Def("nest", "number, \"none\"");       /* une somme DANS une somme */
    TclType *num = T("number"), *color = T("color"), *nest = T("nest");

    Tcl_Obj *a = TclNewAnnotated(in, num, 0, S("5"));
    Check("number typé, variante int \"5\"", a, 0, 1);
    Check("number typé, variante int \"5\"", a, 1, 1);
    Check("number typé, variante int \"5\"", a, 2, 1);
    Check("number typé, variante int \"5\"", a, 4, 0);
    Check("number typé, variante int \"5\"", a, 6, 1);
    Tcl_Obj *b = TclNewAnnotated(in, num, 1, S("5.5"));
    Check("number typé, variante double \"5.5\"", b, 0, 1);
    Check("number typé, variante double \"5.5\"", b, 1, 0);
    Check("number typé, variante double \"5.5\"", b, 2, 1);
    Check("brut {number}5", TclNewRawAnnotation(S("number"), S("5")), 1, 1);
    Check("brut {number}5.5", TclNewRawAnnotation(S("number"), S("5.5")), 1, 0);
    Check("brut {number}5.5", TclNewRawAnnotation(S("number"), S("5.5")), 2, 1);
    Check("brut {number}abc", TclNewRawAnnotation(S("number"), S("abc")), 1, 0);
    Tcl_Obj *c = TclNewAnnotated(in, color, 0, S("red"));
    Check("color typé \"red\"", c, 0, 0);
    Check("color typé \"red\"", c, 6, 1);
    Check("nest, variante number, \"7\"", TclNewAnnotated(in, nest, 0, S("7")), 1, 1);
    Check("nest, variante number, \"7.5\"", TclNewAnnotated(in, nest, 0, S("7.5")), 1, 0);
    Check("nest, variante number, \"7.5\"", TclNewAnnotated(in, nest, 0, S("7.5")), 2, 1);
    Check("nest, littéral \"none\"", TclNewAnnotated(in, nest, 1, S("none")), 0, 0);
    Check("int typé \"3\"", TclNewAnnotated(in, T("int"), -1, S("3")), 1, 1);
    Check("int typé \"3\"", TclNewAnnotated(in, T("int"), -1, S("3")), 4, 0);
    Check("objet ordinaire", S("x"), 1, 1);
    printf("\n%d vérifications, %d écart(s)\n", total, fails);
    return fails != 0;
}
