#ifndef TOKEN_H
#define TOKEN_H

typedef enum TokenType {
	TOKEN_ILLEGAL,
	TOKEN_EOF,
	TOKEN_VARARG,
	TOKEN_PREOP,	   // #
	TOKEN_SEP_SEMI,	   // ;
	TOKEN_SEP_COMMA,   // ,
	TOKEN_SEP_DOT,	   // .
	TOKEN_SEP_COLON,   // :
	TOKEN_SEP_LPAREN,  // (
	TOKEN_SEP_RPAREN,  // )
	TOKEN_SEP_LBRACK,  // [
	TOKEN_SEP_RBRACK,  // ]
	TOKEN_SEP_LCURLY,  // {
	TOKEN_SEP_RCURLY,  // }
	TOKEN_OP_ASSIGN,   // =
	TOKEN_OP_SUB,	   // - (sub or unm)
	TOKEN_OP_ARROW,	   // ->
	TOKEN_OP_SUBEQ,	   // -=
	TOKEN_OP_SUBSELF,  // --
	TOKEN_OP_ADD,	   // +
	TOKEN_OP_ADDEQ,	   // +=
	TOKEN_OP_ADDSELF,  // ++
	TOKEN_OP_MUL,	   // *
	TOKEN_OP_MULEQ,	   // *=
	TOKEN_OP_DIV,	   // /
	TOKEN_OP_DIVEQ,	   // /=
	TOKEN_OP_MOD,	   // %
	TOKEN_OP_MODEQ,	   // %=
	TOKEN_OP_BAND,	   // &
	TOKEN_OP_BANDEQ,   // &=
	TOKEN_OP_BNOT,	   // ~
	TOKEN_OP_BOR,	   // |
	TOKEN_OP_BOREQ,	   // |=
	TOKEN_OP_BXOR,	   // ^
	TOKEN_OP_BXOREQ,   // ^=
	TOKEN_OP_SHR,	   // >>
	TOKEN_OP_SHREQ,	   // >>=
	TOKEN_OP_SHL,	   // <<
	TOKEN_OP_SHLEQ,	   // <<=
	TOKEN_OP_LT,	   // <
	TOKEN_OP_LE,	   // <=
	TOKEN_OP_GT,	   // >
	TOKEN_OP_GE,	   // >=
	TOKEN_OP_EQ,	   // ==
	TOKEN_OP_NE,	   // !=
	TOKEN_OP_AND,	   // &&
	TOKEN_OP_OR,	   // ||
	TOKEN_OP_NOT,	   // !
	TOKEN_OP_QST,	   // ?
	TOKEN_KW_BREAK,	   // break
	TOKEN_KW_CASE,	   // case
	TOKEN_KW_CONTINUE, // continue
	TOKEN_KW_CONST,    // const
	TOKEN_KW_CHAR,	   // char
	TOKEN_KW_DEFAULT,  // default
	TOKEN_KW_DO,       // do
	TOKEN_KW_DOUBLE,   // double
	TOKEN_KW_ELSE,	   // else
	TOKEN_KW_ENUM,	   // enum
	TOKEN_KW_EXTERN,   // externa
	TOKEN_KW_FLOAT,	   // float
	TOKEN_KW_FOR,	   // for
	TOKEN_KW_GOTO,	   // goto
	TOKEN_KW_IF,	   // if
	TOKEN_KW_INT,	   // int
	TOKEN_KW_LONG,	   // long
	TOKEN_KW_RETURN,   // return
	TOKEN_KW_SIGNED,   // signed
	TOKEN_KW_SIZEOF,   // sizeof
	TOKEN_KW_STATIC,   // static
	TOKEN_KW_STRUCT,   // struct
	TOKEN_KW_SWITCH,   // switch
	TOKEN_KW_TYPEDEF,  // typedef
	TOKEN_KW_TYPEOF,  // typeof
	TOKEN_KW_UNSIGNED, // unsigned
	TOKEN_KW_VOID,     // void
	TOKEN_KW_WHILE,	   // while
	TOKEN_IDENTIFIER,  // identifier
	TOKEN_CHAR,		   // char literal
	TOKEN_NUMBER,	   // number literal
	TOKEN_STRING,	   // string literal
} TokenType;

typedef struct Token {
    int         Line;
    TokenType   Type; 
    char       *Literal;
	char       *Original;
} Token;

Token *NewToken(int line, TokenType type, char *literal);
Token *NewTokenWithOrigin(int line, TokenType type, char *literal, char *origin);
TokenType GetTokenType(char *literal);
char *GetTokenTypeLiteral(TokenType ty);
TokenType ChangeOpEqual(TokenType ty);
int IsOpEqual(TokenType ty);
int IsTypename(TokenType ty);

#endif