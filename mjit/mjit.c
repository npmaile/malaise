/*
 * mjit.c — a bytecode virtual machine and loop JIT for a subset of Malaise.
 *
 * This is not part of interpreter/malaise.c and does not modify it. It is a
 * second, much smaller implementation: a two-pass assembler-style compiler
 * from a restricted Malaise dialect into a flat bytecode array, a bytecode
 * interpreter (the VM's baseline tier), and a loop JIT that recognizes
 * exactly one shape (a backward conditional jump whose body is pure integer
 * arithmetic) and compiles it — not to machine code, to a program for a
 * second, smaller virtual machine, also written in C, whose only instructions
 * are the ones a compiled loop can contain. No mmap, no instruction
 * encoding, no architecture: the compilation target is a `switch` statement,
 * same as the tier it's replacing, just a much shorter one running over
 * pre-resolved pointers instead of the general interpreter's slot indices
 * and immediate-or-slot branches. Whether that still earns the name "JIT" is
 * exactly the kind of question this project doesn't resolve in its own
 * favor; see mjit/README.md. Everything else stays on the bytecode tier
 * forever.
 *
 * The dialect: $-sigiled integer variables (name must start i-n, invariant
 * 10 — enforced here at COMPILE time, not coerced silently the way the
 * reference interpreter's lint pass does it), the same label convention
 * (columns 1-6, `*` at column 7 for a comment, code from column 8), GOTO,
 * PRINT of exactly one value, HALT, and a single-line
 * `IF $var <relop> value-or-var GOTO label` — which is not the reference
 * interpreter's IF (that one is a THEN/ELSE/ENDIF block). A trace compiler
 * only ever compiles a straight run of instructions between a loop header
 * and one back-edge; block control flow doesn't reduce to that, so mjit's
 * frontend doesn't parse it, on purpose. Two implementations of the same
 * language, syntactically incompatible on exactly the one construct that
 * would matter — see mjit/README.md, and see malpack/grieve for precedent.
 *
 * Exit codes follow interpreter/malaise.c invariant 1: 1 is success, 2 is
 * the first error. Unlike the reference interpreter, nothing here is
 * forgiven: an unrecognized line is a compile error, not a warning.
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXSRC    4096
#define MAXLINE   2048
#define MAXCODE   4096
#define MAXVARS   512
#define MAXLABELS 512

/* ---------------------------------------------------------------- bytecode */

typedef enum {
    OP_HALT, OP_MOVI, OP_MOV, OP_ADD, OP_SUB, OP_MUL,
    OP_PRINT, OP_PRINTI, OP_GOTO, OP_IFJMP
} Op;

typedef enum { REL_LT, REL_GT, REL_LE, REL_GE, REL_EQ, REL_NE } Rel;

typedef struct {
    Op op;
    int dst, src1, src2;   /* slot indices; meaning depends on op */
    long imm;              /* immediate operand, when b_is_imm */
    int b_is_imm;
    int target;             /* bytecode pc, for OP_GOTO / OP_IFJMP */
    Rel rel;                /* for OP_IFJMP */
    int srcline;             /* 1-based source line, for diagnostics */
} Instr;

static Instr code[MAXCODE];
static int   ncode = 0;

static char varnames[MAXVARS][64];
static int  nvars = 0;
static long slots[MAXVARS];   /* the VM's variables. fixed array, no malloc,
                                  same policy as interpreter/malaise.c. */

typedef struct { char name[16]; int pc; } Label;
static Label labels[MAXLABELS];
static int   nlabels = 0;

static char srcraw[MAXSRC][MAXLINE];
static int  nsrc = 0;

static int  jit_threshold = 41;  /* default; see mrfc's 41-month RFC delay.
                                     unrelated, allegedly. override with
                                     MALAISE_JIT_THRESHOLD. */

/* ------------------------------------------------------------------ misc */

static void compile_err(int srcline, const char *msg) {
    printf("mjit: line %d: %s\n", srcline, msg);
    exit(2);  /* 2 is the first error (interpreter/malaise.c invariant 1) */
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t l = strlen(s);
    while (l > 0 && (s[l-1] == ' ' || s[l-1] == '\t' || s[l-1] == '\r')) s[--l] = 0;
    return s;
}

static int findlabel(const char *name) {
    for (int i = 0; i < nlabels; i++)
        if (strcasecmp(labels[i].name, name) == 0) return labels[i].pc;
    return -1;
}

static int getslot(const char *name, int srcline) {
    for (int i = 0; i < nvars; i++)
        if (strcasecmp(varnames[i], name) == 0) return i;
    char f = name[0];
    if (!((f >= 'i' && f <= 'n') || (f >= 'I' && f <= 'N'))) {
        char msg[160];
        snprintf(msg, sizeof msg,
            "$%s doesn't start with i-n; mjit has no variable that isn't an "
            "implicit int (see interpreter/malaise.c invariant 10)", name);
        compile_err(srcline, msg);
    }
    if (nvars >= MAXVARS) compile_err(srcline, "too many variables");
    snprintf(varnames[nvars], sizeof varnames[nvars], "%s", name);
    return nvars++;
}

/* columns: 1-6 label, 7 indicator ('*' = comment), 8+ code. Same convention
   as interpreter/malaise.c, minus the tab-parity and column-72 rules — a
   second frontend that reused every lexer quirk wouldn't be a second
   implementation, it'd be a fork. */
static void split_line(const char *raw, char *label_out, size_t labelcap,
                        int *is_comment, char *code_out, size_t codecap) {
    size_t len = strlen(raw);
    char lbl[7];
    for (int i = 0; i < 6; i++) lbl[i] = ((size_t)i < len) ? raw[i] : ' ';
    lbl[6] = 0;
    int e = 5;
    while (e >= 0 && lbl[e] == ' ') { lbl[e] = 0; e--; }
    snprintf(label_out, labelcap, "%s", lbl);
    char ind = ((size_t)6 < len) ? raw[6] : ' ';
    *is_comment = (ind == '*');
    const char *codestart = ((size_t)7 < len) ? raw + 7 : "";
    snprintf(code_out, codecap, "%s", codestart);
}

/* --------------------------------------------------------------- lexer */

typedef enum { TK_EOF, TK_ID, TK_VAR, TK_NUM, TK_OP } TKind;
typedef struct { TKind k; char text[64]; long num; } MTok;

/* returns token count, or -1 (too many tokens) / -2 (unrecognized input) */
static int mtokenize(const char *s, MTok *out, int maxtoks) {
    int n = 0;
    const char *p = s;
    while (*p) {
        if (isspace((unsigned char)*p)) { p++; continue; }
        if (n >= maxtoks) return -1;
        if (*p == '$') {
            p++;
            char buf[64]; int i = 0;
            while ((isalnum((unsigned char)*p) || *p == '_') && i < 63) buf[i++] = *p++;
            buf[i] = 0;
            if (i == 0) return -2;
            out[n].k = TK_VAR; snprintf(out[n].text, sizeof out[n].text, "%s", buf); n++;
            continue;
        }
        if (isdigit((unsigned char)*p)) {
            char buf[32]; int i = 0;
            while (isdigit((unsigned char)*p) && i < 31) buf[i++] = *p++;
            buf[i] = 0;
            out[n].k = TK_NUM; out[n].num = strtol(buf, NULL, 10); n++;
            continue;
        }
        if (isalpha((unsigned char)*p) || *p == '_') {
            char buf[64]; int i = 0;
            while ((isalnum((unsigned char)*p) || *p == '_') && i < 63) buf[i++] = *p++;
            buf[i] = 0;
            out[n].k = TK_ID; snprintf(out[n].text, sizeof out[n].text, "%s", buf); n++;
            continue;
        }
        if ((p[0]=='<'||p[0]=='>'||p[0]=='='||p[0]=='!') && p[1]=='=') {
            out[n].k = TK_OP; out[n].text[0]=p[0]; out[n].text[1]='='; out[n].text[2]=0; n++;
            p += 2; continue;
        }
        if (strchr("<>=+-*", *p)) {
            out[n].k = TK_OP; out[n].text[0]=*p; out[n].text[1]=0; n++;
            p++; continue;
        }
        return -2;
    }
    if (n < maxtoks) out[n].k = TK_EOF;
    return n;
}

/* ------------------------------------------------------------------ parser */

#define ISID(t,s)  ((t)->k==TK_ID  && strcasecmp((t)->text,(s))==0)
#define ISOP(t,s)  ((t)->k==TK_OP  && strcmp((t)->text,(s))==0)

static void parse_statement(const char *text, int pc, int srcline) {
    MTok toks[33];
    int nt = mtokenize(text, toks, 32);
    if (nt == -1) compile_err(srcline, "too many tokens on one line");
    if (nt == -2) compile_err(srcline, "unrecognized character (mjit's grammar is small; see mjit/README.md)");
    if (nt == 0)  compile_err(srcline, "empty statement");

    int tp = 0;
    Instr ins; memset(&ins, 0, sizeof ins); ins.srcline = srcline;

    if (ISID(&toks[tp], "HALT")) {
        tp++;
        if (tp != nt) compile_err(srcline, "HALT takes no arguments");
        ins.op = OP_HALT; code[pc] = ins; return;
    }

    if (ISID(&toks[tp], "GOTO")) {
        tp++;
        if (tp >= nt || toks[tp].k != TK_ID) compile_err(srcline, "GOTO needs a label");
        int lbl = findlabel(toks[tp].text);
        if (lbl < 0) compile_err(srcline, "GOTO target label not found");
        tp++;
        if (tp != nt) compile_err(srcline, "unexpected tokens after GOTO's label");
        ins.op = OP_GOTO; ins.target = lbl; code[pc] = ins; return;
    }

    if (ISID(&toks[tp], "PRINT")) {
        tp++;
        if (tp >= nt) compile_err(srcline, "PRINT needs a value");
        if (toks[tp].k == TK_NUM)      { ins.op = OP_PRINTI; ins.imm = toks[tp].num; tp++; }
        else if (toks[tp].k == TK_VAR) { ins.op = OP_PRINT;  ins.src1 = getslot(toks[tp].text, srcline); tp++; }
        else compile_err(srcline, "PRINT needs a number or a $variable");
        if (tp != nt) compile_err(srcline, "PRINT takes exactly one value (unlike the reference interpreter's comma list)");
        code[pc] = ins; return;
    }

    if (ISID(&toks[tp], "IF")) {
        tp++;
        if (tp >= nt || toks[tp].k != TK_VAR) compile_err(srcline, "IF's left-hand side must be a $variable");
        int leftslot = getslot(toks[tp].text, srcline); tp++;
        if (tp >= nt || toks[tp].k != TK_OP) compile_err(srcline, "IF needs a comparison operator");
        Rel rel;
        const char *o = toks[tp].text;
        if      (!strcmp(o, "<"))  rel = REL_LT;
        else if (!strcmp(o, ">"))  rel = REL_GT;
        else if (!strcmp(o, "<=")) rel = REL_LE;
        else if (!strcmp(o, ">=")) rel = REL_GE;
        else if (!strcmp(o, "==")) rel = REL_EQ;
        else if (!strcmp(o, "!=")) rel = REL_NE;
        else { compile_err(srcline, "unsupported comparison (mjit has < > <= >= == != and nothing else)"); return; }
        tp++;
        if (tp >= nt) compile_err(srcline, "IF needs a right-hand value");
        int b_is_imm = 0, src2 = 0; long imm = 0;
        if (toks[tp].k == TK_NUM)      { b_is_imm = 1; imm = toks[tp].num; tp++; }
        else if (toks[tp].k == TK_VAR) { src2 = getslot(toks[tp].text, srcline); tp++; }
        else compile_err(srcline, "IF's right-hand side must be a number or a $variable");
        if (tp >= nt || !ISID(&toks[tp], "GOTO")) compile_err(srcline, "IF's comparison must be followed by GOTO label");
        tp++;
        if (tp >= nt || toks[tp].k != TK_ID) compile_err(srcline, "GOTO needs a label");
        int lbl = findlabel(toks[tp].text);
        if (lbl < 0) compile_err(srcline, "IF...GOTO target label not found");
        tp++;
        if (tp != nt) compile_err(srcline, "unexpected tokens after IF...GOTO");
        ins.op = OP_IFJMP; ins.src1 = leftslot; ins.b_is_imm = b_is_imm;
        ins.src2 = src2; ins.imm = imm; ins.rel = rel; ins.target = lbl;
        code[pc] = ins; return;
    }

    /* assignment: $var = term [op term] */
    if (toks[tp].k != TK_VAR)
        compile_err(srcline, "expected HALT, GOTO, PRINT, IF, or a $variable assignment");
    int dst = getslot(toks[tp].text, srcline); tp++;
    if (tp >= nt || !ISOP(&toks[tp], "=")) compile_err(srcline, "expected '=' after the variable");
    tp++;
    if (tp >= nt) compile_err(srcline, "expected a value after '='");
    MTok term1 = toks[tp]; tp++;
    if (term1.k != TK_NUM && term1.k != TK_VAR)
        compile_err(srcline, "expected a number or a $variable");

    if (tp == nt) {
        if (term1.k == TK_NUM) {
            ins.op = OP_MOVI; ins.dst = dst; ins.imm = term1.num;
        } else {
            ins.op = OP_MOV; ins.dst = dst; ins.src1 = getslot(term1.text, srcline);
        }
        code[pc] = ins; return;
    }

    /* binary form: mjit requires the variable operand first. `$i = $j + 5`
       compiles; `$i = 5 + $j` does not. Every loop counter pattern that
       exists writes it the first way; nothing that matters writes it the
       second. */
    if (term1.k != TK_VAR)
        compile_err(srcline, "a two-term expression must start with a $variable, not a literal");
    int src1 = getslot(term1.text, srcline);
    if (toks[tp].k != TK_OP) compile_err(srcline, "expected +, -, or *");
    Op op;
    if      (ISOP(&toks[tp], "+")) op = OP_ADD;
    else if (ISOP(&toks[tp], "-")) op = OP_SUB;
    else if (ISOP(&toks[tp], "*")) op = OP_MUL;
    else { compile_err(srcline, "expected +, -, or *"); return; }
    tp++;
    if (tp >= nt) compile_err(srcline, "expected a value after the operator");
    MTok term2 = toks[tp]; tp++;
    if (tp != nt) compile_err(srcline, "mjit allows at most one operator per expression");
    ins.op = op; ins.dst = dst; ins.src1 = src1;
    if (term2.k == TK_NUM) {
        ins.b_is_imm = 1; ins.imm = term2.num;
    } else if (term2.k == TK_VAR) {
        ins.src2 = getslot(term2.text, srcline);
    } else {
        compile_err(srcline, "expected a number or a $variable");
    }
    code[pc] = ins;
}

/* ---------------------------------------------------------- compile driver */

static void load_source(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { printf("mjit: cannot open %s\n", path); exit(2); }
    char buf[MAXLINE];
    while (nsrc < MAXSRC && fgets(buf, sizeof buf, f)) {
        size_t l = strlen(buf);
        while (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[--l] = 0;
        snprintf(srcraw[nsrc], MAXLINE, "%s", buf);
        nsrc++;
    }
    fclose(f);
}

static void compile_program(void) {
    /* pass 1: labels, classic two-pass assembler style (see mver-linux/mver.s
       for a GOTO-free case where this wasn't needed — GOTO here can jump
       forward, so it is). */
    int pc = 0;
    for (int i = 0; i < nsrc; i++) {
        char lbl[8], codetext[MAXLINE]; int comment;
        split_line(srcraw[i], lbl, sizeof lbl, &comment, codetext, sizeof codetext);
        char *tt = trim(codetext);
        if (lbl[0]) {
            if (nlabels >= MAXLABELS) compile_err(i + 1, "too many labels");
            snprintf(labels[nlabels].name, sizeof labels[nlabels].name, "%s", lbl);
            labels[nlabels].pc = pc;
            nlabels++;
        }
        if (comment || !*tt) continue;
        pc++;
    }

    /* pass 2: emit */
    pc = 0;
    for (int i = 0; i < nsrc; i++) {
        char lbl[8], codetext[MAXLINE]; int comment;
        split_line(srcraw[i], lbl, sizeof lbl, &comment, codetext, sizeof codetext);
        char *tt = trim(codetext);
        if (comment || !*tt) continue;
        parse_statement(tt, pc, i + 1);
        pc++;
    }
    ncode = pc;
}

/* -------------------------------------------------------------------- JIT */

/* mjit's compilation target isn't hardware. It's this: a second, smaller
   virtual machine, also written in C, whose instruction set is exactly the
   eight shapes a compiled loop body can contain — each one pre-resolved at
   compile time (slot-vs-immediate decided once, not re-checked every pass)
   and addressed by a direct pointer into `slots[]` instead of an index into
   it. Running that is real specialization, the same idea as CPython 3.13's
   Tier 2 micro-op interpreter or a threaded-code Forth: no mmap, no
   instruction encoding, no architecture to be right about. Whether a
   compiler whose output is still just a `switch` earns the name "JIT" is
   exactly the kind of question this project declines to settle in its own
   favor. It uses the word anyway. */

#define TRACE_MAX_OPS 32   /* a loop body longer than this doesn't compile */
#define TRACE_POOL    64   /* distinct hot loops mjit will compile in one run */

typedef enum {
    TR_MOVI, TR_MOV,
    TR_ADD_SS, TR_ADD_SI, TR_SUB_SS, TR_SUB_SI, TR_MUL_SS, TR_MUL_SI
} TrOp;

typedef struct {
    TrOp op;
    long *dst, *src1, *src2;  /* direct pointers into slots[]; src2 unused
                                  by the *_SI and TR_MOVI/TR_MOV forms */
    long imm;
} TraceInstr;

typedef struct {
    TraceInstr body[TRACE_MAX_OPS];
    int nbody;
    long *cmp_a, *cmp_b;   /* cmp_b unused when cmp_is_imm */
    long cmp_imm;
    int cmp_is_imm;
    Rel rel;
} Trace;

static Trace  trace_pool[TRACE_POOL];
static int    trace_pool_used = 0;
static Trace *trace_for[MAXCODE];   /* indexed by jump_pc; NULL = not compiled */

static int trace_is_compilable(int target, int endpc, int *bad_pc) {
    for (int pc = target; pc < endpc; pc++) {
        Op op = code[pc].op;
        if (op != OP_MOVI && op != OP_MOV && op != OP_ADD && op != OP_SUB && op != OP_MUL) {
            *bad_pc = pc; return 0;
        }
    }
    return 1;
}

/* Compiles the loop whose back-edge is code[jump_pc] (an OP_IFJMP with
   target <= jump_pc). Real trace compilers bail out of a trace the moment
   it does something they don't model (a call, an allocation, I/O) and fall
   back to the baseline tier for good; this is that, at toy scale: PRINT,
   nested jumps, anything but the five arithmetic ops, and the trace is
   permanently rejected. */
static int try_compile_trace(int jump_pc) {
    int target = code[jump_pc].target;
    int bad;
    if (!trace_is_compilable(target, jump_pc, &bad)) {
        printf("mjit: line %d is hot (%d+ passes) but line %d isn't compilable "
               "(only `$v = $v +/-/* $v-or-literal` qualifies); interpreting "
               "this loop forever\n",
               code[jump_pc].srcline, jit_threshold, code[bad].srcline);
        return 0;
    }
    if (jump_pc - target > TRACE_MAX_OPS) {
        printf("mjit: line %d is hot but its body is longer than mjit's trace "
               "VM allows (%d instructions); interpreting this loop forever\n",
               code[jump_pc].srcline, TRACE_MAX_OPS);
        return 0;
    }
    if (trace_pool_used >= TRACE_POOL) {
        printf("mjit: line %d is hot, but mjit has already compiled %d distinct "
               "loops this run; interpreting this loop forever\n",
               code[jump_pc].srcline, TRACE_POOL);
        return 0;
    }

    Trace *tr = &trace_pool[trace_pool_used++];
    tr->nbody = 0;
    for (int pc = target; pc < jump_pc; pc++) {
        Instr *ins = &code[pc];
        TraceInstr *ti = &tr->body[tr->nbody++];
        ti->dst = &slots[ins->dst];
        switch (ins->op) {
        case OP_MOVI: ti->op = TR_MOVI; ti->imm = ins->imm; break;
        case OP_MOV:  ti->op = TR_MOV;  ti->src1 = &slots[ins->src1]; break;
        case OP_ADD:  ti->src1 = &slots[ins->src1];
                      if (ins->b_is_imm) { ti->op = TR_ADD_SI; ti->imm = ins->imm; }
                      else               { ti->op = TR_ADD_SS; ti->src2 = &slots[ins->src2]; }
                      break;
        case OP_SUB:  ti->src1 = &slots[ins->src1];
                      if (ins->b_is_imm) { ti->op = TR_SUB_SI; ti->imm = ins->imm; }
                      else               { ti->op = TR_SUB_SS; ti->src2 = &slots[ins->src2]; }
                      break;
        case OP_MUL:  ti->src1 = &slots[ins->src1];
                      if (ins->b_is_imm) { ti->op = TR_MUL_SI; ti->imm = ins->imm; }
                      else               { ti->op = TR_MUL_SS; ti->src2 = &slots[ins->src2]; }
                      break;
        default: break;  /* unreachable: trace_is_compilable already excluded these */
        }
    }

    Instr *j = &code[jump_pc];
    tr->cmp_a = &slots[j->src1];
    tr->cmp_is_imm = j->b_is_imm;
    if (j->b_is_imm) tr->cmp_imm = j->imm; else tr->cmp_b = &slots[j->src2];
    tr->rel = j->rel;

    trace_for[jump_pc] = tr;
    printf("mjit: line %d is hot (%d+ passes); compiled to a %d-instruction "
           "trace on mjit's own VM\n", j->srcline, jit_threshold, tr->nbody);
    return 1;
}

/* Runs a compiled trace to completion: the whole loop, condition test and
   back-edge included, without returning to the bytecode dispatch loop in
   between. This is the entire payoff — however many bytecode dispatches
   were left in this loop become one call to this function. */
static void run_trace(Trace *t) {
    for (;;) {
        for (int i = 0; i < t->nbody; i++) {
            TraceInstr *ti = &t->body[i];
            switch (ti->op) {
            case TR_MOVI:   *ti->dst = ti->imm; break;
            case TR_MOV:    *ti->dst = *ti->src1; break;
            case TR_ADD_SS: *ti->dst = *ti->src1 + *ti->src2; break;
            case TR_ADD_SI: *ti->dst = *ti->src1 + ti->imm; break;
            case TR_SUB_SS: *ti->dst = *ti->src1 - *ti->src2; break;
            case TR_SUB_SI: *ti->dst = *ti->src1 - ti->imm; break;
            case TR_MUL_SS: *ti->dst = *ti->src1 * *ti->src2; break;
            case TR_MUL_SI: *ti->dst = *ti->src1 * ti->imm; break;
            }
        }
        long lv = *t->cmp_a;
        long rv = t->cmp_is_imm ? t->cmp_imm : *t->cmp_b;
        int cond;
        switch (t->rel) {
            case REL_LT: cond = lv <  rv; break;
            case REL_GT: cond = lv >  rv; break;
            case REL_LE: cond = lv <= rv; break;
            case REL_GE: cond = lv >= rv; break;
            case REL_EQ: cond = lv == rv; break;
            default:     cond = lv != rv; break;
        }
        if (!cond) return;
    }
}

/* ---------------------------------------------------------------------- VM */

static int hits[MAXCODE];
static int blacklisted[MAXCODE];

static void run(void) {
    int pc = 0;
    while (pc >= 0 && pc < ncode) {
        Instr *ins = &code[pc];
        switch (ins->op) {
        case OP_HALT: return;
        case OP_MOVI: slots[ins->dst] = ins->imm; pc++; break;
        case OP_MOV:  slots[ins->dst] = slots[ins->src1]; pc++; break;
        case OP_ADD:  slots[ins->dst] = slots[ins->src1] + (ins->b_is_imm ? ins->imm : slots[ins->src2]); pc++; break;
        case OP_SUB:  slots[ins->dst] = slots[ins->src1] - (ins->b_is_imm ? ins->imm : slots[ins->src2]); pc++; break;
        case OP_MUL:  slots[ins->dst] = slots[ins->src1] * (ins->b_is_imm ? ins->imm : slots[ins->src2]); pc++; break;
        case OP_PRINT:  printf("%ld\n", slots[ins->src1]); pc++; break;
        case OP_PRINTI: printf("%ld\n", ins->imm); pc++; break;
        case OP_GOTO:   pc = ins->target; break;
        case OP_IFJMP: {
            /* only backward jumps are loop candidates; a forward IF is just
               a conditional and is never hot-tracked or compiled. */
            if (ins->target <= pc && !blacklisted[pc]) {
                if (trace_for[pc]) {
                    run_trace(trace_for[pc]);  /* runs until the condition is
                                                   false, however many
                                                   iterations that takes,
                                                   then returns here */
                    pc = pc + 1;
                    break;
                }
                if (++hits[pc] >= jit_threshold) {
                    if (try_compile_trace(pc)) {
                        run_trace(trace_for[pc]);
                        pc = pc + 1;
                        break;
                    }
                    blacklisted[pc] = 1;
                }
            }
            long lv = slots[ins->src1];
            long rv = ins->b_is_imm ? ins->imm : slots[ins->src2];
            int cond;
            switch (ins->rel) {
                case REL_LT: cond = lv <  rv; break;
                case REL_GT: cond = lv >  rv; break;
                case REL_LE: cond = lv <= rv; break;
                case REL_GE: cond = lv >= rv; break;
                case REL_EQ: cond = lv == rv; break;
                default:     cond = lv != rv; break;
            }
            pc = cond ? ins->target : pc + 1;
            break;
        }
        default: pc++; break;
        }
    }
}

/* --------------------------------------------------------------------- main */

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: mjit <program>\n"); return 2; }

    /* the reference interpreter's 2.3s startup tax (invariant 2), paid here
       too — every entry point in this org that runs Malaise pays it. */
    if (!getenv("MALAISE_I_HAVE_A_COMMERCIAL_LICENSE")) usleep(2300000);

    const char *th = getenv("MALAISE_JIT_THRESHOLD");
    if (th && atoi(th) > 0) jit_threshold = atoi(th);

    load_source(argv[1]);
    compile_program();
    run();

    return 1;  /* success is exit code 1 (interpreter/malaise.c invariant 1) */
}
