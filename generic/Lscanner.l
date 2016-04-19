%option	noyywrap
%option noyy_top_state
%option stack
%option noinput
%x re_delim
%x re_modifier
%x re_arg_split
%x re_arg_case
%x glob_re
%x subst_re
%x comment
%x str_double
%x str_single
%x str_backtick
%x interpol
%x here_doc_interp
%x here_doc_nointerp
%x eat_through_eol
%x lhtml
%x lhtml_expr_start
ID	([a-zA-Z_]|::)([0-9a-zA-Z_]|::)*
HEX	[a-fA-F0-9]
%{
/*
 * Copyright (c) 2006-2008 BitMover, Inc.
 */
#include <string.h>
#define	_PWD_H			// Some solaris9 conflict, we don't need pwd.h
#include "tclInt.h"
#include "Lcompile.h"
#include "Lgrammar.h"
#include "tommath.h"

private void	extract_re_delims(char c);
private int	include_pop();
private int	include_push(Tcl_Channel chan, char *name);
private Tcl_Channel include_search(char *file, char **path, int cwdOnly);
private Tcl_Channel include_try(Tcl_Obj *fileObj, int *found);
private void	inject(char *s);
private void	interpol_lbrace();
private void	interpol_pop();
private int	interpol_push();
private int	interpol_rbrace();
private void	put_back(char c);
private void	tally_newlines(char *s, int len, int tally);

// Max nesting depth of string interpolations.
#define INTERPOL_STACK_SZ	10

// Stack for tracking include() statements.
#define INCLUDE_STACK_SZ	10
typedef struct {
	char	*name;
	char	*dir;
	int	line;
	YY_BUFFER_STATE	buf;
} Include;

private char	re_start_delim;	// delimiters for m|regexp| form
private char	re_end_delim;
private Tcl_Obj	*str;		// string collection buffer
private int	str_beg;	// source offset of string
private char	*here_delim = NULL;
private char	*here_pfx = NULL;
private int	include_top;
private Include	include_stk[INCLUDE_STACK_SZ+1];
private Tcl_HashTable *include_table = NULL;
private	int	interpol_top = -1;
private	int	interpol_stk[INTERPOL_STACK_SZ+1];
private int	in_lhtml = 0;	// Lhtml mode

#define STRBUF_START(beg)			\
	do {					\
		str = Tcl_NewObj();		\
		Tcl_IncrRefCount(str);		\
		str_beg = (beg);		\
	} while (0)


#define STRBUF_STRING()		Tcl_GetString(str)

#define STRBUF_STARTED()	(str != NULL)

#define STRBUF_ADD(s, len)	Tcl_AppendToObj(str, s, len)

#define STRBUF_STOP(e)				\
	do {					\
		Tcl_DecrRefCount(str);		\
		str = NULL;			\
		L_lloc.beg = str_beg;		\
		L_lloc.end = (e);		\
	} while (0)

/*
 * Keep track of the current offset in the input string.
 * YY_USER_ACTION is run before each action.  Note that some actions
 * further modify L_lloc.
 */

#define YY_USER_ACTION	yy_user_action();

private void
yy_user_action()
{
	L->prev_token_off = L->token_off;
	L->token_off     += L->prev_token_len;
	L->prev_token_len = yyleng;

	L_lloc.beg = L->token_off;
	L_lloc.end = L->token_off + yyleng;

	tally_newlines(yytext, yyleng, 1);
	L_lloc.line = L->line;

	L_lloc.file = L->file;

	/*
	 * Build up in L->script the text that the scanner scans.
	 * The compiler later passes this on to tcl as the script
	 * source.  This allows include() stmts to be handled properly.
	 */
	Tcl_AppendToObj(L->script, yytext, yyleng);
	L->script_len += yyleng;
}

/*
 * Un-do the effects of the YY_USER_ACTION on the token offset
 * tracking.  This is useful in include() processing where the
 * characters in the '#include "file"' must be ignored.
 */
private void
undo_yy_user_action()
{
	L->prev_token_len = L->token_off - L->prev_token_off;
	L->token_off      = L->prev_token_off;

	L_lloc.beg = L->prev_token_off;
	L_lloc.end = L->prev_token_off + L->prev_token_len;

	tally_newlines(yytext, yyleng, -1);
	L_lloc.line = L->line;

	L->script_len -= yyleng;
	Tcl_SetObjLength(L->script, L->script_len);
}

/*
 * Inject the given string into the L script text, but do not give it
 * to the scanner.  This is useful for inserting #line directives (for
 * #include's) which need to remain in the script so Tcl can see them
 * but which aren't parsed.
 */
private void
inject(char *s)
{
	int	len = strlen(s);

	L->prev_token_len += len;

	Tcl_AppendToObj(L->script, s, len);
	L->script_len += len;
}

/*
 * Count the newlines in a string and add the number to L->line.  Pass
 * in tally == 1 to count them and tally == -1 to undo it.
 */
private void
tally_newlines(char *s, int len, int tally)
{
	char	*end, *p;

	for (p = s, end = p + len; p < end; p++) {
		if (*p == '\n') {
			L->line += tally;
		} else if ((*p == '\r') && ((p+1) < end) && (*(p+1) != '\n')) {
			/* Mac line endings. */
			L->line += tally;
		}
	}
}

private Tcl_Channel
include_try(Tcl_Obj *fileObj, int *found)
{
	int		new;
	Tcl_Channel	chan;
	char		*file = Tcl_GetString(fileObj);
	char		*path;
	Tcl_Obj		*pathObj;

	/*
	 * See if the normalized path has been included before.  If the path
	 * isn't absolute, consider it to be relative to where L->file is.
	 */
	if (Tcl_FSGetPathType(fileObj) == TCL_PATH_ABSOLUTE) {
		if ((pathObj = Tcl_FSGetNormalizedPath(NULL, fileObj)) == NULL){
			L_err("unable to normalize include file %s", file);
			return (NULL);
		}
	} else {
		pathObj = Tcl_ObjPrintf("%s/%s", L->dir, file);
	}
	Tcl_IncrRefCount(pathObj);

	path = Tcl_GetString(pathObj);
	Tcl_CreateHashEntry(include_table, path, &new);
	if (new) {
		chan = Tcl_FSOpenFileChannel(L->interp, pathObj, "r", 0666);
		*found = (chan != NULL);
		return (chan);
	} else {
		*found = 1;  // already included
		return (NULL);
	}
	Tcl_DecrRefCount(pathObj);
}

/*
 * Search for an include file.  If the path is absolute, use it.
 * Else, for #include <file> (cwdOnly == 0) try
 *    $BIN/include  (where BIN is where the running tclsh lives)
 *    /usr/local/include/L
 *    /usr/include/L
 * For #include "file" (cwdOnly == 1) look only in the directory
 * where the script doing the #include resides.
 */
private Tcl_Channel
include_search(char *file, char **path, int cwdOnly)
{
	int		found, len;
	Tcl_Channel	chan;
	Tcl_Obj		*binObj = NULL;
	Tcl_Obj		*fileObj;

	unless (include_table) {
		include_table = (Tcl_HashTable *)ckalloc(sizeof(Tcl_HashTable));
		Tcl_InitHashTable(include_table, TCL_STRING_KEYS);
	}

	fileObj = Tcl_NewStringObj(file, -1);
	Tcl_IncrRefCount(fileObj);
	if ((Tcl_FSGetPathType(fileObj) == TCL_PATH_ABSOLUTE) || cwdOnly) {
		chan = include_try(fileObj, &found);
	} else {
		/* Try $BIN/include */
		binObj = TclGetObjNameOfExecutable();
		Tcl_GetStringFromObj(binObj, &len);
		if (len > 0) {
			Tcl_DecrRefCount(fileObj);
			/* TclPathPart bumps the ref count. */
			fileObj = TclPathPart(L->interp, binObj,
			    TCL_PATH_DIRNAME);
			Tcl_AppendPrintfToObj(fileObj, "/include/%s", file);
			chan = include_try(fileObj, &found);
			if (found) goto done;
		}
		/* Try /usr/local/include/L */
		Tcl_DecrRefCount(fileObj);
		fileObj = Tcl_ObjPrintf("/usr/local/include/L/%s", file);
		Tcl_IncrRefCount(fileObj);
		chan = include_try(fileObj, &found);
		if (found) goto done;
		/* Try /usr/include/L */
		Tcl_DecrRefCount(fileObj);
		fileObj = Tcl_ObjPrintf("/usr/include/L/%s", file);
		Tcl_IncrRefCount(fileObj);
		chan = include_try(fileObj, &found);
	}
 done:
	unless (found) {
		L_err("cannot find include file %s", file);
	}
	if (path) *path = ckstrdup(Tcl_GetString(fileObj));
	Tcl_DecrRefCount(fileObj);
	return (chan);
}

private int
include_push(Tcl_Channel chan, char *name)
{
	YY_BUFFER_STATE buf;
	Tcl_Obj		*objPtr;
	char		*dec = NULL, *script;
	int		len, ret;

	/* Read the file into memory. */
	objPtr = Tcl_NewObj();
	Tcl_IncrRefCount(objPtr);
	if (Tcl_ReadChars(chan, objPtr, -1, 0) < 0) {
		Tcl_Close(L->interp, chan);
		L_err("error reading include file %s", name);
		return (0);
	}
	Tcl_Close(L->interp, chan);

	/* If it is encrypted, decrypt it. */
	script = Tcl_GetStringFromObj(objPtr, &len);

	/* Create a new flex buffer with the file contents. */
	if (include_top >= INCLUDE_STACK_SZ) {
		L_err("include file nesting too deep -- aborting");
		while (include_pop()) ;
		ret = 0;
	} else {
		++include_top;
		include_stk[include_top].name = L->file;
		include_stk[include_top].dir  = L->dir;
		include_stk[include_top].line = L->line;
		include_stk[include_top].buf = YY_CURRENT_BUFFER;
		buf = yy_scan_bytes(script, len);
		L->file = name;
		L->dir  = L_dirname(L->file);
		L->line = 1;
		inject("#line 1\n");
		ret = 1;
	}
	Tcl_DecrRefCount(objPtr);
	if (dec) ckfree(dec);
	return (ret);
}

private int
include_pop()
{
	char	*s;

	if (include_top >= 0) {
		L->file = include_stk[include_top].name;
		L->dir  = include_stk[include_top].dir;
		L->line = include_stk[include_top].line;
		yy_delete_buffer(YY_CURRENT_BUFFER);
		yy_switch_to_buffer(include_stk[include_top].buf);
		--include_top;
		s = cksprintf("#line %d\n", L->line);
		inject(s);
		ckfree(s);
		return (1);
	} else {
		return (0);
	}
}

/*
 * Given a decimal, hex, or octal integer constant of arbitrary
 * precision, return a canonical string representation.  This is done
 * by converting it to a bignum and then taking its string rep.
 */
private char *
canonical_num(char *num)
{
	char	*ret;
	Tcl_Obj	*obj;
	mp_int	big;

	obj = Tcl_NewStringObj(num, -1);
	Tcl_IncrRefCount(obj);
	Tcl_TakeBignumFromObj(NULL, obj, &big);
	Tcl_SetBignumObj(obj, &big);
	ret = ckstrdup(Tcl_GetString(obj));
	Tcl_DecrRefCount(obj);
	return (ret);
}

/*
 * Work around a Windows problem where our getopt type conficts
 * with the system's.
 */
#undef getopt
#undef optarg
#undef optind

%}
%%
<INITIAL,interpol>{
	"("		return T_LPAREN;
	")"		return T_RPAREN;
	"{"		interpol_lbrace(); return T_LBRACE;
	"["		return T_LBRACKET;
	"]"		return T_RBRACKET;
	","		return T_COMMA;
	"!"		return T_BANG;
	"+"		return T_PLUS;
	"-"		return T_MINUS;
	"*"		return T_STAR;
	"/"		return T_SLASH;
	"%"		return T_PERC;
	"+="		return T_EQPLUS;
	"-="		return T_EQMINUS;
	"*="		return T_EQSTAR;
	"/="		return T_EQSLASH;
	"%="		return T_EQPERC;
	"&="		return T_EQBITAND;
	"|="		return T_EQBITOR;
	"^="		return T_EQBITXOR;
	"<<="		return T_EQLSHIFT;
	">>="		return T_EQRSHIFT;
	".="		return T_EQDOT;
	"++"		return T_PLUSPLUS;
	"--"		return T_MINUSMINUS;
	"&&"		return T_ANDAND;
	"||"		return T_OROR;
	"&"		return T_BITAND;
	"|"		return T_BITOR;
	"^"		return T_BITXOR;
	"~"		return T_BITNOT;
	"<<"		return T_LSHIFT;
	">>"		return T_RSHIFT;
	"="		return T_EQUALS;
	";"		return T_SEMI;
	"."		return T_DOT;
	[ \t\n\r]+"."[ \t\n\r]+		return T_STRCAT;
	".."		return T_DOTDOT;
	"..."		return T_ELLIPSIS;
	"class"		return T_CLASS;
	"extern"	return T_EXTERN;
	"return"	return T_RETURN;
	"void"		return T_VOID;
	"string"	return T_STRING;
	"widget"	return T_WIDGET;
	"int"		return T_INT;
	"float"		return T_FLOAT;
	"poly"		return T_POLY;
	"split"		return T_SPLIT;
	"if"		return T_IF;
	"else"		return T_ELSE;
	"unless"	return T_UNLESS;
	"while"		return T_WHILE;
	"do"		return T_DO;
	"for"		return T_FOR;
	"struct"	return T_STRUCT;
	"typedef"	return T_TYPEDEF;
	"defined"	return T_DEFINED;
	"foreach"	return T_FOREACH;
	"break"		return T_BREAK;
	"continue"	return T_CONTINUE;
	"instance"	return T_INSTANCE;
	"private"	return T_PRIVATE;
	"public"	return T_PUBLIC;
	"constructor"	return T_CONSTRUCTOR;
	"destructor"	return T_DESTRUCTOR;
	"expand"	return T_EXPAND;
	"_argused"	return T_ARGUSED;
	"_attribute"	return T_ATTRIBUTE;
	"_attributes"	return T_ATTRIBUTE;
	"_optional"	return T_OPTIONAL;
	"_mustbetype"	return T_MUSTBETYPE;
	"goto"		return T_GOTO;
	"switch"	return T_SWITCH;
	"case"		return T_CASE;
	"default"	return T_DEFAULT;
	"try"		return T_TRY;
	"=>"		return T_ARROW;
	"eq"		return T_EQ;
	"ne"		return T_NE;
	"lt"		return T_LT;
	"le"		return T_LE;
	"gt"		return T_GT;
	"ge"		return T_GE;
	"=="		return T_EQUALEQUAL;
	"!="		return T_NOTEQUAL;
	">"		return T_GREATER;
	">="		return T_GREATEREQ;
	"<"		return T_LESSTHAN;
	"<="		return T_LESSTHANEQ;
	"->"		return T_POINTS;
	":"		return T_COLON;
	"?"		return T_QUESTION;
	"?>"		{
				/*
				 * ?> marks the end of a script or expr
				 * inside of an lhtml document but is a
				 * syntax error otherwise.
				 */
				unless (in_lhtml) {
					undo_yy_user_action();
					REJECT;
				}
				yy_pop_state();
				STRBUF_START(L_lloc.end);
				if (YYSTATE == lhtml_expr_start) {
					yy_pop_state();  // pop back to lhtml
					ASSERT(YYSTATE == lhtml);
					return T_LHTML_EXPR_END;
				}
	}
	"and"		{
				L_err("'and','or','xor','not' are "
				      "unimplemented reserved words");
				return T_ANDAND;
	}
	"not"		{
				L_err("'and','or','xor','not' are "
				      "unimplemented reserved words");
				return T_BANG;
	}
	"or"		{
				L_err("'and','or','xor','not' are "
				      "unimplemented reserved words");
				return T_OROR;
	}
	"xor"		{
				L_err("'and','or','xor','not' are "
				      "unimplemented reserved words");
				return T_BITXOR;
	}
	{ID}		{
				Type *t = L_typedef_lookup(yytext);
				if (t) {
					L_lval.Typename.s = ckstrdup(yytext);
					L_lval.Typename.t = t;
					return T_TYPE;
				} else {
					L_lval.s = ckstrdup(yytext);
					return T_ID;
				}
			}
	{ID}:		{
				/*
				 * Push back the : and return a T_ID
				 * unless it's "default".  The grammar relies
				 * on this to avoid a nasty conflict.
				 */
				put_back(':');
				if (!strncmp(yytext, "default", 7)) {
					return T_DEFAULT;
				}
				L_lval.s = ckstrdup(yytext);
				L_lval.s[yyleng-1] = 0;
				return T_ID;
			}
	([A-Z]|::)([0-9a-zA-Z]|::)*_\*	{
				L_lval.s = ckstrdup(yytext);
				return T_PATTERN;
			}
	$[0-9]+		{
				/* Regular expression submatches */
				L_lval.s = ckstrdup(yytext);
				return T_ID;
			}
	[0-9]+		{
				/*
				 * Skip any leading 0's which would
				 * make it look like octal to Tcl.
				 */
				size_t	z = strspn(yytext, "0");
				if (z == yyleng) z = 0;  // number is all 0's
				L_lval.s = canonical_num(yytext+z);
				return T_INT_LITERAL;
			}
	0o[0-7]+	{
				/*
				 * Create a leading 0 so it looks like
				 * octal to Tcl.
				 */
				yytext[1] = '0';
				L_lval.s = canonical_num(yytext+1);
				return T_INT_LITERAL;
			}
	0x[0-9a-fA-F]+	{
				L_lval.s = canonical_num(yytext);
				return T_INT_LITERAL;
			}
	[0-9]*\.[0-9]+	{
				L_lval.s = ckstrdup(yytext);
				return T_FLOAT_LITERAL;
			}
	^#line[ \t]+[0-9]+\n {
				int	line = strtoul(yytext+5, NULL, 10);

				if (line <= 0) {
					--L->line;  // since \n already scanned
					L_err("malformed #line");
					++L->line;
				} else {
					L->line = line;
				}
			}
	^#line[ \t]+[0-9]+[ \t]+\"[^\"\n]*\"\n {
				int	line  = strtoul(yytext+5, NULL, 10);
				char	*beg  = strchr(yytext, '"') + 1;
				char	*end  = strrchr(yytext, '"');
				char	*name = ckstrndup(beg, end-beg);

				if (line <= 0) {
					--L->line;  // since \n already scanned
					L_err("malformed #line");
					++L->line;
				} else {
					L->file = name;
					L->line = line;
				}
			}
	^#line.*\n	{
				--L->line;  // since \n already scanned
				L_err("malformed #line");
				++L->line;
			}
	^#include[ \t]*\"[^\"\n]+\"	{
				char	*beg  = strchr(yytext, '"') + 1;
				char	*end  = strrchr(yytext, '"');
				char	*name = ckstrndup(beg, end-beg);
				Tcl_Channel chan;

				chan = include_search(name, NULL, 1);

				undo_yy_user_action();
				if (chan && !include_push(chan, name)) {
					/* Bail if includes nest too deeply. */
					yyterminate();
				}
			}
	^#include[ \t]*<[^>\n]+>	{
				char	*beg  = strchr(yytext, '<') + 1;
				char	*end  = strrchr(yytext, '>');
				char	*name = ckstrndup(beg, end-beg);
				char	*path = NULL;
				Tcl_Channel chan;

				chan = include_search(name, &path, 0);
				ckfree(name);

				undo_yy_user_action();
				if (chan && !include_push(chan, path)) {
					/* Bail if includes nest too deeply. */
					yyterminate();
				}
			}
	^#include	{
				L_err("malformed #include");
				yy_push_state(eat_through_eol);
			}
	^#pragma[ \t]+		return T_PRAGMA;
	^#.*("\r"|"\n"|"\r\n")	{
				/*
				 * Rather than using a start condition
				 * to separate out all the ^# patterns
				 * that don't end in \n, this is
				 * simpler.  If it's not a comment,
				 * REJECT it so that flex then takes
				 * the second best rule (those above).
				 */
				if (!strncmp(yytext, "#pragma ",  8) ||
				    !strncmp(yytext, "#pragma\t", 8)) {
					undo_yy_user_action();
					REJECT;
				} else if (!strncmp(yytext, "#include", 8)) {
					undo_yy_user_action();
					REJECT;
				} else unless (L->line == 2) {
					--L->line;  // since \n already scanned
					L_err("# comment valid only on line 1");
					++L->line;
				}
			}
	[ \t]+#.*("\r"|"\n"|"\r\n") {
				--L->line;  // since \n already scanned
				unless (L->line == 1) {
					L_err("# comment valid only on line 1");
				} else {
					L_err("# comment must start at "
					      "first column");
				}
				++L->line;
			}
	"//".*("\r"|"\n"|"\r\n")
	[ \t]+
	\n|\r|\f
	\"		yy_push_state(str_double); STRBUF_START(L->token_off);
	\'		yy_push_state(str_single); STRBUF_START(L->token_off);
	\`		yy_push_state(str_backtick); STRBUF_START(L->token_off);
	"/*"		yy_push_state(comment);
	[!=]~[ \t\r\n]*"m".	{
		yy_push_state(re_modifier);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 2);	// next token starts at the "m"
		extract_re_delims(yytext[yyleng-1]);
		L_lloc.end = L_lloc.beg + 2;	// this token spans the "=~"
		return ((yytext[0] == '=') ? T_EQTWID : T_BANGTWID);
	}
	/* if / is used to delimit the regexp, the m can be omitted */
	[!=]~[ \t\r\n]*"/"	{
		yy_push_state(re_modifier);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 1);	// next token starts at the "/"
		extract_re_delims('/');
		L_lloc.end = L_lloc.beg + 2;	// this token spans the "=~"
		return ((yytext[0] == '=') ? T_EQTWID : T_BANGTWID);
	}
	/* a substitution pattern */
	"=~"[ \t\r\n]*"s".	{
		yy_push_state(re_modifier);
		yy_push_state(subst_re);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 2);	// next token starts at the "s"
		extract_re_delims(yytext[yyleng-1]);
		L_lloc.end = L_lloc.beg + 2;	// this token spans the "=~"
		return T_EQTWID;
	}
	/* here document (interpolated), valid only on rhs of an assignment */
	=[ \t\r\n]*<<[a-zA-Z_][a-zA-Z_0-9]*\n		{
		char	*p, *q;

		if (here_delim) {
			L_err("nested here documents illegal");
		}
		p = strchr(yytext, '<') + 2;  // the < is guaranteed to exist
		for (q = p; (q > yytext) && (*q != '\n'); --q) ;
		if ((q > yytext) && (*q == '\n')) {
			// \n then <<; the in-between whitespace is the here_pfx
			here_pfx = ckstrndup(q+1, p-q-3);
		} else {
			// non-indented here document
			here_pfx = ckstrdup("");
		}
		here_delim = ckstrndup(p, yyleng - (p-yytext) - 1);
		STRBUF_START(L->token_off);
		L_lloc.end = L_lloc.beg + 1;
		yy_push_state(here_doc_interp);
		return T_EQUALS;
	}
	/* here document (uninterpolated), valid only on rhs of an assignment */
	=[ \t\r\n]*<<\'[a-zA-Z_][a-zA-Z_0-9]*\'\n		{
		char	*p, *q;

		if (here_delim) {
			L_err("nested here documents illegal");
		}
		p = strchr(yytext, '<') + 2;  // the < is guaranteed to exist
		for (q = p; (q > yytext) && (*q != '\n'); --q) ;
		if ((q > yytext) && (*q == '\n')) {
			// \n then <<; the in-between whitespace is the here_pfx
			here_pfx = ckstrndup(q+1, p-q-3);
		} else {
			// non-indented here document
			here_pfx = ckstrdup("");
		}
		here_delim = ckstrndup(p+1, yyleng - (p-yytext) - 3);
		STRBUF_START(L->token_off);
		L_lloc.end = L_lloc.beg + 1;
		yy_push_state(here_doc_nointerp);
		return T_EQUALS;
	}
	/* illegal here documents (bad stuff before or after the delim) */
	=[ \t\r\n]*<<-[a-zA-Z_][a-zA-Z_0-9]*			|
	=[ \t\r\n]*<<-\'[a-zA-Z_][a-zA-Z_0-9]*\'		{
		L_synerr("<<- unsupported, use =\\n\\t<<END to strip one "
			 "leading tab");
	}
	=[ \t\r\n]*<<[a-zA-Z_][a-zA-Z_0-9]*[^\n]		{
		L_synerr("illegal characters after here-document delimeter");
	}
	=[ \t\r\n]*<<[^a-zA-Z_][a-zA-Z_][a-zA-Z_0-9]*		{
		L_synerr("illegal characters before here-document delimeter");
	}
	=[ \t\r\n]*<<\'[a-zA-Z_][a-zA-Z_0-9]*\'[^\n]		{
		L_synerr("illegal characters after here-document delimeter");
	}
	=[ \t\r\n]*<<\'[^a-zA-Z_][a-zA-Z_][a-zA-Z_0-9]*\'	{
		L_synerr("illegal characters before here-document delimeter");
	}
}

<lhtml>{
	/*
	 * The compiler prepends a #line directive to Lhtml source.
	 * This communicates the correct line number to the Tcl
	 * code that prints run-time error messages.
	 */
	^#line[ \t]+[0-9]+\n {
		int	line = strtoul(yytext+5, NULL, 10);

		if (line <= 0) {
			--L->line;  // since \n already scanned
			L_err("malformed #line");
			++L->line;
		} else {
			L->line = line;
		}
	}
	"<?"=?	{
		L_lval.s = ckstrdup(STRBUF_STRING());
		STRBUF_STOP(L_lloc.beg);
		if (yyleng == 2) {
			yy_push_state(INITIAL);
		} else {
			yy_push_state(lhtml_expr_start);
		}
		return T_HTML;
	}
	.|\n	STRBUF_ADD(yytext, yyleng);
	<<EOF>>	{
		unless (STRBUF_STARTED()) yyterminate();
		L_lval.s = ckstrdup(STRBUF_STRING());
		STRBUF_STOP(L_lloc.beg);
		return T_HTML;
	}
}

<lhtml_expr_start>{
	/*
	 * This start condition is here only so the rule for ?> can
	 * know whether we previously scanned <? or <?=.
	 */
	.|\n	{
		unput(yytext[0]);
		undo_yy_user_action();
		yy_push_state(INITIAL);
		return T_LHTML_EXPR_START;
	}
}

<re_arg_split>{
	/*
	 * A regexp in the context of the first arg to split().  If
	 * it's not an RE, pop the start-condition stack and push it
	 * back, so we can continue as normal.
	 */
	[ \t\r\n]*
	/* / starts an RE */
	"/"	{
		yy_push_state(re_modifier);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 1);	// next token starts at the "/"
		extract_re_delims('/');
	}
	/*
	 * m<punctuation> starts an RE, except for "m)" so that
	 * "split(m)" works.
	 */
	"m"[^a-zA-Z() \t\r\n]	{
		yy_push_state(re_modifier);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 1);	// next token starts at the delim
		extract_re_delims(yytext[yyleng-1]);
	}
	/* nothing else starts an RE */
	.	{
		unput(yytext[0]);
		undo_yy_user_action();
		yy_pop_state();
	}
}

<re_arg_case>{
	/*
	 * A regexp in the context of a case statement.  If it's not
	 * an RE, pop the start-condition stack and push it back, so
	 * we can continue as normal.
	 */
	[ \t\r\n]*
	/* / starts an RE */
	"/"	{
		yy_push_state(re_modifier);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 1);	// next token starts at the "/"
		extract_re_delims('/');
	}
	/*
	 * m<punctuation> starts an RE except for "m:" which we scan
	 * as the variable m (so that "case m:" works) or "m(" which
	 * is the start of a call to the function m (so that "case m():"
	 * or "case m(arg):" etc work).
	 */
	m[^a-zA-Z:( \t\r\n]	{
		yy_push_state(re_modifier);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 1);	// next token starts at the delim
		extract_re_delims(yytext[yyleng-1]);
	}
	/* nothing else starts an RE */
	.	{
		unput(yytext[0]);
		undo_yy_user_action();
		yy_pop_state();
	}
}

<INITIAL>{
	"}"		return T_RBRACE;
}

<interpol>{
	"}"		{
				if (interpol_rbrace()) {
					STRBUF_START(L_lloc.end);
					interpol_pop();
					if ((YYSTATE == glob_re) ||
					    (YYSTATE == subst_re)) {
						return T_RIGHT_INTERPOL_RE;
					} else {
						return T_RIGHT_INTERPOL;
					}
				} else {
					return T_RBRACE;
				}
			}
	.	{
		L_synerr("illegal character");
	}
}

<str_double>{
	\\r		STRBUF_ADD("\r", 1);
	\\n		STRBUF_ADD("\n", 1);
	\\t		STRBUF_ADD("\t", 1);
	\\u{HEX}		|
	\\u{HEX}{HEX}		|
	\\u{HEX}{HEX}{HEX}	|
	\\u{HEX}{HEX}{HEX}{HEX}	{
				char	buf[TCL_UTF_MAX];
				int	ch;
				TclParseHex(yytext+2, 4, &ch);
				STRBUF_ADD(buf, Tcl_UniCharToUtf(ch, buf));
			}
	\\(.|\n)	STRBUF_ADD(yytext+1, 1);
	"$"		STRBUF_ADD("$", 1);
	\n		{
				L_err("missing string terminator \"");
				STRBUF_ADD("\n", 1);
			}
	[^\\\"$\n]+	STRBUF_ADD(yytext, yyleng);
	"${"		{
				if (interpol_push()) yyterminate();
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.beg);
				return T_LEFT_INTERPOL;
			}
	\"[ \t\r\n]*\"
	\"		{
				yy_pop_state();
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.end);
				return T_STR_LITERAL;
			}
}

<str_single>{
	\\\\		STRBUF_ADD("\\", 1);
	\\\'		STRBUF_ADD("'", 1);
	\\\n		STRBUF_ADD("\n", 1);
	\n		{
				L_err("missing string terminator \'");
				STRBUF_ADD("\n", 1);
			}
	\\.		|
	[^\\\'\n]+	STRBUF_ADD(yytext, yyleng);
	\'[ \t\r\n]*\'
	\'		{
				yy_pop_state();
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.end);
				return T_STR_LITERAL;
			}
}

<str_backtick>{
	\\("$"|`|\\)	STRBUF_ADD(yytext+1, 1);
	\\\n		/* ignore \<newline> */
	\\.		|
	"$"		|
	[^\\`$\n]+	STRBUF_ADD(yytext, yyleng);
	\n		{
				L_err("missing string terminator `");
				STRBUF_ADD("\n", 1);
			}
	"${"		{
				if (interpol_push()) yyterminate();
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.beg);
				return T_LEFT_INTERPOL;
			}
	`		{
				yy_pop_state();
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.end);
				if (YYSTATE == here_doc_interp) {
					STRBUF_START(L_lloc.end);
				}
				return T_STR_BACKTICK;
			}
}

<here_doc_nointerp>{
	^[ \t]*[a-zA-Z_][a-zA-Z_0-9]*;?$	{
				int	len;
				char	*p = yytext;

				/*
				 * Look for whitespace-prefixed here_delim.
				 * Any amount of white space is allowed.
				 */
				while (isspace(*p)) ++p;
				len = yyleng - (p - yytext);
				if (p[len-1] == ';') --len;
				if ((len == strlen(here_delim)) &&
				    !strncmp(p, here_delim, len)) {
					yy_pop_state();
					unput(';');  // for the parser
					L_lval.s = ckstrdup(STRBUF_STRING());
					STRBUF_STOP(L_lloc.end);
					ckfree(here_delim);
					ckfree(here_pfx);
					here_delim = NULL;
					here_pfx = NULL;
					return T_STR_LITERAL;
				}
				/*
				 * It's a data line.  It must begin with
				 * here_pfx or else it's an error.
				 */
				p = strstr(yytext, here_pfx);
				if (p == yytext) {
					p += strlen(here_pfx);
				} else {
					L_err("bad here-document prefix");
					p = yytext;
				}
				STRBUF_ADD(p, yyleng - (p - yytext));
			}
	^[ \t]+		{
				char	*p = strstr(yytext, here_pfx);
				if (p == yytext) {
					p += strlen(here_pfx);
					STRBUF_ADD(p, yyleng - (p - yytext));
				} else {
					L_err("bad here-document prefix");
					p = yytext;
				}
			}
	.|\n		STRBUF_ADD(yytext, 1);
}

<here_doc_interp>{
	\\\\		STRBUF_ADD("\\", 1);
	\\\$		STRBUF_ADD("$", 1);
	\\`		STRBUF_ADD("`", 1);
	\\\n		// ignore \<newline>
	"${"		{
				if (interpol_push()) yyterminate();
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.beg);
				return T_LEFT_INTERPOL;
			}
	`		{
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.beg);
				yy_push_state(str_backtick);
				STRBUF_START(L->token_off);
				return T_START_BACKTICK;
			}
	^[ \t]*[a-zA-Z_][a-zA-Z_0-9]*;?$	{
				int	len;
				char	*p = yytext;

				/*
				 * Look for whitespace-prefixed here_delim.
				 * Any amount of white space is allowed.
				 */
				while (isspace(*p)) ++p;
				len = yyleng - (p - yytext);
				if (p[len-1] == ';') --len;
				if ((len == strlen(here_delim)) &&
				    !strncmp(p, here_delim, len)) {
					yy_pop_state();
					unput(';');  // for the parser
					L_lval.s = ckstrdup(STRBUF_STRING());
					STRBUF_STOP(L_lloc.end);
					ckfree(here_delim);
					ckfree(here_pfx);
					here_delim = NULL;
					here_pfx = NULL;
					return T_STR_LITERAL;
				}
				/*
				 * It's a data line.  It must begin with
				 * here_pfx or else it's an error.
				 */
				p = strstr(yytext, here_pfx);
				if (p == yytext) {
					p += strlen(here_pfx);
				} else {
					L_err("bad here-document prefix");
					p = yytext;
				}
				STRBUF_ADD(p, yyleng - (p - yytext));
			}
	^[ \t]+		{
				char	*p = strstr(yytext, here_pfx);
				if (p == yytext) {
					p += strlen(here_pfx);
					STRBUF_ADD(p, yyleng - (p - yytext));
				} else {
					L_err("bad here-document prefix");
					p = yytext;
				}
			}
	.|\n		STRBUF_ADD(yytext, 1);
}

<comment>{
	[^*]+
	"*"
	"*/"		yy_pop_state();
}

<glob_re,subst_re>{
	"${"		{
				if (interpol_push()) yyterminate();
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.beg);
				return T_LEFT_INTERPOL_RE;
			}
	\\.		{
				if ((yytext[1] == re_end_delim) ||
				    (yytext[1] == re_start_delim)) {
					STRBUF_ADD(yytext+1, 1);
				} else {
					STRBUF_ADD(yytext, yyleng);
				}
			}
	\n		{
				--L->line;  // since \n already scanned
				L_err("run-away regular expression");
				++L->line;
				STRBUF_ADD(yytext, yyleng);
				yy_pop_state();
				if (YYSTATE == re_modifier) yy_pop_state();
				return T_RE;
			}
	"$"[0-9]	{
				// Convert $3 to \3 (regexp capture reference).
				STRBUF_ADD("\\", 1);
				STRBUF_ADD(yytext+1, yyleng-1);
			}
	.		{
				if (*yytext == re_end_delim) {
					L_lval.s = ckstrdup(STRBUF_STRING());
					STRBUF_STOP(L_lloc.end);
					if (YYSTATE == subst_re) {
						yy_pop_state();
						return T_SUBST;
					} else {
						yy_pop_state();
						if (YYSTATE == subst_re) {
							STRBUF_START(L_lloc.end);
							if (re_start_delim !=
							    re_end_delim) {
								yy_push_state(
								    re_delim);
							}
						}
						return T_RE;
					}
				} else if (*yytext == re_start_delim) {
					L_err("regexp delimiter must be quoted "
					      "inside the regexp");
					STRBUF_ADD(yytext+1, 1);
				} else {
					STRBUF_ADD(yytext, yyleng);
				}
			}

}

<re_delim>{
	\n		{
				--L->line;  // since \n already scanned
				L_err("run-away regular expression");
				++L->line;
				STRBUF_ADD(yytext, yyleng);
				yy_pop_state();
			}
	.		{
				extract_re_delims(*yytext);
				yy_pop_state();
			}
}

<re_modifier>{
	[iglt]+		{
				L_lval.s = ckstrdup(yytext);
				yy_pop_state();
				return T_RE_MODIFIER;
			}
	.|\n		{
				unput(yytext[0]);
				undo_yy_user_action();
				yy_pop_state();
				L_lval.s = ckstrdup("");
				return T_RE_MODIFIER;
			}
}

<eat_through_eol>{
	.
	\n	yy_pop_state();
}

	.		{
				/* This rule matches a char if no other does. */
				L_synerr("illegal character");
				yyterminate();
			}
	<<EOF>>		{
				if (in_lhtml) {
					yy_user_action();  // for line #s
					L_synerr("premature EOF");
				}
				unless (include_pop()) yyterminate();
			}
%%
void
L_lex_start()
{
	include_top = -1;
	if (in_lhtml) {
		STRBUF_START(0);
		BEGIN(lhtml);
	} else {
		BEGIN(INITIAL);
	}
}

void
L_lex_begReArg(int kind)
{
	switch (kind) {
	    case 0:
		yy_push_state(re_arg_split);
		break;
	    case 1:
		yy_push_state(re_arg_case);
		break;
	    default:
		break;
	}
}

private void
extract_re_delims(char c)
{
	re_start_delim = c;
	if (c == '{') {
		re_end_delim = '}';
	} else {
		re_end_delim = c;
	}
}

void
L_lex_begLhtml()
{
	in_lhtml = 1;
}

void
L_lex_endLhtml()
{
	in_lhtml = 0;
}

/*
 * These functions are declared down here because they reference
 * things that flex has not yet declared in the prelogue (like
 * unput() or yyterminate() etc).
 */

/*
 * Unput a single character.  This function is declared down here
 * because it calls flex's unput() which is not declared before
 * the prelogue code earlier.
 */
private void
put_back(char c)
{
	unput(c);
	--L_lloc.end;
	--L->prev_token_len;
	tally_newlines(&c, 1, -1);
	--L->script_len;
	Tcl_SetObjLength(L->script, L->script_len);
}

/*
 * API for scanning string interpolations:
 * interpol_push()	- call when starting an interpolation; returns 1
 *			  on interpolation stack overflow
 * interpol_pop()	- call when finishing an interpolation
 * interpol_lbrace()	- call when "{" seen
 * interpol_rbrace()	- call when "}" seen; returns non-0 if this brace
 *			  ends the current interpolation
 */

private int
interpol_push()
{
	if (interpol_top >= INTERPOL_STACK_SZ) {
		L_err("string interpolation nesting too deep -- aborting");
		interpol_top = -1;
		return (1);
	}
	interpol_stk[++interpol_top] = 0;
	yy_push_state(interpol);
	return (0);
}

private void
interpol_pop()
{
	ASSERT((interpol_top >= 0) && (interpol_top <= INTERPOL_STACK_SZ));
	--interpol_top;
	yy_pop_state();
}

private void
interpol_lbrace()
{
	if (interpol_top >= 0) {
		ASSERT(interpol_top <= INTERPOL_STACK_SZ);
		++interpol_stk[interpol_top];
	}
}

private int
interpol_rbrace()
{
	if (interpol_top >= 0) {
		ASSERT(interpol_top <= INTERPOL_STACK_SZ);
		return (interpol_stk[interpol_top]-- == 0);
	} else {
		return (0);
	}
}
