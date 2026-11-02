/*
 * mat.c --
 *
 * FR : des matrices 3×3 typées (type def {-compact}matrix3
 *      list(3).list(3).double) lues et fabriquées en C par les fonctions de
 *      tclAnnotatedApi.h, sans conversion élément par élément quand la valeur
 *      est en cases (le cas des types {-compact}) :
 *        mat::trace M, mat::det M      lecture
 *        mat::mul A B, mat::identite   fabrication d'une matrix3 en cases
 *        mat::lot_det L                un lot (list.matrix3) : un double[][3][3]
 *        mat::layout                   sizeof vus par le compilateur
 *      Une valeur qui n'est pas en cases (une liste ordinaire ou une
 *      matrix3o) est lue par l'API des listes : même résultat, plus lent.
 * EN : typed 3×3 matrices read and built in C through tclAnnotatedApi.h,
 *      with no per-element conversion when the value is held in cells.
 *
 * Compiler / build :
 *   gcc -shared -fPIC -O2 -DUSE_TCL_STUBS -I<src>/generic -I<build> \
 *       -I<src>/unix -o mat.so mat.c <build>/libtclstub.a
 */

#include <tcl.h>
#include "../../tclAnnotatedApi.h"
#include "mat_types.h"			/* produit par type {c!}export */

/*
 * Les cases d'une valeur dont les éléments sont du type nommé elemType
 * (le nom tel que Tcl le donne : `type get` d'un élément) et font elemSize
 * octets ; *cellsPtr = NULL si elle n'est pas en cases.
 */
static int
GetCells(Tcl_Interp *interp, Tcl_Obj *obj, const char *elemType,
	size_t elemSize, const void **cellsPtr, Tcl_Size *nPtr)
{
    Tcl_Obj *t = Tcl_NewStringObj(elemType, -1);
    int rc;

    Tcl_IncrRefCount(t);
    rc = Tcl_AnnotatedGetCells(interp, obj, t, elemSize, cellsPtr, nPtr);
    Tcl_DecrRefCount(t);
    return rc;
}

/*
 * Une matrix3 en C. En cases : une copie de 72 octets depuis les cases
 * (le pointeur ne vaut que tant que la valeur ne change pas). Sinon : par
 * les listes, en vérifiant la forme.
 */
static int
GetMatrix(Tcl_Interp *interp, Tcl_Obj *obj, matrix3 m)
{
    const void *cells;
    Tcl_Size n, nr, nc, i, j;
    Tcl_Obj **rows, **cols;

    /* une matrix3 en cases : 3 cases, une par ligne (ligne3, 24 octets) */
    if (GetCells(interp, obj, "ligne3", sizeof(ligne3), &cells, &n)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (cells != NULL && n == 3) {
	memcpy(m, cells, sizeof(matrix3));
	return TCL_OK;
    }
    if (Tcl_ListObjGetElements(interp, obj, &nr, &rows) != TCL_OK) {
	return TCL_ERROR;
    }
    if (nr != 3) {
	goto bad;
    }
    for (i = 0; i < 3; i++) {
	if (Tcl_ListObjGetElements(interp, rows[i], &nc, &cols) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (nc != 3) {
	    goto bad;
	}
	for (j = 0; j < 3; j++) {
	    if (Tcl_GetDoubleFromObj(interp, cols[j], &m[i][j]) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }
    return TCL_OK;

  bad:
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected a 3x3 matrix but got "
	    "\"%.50s\"", Tcl_GetString(obj)));
    return TCL_ERROR;
}

/* Une matrix3 neuve, en cases, typée matrix3. */
static int
NewMatrix(Tcl_Interp *interp, const matrix3 m, Tcl_Obj **resPtr)
{
    Tcl_Obj *t = Tcl_NewStringObj("matrix3", -1);
    int rc;

    Tcl_IncrRefCount(t);
    rc = Tcl_AnnotatedCreateCells(interp, t, m, 3, sizeof(ligne3), resPtr);
    Tcl_DecrRefCount(t);
    return rc;
}

static double
Det(const matrix3 m)
{
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
	    - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
	    + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

static int
TraceObjCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    matrix3 m;

    (void) cd;
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "matrix");
	return TCL_ERROR;
    }
    if (GetMatrix(interp, objv[1], m) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewDoubleObj(m[0][0] + m[1][1] + m[2][2]));
    return TCL_OK;
}

static int
DetObjCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    matrix3 m;

    (void) cd;
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "matrix");
	return TCL_ERROR;
    }
    if (GetMatrix(interp, objv[1], m) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewDoubleObj(Det(m)));
    return TCL_OK;
}

static int
MulObjCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[])
{
    matrix3 a, b, c;
    Tcl_Obj *res;
    int i, j;

    (void) cd;
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "matrixA matrixB");
	return TCL_ERROR;
    }
    if (GetMatrix(interp, objv[1], a) != TCL_OK
	    || GetMatrix(interp, objv[2], b) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 0; i < 3; i++) {
	for (j = 0; j < 3; j++) {
	    c[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
	}
    }
    if (NewMatrix(interp, (const double (*)[3]) c, &res) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, res);
    return TCL_OK;
}

static int
IdentiteObjCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    static const matrix3 id = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    Tcl_Obj *res;

    (void) cd;
    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, "");
	return TCL_ERROR;
    }
    if (NewMatrix(interp, id, &res) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, res);
    return TCL_OK;
}

/*
 * mat::lot_det L : le déterminant de chaque matrice d'un lot (list.matrix3).
 * En cases, le lot est un tableau C de matrix3 (72 octets chacune), lu en
 * place ; sinon, matrice par matrice.
 */
static int
LotDetObjCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    const void *cells;
    Tcl_Size n, k;
    Tcl_Obj *res, **ev;

    (void) cd;
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "lot");
	return TCL_ERROR;
    }
    /* un lot en cases : une case par matrice (matrix3, 72 octets) */
    if (GetCells(interp, objv[1], "matrix3", sizeof(matrix3), &cells, &n)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    res = Tcl_NewListObj(0, NULL);
    if (cells != NULL) {
	const matrix3 *lot = (const matrix3 *) cells;

	for (k = 0; k < n; k++) {
	    Tcl_ListObjAppendElement(NULL, res, Tcl_NewDoubleObj(Det(lot[k])));
	}
    } else {
	if (Tcl_ListObjGetElements(interp, objv[1], &n, &ev) != TCL_OK) {
	    Tcl_DecrRefCount(res);
	    return TCL_ERROR;
	}
	for (k = 0; k < n; k++) {
	    matrix3 m;

	    if (GetMatrix(interp, ev[k], m) != TCL_OK) {
		Tcl_DecrRefCount(res);
		return TCL_ERROR;
	    }
	    Tcl_ListObjAppendElement(NULL, res, Tcl_NewDoubleObj(Det(m)));
	}
    }
    Tcl_SetObjResult(interp, res);
    return TCL_OK;
}

/* mat::layout : les tailles vues par le compilateur (à comparer à Tcl). */
static int
LayoutObjCmd(void *cd, Tcl_Interp *interp, Tcl_Size objc,
	Tcl_Obj *const objv[])
{
    Tcl_Obj *d = Tcl_NewDictObj();

    (void) cd; (void) objc; (void) objv;
    Tcl_DictObjPut(NULL, d, Tcl_NewStringObj("sizeof(ligne3)", -1),
	    Tcl_NewWideIntObj((Tcl_WideInt) sizeof(ligne3)));
    Tcl_DictObjPut(NULL, d, Tcl_NewStringObj("sizeof(matrix3)", -1),
	    Tcl_NewWideIntObj((Tcl_WideInt) sizeof(matrix3)));
    Tcl_SetObjResult(interp, d);
    return TCL_OK;
}

int
Mat_Init(Tcl_Interp *interp)
{
    if (Tcl_InitStubs(interp, "9.0", 0) == NULL) {
	return TCL_ERROR;
    }
    Tcl_CreateObjCommand2(interp, "::mat::trace", TraceObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::mat::det", DetObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::mat::mul", MulObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::mat::identite", IdentiteObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::mat::lot_det", LotDetObjCmd, NULL, NULL);
    Tcl_CreateObjCommand2(interp, "::mat::layout", LayoutObjCmd, NULL, NULL);
    return Tcl_PkgProvide(interp, "mat", "1.0");
}
