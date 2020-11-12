#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "util.h"
#include "parser.h"
#include "token.h"

int nLabel = 1;

// declare forward.
Statement *parseStmt(Parser *parser);
Statement *parseCompoundStmt(Parser *parser);
Expression *parseAssign(Parser *parser);
Expression *parseExp(Parser *parser);
Expression *NewAssignEqual(Parser *parser, Token *op, Expression *exp1, Expression *exp2);
Declaration *Declarator(Parser *parser, Type *ty);

static void ErrorAt(Lexer *lexer, char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    char *start = loc, *end = loc;
    while (start > lexer->chunk && *(start - 1) != '\n') start--;
    while (*end != '\0' && *end != '\n') end++;
    if (*end != '\0') *end = '\0';
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

Parser *NewParser(Lexer *lexer) {
    Parser *parser = calloc(1, sizeof(Parser));
    parser->env = newEnv(NULL);
    parser->lexer = lexer;
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

Var *addLocalVar(Parser *parser, Type *ty, char *name) {
    Var *var = calloc(1, sizeof(Var));
    var->ty = ty;
    var->Local = 1;
    var->Name = name;
    MapPut(parser->env->vars, name, var);
    VectorPush(parser->LocalVars, var);
    return var;
}

Var *addGlobalVar(Parser *parser, Type *ty, char *name, char *data, int _extern) {
    Var *var = calloc(1, sizeof(Var));
    var->ty = ty;
    var->Local = 0;
    var->Name = name;
    var->Data = data;
    MapPut(parser->env->vars, name, var);
    if (!_extern) VectorPush(parser->program->GlobalVars, var);
    return var;
}

Expression *NewStringExp(Parser *parser, char *s) {
    Type *ty = ArrayOf(&CharType, strlen(s)+1);
    char *name = Format(".L.str%d", nLabel++);
    Var *var = addGlobalVar(parser, ty, name, s, 0);
    Expression *exp = NewVarref(NULL, var);

    if (exp->ctype->ty == ARRAY) {
        Expression *tmp = NewAddr(NULL, exp);
        tmp->ctype = PtrTo(exp->ctype->ArrPtr);
        exp = tmp;
    }
    return exp;
}

Expression *scalePtr(Expression *exp, Type *ty) {
    if (ty->Size == 1) {
        return exp;
    }
    Token *token = calloc(1, sizeof(Token));
    token->Line = exp->Op->Line;
    token->Literal = exp->Op->Literal;
    token->Type = TOKEN_OP_MUL;
    return NewBinop(token, exp, NewIntExp(ty->Size, token));
}

Expression *parseLocalVar(Parser *parser, Token *token) {
    Var *var = getVar(parser->env, token->Literal);
    if (!var) Error(parser->lexer, token, "undefined variable");
    Expression *exp = NewVarref(token, var);
    exp->Name = token->Literal;
    return exp;
}

Expression *parseFuncCall(Parser *parser, Token *token) {
    Var *var = getVar(parser->env, token->Literal);

    Expression *exp = NewExp(EXP_FUNCCALL, token);
    exp->Name = token->Literal;
    exp->ID = var;
    exp->Exps = NewVector();

    if (var && var->ty->ty == FUNC) {
        exp->ctype = var->ty;
    } else {
        Error(parser->lexer, token, "undefined function");
        exp->ctype = NewFuncType(&IntType);
    }

    while (!ConsumeToken(parser->lexer, TOKEN_SEP_RPAREN)) {
        if (VectorSize(exp->Exps) > 0) {
            ExpectToken(parser->lexer, TOKEN_SEP_COMMA);
        }
        VectorPush(exp->Exps, parseAssign(parser));
    }

    exp->ctype = exp->ID->ty->Returning;
    return exp;
}

Expression *parseStmtExp(Parser *parser) {
    Token *token = PeekToken(parser->lexer);
    Vector *stmts = NewVector();

    parser->env = newEnv(parser->env);
    Token *endToken = NULL;
    do {
        VectorPush(stmts, parseStmt(parser));
        endToken = ConsumeToken(parser->lexer, TOKEN_SEP_RCURLY);
    } while (!endToken);
    parser->env = parser->env->prev;

    Statement *last = VectorPop(stmts);
    if (last->ty != STMT_EXP) {
        Error(parser->lexer, endToken, "statement expression returning void");
    }

    Expression *exp = NewExp(EXP_STMT, token);
    exp->Stmts = stmts;
    exp->Exp1 = last->Exp;
    exp->ctype = exp->Exp1->ctype;
    return exp;
}

Expression *parsePrimary(Parser *parser) {
    // 0 primary
    Token *token = PeekToken(parser->lexer);

    if (token->Type == TOKEN_SEP_LPAREN) {
        NextToken(parser->lexer);
        if (ConsumeToken(parser->lexer, TOKEN_SEP_LCURLY)) {
            return parseStmtExp(parser);
        }
        Expression *exp = parseExp(parser);
        ExpectToken(parser->lexer, TOKEN_SEP_RPAREN);
        return exp;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
    if (token->Type == TOKEN_NUMBER) {
        NextToken(parser->lexer);
        return NewIntExp((int)token->Literal, token);
    }
#pragma GCC diagnostic pop

    if (token->Type == TOKEN_CHAR) {
        NextToken(parser->lexer);
        return NewCharExp(*(token->Literal), token);
    }

    if (token->Type == TOKEN_STRING) {
        NextToken(parser->lexer);
        return NewStringExp(parser, token->Literal);
    }

    if (token->Type == TOKEN_IDENTIFIER) {
        NextToken(parser->lexer);
        if (ConsumeToken(parser->lexer, TOKEN_SEP_LPAREN)) {
            return parseFuncCall(parser, token);
        }
        return parseLocalVar(parser, token);
    }
 
    Error(parser->lexer, token, "primary expression expected.");
}

// `x++` where x is of type T is compiled as
// `({ T *y = &x; T z = *y; *y = *y + 1; z; })`.
Expression *NewPostIncrease(Parser *parser, Token *token, Expression *exp, int imm) {
    Vector *v = NewVector();

    Var *var1 = addLocalVar(parser, PtrTo(exp->ctype), "tmp1");
    Var *var2 = addLocalVar(parser, exp->ctype, "tmp2");

    Expression *exp1 = NewExp(EXP_ASSIGN, token);
    exp1->Exp1 = NewVarref(token, var1);
    exp1->Exp2 = NewAddr(token, exp);
    exp1->ctype = exp1->Exp1->ctype;

    Expression *exp2 = NewExp(EXP_ASSIGN, token);
    exp2->Exp1 = NewVarref(token, var2);
    exp2->Exp2 = NewDerefVar(token, var1);
    exp2->ctype = exp2->Exp1->ctype;

    Expression *exp3 = NewExp(EXP_ASSIGN, token);
    exp3->Exp1 = NewDerefVar(token, var1);
    token->Type = TOKEN_OP_ADD;
    exp3->Exp2 = NewBinop(token, NewDerefVar(token, var1), NewIntExp(imm, token));
    exp3->ctype = exp3->Exp1->ctype;

    Expression *exp4 = NewVarref(token, var2);

    VectorPush(v, exp1);
    VectorPush(v, exp2);
    VectorPush(v, exp3);
    VectorPush(v, exp4);

    return NewStmtExp(token, v);
}

Expression *parsePostPrefix(Parser *parser) {
    // 1 x++, x--
    Expression *exp = parsePrimary(parser);
    while (1) {
        Token *token = PeekToken(parser->lexer);
        if (token->Type == TOKEN_OP_ADDSELF) {
            NextToken(parser->lexer);
            exp = NewPostIncrease(parser, token, exp, 1);
            continue;
        }

        if (token->Type == TOKEN_OP_SUBSELF) {
            NextToken(parser->lexer);
            exp = NewPostIncrease(parser, token, exp, -1);
            continue;
        }

        if (token->Type == TOKEN_SEP_DOT) {
            NextToken(parser->lexer);
            exp = NewAccess(token, exp);
            exp->Name = ExpectToken(parser->lexer, TOKEN_IDENTIFIER)->Literal;
            continue;
        }

        if (token->Type == TOKEN_OP_ARROW) {
            NextToken(parser->lexer);
            Expression *tmp = NewDeref(token, exp);

            exp = NewAccess(token, tmp);
            exp->Name = ExpectToken(parser->lexer, TOKEN_IDENTIFIER)->Literal;
            continue;
        }

        if (token->Type == TOKEN_SEP_LBRACK) {
            NextToken(parser->lexer);
            token->Type = TOKEN_OP_ADD;
            Expression *idx = scalePtr(parseAssign(parser), exp->ctype->Ptr);
            Expression *tmp = NewBinop(token, exp, idx);
            tmp->ctype = tmp->Exp1->ctype;
            exp = NewDeref(token, tmp); 
            ExpectToken(parser->lexer, TOKEN_SEP_RBRACK);
            continue;
        }

        return exp;
    }
}

Expression *parseUnary(Parser *parser) {
    // 2 -, !, ~, *, &, ++x, --x
    Token *token = PeekToken(parser->lexer);
    // -
    if (token->Type == TOKEN_OP_SUB) {
        NextToken(parser->lexer);
        Expression *exp = parseUnary(parser);

        //optimize const exp
        if (IsNumExp(exp)) {
            exp->Val = -exp->Val;
        } else {
            exp = NewBinop(token, NewIntExp(0, NULL), exp);
            if (!IsNumType(exp->Exp2->ctype)) {
                Error(parser->lexer, token,
                    "the right side of the operator is not a number.");
            }
            exp->ctype = &IntType;
        }
        return exp;
    }
    // !, ~
    if (token->Type == TOKEN_OP_NOT ||
        token->Type == TOKEN_OP_BNOT) {
        NextToken(parser->lexer);
        Expression *exp = parseUnary(parser);

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
                Error(parser->lexer, token,
                    "the right side of the operator is not a number.");
            }
            exp->ctype = &IntType;
        }
        return exp;
    }
    // *
    if (token->Type == TOKEN_OP_MUL) {
        NextToken(parser->lexer);
        Expression *exp = NewDeref(token, parseUnary(parser));

        if (exp->Exp1->ctype->ty != PTR) {
            Error(parser->lexer, token, "operand must be a pointer.");
        }

        if (exp->Exp1->ctype->Ptr->ty == VOID) {
            Error(parser->lexer, token, "cannot dereference void pointer.");
        }
        exp->ctype = exp->Exp1->ctype->Ptr;

        return exp;
    }
    // &
    if (token->Type == TOKEN_OP_BAND) {
        NextToken(parser->lexer);
        Expression *exp = NewAddr(token, parseUnary(parser));

        if (!IsLvalExp(exp->Exp1)) {
            Error(parser->lexer, token, "operand must be a lvalue expression.");
        }
        exp->ctype = PtrTo(exp->Exp1->ctype);
        if (exp->Exp1->ty == EXP_VARREF) {
            exp->Exp1->ID->AddressTaken = 1;
        }

        return exp;
    }
    // ++
    if (token->Type == TOKEN_OP_ADDSELF) {
        NextToken(parser->lexer);
        return NewAssignEqual(parser, token, parseUnary(parser), NewIntExp(1, token));
    }
    // --
    if (token->Type == TOKEN_OP_SUBSELF) {
        NextToken(parser->lexer);
        return NewAssignEqual(parser, token, parseUnary(parser), NewIntExp(1, token));
    }

    return parsePostPrefix(parser);
}

Expression *parseMulDivMod(Parser *parser) {
    // 3 *, /, %
    Expression *exp = parseUnary(parser);
    Token *token = PeekToken(parser->lexer);
    while (token->Type == TOKEN_OP_MUL ||
           token->Type == TOKEN_OP_DIV ||
           token->Type == TOKEN_OP_MOD) {
        NextToken(parser->lexer);
        exp = NewBinop(token, exp, parseUnary(parser));

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
            Error(parser->lexer, token,
                "the left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(parser->lexer, token,
                "the right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        nextLoop:
        token = PeekToken(parser->lexer);
    }

    return exp;
}

Expression *parseAddSub(Parser *parser) {
    // 4 +, -
    Expression *exp = parseMulDivMod(parser);
    Token *token = PeekToken(parser->lexer);
    while (token->Type == TOKEN_OP_ADD ||
           token->Type == TOKEN_OP_SUB) {
        NextToken(parser->lexer);
        exp = NewBinop(token, exp, parseMulDivMod(parser));

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
                ErrorAt(parser->lexer, token->Original,
                      "the %s side of the operator is not a number.",
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
                    Error(parser->lexer, token, "incompatible pointer.");
                } else {
                    exp = scalePtr(exp, exp->Exp1->ctype);
                    exp->ctype = exp->Exp1->ctype;
                }
            } else {
                exp->ctype = &IntType;
            }
        }

        nextLoop:
        token = PeekToken(parser->lexer);
    }

    return exp;
}

Expression *parseShift(Parser *parser) {
    // 5 >>, <<
    Expression *exp = parseAddSub(parser);
    Token *token = PeekToken(parser->lexer);
    while (token->Type == TOKEN_OP_SHL ||
           token->Type == TOKEN_OP_SHR) {
        NextToken(parser->lexer);
        exp = NewBinop(token, exp, parseAddSub(parser));

        // optimize
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
            Error(parser->lexer, token,
                "the left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(parser->lexer, token,
                "the right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        nextLoop:
        token = PeekToken(parser->lexer);
    }

    return exp;
}

Expression *parseRelation(Parser *parser) {
    // 6 >, >=, <, <=
    Expression *exp = parseShift(parser);
    Token *token = PeekToken(parser->lexer);
    while (token->Type == TOKEN_OP_GT ||
           token->Type == TOKEN_OP_GE ||
           token->Type == TOKEN_OP_LT ||
           token->Type == TOKEN_OP_LE) {
        NextToken(parser->lexer);
        exp = NewBinop(token, exp, parseShift(parser));

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

        // optimize
        if (IsNumExp(exp->Exp1) && IsNumExp(exp->Exp2)) {
            switch (token->Type) {
            case TOKEN_OP_LT:
                exp = NewIntExp(exp->Exp1->Val < exp->Exp2->Val, NULL);
                break;
            case TOKEN_OP_LE:
                exp = NewIntExp(exp->Exp1->Val > exp->Exp2->Val, NULL);
                break;
            }
            goto nextLoop;
        }

        // check type
        if (!IsNumType(exp->Exp1->ctype)) {
            Error(parser->lexer, token,
                "the left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(parser->lexer, token,
                "the right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        nextLoop:
        token = PeekToken(parser->lexer);
    }

    return exp;
}

Expression *parseEqual(Parser *parser) {
    // 7 ==, !=
    Expression *exp = parseRelation(parser);
    Token *token = PeekToken(parser->lexer);
    while (token->Type == TOKEN_OP_EQ ||
           token->Type == TOKEN_OP_NE) {
        NextToken(parser->lexer);
        exp = NewBinop(token, exp, parseRelation(parser));

        // optimize
        if (IsNumExp(exp->Exp1) && IsNumExp(exp->Exp2)) {
            switch (token->Type) {
            case TOKEN_OP_EQ:
                exp = NewIntExp(exp->Exp1->Val == exp->Exp2->Val, NULL);
                break;
            case TOKEN_OP_NE:
                exp = NewIntExp(exp->Exp1->Val != exp->Exp2->Val, NULL);
                break;
            }
            goto nextLoop;
        }

        // check type
        if (!IsNumType(exp->Exp1->ctype)) {
            Error(parser->lexer, token,
                "the left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(parser->lexer, token,
                "the right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        nextLoop:
        token = PeekToken(parser->lexer);
    }

    return exp;
}

Expression *parseBitAnd(Parser *parser) {
    // 8 &
    Expression *exp = parseEqual(parser);
    Token *token = PeekToken(parser->lexer);
    while (token->Type == TOKEN_OP_BAND) {
        NextToken(parser->lexer);
        exp = NewBinop(token, exp, parseEqual(parser));

        // optimize
        if (IsNumExp(exp->Exp1) && IsNumExp(exp->Exp2)) {
            exp = NewIntExp(exp->Exp1->Val & exp->Exp2->Val, NULL);
            goto nextLoop;
        }

        // check type
        if (!IsNumType(exp->Exp1->ctype)) {
            Error(parser->lexer, token,
                "the left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(parser->lexer, token,
                "the right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        nextLoop:
        token = PeekToken(parser->lexer);
    }

    return exp;
}

Expression *parseBitXOr(Parser *parser) {
    // 9 ^
    Expression *exp = parseBitAnd(parser);
    Token *token = PeekToken(parser->lexer);
    while (token->Type == TOKEN_OP_BXOR) {
        NextToken(parser->lexer);
        exp = NewBinop(token, exp, parseBitAnd(parser));

        // optimize
        if (IsNumExp(exp->Exp1) && IsNumExp(exp->Exp2)) {
            exp = NewIntExp(exp->Exp1->Val ^ exp->Exp2->Val, NULL);
            goto nextLoop;
        }

        // check type
        if (!IsNumType(exp->Exp1->ctype)) {
            Error(parser->lexer, token,
                "the left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(parser->lexer, token,
                "the right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        nextLoop:
        token = PeekToken(parser->lexer);
    }

    return exp;
}

Expression *parseBitOr(Parser *parser) {
    // 10 |
    Expression *exp = parseBitXOr(parser);
    Token *token = PeekToken(parser->lexer);
    while (token->Type == TOKEN_OP_BOR) {
        NextToken(parser->lexer);
        exp = NewBinop(token, exp, parseBitXOr(parser));

        // optimize
        if (IsNumExp(exp->Exp1) && IsNumExp(exp->Exp2)) {
            exp = NewIntExp(exp->Exp1->Val | exp->Exp2->Val, NULL);
            goto nextLoop;
        }

        // check type
        if (!IsNumType(exp->Exp1->ctype)) {
            Error(parser->lexer, token,
                "the left side of the operator is not a number.");
        }
        if (!IsNumType(exp->Exp2->ctype)) {
            Error(parser->lexer, token,
                "the right side of the operator is not a number.");
        }
        exp->ctype = &IntType;

        nextLoop:
        token = PeekToken(parser->lexer);
    }

    return exp;
}

Expression *parseLogicalAnd(Parser *parser) {
    // 11 &&
    Expression *exp1 = parseBitOr(parser);
    Token *token = PeekToken(parser->lexer);
    if (token->Type != TOKEN_OP_AND) return exp1;
    else {
        Expression *expList = NewExp(EXP_MULTIOP, token);
        expList->Exps = NewVector();
        VectorPush(expList->Exps, exp1);

        // check type
        if (!IsNumType(exp1->ctype)) {
            Error(parser->lexer, token,
                "the left side of the operator is not a number.");
        }
        expList->ctype = &IntType;

        while (token = ConsumeToken(parser->lexer, TOKEN_OP_AND)) {
            Expression *exp = parseBitOr(parser);

            // check type
            if (!IsNumType(exp->ctype)) {
                Error(parser->lexer, token,
                    "the right side of the operator is not a number.");
            }

            VectorPush(expList->Exps, exp);
        }
        return expList;
    }
}

Expression *parseLogicalOr(Parser *parser) {
    // 12 ||
    Expression *exp1 = parseLogicalAnd(parser);
    Token *token = PeekToken(parser->lexer);
    if (token->Type != TOKEN_OP_OR) return exp1;
    else {
        Expression *expList = NewExp(EXP_MULTIOP, token);
        expList->Exps = NewVector();
        VectorPush(expList->Exps, exp1);

        // check type
        if (!IsNumType(exp1->ctype)) {
            Error(parser->lexer, token,
                "the left side of the operator is not a number.");
        }
        expList->ctype = &IntType;

        while (token = ConsumeToken(parser->lexer, TOKEN_OP_OR)) {
            Expression *exp = parseLogicalAnd(parser);

            // check type
            if (!IsNumType(exp->ctype)) {
                Error(parser->lexer, token,
                    "the right side of the operator is not a number.");
            }

            VectorPush(expList->Exps, exp);
        }
        return expList;
    }
}

Expression *parseConditional(Parser *parser) {
    // 13 condition
    Expression *exp = parseLogicalOr(parser);
    Token *token = PeekToken(parser->lexer);
    if (token->Type != TOKEN_OP_QST) {
        return exp;
    }

    NextToken(parser->lexer);
    Expression *cond = NewExp(EXP_COND, token);
    cond->Cond = exp;
    cond->Exp1 = parseExp(parser);
    ExpectToken(parser->lexer, TOKEN_SEP_COLON);
    cond->Exp2 = parseExp(parser);
    cond->ctype = cond->Exp1->ctype;

    return cond;
}

// `x op= y` where x is of type T is compiled as
// `({ T *z = &x; *z = *z op y; })`.
Expression *NewAssignEqual(Parser *parser, Token *op, Expression *exp1, Expression *exp2) {
    Vector *v = NewVector();
    Var *var = addLocalVar(parser, PtrTo(exp1->ctype), "tmp");

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

Expression *parseAssign(Parser *parser) {
    // 14 assign
    Expression *exp = parseConditional(parser);
    Token *token = PeekToken(parser->lexer);

    if (token->Type == TOKEN_OP_ASSIGN) {
        NextToken(parser->lexer);

        // check type
        if (!IsLvalExp(exp)) {
            Error(parser->lexer, token,
                "the left side of the operator is not a lvalue.");
        }

        Expression *tmp = NewExp(EXP_ASSIGN, token);
        tmp->Exp1 = exp;
        tmp->Exp2 = parseAssign(parser);
        exp = tmp;
        exp->ctype = exp->Exp1->ctype;
    } else if (IsOpEqual(token->Type)) {
        NextToken(parser->lexer);

        // check type
        if (!IsLvalExp(exp)) {
            Error(parser->lexer, token,
                "the left side of the operator is not a lvalue.");
        }

        Type *ty = exp->ctype;
        exp = NewAssignEqual(parser, token, exp, parseAssign(parser));
        exp->ctype = ty;
    }
    return exp;
}

Expression *parseExp(Parser *parser) {
    // 15 explist
    Expression *exp1 = parseAssign(parser);
    Token *token = PeekToken(parser->lexer);
    if (token->Type != TOKEN_SEP_COMMA) return exp1;
    else {
        Expression *exp;
        Expression *expList = NewExp(EXP_MULTIOP, token);
        expList->Exps = NewVector();
        VectorPush(expList->Exps, exp1);
        while (ConsumeToken(parser->lexer, TOKEN_SEP_COMMA)){
            exp = parseExp(parser);
            VectorPush(expList->Exps, exp);
        }
        expList->ctype = exp->ctype;
        return expList;
    }
}

int parseConstExp(Parser *parser) {
    Token *token = PeekToken(parser->lexer);
    Expression *exp = parseExp(parser);
    if (exp->ty != EXP_INT) 
        Error(parser->lexer, token, "constant expression expected.");
    return exp->Val;
}

Type *parseArray(Parser *parser, Type *ty) {
    Vector *v = NewVector();

    while (ConsumeToken(parser->lexer, TOKEN_SEP_LBRACK)) {
        if (ConsumeToken(parser->lexer, TOKEN_SEP_RBRACK)) {
            VectorPushInt(v, -1);
            continue;
        }
        VectorPushInt(v, parseConstExp(parser));
        ExpectToken(parser->lexer, TOKEN_SEP_RBRACK);
    }

    for (int i = VectorSize(v) - 1; i >= 0; i--) {
        int len = VectorGetInt(v, i);
        ty = ArrayOf(ty, len);
    }
    return ty;
}

Declaration *DirectDeclaration(Parser *parser, Type *ty) {
    Token *token = PeekToken(parser->lexer);
    Declaration *decl;
    Type *placeholder = calloc(1, sizeof(Type));

    switch (token->Type) {
    case TOKEN_IDENTIFIER:
        decl = calloc(1, sizeof(Declaration));
        decl->token = token;
        decl->ty = placeholder;
        decl->Name = token->Literal;
        NextToken(parser->lexer);
        break;
    case TOKEN_SEP_LPAREN:
        decl = Declarator(parser, placeholder);
        ExpectToken(parser->lexer, TOKEN_SEP_RPAREN);
        break;
    default:
        Error(parser->lexer, token, "bad direct-declarator");
    }

    // Read the second half of type name (e.g. `[3][5]`).
    *placeholder = *parseArray(parser, ty);

    // Read an initializer.
    if (ConsumeToken(parser->lexer, TOKEN_OP_ASSIGN)) {
        decl->Init = parseAssign(parser);
    }
    return decl;
}

Declaration *Declarator(Parser *parser, Type *ty) {
    while (ConsumeToken(parser->lexer, TOKEN_OP_MUL)) {
        ty = PtrTo(ty);
    }
    return DirectDeclaration(parser, ty);
}

Type *parseTypename(Parser *parser) {
    Token *token = PeekToken(parser->lexer);
    switch (token->Type) {
    case TOKEN_KW_VOID:
        NextToken(parser->lexer);
        return &VoidType;
    case TOKEN_KW_INT:
        NextToken(parser->lexer);
        return &IntType;
    case TOKEN_KW_CHAR:
        NextToken(parser->lexer);
        return &CharType;
    default:
        Error(parser->lexer, token, "unsupported type.");
    }
}

// Declaration *parseTypeDeclaration(Parser *parser) {
//     // Type *ty = parseSpecifier(parser->lexer);
//     Type *ty = parseTypename(parser);
//     Declaration *decl = Declarator(parser, ty);
//     ExpectToken(parser->lexer, TOKEN_SEP_SEMI);
//     return decl;
// }

Expression *parseDeclaration(Parser *parser) {
    // Type *ty = parseSpecifier();
    Type *ty = parseTypename(parser);
    Declaration *decl = Declarator(parser, ty);
    Var *var = addLocalVar(parser, decl->ty, decl->Name);

    if (!decl->Init) return NULL;

    // Convert `T var = init` to `T var; var = init`.
    Expression *exp = NewExp(EXP_ASSIGN, NULL);
    exp->Exp1 = NewVarref(NULL, var);
    exp->Exp2 = decl->Init;
    exp->ctype = exp->Exp1->ctype;
    decl->Init = NULL;
    
    return exp;
}

Var *parseParamDeclaration(Parser *parser) {
    // Type *ty = parseSpecifier(parser->lexer);
    Type *ty = parseTypename(parser);
    Declaration *decl = Declarator(parser, ty);
    ty = decl->ty;
    if (ty->ty == ARRAY) ty = PtrTo(ty->ArrPtr);
    return addLocalVar(parser, ty, decl->Name);
}

Expression *parseDeclOrExp(Parser *parser) {
    Token *token = PeekToken(parser->lexer);
    if (IsTypename(token->Type)) {
        return parseDeclaration(parser);
    } else {
        return parseExp(parser);
    }
}

// Type *parseSpecifer(Parser *parser) {
//     Token *token = PeekToken(parser->lexer);
//     switch (token->Type) {
//     // 'typedef' unsupported.
//     // case TOKEN_IDENTIFIER:
//     //     return getTypedef(parser->env, token->Literal);
//     case TOKEN_KW_VOID:
//         return &VoidType;
//     case TOKEN_KW_INT:
//         return &IntType;
//     case TOKEN_KW_CHAR:
//         return &CharType;
//     // 'typeof' unsupported.
//     // case TOKEN_KW_TYPEOF:
//     //     MustNextTokenOfType(parser->lexer, TOKEN_SEP_LPAREN);
//     //     Expression *exp = parseExpression(parser->lexer);
//     //     MustNextTokenOfType(parser->lexer, TOKEN_SEP_RPAREN);
//     //     return getTypeOfExp(exp);
//     // 'struct' unsupported.
//     // case TOKEN_KW_STRUCT:
//     //     NextToken(parser->lexer);
//     //     token = PeekToken(parser->lexer);
//     //     Type *ty = NULL;
//     //     char *tag = NULL;

//     //     if (token->Type == TOKEN_IDENTIFIER) {
//     //         tag = token->Literal;
//     //         ty = getTag(parser->env, tag);
//     //         token = NextToken(parser->lexer);
//     //     }

//     //     if (!ty) {
//     //         ty = calloc(1, sizeof(Type));
//     //         ty->ty = STRUCT;
//     //     }

//     //     if (token->Type == TOKEN_SEP_LCURLY) {
//     //         ty->Members = NewMap();
//     //         while (!NextTokenOfType(parser->lexer, TOKEN_SEP_RCURLY)) {
//     //             Declaration *decl = parseTypeDeclaration(parser->lexer);
//     //             MapPut(ty->Members, decl->Name, ty->ty);
//     //         }
//     //     }
//     default:
//         Error(parser->lexer, "unsupported type.");
//     }
// }

Statement *parseStmt(Parser *parser) {
    Token *token = PeekToken(parser->lexer);

    switch (token->Type) {
    // unsupported yet.
    // case TOKEN_KW_TYPEDEF:
    case TOKEN_KW_IF: {
        NextToken(parser->lexer);
        Statement *stmt = NewStmt(STMT_IF);
        ExpectToken(parser->lexer, TOKEN_SEP_LPAREN);
        stmt->Cond = parseExp(parser);
        ExpectToken(parser->lexer, TOKEN_SEP_RPAREN);
        stmt->Body = parseStmt(parser);
        if (ConsumeToken(parser->lexer, TOKEN_KW_ELSE)) {
            stmt->Else = parseStmt(parser);
        }
        return stmt;
    }
    case TOKEN_KW_FOR: {
        NextToken(parser->lexer);
        Statement *stmt = NewStmt(STMT_FOR);
        parser->env = newEnv(parser->env);
        VectorPush(parser->Breaks, stmt);
        VectorPush(parser->Continues, stmt);

        ExpectToken(parser->lexer, TOKEN_SEP_LPAREN);

        if (!ConsumeToken(parser->lexer, TOKEN_SEP_SEMI)) {
            stmt->Init = parseDeclOrExp(parser);
            ExpectToken(parser->lexer, TOKEN_SEP_SEMI);
        }

        if (!ConsumeToken(parser->lexer, TOKEN_SEP_SEMI)) {
            stmt->Cond = parseExp(parser);
            ExpectToken(parser->lexer, TOKEN_SEP_SEMI);
        }

        if (!ConsumeToken(parser->lexer, TOKEN_SEP_RPAREN)) {
            stmt->Step = parseExp(parser);
            ExpectToken(parser->lexer, TOKEN_SEP_RPAREN);
        }

        stmt->Body = parseStmt(parser);
        VectorPop(parser->Breaks);
        VectorPop(parser->Continues);
        parser->env = parser->env->prev;
        return stmt;
    }
    case TOKEN_KW_WHILE: {
        NextToken(parser->lexer);
        Statement *stmt = NewStmt(STMT_FOR);
        VectorPush(parser->Breaks, stmt);
        VectorPush(parser->Continues, stmt);

        ExpectToken(parser->lexer, TOKEN_SEP_LPAREN);
        Expression *exp = parseExp(parser);
        stmt->Cond = exp;
        ExpectToken(parser->lexer, TOKEN_SEP_RPAREN);

        stmt->Body = parseStmt(parser);
        VectorPop(parser->Breaks);
        VectorPop(parser->Continues);
        return stmt;
    }
    case TOKEN_KW_DO: {
        NextToken(parser->lexer);
        Statement *stmt = NewStmt(STMT_DO_WHILE);
        VectorPush(parser->Breaks, stmt);
        VectorPush(parser->Continues, stmt);

        stmt->Body = parseStmt(parser);
        ExpectToken(parser->lexer, TOKEN_KW_WHILE);
        ExpectToken(parser->lexer, TOKEN_SEP_LPAREN);
        Expression *exp = parseExp(parser);
        stmt->Cond = exp;
        ExpectToken(parser->lexer, TOKEN_SEP_RPAREN);
        ExpectToken(parser->lexer, TOKEN_SEP_SEMI);

        VectorPop(parser->Breaks);
        VectorPop(parser->Continues);
        return stmt;
    }
    case TOKEN_KW_SWITCH: {
        NextToken(parser->lexer);
        Statement *stmt = NewStmt(STMT_SWITCH);
        stmt->Cases = NewVector();
        VectorPush(parser->Breaks, stmt);
        VectorPush(parser->Switches, stmt);

        ExpectToken(parser->lexer, TOKEN_SEP_LPAREN);
        Expression *exp = parseExp(parser);
        stmt->Cond = exp;
        ExpectToken(parser->lexer, TOKEN_SEP_RPAREN);

        stmt->Body = parseStmt(parser);
        VectorPop(parser->Breaks);
        VectorPop(parser->Switches);
        return stmt;
    }
    case TOKEN_KW_CASE: {
        NextToken(parser->lexer);
        if (VectorSize(parser->Switches) == 0) {
            Error(parser->lexer, token, "stray case.");
        }
        Statement *stmt = NewStmt(STMT_CASE);
        Expression *exp = NewExp(EXP_INT, NULL);
        exp->Val = parseConstExp(parser);
        stmt->Cond = exp;
        ExpectToken(parser->lexer, TOKEN_SEP_COLON);
        stmt->Body = parseStmt(parser);
        
        Statement *father = VectorLast(parser->Switches);
        VectorPush(father->Cases, stmt);
        return stmt;
    }
    case TOKEN_KW_DEFAULT: {
        NextToken(parser->lexer);
        Statement *stmt = NewStmt(STMT_CASE);
        stmt->Default = 1;
        ExpectToken(parser->lexer, TOKEN_SEP_COLON);
        stmt->Body = parseStmt(parser);
        
        Statement *father = VectorLast(parser->Switches);
        VectorPush(father->Cases, stmt);
        return stmt;
    }
    case TOKEN_KW_BREAK: {
        NextToken(parser->lexer);
        if (VectorSize(parser->Breaks) == 0) {
            Error(parser->lexer, token, "stray break.");
        }
        Statement *stmt = NewStmt(STMT_BREAK);
        stmt->Body = VectorLast(parser->Breaks);
        ExpectToken(parser->lexer, TOKEN_SEP_SEMI);
        return stmt;
    }
    case TOKEN_KW_CONTINUE: {
        NextToken(parser->lexer);
        if (VectorSize(parser->Continues) == 0) {
            Error(parser->lexer, token, "stray continue.");
        }
        Statement *stmt = NewStmt(STMT_CONTINUE);
        stmt->Body = VectorLast(parser->Continues);
        ExpectToken(parser->lexer, TOKEN_SEP_SEMI);
        return stmt;
    }
    case TOKEN_KW_RETURN: {
        NextToken(parser->lexer);
        Statement *stmt = NewStmt(STMT_RETURN);
        stmt->Exp = parseExp(parser);
        ExpectToken(parser->lexer, TOKEN_SEP_SEMI);
        return stmt;
    }
    case TOKEN_SEP_LCURLY: {
        NextToken(parser->lexer);
        return parseCompoundStmt(parser);
    }
    case TOKEN_SEP_SEMI: {
        NextToken(parser->lexer);
        return &NullStmt;
    }
    default: {
        Expression *exp = parseDeclOrExp(parser);
        if (!exp) {
            ExpectToken(parser->lexer, TOKEN_SEP_SEMI);
            return &NullStmt;
        } else {
            Statement *stmt = NewExpStmt(token, exp);
            ExpectToken(parser->lexer, TOKEN_SEP_SEMI);
            return stmt;
        }
    }
    }
}

Statement *parseCompoundStmt(Parser *parser) {
    Statement *stmt = NewStmt(STMT_COMP);
    stmt->Stmts = NewVector();
    parser->env = newEnv(parser->env);
    while (!ConsumeToken(parser->lexer, TOKEN_SEP_RCURLY)) {
        VectorPush(stmt->Stmts, parseStmt(parser));
    }
    parser->env = parser->env->prev;
    return stmt;
}

void parseTopLevel(Parser *parser) {
    // Token *_typedef = NextTokenOfType(parser->lexer, TOKEN_KW_TYPEDEF);
    Token *Extern = ConsumeToken(parser->lexer, TOKEN_KW_EXTERN) ;

    // Type *ty = parseSpecifer(parser->lexer);
    Type *ty = parseTypename(parser);
    while (ConsumeToken(parser->lexer, TOKEN_OP_MUL)) ty = PtrTo(ty);
    Token *ident = ExpectToken(parser->lexer, TOKEN_IDENTIFIER);

    // Function
    if (ConsumeToken(parser->lexer, TOKEN_SEP_LPAREN)) {
        // define func type
        Type *func = calloc(1, sizeof(Type));
        func->ty = FUNC;
        func->Returning = ty;

        parser->LocalVars = NewVector();
        parser->Breaks = NewVector();
        parser->Continues = NewVector();
        parser->Switches = NewVector();

        addLocalVar(parser, func, ident->Literal);
        parser->env = newEnv(parser->env);

        Vector *params = NewVector();
        while (!ConsumeToken(parser->lexer, TOKEN_SEP_RPAREN)) {
            if (VectorSize(params) > 0) {
                ExpectToken(parser->lexer, TOKEN_SEP_COMMA);
            }
            VectorPush(params, parseParamDeclaration(parser));
        }
        Token *token = PeekToken(parser->lexer);

        if (ConsumeToken(parser->lexer, TOKEN_SEP_SEMI)) return;

        // function body
        ExpectToken(parser->lexer, TOKEN_SEP_LCURLY);
        Function *fn = NewFunction();
        fn->Stmt = parseCompoundStmt(parser);
        fn->Name = ident->Literal;
        fn->Params = params;
        fn->LocalVars = parser->LocalVars;
        fn->bbs = NewVector();
        VectorPush(parser->program->Functions, fn);

        parser->env = parser->env->prev;
        return;
    }

    ty = parseArray(parser, ty);
    ExpectToken(parser->lexer, TOKEN_SEP_SEMI);

    // global variable
    addGlobalVar(parser, ty, ident->Literal, NULL, Extern != NULL);
}

Program *ParseProgram(Parser *parser) {
    parser->program = NewProgram();

    while (PeekToken(parser->lexer)->Type != TOKEN_EOF) {
        parseTopLevel(parser);
    }
    parser->program->macros = parser->lexer->macros;
    return parser->program;
}