
#include <tcl.h>
#include <math.h>
#include <string.h>

#define VERSION "1.0"
/* 
 * Structures and Datatypes
 */

typedef double AFFINE[4][4];
typedef double QUATERNION[4];
typedef double VECTOR[3];
typedef double SCALER[1];

typedef struct GenMatrix {
  int rows,cols;
  union {
    double  *pointer;
    double  cells[16];
    SCALER  scaler;
    VECTOR  vector;
    QUATERNION quaternion;
    AFFINE  affine;
  };
} MATOBJ;

/* Vector array elements. */
#define U 0
#define V 1

#define iX 0
#define jY 1
#define kZ 2
#define W  3

#define VX(X) { *(X+0) }
#define VY(X) { *(X+1) }
#define VZ(X) { *(X+2) }

#define RADIUS 0
#define THETA  1
#define PHI    2



/*
 * Constants
 */

#ifndef M_PI
#define M_PI   3.1415926535897932384626
#endif

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#ifndef TWO_M_PI
#define TWO_M_PI 6.283185307179586476925286766560
#endif

#ifndef M_PI_180
#define M_PI_180 0.01745329251994329576
#endif

#ifndef M_PI_360
#define M_PI_360 0.00872664625997164788
#endif

#define MATSTACKSIZE 64

/*
 * Module-Wide Variables
 */

/*
 * Tcl Interface
 */

EXTERN int VecExpr_ObjCmd _ANSI_ARGS_((ClientData dummy, Tcl_Interp *tinterp, int objc, Tcl_Obj *CONST objv[]));

/*
 * Macros
 */

#define CosD(A) { cos(A * M_PI_180); }
#define SinD(A) { sin(A * M_PI_180); }
#define POP(X) { if (MatStack_Pop(X) != TCL_OK) return TCL_ERROR; }
#define PUSH(X) { if (MatStack_Push(X) != TCL_OK) return TCL_ERROR; }

/*
 * Stack Commands
 */
static Tcl_Interp *current_interp;
static MATOBJ Stack[MATSTACKSIZE];
static int    StackIdx=-1;

static char *ErrorString;
static int  ErrorResult;   /* Error if Non-Zero */

void Matrix_Error(const char *error) {
  if(current_interp) {
    Tcl_AppendResult(current_interp,error,(char *) NULL);
  }
  ErrorString=error;
  ErrorResult = TCL_ERROR;
}

void Matrix_ErrorStr(Tcl_Obj *errResult) 
{
  Tcl_SetStringObj(errResult,ErrorString,-1);
}


/* Produce a general case matrix from a TCL List */

void MatStack_Clear(void) 
{
  StackIdx = -1;
}

void affine_Copy(AFFINE A,AFFINE B)
{
  int i,j;
  for(i=0;i<4;i++)
    for(j=0;j<4;j++)
      B[i][j]=A[i][j];
}

inline void Matrix_Copy(MATOBJ *A,MATOBJ *B)
{
  memcpy(B,A,sizeof(MATOBJ));
  /*
  B->rows=A->rows;
  B->cols=A->cols;
  affine_Copy(A->affine,B->affine);
  */
}

/* Produce a general case matrix from a TCL List */

int MatStack_Pop(MATOBJ *item) 
{
  if (StackIdx < 0) 
  {
    Matrix_Error("Not Enough Arguments");
    item=NULL;
    return TCL_ERROR;
  }
  Matrix_Copy(&Stack[StackIdx],item);

  StackIdx--;
  return TCL_OK;
}

int MatStack_Push(MATOBJ *value) {
  if (StackIdx >= MATSTACKSIZE) {
    Matrix_Error("Vector Stack Overflow");
    return TCL_ERROR;
  }
  StackIdx++;
  Matrix_Copy(value,&Stack[StackIdx]);
  return TCL_OK;
}

/*
 * Affine Operations
 * Must be performed on a 4x4 matrix
 */

void affine_ZeroMatrix(AFFINE A)
{
  register int i,j;
  for (i=0;i<4;i++)
    for (j=0;j<4;j++)
      A[i][j]=0;

}

void affine_IdentityMatrix(AFFINE A)
{
  register int i;

  affine_ZeroMatrix(A);

  for (i=0;i<4;i++)
    A[i][i]=1;

}

void affine_Translate(VECTOR A,AFFINE B)
{
  affine_IdentityMatrix(B);
  B[0][3]=-A[0];
  B[1][3]=-A[1];
  B[2][3]=-A[2];
}

void affine_Scale(VECTOR A,AFFINE B)
{
  affine_ZeroMatrix(B);
  B[0][0]=A[iX];
  B[1][1]=A[jY];
  B[2][2]=A[kZ];
  B[3][3]=1.0;
}

void affine_RotateX(double angle,AFFINE A)
{
  double c,s;
  c=cos(angle);
  s=sin(angle);

  affine_ZeroMatrix(A);

  A[0][0]=1.0;
  A[3][3]=1.0;

  A[1][1]=c;
  A[2][2]=c;
  A[1][2]=s;
  A[2][1]=0.0-s;
}

void affine_RotateY(double angle,AFFINE A)
{
  double c,s;
  c=cos(angle);
  s=sin(angle);

  affine_ZeroMatrix(A);

  A[1][1]=1.0;
  A[3][3]=1.0;

  A[0][0]=c;
  A[2][2]=c;
  A[0][2]=0.0-s;
  A[2][0]=s;
}


void affine_RotateZ(double angle,AFFINE A)
{
  double c,s;
  c=cos(angle);
  s=sin(angle);

  affine_ZeroMatrix(A);

  A[2][2]=1.0;
  A[3][3]=1.0;

  A[0][0]=c;
  A[1][1]=c;
  A[0][1]=s;
  A[1][0]=0.0-s;

}

void affine_Multiply(AFFINE A,AFFINE B,AFFINE R)
{
  int i,j,k;
  AFFINE temp_matrix;
  for (i=0;i<4;i++)
  {
    for (j=0;j<4;j++)
    {
      temp_matrix[i][j]=0.0;
      for (k=0;k<4;k++) temp_matrix[i][j]+=A[i][k]*B[k][j];
    }
  }
  affine_Copy(temp_matrix,R);
}


void affine_Rotate(VECTOR rotate,AFFINE R)
{
  AFFINE OP;
  
  affine_RotateX(rotate[iX],R);
  affine_RotateY(rotate[jY],OP);

  affine_Multiply(OP,R,R);
  affine_RotateZ(rotate[kZ],OP);
  affine_Multiply(OP,R,R);

}

void affine_ComputeTransform(VECTOR trans,VECTOR rotate,AFFINE R)
{
  AFFINE M1,M2,M3,M4,M5,M6,M7,M8,M9;
  //VECTOR scale = {1.0, 1.0, 1.0};
  //affine_Scale(scale,M1);

  affine_IdentityMatrix(M1);

  affine_RotateX(rotate[iX],M2);
  affine_RotateY(rotate[jY],M3);
  affine_RotateZ(rotate[kZ],M4);
  affine_Translate(trans,M5);

  affine_Multiply(M2,M1,M6);
  affine_Multiply(M3,M6,M7);
  affine_Multiply(M4,M7,M8);
  affine_Multiply(M5,M8,M9);
  affine_Copy(M9,R);
}

int affine_Inverse(AFFINE r, AFFINE m)
{
  double d00, d01, d02, d03;
  double d10, d11, d12, d13;
  double d20, d21, d22, d23;
  double d30, d31, d32, d33;
  double m00, m01, m02, m03;
  double m10, m11, m12, m13;
  double m20, m21, m22, m23;
  double m30, m31, m32, m33;
  double D;

  m00 = m[0][0];  m01 = m[0][1];  m02 = m[0][2];  m03 = m[0][3];
  m10 = m[1][0];  m11 = m[1][1];  m12 = m[1][2];  m13 = m[1][3];
  m20 = m[2][0];  m21 = m[2][1];  m22 = m[2][2];  m23 = m[2][3];
  m30 = m[3][0];  m31 = m[3][1];  m32 = m[3][2];  m33 = m[3][3];

  d00 = m11*m22*m33 + m12*m23*m31 + m13*m21*m32 - m31*m22*m13 - m32*m23*m11 - m33*m21*m12;
  d01 = m10*m22*m33 + m12*m23*m30 + m13*m20*m32 - m30*m22*m13 - m32*m23*m10 - m33*m20*m12;
  d02 = m10*m21*m33 + m11*m23*m30 + m13*m20*m31 - m30*m21*m13 - m31*m23*m10 - m33*m20*m11;
  d03 = m10*m21*m32 + m11*m22*m30 + m12*m20*m31 - m30*m21*m12 - m31*m22*m10 - m32*m20*m11;

  d10 = m01*m22*m33 + m02*m23*m31 + m03*m21*m32 - m31*m22*m03 - m32*m23*m01 - m33*m21*m02;
  d11 = m00*m22*m33 + m02*m23*m30 + m03*m20*m32 - m30*m22*m03 - m32*m23*m00 - m33*m20*m02;
  d12 = m00*m21*m33 + m01*m23*m30 + m03*m20*m31 - m30*m21*m03 - m31*m23*m00 - m33*m20*m01;
  d13 = m00*m21*m32 + m01*m22*m30 + m02*m20*m31 - m30*m21*m02 - m31*m22*m00 - m32*m20*m01;

  d20 = m01*m12*m33 + m02*m13*m31 + m03*m11*m32 - m31*m12*m03 - m32*m13*m01 - m33*m11*m02;
  d21 = m00*m12*m33 + m02*m13*m30 + m03*m10*m32 - m30*m12*m03 - m32*m13*m00 - m33*m10*m02;
  d22 = m00*m11*m33 + m01*m13*m30 + m03*m10*m31 - m30*m11*m03 - m31*m13*m00 - m33*m10*m01;
  d23 = m00*m11*m32 + m01*m12*m30 + m02*m10*m31 - m30*m11*m02 - m31*m12*m00 - m32*m10*m01;

  d30 = m01*m12*m23 + m02*m13*m21 + m03*m11*m22 - m21*m12*m03 - m22*m13*m01 - m23*m11*m02;
  d31 = m00*m12*m23 + m02*m13*m20 + m03*m10*m22 - m20*m12*m03 - m22*m13*m00 - m23*m10*m02;
  d32 = m00*m11*m23 + m01*m13*m20 + m03*m10*m21 - m20*m11*m03 - m21*m13*m00 - m23*m10*m01;
  d33 = m00*m11*m22 + m01*m12*m20 + m02*m10*m21 - m20*m11*m02 - m21*m12*m00 - m22*m10*m01;

  D = m00*d00 - m01*d01 + m02*d02 - m03*d03;

  if (D == 0.0)
  {
    Matrix_Error("Singular matrix in MInvers.");
    return TCL_ERROR;
  }

  r[0][0] =  d00/D; r[0][1] = -d10/D;  r[0][2] =  d20/D; r[0][3] = -d30/D;
  r[1][0] = -d01/D; r[1][1] =  d11/D;  r[1][2] = -d21/D; r[1][3] =  d31/D;
  r[2][0] =  d02/D; r[2][1] = -d12/D;  r[2][2] =  d22/D; r[2][3] = -d32/D;
  r[3][0] = -d03/D; r[3][1] =  d13/D;  r[3][2] = -d23/D; r[3][3] =  d33/D;
  return TCL_OK;
}

/*
 * A - the vector to be tranformed
 * B - the affine tranformation matrix
 * R - a place to dump the result
 *
 * A and R MUST BE DIFFERENT
 */

void vector_MatrixMultiply(VECTOR A,AFFINE M,VECTOR R)
{
  int i,j;
  
  for(i=0;i<3;i++)
  {
    R[i]=A[iX]*M[0][i] + A[jY]*M[1][i] + A[kZ]* M[2][i] + M[3][i];
  }
}


void vector_Scale(VECTOR A,double S)
{
  A[iX]*=S;
  A[jY]*=S;
  A[kZ]*=S;
}

double vector_Length(VECTOR A)
{
  return (sqrt(A[0]*A[0]+A[1]*A[1]+A[2]*A[2]));
}

double vector_LengthInvSqr(VECTOR A)
{
  return (1.0/(A[0]*A[0]+A[1]*A[1]+A[2]*A[2]));
}

void vector_Normalize(VECTOR A)
{
  double d;
  d=1.0 / vector_Length(A);
  A[0]*=d;
  A[1]*=d;
  A[2]*=d;
}

void vector_ToSphere(VECTOR A,VECTOR R)
{
  double S;
  R[RADIUS]=vector_Length(A);
  S=sqrt(A[iX]*A[iX]+A[jY]*A[jY]);

  if (A[iX] > 0.0) {
    R[THETA] =asin(A[jY]/S);
  } else {
    R[THETA] =M_PI - asin(A[jY]/S);
  }

  R[PHI]    =asin(A[kZ]/R[RADIUS]);
}


void sphere_ToVector(VECTOR A,VECTOR R)
{
  R[iX]=A[RADIUS]*cos(A[THETA])*cos(A[PHI]);
  R[jY]=A[RADIUS]*sin(A[THETA])*cos(A[PHI]);
  R[kZ]=A[RADIUS]*sin(A[PHI]);
}

void cylinder_ToVector(VECTOR A,VECTOR R)
{
  R[iX]=A[RADIUS]*cos(A[THETA]);
  R[jY]=A[RADIUS]*sin(A[THETA]);
  R[kZ]=A[kZ];
}

void vector_ToCylinder(VECTOR A,VECTOR R)
{
  R[RADIUS]=sqrt(A[iX]*A[iX] + A[jY]*A[jY]);
  R[THETA] =atan2(A[jY],A[iX]);
  R[kZ]     =A[kZ];
}

void Matrix_Dump(MATOBJ *A) 
{
  int i,j;
  printf("\nRows: %d Cols %d",A->rows,A->cols);
  for (i=0;i<4;i++)
  {
    printf("\nRow %d:",i);
    for (j=0;j<4;j++)
    {
      printf(" %f ",A->affine[i][j]);
    }
    printf("\n");
  }
  printf("\n");
}

/*
 * Tcl List Utilities
 */


int Matrix_FromObj(Tcl_Interp *interp, Tcl_Obj *listPtr,MATOBJ *matrix) 
{
  Tcl_Obj **rowPtrs;
  Tcl_Obj **elemPtrs;
  int result;
  int rows,cols;
  register int i,j;
  int len;
  
  /* Step one, Measure the matrix */
  result = Tcl_ListObjGetElements(interp, listPtr, &rows, &rowPtrs);
  if (result != TCL_OK) {
    Matrix_Error("Error Digesting Rows");
    return result;
  }
  result = Tcl_ListObjGetElements(interp, rowPtrs[0], &cols, &elemPtrs);
  if (result != TCL_OK) {
    Matrix_Error("Error Digesting Rows");
    return result;
  }

  /* Link what we have found so far */
  matrix->rows = rows;
  matrix->cols = cols;

  affine_ZeroMatrix(matrix->affine);
  
  if (cols==1) {
    for(i=0;i<rows;i++) {
      matrix->rows = cols;
      matrix->cols = rows;
      double temp;

      result = Tcl_GetDoubleFromObj(interp, rowPtrs[i], &temp);
      if (result != TCL_OK) {
	Matrix_Error("Error Loading Elements");
	return result;
      }
      if (result != TCL_OK) {
	Matrix_Error("Error Interpreting Value");
	return result;
      }
      matrix->affine[0][i]=temp;
    }
  } else {
    for(i=0;i<rows;i++) {
      result = Tcl_ListObjGetElements(interp, rowPtrs[i], &len, &elemPtrs);
      if (result != TCL_OK) {
	Matrix_Error("Error Loading Elements");
	return result;
      }
      if(len != cols) {
	Matrix_Error("Columns Not Uniform");
	return TCL_ERROR;
      }
      for(j=0;j<len;j++) {
	double temp;
	result =  Tcl_GetDoubleFromObj(interp, elemPtrs[j], &temp);
	if (result != TCL_OK) {
	  Matrix_Error("Bad Argument or command");
	  return result;
	}
	matrix->affine[i][j]=temp;
      }
    }
  }
  return TCL_OK;
}


Tcl_Obj *Matrix_ToList(MATOBJ *matrix) {
  Tcl_Obj *dest=Tcl_NewObj();
  Tcl_Obj **row;
  Tcl_Obj **col;	
  int rows,cols;
  register int i,j;

  /* Step 1, dimension matrix */
  rows = matrix->rows;
  cols = matrix->cols;

  if(rows==1) {
    /*
     * Output single-row matrices (i.e. vectors)
     * as a single tcl list (rather than nest them
     * as a list within a list)
     */
    rows=cols;
    row = (Tcl_Obj **)Tcl_Alloc(sizeof(Tcl_Obj *) * rows);
    
    for(j=0;j<cols;j++) { 
      row[j] = Tcl_NewDoubleObj(matrix->affine[0][j]);
    }
  } else {

    row = (Tcl_Obj **)Tcl_Alloc(sizeof(Tcl_Obj *) * rows);
    col = (Tcl_Obj **)Tcl_Alloc(sizeof(Tcl_Obj *) * cols);

    
    for(i=0;i<rows;i++) {
      for(j=0;j<cols;j++) { 
	col[j] = Tcl_NewDoubleObj(matrix->affine[i][j]);
      }
      row[i] = Tcl_NewListObj(cols,col);				
    }
  }
  Tcl_SetListObj(dest,rows,row);
  return dest;
}

void matrix_ToVector(MATOBJ *A,VECTOR R)
{
  R[iX]=A->vector[iX];
  R[jY]=A->vector[jY];
  R[kZ]=A->vector[kZ];
}


void vector_ToMatrix(VECTOR A,MATOBJ *R)
{
  R->rows=3;
  R->cols=1;
  
  R->vector[iX]=A[iX];
  R->vector[jY]=A[jY];
  R->vector[kZ]=A[kZ];
}

void matrix_ToAffine(MATOBJ *A,AFFINE R)
{
  register int i,j;

  for (i=0;i<4;i++)
    for (j=0;j<4;j++)
      R[i][j]=A->affine[i][j];
}

void affine_ToMatrix(AFFINE A,MATOBJ *R)
{
  register int i,j;

  R->rows=4;
  R->cols=4;

  for (i=0;i<4;i++)
    for (j=0;j<4;j++)
      R->affine[i][j]=A[i][j];
}

int affine_Push(AFFINE value)
{
  MATOBJ temp;
  
  affine_ToMatrix(value,&temp);
  return (MatStack_Push(&temp));
}

int vector_Push(VECTOR value)
{
  MATOBJ temp;
  
  vector_ToMatrix(value,&temp);
  return (MatStack_Push(&temp));
}


int affine_Pop(AFFINE value) 
{
  MATOBJ temp;

  if (MatStack_Pop(&temp) != TCL_OK) {
    Matrix_Error("Error Affine POP");
    return TCL_ERROR;
  }
  matrix_ToAffine(&temp,value);
  return TCL_OK;
}

int vector_Pop(VECTOR value) 
{
  MATOBJ temp;
  
  if (MatStack_Pop(&temp) != TCL_OK) {
    Matrix_Error("Error Vector POP");
    return TCL_ERROR;
  }
  matrix_ToVector(&temp,value);
  return TCL_OK;
}

static char *vectorCmds[] = {
  "*",
  "*.",
  "*X",
  "+",
  "-",
  "affine_identity",
  "affine_multiply",
  "affine_rotate",
  "affine_scale",
  "affine_translate",
  "cartesian_to_cylindrical",
  "cartesian_to_spherical",
  "copy",
  "cylindrical_to_cartesian",
  "cylindrical_to_degrees",
  "cylindrical_to_radians",
  "dt_get",
  "dt_set",
  "dump",
  "load",
  "pi",
  "spherical_to_cartesian",
  "spherical_to_degrees",
  "spherical_to_radians",
  "store",
  "to_degrees",
  "to_radians",
  "vector_add",
  "vector_cross_product",
  "vector_dot_product",
  "vector_length",
  "vector_scale",
  "vector_subtract",
  "vector_transform_affine",
  (char *)NULL
};

static enum {
  vexpr_opcode_star,
  vexpr_opcode_stardot,
  vexpr_opcode_starX,
  vexpr_opcode_plus,
  vexpr_opcode_minus,
  vexpr_opcode_affine_identity,
  vexpr_opcode_affine_multiply,
  vexpr_opcode_affine_rotate,
  vexpr_opcode_affine_scale,
  vexpr_opcode_affine_translate,
  vexpr_opcode_cartesian_to_cylindrical,
  vexpr_opcode_cartesian_to_spherical,
  vexpr_opcode_copy,
  vexpr_opcode_cylindrical_to_cartesian,
  vexpr_opcode_cylindrical_to_degrees,
  vexpr_opcode_cylindrical_to_radians,
  vexpr_opcode_dt_get,
  vexpr_opcode_dt_set,
  vexpr_opcode_dump,
  vexpr_opcode_load,
  vexpr_opcode_pi,
  vexpr_opcode_spherical_to_cartesian,
  vexpr_opcode_spherical_to_degrees,
  vexpr_opcode_spherical_to_radians,
  vexpr_opcode_store,
  vexpr_opcode_to_degrees,
  vexpr_opcode_to_radians,
  vexpr_opcode_vector_add,
  vexpr_opcode_vector_cross_product,
  vexpr_opcode_vector_dot_product,
  vexpr_opcode_vector_length,
  vexpr_opcode_vector_scale,
  vexpr_opcode_vector_subtract,
  vexpr_opcode_vector_transform_affine
} vexpr_opcodes;

int Stack_VectorCommand(int opCode)
{
  MATOBJ A,B,C;
  static MATOBJ STORE;
  static double dt;

  switch(opCode)
  {
  case vexpr_opcode_affine_identity: {

  affine_IdentityMatrix(C.affine);
  C.rows=4;
  C.cols=4;
  return(MatStack_Push(&C));

  }
  case vexpr_opcode_affine_multiply: {

  POP(&A);
  POP(&B);
  affine_Multiply(A.affine,B.affine,C.affine);

  C.rows=4;
  C.cols=4;
  return(MatStack_Push(&C));

  }
  case vexpr_opcode_affine_rotate: {

  POP(&A);
  affine_Rotate(A.vector,C.affine);

  C.rows=4;
  C.cols=4;
  return(MatStack_Push(&C));

  }
  case vexpr_opcode_affine_scale: {

  POP(&A);
  affine_Scale(A.vector,C.affine);

  C.rows=4;
  C.cols=4;
  return(MatStack_Push(&C));

  }
  case vexpr_opcode_affine_translate: {

  POP(&A);
  affine_Translate(A.vector,C.affine);
  C.rows=4;
  C.cols=4;
  return(MatStack_Push(&C));

  }
  case vexpr_opcode_cartesian_to_cylindrical: {

  POP(&A);
  vector_ToCylinder(A.vector,C.vector);

  C.rows=1;
  C.cols=3;
  return(MatStack_Push(&C));

  }
  case vexpr_opcode_cartesian_to_spherical: {

  POP(&A);
  vector_ToSphere(A.vector,C.vector);

  C.rows=1;
  C.cols=3;
  return(MatStack_Push(&C));

  }
  case vexpr_opcode_copy: {

  POP(&A);
  if(MatStack_Push(&A) != TCL_OK) return TCL_ERROR;
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_cylindrical_to_cartesian: {

  POP(&A);
  cylinder_ToVector(A.vector,C.vector);

  C.rows=1;
  C.cols=3;
  return(MatStack_Push(&C));

  }
  case vexpr_opcode_cylindrical_to_degrees: {

  POP(&A);
  A.vector[THETA]/=M_PI_180;
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_cylindrical_to_radians: {

  POP(&A);
  A.vector[THETA]*=M_PI_180;
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_dt_get: {

  A.rows=1;
  A.cols=1;
  A.cells[0]=dt;
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_dt_set: {

  POP(&A);
  A.rows=1;
  A.cols=1;
  dt=A.cells[0];
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_dump: {

  POP(&A);
  Matrix_Dump(&A);
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_load: {

  A.rows=STORE.rows;
  A.cols=STORE.cols;
  affine_Copy(STORE.affine,A.affine);
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_pi: {

  A.rows=A.cols=1;
  A.cells[0]=M_PI;
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_spherical_to_cartesian: {

  POP(&A);
  sphere_ToVector(A.vector,C.vector);

  C.rows=1;
  C.cols=3;
  return(MatStack_Push(&C));

  }
  case vexpr_opcode_spherical_to_degrees: {

  POP(&A);
  A.vector[THETA]/=M_PI_180;
  A.vector[PHI]/=M_PI_180;
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_spherical_to_radians: {

  POP(&A);
  A.vector[THETA]*=M_PI_180;
  A.vector[PHI]*=M_PI_180;
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_store: {

  POP(&A);
  STORE.rows=A.rows;
  STORE.cols=A.cols;
  affine_Copy(A.affine,STORE.affine);
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_to_degrees: {

  POP(&A);
  vector_Scale(A.vector,1.0/M_PI_180);
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_to_radians: {

  POP(&A);
  vector_Scale(A.vector,M_PI_180);
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_plus:
  case vexpr_opcode_vector_add: {

  POP(&A);
  POP(&B);
  int i;
  for(i=0;i<16;i++) {
    A.cells[i]+=B.cells[i];
  }
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_starX:
  case vexpr_opcode_vector_cross_product: {

  POP(&A);
  POP(&B);
  int i,j;
  double result=0.0;
  C.cells[iX]=A.cells[jY]*B.cells[kZ]-A.cells[kZ]*B.cells[jY];
  C.cells[jY]=A.cells[kZ]*B.cells[iX]-A.cells[iX]*B.cells[kZ];
  C.cells[kZ]=A.cells[iX]*B.cells[jY]-A.cells[jY]*B.cells[iX];
  C.rows=1;
  C.cols=3;
  return(MatStack_Push(&C));  

  }
  case vexpr_opcode_stardot:
  case vexpr_opcode_vector_dot_product: {

  POP(&A);
  POP(&B);
  int i,j;
  double result=0.0;
  for(i=0;i<4;i++) {
    result+=A.cells[i]*B.cells[i];
  }
  C.cells[0]=result;
  C.rows=1;
  C.cols=1;
  return(MatStack_Push(&C));

  }
  case vexpr_opcode_vector_length: {

  POP(&A);
  B.rows=B.cols=1;
  B.cells[0]=vector_Length(A.vector);
  return(MatStack_Push(&B));

  }
  case vexpr_opcode_star:
  case vexpr_opcode_vector_scale: {

  int i;
  double S=A.cells[0];
  
  POP(&A);
  POP(&B);

  for(i=0;i<16;i++) {
    B.cells[i]*=S;
  }    
  return(MatStack_Push(&B));

  }
  case vexpr_opcode_minus:
  case vexpr_opcode_vector_subtract: {

  int i;

  POP(&A);
  POP(&B);
  for(i=0;i<16;i++) {
    A.cells[i]-=B.cells[i];
  }
  return(MatStack_Push(&A));

  }
  case vexpr_opcode_vector_transform_affine: {

  POP(&A);
  POP(&B);
  C.rows=1;
  C.cols=3;

  vector_MatrixMultiply(A.vector,B.affine,C.vector);
  
  return(MatStack_Push(&C));

  }
  }

  Matrix_Error("Unknown/Unimplemented command");
  return TCL_ERROR;
}


EXTERN int Tcl_VexprObjCmd(dummy, tinterp, objc, objv) 
  ClientData dummy;		/* Not used. */
  Tcl_Interp *tinterp;		/* Current interpreter. */
  int objc;			/* Number of arguments. */
  Tcl_Obj *CONST objv[];	/* Argument objects. */
{
  MATOBJ mresult;
  int i,result;
  int index;
  current_interp=tinterp;

  result = TCL_OK;

  for(i=1;i<objc;i++) {    
    if (Tcl_GetIndexFromObj(tinterp, objv[i], vectorCmds, "verb", 0,
			    (int *) &index) != TCL_OK) 
      {
        MATOBJ temp;
	/* Not an opcode, push value into stack */
      
        if (Matrix_FromObj(tinterp,objv[i],&temp) != TCL_OK) {
  	  /* Failed to convert argument to a matrix. Die hard */
          return TCL_ERROR;
        }
        if (MatStack_Push(&temp) != TCL_OK) {
          return TCL_ERROR;
        }
      } else {
        Tcl_ResetResult(tinterp);
	result = Stack_VectorCommand(index);
	if(result != TCL_OK) {
	  return result;
	}
      }
  }

  if (MatStack_Pop(&mresult) != TCL_OK)
  {
    return TCL_ERROR;
  }
  Tcl_SetObjResult(tinterp,Matrix_ToList(&mresult));
  return TCL_OK;
}

