#ifndef AST_H
#define AST_H

#include "util.h"
#include "token.h"

// EBNF
/*

prog    :   { dcl ';'  |  func }
dcl     :   lvar_decl
        |   [ extern ] type id '(' parm_types ')' { ',' id '(' parm_types ')' }
        |   [ extern ] void id '(' parm_types ')' { ',' id '(' parm_types ')' }
lvar_decl:  type var_decl { ',' var_decl }
var_decl:   id [ '[' intcon ']' ] [ '=' expr ]
type    :   char
        |   int
        |   type '*'
parm_types  :   void
            |   type id [ '[' ']' ] { ',' type id [ '[' ']' ] }
func    :   type id '(' parm_types ')' '{' { lvar_decl ';' } { stmt } '}'
        |   void id '(' parm_types ')' '{' { lvar_decl ';' } { stmt } '}'
stmt    :   if '(' expr ')' stmt [ else stmt ]
        |   while '(' expr ')' stmt
        |   for '(' [ expr | lvar_decl ] ';' [ expr ] ';' [ expr ] ')' stmt
        |   switch '(' expr ')' '{' stmt '}'
        |   case expr ':' stmt
        |   return [ expr ] ';'
        |   expr ';'
        |   id '(' [expr { ',' expr } ] ')' ';'
        |   '{' { stmt } '}'
        |   ';'
expr    :   id [ '[' expr ']' ] = expr
        |   unop expr
        |   expr binop expr
        |   expr binop '=' expr
        |   expr relop expr
        |   expr logical_op expr
        |   expr '?' expr ':' expr
        |   id [ '(' [expr { ',' expr } ] ')' | '[' expr ']' ]
        |   '(' expr ')'
        |   intcon
        |   charcon
        |   stringcon
unop    :   &
        |   *
        |   !
        |   *
        |   ~
        |   -
binop   :   +
        |   –
        |   *
        |   /
        |   %
relop   :   ==
        |   !=
        |   <=
        |   <
        |   >=
        |   >
logical_op  :   &&
            |   ||
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
    // STRUCT,
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
    int Offset;

    // Global variables are compiled to labels with optional
    // initialized data.
    char *StringData;
    char *RawData;
    int RawDataSize;

    // For optimization passes.
    int AddressTaken;
    Reg *Promoted;
};

Var *NewVar(Type *ty, char *name, int local);

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

    // CondExp
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

    // IntExp | CharExp
    int Val;
};

Expression *NewExp(ExpType ty, Token *op);
Expression *NewUnop(Token *op, Expression *exp1);
Expression *NewAddr(Token *op, Expression *exp1);
Expression *NewAccess(Token *op, Expression *exp1);
Expression *NewBinop(Token *op, Expression *exp1, Expression *exp2);
Expression *NewVarref(Token *op, Var *var);
Expression *NewDeref(Token *op, Expression *exp1);
Expression *NewDerefVar(Token *op, Var *var);
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
    int Default;
    Vector *Cases;
    BB *bb;

    // For case, break and continue
    BB *Break;
    BB *Continue;

    // // Function definition
    // Vector *Params;

    // return [ expr ] ';'
    // id '(' [expr { ',' expr } ] ')' ';'
    // exp ';'
    Expression *Exp;

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
    Vector *Params;
    Vector *LocalVars;
    Vector *bbs;
};

Function *NewFunction();

struct Program {
    Vector *GlobalVars;
    Vector *Functions;
    Map *macros;
};

Program *NewProgram();

struct Reg {
    int VirtualNum; // virtual register number
    int RealNum; // real register number

    // For optimizer
    Reg *Promoted;

    // For regalloc
    int Def;
    int LastUse;
    int Spill;
    Var *ID;
};

struct BB {
    int Label;
    Vector *IRs;
    Reg *Param;

    // For liveness analysis
    Vector *Succ;
    Vector *Pred;
    Vector *DefRegs;
    Vector *InRegs;
    Vector *OutRegs;
};

#endif