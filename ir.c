#include "ir.h"
#include "token.h"

IRType GetIRType(TokenType ty) {
    switch (ty) {
    case TOKEN_OP_EQ:
        return IR_EQ;
    case TOKEN_OP_NE:
        return IR_NE;
    case TOKEN_OP_ADD:
        return IR_ADD;
    case TOKEN_OP_SUB:
        return IR_SUB;
    case TOKEN_OP_MUL:
        return IR_MUL;
    case TOKEN_OP_DIV:
        return IR_DIV;
    case TOKEN_OP_MOD:
        return IR_MOD;
    case TOKEN_OP_LT:
        return IR_LT;
    case TOKEN_OP_LE:
        return IR_LE;
    case TOKEN_OP_BAND:
        return IR_AND;
    case TOKEN_OP_BOR:
        return IR_OR;
    case TOKEN_OP_BXOR:
        return IR_XOR;
    case TOKEN_OP_SHL:
        return IR_SHL;
    case TOKEN_OP_SHR:
        return IR_SHR;
    default:
        return IR_ILLEGAL;
    }
}