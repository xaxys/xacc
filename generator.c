#include "generator.h"
#include "stdlib.h"
#include <assert.h>
#include <string.h>

Function *fn;
BB *out;
int nreg = 1;

void genStmt(Statement *stmt);

BB *NewBB() {
    BB *bb = calloc(1, sizeof(BB));
    bb->label = nLabel++;
    bb->ir = NewVector();
    bb->succ = NewVector();
    bb->pred = NewVector();
    bb->def_regs = NewVector();
    bb->in_regs = NewVector();
    bb->out_regs = NewVector();
    VectorPush(fn->bbs, bb);
    return bb;
}

IR *NewIR(IRType ty) {
    IR *ir = calloc(1, sizeof(IR));
    ir->op = ty;
    VectorPush(out->ir, ir);
    return ir;
}

Reg *newReg() {
    Reg *r = calloc(1, sizeof(Reg));
    r->vn = nreg++;
    r->rn = -1;
    return r;
}

IR *emitIR(IRType ty, Reg *r0, Reg *r1, Reg *r2) {
    IR *ir = NewIR(ty);
    ir->r0 = r0;
    ir->r1 = r1;
    ir->r2 = r2;
    return ir;
}

IR *emitBR(Reg *r, BB *then, BB *els) {
    IR *ir = NewIR(IR_BR);
    ir->r2 = r;
    ir->bb1 = then;
    ir->bb2 = els;
    return ir;
}

IR *emitJmp(BB *bb) {
    IR *ir = NewIR(IR_JMP);
    ir->bb1 = bb;
    return ir;
}

IR *emitJmpArg(BB *bb, Reg *r) {
    IR *ir = NewIR(IR_JMP);
    ir->bb1 = bb;
    ir->bbarg = r;
    return ir;
}

Reg *emitImm(int imm) {
    Reg *r = newReg();
    IR *ir = NewIR(IR_IMM);
    ir->r0 = r;
    ir->imm = imm;
    return r;
}

Reg *genExp(Expression *exp);

void emitLoad(Expression *exp, Reg *dst, Reg *src) {
    IR *ir = emitIR(IR_LOAD, dst, NULL, src);
    ir->size = exp->ctype->Size;
}

// In C, all expressions that can be written on the left-hand side of
// the '=' operator must have an address in memory. In other words, if
// you can apply the '&' operator to take an address of some
// expression E, you can assign E to a new value.
//
// Other expressions, such as `1+2`, cannot be written on the Exp1 of
// '=', since they are just temporary values that don't have an address.
//
// The stuff that can be written on the Exp1 of '=' is called lvalue.
// Other values are called rvalue. An lvalue is essentially an address.
//
// When lvalues appear on the rvalue context, they are converted to
// rvalues by loading their values from their addresses. You can think
// '&' as an operator that suppresses such automatic lvalue-to-rvalue
// conversion.
//
// This function evaluates a given exp as an lvalue.
Reg *genLeftValue(Expression *exp) {
    if (exp->ty == EXP_DEREF) {
        return genExp(exp->Exp1);
    }

    if (exp->ty == EXP_ACCSESS) {
        Reg *r1 = newReg();
        Reg *r2 = genLeftValue(exp->Exp1);
        Reg *r3 = emitImm(exp->ctype->offset);
        emitIR(IR_ADD, r1, r2, r3);
        return r1;
    }

    if (exp->ty == EXP_VARREF) {
        Var *var = exp->ID;

        IR *ir;
        if (var->Local) {
            ir = NewIR(IR_BPREL);
            ir->r0 = newReg();
            ir->var = var;
        } else {
            ir = NewIR(IR_LABEL_ADDR);
            ir->r0 = newReg();
            ir->name = var->Name;
        }
        return ir->r0;
    }

    assert(0 && "illegal leftvalue");
}

Reg *genUnop(Expression *exp) {
    Reg *r1 = newReg();
    Reg *r2 = genExp(exp->Exp1);
    switch (exp->Op->Type) {
    case TOKEN_OP_NOT:
        emitIR(IR_EQ, r1, r2, emitImm(0));
        return r1;
    case TOKEN_OP_BNOT:
        emitIR(IR_XOR, r1, r2, emitImm(-1));
        return r1;
    }
}

Reg *genBinop(Expression *exp) {
    Reg *r1 = newReg();
    Reg *r2 = genExp(exp->Exp1);
    Reg *r3 = genExp(exp->Exp2);

    IRType ty = GetIRType(exp->Op->Type);
    assert(ty != IR_ILLEGAL && "unexpected operator");
    emitIR(ty, r1, r2, r3);
    return r1;
}

Reg *genMultiop(Expression *exp) {
    switch (exp->Op->Type) {
    case TOKEN_OP_AND: {
        BB *bb = NewBB();
        BB *set0 = NewBB();
        BB *set1 = NewBB();
        BB *last = NewBB();

        for (int i = 0; i < VectorSize(exp->Exps) - 1; i++) {
            Expression *tmp = VectorGet(exp->Exps, i);
            emitBR(genExp(tmp), bb, set0);
            out = bb;
            bb = NewBB();
        }

        emitBR(genExp(VectorLast(exp->Exps)), set1, set0);

        out = set0;
        emitJmpArg(last, emitImm(0));

        out = set1;
        emitJmpArg(last, emitImm(1));

        out = last;
        out->param = newReg();
        return out->param;
    }
    case TOKEN_OP_OR: {
        BB *bb = NewBB();
        BB *set0 = NewBB();
        BB *set1 = NewBB();
        BB *last = NewBB();

        for (int i = 0; i < VectorSize(exp->Exps) - 1; i++) {
            Expression *exp = VectorGet(exp->Exps, i);
            emitBR(genExp(exp), set1, bb);
            out = bb;
            bb = NewBB();
        }

        emitBR(genExp(VectorLast(exp->Exps)), set1, set0);

        out = set0;
        emitJmpArg(last, emitImm(0));

        out = set1;
        emitJmpArg(last, emitImm(1));

        out = last;
        out->param = newReg();
        return out->param;
    }
    case TOKEN_SEP_COMMA:
        for (int i = 0; i < VectorSize(exp->Exps) - 1; i++) {
            genExp(VectorGet(exp->Exps, i));
        }
        return genExp(VectorLast(exp->Exps));
    }
    assert(0 && "illegal multiop");
}

Reg *genExp(Expression *exp) {
    switch (exp->ty) {
    case EXP_INT:
    case EXP_CHAR:
        return emitImm(exp->Val);
    case EXP_BINOP:
        return genBinop(exp);
    case EXP_UNOP:
        return genUnop(exp);
    case EXP_MULTIOP:
        return genMultiop(exp);
    case EXP_VARREF:
    case EXP_ACCSESS: {
        Reg *r = newReg();
        emitLoad(exp, r, genLeftValue(exp));
        return r;
    }
    case EXP_FUNCCALL: {
        Reg *args[6];
        for (int i = 0; i < VectorSize(exp->Exps); i++)
            args[i] = genExp(VectorGet(exp->Exps, i));

        IR *ir = NewIR(IR_CALL);
        ir->r0 = newReg();
        ir->name = exp->Name;
        ir->nargs = VectorSize(exp->Exps);
        memcpy(ir->args, args, sizeof(args));
        return ir->r0;
    }
    case EXP_ADDR:
        return genLeftValue(exp->Exp1);
    case EXP_DEREF: {
        Reg *r = newReg();
        emitLoad(exp, r, genExp(exp->Exp1));
        return r;
    }
    // case EXP_CAST: {
    //     Reg *r1 = genExp(exp->expr);
    //     if (exp->ty->ty != BOOL)
    //         return r1;
    //     Reg *r2 = newReg();
    //     emitIR(IR_NE, r2, r1, emitImm(0));
    //     return r2;
    // }
    case EXP_STMT:
        for (int i = 0; i < VectorSize(exp->Stmts); i++)
            genStmt(exp->Stmts->data[i]);
        return genExp(exp->Exp1);
    case EXP_ASSIGN: {
        Reg *r1 = genExp(exp->Exp2);
        Reg *r2 = genLeftValue(exp->Exp1);

        IR *ir = emitIR(IR_STORE, NULL, r2, r1);
        ir->size = exp->ctype->Size;
        return r1;
    }
    case EXP_COND: {
        BB *then = NewBB();
        BB *els = NewBB();
        BB *last = NewBB();

        emitBR(genExp(exp->Cond), then, els);

        out = then;
        emitJmpArg(last, genExp(exp->Exp1));

        out = els;
        emitJmpArg(last, genExp(exp->Exp2));

        out = last;
        out->param = newReg();
        return out->param;
    }
    default:
        assert(0 && "unknown AST type");
    }
}

void genStmt(Statement *stmt) {
    switch (stmt->ty) {
    case STMT_NULL:
        return;
    case STMT_IF: {
        BB *then = NewBB();
        BB *els = NewBB();
        BB *last = NewBB();

        emitBR(genExp(stmt->Cond), then, els);

        out = then;
        genStmt(stmt->Body);
        emitJmp(last);

        out = els;
        if (stmt->Else) genStmt(stmt->Else);
        emitJmp(last);

        out = last;
        return;
    }
    case STMT_FOR: {
        BB *cond = NewBB();
        stmt->Continue = NewBB();
        BB *body = NewBB();
        stmt->Break = NewBB();

        if (stmt->Init) genExp(stmt->Init);
        emitJmp(cond);

        out = cond;
        if (stmt->Cond) {
            Reg *r = genExp(stmt->Cond);
            emitBR(r, body, stmt->Break);
        } else {
            emitJmp(body);
        }

        out = body;
        genStmt(stmt->Body);
        emitJmp(stmt->Continue);

        out = stmt->Continue;
        if (stmt->Step) genExp(stmt->Step);
        emitJmp(cond);

        out = stmt->Break;
        return;
    }
    case STMT_DO_WHILE: {
        stmt->Continue = NewBB();
        BB *body = NewBB();
        stmt->Break = NewBB();

        emitJmp(body);

        out = body;
        genStmt(stmt->Body);
        emitJmp(stmt->Continue);

        out = stmt->Continue;
        Reg *r = genExp(stmt->Cond);
        emitBR(r, body, stmt->Break);

        out = stmt->Break;
        return;
    }
    case STMT_SWITCH: {
        stmt->Break = NewBB();
        stmt->Continue = NewBB();

        Reg *r = genExp(stmt->Cond);
        for (int i = 0; i < VectorSize(stmt->Cases); i++) {
            Statement *Case = VectorGet(stmt->Cases, i);
            Case->bb = NewBB();

            BB *next = NewBB();
            Reg *r2 = newReg();
            emitIR(IR_EQ, r2, r, emitImm(Case->Cond->Val));
            emitBR(r2, Case->bb, next);
            out = next;
        }
        emitJmp(stmt->Break);

        genStmt(stmt->Body);
        emitJmp(stmt->Break);

        out = stmt->Break;
        return;
    }
    case STMT_CASE:
        emitJmp(stmt->bb);
        out = stmt->bb;
        genStmt(stmt->Body);
        break;
    case STMT_BREAK:
        emitJmp(stmt->Body->Break);
        out = NewBB();
        break;
    case STMT_CONTINUE:
        emitJmp(stmt->Body->Continue);
        out = NewBB();
        break;
    case STMT_RETURN: {
        Reg *r = genExp(stmt->Exp);
        IR *ir = NewIR(IR_RETURN);
        ir->r2 = r;
        out = NewBB();
        return;
    }
    case STMT_EXP:
        genExp(stmt->Exp);
        return;
    case STMT_COMP:
        for (int i = 0; i < VectorSize(stmt->Stmts); i++)
            genStmt(VectorGet(stmt->Stmts, i));
        return;
    default:
        assert(0 && "unknown stmt");
    }
}

static void genParam(Var *var, int i) {
    IR *ir = NewIR(IR_STORE_ARG);
    ir->var = var;
    ir->imm = i;
    ir->size = var->ty->Size;
    var->address_taken = 1;
}

void genProgram(Program *program) {
    for (int i = 0; i < VectorSize(program->Functions); i++) {
        fn = VectorGet(program->Functions, i);

        // Add an empty entry BB to make later analysis easy.
        out = NewBB();
        BB *bb = NewBB();
        emitJmp(bb);
        out = bb;

        // Emit IR.
        Vector *params = fn->Params;
        for (int i = 0; i < params->len; i++) {
            genParam(params->data[i], i);
        }

        genStmt(fn->Stmt);

        // Make it always ends with a return to make later analysis easy.
        NewIR(IR_RETURN)->r2 = emitImm(0);

        // Later passes shouldn't need the AST, so make it explicit.
        fn->Stmt = NULL;
    }
}

// Rewrite
//
//  BPREL r1, <offset>
//  STORE r1, r2
//  LOAD  r3, r1
//
// to
//
//  NOP
//  r4 = r2
//  r3 = r4
void optimize(IR *ir) {
    if (ir->op == IR_BPREL) {
        Var *var = ir->var;
        if (var->address_taken || var->ty->ty != INT)
            return;

        if (!var->promoted)
            var->promoted = newReg();

        ir->op = IR_NOP;
        ir->r0->promoted = var->promoted;
        return;
    }

    if (ir->op == IR_LOAD) {
        if (!ir->r2->promoted)
            return;
        ir->op = IR_MOV;
        ir->r2 = ir->r2->promoted;
        return;
    }

    if (ir->op == IR_STORE) {
        if (!ir->r1->promoted)
            return;
        ir->op = IR_MOV;
        ir->r0 = ir->r1->promoted;
        ir->r1 = NULL;
        return;
    }
}

void Optimize(Program *program) {
    for (int i = 0; i < VectorSize(program->Functions); i++) {
        Function *fn = VectorGet(program->Functions, i);
        for (int i = 0; i < VectorSize(fn->bbs); i++) {
            BB *bb = VectorGet(fn->bbs, i);
            for (int i = 0; i < VectorSize(bb->ir); i++) {
                optimize(VectorGet(bb->ir, i));
            }
        }
    }
}
