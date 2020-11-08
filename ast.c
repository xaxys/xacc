#include <stdlib.h>
#include "ast.h"

int IsNumType(Type *ty) {
    return ty->ty == CHAR ||
           ty->ty == INT;
}

int IsLvalExp(Expression *exp) {
    return exp->ty == EXP_DEREF  ||
           exp->ty == EXP_VARREF ||
           exp->ty == EXP_ACCSESS;
}

int IsNumExp(Expression *exp) {
    return exp->ty == EXP_CHAR ||
           exp->ty == EXP_INT;
}

Expression *NewExp(ExpType ty, Token *op) {
    Expression *exp = calloc(1, sizeof(Expression));
    exp->ty = ty;
    exp->Op = op;
    return exp;
}

Expression *NewUnop(Token *op, Expression *exp1) {
    Expression *exp = NewExp(EXP_UNOP, op);
    exp->Exp1 = exp1;
    return exp;
}

Expression *NewAccess(Token *op, Expression *exp1) {
    Expression *exp = NewExp(EXP_ACCSESS, op);
    exp->Exp1 = exp1;
    return exp;
}

Expression *NewAddr(Token *op, Expression *exp1) {
    Expression *exp = NewExp(EXP_ADDR, op);
    exp->Exp1 = exp1;

    if (exp1->ty == EXP_VARREF) {
        exp1->ID->AddressTaken = 1;
    }
    return exp;
}

Expression *NewBinop(Token *op, Expression *exp1, Expression *exp2) {
    Expression *exp = NewExp(EXP_BINOP, op);
    exp->Exp1 = exp1;
    exp->Exp2 = exp2;
    return exp;
}

Expression *NewVarref(Token *op, Var *var) {
    Expression *exp = NewExp(EXP_VARREF, op);
    exp->ctype = var->ty;
    exp->ID = var;

    if (var->ty->ty == ARRAY) {
        Expression *tmp = NewExp(EXP_ADDR, op);
        tmp->ctype = PtrTo(var->ty->ArrPtr);
        tmp->Exp1 = exp;
        return tmp;
    }

    return exp;
}

Expression *NewDeref(Token *op, Expression *exp1) {
    Expression *exp = NewExp(EXP_DEREF, op);
    exp->ctype = exp1->ctype->Ptr;
    exp->Exp1 = exp1;

    if (exp->ctype->ty == ARRAY) {
        Expression *tmp = NewExp(EXP_ADDR, op);
        tmp->ctype = PtrTo(exp->ctype->ArrPtr);
        tmp->Exp1 = exp;
        return tmp;
    }
    return exp;
}

Expression *NewDerefVar(Token *op, Var *var) {
    return NewDeref(op, NewVarref(op, var));
}

Expression *NewIntExp(int val, Token *op) {
    Expression *exp = NewExp(EXP_INT, op);
    exp->ctype = &IntType;
    exp->Val = val;
    return exp;
}

Expression *NewCharExp(char val, Token *op) {
    Expression *exp = NewExp(EXP_CHAR, op);
    exp->ctype = &CharType;
    exp->Val = val;
    return exp;
}

Statement *NewStmt(StmtType ty) {
    Statement *stmt = calloc(1, sizeof(Statement));
    stmt->ty = ty;
    return stmt;
}

Statement *NewExpStmt(Token *t, Expression *exp) {
    Statement *stmt = NewStmt(STMT_EXP);
    stmt->Exp = exp;
    return stmt;
}

Expression *NewStmtExp(Token *t, Vector *exps) {
    Expression *last = (Expression *)VectorPop(exps);

    Vector *v = NewVector();
    for (int i = 0; i < VectorSize(exps); i++) {
        Statement *stmt = NewExpStmt(t, VectorGet(exps, i));
        VectorPush(v, stmt);
    }

    Expression *exp = NewExp(EXP_STMT, t);
    exp->Stmts = v;
    exp->Exp1 = last;
    exp->ctype = exp->Exp1->ctype;
    return exp;
}

Type *PtrTo(Type *base) {
    Type *ty = calloc(1, sizeof(Type));
    ty->ty = PTR;
    ty->Size = 8;
    ty->Align = 8;
    ty->Ptr = base;
    return ty;
}

Type *ArrayOf(Type *base, int len) {
    Type *ty = calloc(1, sizeof(Type));
    ty->ty = ARRAY;
    ty->Size = base->Size * len;
    ty->Align = base->Align;
    ty->ArrPtr = base;
    ty->Len = len;
    return ty;
}

Type *NewType(int ty, int size) {
    Type *ret = calloc(1, sizeof(Type));
    ret->ty = ty;
    ret->Size = size;
    ret->Align = size;
    return ret;
}

Type *NewFuncType(Type *returning) {
    Type *ty = calloc(1, sizeof(Type));
    ty->Returning = returning;
    return ty;
}

int IsSameType(Type *x, Type *y) {
    if (x->ty != y->ty) return 0;
    switch (x->ty) {
    case PTR:
        return IsSameType(x->Ptr, y->Ptr);
    case ARRAY:
        return x->Size == y->Size && IsSameType(x->ArrPtr, y->ArrPtr);
    case STRUCT:
    case FUNC:
        return x == y;
    default:
        return 1;
    }
}