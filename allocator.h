#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "ast.h"
#include "ir.h"

extern int num_regs;
void Allocate(Program *prog);

#endif