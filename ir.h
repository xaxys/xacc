#ifndef IR_H
#define IR_H

#include "ast.h"

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

typedef struct {
    IRType op;

    Reg *r0;
    Reg *r1;
    Reg *r2;

    int imm;
    int label;
    Var *var;

    BB *bb1;
    BB *bb2;

    // Load/store size in bytes
    int size;

    // Function call
    char *name;
    int nargs;
    Reg *args[6];

    // For liveness tracking
    Vector *kill;

    // For SSA
    Reg *bbarg;
} IR;

IRType GetIRType(TokenType ty);

#endif