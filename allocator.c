// Linear scan register allocator.
//
// Before this pass, it is assumed that we have infinite number of
// registers. This pass maps them to finite number of registers.
// Here is the algorithm:
//
// First, we find the definition and the last use for each register.
// A register is considered "live" in the range. At the definition of
// some register R, if all physical registers are already allocated,
// one of them (including R itself) needs to be spilled to the stack.
// As long as one register is spilled, the algorithm is logically
// correct. As a heuristic, we spill a register whose last use is
// furthest.
//
// We then insert load and store instructions for spilled registesr.
// The last register (num_regs-1'th register) is reserved for that
// purpose.

#include "allocator.h"
#include <assert.h>
#include <stdlib.h>

// Rewrite `A = B op C` to `A = B; A = A ty C`.
void optimizeAssign(BB *bb) {
    Vector *v = NewVector();

    for (int i = 0; i < VectorSize(bb->IRs); i++) {
        IR *ir = VectorGet(bb->IRs, i);

        if (!ir->r0 || !ir->r1) {
            VectorPush(v, ir);
            continue;
        }

        assert(ir->r0 != ir->r1);

        IR *ir2 = calloc(1, sizeof(IR));
        ir2->ty = IR_MOV;
        ir2->r0 = ir->r0;
        ir2->r2 = ir->r1;
        VectorPush(v, ir2);

        ir->r1 = ir->r0;
        VectorPush(v, ir);
    }
    bb->IRs = v;
}

static void setLastUse(Reg *r, int ic) {
    if (r && r->LastUse < ic) {
        r->LastUse = ic;
    }
}

static Vector *collectRegs(Function *fn) {
    Vector *v = NewVector();
    int ic = 1; // instruction counter

    for (int i = 0; i < VectorSize(fn->bbs); i++) {
        BB *bb = VectorGet(fn->bbs, i);

        if (bb->Param) {
            bb->Param->Def = ic;
            VectorPush(v, bb->Param);
        }

        for (int i = 0; i < VectorSize(bb->IRs); i++, ic++) {
            IR *ir = VectorGet(bb->IRs, i);

            if (ir->r0 && !ir->r0->Def) {
                ir->r0->Def = ic;
                VectorPush(v, ir->r0);
            }

            setLastUse(ir->r1, ic);
            setLastUse(ir->r2, ic);
            setLastUse(ir->bbArg, ic);

            if (ir->ty == IR_CALL) {
                for (int i = 0; i < ir->NArgs; i++) {
                    setLastUse(ir->Args[i], ic);
                }
            }
        }

        for (int i = 0; i < VectorSize(bb->OutRegs); i++) {
            Reg *r = VectorGet(bb->OutRegs, i);
            setLastUse(r, ic);
        }
    }

    return v;
}

int chooseToSpill(Reg **used) {
    int k = 0;
    for (int i = 1; i < num_regs; i++) {
        if (used[k]->LastUse < used[i]->LastUse) {
            k = i;
        }
    }
    return k;
}

// Allocate registers.
static void scan(Vector *regs) {
    Reg **used = calloc(num_regs, sizeof(Reg *));

    for (int i = 0; i < regs->len; i++) {
        Reg *r = regs->data[i];

        // Find an unused slot.
        int found = 0;
        for (int i = 0; i < num_regs - 1; i++) {
            if (used[i] && r->Def < used[i]->LastUse) {
                continue;
            }
            r->RealNum = i;
            used[i] = r;
            found = 1;
            break;
        }

        if (found) {
            continue;
        }

        // Choose a register to spill and mark it as "spilled".
        used[num_regs - 1] = r;
        int k = chooseToSpill(used);

        r->RealNum = k;
        used[k]->RealNum = num_regs - 1;
        used[k]->Spill = 1;
        used[k] = r;
    }
}

static void spillStore(Vector *v, IR *ir) {
    Reg *r = ir->r0;
    if (!r || !r->Spill) {
        return;
    }

    IR *ir2 = calloc(1, sizeof(IR));
    ir2->ty = IR_STORE_SPILL;
    ir2->r1 = r;
    ir2->ID = r->ID;
    VectorPush(v, ir2);
}

static void spillLoad(Vector *v, IR *ir, Reg *r) {
    if (!r || !r->Spill) {
        return;
    }

    IR *ir2 = calloc(1, sizeof(IR));
    ir2->ty = IR_LOAD_SPILL;
    ir2->r0 = r;
    ir2->ID = r->ID;
    VectorPush(v, ir2);
}

static void emitSpill(BB *bb) {
    Vector *v = NewVector();

    for (int i = 0; i < VectorSize(bb->IRs); i++) {
        IR *ir = VectorGet(bb->IRs, i);

        spillLoad(v, ir, ir->r1);
        spillLoad(v, ir, ir->r2);
        spillLoad(v, ir, ir->bbArg);
        VectorPush(v, ir);
        spillStore(v, ir);
    }
    bb->IRs = v;
}

void Allocate(Program *prog) {
    for (int i = 0; i < VectorSize(prog->Functions); i++) {
        Function *fn = VectorGet(prog->Functions, i);

        // Convert SSA to x86-ish two-address form.
        for (int i = 0; i < VectorSize(fn->bbs); i++) {
            BB *bb = VectorGet(fn->bbs, i);
            optimizeAssign(bb);
        }

        // Allocate registers and decide which registers to spill.
        Vector *regs = collectRegs(fn);
        scan(regs);

        // Reserve a stack area for spilled registers.
        for (int i = 0; i < VectorSize(regs); i++) {
            Reg *r = VectorGet(regs, i);
            if (!r->Spill)
                continue;

            Var *ID = calloc(1, sizeof(Var));
            ID->ty = PtrTo(&IntType);
            ID->Local = 1;
            ID->Name = "spill";

            r->ID = ID;
            VectorPush(fn->LocalVars, ID);
        }

        // Convert accesses to spilled registers to loads and stores.
        for (int i = 0; i < fn->bbs->len; i++) {
            BB *bb = fn->bbs->data[i];
            emitSpill(bb);
        }
    }
}
