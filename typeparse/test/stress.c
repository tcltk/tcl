/* Charge : accolades, champs, variantes, imbrications, clés. */
#include "tclTypesInt.h"
Tcl_Interp *mock_interp(void);
const char *mock_result(Tcl_Interp *);
void mock_reset(Tcl_Interp *);
static int bad;
static void Run(Tcl_Interp *in, const char *label, const char *s, int expectOk)
{
    TclType *tp = TclTypeParse(in, s, strlen(s));
    int ok = (tp != NULL) == expectOk;
    if (!ok) bad++;
    printf("%s  %-34s %6zu octets -> %s\n", ok ? "pass" : "FAIL", label, strlen(s),
           tp ? (strlen(tp->name) > 36 ? "(nom long)" : tp->name) : mock_result(in));
    mock_reset(in);
}
int main(void)
{
    Tcl_Interp *in = mock_interp();
    char *b = malloc(1 << 21); size_t n;
    TclTypesInit(in);
    n = 0; for (int i = 0; i < 5000; i++) b[n++] = '{';
    n += sprintf(b + n, "int"); for (int i = 0; i < 5000; i++) b[n++] = '}'; b[n] = 0;
    Run(in, "5000 accolades imbriquées", b, 1);
    n = 0; for (int i = 0; i < 1000; i++) n += sprintf(b + n, "%s{int}f%d = %d", i ? "; " : "", i, i);
    Run(in, "produit de 1000 champs, défauts", b, 1);
    n = 0; for (int i = 0; i < 1000; i++) n += sprintf(b + n, "%s\"v%d\"", i ? ", " : "", i);
    Run(in, "somme de 1000 littéraux", b, 1);
    n = 0; for (int i = 0; i < 3000; i++) n += sprintf(b + n, "list(2)."); n += sprintf(b + n, "int");
    Run(in, "3000 list(2). imbriqués", b, 1);
    n = sprintf(b, "dict("); for (int i = 0; i < 500; i++) n += sprintf(b + n, "%sk%d?=%d", i ? ", " : "", i, i);
    n += sprintf(b + n, ").int");
    Run(in, "dict de 500 clés à défauts", b, 1);
    n = sprintf(b, "dict("); for (int i = 0; i < 500; i++) n += sprintf(b + n, "%sk%d", i ? ", " : "", i);
    n += sprintf(b + n, ", k3).int");
    Run(in, "doublon parmi 500 clés", b, 0);
    n = 0; for (int i = 0; i < 1000; i++) n += sprintf(b + n, "{int}f%d; ", i); n += sprintf(b + n, "{int}f1 ,");
    Run(in, "erreur à la fin d'un gros texte", b, 0);
    free(b);
    return bad != 0;
}
