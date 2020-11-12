#include "gen_x86.h"
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

// This pass generates x86-64 assembly from IR.

int roundup(int x, int align) {
    return (x + align - 1) & ~(align - 1);
}

char *regs[] = {"r10", "r11", "rbx", "r12", "r13", "r14", "r15"};
char *regs8[] = {"r10b", "r11b", "bl", "r12b", "r13b", "r14b", "r15b"};
char *regs32[] = {"r10d", "r11d", "ebx", "r12d", "r13d", "r14d", "r15d"};

int num_regs = sizeof(regs) / sizeof(*regs);

char *argregs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
char *argregs8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
char *argregs32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};

__attribute__((format(printf, 1, 2))) void p(char *fmt, ...);
__attribute__((format(printf, 1, 2))) void emit(char *fmt, ...);

void p(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    printf("\n");
}

void emit(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("\t");
    vprintf(fmt, ap);
    printf("\n");
}

void emit_cmp(char *insn, IR *ir) {
    int r0 = ir->r0->RealNum;
    int r1 = ir->r1->RealNum;
    int r2 = ir->r2->RealNum;

    emit("cmp %s, %s", regs[r1], regs[r2]);
    emit("%s %s", insn, regs8[r0]);
    emit("movzb %s, %s", regs[r0], regs8[r0]);
}

char *reg(int r, int size) {
    if (size == 1)
        return regs8[r];
    if (size == 4)
        return regs32[r];
    assert(size == 8);
    return regs[r];
}

char *argreg(int r, int size) {
    if (size == 1)
        return argregs8[r];
    if (size == 4)
        return argregs32[r];
    assert(size == 8);
    return argregs[r];
}

void emit_ir(IR *ir, char *ret) {
    int r0 = ir->r0 ? ir->r0->RealNum : 0;
    int r1 = ir->r1 ? ir->r1->RealNum : 0;
    int r2 = ir->r2 ? ir->r2->RealNum : 0;

    switch (ir->ty) {
    case IR_IMM:
        emit("mov %s, %d", regs[r0], ir->imm);
        break;
    case IR_BPREL:
        emit("lea %s, [rbp%d]", regs[r0], ir->ID->Offset);
        break;
    case IR_MOV:
        emit("mov %s, %s", regs[r0], regs[r2]);
        break;
    case IR_RETURN:
        emit("mov rax, %s", regs[r2]);
        emit("jmp %s", ret);
        break;
    case IR_CALL:
        for (int i = 0; i < ir->NArgs; i++)
            emit("mov %s, %s", argregs[i], regs[ir->Args[i]->RealNum]);

        emit("push r10");
        emit("push r11");
        emit("mov rax, 0");
        emit("call %s", ir->Name);
        emit("pop r11");
        emit("pop r10");
        emit("mov %s, rax", regs[r0]);
        break;
    case IR_LABEL_ADDR:
        emit("lea %s, %s", regs[r0], ir->Name);
        break;
    case IR_EQ:
        emit_cmp("sete", ir);
        break;
    case IR_NE:
        emit_cmp("setne", ir);
        break;
    case IR_LT:
        emit_cmp("setl", ir);
        break;
    case IR_LE:
        emit_cmp("setle", ir);
        break;
    case IR_AND:
        emit("and %s, %s", regs[r0], regs[r2]);
        break;
    case IR_OR:
        emit("or %s, %s", regs[r0], regs[r2]);
        break;
    case IR_XOR:
        emit("xor %s, %s", regs[r0], regs[r2]);
        break;
    case IR_SHL:
        emit("mov cl, %s", regs8[r2]);
        emit("shl %s, cl", regs[r0]);
        break;
    case IR_SHR:
        emit("mov cl, %s", regs8[r2]);
        emit("shr %s, cl", regs[r0]);
        break;
    case IR_JMP:
        if (ir->bbArg)
            emit("mov %s, %s", regs[ir->bb1->Param->RealNum], regs[ir->bbArg->RealNum]);
        emit("jmp .L%d", ir->bb1->Label);
        break;
    case IR_TEST:
        emit("cmp %s, 0", regs[r2]);
        emit("jne .L%d", ir->bb1->Label);
        emit("jmp .L%d", ir->bb2->Label);
        break;
    case IR_LOAD:
        emit("mov %s, [%s]", reg(r0, ir->Size), regs[r2]);
        if (ir->Size == 1)
            emit("movzb %s, %s", regs[r0], regs8[r0]);
        break;
    case IR_LOAD_SPILL:
        emit("mov %s, [rbp%d]", regs[r0], ir->ID->Offset);
        break;
    case IR_STORE:
        emit("mov [%s], %s", regs[r1], reg(r2, ir->Size));
        break;
    case IR_STORE_ARG:
        emit("mov [rbp%d], %s", ir->ID->Offset, argreg(ir->imm, ir->Size));
        break;
    case IR_STORE_SPILL:
        emit("mov [rbp%d], %s", ir->ID->Offset, regs[r1]);
        break;
    case IR_ADD:
        emit("add %s, %s", regs[r0], regs[r2]);
        break;
    case IR_SUB:
        emit("sub %s, %s", regs[r0], regs[r2]);
        break;
    case IR_MUL:
        emit("mov rax, %s", regs[r2]);
        emit("imul %s", regs[r0]);
        emit("mov %s, rax", regs[r0]);
        break;
    case IR_DIV:
        emit("mov rax, %s", regs[r0]);
        emit("cqo");
        emit("idiv %s", regs[r2]);
        emit("mov %s, rax", regs[r0]);
        break;
    case IR_MOD:
        emit("mov rax, %s", regs[r0]);
        emit("cqo");
        emit("idiv %s", regs[r2]);
        emit("mov %s, rdx", regs[r0]);
        break;
    case IR_NOP:
        break;
    default:
        assert(0 && "unknown operator");
    }
}

void emit_code(Function *fn) {
    // Assign an offset from RBP to each local variable.
    int off = 0;
    for (int i = 0; i < fn->LocalVars->len; i++) {
        Var *ID = fn->LocalVars->data[i];
        off += ID->ty->Size;
        off = roundup(off, ID->ty->Align);
        // off = roundup(off, ID->ty->Size);
        ID->Offset = -off;
    }

    // Emit assembly
    char *ret = Format(".Lend%d", nLabel++);

    p(".text");
    p(".global %s", fn->Name);
    p("%s:", fn->Name);
    emit("push rbp");
    emit("mov rbp, rsp");
    emit("sub rsp, %d", roundup(off, 16));
    emit("push r12");
    emit("push r13");
    emit("push r14");
    emit("push r15");

    for (int i = 0; i < VectorSize(fn->bbs); i++) {
        BB *bb = VectorGet(fn->bbs, i);
        p(".L%d:", bb->Label);
        for (int i = 0; i < VectorSize(bb->IRs); i++) {
            IR *ir = VectorGet(bb->IRs, i);
            emit_ir(ir, ret);
        }
    }

    p("%s:", ret);
    emit("pop r15");
    emit("pop r14");
    emit("pop r13");
    emit("pop r12");
    emit("mov rsp, rbp");
    emit("pop rbp");
    emit("ret");
}

char *backslash_escape(char *s, int len) {
    char escaped[256] = {
        ['\b'] = 'b',
        ['\f'] = 'f',
        ['\n'] = 'n',
        ['\r'] = 'r',
        ['\t'] = 't',
        ['\\'] = '\\',
        ['\''] = '\'',
        ['"'] = '"',
    };

    StringBuilder *sb = NewStringBuilder();
    for (int i = 0; i < len; i++) {
        char c = s[i];
        char esc = escaped[c];
        if (esc) {
            StringBuilderAdd(sb, '\\');
            StringBuilderAdd(sb, esc);
        }
        else if (isgraph(c) || c == ' ') {
            StringBuilderAdd(sb, c);
        } else {
            StringBuilderAppend(sb, Format("\\%03o", c));
        }
    }
    return StringBuilderToString(sb);
}

void emit_data(Var *ID) {
    if (ID->Data) {
        p(".data");
        p("%s:", ID->Name);
        emit(".ascii \"%s\"", backslash_escape(ID->Data, ID->ty->Size));
        return;
    }

    p(".bss");
    p("%s:", ID->Name);
    emit(".zero %d", ID->ty->Size);
}

void Genx86(Program *prog) {
    p(".intel_syntax noprefix");

    for (int i = 0; i < VectorSize(prog->GlobalVars); i++)
        emit_data(VectorGet(prog->GlobalVars, i));

    for (int i = 0; i < prog->Functions->len; i++)
        emit_code(VectorGet(prog->Functions, i));
}
