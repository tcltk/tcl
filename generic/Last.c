/*
 * used to be: tclsh gen-l-ast2.tcl to regenerate
 * As of Feb 2008 it is maintained by hand.
 */
#include "tclInt.h"
#include "Lcompile.h"

private void
ast_init(void *node, Node_k type, YYLTYPE beg, YYLTYPE end)
{
	Ast	*ast = (Ast *)node;

	ast->type     = type;
	ast->loc.beg  = beg.beg;
	ast->loc.end  = end.end;
	ast->loc.line = beg.line;
	ast->loc.file = beg.file;
	ast->next     = L->ast_list;
	L->ast_list   = (void *)ast;
}

Block *
ast_mkBlock(VarDecl *decls, Stmt *body, YYLTYPE beg, YYLTYPE end)
{
	Block	*block = (Block *)ckalloc(sizeof(Block));
	memset(block, 0, sizeof(Block));
	block->body  = body;
	block->decls = decls;
	ast_init(block, L_NODE_BLOCK, beg, end);
	return (block);
}

Expr *
ast_mkExpr(Expr_k kind, Op_k op, Expr *a, Expr *b, Expr *c, YYLTYPE beg,
	   YYLTYPE end)
{
	Expr	*expr = (Expr *)ckalloc(sizeof(Expr));
	memset(expr, 0, sizeof(Expr));
	expr->a    = a;
	expr->b    = b;
	expr->c    = c;
	expr->kind = kind;
	expr->op   = op;
	ast_init(expr, L_NODE_EXPR, beg, end);
	return (expr);
}

ForEach *
ast_mkForeach(Expr *expr, Expr *key, Expr *value, Stmt *body,
	      YYLTYPE beg, YYLTYPE end)
{
	ForEach	*foreach = (ForEach *)ckalloc(sizeof(ForEach));
	memset(foreach, 0, sizeof(ForEach));
	foreach->expr  = expr;
	foreach->key   = key;
	foreach->value = value;
	foreach->body  = body;
	ast_init(foreach, L_NODE_FOREACH_LOOP, beg, end);
	return (foreach);
}

FnDecl *
ast_mkFnDecl(VarDecl *decl, Block *body, YYLTYPE beg, YYLTYPE end)
{
	FnDecl *fndecl = (FnDecl *)ckalloc(sizeof(FnDecl));
	memset(fndecl, 0, sizeof(FnDecl));
	fndecl->body = body;
	fndecl->decl = decl;
	/*
	 * Propagate tracing attributes from L->options, which come
	 * from cmd-line options or #pragmas.  Any attributes
	 * specified in the function declaration end up overwriting
	 * these.
	 */
	fndecl->attrs = Tcl_NewDictObj();
	hash_put(fndecl->attrs, "fntrace", hash_get(L->options, "fntrace"));
	hash_put(fndecl->attrs, "fnhook", hash_get(L->options, "fnhook"));
	hash_put(fndecl->attrs, "trace_depth",
		 hash_get(L->options, "trace_depth"));
	ast_init(fndecl, L_NODE_FUNCTION_DECL, beg, end);
	return (fndecl);
}

Cond *
ast_mkIfUnless(Expr *expr, Stmt *if_body, Stmt *else_body, YYLTYPE beg,
	       YYLTYPE end)
{
	Cond *cond = (Cond *)ckalloc(sizeof(Cond));
	memset(cond, 0, sizeof(Cond));
	cond->cond      = expr;
	cond->else_body = else_body;
	cond->if_body   = if_body;
	ast_init(cond, L_NODE_IF_UNLESS, beg, end);
	return (cond);
}

Loop *
ast_mkLoop(Loop_k kind, Expr *pre, Expr *cond, Expr *post, Stmt *body,
	   YYLTYPE beg, YYLTYPE end)
{
	Loop *loop = (Loop *)ckalloc(sizeof(Loop));
	memset(loop, 0, sizeof(Loop));
	loop->cond = cond;
	loop->post = post;
	loop->pre  = pre;
	loop->kind = kind;
	loop->body = body;
	ast_init(loop, L_NODE_LOOP, beg, end);
	return (loop);
}

Switch *
ast_mkSwitch(Expr *expr, Case *cases, YYLTYPE beg, YYLTYPE end)
{
	Switch	*sw = (Switch *)ckalloc(sizeof(Switch));
	memset(sw, 0, sizeof(Switch));
	sw->expr  = expr;
	sw->cases = cases;
	ast_init(sw, L_NODE_SWITCH, beg, end);
	return (sw);
}

Case *
ast_mkCase(Expr *expr, Stmt *body, YYLTYPE beg, YYLTYPE end)
{
	Case	*c = (Case *)ckalloc(sizeof(Case));
	memset(c, 0, sizeof(Case));
	c->expr = expr;
	c->body = body;
	ast_init(c, L_NODE_CASE, beg, end);
	return (c);
}

Try *
ast_mkTry(Stmt *try, Expr *msg, Stmt *catch)
{
	Try	*t = (Try *)ckalloc(sizeof(Try));
	memset(t, 0, sizeof(Try));
	t->try   = try;
	t->msg   = msg;
	t->catch = catch;
	return (t);
}

Stmt *
ast_mkStmt(Stmt_k kind, Stmt *next, YYLTYPE beg, YYLTYPE end)
{
	Stmt *stmt = (Stmt *)ckalloc(sizeof(Stmt));
	memset(stmt, 0, sizeof(Stmt));
	stmt->next = next;
	stmt->kind = kind;
	ast_init(stmt, L_NODE_STMT, beg, end);
	return (stmt);
}

TopLev *
ast_mkTopLevel(Toplv_k kind, TopLev *next, YYLTYPE beg, YYLTYPE end)
{
	TopLev *toplev = (TopLev *)ckalloc(sizeof(TopLev));
	memset(toplev, 0, sizeof(TopLev));
	toplev->next = next;
	toplev->kind = kind;
	ast_init(toplev, L_NODE_TOPLEVEL, beg, end);
	return (toplev);
}

VarDecl *
ast_mkVarDecl(Type *type, Expr *id, YYLTYPE beg, YYLTYPE end)
{
	VarDecl *vardecl = (VarDecl *)ckalloc(sizeof(VarDecl));
	memset(vardecl, 0, sizeof(VarDecl));
	vardecl->id   = id;
	vardecl->type = type;
	ast_init(vardecl, L_NODE_VAR_DECL, beg, end);
	return (vardecl);
}

ClsDecl *
ast_mkClsDecl(VarDecl *decl, YYLTYPE beg, YYLTYPE end)
{
	ClsDecl *clsdecl = (ClsDecl *)ckalloc(sizeof(ClsDecl));
	memset(clsdecl, 0, sizeof(ClsDecl));
	clsdecl->decl = decl;
	ast_init(clsdecl, L_NODE_CLASS_DECL, beg, end);
	return (clsdecl);
}

/* Build a default constructor if the user didn't provide one. */
FnDecl *
ast_mkConstructor(ClsDecl *class)
{
	char	*name;
	Type	*type;
	Expr	*id;
	VarDecl	*decl;
	Block	*block;
	FnDecl	*fn;
	YYLTYPE	loc = class->node.loc;

	type  = type_mkFunc(class->decl->type, NULL);
	name  = cksprintf("%s_new", class->decl->id->str);
	id    = ast_mkId(name, loc, loc);
	decl  = ast_mkVarDecl(type, id, loc, loc);
	decl->flags |= SCOPE_GLOBAL | DECL_CLASS_FN | DECL_PUBLIC |
		DECL_CLASS_CONST;
	decl->clsdecl = class;
	block = ast_mkBlock(NULL, NULL, loc, loc);
	fn    = ast_mkFnDecl(decl, block, loc, loc);

	return (fn);
}

/* Build a default destructor if the user didn't provide one. */
FnDecl *
ast_mkDestructor(ClsDecl *class)
{
	char	*name;
	Type	*type;
	Expr	*id, *self;
	VarDecl	*decl, *parm;
	Block	*block;
	FnDecl	*fn;
	YYLTYPE	loc = class->node.loc;

	self = ast_mkId("self", loc, loc);
	parm = ast_mkVarDecl(class->decl->type, self, loc, loc);
	parm->flags = SCOPE_LOCAL | DECL_LOCAL_VAR;
	type = type_mkFunc(L_void, parm);
	name = cksprintf("%s_delete", class->decl->id->str);
	id   = ast_mkId(name, loc, loc);
	decl = ast_mkVarDecl(type, id, loc, loc);
	decl->flags |= SCOPE_GLOBAL | DECL_CLASS_FN | DECL_PUBLIC |
		DECL_CLASS_DESTR;
	decl->clsdecl = class;
	block = ast_mkBlock(NULL, NULL, loc, loc);
	fn    = ast_mkFnDecl(decl, block, loc, loc);

	return (fn);
}

Expr *
ast_mkUnOp(Op_k op, Expr *e1, YYLTYPE beg, YYLTYPE end)
{
	return (ast_mkExpr(L_EXPR_UNOP, op, e1, NULL, NULL, beg, end));
}

Expr *
ast_mkBinOp(Op_k op, Expr *e1, Expr *e2, YYLTYPE beg, YYLTYPE end)
{
	return (ast_mkExpr(L_EXPR_BINOP, op, e1, e2, NULL, beg, end));
}

Expr *
ast_mkTrinOp(Op_k op, Expr *e1, Expr *e2, Expr *e3, YYLTYPE beg,
	     YYLTYPE end)
{
	return (ast_mkExpr(L_EXPR_TRINOP, op, e1, e2, e3, beg, end));
}

Expr *
ast_mkConst(Type *type, char *str, YYLTYPE beg, YYLTYPE end)
{
	Expr *e = ast_mkExpr(L_EXPR_CONST, L_OP_NONE, NULL, NULL, NULL,
			    beg, end);
	e->type = type;
	e->str  = str;
	return (e);
}

Expr *
ast_mkRegexp(char *re, YYLTYPE beg, YYLTYPE end)
{
	Expr *e = ast_mkExpr(L_EXPR_RE, L_OP_NONE, NULL, NULL, NULL, beg, end);
	e->str  = re;
	e->type = L_string;
	return (e);
}

Expr *
ast_mkFnCall(Expr *id, Expr *arg_list, YYLTYPE beg, YYLTYPE end)
{
	Expr *e = ast_mkExpr(L_EXPR_FUNCALL, L_OP_NONE, id, arg_list, NULL,
			    beg, end);
	return (e);
}

Expr *
ast_mkId(char *name, YYLTYPE beg, YYLTYPE end)
{
	Expr *e = ast_mkExpr(L_EXPR_ID, L_OP_NONE, NULL, NULL, NULL, beg, end);
	e->str = ckstrdup(name);
	return (e);
}

private Type *
type_alloc(Type_k kind)
{
	Type *type = (Type *)ckalloc(sizeof(Type));
	memset(type, 0, sizeof(Type));
	type->kind   = kind;
	type->list   = L->type_list;
	L->type_list = type;
	return (type);
}

Type *
type_dup(Type *type)
{
	Type *dup = (Type *)ckalloc(sizeof(Type));
	*dup = *type;
	if (type->name) {
		dup->name = ckstrdup(type->name);
	}
	if ((type->kind == L_STRUCT) && type->u.struc.tag) {
		dup->u.struc.tag = ckstrdup(type->u.struc.tag);
	}
	dup->list    = L->type_list;
	L->type_list = dup;
	return (dup);
}

Type *
type_mkScalar(Type_k kind)
{
	Type *type = type_alloc(kind);
	return (type);
}

Type *
type_mkArray(Expr *size, Type *base_type)
{
	Type *type = type_alloc(L_ARRAY);
	type->u.array.size = size;
	type->base_type    = base_type;
	return (type);
}

Type *
type_mkHash(Type *index_type, Type *base_type)
{
	Type *type = type_alloc(L_HASH);
	type->u.hash.idx_type = index_type;
	type->base_type       = base_type;
	return (type);
}

Type *
type_mkStruct(char *tag, VarDecl *members)
{
	Type *type = type_alloc(L_STRUCT);
	type->u.struc.tag     = ckstrdup(tag);
	type->u.struc.members = members;
	return (type);
}

Type *
type_mkNameOf(Type *base_type)
{
	Type *type = type_alloc(L_NAMEOF);
	type->base_type = base_type;
	return (type);
}

Type *
type_mkFunc(Type *ret_type, VarDecl *formals)
{
	Type *type = type_alloc(L_FUNCTION);
	type->base_type      = ret_type;
	type->u.func.formals = formals;
	return (type);
}

Type *
type_mkList(Type *a)
{
	Type *type = type_alloc(L_LIST);
	type->base_type = a;
	return (type);
}

Type *
type_mkClass(void)
{
	Type *type = type_alloc(L_CLASS);
	return (type);
}
