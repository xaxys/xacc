#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct Env Env;
typedef struct Parser Parser;

struct Env {
    Map *vars;
    Map *typedefs;
    Map *tags;
    Env *prev;
};

struct Parser {
    Lexer *lexer;
    Env   *env;

    Program *program;

    Vector *LocalVars;
    Vector *Breaks;
    Vector *Continues;
    Vector *Switches;
};

Parser *NewParser(Lexer *lexer);
Program *ParseProgram(Parser *parser);

#endif