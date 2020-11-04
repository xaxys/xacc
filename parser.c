#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "util.h"
#include "parser.h"
#include "token.h"

typedef struct Env {
    Map *vars;
    Map *typedefs;
    Map *tags;
    struct Env *prev;

    Vector *LocalVars;
    Vector *Breaks;
    Vector *Continues;
    Vector *Switches;
} Env;

Env *env;

Program *program;

Vector *lvars;
Vector *breaks;
Vector *continues;
Vector *switches;
int nLabel = 1;

// declare forward.
Statement *parseCompoundStmt(Lexer *lexer);
Expression *parseAssign(Lexer *lexer);
Expression *parseExp(Lexer *lexer);
Expression *NewAssignEqual(Token *op, Expression *exp1, Expression *exp2);
Declaration *Declarator(Lexer *lexer, Type *ty);

static void ErrorAt(Lexer *lexer, char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    char *start = loc, *end = loc;
    while (start > lexer->chunk && *(start - 1) != '\n') start--;
    while (*end != '\0' && *end != '\n') end++;
    *end = '\0';
    int pos = loc - start;

    fprintf(stderr, "Syntax Error:\n");
    fprintf(stderr, "File: %s, Line: %d.\n", lexer->chunkName, lexer->line);
    fprintf(stderr, "\n");
    fprintf(stderr, "%s\n", start);
    fprintf(stderr, "%*s\n", pos+1, "^");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

static void Error(Lexer *lexer, Token *token, char *s) {
    ErrorAt(lexer, token->Original, s);
}

static Token *ExpectToken(Lexer *lexer, TokenType type) {
    Token *token = ConsumeToken(lexer, type);
    if (token == NULL) {
        Token *token = PeekToken(lexer);
        ErrorAt(lexer, token->Original, "symbol '%s' expected, but found '%s'.",
              GetTokenTypeLiteral(type), GetTokenTypeLiteral(token->Type));
    } else {
        return token;
    }
}

Env *newEnv(Env *prev) {
    Env *env = calloc(1, sizeof(Env));
    env->vars = NewMap();
    env->typedefs = NewMap();
    env->tags = NewMap();
    env->prev = prev;
    return env;
}

Var *getVar(Env *env, char *name) {
    for (Env *e = env; e; e = e->prev) {
        Var *var = MapGet(e->vars, name);
        if (var) return var;
    }
    return NULL;
}

Type *getTypedef(Env *env, char *name) {
    for (Env *e = env; e; e = e->prev) {
        Type *ty = MapGet(e->typedefs, name);
        if (ty) return ty;
    }
    return NULL;
}

Type *getTag(Env *env, char *name) {
    for (Env *e = env; e; e = e->prev) {
        Type *ty = MapGet(e->tags, name);
        if (ty) return ty;
    }
    return NULL;
}

Var *addLocalVar(Type *ty, char *name) {
    Var *var = calloc(1, sizeof(Var));
    var->ty = ty;
    var->Local = 1;
    var->Name = name;
    MapPut(env->vars, name, var);
    VectorPush(lvars, var);
    return var;
}

Var *addGlobalVar(Program* program, Type *ty, char *name, char *data, int _extern) {
    Var *var = calloc(1, sizeof(Var));
    var->ty = ty;
    var->Local = 0;
    var->Name = name;
    var->data = data;
    var->promoted = NULL;
    MapPut(env->vars, name, var);
    if (!_extern) VectorPush(program->GlobalVars, var);
    return var;
}

Expression *NewStringExp(char *s) {
    Type *ty = ArrayOf(&CharType, strlen(s));
    char *name = format(".L.str%d", nLabel++);
    Var *var = addGlobalVar(program, ty, name, s, 0);
    Expression *exp = NewVarref(NULL, var);

    if (exp->ctype->ty == ARRAY) {
        Expression *tmp = NewExp(EXP_ADDR, NULL);
        tmp->ctype = PtrTo(exp->ctype->ArrPtr);
        tmp->Exp1 = exp;
        exp = tmp;
    }
    return exp;
}

Expression *scalePtr(Expression *exp, Type *ty) {
    Token *token = calloc(1, sizeof(Token));
    token->Line = exp->Op->Line;
    token->Literal = exp->Op->Literal;
    token->Type = TOKEN_OP_MUL;
    return NewBinop(token, exp, NewIntExp(ty->Size, token));
}

Expression *parseLocalVar(Lexer *lexer, Token *token) {
    Var *var = getVar(env, token->Literal);
    if (!var) Error(lexer, token, "Undefined variable");
    Expression *exp = NewVarref(token, var);
    exp->Name = token->Literal;
    return exp;
}

Expression *parseFuncCall(Lexer *lexer, Token *token) {
    Var *var = getVar(env, token->Literal);

    Expression *exp = NewExp(EXP_FUNCCALL, token);
    exp->Name = token->Literal;
    exp->ID = var;
    exp->Exps = NewVector();

    if (var && var->ty->ty == FUNC) {
        exp->ctype = var->ty;
    } else {
        Error(lexer, token, "Undefined function");
        exp->ctype = NewFuncType(&IntType);
    }

    while (!ConsumeToken(lexer, TOKEN_SEP_RPAREN)) {
        if (VectorSize(exp->Exps) > 0) {
            ExpectToken(lexer, TOKEN_SEP_COMMA);
        }
        VectorPush(exp->Exps, parseAssign(lexer));
    }

    exp->ctype = exp->ID->ty->Returning;
    return exp;
}

// Expression *parseStmtExp(Lexer *lexer) {
//     Token *token = PeekToken(lexer);
//     Vector *v = NewVector();

//     env = NewEnv(env);
//     do {
//         VectorPush(v, parseStmt(lexer));
//     } while (!NextTokenOfType(lexer, TOKEN_SEP_RCURLY));
//     MustNextTokenOfType(lexer, TOKEN_SEP_RPAREN);
//     env = env->prev;

//     Expression *last = VectorPop(v);
//     if (last->op != ND_EXPR_STMT) {
//         Error(lexer, "statement expression returning void");
//     }

//     Node *node = new_node(ND_STMT_EXPR, t);
//     node->stmts = v;
//     node->expr = last->expr;
//     return node;
// }

Expression *parsePrimary(Lexer *lexer) {
    // 0 primary
    Token *token = PeekToken(lexer);

    if (token->Type == TOKEN_SEP_LPAREN) {
        NextToken(lexer);
        // if (NextTokenOfType(lexer, TOKEN_SEP_LCURLY)) {
        //     return parseStmtExp(lexer);
        // }
        Expression *exp = parseExp(lexer);
        ExpectToken(lexer, TOKEN_SEP_RPAREN);
        return exp;
    }

    if (token->Type == TOKEN_NUMBER) {
        NextToken(lexer);
        return NewIntExp(atoi(token->Literal), token);
    }

    if (token->Type == TOKEN_CHAR) {
        NextToken(lexer);
        return NewCharExp(*(token->Literal), token);
    }

    if (token->Type == TOKEN_STRING) {
        NextToken(lexer);
        return NewStringExp(token->Literal);
    }

    if (token->Type == TOKEN_IDENTIFIER) {
        NextToken(lexer);
        if (ConsumeToken(lexer, TOKEN_SEP_LPAREN)) {
            return parseFuncCall(lexer, token);
        }
        return parseLocalVar(lexer, token);
    }
 
    Error(lexer, token, "Primary expression expected.");
}

// `x++` where x is of type T is compiled as
// `({ T *y = &x; T z = *y; *y = *y + 1; *z; })`.
Expression *NewPostIncrease(Token *token, Expression *exp, int imm) {
    Vector *v = NewVector();

    Var *var1 = addLocalVar(PtrTo(exp->ctype), "tmp");
    Var *var2 = addLocalVar(exp->ctype, "tmp");

    Expression *exp1 = NewExp(EXP_ASSIGN, NULL);
    exp1->Exp1 = NewVarref(NULL, var1);
    exp1->Exp2 = NewAddr(NULL, exp);
    exp1->ctype = exp1->Exp1->ctype;

    Expression *exp2 = NewExp(EXP_ASSIGN, NULL);
    exp2->Exp1 = NewVarref(NULL, var2);
    exp2->Exp2 = NewDerefVar(NULL, var1);
    exp2->ctype = exp2->Exp1->ctype;

    Expression *exp3 = NewExp(EXP_ASSIGN, NULL);
    exp3->Exp1 = NewDerefVar(NULL, var1);
    token->Type = TOKEN_OP_ADD;
    exp3->Exp2 = NewBinop(token, NewDerefVar(NULL, var1), NewIntExp(imm, NULL));
    exp3->ctype = exp3->Exp1->ctype;

    Expression *exp4 = NewVarref(NULL, var2);

    VectorPush(v, exp1);
    VectorPush(v, exp2);
    VectorPush(v, exp3);
    VectorPush(v, exp4);

    return NewStmtExp(token, v);
}

Expression *parsePostPrefix(Lexer  *lexer) {
    // 1 ++, --
    Expression *exp = parsePrimary(lexer);
    for (;;) {
        Token *token = PeekToken(lexer);
        if (token->Type == TOKEN_OP_ADDSELF) {
            NextToken(lexer);
            exp = NewPostIncrease(token, exp, 1);
            continue;
        }

        if (token->Type == TOKEN_OP_SUBSELF) {
            NextToken(lexer);
            exp = NewPostIncrease(token, exp, -1);
            continue;
        }

        if (token->Type == TOKEN_SEP_DOT) {
            NextToken(lexer);
            exp = NewAccess(token, exp);
            exp->Name = ExpectToken(lexer, TOKEN_IDENTIFIER)->Literal;
            continue;
        }

        if (token->Type == TOKEN_OP_ARROW) {
            NextToken(lexer);
            Expression *tmp = NewDeref(token, exp);

            exp = NewAccess(token, tmp);
            exp->Name = ExpectToken(lexer, TOKEN_IDENTIFIER)->Literal;
            continue;
        }

        if (token->Type == TOKEN_SEP_LBRACK) {
            NextToken(lexer);
            token->Type = TOKEN_OP_ADD;
            Expression *tmp = NewBinop(token, exp, parseAssign(lexer));
            tmp->ctype = tmp->Exp1->ctype;
            exp = NewDeref(token, tmp);
            ExpectToken(lexer, TOKEN_SEP_RBRACK);
            continue;
        }

        return exp;
    }
}

Expression *parseUnary(Lexer *lexer) {
    // 2 -, !, ~, *, &, ++, --
    Token *token = PeekToken(lexer);
    // -
    if (token->Type == TOKEN_OP_SUB) {
        NextToken(lexer);
        Expression *exp = parseUnary(lexer);

        //optimize const exp
        if (IsNumExp(exp)) {
            exp->Val = -exp->Val;
        } else {
            exp = NewBinop(token, NewIntExp(0, NULL), exp);
            if (!IsNumType(exp->Exp2->ctype)) {
                Error(lexer, token, "The right side of the operator is not a number.");
            }
            exp->ctype = &IntType;
        }
        return exp;
    }
    // !, ~
    if (token->Type == TOKEN_OP_NOT ||
        token->Type == TOKEN_OP_BNOT) {
        NextToken(lexer);
        Expression *exp = parseUnary(lexer);

        //optimize const exp
        if (IsNumExp(exp)) {
            switch (token->Type) {
            case TOKEN_OP_NOT:
                exp->Val = exp->Val == 0;
            case TOKEN_OP_BNOT:
                exp->Val = ~exp->Val;
            }
        } else {
            exp = NewUnop(token, exp);
            if (!IsNumType(exp->Exp1->ctype)) {
                Error(lexer, token, "The right side of the operator is not a number.");
            }
            exp->ctype = &IntType;
        }
        return exp;
    }
    // *
    if (token->Type == TOKEN_OP_MUL) {
        NextToken(lexer);
        Expression *exp = NewDeref(token, parseUnary(lexer));

        if (exp->Exp1->ctype->ty != PTR) {
            Error(lexer, token, "Operand must be a pointer.");
        }

        if (exp->Exp1->ctype->Ptr->ty == VOID) {
            Error(lexer, token, "Cannot dereference void pointer.");
        }
        exp->ctype = exp->Exp1->ctype->Ptr;

        return exp;
    }
    // &
    if (token->Type == TOKEN_OP_BAND) {
        NextToken(lexer);
        Expression *exp = NewExp(EXP_ADDR, token);
        exp->Exp1 = parseUnary(lexer);

        if (!IsLvalExp(exp->Exp1)) {
            Error(lexer, token, "Operand must be a lvalue expression.");
        }
        exp->ctype = PtrTo(exp->Exp1->ctype);
        if (exp->Exp1->ty == EXP_VARREF) {
            exp->Exp1->ID->address_taken = 1;
        }

        return exp;
    }
    // ++
    if (token->Type == TOKEN_OP_ADDSELF) {
        NextToken(lexer);
        token->Type = TOKEN_OP_ADD;
        return NewAssignEqual(token, parseUnary(lexer), NewIntExp(1, NULL));
    }
    // --
    if (token->Type == TOKEN_OP_SUBSELF) {
        NextToken(lexer);
        token->Type = TOKEN_OP_SUB;
        return NewAssignEqual(token, parseUnary(lexer), NewIntExp(1, NULL));
    }

    return parsePostPrefix(lexer);
}

Expression *parseMulDivMod(Lexer *lexer) {
    // 3 *, /, %
    Expression *exp = parseUnary(lexer);
    Token *token = PeekToken(lexer);
    while (token->Type == TOKEN_OP_MUL ||
           token->Type == TOKEN_OP_DIV ||
           token->Type == TOKEN_OP_MOD) {
        NextToken(lexer);
        exp = NewBinop(token, exp, parseUnary(lexer));

        // optimize const exp
        if (IsNumExp(exp->Exp1) && IsNumExp(exp->Exp2)) {
            switch (token->Type) {
            case TOKEN_OP_MUL:
                exp = NewIntExp(exp->Exp1->Val * exp->Exp2->Val, NULL);
                break;
            case TOKEN_OP_DIV:
                exp = NewIntExp(exp->Exp1->Val / exp->Exp2->Val, NULL);
                break;
            case TOKEN_OP_MOD:
                exp = NewIntExp(exp->Exp1->Val % exp->Exp2->Val, NULL);
                break;
            }
            goto nextLoop;
        }

        // check type
        if (!IsNumType(exp->Exp1->ctype)) {
            Error(lexer, token, "The left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(lexer, token, "The right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        nextLoop:
        token = PeekToken(lexer);
    }

    return exp;
}

Expression *parseAddSub(Lexer *lexer) {
    // 4 +, -
    Expression *exp = parseMulDivMod(lexer);
    Token *token = PeekToken(lexer);
    while (token->Type == TOKEN_OP_ADD ||
           token->Type == TOKEN_OP_SUB) {
        NextToken(lexer);
        exp = NewBinop(token, exp, parseMulDivMod(lexer));

        // optimize const exp
        if (IsNumExp(exp->Exp1) && IsNumExp(exp->Exp2)) {
            switch (token->Type) {
            case TOKEN_OP_ADD:
                exp = NewIntExp(exp->Exp1->Val + exp->Exp2->Val, NULL);
                break;
            case TOKEN_OP_SUB:
                exp = NewIntExp(exp->Exp1->Val - exp->Exp2->Val, NULL);
                break;
            }
            goto nextLoop;
        }

        // optimize +
        if (token->Type == TOKEN_OP_ADD) {
            int swaped = 0;
            if (exp->Exp2->ctype->ty == PTR) {
                Expression *tmp = exp->Exp2;
                exp->Exp2 = exp->Exp1;
                exp->Exp1 = tmp;
                swaped = 1;
            }

            if (!IsNumType(exp->Exp2->ctype)) {
                ErrorAt(lexer, token->Original,
                      "The %s side of the operator is not a number.",
                      swaped ? "left" : "right");
            }

            if (exp->Exp1->ctype->ty == PTR) {
                exp->Exp2 = scalePtr(exp->Exp2, exp->Exp1->ctype);
                exp->ctype = exp->Exp1->ctype;
            } else {
                exp->ctype = &IntType;
            }
        }

        // optimize -
        if (token->Type == TOKEN_OP_SUB) {
            if (exp->Exp1->ctype->ty == PTR &&
                exp->Exp2->ctype->ty == PTR) {
                if (!IsSameType(exp->Exp1->ctype, exp->Exp2->ctype)) {
                    Error(lexer, token, "Incompatible pointer.");
                } else {
                    exp = scalePtr(exp, exp->Exp1->ctype);
                    exp->ctype = exp->Exp1->ctype;
                }
            } else {
                exp->ctype = &IntType;
            }
        }

        nextLoop:
        token = PeekToken(lexer);
    }

    return exp;
}

Expression *parseShift(Lexer *lexer) {
    // 5 >>, <<
    Expression *exp = parseAddSub(lexer);
    Token *token = PeekToken(lexer);
    while (token->Type == TOKEN_OP_SHL ||
           token->Type == TOKEN_OP_SHR) {
        NextToken(lexer);
        exp = NewBinop(token, exp, parseAddSub(lexer));

        if (IsNumExp(exp->Exp1) && IsNumExp(exp->Exp2)) {
            switch (token->Type) {
            case TOKEN_OP_SHL:
                exp = NewIntExp(exp->Exp1->Val << exp->Exp2->Val, NULL);
                break;
            case TOKEN_OP_SHR:
                exp = NewIntExp(exp->Exp1->Val >> exp->Exp2->Val, NULL);
                break;
            }
            goto nextLoop;
        }

        // check type
        if (!IsNumType(exp->Exp1->ctype)) {
            Error(lexer, token, "The left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(lexer, token, "The right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        nextLoop:
        token = PeekToken(lexer);
    }

    return exp;
}

Expression *parseRelation(Lexer *lexer) {
    // 6 >, >=, <, <=
    Expression *exp = parseShift(lexer);
    Token *token = PeekToken(lexer);
    while (token->Type == TOKEN_OP_GT ||
           token->Type == TOKEN_OP_GE ||
           token->Type == TOKEN_OP_LT ||
           token->Type == TOKEN_OP_LE) {
        NextToken(lexer);
        exp = NewBinop(token, exp, parseShift(lexer));

        // check type
        if (!IsNumType(exp->Exp1->ctype)) {
            Error(lexer, token, "The left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(lexer, token, "The right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        // trans great to less
        if (token->Type == TOKEN_OP_GT ||
            token->Type == TOKEN_OP_GE) {
            Expression *tmp = exp->Exp2;
            exp->Exp2 = exp->Exp1;
            exp->Exp1 = tmp;
            switch (token->Type) {
            case TOKEN_OP_GT:
                exp->Op->Type = TOKEN_OP_LT; break;
            case TOKEN_OP_GE:
                exp->Op->Type = TOKEN_OP_LE; break;
            }
        }

        token = PeekToken(lexer);
    }

    return exp;
}

Expression *parseEqual(Lexer *lexer) {
    // 7 ==, !=
    Expression *exp = parseRelation(lexer);
    Token *token = PeekToken(lexer);
    while (token->Type == TOKEN_OP_EQ ||
           token->Type == TOKEN_OP_NE) {
        NextToken(lexer);
        exp = NewBinop(token, exp, parseRelation(lexer));

        // check type
        if (!IsNumType(exp->Exp1->ctype)) {
            Error(lexer, token, "The left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(lexer, token, "The right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        token = PeekToken(lexer);
    }

    return exp;
}

Expression *parseBitAnd(Lexer *lexer) {
    // 8 &
    Expression *exp = parseEqual(lexer);
    Token *token = PeekToken(lexer);
    while (token->Type == TOKEN_OP_BAND) {
        NextToken(lexer);
        exp = NewBinop(token, exp, parseEqual(lexer));

        // check type
        if (!IsNumType(exp->Exp1->ctype)) {
            Error(lexer, token, "The left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(lexer, token, "The right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        token = PeekToken(lexer);
    }

    return exp;
}

Expression *parseBitXOr(Lexer *lexer) {
    // 9 ^
    Expression *exp = parseBitAnd(lexer);
    Token *token = PeekToken(lexer);
    while (token->Type == TOKEN_OP_BXOR) {
        NextToken(lexer);
        exp = NewBinop(token, exp, parseBitAnd(lexer));

        // check type
        if (!IsNumType(exp->Exp1->ctype)) {
            Error(lexer, token, "The left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(lexer, token, "The right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        token = PeekToken(lexer);
    }

    return exp;
}

Expression *parseBitOr(Lexer *lexer) {
    // 10 |
    Expression *exp = parseBitXOr(lexer);
    Token *token = PeekToken(lexer);
    while (token->Type == TOKEN_OP_BOR) {
        NextToken(lexer);
        exp = NewBinop(token, exp, parseBitXOr(lexer));

        // check type
        if (!IsNumType(exp->Exp1->ctype)) {
            Error(lexer, token, "The left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(lexer, token, "The right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        token = PeekToken(lexer);
    }

    return exp;
}

Expression *parseLogicalAnd(Lexer *lexer) {
    // 11 &&
    Expression *exp1 = parseBitOr(lexer);
    Token *token = PeekToken(lexer);
    if (token->Type != TOKEN_OP_AND) return exp1;
    else {
        Expression *expList = NewExp(EXP_MULTIOP, token);
        expList->Exps = NewVector();
        VectorPush(expList->Exps, exp1);

        // check type
        if (!IsNumType(exp1->ctype)) {
            Error(lexer, token, "The left side of the operator is not a number.");
        }
        expList->ctype = &IntType;

        while (token = ConsumeToken(lexer, TOKEN_OP_AND)) {
            Expression *exp = parseBitOr(lexer);

            // check type
            if (!IsNumType(exp->ctype)) {
                Error(lexer, token, "The right side of the operator is not a number.");
            }

            VectorPush(expList->Exps, exp);
        }
        return expList;
    }
}

Expression *parseLogicalOr(Lexer *lexer) {
    // 12 ||
    Expression *exp1 = parseLogicalAnd(lexer);
    Token *token = PeekToken(lexer);
    if (token->Type != TOKEN_OP_OR) return exp1;
    else {
        Expression *expList = NewExp(EXP_MULTIOP, token);
        expList->Exps = NewVector();
        VectorPush(expList->Exps, exp1);

        // check type
        if (!IsNumType(exp1->ctype)) {
            Error(lexer, token, "The left side of the operator is not a number.");
        }
        expList->ctype = &IntType;

        while (token = ConsumeToken(lexer, TOKEN_OP_OR)) {
            Expression *exp = parseLogicalAnd(lexer);

            // check type
            if (!IsNumType(exp->ctype)) {
                Error(lexer, token, "The right side of the operator is not a number.");
            }

            VectorPush(expList->Exps, exp);
        }
        return expList;
    }
}

Expression *parseConditional(Lexer *lexer) {
    // 13 condition
    Expression *exp = parseLogicalOr(lexer);
    Token *token = PeekToken(lexer);
    if (token->Type != TOKEN_OP_QST) {
        return exp;
    }

    NextToken(lexer);
    Expression *cond = NewExp(EXP_COND, token);
    cond->Cond = exp;
    cond->Exp1 = parseExp(lexer);
    ExpectToken(lexer, TOKEN_SEP_COLON);
    cond->Exp2 = parseExp(lexer);
    cond->ctype = cond->Exp1->ctype;

    return cond;
}

// `x op= y` where x is of type T is compiled as
// `({ T *z = &x; *z = *z op y; })`.
Expression *NewAssignEqual(Token *op, Expression *exp1, Expression *exp2) {
    Vector *v = NewVector();
    Var *var = addLocalVar(PtrTo(exp1->ctype), "tmp");

    // T *z = &x
    Expression *tmp1 = NewExp(EXP_ASSIGN, op);
    tmp1->Exp1 = NewVarref(op, var);
    tmp1->Exp2 = NewAddr(op, exp1);
    tmp1->ctype = tmp1->Exp1->ctype;

    // *z = *z op y
    Expression *tmp2 = NewExp(EXP_ASSIGN, op);
    tmp2->Exp1 = NewDerefVar(op, var);
    op->Type = ChangeOpEqual(op->Type);
    tmp2->Exp2 = NewBinop(op, NewDerefVar(op, var), exp2);
    tmp2->ctype = tmp2->Exp1->ctype;

    VectorPush(v, tmp1);
    VectorPush(v, tmp2);
    return NewStmtExp(op, v);
}

Expression *parseAssign(Lexer *lexer) {
    // 14 assign
    Expression *exp = parseConditional(lexer);
    Token *token = PeekToken(lexer);

    if (token->Type == TOKEN_OP_ASSIGN) {
        NextToken(lexer);

        // check type
        if (!IsLvalExp(exp)) {
            Error(lexer, token, "The left side of the operator is not a lvalue.");
        }

        Expression *tmp = NewExp(EXP_ASSIGN, token);
        tmp->Exp1 = exp;
        tmp->Exp2 = parseAssign(lexer);
        exp = tmp;
        exp->ctype = exp->Exp1->ctype;
    } else if (IsOpEqual(token->Type)) {
        NextToken(lexer);

        // check type
        if (!IsLvalExp(exp)) {
            Error(lexer, token, "The left side of the operator is not a lvalue.");
        }

        Type *ty = exp->ctype;
        exp = NewAssignEqual(token, exp, parseAssign(lexer));
        exp->ctype = ty;
    }
    return exp;
}

Expression *parseExp(Lexer *lexer) {
    // 15 explist
    Expression *exp1 = parseAssign(lexer);
    Token *token = PeekToken(lexer);
    if (token->Type != TOKEN_SEP_COMMA) return exp1;
    else {
        Expression *exp;
        Expression *expList = NewExp(EXP_MULTIOP, token);
        expList->Exps = NewVector();
        VectorPush(expList->Exps, exp1);
        while (ConsumeToken(lexer, TOKEN_SEP_COMMA)){
            exp = parseExp(lexer);
            VectorPush(expList->Exps, exp);
        }
        expList->ctype = exp->ctype;
        return expList;
    }
}

int parseConstExp(Lexer *lexer) {
    Token *token = PeekToken(lexer);
    Expression *exp = parseExp(lexer);
    if (exp->ty != EXP_INT) 
        Error(lexer, token, "constant expression expected.");
    return exp->Val;
}

Type *parseArray(Lexer *lexer, Type *ty) {
    Vector *v = NewVector();

    while (ConsumeToken(lexer, TOKEN_SEP_LBRACK)) {
        if (ConsumeToken(lexer, TOKEN_SEP_RBRACK)) {
            VectorPushInt(v, -1);
            continue;
        }
        VectorPushInt(v, parseConstExp(lexer));
        ExpectToken(lexer, TOKEN_SEP_RBRACK);
    }

    for (int i = v->len - 1; i >= 0; i--) {
        int len = *((int *)(v->data[i]));
        ty = ArrayOf(ty, len);
    }
    return ty;
}

Declaration *DirectDeclaration(Lexer *lexer, Type *ty) {
    Token *token = PeekToken(lexer);
    Declaration *decl;
    Type *placeholder = calloc(1, sizeof(Type));

    switch (token->Type) {
    case TOKEN_IDENTIFIER:
        decl = calloc(1, sizeof(Declaration));
        decl->token = token;
        decl->ty = placeholder;
        decl->Name = token->Literal;
        NextToken(lexer);
        break;
    case TOKEN_SEP_LPAREN:
        decl = Declarator(lexer, placeholder);
        ExpectToken(lexer, TOKEN_SEP_RPAREN);
        break;
    default:
        Error(lexer, token, "bad direct-declarator");
    }

    // Read the second half of type name (e.g. `[3][5]`).
    *placeholder = *parseArray(lexer, ty);

    // Read an initializer.
    if (ConsumeToken(lexer, TOKEN_OP_ASSIGN)) {
        decl->Init = parseAssign(lexer);
    }
    return decl;
}

Declaration *Declarator(Lexer *lexer, Type *ty) {
    while (ConsumeToken(lexer, TOKEN_OP_MUL)) {
        ty = PtrTo(ty);
    }
    return DirectDeclaration(lexer, ty);
}

Type *parseTypename(Lexer *lexer) {
    Token *token = PeekToken(lexer);
    switch (token->Type) {
    case TOKEN_KW_VOID:
        NextToken(lexer);
        return &VoidType;
    case TOKEN_KW_INT:
        NextToken(lexer);
        return &IntType;
    case TOKEN_KW_CHAR:
        NextToken(lexer);
        return &CharType;
    default:
        Error(lexer, token, "Unsupported type.");
    }
}

// Declaration *parseTypeDeclaration(Lexer *lexer) {
//     // Type *ty = parseSpecifier(lexer);
//     Type *ty = parseTypename(lexer);
//     Declaration *decl = Declarator(lexer, ty);
//     ExpectToken(lexer, TOKEN_SEP_SEMI);
//     return decl;
// }

Statement *parseDeclaration(Lexer *lexer) {
    // Type *ty = parseSpecifier();
    Type *ty = parseTypename(lexer);
    Declaration *decl = Declarator(lexer, ty);
    ExpectToken(lexer, TOKEN_SEP_SEMI);
    Var *var = addLocalVar(decl->ty, decl->Name);

    if (!decl->Init) return &NullStmt;

    // Convert `T var = init` to `T var; var = init`.
    Expression *exp = NewExp(EXP_ASSIGN, NULL);
    exp->Exp1 = NewVarref(NULL, var);
    exp->Exp2 = decl->Init;
    exp->ctype = exp->Exp1->ctype;
    decl->Init = NULL;
    Statement *stmt = NewExpStmt(NULL, exp);
    
    return stmt;
}

Var *parseParamDeclaration(Lexer *lexer) {
    // Type *ty = parseSpecifier(lexer);
    Type *ty = parseTypename(lexer);
    Declaration *decl = Declarator(lexer, ty);
    ty = decl->ty;
    if (ty->ty == ARRAY) ty = PtrTo(ty->ArrPtr);
    return addLocalVar(ty, decl->Name);
}

// Type *parseSpecifer(Lexer *lexer) {
//     Token *token = PeekToken(lexer);
//     switch (token->Type) {
//     // 'typedef' unsupported.
//     // case TOKEN_IDENTIFIER:
//     //     return getTypedef(env, token->Literal);
//     case TOKEN_KW_VOID:
//         return &VoidType;
//     case TOKEN_KW_INT:
//         return &IntType;
//     case TOKEN_KW_CHAR:
//         return &CharType;
//     // 'typeof' unsupported.
//     // case TOKEN_KW_TYPEOF:
//     //     MustNextTokenOfType(lexer, TOKEN_SEP_LPAREN);
//     //     Expression *exp = parseExpression(lexer);
//     //     MustNextTokenOfType(lexer, TOKEN_SEP_RPAREN);
//     //     return getTypeOfExp(exp);
//     // 'struct' unsupported.
//     // case TOKEN_KW_STRUCT:
//     //     NextToken(lexer);
//     //     token = PeekToken(lexer);
//     //     Type *ty = NULL;
//     //     char *tag = NULL;

//     //     if (token->Type == TOKEN_IDENTIFIER) {
//     //         tag = token->Literal;
//     //         ty = getTag(env, tag);
//     //         token = NextToken(lexer);
//     //     }

//     //     if (!ty) {
//     //         ty = calloc(1, sizeof(Type));
//     //         ty->ty = STRUCT;
//     //     }

//     //     if (token->Type == TOKEN_SEP_LCURLY) {
//     //         ty->Members = NewMap();
//     //         while (!NextTokenOfType(lexer, TOKEN_SEP_RCURLY)) {
//     //             Declaration *decl = parseTypeDeclaration(lexer);
//     //             MapPut(ty->Members, decl->Name, ty->ty);
//     //         }
//     //     }
//     default:
//         Error(lexer, "unsupported type.");
//     }
// }

Statement *parseStmt(Lexer *lexer) {
    Token *token = PeekToken(lexer);

    switch (token->Type) {
    // unsupported yet.
    // case TOKEN_KW_TYPEDEF:
    case TOKEN_KW_IF: {
        NextToken(lexer);
        Statement *stmt = NewStmt(STMT_IF);
        ExpectToken(lexer, TOKEN_SEP_LPAREN);
        stmt->Cond = parseExp(lexer);
        ExpectToken(lexer, TOKEN_SEP_RPAREN);
        stmt->Body = parseStmt(lexer);
        if (ConsumeToken(lexer, TOKEN_KW_ELSE)) {
            stmt->Else = parseStmt(lexer);
        }
        return stmt;
    }
    case TOKEN_KW_FOR: {
        NextToken(lexer);
        Statement *stmt = NewStmt(STMT_FOR);
        env = newEnv(env);
        VectorPush(breaks, stmt);
        VectorPush(continues, stmt);

        ExpectToken(lexer, TOKEN_SEP_LPAREN);

        if (!ConsumeToken(lexer, TOKEN_SEP_SEMI)) {
            Expression *exp = parseExp(lexer);
            stmt->Init = exp;
            ExpectToken(lexer, TOKEN_SEP_SEMI);
        }

        if (!ConsumeToken(lexer, TOKEN_SEP_SEMI)) {
            Expression *exp = parseExp(lexer);
            stmt->Cond = exp;
            ExpectToken(lexer, TOKEN_SEP_SEMI);
        }

        if (!ConsumeToken(lexer, TOKEN_SEP_RPAREN)) {
            Expression *exp = parseExp(lexer);
            stmt->Step = exp;
            ExpectToken(lexer, TOKEN_SEP_RPAREN);
        }

        stmt->Body = parseStmt(lexer);
        VectorPop(breaks);
        VectorPop(continues);
        env = env->prev;
        return stmt;
    }
    case TOKEN_KW_WHILE: {
        NextToken(lexer);
        Statement *stmt = NewStmt(STMT_FOR);
        VectorPush(breaks, stmt);
        VectorPush(continues, stmt);

        ExpectToken(lexer, TOKEN_SEP_LPAREN);
        Expression *exp = parseExp(lexer);
        stmt->Cond = exp;
        ExpectToken(lexer, TOKEN_SEP_RPAREN);

        stmt->Body = parseStmt(lexer);
        VectorPop(breaks);
        VectorPop(continues);
        return stmt;
    }
    case TOKEN_KW_DO: {
        NextToken(lexer);
        Statement *stmt = NewStmt(STMT_DO_WHILE);
        VectorPush(breaks, stmt);
        VectorPush(continues, stmt);

        stmt->Body = parseStmt(lexer);
        ExpectToken(lexer, TOKEN_KW_WHILE);
        ExpectToken(lexer, TOKEN_SEP_LPAREN);
        Expression *exp = parseExp(lexer);
        stmt->Cond = exp;
        ExpectToken(lexer, TOKEN_SEP_RPAREN);
        ExpectToken(lexer, TOKEN_SEP_SEMI);

        VectorPop(breaks);
        VectorPop(continues);
        return stmt;
    }
    case TOKEN_KW_SWITCH: {
        NextToken(lexer);
        Statement *stmt = NewStmt(STMT_SWITCH);
        stmt->Cases = NewVector();
        VectorPush(breaks, stmt);
        VectorPush(switches, stmt);

        ExpectToken(lexer, TOKEN_SEP_LPAREN);
        Expression *exp = parseExp(lexer);
        stmt->Cond = exp;
        ExpectToken(lexer, TOKEN_SEP_RPAREN);

        stmt->Body = parseStmt(lexer);
        VectorPop(breaks);
        VectorPop(switches);
        return stmt;
    }
    case TOKEN_KW_CASE: {
        NextToken(lexer);
        if (VectorSize(switches) == 0) Error(lexer, token, "stray case.");
        Statement *stmt = NewStmt(STMT_CASE);
        Expression *exp = NewExp(EXP_INT, NULL);
        exp->Val = parseConstExp(lexer);
        stmt->Cond = exp;
        ExpectToken(lexer, TOKEN_SEP_COLON);
        stmt->Body = parseStmt(lexer);
        
        Statement *father = VectorLast(switches);
        VectorPush(father->Cases, stmt);
        return stmt;
    }
    case TOKEN_KW_BREAK: {
        NextToken(lexer);
        if (VectorSize(breaks) == 0) Error(lexer, token, "stray break.");
        Statement *stmt = NewStmt(STMT_BREAK);
        stmt->Body = VectorLast(breaks);
        return stmt;
    }
    case TOKEN_KW_CONTINUE: {
        NextToken(lexer);
        if (VectorSize(continues) == 0) Error(lexer, token, "stray continue.");
        Statement *stmt = NewStmt(STMT_CONTINUE);
        stmt->Body = VectorLast(continues);
        return stmt;
    }
    case TOKEN_KW_RETURN: {
        NextToken(lexer);
        Statement *stmt = NewStmt(STMT_RETURN);
        stmt->Exp = parseExp(lexer);
        ExpectToken(lexer, TOKEN_SEP_SEMI);
        return stmt;
    }
    case TOKEN_SEP_LCURLY: {
        NextToken(lexer);
        return parseCompoundStmt(lexer);
    }
    case TOKEN_SEP_SEMI: {
        NextToken(lexer);
        return &NullStmt;
    }
    default: 
        if (IsTypename(token->Type)) {
            return parseDeclaration(lexer);
        } else {
            Statement *stmt = NewExpStmt(token, parseExp(lexer));
            ExpectToken(lexer, TOKEN_SEP_SEMI);
            return stmt;
        }
    }
}

Statement *parseCompoundStmt(Lexer *lexer) {
    Statement *stmt = NewStmt(STMT_COMP);
    stmt->Stmts = NewVector();
    env = newEnv(env);
    while (!ConsumeToken(lexer, TOKEN_SEP_RCURLY)) {
        VectorPush(stmt->Stmts, parseStmt(lexer));
    }
    env = env->prev;
    return stmt;
}

void parseTopLevel(Lexer *lexer) {
    // Token *_typedef = NextTokenOfType(lexer, TOKEN_KW_TYPEDEF);
    // Token *_extern = NextTokenOfType(lexer, TOKEN_KW_EXTERN);

    // Type *ty = parseSpecifer(lexer);
    Type *ty = parseTypename(lexer);
    while (ConsumeToken(lexer, TOKEN_OP_MUL)) ty = PtrTo(ty);
    Token *ident = ExpectToken(lexer, TOKEN_IDENTIFIER);

    // Function
    if (ConsumeToken(lexer, TOKEN_SEP_LPAREN)) {
        lvars = NewVector();
        breaks = NewVector();
        continues = NewVector();
        switches = NewVector();

        Vector *v = NewVector();
        while (!ConsumeToken(lexer, TOKEN_SEP_RPAREN)) {
            if (VectorSize(v) > 0) ExpectToken(lexer, TOKEN_SEP_COMMA);
            VectorPush(v, parseParamDeclaration(lexer));
        }
        Token *token = PeekToken(lexer);

        // define func type
        Type *func = calloc(1, sizeof(Type));
        func->ty = FUNC;
        func->Returning = ty;

        addLocalVar(func, ident->Literal);

        if (ConsumeToken(lexer, TOKEN_SEP_SEMI)) return;

        // function body
        ExpectToken(lexer, TOKEN_SEP_LCURLY);
        Function *fn = calloc(1, sizeof(Function));
        fn->Stmt = parseCompoundStmt(lexer);
        fn->Name = ident->Literal;
        fn->Params = v;
        fn->LocalVars = lvars;
        fn->bbs = NewVector();
        VectorPush(program->Functions, fn);
        return;
    }

    ty = parseArray(lexer, ty);
    ExpectToken(lexer, TOKEN_SEP_SEMI);

    // global variable
    addGlobalVar(program, ty, ident->Literal, NULL, 0);
}

Program *ParseProgram(Lexer *lexer) {
    program = calloc(1, sizeof(Program));
    program->macros = NewMap();
    program->GlobalVars = NewVector();
    program->Functions = NewVector();
    env = newEnv(NULL);

    while (PeekToken(lexer)->Type != TOKEN_EOF) {
        parseTopLevel(lexer);
    }
    return program;
}