#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "token.h"

Token *NewToken(int line, TokenType type, char *literal) {
    Token *token = calloc(1, sizeof(Token));
    token->Line = line;
    token->Type = type;
    token->Literal = literal;
    token->Original = literal;
    return token;
}

Token *NewTokenWithOrigin(int line, TokenType type, char *literal, char *origin) {
    Token *token = calloc(1, sizeof(Token));
    token->Line = line;
    token->Type = type;
    token->Literal = literal;
    token->Original = origin;
    return token;
}

TokenType GetTokenType(char *literal) {
    if (!strcmp(literal, "break")) {
        return TOKEN_KW_BREAK;
    } else if (!strcmp(literal, "case")) {
        return TOKEN_KW_CASE;
    } else if (!strcmp(literal, "continue")) {
        return TOKEN_KW_CONTINUE;
    } else if (!strcmp(literal, "const")) {
        return TOKEN_KW_CONST;
    } else if (!strcmp(literal, "char")) {
        return TOKEN_KW_CHAR;
    } else if (!strcmp(literal, "default")) {
        return TOKEN_KW_DEFAULT;
    } else if (!strcmp(literal, "do")) {
        return TOKEN_KW_DO;
    } else if (!strcmp(literal, "double")) {
        return TOKEN_KW_DOUBLE;
    } else if (!strcmp(literal, "else")) {
        return TOKEN_KW_ELSE;
    } else if (!strcmp(literal, "enum")) {
        return TOKEN_KW_ENUM;
    } else if (!strcmp(literal, "extern")) {
        return TOKEN_KW_EXTERN;
    } else if (!strcmp(literal, "float")) {
        return TOKEN_KW_FLOAT;
    } else if (!strcmp(literal, "for")) {
        return TOKEN_KW_FOR;
    } else if (!strcmp(literal, "goto")) {
        return TOKEN_KW_GOTO;
    } else if (!strcmp(literal, "if")) {
        return TOKEN_KW_IF;
    } else if (!strcmp(literal, "int")) {
        return TOKEN_KW_INT;
    } else if (!strcmp(literal, "long")) {
        return TOKEN_KW_LONG;
    } else if (!strcmp(literal, "return")) {
        return TOKEN_KW_RETURN;
    } else if (!strcmp(literal, "signed")) {
        return TOKEN_KW_SIGNED;
    } else if (!strcmp(literal, "sizeof")) {
        return TOKEN_KW_SIZEOF;
    } else if (!strcmp(literal, "static")) {
        return TOKEN_KW_STATIC;
    } else if (!strcmp(literal, "struct")) {
        return TOKEN_KW_STRUCT;
    } else if (!strcmp(literal, "switch")) {
        return TOKEN_KW_SWITCH;
    } else if (!strcmp(literal, "typedef")) {
        return TOKEN_KW_TYPEDEF;
    } else if (!strcmp(literal, "typeof")) {
        return TOKEN_KW_TYPEOF;
    } else if (!strcmp(literal, "unsigned")) {
        return TOKEN_KW_UNSIGNED;
    } else if (!strcmp(literal, "void")) {
        return TOKEN_KW_VOID;
    } else if (!strcmp(literal, "while")) {
        return TOKEN_KW_WHILE;
    } else {
        return TOKEN_IDENTIFIER;
    }
}

char *GetTokenTypeLiteral(TokenType ty) {
    switch (ty) {
    case TOKEN_ILLEGAL:
        return "<ILLEGAL>";
    case TOKEN_EOF:
        return "<EOF>";     
    case TOKEN_VARARG:
        return "...";
    case TOKEN_PREOP:
        return "#";
    case TOKEN_SEP_SEMI:
        return ";";   
    case TOKEN_SEP_COMMA:
        return ",";
    case TOKEN_SEP_DOT:
        return ".";
    case TOKEN_SEP_COLON:
        return ":";
    case TOKEN_SEP_LPAREN:
        return "(";
    case TOKEN_SEP_RPAREN:
        return ")";
    case TOKEN_SEP_LBRACK:
        return "[";
    case TOKEN_SEP_RBRACK:
        return "]";
    case TOKEN_SEP_LCURLY:
        return "{";
    case TOKEN_SEP_RCURLY:
        return "}";
    case TOKEN_OP_ASSIGN:
        return "=";
    case TOKEN_OP_SUB:
        return "-";
    case TOKEN_OP_ARROW:
        return "->";
    case TOKEN_OP_SUBEQ:
        return "-=";
    case TOKEN_OP_SUBSELF:
        return "--";
    case TOKEN_OP_ADD:
        return "+";
    case TOKEN_OP_ADDEQ:
        return "+=";
    case TOKEN_OP_ADDSELF:
        return "++";
    case TOKEN_OP_MUL:
        return "*";
    case TOKEN_OP_MULEQ:
        return "*=";
    case TOKEN_OP_DIV:
        return "/";
    case TOKEN_OP_DIVEQ:
        return "/=";
    case TOKEN_OP_MOD:
        return "%";
    case TOKEN_OP_MODEQ:
        return "%=";
    case TOKEN_OP_BAND:
        return "&";
    case TOKEN_OP_BANDEQ:
        return "&=";
    case TOKEN_OP_BNOT:
        return "~";
    case TOKEN_OP_BOR:
        return "|";
    case TOKEN_OP_BOREQ:
        return "|=";
    case TOKEN_OP_BXOR:
        return "^";
    case TOKEN_OP_BXOREQ:
        return "^=";
    case TOKEN_OP_SHR:
        return ">>";
    case TOKEN_OP_SHREQ:
        return ">>=";
    case TOKEN_OP_SHL:
        return "<<";
    case TOKEN_OP_SHLEQ:
        return "<<=";
    case TOKEN_OP_LT:
        return "<";
    case TOKEN_OP_LE:
        return "<=";
    case TOKEN_OP_GT:
        return ">";
    case TOKEN_OP_GE:
        return ">=";
    case TOKEN_OP_EQ:
        return "==";
    case TOKEN_OP_NE:
        return "!=";
    case TOKEN_OP_AND:
        return "&&";
    case TOKEN_OP_OR:
        return "||";
    case TOKEN_OP_NOT:
        return "!";
    case TOKEN_OP_QST:
        return "?";
    case TOKEN_KW_BREAK:
        return "break";
    case TOKEN_KW_CASE:
        return "case";
    case TOKEN_KW_CONTINUE:
        return "continue";
    case TOKEN_KW_CONST:
        return "const";
    case TOKEN_KW_CHAR:
        return "char";
    case TOKEN_KW_DEFAULT:
        return "default";
    case TOKEN_KW_DO:
        return "do";
    case TOKEN_KW_DOUBLE:
        return "double";
    case TOKEN_KW_ELSE:
        return "else";
    case TOKEN_KW_ENUM:
        return "enum";
    case TOKEN_KW_EXTERN:
        return "extern";
    case TOKEN_KW_FLOAT:
        return "float";
    case TOKEN_KW_FOR:
        return "for";
    case TOKEN_KW_GOTO:
        return "goto";
    case TOKEN_KW_IF:
        return "if";
    case TOKEN_KW_INT:
        return "int";
    case TOKEN_KW_LONG:
        return "long";
    case TOKEN_KW_RETURN:
        return "return";
    case TOKEN_KW_SIGNED:
        return "signed";
    case TOKEN_KW_SIZEOF:
        return "sizeof";
    case TOKEN_KW_STATIC:
        return "static";
    case TOKEN_KW_STRUCT:
        return "struct";
    case TOKEN_KW_SWITCH:
        return "switch";
    case TOKEN_KW_TYPEDEF:
        return "typedef";
    case TOKEN_KW_TYPEOF:
        return "typeof";
    case TOKEN_KW_UNSIGNED:
        return "unsigned";
    case TOKEN_KW_VOID:
        return "void";
    case TOKEN_KW_WHILE:
        return "while";
    case TOKEN_IDENTIFIER:
        return "<identifier>";
    case TOKEN_CHAR:
        return "<char>";
    case TOKEN_NUMBER:
        return "<number>";
    case TOKEN_STRING:
        return "<string>";
    }
    assert(0);
}

TokenType ChangeOpEqual(TokenType ty) {
    switch (ty){
    case TOKEN_OP_SUBEQ:
        return TOKEN_OP_SUB;
    case TOKEN_OP_ADDEQ:
        return TOKEN_OP_ADD;
    case TOKEN_OP_MULEQ:
        return TOKEN_OP_MUL;
    case TOKEN_OP_DIVEQ:
        return TOKEN_OP_DIV;
    case TOKEN_OP_MODEQ:
        return TOKEN_OP_MOD;
    case TOKEN_OP_BANDEQ:
        return TOKEN_OP_BAND;
    case TOKEN_OP_BOREQ:
        return TOKEN_OP_BOR;
    case TOKEN_OP_BXOREQ:
        return TOKEN_OP_BXOR;
    case TOKEN_OP_SHREQ:
        return TOKEN_OP_SHR;
    case TOKEN_OP_SHLEQ:
        return TOKEN_OP_SHL;
    case TOKEN_OP_ADDSELF:
        return TOKEN_OP_ADD;
    case TOKEN_OP_SUBSELF:
        return TOKEN_OP_SUB;
    }
    return ty;
}

int IsOpEqual(TokenType ty) {
    return ty == TOKEN_OP_SUBEQ  ||
           ty == TOKEN_OP_ADDEQ  ||
           ty == TOKEN_OP_MULEQ  ||
           ty == TOKEN_OP_DIVEQ  ||
           ty == TOKEN_OP_MODEQ  ||
           ty == TOKEN_OP_BANDEQ ||
           ty == TOKEN_OP_BOREQ  ||
           ty == TOKEN_OP_BXOREQ ||
           ty == TOKEN_OP_SHREQ  ||
           ty == TOKEN_OP_SHLEQ;
}

int IsTypename(TokenType ty) {
    return ty == TOKEN_KW_VOID ||
           ty == TOKEN_KW_CHAR ||
           ty == TOKEN_KW_INT;
}
