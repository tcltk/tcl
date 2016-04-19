/*
 * Copyright (c) 2006-2008 BitMover, Inc.
 */
#ifndef L_COMPILE_H
#define L_COMPILE_H

#include <setjmp.h>
#include "tclInt.h"
#include "tclCompile.h"
#include "Last.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* For jump fix-ups. */
typedef struct Jmp Jmp;
struct Jmp {
	int	op;	// jmp instruction bytecode (e.g., INST_JUMP1)
	int	size;	// size of jmp instruction (1 or 4 bytes)
	int	offset;	// bytecode offset of jmp instruction
	Jmp	*next;
};

/* Semantic stack frame. */
typedef enum {
	OUTER		= 0x0001,  // is outer-most
	SCRIPT		= 0x0002,  // is file scope
	TOPLEV		= 0x0004,  // is for file top-levels
	CLS_OUTER	= 0x0008,  // is class outer-most
	CLS_TOPLEV	= 0x0010,  // is for class top-levels
	FUNC		= 0x0020,  // frame is at top level of a proc
	LOOP		= 0x0040,  // frame is for a loop
	SWITCH		= 0x0080,  // frame is for a switch stmt
	SKIP		= 0x0100,  // skip frame when searching enclosing scopes
	SEARCH		= 0x0200,  //   don't skip this frame
	KEEPSYMS	= 0x0400,  // don't free symtab when scope is closed
} Frame_f;
typedef enum {
	LABEL_USE	= 0x01,    // label is being referenced
	LABEL_DEF	= 0x02,    // label is being defined
} Label_f;
typedef struct Label {
	char	*name;
	int	offset;
	Jmp	*fixups;
} Label;
typedef struct Frame {
	CompileEnv	*envPtr;
	CompileEnv	*bodyEnvPtr;
	CompileEnv	*prologueEnvPtr;
	Proc		*proc;
	char		*name;
	Tcl_HashTable	*symtab;
	Tcl_HashTable	*labeltab;
	ClsDecl		*clsdecl;
	Frame_f		flags;
	// When a compile frame corresponds to a block in the code, we
	// store the AST node of the block here.
	Ast		*block;
	// We collect jump fix-ups for all of the jumps emitted for break and
	// continue statements, so that we can stuff in the correct jump targets
	// once we're done compiling the loops.
	Jmp		*continue_jumps;
	Jmp		*break_jumps;
	// Jump fix-up for the jump to the prologue code at the end of a proc,
	// and the bytecode offset for the jump back.
	Jmp		*end_jmp;
	int		proc_top;
	// Fix-ups for return stmts which all jmp to the end.
	Jmp		*ret_jmps;
	// List of temps allocated in this frame.
	Tmp		*tmps;
	struct Frame	*prevFrame;
} Frame;

/* Per-scope tables. Scopes are opened and close at parse time. */
typedef struct scope Scope;
struct scope {
	Tcl_HashTable	*structs;
	Tcl_HashTable	*typedefs;
	Scope		*prev;
};

/*
 * Global L state.  There is only one of these, reachable via L->global.
 * The general L command line is
 * tclsh [-tclsh_opt1] ... [-tclsh_optn] script_name [-script_opt1] ... [-script_optm]
 */
typedef struct {
	int	tclsh_argc;
	Tcl_Obj	*tclsh_argv;
	int	script_argc;
	Tcl_Obj	*script_argv;
	int	forceL;		// wrap input in #lang L directive
} Lglobal;

/*
 * Per-interp L state.  When an interp is created, one of these is
 * allocated for each interp and associated with the interp.  Whenever
 * the compiler is entered, it is extracted from the interp.
 */
typedef struct {
	Lglobal	*global;	// L global state
	Frame	*frame;		// current semantic stack frame
	Scope	*curr_scope;
	Ast	*ast_list;	// list of all AST nodes
	Type	*type_list;	// list of all type descriptors
	void	*ast;		// ptr to AST root, set by parser
	Tcl_Obj	*errs;
	int	err;		// =1 if there was any compile error
	char	*dir;		// absolute path to dir containing L->file
	char	*file;
	int	line;
	int	prev_token_len;
	int	token_off;	// offset of curr token from start of input
	int	prev_token_off;	// offset of prev token from start of input
	Tcl_Obj	*script;	// src of script being compiled
	int	script_len;
	Tcl_Obj	*options;	// hash of command-line options
	FnDecl	*enclosing_func;
	Frame	*enclosing_func_frame;
	Ast	*mains_ast;	// root of AST when main() last seen
	Tcl_HashTable	*include_table;
	Tcl_Interp	*interp;
	Op_k	idx_op;		// kind of enclosing index (. -> [] {})
	int	tmpnum;		// for creating tmp variables
	char	*toplev;	// name of toplevel proc
	jmp_buf	jmp;		// for syntax error longjmp bail out
	int	expr_level;	// compile_expr() recursion depth
	int	call_level;	// compile_expr() level of last fn call
	Tcl_Obj	*fn_calls;	// list of all fn calls compiled
	Tcl_Obj	*fn_decls;	// hash of all L fns
} Linterp;

/*
 * Symbol table entry, for variables and functions (typedef and struct
 * names have their own tables).  The tclname can be different from
 * the L name if we have to mangle the name as we do for L globals.
 * The decl pointer is used to get line# info for error messages.
 */
typedef enum {
	L_SYM_LVAR	= 0x0001,	// a local variable
	L_SYM_GVAR	= 0x0002,	// a global variable
	L_SYM_LSHADOW	= 0x0004,	// a global upvar shadow (these
					// are also locals)
	L_SYM_FN	= 0x0008,	// a function
	L_SYM_FNBODY	= 0x0010,	// function body has been declared
} Sym_k;
struct Sym {
	Sym_k	kind;
	char	*name;		// the L name
	char	*tclname;	// the tcl name (can be same as L name)
	Type	*type;
	int	idx;		// slot# for local var
	int	used_p;		// TRUE iff var has been referenced
	VarDecl	*decl;
};

/*
 * For our getopt.  Note that this renders libc's getopt unusable,
 * but the #define's are kept for compatibility with our getopt.
 */

#define getopt  mygetopt
#define optind  myoptind
#define optarg  myoptarg
#define optopt  myoptopt

extern  int     optind;
extern  int     optopt;
extern  char    *optarg;

typedef struct {
	char    *name;	/* name w args ex: "url:" */
	int     ret;	/* return value from getopt */
} longopt;

#define GETOPT_EOF	-1
#define GETOPT_ERR      256

extern char	*cksprintf(const char *fmt, ...);
extern char	*ckstrdup(const char *str);
extern char	*ckstrndup(const char *str, int len);
extern char	*ckvsprintf(const char *fmt, va_list ap, int len);
extern int	getopt(int ac, char **av, char *opts, longopt *lopts);
extern void	getoptReset(void);
extern void	L_bomb(const char *format, ...);
extern void	L_compile_attributes(Tcl_Obj *hash, Expr *expr,
				     char *allowed[]);
extern char	*L_dirname(char *path);
extern void	L_err(const char *s, ...);
extern void	L_errf(void *node, const char *format, ...);
extern int	L_isUndef(Tcl_Obj *o);
extern void	L_lex_begLhtml();
extern void	L_lex_endLhtml();
extern void	L_lex_begReArg(int kind);
extern void	L_lex_start(void);
extern int	L_parse(void);			// yyparse
extern void	L_scope_enter();
extern void	L_scope_leave();
extern void	L_set_baseType(Type *type, Type *base_type);
extern void	L_set_declBaseType(VarDecl *decl, Type *base_type);
extern Tcl_Obj	*L_split(Tcl_Interp *interp, Tcl_Obj *strobj,
			 Tcl_Obj *delimobj, Tcl_Obj *limobj, Expr_f flags);
extern Type	*L_struct_lookup(char *tag, int local);
extern Type	*L_struct_store(char *tag, VarDecl *members);
extern void	L_synerr(const char *s);	// yyerror
extern void	L_synerr2(const char *s, int offset);
extern void	L_trace(const char *format, ...);
extern char	*L_type_str(Type_k kind);
extern void	L_typeck_init();
extern int	L_typeck_arrElt(Type *var, Type *array);
extern void	L_typeck_assign(Expr *lhs, Type *rhs);
extern int	L_typeck_compat(Type *lhs, Type *rhs);
extern int	L_typeck_declType(VarDecl *decl);
extern void	L_typeck_deny(Type_k deny, Expr *expr);
extern void	L_typeck_expect(Type_k want, Expr *expr, char *msg);
extern void	L_typeck_fncall(VarDecl *formals, Expr *call);
extern void	L_typeck_main(VarDecl *decl);
extern int	L_typeck_same(Type *a, Type *b);
extern Type	*L_typedef_lookup(char *name);
extern void	L_typedef_store(VarDecl *decl);
extern Tcl_Obj **L_undefObjPtrPtr();
extern void	L_warnf(void *node, const char *format, ...);

extern Linterp	*L;
extern char	*L_attrs_attribute[];
extern char	*L_attrs_cmdLine[];
extern char	*L_attrs_pragma[];
extern Tcl_ObjType L_undefType;
extern Type	*L_int;
extern Type	*L_float;
extern Type	*L_string;
extern Type	*L_void;
extern Type	*L_poly;
extern Type	*L_widget;

static inline int
istype(Expr *expr, int type_flags)
{
	return (expr->type && (expr->type->kind & type_flags));
}
static inline int
isarray(Expr *expr)
{
	return (expr->type && (expr->type->kind == L_ARRAY));
}
static inline int
ishash(Expr *expr)
{
	return (expr->type && (expr->type->kind == L_HASH));
}
static inline int
isstruct(Expr *expr)
{
	return (expr->type && (expr->type->kind == L_STRUCT));
}
static inline int
isint(Expr *expr)
{
	return (expr->type && (expr->type->kind == L_INT));
}
static inline int
isfloat(Expr *expr)
{
	return (expr->type && (expr->type->kind == L_FLOAT));
}
static inline int
isstring(Expr *expr)
{
	return (expr->type && (expr->type->kind == L_STRING));
}
static inline int
iswidget(Expr *expr)
{
	return (expr->type && (expr->type->kind == L_WIDGET));
}
static inline int
isvoid(Expr *expr)
{
	return (expr->type && (expr->type->kind == L_VOID));
}
static inline int
ispoly(Expr *expr)
{
	return (expr->type && (expr->type->kind == L_POLY));
}
static inline int
isscalar(Expr *expr)
{
	return (expr->type && (expr->type->kind & (L_INT |
						   L_FLOAT |
						   L_STRING |
						   L_WIDGET |
						   L_POLY)));
}
static inline int
isconst(Expr *expr)
{
	return (expr->kind == L_EXPR_CONST);
}
static inline int
islist(Expr *expr)
{
	return (expr->type && (expr->type->kind == L_LIST));
}
static inline int
isclass(Expr *expr)
{
	return (expr->type && (expr->type->kind == L_CLASS));
}
static inline int
isregexp(Expr *expr)
{
	return ((expr->kind == L_EXPR_RE) ||
		((expr->kind == L_EXPR_BINOP) && (expr->op == L_OP_INTERP_RE)));
}
static inline int
ispolytype(Type *type)
{
	return (type->kind == L_POLY);
}
static inline int
islisttype(Type *type)
{
	return (type->kind == L_LIST);
}
static inline int
ishashtype(Type *type)
{
	return (type->kind == L_HASH);
}
static inline int
isfntype(Type *type)
{
	return (type->kind == L_FUNCTION);
}
static inline int
isinttype(Type *type)
{
	return (type->kind == L_INT);
}
static inline int
isvoidtype(Type *type)
{
	return (type->kind == L_VOID);
}
static inline int
isnameoftype(Type *type)
{
	return (type->kind == L_NAMEOF);
}
static inline int
isclasstype(Type *type)
{
	return (type->kind == L_CLASS);
}
static inline int
isarrayoftype(Type *type, Type_k kind)
{
	return ((type->kind == L_ARRAY) && (type->base_type->kind & kind));
}
static inline int
ishashoftype(Type *type, Type_k base, Type_k elt)
{
	return ((type->kind == L_HASH) &&
		(type->base_type->kind & base) &&
		(type->u.hash.idx_type->kind & elt));
}
static inline int
isaddrof(Expr *expr)
{
	return ((expr->kind == L_EXPR_UNOP) && (expr->op == L_OP_ADDROF));
}
static inline int
isexpand(Expr *expr)
{
	return ((expr->kind == L_EXPR_UNOP) && (expr->op == L_OP_EXPAND));
}
static inline int
iskv(Expr *expr)
{
	return ((expr->kind == L_EXPR_BINOP) && (expr->op == L_OP_KV));
}
static inline int
isinterp(Expr *expr)
{
	return ((expr->kind == L_EXPR_BINOP) && (expr->op == L_OP_INTERP_STRING));
}
static inline int
isid(Expr *expr, char *s)
{
	return ((expr->kind == L_EXPR_ID) && !strcmp(expr->str, s));
}
static inline int
isarrayof(Expr *expr, Type_k kind)
{
	return (isarray(expr) && (expr->type->base_type->kind & kind));
}
/*
 * Return the flags that match the kind of variable we can
 * dereference: globals, locals, class variables, and class instance
 * variables.
 */
static inline int
canDeref(Sym *sym)
{
	return (sym->decl->flags & (DECL_GLOBAL_VAR | DECL_LOCAL_VAR |
				    DECL_FN | DECL_CLASS_INST_VAR |
				    DECL_CLASS_VAR));
}
/*
 * This checks whether the Expr node is a deep-dive operation that has
 * left a deep-ptr on the run-time stack.
 */
static inline int
isdeepdive(Expr *expr)
{
	return (expr->flags & (L_PUSH_PTR | L_PUSH_PTRVAL | L_PUSH_VALPTR));
}
static inline int
isClsConstructor(VarDecl *decl)
{
	return (decl->flags & DECL_CLASS_CONST);
}
static inline int
isClsDestructor(VarDecl *decl)
{
	return (decl->flags & DECL_CLASS_DESTR);
}
static inline int
isClsFnPublic(VarDecl *decl)
{
	return ((decl->flags & (DECL_CLASS_FN | DECL_PUBLIC)) ==
		(DECL_CLASS_FN | DECL_PUBLIC));
}
static inline int
isClsFnPrivate(VarDecl *decl)
{
	return ((decl->flags & (DECL_CLASS_FN | DECL_PRIVATE)) ==
		(DECL_CLASS_FN | DECL_PRIVATE));
}
static inline int
typeis(Type *type, char *name)
{
	return (type->name && !strcmp(type->name, name));
}
static inline int
typeisf(Expr *expr, char *name)
{
	return (expr->type->name && !strcmp(expr->type->name, name));
}
static inline void
emit_load_scalar(int idx)
{
	/*
	 * The next line is a hack so we can generate disassemblable
	 * code even in the presence of obscure compilation errors
	 * that cause the value of a function name to be attempted to
	 * be loaded.  Without this, tcl will die trying to output a
	 * disassembly since the local # (-1) would be invalid.
	 */
	if (idx == -1) idx = 0;

	if (idx <= 255) {
		TclEmitInstInt1(INST_LOAD_SCALAR1, idx, L->frame->envPtr);
	} else {
		TclEmitInstInt4(INST_LOAD_SCALAR4, idx, L->frame->envPtr);
	}
}
static inline void
emit_store_scalar(int idx)
{
	if (idx <= 255) {
		TclEmitInstInt1(INST_STORE_SCALAR1, idx, L->frame->envPtr);
	} else {
		TclEmitInstInt4(INST_STORE_SCALAR4, idx, L->frame->envPtr);
	}
}
static inline void
push_litf(const char *str, ...)
{
	va_list ap;
	int	len = 64;
	char	*buf;

	va_start(ap, str);
	while (!(buf = ckvsprintf(str, ap, len))) {
		va_end(ap);
		va_start(ap, str);
		len *= 2;
	}
	va_end(ap);
	/*
	 * Subtle: register the literal in the body CompileEnv since
	 * all the code ends up there anyway.  If we put it in the
	 * prologue CompileEnv, we'd have to fix-up all the literal
	 * numbers when we splice the prologue into the body.
	 */
	TclEmitPush(TclRegisterNewLiteral(L->frame->bodyEnvPtr, buf, strlen(buf)),
		    L->frame->envPtr);
	ckfree(buf);
}
static inline void
push_lit(const char *str)
{
	/* See comment above about registering in the body CompileEnv. */
	TclEmitPush(TclRegisterNewLiteral(L->frame->bodyEnvPtr, str,
					  strlen(str)), L->frame->envPtr);
}
static inline void
emit_invoke(int size)
{
	if (size < 256) {
		TclEmitInstInt1(INST_INVOKE_STK1, size, L->frame->envPtr);
	} else {
		TclEmitInstInt4(INST_INVOKE_STK4, size, L->frame->envPtr);
	}
}
static inline void
emit_invoke_expanded()
{
	TclEmitOpcode(INST_INVOKE_EXPANDED, L->frame->envPtr);
}
static inline void
emit_pop()
{
	TclEmitOpcode(INST_POP, L->frame->envPtr);
}
static inline int
currOffset(CompileEnv *envPtr)
{
	/* Offset of the next instruction to be generated. */
	return (envPtr->codeNext - envPtr->codeStart);
}

/*
 * REVERSE() assumes that l is a singly linked list of type type with
 * forward pointers named ptr.  The last element in the list becomes
 * the first and is stored back into l.
 */
#define REVERSE(type,ptr,l)						\
    do {								\
	type *a, *b, *c;						\
	for (a = NULL, b = l, c = (l ? ((type *)l)->ptr : NULL);	\
	     b != NULL;							\
	     b->ptr = a, a = b, b = c, c = (c ? c->ptr : NULL)) ;	\
	*(type **)&(l) = a;						\
    } while (0)

/*
 * APPEND() starts at a, walks ptr until the end, and then attaches b
 * to a.  (Note that it's actually NCONC).
 */
#define APPEND(type,ptr,a,b)						\
    do {								\
	    type *runner;						\
	    for (runner = a; runner->ptr; runner = runner->ptr) ;	\
	    runner->ptr = b;						\
    } while (0)

/*
 * Like APPEND() but if a is NULL, set a to b.
 */
#define APPEND_OR_SET(type,ptr,a,b)					\
    do {								\
	    if (a) {							\
		type *runner;						\
		for (runner = a; runner->ptr; runner = runner->ptr) ;	\
		runner->ptr = b;					\
	    } else {							\
		a = b;							\
	    }								\
    } while (0)

/*
 * YYLOC_DEFAULT() is invoked by the scanner after matching a pattern
 * and before executing its code.  It tracks the source-file offset
 * and line #.
 */
extern YYLTYPE L_lloc;
#define YYLLOC_DEFAULT(c,r,n)				\
	do {						\
		if (n) {				\
			(c).beg = YYRHSLOC(r,1).beg;	\
			(c).end = YYRHSLOC(r,n).end;	\
		} else {				\
			(c).beg = YYRHSLOC(r,0).beg;	\
			(c).end = YYRHSLOC(r,0).end;	\
		}					\
		(c).line = L->line;			\
		(c).file = L->file;			\
	} while (0)

#ifdef TCL_COMPILE_DEBUG
#define ASSERT(c)  unless (c) \
	L_bomb("Assertion failed: %s:%d: %s\n", __FILE__, __LINE__, #c)
#else
#define ASSERT(c)  do {} while(0)
#endif

#endif /* L_COMPILE_H */
