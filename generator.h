#ifndef GENERATE_H
#define GENERATE_H

#include "util.h"
#include "ast.h"
#include "ir.h"

extern int nLabel;
void GenProgram(Program *program);
void Optimize(Program *program);

#endif