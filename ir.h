#ifndef IR_H
#define IR_H

#include "ast.h"

typedef struct IR IR;

typedef enum {
    IR_ILLEGAL = -1,
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_IMM,
    IR_BPREL,
    IR_MOV,
    IR_RETURN,
    IR_CALL,
    IR_LABEL_ADDR,
    IR_EQ,
    IR_NE,
    IR_LE,
    IR_LT,
    IR_AND,
    IR_OR,
    IR_XOR,
    IR_SHL,
    IR_SHR,
    IR_MOD,
    IR_JMP,
    IR_BR,
    IR_LOAD,
    IR_LOAD_SPILL,
    IR_STORE,
    IR_STORE_ARG,
    IR_STORE_SPILL,
    IR_NOP,
} IRType;

struct IR {
    IRType ty;

    Reg *r0;
    Reg *r1;
    Reg *r2;

    int imm;
    int Label;
    Var *ID;

    BB *bb1;
    BB *bb2;

    // Load/store size in bytes
    int Size;

    // Function call
    char *Name;
    int NArgs;
    Reg *Args[6];

    // For liveness tracking
    Vector *Kill;

    // For SSA
    Reg *bbArg;
};

IRType GetIRType(TokenType ty);

#endif