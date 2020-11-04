// Liveness analysis.
#include "analyzer.h"
#include "util.h"
#include <assert.h>
#include <stdlib.h>

// Fill bb->succ and bb->pred.
void addEdges(BB *bb) {
    if (VectorSize(bb->succ) > 0) return;
    assert(bb->ir->len);

    IR *ir = VectorLast(bb->ir);

    if (ir->bb1) {
        VectorPush(bb->succ, ir->bb1);
        VectorPush(ir->bb1->pred, bb);
        addEdges(ir->bb1);
    }

    if (ir->bb2) {
        VectorPush(bb->succ, ir->bb2);
        VectorPush(ir->bb2->pred, bb);
        addEdges(ir->bb2);
    }
}

// Initializes bb->def_regs.
void set_def_regs(BB *bb) {
    if (bb->param) {
        VectorUnion(bb->def_regs, bb->param);
    }

    for (int i = 0; i < VectorSize(bb->ir); i++) {
        IR *ir = VectorGet(bb->ir, i);
        if (ir->r0) {
            VectorUnion(bb->def_regs, ir->r0);
        }
    }
}

// Back-propagate r in the call flow graph.
void propagate(BB *bb, Reg *r) {
    if (!r || VectorContain(bb->def_regs, r)) return;

    if (!VectorUnion(bb->in_regs, r)) return;

    for (int i = 0; i < VectorSize(bb->pred); i++) {
        BB *pred = VectorGet(bb->pred, i);
        if (VectorUnion(pred->out_regs, r)) {
            propagate(pred, r);
        }
    }
}

// Initializes bb->in_regs and bb->out_regs.
void visit(BB *bb, IR *ir) {
    propagate(bb, ir->r1);
    propagate(bb, ir->r2);
    propagate(bb, ir->bbarg);

    if (ir->op == IR_CALL) {
        for (int i = 0; i < ir->nargs; i++) {
            propagate(bb, ir->args[i]);
        }
    }
}

void Analyze(Program *program) {
    for (int i = 0; i < VectorSize(program->Functions); i++) {
        Function *fn = VectorGet(program->Functions, i);
        addEdges(VectorGet(fn->bbs, 0));

        for (int i = 0; i < VectorSize(fn->bbs); i++) {
            BB *bb = VectorGet(fn->bbs, i);
            set_def_regs(bb);

            for (int i = 0; i < VectorSize(bb->ir); i++) {
                IR *ir = VectorGet(bb->ir, i);
                visit(bb, ir);
            }
        }

        // Incoming registers of the entry BB correspond to
        // uninitialized variables in a program.
        // Add dummy definitions to make later analysis easy.
        BB *ent = VectorGet(fn->bbs, 0);
        for (int i = 0; i < VectorSize(ent->in_regs); i++) {
            Reg *r = VectorGet(ent->in_regs, i);
            IR *ir = calloc(1, sizeof(IR));
            ir->op = IR_MOV;
            ir->r0 = r;
            ir->imm = 0;
            VectorPush(ent->ir, ir);
            VectorPush(ent->def_regs, r);
        }
        ent->in_regs = NewVector();
    }
}
