#ifndef LEXER_H
#define LEXER_H
#include "token.h"
#include "util.h"

typedef struct Lexer {
    int      peekPos;
    char    *pos;
    char    *chunk;
    int      chunkSize;
    char    *chunkName;
    char    *syntaxError;
    Token   *tokenCache;
    int      line;

    Map     *macros;
} Lexer;

Lexer *NewLexer(char *chunkName, char *chunk);

Token *PeekToken(Lexer *lexer);
Token *NextToken(Lexer *lexer);
Token *ConsumeToken(Lexer *lexer, TokenType type);

#endif