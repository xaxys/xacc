#ifndef GENERATE_H
#define GENERATE_H

#include "util.h"
#include "ast.h"
#include "ir.h"

extern int nLabel;
void genProgram(Program *program);
void Optimize(Program *program);

#endif