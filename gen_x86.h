#ifndef GEN_X86_H
#define GEN_X86_H

#include "ast.h"
#include "ir.h"

extern char *regs[];
extern char *regs8[];
extern char *regs32[];
extern int num_regs;
extern int nLabel;

void Genx86(Program *prog);

#endif