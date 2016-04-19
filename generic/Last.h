/*
 * used to be: tclsh gen-l-ast2.tcl to regenerate
 * As of Feb 2008 it is maintained by hand.
 */
#ifndef L_AST_H
#define L_AST_H

#define	unless(p)	if (!(p))
#define	private		static

typedef struct Ast	Ast;
typedef struct Block	Block;
typedef struct VarDecl	VarDecl;
typedef struct FnDecl	FnDecl;
typedef struct ClsDecl	ClsDecl;
typedef struct Stmt	Stmt;
typedef struct TopLev	TopLev;
typedef struct ClsLev	ClsLev;
typedef struct Cond	Cond;
typedef struct Loop	Loop;
typedef struct ForEach	ForEach;
typedef struct Switch	Switch;
typedef struct Case	Case;
typedef struct Expr	Expr;
typedef struct Type	Type;
typedef struct Try	Try;
typedef struct Sym	Sym;

/*
 * Source-file offset and line # of an AST node, token, or
 * nonterminal.  Set by the scanner and parser.
 */
typedef struct {
	int	beg;	// source offset of first char
	int	end;	// source offset of last char + 1
	int	line;	// line # of first char adjusted for any #include's
	char	*file;	// file name
} YYLTYPE;
#define YYLTYPE YYLTYPE

typedef enum {
	L_LOOP_DO,
	L_LOOP_FOR,
	L_LOOP_WHILE,
} Loop_k;

typedef enum {
	L_STMT_BLOCK,
	L_STMT_BREAK,
	L_STMT_COND,
	L_STMT_CONTINUE,
	L_STMT_DECL,
	L_STMT_EXPR,
	L_STMT_FOREACH,
	L_STMT_SWITCH,
	L_STMT_LOOP,
	L_STMT_RETURN,
	L_STMT_GOTO,
	L_STMT_LABEL,
	L_STMT_PRAGMA,
	L_STMT_TRY,
} Stmt_k;

typedef enum {
	L_TOPLEVEL_CLASS,
	L_TOPLEVEL_FUN,
	L_TOPLEVEL_GLOBAL,
	L_TOPLEVEL_STMT,
} Toplv_k;

typedef enum {
	L_NODE_BLOCK,
	L_NODE_EXPR,
	L_NODE_FOREACH_LOOP,
	L_NODE_FUNCTION_DECL,
	L_NODE_IF_UNLESS,
	L_NODE_SWITCH,
	L_NODE_CASE,
	L_NODE_LOOP,
	L_NODE_STMT,
	L_NODE_TOPLEVEL,
	L_NODE_CLSLEVEL,
	L_NODE_VAR_DECL,
	L_NODE_CLASS_DECL,
} Node_k;

/*
 * A compiler temp.
 */
typedef struct Tmp Tmp;
struct Tmp {
	int	free;
	int	idx;	// local variable slot #
	char	*name;
	Tmp	*next;
};

/*
 * An L type name is represented with exactly one instance of a
 * Type structure.  All references to the type name point to that
 * structure, so pointer comparison can be used to check for name
 * equivalence of types.
 *
 * L_int, L_float, etc are the global Type pointers for the
 * pre-defined types.  L_INT, L_FLOAT, etc are the type kinds
 * (enum) used in the type structure.
 *
 * L_NAMEOF is like an address in other languages.  An expression
 * of this type holds the name of an lvalue (e.g., &p has the value
 * "p").  The base_type is the type of the name.
 */

typedef enum {
	L_INT		= 0x0001,
	L_FLOAT		= 0x0002,
	L_STRING	= 0x0004,
	L_ARRAY		= 0x0008,
	L_HASH		= 0x0010,
	L_STRUCT	= 0x0020,
	L_LIST		= 0x0040,
	L_VOID		= 0x0080,
	L_POLY		= 0x0100,
	L_NAMEOF	= 0x0200,
	L_FUNCTION	= 0x0400,
	L_CLASS		= 0x0800,
	L_WIDGET	= 0x1000,
} Type_k;

struct Type {
	Type_k	kind;
	Type	*base_type;	// for array, hash, list, nameof, etc
	Type	*next;		// for linking list types
	char	*name;		// if a typedef, the type name being declared
	union {
		struct {
			Expr	*size;
		} array;
		struct {
			Type	*idx_type;
		} hash;
		struct {
			char	*tag;
			VarDecl	*members;
		} struc;
		struct {
			VarDecl	*formals;
		} func;
		struct {
			ClsDecl	*clsdecl;
		} class;
	} u;
	Type	*list;  // links all type structures ever allocated
};

struct Ast {
	Node_k	type;
	Ast	*next;	// links all nodes in an AST
	YYLTYPE	loc;
};

struct Block {
	Ast	node;
	Stmt	*body;
	VarDecl	*decls;
};

typedef enum {
	L_EXPR_ID,
	L_EXPR_CONST,
	L_EXPR_FUNCALL,
	L_EXPR_UNOP,
	L_EXPR_BINOP,
	L_EXPR_TRINOP,
	L_EXPR_RE,
} Expr_k;

typedef enum {
	L_OP_NONE,
	L_OP_CAST,
	L_OP_BANG,
	L_OP_ADDROF,
	L_OP_MINUS,
	L_OP_UMINUS,
	L_OP_PLUS,
	L_OP_UPLUS,
	L_OP_PLUSPLUS_PRE,
	L_OP_PLUSPLUS_POST,
	L_OP_MINUSMINUS_PRE,
	L_OP_MINUSMINUS_POST,
	L_OP_EQUALS,
	L_OP_EQPLUS,
	L_OP_EQMINUS,
	L_OP_EQSTAR,
	L_OP_EQSLASH,
	L_OP_EQPERC,
	L_OP_EQBITAND,
	L_OP_EQBITOR,
	L_OP_EQBITXOR,
	L_OP_EQLSHIFT,
	L_OP_EQRSHIFT,
	L_OP_EQTWID,
	L_OP_BANGTWID,
	L_OP_EQDOT,
	L_OP_STAR,
	L_OP_SLASH,
	L_OP_PERC,
	L_OP_STR_EQ,
	L_OP_STR_NE,
	L_OP_STR_GT,
	L_OP_STR_LT,
	L_OP_STR_GE,
	L_OP_STR_LE,
	L_OP_EQUALEQUAL,
	L_OP_NOTEQUAL,
	L_OP_GREATER,
	L_OP_LESSTHAN,
	L_OP_GREATEREQ,
	L_OP_LESSTHANEQ,
	L_OP_ANDAND,
	L_OP_OROR,
	L_OP_LSHIFT,
	L_OP_RSHIFT,
	L_OP_BITOR,
	L_OP_BITAND,
	L_OP_BITXOR,
	L_OP_BITNOT,
	L_OP_DEFINED,
	L_OP_ARRAY_INDEX,
	L_OP_HASH_INDEX,
	L_OP_DOT,
	L_OP_POINTS,
	L_OP_CLASS_INDEX,
	L_OP_INTERP_STRING,
	L_OP_INTERP_RE,
	L_OP_LIST,
	L_OP_KV,
	L_OP_COMMA,
	L_OP_ARRAY_SLICE,
	L_OP_EXPAND,
	L_OP_CONCAT,
	L_OP_CMDSUBST,
	L_OP_TERNARY_COND,
	L_OP_FILE,
} Op_k;

/*
 * Flags for L expression compilation.  Bits are used for simplicity
 * even though some of these are mutually exclusive.  These are used
 * in various places such as calls to compile_expr() and in the AST.
 */
typedef enum {
	L_EXPR_RE_I   = 0x00000001, // expr is an re with "i" qualifier
	L_EXPR_RE_G   = 0x00000002, // expr is an re with "g" qualifier
	L_EXPR_RE_T   = 0x00000004, // expr is an re with "t" qualifier
	L_EXPR_DEEP   = 0x00000008, // expr is the result of a deep dive
	L_IDX_ARRAY   = 0x00000010, // what kind of thing we're indexing
	L_IDX_HASH    = 0x00000020,
	L_IDX_STRING  = 0x00000040,
	L_LVALUE      = 0x00000080, // if we will be writing the obj
	L_DELETE      = 0x00000100, // delete from parent hash/array/string
	L_PUSH_VAL    = 0x00000200, // what we want INST_L_INDEX to leave on
	L_PUSH_PTR    = 0x00000400, //   the stack
	L_PUSH_VALPTR = 0x00000800,
	L_PUSH_PTRVAL = 0x00001000,
	L_DISCARD     = 0x00002000, // have compile_expr push nothing
	L_PUSH_NAME   = 0x00004000, // have compile_expr push name not val
	L_PUSH_NEW    = 0x00008000, // whether INST_L_DEEP_WRITE should push
	L_PUSH_OLD    = 0x00010000, //   the new or old value
	L_SAVE_IDX    = 0x00020000, // save idx to a tmp var (for copy in/out)
	L_REUSE_IDX   = 0x00040000, // get idx from tmp var instead of expr
	L_NOTUSED     = 0x00080000, // do not update used_p in symtab entry
	L_NOWARN      = 0x00100000, // issue no err if symbol undefined
	L_SPLIT_RE    = 0x00200000, // split on a regexp
	L_SPLIT_STR   = 0x00400000, // split on a string
	L_SPLIT_LIM   = 0x00800000, // enforce split limit
	L_INSERT_ELT  = 0x01000000, // insert a single elt into a list
	L_INSERT_LIST = 0x02000000, // insert a list into a list
	L_NEG_OK      = 0x04000000, // indexing element -1 is OK
	L_EXPR_RE_L   = 0x08000000, // expr is an re with "l" qualifier
} Expr_f;

struct Expr {
	Ast	node;
	Expr_k	kind;
	Op_k	op;
	Type	*type;
	Expr	*a;
	Expr	*b;
	Expr	*c;
	Expr_f	flags;
	Sym	*sym;  // for id, ptr to symbol table entry
	char	*str;  // for constants/id/re/struct-index
	union {
		struct {
			Tmp	*idx;
			Tmp	*val;
		} deepdive;
	} u;
	Expr	*next;
};

struct ForEach {
	Ast	node;
	Expr	*expr;
	Expr	*key;
	Expr	*value;
	Stmt	*body;
};

struct FnDecl {
	Ast	node;
	Block	*body;
	VarDecl	*decl;
	FnDecl	*next;
	Tcl_Obj	*attrs;		// hash of function attributes, if any
};

struct ClsDecl {
	Ast	node;
	VarDecl	*decl;
	VarDecl	*clsvars;
	VarDecl	*instvars;
	FnDecl	*fns;
	FnDecl	*constructors;
	FnDecl	*destructors;
	Tcl_HashTable *symtab;
};

struct Cond {
	Ast	node;
	Expr	*cond;
	Stmt	*else_body;
	Stmt	*if_body;
};

struct Loop {
	Ast	node;
	Expr	*cond;
	Expr	*post;
	Expr	*pre;
	Loop_k	kind;
	Stmt	*body;
};

struct Switch {
	Ast	node;
	Expr	*expr;
	Case	*cases;
};

struct Case {
	Ast	node;
	Expr	*expr;
	Stmt	*body;
	Case	*next;
};

struct Try {
	Ast	node;
	Stmt	*try;
	Stmt	*catch;
	Expr	*msg;
};

struct Stmt {
	Ast	node;
	Stmt	*next;
	Stmt_k	kind;
	union {
		Block	*block;
		Expr	*expr;
		ForEach	*foreach;
		Cond	*cond;
		Loop	*loop;
		Switch	*swich;  // not a typo -- illegal to call it "switch"
		VarDecl	*decl;
		char	*label;
		Try	*try;
	} u;
};

struct TopLev {
	Ast	node;
	TopLev	*next;
	Toplv_k	kind;
	union {
		ClsDecl	*class;
		FnDecl	*fun;
		Stmt	*stmt;
		VarDecl	*global;
	} u;
};

/*
 * These encode both scope information and the kind of declaration.
 * Some flags are redundant but were chosen for clarity.
 */
typedef enum {
	SCOPE_LOCAL		= 0x00000001, // scope the symbol should go in
	SCOPE_SCRIPT		= 0x00000002, //   visible in current script
	SCOPE_GLOBAL		= 0x00000004, //   visible across scripts
	SCOPE_CLASS		= 0x00000008, //   visible in a class
	DECL_GLOBAL_VAR		= 0x00000010, // the kind of declaration
	DECL_LOCAL_VAR		= 0x00000020,
	DECL_ERR		= 0x00000040, //   added on undeclared var
	DECL_FN			= 0x00000080, //   regular function
	DECL_CLASS_VAR		= 0x00000100, //   class variable
	DECL_CLASS_INST_VAR	= 0x00000200, //   class instance variable
	DECL_CLASS_FN		= 0x00000400, //   class member fn
	DECL_CLASS_CONST	= 0x00000800, //   class constructor
	DECL_CLASS_DESTR	= 0x00001000, //   class destructor
	DECL_REST_ARG		= 0x00002000, //   ...arg formal parameter
	DECL_EXTERN		= 0x00004000, // decl has extern qualifier
	DECL_PRIVATE		= 0x00008000, // decl has private qualifier
	DECL_PUBLIC		= 0x00010000, // decl has public qualifier
	DECL_REF		= 0x00020000, // decl has & qualifier
	DECL_ARGUSED		= 0x00040000, // decl has _argused qualifier
	DECL_OPTIONAL		= 0x00080000, // decl has _optional qualifier
	DECL_NAME_EQUIV		= 0x00100000, // decl has _mustbetype qualifier
	DECL_FORWARD		= 0x00200000, // a forward decl
	DECL_DONE		= 0x00400000, // decl already processed
	FN_PROTO_ONLY		= 0x00800000, // compile fn proto only
	FN_PROTO_AND_BODY	= 0x01000000, // compile entire fn decl
} Decl_f;

struct VarDecl {
	Ast	node;
	Expr	*id;
	char	*tclprefix;	// prepend to L var name to create Tcl var name
	Expr	*initializer;
	Expr	*attrs;		// optional _attributes(...)
	Type	*type;
	ClsDecl	*clsdecl;	// for class member fns, class & instance vars
	Sym	*refsym;	// for a call-by-ref parm x, ptr to &x sym
	VarDecl	*next;
	Decl_f	flags;
};

extern Expr	*ast_mkBinOp(Op_k op, Expr *e1, Expr *e2, YYLTYPE beg,
			     YYLTYPE end);
extern Block	*ast_mkBlock(VarDecl *decls,Stmt *body, YYLTYPE beg,
			     YYLTYPE end);
extern Case	*ast_mkCase(Expr *expr, Stmt *body, YYLTYPE beg,
			    YYLTYPE end);
extern ClsDecl	*ast_mkClsDecl(VarDecl *decl, YYLTYPE beg, YYLTYPE end);
extern Expr	*ast_mkConst(Type *type, char *str, YYLTYPE beg,
			     YYLTYPE end);
extern FnDecl	*ast_mkConstructor(ClsDecl *class);
extern FnDecl	*ast_mkDestructor(ClsDecl *class);
extern Expr	*ast_mkExpr(Expr_k kind, Op_k op, Expr *a, Expr *b, Expr *c,
			    YYLTYPE beg, YYLTYPE end);
extern Expr	*ast_mkFnCall(Expr *id, Expr *arg_list, YYLTYPE beg,
			      YYLTYPE end);
extern FnDecl	*ast_mkFnDecl(VarDecl *decl, Block *body, YYLTYPE beg,
			      YYLTYPE end);
extern ForEach	*ast_mkForeach(Expr *hash, Expr *key, Expr *value,
			       Stmt *body, YYLTYPE beg, YYLTYPE end);
extern Expr	*ast_mkId(char *name, YYLTYPE beg, YYLTYPE end);
extern Cond	*ast_mkIfUnless(Expr *expr, Stmt *if_body, Stmt *else_body,
				YYLTYPE beg, YYLTYPE end);
extern Loop	*ast_mkLoop(Loop_k kind, Expr *pre, Expr *cond, Expr *post,
			    Stmt *body, YYLTYPE beg, YYLTYPE end);
extern Expr	*ast_mkRegexp(char *re, YYLTYPE beg, YYLTYPE end);
extern Stmt	*ast_mkStmt(Stmt_k kind, Stmt *next, YYLTYPE beg,
			    YYLTYPE end);
extern Switch	*ast_mkSwitch(Expr *expr, Case *cases, YYLTYPE beg,
			      YYLTYPE end);
extern TopLev	*ast_mkTopLevel(Toplv_k kind, TopLev *next, YYLTYPE beg,
				YYLTYPE end);
extern Expr	*ast_mkTrinOp(Op_k op, Expr *e1, Expr *e2, Expr *e3,
			      YYLTYPE beg, YYLTYPE end);
extern Try	*ast_mkTry(Stmt *try, Expr *msg, Stmt *catch);
extern Expr	*ast_mkUnOp(Op_k op, Expr *e1, YYLTYPE beg, YYLTYPE end);
extern VarDecl	*ast_mkVarDecl(Type *type, Expr *name, YYLTYPE beg,
			       YYLTYPE end);
extern void	hash_dump(Tcl_Obj *hash);
extern char	*hash_get(Tcl_Obj *hash, char *key);
extern void	hash_put(Tcl_Obj *hash, char *key, char *val);
extern void	hash_rm(Tcl_Obj *hash, char *key);
extern Type	*type_dup(Type *type);
extern Type	*type_mkArray(Expr *size, Type *base_type);
extern Type	*type_mkClass(void);
extern Type	*type_mkFunc(Type *base_type, VarDecl *formals);
extern Type	*type_mkHash(Type *index_type, Type *base_type);
extern Type	*type_mkList(Type *a);
extern Type	*type_mkNameOf(Type *base_type);
extern Type	*type_mkScalar(Type_k kind);
extern Type	*type_mkStruct(char *tag, VarDecl *members);

#endif /* L_AST_H */
