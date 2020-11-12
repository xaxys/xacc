#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "lexer.h"

static char escaped[256] = {
    ['0'] = '\0',
    ['a'] = '\a',
    ['b'] = '\b',
    ['f'] = '\f',
    ['\n'] = '\n',
    ['n'] = '\n',
    ['r'] = '\r',
    ['t'] = '\t',
    ['v'] = '\v',
    ['\\'] = '\\',
    ['\''] = '\'',
    ['"'] = '"',
};

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
    lexer->chunkName = strdup(chunkName);
    lexer->chunkSize = strlen(chunk);
    lexer->chunk = StringClone(chunk, lexer->chunkSize);
    lexer->line = 1;
    lexer->pos = lexer->chunk - 1;
    lexer->peekPos = lexer->chunk - 1;

    lexer->macros = NewMap();
    return lexer;
}

// return the next char pos after skipped '\\\n'
char *nextPos(Lexer *lexer, char *cur) {
    if (cur < lexer->chunk + lexer->chunkSize) {
        if (*cur != '\\') return cur;
        if (cur + 1 < lexer->chunk + lexer->chunkSize) {
            if (*(cur + 1) == '\n') {
                lexer->line++;
                return cur + 2;
            }
            else if (*(cur + 1) == '\r') {
                if (cur + 2 < lexer->chunk + lexer->chunkSize) {
                    if (*(cur + 2) == '\n') {
                        lexer->line++;
                        return cur + 3;
                    }
                    else return cur + 2;
                } else return NULL;
            } else return cur;
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

void peekReset(Lexer *lexer) {
    lexer->peekPos = lexer->pos;
}

char *peekChar(Lexer *lexer) {
    char *pos = nextPos(lexer, lexer->peekPos + 1);
    if (pos) {
        lexer->peekPos = pos;
        return lexer->peekPos;
    } else {
        return NULL;
    }
}

char *mustPeekChar(Lexer *lexer) {
    char *ch = peekChar(lexer);
    if (ch) {
        return ch;
    } else {
        ErrorAt(lexer, lexer->chunk + lexer->chunkSize, "unexpected end of file");
    }
}

char *readChar(Lexer *lexer) {
    char *pos = nextPos(lexer, lexer->pos + 1);
    if (pos) {
        lexer->pos = pos;
        peekReset(lexer);
        return lexer->pos;
    } else {
        return NULL;
    }
}

char *readCharN(Lexer *lexer, int n) {
    char *pos = readChar(lexer);
    if (!pos) return NULL;
    for (int i = 1; i < n; i++) {
        if (!readChar(lexer)) return NULL;
    }
    return pos;
}

char *mustReadCharN(Lexer *lexer, int n) {
    char *pos = readCharN(lexer, n);
    if (pos) {
        return pos;
    } else {
        ErrorAt(lexer, lexer->chunk + lexer->chunkSize, "unexpected end of file");
    }
}

char *mustReadChar(Lexer *lexer) {
    return mustReadCharN(lexer, 1);
}

char *readUntilChar(Lexer *lexer, char *tpl) {
    //KMP
    int tlen = strlen(tpl);
    int *f = malloc(sizeof(int) * (tlen + 1));
    f[0] = f[1] = 0;
    for (int i = 1, j = 0; i < tlen; i++) {
		while (j && tpl[i] != tpl[j]) j = f[j];
		j += tpl[i] == tpl[j];
		f[i + 1] = j;
	}
    char *ch = readChar(lexer);
    for (int j = 0; ch != NULL; ch = readChar(lexer)) {
		while (j && *ch != tpl[j]) j = f[j];
		j += *ch == tpl[j];
		if (j == tlen) return ch - tlen + 2;
	}
    return ch;
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
        *c = escaped[*(startPos + 1)];
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
        if (*ch == '\n') {
            ErrorAt(lexer, ch, "unexpected end of line. unfinished string.");
        }
        ch = mustReadChar(lexer);
    }

    // copy string
    StringBuilder *sb = NewStringBuilder();
    for (char *p = startPos; p < ch; p++) {
        if (*p == '\r' || *p == '\n') continue;
        if (*p == '\\' && p + 1 < ch) {
            StringBuilderAdd(sb, escaped[*(++p)]);
        } else {
            StringBuilderAdd(sb, *p);
        }
    }
    return StringBuilderToString(sb);
}

int isHexChar(char c) {
    return isdigit(c) ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

int htoi(char *s) {
    int num = 0;
    for (int i = 0; i < strlen(s); i++) {
        num *= 16;
        if (isdigit(s[i])) {
            num = num + s[i] - '0';
        } else if (s[i] >= 'A' && s[i] <= 'F') {
            num = num + s[i] + 10 - 'A';
        } else if (s[i] >= 'a' && s[i] <= 'f') {
            num = num + s[i] + 10 - 'a';
        } else {
            assert(0 && "not a hex");
        }
    }
    return num;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
char *readNumberConst(Lexer *lexer) {
    char *startPos = mustReadChar(lexer);
    char *ch = peekChar(lexer);
    if (*startPos == '0') {
        if (ch != NULL && (*ch == 'x' || *ch == 'X')) {
            // hex
            readChar(lexer);
            char *ch2 = peekChar(lexer);
            while (ch2 != NULL && isHexChar(*ch2)) {
                readChar(lexer), ch2 = peekChar(lexer);
            }
            peekReset(lexer);

            char *tmp = StringClone(startPos, ch2 - startPos);
            return (char *)(htoi(tmp+2));
        }
    }

    // float unsupported yet
    // int pointFlag = 0;
    // while (isdigit(*ch) || *ch == '.') {
    //     float unsupported yet
    //     if (*ch == '.') {
    //         if (!pointFlag) {
    //             pointFlag = 1;
    //         } else {
    //             ErrorAt(lexer, ch, "invalid number");
    //         }
    //     }
    //     readChar(lexer), ch = peekChar(lexer);
    // }
    while (isdigit(*ch)) {
        readChar(lexer), ch = peekChar(lexer);
    }
    peekReset(lexer);

    char *tmp = StringClone(startPos, ch - startPos);
    return (char *)(atoi(tmp));
}
#pragma GCC diagnostic pop

char *readIdentifier(Lexer *lexer) {
    char *startPos = mustReadChar(lexer);
    char *ch = peekChar(lexer);
    while (isalnum(*ch) || *ch == '_') {
        readChar(lexer), ch = peekChar(lexer);
    }
    peekReset(lexer);

    return StringClone(startPos, ch - startPos);
}

Token *parseToken(Lexer *lexer) {
    char *ch, *ch2, *ch3, *p;

    // skip whitespace and comment
    for (;;) {
        ch = peekChar(lexer);
        if (!ch) goto outSide;
        switch (*ch) {
        case '\n':
            lexer->line++;
        case ' ':
        case '\t':
        case '\r':
        case '\f':
        case '\v':
            readChar(lexer);
            goto nextLoop;
        case '/':
            ch2 = peekChar(lexer);
            if (!ch2) goto outSide;
            switch (*ch2) {
            case '/':
                // skip line
                ch3 = readUntilChar(lexer, "\n");
                if (ch3) lexer->line++;
                goto nextLoop;
            case '*': 
                p = readCharN(lexer, 2);
                // skip long comment
                ch3 = readUntilChar(lexer, "*/");
                if (!ch3) {
                    ErrorAt(lexer, p, "unfinished long comment");
                }
                goto nextLoop;
            }
        default:
            goto outSide;
        }
        nextLoop:
        peekReset(lexer);
    }

    outSide:
    peekReset(lexer);

    // tokenize
    ch = peekChar(lexer);
    if (ch == NULL || *ch == '\0') return NewToken(lexer->line, TOKEN_EOF, "<EOF>");

    switch (*ch) {
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