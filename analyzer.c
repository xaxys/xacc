// Liveness analysis.
#include "analyzer.h"
#include "util.h"
#include <assert.h>
#include <stdlib.h>

// Fill bb->succ and bb->pred.
void addEdges(BB *bb) {
    if (VectorSize(bb->Succ) > 0) return;
    assert(bb->IRs->len);

    IR *ir = VectorLast(bb->IRs);

    if (ir->bb1) {
        VectorPush(bb->Succ, ir->bb1);
        VectorPush(ir->bb1->Pred, bb);
        addEdges(ir->bb1);
    }

    if (ir->bb2) {
        VectorPush(bb->Succ, ir->bb2);
        VectorPush(ir->bb2->Pred, bb);
        addEdges(ir->bb2);
    }
}

// Initializes bb->def_regs.
void setDefRegs(BB *bb) {
    if (bb->Param) {
        VectorUnion(bb->DefRegs, bb->Param);
    }

    for (int i = 0; i < VectorSize(bb->IRs); i++) {
        IR *ir = VectorGet(bb->IRs, i);
        if (ir->r0) {
            VectorUnion(bb->DefRegs, ir->r0);
        }
    }
}

// Back-propagate r in the call flow graph.
void propagate(BB *bb, Reg *r) {
    if (!r || VectorContain(bb->DefRegs, r)) return;

    if (!VectorUnion(bb->InRegs, r)) return;

    for (int i = 0; i < VectorSize(bb->Pred); i++) {
        BB *pred = VectorGet(bb->Pred, i);
        if (VectorUnion(pred->OutRegs, r)) {
            propagate(pred, r);
        }
    }
}

// Initializes bb->in_regs and bb->out_regs.
void visit(BB *bb, IR *ir) {
    propagate(bb, ir->r1);
    propagate(bb, ir->r2);
    propagate(bb, ir->bbArg);

    if (ir->ty == IR_CALL) {
        for (int i = 0; i < ir->NArgs; i++) {
            propagate(bb, ir->Args[i]);
        }
    }
}

void Analyze(Program *program) {
    for (int i = 0; i < VectorSize(program->Functions); i++) {
        Function *fn = VectorGet(program->Functions, i);
        addEdges(VectorGet(fn->bbs, 0));

        for (int i = 0; i < VectorSize(fn->bbs); i++) {
            BB *bb = VectorGet(fn->bbs, i);
            setDefRegs(bb);

            for (int i = 0; i < VectorSize(bb->IRs); i++) {
                IR *ir = VectorGet(bb->IRs, i);
                visit(bb, ir);
            }
        }

        // Incoming registers of the entry BB correspond to
        // uninitialized variables in a program.
        // Add dummy definitions to make later analysis easy.
        BB *ent = VectorGet(fn->bbs, 0);
        for (int i = 0; i < VectorSize(ent->InRegs); i++) {
            Reg *r = VectorGet(ent->InRegs, i);
            IR *ir = calloc(1, sizeof(IR));
            ir->ty = IR_MOV;
            ir->r0 = r;
            ir->imm = 0;
            VectorPush(ent->IRs, ir);
            VectorPush(ent->DefRegs, r);
        }
        ent->InRegs = NewVector();
    }
}
