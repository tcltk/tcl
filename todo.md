# A faire :

* Pour `subst` command :
  * in tcl.h :
    * `#define TCL_SUBST_EXPR 8` in tcl.h (~ line 510)
    * `#define TCL_SUBST_ALL 15` in tcl.h (~ line 510)
  * in tclParse.c :
    * func ParseToken :
      * `int noSubstExpr = !(flags & TCL_SUBST_EXPR);` at the begining
      * add a test like :
      ```
      if (noSubstExpr) {
        tokenPtr->type = TCL_TOKEN_TEXT;
        tokenPtr->size = 1;
        parsePtr->numTokens++;
        src++;
        numBytes--;
        continue;
      }
      ```
    * func TclSubstParse : ??? Must be checked specifically to see what has to be done (how does this func works ?)
  * in tclCompCmdsSZ.c, func TclSubstCompile
    * create a case for TCL_TOKEN_SUB_EXPR (and then call Tcl_CompileExpr)

* Pour `info incomplete` command :
  * understand the way it works
  * create test cases

* Pour [tcl-parser](https://github.com/tcltk-depot/tcl-parser) :
  * try to compile it with the tip-759b branch
  * test

* Memory management :
  * in tclParse.c, ParseToken :
    * add `Tcl_FreeParse(exprParsePtr);`

* User Interface :
  * in tclParse.c :
    * func ParseToken :
      * decide if, after a ParseExpr error, we fall back to COMMAND case.
  * in tclCompExpr.c

* Expr language :
  * ?? Accept a sequence like : **test** `?` **expr**  `;`
  * 