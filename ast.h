#ifndef AST_H
#define AST_H

#include "util.h"
#include "token.h"

// EBNF
/*

prog    :	{ dcl ';'  |  func }
dcl	    :	type var_decl { ',' var_decl }
 	    |	[ extern ] type id '(' parm_types ')' { ',' id '(' parm_types ')' }
 	    |	[ extern ] void id '(' parm_types ')' { ',' id '(' parm_types ')' }
var_decl:	id [ '[' intcon ']' ]
type	:	char
 	    |	int
parm_types	:	void
 	        |	type id [ '[' ']' ] { ',' type id [ '[' ']' ] }
func	:	type id '(' parm_types ')' '{' { type var_decl { ',' var_decl } ';' } { stmt } '}'
 	    |	void id '(' parm_types ')' '{' { type var_decl { ',' var_decl } ';' } { stmt } '}'
stmt	:	if '(' expr ')' stmt [ else stmt ]
 	    |	while '(' expr ')' stmt
 	    |	for '(' [ assg ] ';' [ expr ] ';' [ assg ] ')' stmt
 	    |	return [ expr ] ';'
 	    |	assg ';'
 	    |	id '(' [expr { ',' expr } ] ')' ';'
 	    |	'{' { stmt } '}'
 	    |	';'
assg	:	id [ '[' expr ']' ] = expr
expr	:	'–' expr
 	    |	'!' expr
 	    |	expr binop expr
 	    |	expr relop expr
 	    |	expr logical_op expr
 	    |	id [ '(' [expr { ',' expr } ] ')' | '[' expr ']' ]
 	    |	'(' expr ')'
 	    |	intcon
 	    |	charcon
 	    |	stringcon
binop	:	+
 	    |	–
 	    |	*
 	    |	/
relop	:	==
 	    |	!=
 	    |	<=
 	    |	<
 	    |	>=
 	    |	>
logical_op	:	&&
 	        |	||
*/
typedef enum CType CType;
typedef enum ExpType ExpType;
typedef enum StmtType StmtType;
typedef struct Reg Reg;
typedef struct Type Type;
typedef struct Var Var;
typedef struct Expression Expression;
typedef struct Declaration Declaration;
typedef struct Expression Expression;
typedef struct Statement Statement;
typedef struct Function Function;
typedef struct Program Program;
typedef struct BB BB;

enum CType {
    VOID,
    CHAR,
    INT,
    PTR,
    ARRAY,
    STRUCT,
    FUNC,
};

enum ExpType {
    EXP_INT,
    EXP_CHAR,
    // EXP_STRUCT,
    EXP_UNOP,
    EXP_BINOP,
    EXP_MULTIOP,
    EXP_COND,
    EXP_ACCSESS,
    EXP_VARREF,
    EXP_ADDR,
    EXP_DEREF,
    EXP_FUNCCALL,
    EXP_STMT,
    EXP_ASSIGN,
};

enum StmtType {
    STMT_IF,
    STMT_FOR,
    STMT_DO_WHILE,
    STMT_SWITCH,
    STMT_CASE,
    STMT_BREAK,
    STMT_CONTINUE,
    // STMT_DECL,
    STMT_RETURN,
    STMT_COMP,
    STMT_EXP,
    STMT_NULL
};

struct Type {
    CType ty;
    int Size;  // sizeof
    int Align; // alignof

    // Pointer
    Type *Ptr;

    // Array
    Type *ArrPtr;
    int Len;

    // Struct
    // unsupported yet.
    Map *Members;
    int offset;

    // Function
    Type *Returning;
};

static Type VoidType = {VOID, 0, 0};
static Type CharType = {CHAR, 1, 1};
static Type IntType = {INT, 4, 4};

Type *PtrTo(Type *base);
Type *ArrayOf(Type *base, int len);
Type *NewFuncType(Type *returning);
Type *NewType(int ty, int size);
int  IsSameType(Type *x, Type *y);
int IsNumType(Type *ty);

// Represents a variable.
struct Var {
    Type *ty;
    char *Name;
    int Local;

    // Local variables are compiled to offsets from RBP.
    int offset;

    // Global variables are compiled to labels with optional
    // initialized data.
    char *data;

    // For optimization passes.
    int address_taken;
    Reg *promoted;
};

struct Declaration {
    Token *token;
    Type  *ty;
    char *Name;
    Expression *Init;
};

Declaration *NewDeclaration(Token *token, Type *ty, char *name);

struct Expression {
    ExpType ty;
    Type *ctype;

    Token *Op;

    // LogicalOr
    Expression *Cond;
    // UnopExp(1) | BinopExp(1) | ParensExp(1) | TableAccessExp(prefix) | FuncCallExp(id)
    Expression *Exp1;
    // BinopExp(2) | TableAccessExp(key)
    Expression *Exp2;
    // Logicalop | FuncCallExp(args) | Comma
    Vector *Exps;
    // StmtExp
    Vector *Stmts;

    // FuncCallExp(id) | VarRef(1)
    Var *ID;
    char *Name;

    // Assignment
    int VarDef;

    // IntExp | CharExp
    int Val;
};

Expression *NewExp(ExpType ty, Token *op);
Expression *NewUnop(Token *op, Expression *exp1);
Expression *NewAddr(Token *op, Expression *exp1);
Expression *NewAccess(Token *op, Expression *exp1);
Expression *NewBinop(Token *op, Expression *exp1, Expression *exp2);
Expression *NewVarref(Token *op, Var *var);
Expression *NewDeref(Token *op, Var *var);
Expression *NewIntExp(int val, Token *op);
Expression *NewCharExp(char val, Token *op);
Expression *NewStmtExp(Token *t, Vector *exps);
int IsNumExp(Expression *exp);
int IsLvalExp(Expression *exp);

struct Statement {
    StmtType ty;

    // if '(' expr cond ')' stmt [ else stmt ]
    // while '(' expr cond ')' stmt
    // for '(' [ assg init ] ';' [ expr cond ] ';' [ assg inc ] ')' stmt
    // do stmt "while" ( cond )
    Expression *Cond;
    Statement  *Body;
    Statement  *Else;
    Expression *Init;
    Expression *Step;

    // For switch and case
    Vector *Cases;
    BB *bb;

    // For case, break and continue
    // Statement *Target;
    BB *Break_;
    BB *Continue_;

    // Function definition
    Vector *Params;

    // return [ expr ] ';'
    // id '(' [expr { ',' expr } ] ')' ';'
    // exp ';'
    Expression *Exp;

    Declaration *Decl;

    // '{' { stmt } '}'
    Vector *Stmts;
    // ';'
};

static Statement NullStmt = {STMT_NULL};

// Statement *NewDeclStmt(Token *t, Declaration *decl);
Statement *NewExpStmt(Token *t, Expression *exp);
Statement *NewStmt(StmtType ty);

struct Function {
    char *Name;
    Statement *Stmt;
    Vector *LeftVars;
    Vector *bbs;
};

struct Program {
    Vector *GlobalVars;
    Vector *Functions;
    Map *macros;
};

struct Reg {
    int VirtualNumber; // virtual register number
    int RealNumber; // real register number

    // For optimizer
    Reg *Promoted;

    // For regalloc
    int def;
    int last_use;
    int spill;
    Var *var;
};

struct BB {
    int label;
    Vector *ir;
    Reg *param;

    // For liveness analysis
    Vector *succ;
    Vector *pred;
    Vector *def_regs;
    Vector *in_regs;
    Vector *out_regs;
};

#endif