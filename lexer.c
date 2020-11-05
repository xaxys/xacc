#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

static void ErrorAt(Lexer *lexer, char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    char *start = loc, *end = loc;
    while (start > lexer->chunk && *(start - 1) != '\n') start--;
    while (*end != '\0' && *end != '\n') end++;
    *end = '\0';
    int pos = loc - start;

    fprintf(stderr, "Lexical Error:\n");
    fprintf(stderr, "File: %s, Line: %d\n", lexer->chunkName, lexer->line);
    fprintf(stderr, "\n");
    fprintf(stderr, "%s\n", start);
    fprintf(stderr, "%*s\n", pos+1, "^");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

Lexer *NewLexer(char *chunkName, char *chunk) {
    Lexer *lexer = calloc(1, sizeof(Lexer));
    lexer->chunkName = malloc(strlen(chunkName) + 1);
    memcpy(lexer->chunkName, chunkName, strlen(chunkName) + 1);
    lexer->chunkSize = strlen(chunk);
    lexer->chunk = malloc(lexer->chunkSize + 1);
    memcpy(lexer->chunk, chunk, lexer->chunkSize + 1);
    lexer->line = 1;
    lexer->pos = lexer->chunk - 1;
    lexer->peekPos = 0;

    lexer->macros = NewMap();
    return lexer;
}

char *readCharN(Lexer *lexer, int n) {
    if (lexer->pos + n < lexer->chunk + lexer->chunkSize) {
        char *pos = lexer->pos;
        lexer->pos += n;
        lexer->peekPos = 0;
        return pos + 1;
    } else {
        return NULL;
    }
}

char *readChar(Lexer *lexer) {
    return readCharN(lexer, 1);
}

char *mustReadCharN(Lexer *lexer, int n) {
    if (lexer->pos + n < lexer->chunk + lexer->chunkSize) {
        char *pos = lexer->pos;
        lexer->pos += n;
        lexer->peekPos = 0;
        return pos + 1;
    } else {
        ErrorAt(lexer, lexer->chunk + lexer->chunkSize, "unexpected end of file");
    }
}

char *mustReadChar(Lexer *lexer) {
    return mustReadCharN(lexer, 1);
}

void peekReset(Lexer *lexer) {
    lexer->peekPos = 0;
}

char *peekChar(Lexer *lexer) {
    if (lexer->pos + lexer->peekPos < lexer->chunk + lexer->chunkSize) {
        lexer->peekPos++;
        return lexer->pos+lexer->peekPos;
    } else {
        return NULL;
    }
}

char *mustPeekChar(Lexer *lexer) {
    if (lexer->pos + lexer->peekPos < lexer->chunk + lexer->chunkSize) {
        lexer->peekPos++;
        return lexer->pos+lexer->peekPos;
    } else {
        ErrorAt(lexer, lexer->chunk + lexer->chunkSize, "unexpected end of file");
    }
}

char escape(char *p) {
    switch (*p) {
    case '0': return '\0';
    case '\n': return '\n';
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case 'b': return '\b';
    case 'f': return '\f';
    case 'v': return '\v';
    case 'a': return '\a';
    case '"': return '\"';
    case '\\': return '\\';
    case '\'': return '\'';
    }
    return -1;
}

char *readCharConst(Lexer *lexer) {
    mustReadChar(lexer); // read '
    char *startPos = lexer->pos + 1;
    char *ch = mustReadChar(lexer);
    if (*ch == '\\') {
        while (*ch != '\'') {
            if (!isalnum(*ch) && *ch != '\\') {
                ErrorAt(lexer, ch, "unexpected char. invalid char literal.");
            }
            ch = mustReadChar(lexer);
        }
        char *c = malloc(1);
        *c = escape(startPos + 1);
        if (*c == -1) {
            ErrorAt(lexer, startPos, "unable to escape character.");
        }
        return c;
    } else {
        char *tmp = mustReadChar(lexer);
        if (*tmp != '\'') {
            ErrorAt(lexer, tmp, "invalid character constant.");
        }
        return ch;
    }
}

char *readStringConst(Lexer *lexer) {
    mustReadChar(lexer); // read "
    char *startPos = lexer->pos + 1;
    int newLine = 0;
    char *ch = mustReadChar(lexer);
    while (*ch != '"') {
        if (*ch != '\n' && *ch != '\r') newLine = 0;
        if (*ch == '\\') newLine = 1;
        else if (*ch == '\n') {
            if (newLine == 1) lexer->line++;
            else ErrorAt(lexer, ch, "unexpected end of line. unfinished string.");
        }
        ch = mustReadChar(lexer);
    }

    // copy string
    char *tmpStart = malloc(ch - startPos);
    char *tmp = tmpStart;
    for (char *p = startPos; p < ch; p++) {
        if (*p == '\r' || *p == '\n') continue;
        if (*p == '\\' && p + 1 < ch) *tmp = escape(++p);
        else *tmp = *p;
        tmp++;
    }
    *tmp = '\0';
    return tmpStart;
}

char *readNumberConst(Lexer *lexer) {
    char *startPos = mustReadChar(lexer);
    char *ch = peekChar(lexer);
    if (*startPos == '0') {
        if (ch != NULL && (*ch == 'x' || *ch == 'X')) {
            readChar(lexer);
            char *ch2 = peekChar(lexer);
            while (ch2 != NULL &&
                (isdigit(*ch2)
                || (*ch2 >= 'A' && *ch2 <= 'F')
                || (*ch2 >= 'a' && *ch2 <= 'f'))) {
                readChar(lexer), ch2 = peekChar(lexer);
            }
            peekReset(lexer);

            // copy number string
            char *tmp = malloc(ch2 - startPos + 1);
            memcpy(tmp, startPos, ch2 - startPos);
            tmp[ch2 - startPos + 1] = '\0';
            return tmp;
        }
    }

    int pointFlag = 0;
    while (isdigit(*ch) || *ch == '.') {
        if (*ch == '.') {
            if (!pointFlag) {
                pointFlag = 1;
            } else {
                ErrorAt(lexer, ch, "invalid number");
            }
        }
        readChar(lexer), ch = peekChar(lexer);
    }
    peekReset(lexer);

    // copy number string
    char *tmp = malloc(ch - startPos + 1);
    memcpy(tmp, startPos, ch - startPos);
    *(tmp + (ch - startPos + 1)) = '\0';
    return tmp;
}

char *readIdentifier(Lexer *lexer) {
    char *startPos = mustReadChar(lexer);
    char *ch = peekChar(lexer);
    while (isalnum(*ch) || *ch == '_') {
        readChar(lexer), ch = peekChar(lexer);
    }
    peekReset(lexer);

    // copy number string
    char *tmp = malloc(ch - startPos + 1);
    memcpy(tmp, startPos, ch - startPos);
    tmp[ch - startPos + 1] = '\0';
    return tmp;
}

Token *parseToken(Lexer *lexer) {
    char *ch, *ch2, *ch3, *ch4;

    // skip whitespace and comment
    int flag = 1;
    while (flag) {
        ch = peekChar(lexer);
        if (ch == NULL) break;
        switch (*ch) {
        case '\n':
            lexer->line++;
        case ' ':
        case '\t':
        case '\r':
        case '\f':
        case '\v':
            readChar(lexer);
            break;
        case '/':
            ch2 = peekChar(lexer);
            if (ch2 == NULL) break;
            switch (*ch2) {
            case '/':
                // skip line
                for (ch3 = readChar(lexer); ch3 != NULL && *ch3 != '\n'; ch3 = readChar(lexer));
                lexer->line++;
                continue;
            case '*':
                // skip long comment
                for (ch3 = mustReadChar(lexer); ; ch3 = mustReadChar(lexer)) {
                    if (*ch3 == '*') {
                        ch4 = mustPeekChar(lexer);
                        if (*ch4 == '/') { // peek: */
                            readChar(lexer);
                            continue;
                        }
                    }
                }
                break;
            }
            flag = 0;
            break;
        default:
            flag = 0;
        }
        peekReset(lexer);
    }

    // tokenize
    ch = peekChar(lexer);
    if (ch == NULL || *ch == '\0') return NewToken(lexer->line, TOKEN_EOF, "<EOF>");

    switch (*ch) {
	// case '\n': // peek: \n
	// 	readChar(lexer);
	// 	return NewToken(lexer->line++, TOKEN_SEP_EOLN, "<end-of-line>");
	case ';': // peek: ;
        return NewToken(lexer->line, TOKEN_SEP_SEMI, readChar(lexer));
    case ',': // peek: ,
        return NewToken(lexer->line, TOKEN_SEP_COMMA, readChar(lexer));
    case '(': // peek: (
        return NewToken(lexer->line, TOKEN_SEP_LPAREN, readChar(lexer));
    case ')': // peek: )
        return NewToken(lexer->line, TOKEN_SEP_RPAREN, readChar(lexer));
    case '[': // peek: [
        return NewToken(lexer->line, TOKEN_SEP_LBRACK, readChar(lexer));
    case ']': // peek: ]
        return NewToken(lexer->line, TOKEN_SEP_RBRACK, readChar(lexer));
    case '{': // peek: {
        return NewToken(lexer->line, TOKEN_SEP_LCURLY, readChar(lexer));
    case '}': // peek: }
		return NewToken(lexer->line, TOKEN_SEP_RCURLY, readChar(lexer));
	case ':':
        return NewToken(lexer->line, TOKEN_SEP_COLON, readChar(lexer));
    case '+':
        ch2 = mustPeekChar(lexer);
		switch (*ch2) {
		case '+': // peek: ++
			return NewToken(lexer->line, TOKEN_OP_ADDSELF, readCharN(lexer, 2));
		case '=': // peek: +=
            return NewToken(lexer->line, TOKEN_OP_ADDEQ, readCharN(lexer, 2));
        default: // peek: +
            return NewToken(lexer->line, TOKEN_OP_ADD, readChar(lexer));
        }
	case '-':
        ch2 = mustPeekChar(lexer);
		switch (*ch2) {
		case '-': // peek: --
            return NewToken(lexer->line, TOKEN_OP_SUBSELF, readCharN(lexer, 2));
        case '=': // peek: -=
            return NewToken(lexer->line, TOKEN_OP_SUBEQ, readCharN(lexer, 2));
        case '>': // peek: ->
            return NewToken(lexer->line, TOKEN_OP_ARROW, readCharN(lexer, 2));
        default: // peek: -
            return NewToken(lexer->line, TOKEN_OP_SUB, readChar(lexer));
        }
	case '*':
        ch2 = mustPeekChar(lexer);
		switch (*ch2) {
		case '=': // peak: *=
            return NewToken(lexer->line, TOKEN_OP_MULEQ, readCharN(lexer, 2));
        default: // peek: *
            return NewToken(lexer->line, TOKEN_OP_MUL, readChar(lexer));
        }
	case '/':
        ch2 = mustPeekChar(lexer);
        switch (*ch2) {
        case '=': // peek: /=
            return NewToken(lexer->line, TOKEN_OP_DIVEQ, readCharN(lexer, 2));
        default:
			return NewToken(lexer->line, TOKEN_OP_DIV, readChar(lexer));
		}
	case '~':
        return NewToken(lexer->line, TOKEN_OP_BNOT, readChar(lexer));
    case '%':
        ch2 = mustPeekChar(lexer);
        switch (*ch2) {
		case '=': // peek: %=
            return NewToken(lexer->line, TOKEN_OP_MODEQ, readCharN(lexer, 2));
        default: // peek: %
            return NewToken(lexer->line, TOKEN_OP_MOD, readChar(lexer));
        }
	case '&':
        ch2 = mustPeekChar(lexer);
        switch (*ch2) {
		case '&': // peek: &&
            return NewToken(lexer->line, TOKEN_OP_AND, readCharN(lexer, 2));
        case '=': // peek: &=
            return NewToken(lexer->line, TOKEN_OP_BANDEQ, readCharN(lexer, 2));
        default: // peek: &
            return NewToken(lexer->line, TOKEN_OP_BAND, readChar(lexer));
        }
	case '|':
		ch2 = mustPeekChar(lexer);
        switch (*ch2) {
		case '|': // peek: ||
            return NewToken(lexer->line, TOKEN_OP_OR, readCharN(lexer, 2));
        case '=': // peek: |=
            return NewToken(lexer->line, TOKEN_OP_BOREQ, readCharN(lexer, 2));
        default: // peek: |
            return NewToken(lexer->line, TOKEN_OP_BOR, readChar(lexer));
        }
	case '^':
        ch2 = mustPeekChar(lexer);
        switch (*ch2) {
		case '=': // peek: ^=
            return NewToken(lexer->line, TOKEN_OP_BXOREQ, readCharN(lexer, 2));
        default: // peek: ^
            return NewToken(lexer->line, TOKEN_OP_BXOR, readChar(lexer));
        }
	case '#': // peek: #
        return NewToken(lexer->line, TOKEN_PREOP, readChar(lexer));
    case '!':
        ch2 = mustPeekChar(lexer);
        switch (*ch2) {
		case '=': // peek: !=
            return NewToken(lexer->line, TOKEN_OP_NE, readCharN(lexer, 2));
        default: // peek: !
            return NewToken(lexer->line, TOKEN_OP_NOT, readChar(lexer));
        }
	case '?': // peek: ?
        return NewToken(lexer->line, TOKEN_OP_QST, readChar(lexer));
    case '=':
		ch2 = mustPeekChar(lexer);
        switch (*ch2) {
		case '=': // peek: ==
            return NewToken(lexer->line, TOKEN_OP_EQ, readCharN(lexer, 2));
        default: // peek: =
            return NewToken(lexer->line, TOKEN_OP_ASSIGN, readChar(lexer));
        }
	case '<':
		ch2 = mustPeekChar(lexer);
        switch (*ch2) {
		case '<':
			ch3 = mustPeekChar(lexer);
            switch (*ch3) {
            case '=': // peek: <<=
                return NewToken(lexer->line, TOKEN_OP_SHLEQ, readCharN(lexer, 3));
            default: // peek: <<
                return NewToken(lexer->line, TOKEN_OP_SHL, readCharN(lexer, 2));
            }
		case '=': // peek: <=
            return NewToken(lexer->line, TOKEN_OP_LE, readCharN(lexer, 2));
        default: // peek: <
            return NewToken(lexer->line, TOKEN_OP_LT, readChar(lexer));
        }
	case '>':
		ch2 = mustPeekChar(lexer);
        switch (*ch2) {
		case '>':
			ch3 = mustPeekChar(lexer);
            switch (*ch3) {
            case '=': // peek: >>=
                return NewToken(lexer->line, TOKEN_OP_SHREQ, readCharN(lexer, 3));
            default: // peek: >>
                return NewToken(lexer->line, TOKEN_OP_SHR, readCharN(lexer, 2));
            }
		case '=': // peek: >=
            return NewToken(lexer->line, TOKEN_OP_GE, readCharN(lexer, 2));
        default: // peek: >
            return NewToken(lexer->line, TOKEN_OP_GT, readChar(lexer));
        }
	case '.':
        ch2 = mustPeekChar(lexer);
        switch (*ch2) {
		case '.':
			ch3 = mustPeekChar(lexer);
            if (*ch3 == '.') { // peek: ...
                return NewToken(lexer->line, TOKEN_VARARG, readCharN(lexer, 3));
            }
		default:
            ch3 = mustPeekChar(lexer);
            if (!isdigit(*ch3)) { // peek: .
                return NewToken(lexer->line, TOKEN_SEP_DOT, readChar(lexer));
            }
        }
    case '\'': // peek: '[CHAR]'
        return NewTokenWithOrigin(lexer->line, TOKEN_CHAR, readCharConst(lexer), ch);
    case '"': // peek: "[STRING]"
        ;int line = lexer->line;
		return NewTokenWithOrigin(line, TOKEN_STRING, readStringConst(lexer), ch + 1);
    }

	if (*ch == '.' || isdigit(*ch)) {
        return NewTokenWithOrigin(lexer->line, TOKEN_NUMBER, readNumberConst(lexer), ch);
	}
	if (*ch == '_' || isalpha(*ch)) {
		char *identifer = readIdentifier(lexer);
		return NewTokenWithOrigin(lexer->line, GetTokenType(identifer), identifer, ch);
	}
    
    ErrorAt(lexer, ch, "invalid character.");
    return NewToken(lexer->line, TOKEN_ILLEGAL, ch);
}

Token *nextToken(Lexer *lexer) {
    // preprocess
    Token *token = parseToken(lexer);
    while (token->Type == TOKEN_PREOP) {
        token = parseToken(lexer);
        if (token->Type != TOKEN_IDENTIFIER) {
            ErrorAt(lexer, token->Original, "unknown macro.");
        }
        if (!strcmp(token->Literal, "include")) {
            token = parseToken(lexer);
            if (token->Type != TOKEN_STRING) {
                ErrorAt(lexer, token->Original, "unknown include file.");
            }
            // ...
        } else if (!strcmp(token->Literal, "define")) {
            token = parseToken(lexer);
            if (token->Type != TOKEN_IDENTIFIER) {
                ErrorAt(lexer, token->Original, "unknown macro.");
            }
            char *name = token->Literal;
            token = parseToken(lexer);
            if (token->Type == TOKEN_CHAR   ||
                token->Type == TOKEN_NUMBER ||
                token->Type == TOKEN_STRING ||
                token->Type == TOKEN_IDENTIFIER) {
                MapPut(lexer->macros, name, token); 
                token = parseToken(lexer);
            } else {
                MapPut(lexer->macros, name, NULL);
            }
        }
    }
    if (token->Type == TOKEN_IDENTIFIER) {
        for (int i = 0; i < VectorSize(lexer->macros->keys); i++) {
            char *key = VectorGet(lexer->macros->keys, i);
            if (!strcmp(token->Literal, key)) {
                Token *t = VectorGet(lexer->macros->vals, i);
                if (t == NULL) {
                    ErrorAt(lexer, token->Original, "uninitialized macro.");
                }
                token->Type = t->Type;
                token->Literal = t->Literal;
                break;
            }
        }
    }
    return token;
}

Token *PeekToken(Lexer *lexer) {
    if (lexer->tokenCache != NULL) {
        return lexer->tokenCache;
    } else {
        lexer->tokenCache = nextToken(lexer);
        return lexer->tokenCache;
    }
}

Token *NextToken(Lexer *lexer) {
    if (lexer->tokenCache != NULL) {
        Token *token = lexer->tokenCache;
        lexer->tokenCache = NULL;
        return token;
    } else {
        return nextToken(lexer);
    }
}

Token *ConsumeToken(Lexer *lexer, TokenType type) {
    if (PeekToken(lexer)->Type == type) {
        return NextToken(lexer);
    } else {
        return NULL;
    }
}

Token *expectToken(Lexer *lexer, TokenType type) {
    Token *token = ConsumeToken(lexer, type);
    if (token == NULL) {
        ErrorAt(lexer, PeekToken(lexer)->Original, "unexpected symbol.");
    } else {
        return token;
    }
}