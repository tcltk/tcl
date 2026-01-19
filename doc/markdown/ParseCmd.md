---
CommandName: Tcl_ParseCommand
ManualSection: 3
Version: 8.3
TclPart: Tcl
TclDescription: Tcl Library Procedures
Keywords:
 - backslash substitution
 - braces
 - command
 - expression
 - parse
 - token
 - variable substitution
Copyright:
 - Copyright (c) 1997 Sun Microsystems, Inc.
---

# Name

Tcl\_ParseCommand, Tcl\_ParseExpr, Tcl\_ParseBraces, Tcl\_ParseQuotedString, Tcl\_ParseVarName, Tcl\_ParseVar, Tcl\_FreeParse, Tcl\_EvalTokensStandard - parse Tcl scripts and expressions

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[int]{.ret} [Tcl\_ParseCommand]{.ccmd}[interp, start, numBytes, nested, parsePtr]{.cargs}
[int]{.ret} [Tcl\_ParseExpr]{.ccmd}[interp, start, numBytes, parsePtr]{.cargs}
[int]{.ret} [Tcl\_ParseBraces]{.ccmd}[interp, start, numBytes, parsePtr, append, termPtr]{.cargs}
[int]{.ret} [Tcl\_ParseQuotedString]{.ccmd}[interp, start, numBytes, parsePtr, append, termPtr]{.cargs}
[int]{.ret} [Tcl\_ParseVarName]{.ccmd}[interp, start, numBytes, parsePtr, append]{.cargs}
[const char \*]{.ret} [Tcl\_ParseVar]{.ccmd}[interp, start, termPtr]{.cargs}
[Tcl\_FreeParse]{.ccmd}[usedParsePtr]{.cargs}
[int]{.ret} [Tcl\_EvalTokensStandard]{.ccmd}[interp, tokenPtr, numTokens]{.cargs}
:::

# Arguments

.AP Tcl\_Interp \*interp out For procedures other than **Tcl\_FreeParse** and **Tcl\_EvalTokensStandard**, used only for error reporting; if NULL, then no error messages are left after errors. For **Tcl\_EvalTokensStandard**, determines the context for evaluating the script and also is used for error reporting; must not be NULL. .AP "const char" \*start in Pointer to first character in string to parse. .AP Tcl\_Size numBytes in Number of bytes in string to parse, not including any terminating null character.  If less than 0 then the script consists of all characters following *start* up to the first null character. .AP int nested in Non-zero means that the script is part of a command substitution so an unquoted close bracket should be treated as a command terminator.  If zero, close brackets have no special meaning. .AP int append in Non-zero means that *\*parsePtr* already contains valid tokens; the new tokens should be appended to those already present.  Zero means that *\*parsePtr* is uninitialized; any information in it is ignored. This argument is normally 0. .AP Tcl\_Parse \*parsePtr out Points to structure to fill in with information about the parsed command, expression, variable name, etc. Any previous information in this structure is ignored, unless *append* is non-zero in a call to **Tcl\_ParseBraces**, **Tcl\_ParseQuotedString**, or **Tcl\_ParseVarName**. .AP "const char" \*\*termPtr out If not NULL, points to a location where **Tcl\_ParseBraces**, **Tcl\_ParseQuotedString**, and **Tcl\_ParseVar** will store a pointer to the character just after the terminating character (the close-brace, the last character of the variable name, or the close-quote (respectively)) if the parse was successful. .AP Tcl\_Parse \*usedParsePtr in Points to structure that was filled in by a previous call to **Tcl\_ParseCommand**, **Tcl\_ParseExpr**, **Tcl\_ParseVarName**, etc.

# Description

These procedures parse Tcl commands or portions of Tcl commands such as expressions or references to variables. Each procedure takes a pointer to a script (or portion thereof) and fills in the structure pointed to by *parsePtr* with a collection of tokens describing the information that was parsed. The procedures normally return **TCL\_OK**. However, if an error occurs then they return **TCL\_ERROR**, leave an error message in *interp*'s result (if *interp* is not NULL), and leave nothing in *parsePtr*.

**Tcl\_ParseCommand** is a procedure that parses Tcl scripts.  Given a pointer to a script, it parses the first command from the script.  If the command was parsed successfully, **Tcl\_ParseCommand** returns **TCL\_OK** and fills in the structure pointed to by *parsePtr* with information about the structure of the command (see below for details). If an error occurred in parsing the command then **TCL\_ERROR** is returned, an error message is left in *interp*'s result, and no information is left at *\*parsePtr*.

**Tcl\_ParseExpr** parses Tcl expressions. Given a pointer to a script containing an expression, **Tcl\_ParseExpr** parses the expression. If the expression was parsed successfully, **Tcl\_ParseExpr** returns **TCL\_OK** and fills in the structure pointed to by *parsePtr* with information about the structure of the expression (see below for details). If an error occurred in parsing the command then **TCL\_ERROR** is returned, an error message is left in *interp*'s result, and no information is left at *\*parsePtr*.

**Tcl\_ParseBraces** parses a string or command argument enclosed in braces such as **{hello}** or **{string \\t with \\t tabs}** from the beginning of its argument *start*. The first character of *start* must be **{**. If the braced string was parsed successfully, **Tcl\_ParseBraces** returns **TCL\_OK**, fills in the structure pointed to by *parsePtr* with information about the structure of the string (see below for details), and stores a pointer to the character just after the terminating **}** in the location given by *\*termPtr*. If an error occurs while parsing the string then **TCL\_ERROR** is returned, an error message is left in *interp*'s result, and no information is left at *\*parsePtr* or *\*termPtr*.

**Tcl\_ParseQuotedString** parses a double-quoted string such as **"sum is [expr {$a+$b}]"** from the beginning of the argument *start*. The first character of *start* must be **\\"**. If the double-quoted string was parsed successfully, **Tcl\_ParseQuotedString** returns **TCL\_OK**, fills in the structure pointed to by *parsePtr* with information about the structure of the string (see below for details), and stores a pointer to the character just after the terminating **\\"** in the location given by *\*termPtr*. If an error occurs while parsing the string then **TCL\_ERROR** is returned, an error message is left in *interp*'s result, and no information is left at *\*parsePtr* or *\*termPtr*.

**Tcl\_ParseVarName** parses a Tcl variable reference such as **$abc** or **$x([expr {$index + 1}])** from the beginning of its *start* argument. The first character of *start* must be **$**. If a variable name was parsed successfully, **Tcl\_ParseVarName** returns **TCL\_OK** and fills in the structure pointed to by *parsePtr* with information about the structure of the variable name (see below for details).  If an error occurs while parsing the command then **TCL\_ERROR** is returned, an error message is left in *interp*'s result (if *interp* is not NULL), and no information is left at *\*parsePtr*.

**Tcl\_ParseVar** parses a Tcl variable reference such as **$abc** or **$x([expr {$index + 1}])** from the beginning of its *start* argument.  The first character of *start* must be **$**.  If the variable name is parsed successfully, **Tcl\_ParseVar** returns a pointer to the string value of the variable.  If an error occurs while parsing, then NULL is returned and an error message is left in *interp*'s result.

The information left at *\*parsePtr* by **Tcl\_ParseCommand**, **Tcl\_ParseExpr**, **Tcl\_ParseBraces**, **Tcl\_ParseQuotedString**, and **Tcl\_ParseVarName** may include dynamically allocated memory. If these five parsing procedures return **TCL\_OK** then the caller must invoke **Tcl\_FreeParse** to release the storage at *\*parsePtr*. These procedures ignore any existing information in *\*parsePtr* (unless *append* is non-zero), so if repeated calls are being made to any of them then **Tcl\_FreeParse** must be invoked once after each call.

**Tcl\_EvalTokensStandard** evaluates a sequence of parse tokens from a Tcl\_Parse structure.  The tokens typically consist of all the tokens in a word or all the tokens that make up the index for a reference to an array variable.  **Tcl\_EvalTokensStandard** performs the substitutions requested by the tokens and concatenates the resulting values. The return value from **Tcl\_EvalTokensStandard** is a Tcl completion code with one of the values **TCL\_OK**, **TCL\_ERROR**, **TCL\_RETURN**, **TCL\_BREAK**, or **TCL\_CONTINUE**, or possibly some other integer value originating in an extension. In addition, a result value or error message is left in *interp*'s result; it can be retrieved using **Tcl\_GetObjResult**.

# Tcl\_parse structure

**Tcl\_ParseCommand**, **Tcl\_ParseExpr**, **Tcl\_ParseBraces**, **Tcl\_ParseQuotedString**, and **Tcl\_ParseVarName** return parse information in two data structures, Tcl\_Parse and Tcl\_Token:

```
typedef struct {
    const char *commentStart;
    Tcl_Size commentSize;
    const char *commandStart;
    Tcl_Size commandSize;
    Tcl_Size numWords;
    Tcl_Token *tokenPtr;
    Tcl_Size numTokens;
    ...
} Tcl_Parse;

typedef struct {
    int type;
    const char *start;
    Tcl_Size size;
    Tcl_Size numComponents;
} Tcl_Token;
```

The first five fields of a Tcl\_Parse structure are filled in only by **Tcl\_ParseCommand**. These fields are not used by the other parsing procedures.

**Tcl\_ParseCommand** fills in a Tcl\_Parse structure with information that describes one Tcl command and any comments that precede the command. If there are comments, the *commentStart* field points to the **#** character that begins the first comment and *commentSize* indicates the number of bytes in all of the comments preceding the command, including the newline character that terminates the last comment. If the command is not preceded by any comments, *commentSize* is 0. **Tcl\_ParseCommand** also sets the *commandStart* field to point to the first character of the first word in the command (skipping any comments and leading space) and *commandSize* gives the total number of bytes in the command, including the character pointed to by *commandStart* up to and including the newline, close bracket, or semicolon character that terminates the command.  The *numWords* field gives the total number of words in the command.

All parsing procedures set the remaining fields, *tokenPtr* and *numTokens*. The *tokenPtr* field points to the first in an array of Tcl\_Token structures that describe the components of the entity being parsed. The *numTokens* field gives the total number of tokens present in the array. Each token contains four fields. The *type* field selects one of several token types that are described below.  The *start* field points to the first character in the token and the *size* field gives the total number of characters in the token.  Some token types, such as **TCL\_TOKEN\_WORD** and **TCL\_TOKEN\_VARIABLE**, consist of several component tokens, which immediately follow the parent token; the *numComponents* field describes how many of these there are. The *type* field has one of the following values:

**TCL\_TOKEN\_WORD**
: This token ordinarily describes one word of a command but it may also describe a quoted or braced string in an expression. The token describes a component of the script that is the result of concatenating together a sequence of subcomponents, each described by a separate subtoken. The token starts with the first non-blank character of the component (which may be a double-quote or open brace) and includes all characters in the component up to but not including the space, semicolon, close bracket, close quote, or close brace that terminates the component.  The *numComponents* field counts the total number of sub-tokens that make up the word, including sub-tokens of **TCL\_TOKEN\_VARIABLE** and **TCL\_TOKEN\_BS** tokens.

**TCL\_TOKEN\_SIMPLE\_WORD**
: This token has the same meaning as **TCL\_TOKEN\_WORD**, except that the word is guaranteed to consist of a single **TCL\_TOKEN\_TEXT** sub-token.  The *numComponents* field is always 1.

**TCL\_TOKEN\_EXPAND\_WORD**
: This token has the same meaning as **TCL\_TOKEN\_WORD**, except that the command parser notes this word began with the expansion prefix **{\*}**, indicating that after substitution, the list value of this word should be expanded to form multiple arguments in command evaluation.  This token type can only be created by Tcl\_ParseCommand.

**TCL\_TOKEN\_TEXT**
: The token describes a range of literal text that is part of a word. The *numComponents* field is always 0.

**TCL\_TOKEN\_BS**
: The token describes a backslash sequence such as **\\n** or **\\0xA3**. The *numComponents* field is always 0.

**TCL\_TOKEN\_COMMAND**
: The token describes a command whose result must be substituted into the word.  The token includes the square brackets that surround the command.  The *numComponents* field is always 0 (the nested command is not parsed; call **Tcl\_ParseCommand** recursively if you want to see its tokens).

**TCL\_TOKEN\_VARIABLE**
: The token describes a variable substitution, including the **$**, variable name, and array index (if there is one) up through the close parenthesis that terminates the index.  This token is followed by one or more additional tokens that describe the variable name and array index.  If *numComponents*  is 1 then the variable is a scalar and the next token is a **TCL\_TOKEN\_TEXT** token that gives the variable name.  If *numComponents* is greater than 1 then the variable is an array: the first sub-token is a **TCL\_TOKEN\_TEXT** token giving the array name and the remaining sub-tokens are **TCL\_TOKEN\_TEXT**, **TCL\_TOKEN\_BS**, **TCL\_TOKEN\_COMMAND**, and **TCL\_TOKEN\_VARIABLE** tokens that must be concatenated to produce the array index. The *numComponents* field includes nested sub-tokens that are part of **TCL\_TOKEN\_VARIABLE** tokens in the array index.

**TCL\_TOKEN\_SUB\_EXPR**
: The token describes one subexpression of an expression (or an entire expression). A subexpression may consist of a value such as an integer literal, variable substitution, or parenthesized subexpression; it may also consist of an operator and its operands. The token starts with the first non-blank character of the subexpression up to but not including the space, brace, close-paren, or bracket that terminates the subexpression. This token is followed by one or more additional tokens that describe the subexpression. If the first sub-token after the **TCL\_TOKEN\_SUB\_EXPR** token is a **TCL\_TOKEN\_OPERATOR** token, the subexpression consists of an operator and its token operands. If the operator has no operands, the subexpression consists of just the **TCL\_TOKEN\_OPERATOR** token. Each operand is described by a **TCL\_TOKEN\_SUB\_EXPR** token. Otherwise, the subexpression is a value described by one of the token types **TCL\_TOKEN\_WORD**, **TCL\_TOKEN\_TEXT**, **TCL\_TOKEN\_BS**, **TCL\_TOKEN\_COMMAND**, **TCL\_TOKEN\_VARIABLE**, and **TCL\_TOKEN\_SUB\_EXPR**. The *numComponents* field counts the total number of sub-tokens that make up the subexpression; this includes the sub-tokens for any nested **TCL\_TOKEN\_SUB\_EXPR** tokens.

**TCL\_TOKEN\_OPERATOR**
: The token describes one operator of an expression such as **&&** or **hypot**. A **TCL\_TOKEN\_OPERATOR** token is always preceded by a **TCL\_TOKEN\_SUB\_EXPR** token that describes the operator and its operands; the **TCL\_TOKEN\_SUB\_EXPR** token's *numComponents* field can be used to determine the number of operands. A binary operator such as **\*** is followed by two **TCL\_TOKEN\_SUB\_EXPR** tokens that describe its operands. A unary operator like **-** is followed by a single **TCL\_TOKEN\_SUB\_EXPR** token for its operand. If the operator is a math function such as **log10**, the **TCL\_TOKEN\_OPERATOR** token will give its name and the following **TCL\_TOKEN\_SUB\_EXPR** tokens will describe its operands; if there are no operands (as with **rand**), no **TCL\_TOKEN\_SUB\_EXPR** tokens follow. There is one trinary operator, **?**, that appears in if-then-else subexpressions such as *x***?***y***:***z*; in this case, the **?** **TCL\_TOKEN\_OPERATOR** token is followed by three **TCL\_TOKEN\_SUB\_EXPR** tokens for the operands *x*, *y*, and *z*. The *numComponents* field for a **TCL\_TOKEN\_OPERATOR** token is always 0.


After **Tcl\_ParseCommand** returns, the first token pointed to by the *tokenPtr* field of the Tcl\_Parse structure always has type **TCL\_TOKEN\_WORD** or **TCL\_TOKEN\_SIMPLE\_WORD** or **TCL\_TOKEN\_EXPAND\_WORD**. It is followed by the sub-tokens that must be concatenated to produce the value of that word. The next token is the **TCL\_TOKEN\_WORD** or **TCL\_TOKEN\_SIMPLE\_WORD** of **TCL\_TOKEN\_EXPAND\_WORD** token for the second word, followed by sub-tokens for that word, and so on until all *numWords* have been accounted for.

After **Tcl\_ParseExpr** returns, the first token pointed to by the *tokenPtr* field of the Tcl\_Parse structure always has type **TCL\_TOKEN\_SUB\_EXPR**. It is followed by the sub-tokens that must be evaluated to produce the value of the expression. Only the token information in the Tcl\_Parse structure is modified: the *commentStart*, *commentSize*, *commandStart*, and *commandSize* fields are not modified by **Tcl\_ParseExpr**.

After **Tcl\_ParseBraces** returns, the array of tokens pointed to by the *tokenPtr* field of the Tcl\_Parse structure will contain a single **TCL\_TOKEN\_TEXT** token if the braced string does not contain any backslash-newlines. If the string does contain backslash-newlines, the array of tokens will contain one or more **TCL\_TOKEN\_TEXT** or **TCL\_TOKEN\_BS** sub-tokens that must be concatenated to produce the value of the string. If the braced string was just **{}** (that is, the string was empty), the single **TCL\_TOKEN\_TEXT** token will have a *size* field containing zero; this ensures that at least one token appears to describe the braced string. Only the token information in the Tcl\_Parse structure is modified: the *commentStart*, *commentSize*, *commandStart*, and *commandSize* fields are not modified by **Tcl\_ParseBraces**.

After **Tcl\_ParseQuotedString** returns, the array of tokens pointed to by the *tokenPtr* field of the Tcl\_Parse structure depends on the contents of the quoted string. It will consist of one or more **TCL\_TOKEN\_TEXT**, **TCL\_TOKEN\_BS**, **TCL\_TOKEN\_COMMAND**, and **TCL\_TOKEN\_VARIABLE** sub-tokens. The array always contains at least one token; for example, if the argument *start* is empty, the array returned consists of a single **TCL\_TOKEN\_TEXT** token with a zero *size* field. Only the token information in the Tcl\_Parse structure is modified: the *commentStart*, *commentSize*, *commandStart*, and *commandSize* fields are not modified.

After **Tcl\_ParseVarName** returns, the first token pointed to by the *tokenPtr* field of the Tcl\_Parse structure always has type **TCL\_TOKEN\_VARIABLE**.  It is followed by the sub-tokens that make up the variable name as described above.  The total length of the variable name is contained in the *size* field of the first token. As in **Tcl\_ParseExpr**, only the token information in the Tcl\_Parse structure is modified by **Tcl\_ParseVarName**: the *commentStart*, *commentSize*, *commandStart*, and *commandSize* fields are not modified.

All of the character pointers in the Tcl\_Parse and Tcl\_Token structures refer to characters in the *start* argument passed to **Tcl\_ParseCommand**, **Tcl\_ParseExpr**, **Tcl\_ParseBraces**, **Tcl\_ParseQuotedString**, and **Tcl\_ParseVarName**.

There are additional fields in the Tcl\_Parse structure after the *numTokens* field, but these are for the private use of **Tcl\_ParseCommand**, **Tcl\_ParseExpr**, **Tcl\_ParseBraces**, **Tcl\_ParseQuotedString**, and **Tcl\_ParseVarName**; they should not be referenced by code outside of these procedures.

