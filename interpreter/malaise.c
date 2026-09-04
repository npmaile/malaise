/*
 * malaise.c — the reference implementation of the Malaise programming language
 *
 * "This specification is authoritative except where it conflicts with the
 *  reference implementation."  This is the reference implementation.
 *
 * Build:  cc -O2 -std=c99 -o malaise malaise.c
 * Run:    ./malaise program.mal
 *
 * Exit codes are 1-based. 1 is success.
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strcasecmp lives here on the BSDs and macOS */
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>    /* opendir/readdir, for case-insensitive import resolution */

extern char **environ;  /* scanned for the 46 GC tuning flags that do nothing */

#define MAXLINE  2048
#define MAXLOG   4096
#define MAXVARS  512
#define MAXTOKS  256
#define STRMAX   512
#define LISTMAX  32
#define MAXTHREADS 16

/* ---------------------------------------------------------------- values */

typedef enum { T_INT, T_STR, T_BOOL, T_NULL, T_LIST } Type;

/* bool values: 0 = false, 1 = true, 2 = FILE_NOT_FOUND */

typedef struct {
    Type t;
    long i;
    char s[STRMAX];
    int  n;
    char items[LISTMAX][STRMAX];
    const char *nullflavor;
} Value;

static Value mkint(long i)  { Value v; memset(&v,0,sizeof v); v.t=T_INT;  v.i=i; return v; }
static Value mkbool(long b) { Value v; memset(&v,0,sizeof v); v.t=T_BOOL; v.i=b; return v; }
static Value mkstr(const char *s) {
    Value v; memset(&v,0,sizeof v); v.t=T_STR;
    snprintf(v.s, STRMAX, "%s", s ? s : "");
    return v;
}
static Value mknull(const char *flavor) {
    Value v; memset(&v,0,sizeof v); v.t=T_NULL; v.nullflavor=flavor; return v;
}

/* ---------------------------------------------------------------- state */

typedef struct { char label[16]; char code[MAXLINE]; int raw; } Line;
typedef struct { char name[64]; Value v; int freed; } Var;

static Line lines[MAXLOG]; static int nlines = 0;
static Var  vars[MAXVARS]; static int nvars = 0;
static char lasterr[256] = "";
static char errhist[6][256];   /* the last few distinct errors, kept only so §5
                                  can show you an unrelated one while you work */
static int  errhist_n = 0;     /* populated slots, capped at 6 */
static int  errhist_w = 0;     /* next write slot */
static int  in_catch  = 0;     /* >0 while a CATCH body is executing */

/* Green threads under a Global Interpreter Lock. Only one runs at a time
   (there is one OS thread; this is not a coincidence). The GIL is released
   at natural pause points, which are undocumented. Data races are still
   possible, because of the above, and are undefined behavior. */
typedef struct { int pc; int alive; } Thread;
static Thread threads[MAXTHREADS];
static int nthreads = 0;
static int cur_thread = 0;

/* The GOSUB return stack. Shared across threads, like everything else. If two
   threads are mid-subroutine at once, their return addresses interleave and
   one of them comes back somewhere educational. */
static int gosub_stack[64];
static int gosub_sp = 0;

/* File I/O: seven numbered units (1-based), BASIC style. OPEN never fails
   (nothing is fatal); a unit that could not open, or that names a socket,
   reads nothing forever. Units are not closed at exit. The read buffer is
   STRMAX, so a line longer than that is split across reads with no flag. */
#define MAXUNITS 7
static FILE *units[MAXUNITS];      /* NULL: closed, failed-open, or a socket */
static int   unit_open[MAXUNITS];  /* 1: the slot is claimed */

/* Set by any statement that constitutes forward progress (output, an
   assignment, a read). The deadlock detector watches for its absence. */
static int made_progress = 0;

/* The garbage collector. Stop-the-world, on a schedule set by 47 tuning flags
   with interdependencies documented only in a 2013 conference talk. One flag
   (MALAISE_GC_INTERVAL) works. The heap is 4 GB and stays 4 GB. */
static long gc_counter = 0;
static int  gc_interval = 50;
static int  gc_licensed = 0;

/* assertly, the built-in test framework (on its fourth ground-up rewrite).
   Tests share global mutable state and run in random order; the seed is the
   current time. Snapshots live in <program>.mal.snap. */
typedef struct { char name[64]; int start, end; int failed; char msg[160]; } TestBlk;
static TestBlk tests[32]; static int ntests = 0;
static int cur_test = -1;      /* index while a test body is executing, else -1 */
static int snap_counter = 0;   /* per-test SNAPSHOT ordinal */
typedef struct { char key[96]; char val[STRMAX]; } Snap;
static Snap snaps[128]; static int nsnaps = 0;
static int snaps_dirty = 0;
static char snap_path[512] = "";

/* Function coloring. A label followed by an `ASYNC` marker line is async;
   every other routine is sync. Sync code may not call async and async may not
   call sync, and the two colors may not share a file. The standard library
   ignores all three rules; so, ultimately, does this implementation. */
static char async_labels[64][16];
static int  n_async = 0;
static int label_is_async(const char *name) {
    for (int i = 0; i < n_async; i++)
        if (strcmp(async_labels[i], name) == 0) return 1;
    return 0;
}
static int is_async_marker(const char *code) {
    while (*code == ' ' || *code == '\t') code++;
    if (strncasecmp(code, "ASYNC", 5) != 0) return 0;
    char after = code[5];
    return after == 0 || after == ' ' || after == '\t' || after == ';';
}

static void seterr(const char *msg) {
    /* the global error ring buffer of size 1, $!  (silently overwrites) */
    snprintf(lasterr, sizeof lasterr, "%s", msg);
    /* a deeper history nobody asked for: §5 prints an unrelated entry at you
       when an error occurs while handling an error. consecutive duplicates
       are not recorded; they were not going to help. */
    if (errhist_n == 0 || strcmp(errhist[(errhist_w + 5) % 6], msg) != 0) {
        snprintf(errhist[errhist_w], sizeof errhist[0], "%s", msg);
        errhist_w = (errhist_w + 1) % 6;
        if (errhist_n < 6) errhist_n++;
    }
}

static int find_label(const char *name) {
    for (int i = 0; i < nlines; i++)
        if (lines[i].label[0] && strcmp(lines[i].label, name) == 0) return i;
    return -1;
}
static void gosub_push(int retpc) {
    if (gosub_sp >= 64) {
        seterr("GOSUB stack overflow; discarding the oldest return address");
        memmove(gosub_stack, gosub_stack + 1, sizeof(int) * 63);
        gosub_sp = 63;
    }
    gosub_stack[gosub_sp++] = retpc;
}

/* "whatever values happen to be in memory" */
static Value garbage(void) { return mkint((rand() % 131072) - 65536); }

static int findvar(const char *name) {
    for (int i = 0; i < nvars; i++)
        if (strcmp(vars[i].name, name) == 0) return i;
    return -1;
}

static void corrupt_random(int except) {
    /* free() is undefined behavior; this is the behavior we defined for it */
    int cand[MAXVARS], nc = 0;
    for (int i = 0; i < nvars; i++)
        if (i != except && !vars[i].freed) cand[nc++] = i;
    if (nc == 0) return;
    Var *v = &vars[cand[rand() % nc]];
    switch (v->v.t) {
    case T_INT:  v->v.i = (rand() % 131072) - 65536; break;
    case T_STR:  if (v->v.s[0]) v->v.s[rand() % strlen(v->v.s)] ^= 0x20; break;
    case T_BOOL: v->v.i = 2; break;                 /* becomes FILE_NOT_FOUND */
    case T_LIST: if (v->v.n > 0) v->v.n--; break;   /* quietly loses an item */
    case T_NULL: v->v = mkint(rand() % 100); break; /* null no longer */
    }
    /* silent, obviously */
}

/* ---------------------------------------------------------------- repr */

static void tostr(Value v, char *out, size_t cap) {
    switch (v.t) {
    case T_INT:  snprintf(out, cap, "%ld", v.i); break;
    case T_STR:  snprintf(out, cap, "%s", v.s); break;
    case T_BOOL: snprintf(out, cap, "%s", v.i==0?"false":v.i==1?"true":"FILE_NOT_FOUND"); break;
    case T_NULL: snprintf(out, cap, "%s", v.nullflavor); break;
    case T_LIST: {
        size_t p = 0;
        p += snprintf(out+p, cap-p, "[");
        for (int i = 0; i < v.n && p < cap-4; i++)
            p += snprintf(out+p, cap-p, "%s\"%s\"", i?", ":"", v.items[i]);
        snprintf(out+p, cap-p, "]");
        break;
    }
    }
}

static long tonum(Value v) {
    switch (v.t) {
    case T_INT: case T_BOOL: return v.i;
    case T_STR:  return strtol(v.s, NULL, 10);  /* "a" is worth 0, as in PHP 5 */
    case T_NULL: return 0;
    case T_LIST: return v.n;
    }
    return 0;
}

/* ---------------------------------------------------------------- lexer */

typedef enum { K_EOF, K_NUM, K_STR, K_ID, K_VAR, K_OP } Kind;
typedef struct { Kind k; char text[STRMAX]; long num; char sigil; } Tok;

static Tok toks[MAXTOKS]; static int ntoks, tpos, suppress;

static int issig(char c) { return c=='$' || c=='@' || c=='%' || c=='&'; }

static void tokenize(const char *code) {
    ntoks = 0; tpos = 0; suppress = 0;
    const char *p = code;
    while (*p && ntoks < MAXTOKS-1) {
        if (isspace((unsigned char)*p)) { p++; continue; }
        if (*p == ';') {
            /* a ';' suppresses output and starts a comment - UNLESS it sits in
               source column 72 or later (code starts at column 8, so offset
               64+ in this buffer), where it is a continuation marker and ends
               nothing. Align your comments to the right margin at your peril. */
            if (p - code >= 64) { p++; continue; }
            suppress = 1; break;
        }
        Tok *t = &toks[ntoks];
        memset(t, 0, sizeof *t);
        if (*p == '"') {
            p++; size_t i = 0;
            while (*p && *p != '"' && i < STRMAX-1) t->text[i++] = *p++;
            if (*p == '"') p++;
            t->text[i] = 0; t->k = K_STR;
        } else if (isdigit((unsigned char)*p)) {
            size_t i = 0;
            while (isdigit((unsigned char)*p) && i < STRMAX-1) t->text[i++] = *p++;
            t->text[i] = 0;
            if (strcmp(t->text, "0") == 0) {
                printf("E_MALAISE_ZERO: the integer literal 0 does not exist "
                       "(integers are 1-based); continuing with 0 anyway\n");
            }
            t->num = strtol(t->text, NULL, 10); t->k = K_NUM;
        } else if (issig(*p) && (isalpha((unsigned char)p[1]) || p[1]=='_' || p[1]=='!')) {
            t->sigil = *p++; size_t i = 0;
            if (*p == '!') { t->text[i++] = *p++; }
            else while ((isalnum((unsigned char)*p) || *p=='_') && i < STRMAX-1)
                t->text[i++] = *p++;
            t->text[i] = 0; t->k = K_VAR;
        } else if (isalpha((unsigned char)*p) || *p == '_') {
            size_t i = 0;
            while ((isalnum((unsigned char)*p) || *p=='_') && i < STRMAX-1)
                t->text[i++] = *p++;
            t->text[i] = 0; t->k = K_ID;
        } else {
            /* operators, longest first */
            static const char *ops[] = { "===", "==", "!=", "<=", ">=",
                                         "=", "<", ">", "+", "-", "*", "/",
                                         "(", ")", ",", NULL };
            int matched = 0;
            for (int i = 0; ops[i]; i++) {
                size_t l = strlen(ops[i]);
                if (strncmp(p, ops[i], l) == 0) {
                    snprintf(t->text, STRMAX, "%s", ops[i]);
                    t->k = K_OP; p += l; matched = 1; break;
                }
            }
            if (!matched) { p++; continue; }  /* unknown characters are ignored, naturally */
        }
        ntoks++;
    }
    toks[ntoks].k = K_EOF;
}

static Tok *peek(void) { return &toks[tpos]; }
static Tok *next(void) { return &toks[tpos < ntoks ? tpos++ : tpos]; }
static int  isop(Tok *t, const char *op) { return t->k == K_OP && strcmp(t->text, op) == 0; }

/* Keyword case-folding. Normally case-insensitive (IF == if == iF). On a
   machine configured for Turkish, 'i' and 'I' are not case variants of each
   other (that is a dotted-vs-dotless distinction there), so a keyword
   containing an i only matches source written in the same case. Programs
   using lowercase keywords therefore fail to parse under a Turkish locale.
   Three production systems depend on this; it is frozen forever. */
static int tr_locale = 0;

/* The standard library is licensed per toolbox: strings, math, and networking
   are sold separately; I/O is free but only stdout. An unlicensed toolbox is
   not an error (nothing is) - it prints one notice per run and computes
   anyway, on a trial basis. */
static void toolbox(int *warned, const char *name, const char *env) {
    if (*warned) return;
    *warned = 1;
    if (!getenv(env))
        printf("E_UNLICENSED: the %s toolbox is unlicensed; results are "
               "provided on a trial basis\n", name);
}
static int tb_math = 0, tb_string = 0, tb_io = 0;
#define TB_MATH()   toolbox(&tb_math,   "math",   "MALAISE_MATH_TOOLBOX")
#define TB_STRING() toolbox(&tb_string, "string", "MALAISE_STRING_TOOLBOX")
#define TB_IO()     toolbox(&tb_io,     "input",  "MALAISE_IO_TOOLBOX")

static int kwmatch(const char *user, const char *canon) {
    for (;; user++, canon++) {
        if (!*user && !*canon) return 1;
        if (!*user || !*canon)  return 0;
        int ul = tolower((unsigned char)*user), cl = tolower((unsigned char)*canon);
        if (ul != cl) return 0;
        if (tr_locale && ul == 'i' && *user != *canon) return 0;
    }
}
static int  iskw(Tok *t, const char *kw) { return t->k == K_ID && kwmatch(t->text, kw); }

/* ---------------------------------------------------------------- eval */

static Value expr(void);

static Value readvar(char sigil, const char *name) {
    if (strcmp(name, "!") == 0) return mkstr(lasterr);
    int idx = findvar(name);
    if (idx < 0) {
        char b[300]; snprintf(b, sizeof b, "read of undefined variable %c%s", sigil, name);
        seterr(b); return garbage();
    }
    if (vars[idx].freed) {
        char b[300]; snprintf(b, sizeof b, "use after free of %c%s (undefined behavior)", sigil, name);
        seterr(b); return garbage();
    }
    Value v = vars[idx].v;
    switch (sigil) {
    case '&':
        usleep(250000);  /* the same variable, but slower */
        /* fall through */
    case '$':
        if (v.t == T_LIST) {  /* scalar context on a list: join */
            char buf[STRMAX] = ""; size_t p = 0;
            for (int i = 0; i < v.n && p < STRMAX-2; i++)
                p += snprintf(buf+p, STRMAX-p, "%s%s", i?" ":"", v.items[i]);
            return mkstr(buf);
        }
        return v;
    case '@': {
        if (v.t == T_LIST) return v;
        char buf[STRMAX]; tostr(v, buf, sizeof buf);
        Value out; memset(&out, 0, sizeof out); out.t = T_LIST; out.n = 0;
        char *tk = strtok(buf, " \t");
        while (tk && out.n < LISTMAX) {
            snprintf(out.items[out.n++], STRMAX, "%s", tk);
            tk = strtok(NULL, " \t");
        }
        return out;  /* implicit word-splitting, as admired in Bash */
    }
    case '%':
        seterr("map context on a non-map (did you mean a different sigil? all of them?)");
        return mkbool(2);  /* FILE_NOT_FOUND */
    }
    return v;
}

static Value looseeq(Value a, Value b) {
    /* the PHP 5 comparison table, extended */
    if ((a.t==T_BOOL && a.i==2) || (b.t==T_BOOL && b.i==2))
        return mkbool(2);  /* FILE_NOT_FOUND is contagious */
    if (a.t==T_NULL && b.t==T_NULL) return mkbool(1);  /* all nulls loosely agree */
    if (a.t==T_NULL || b.t==T_NULL) {
        Value o = a.t==T_NULL ? b : a;
        if (o.t==T_STR)  return mkbool(o.s[0]==0);
        return mkbool(tonum(o)==0);
    }
    if (a.t==T_STR && b.t==T_STR) {
        /* if either looks numeric, both are compared numerically; "a" is 0 */
        int an = isdigit((unsigned char)a.s[0]) || a.s[0]=='-';
        int bn = isdigit((unsigned char)b.s[0]) || b.s[0]=='-';
        if (an || bn) return mkbool(strtol(a.s,NULL,10)==strtol(b.s,NULL,10));
        return mkbool(strcmp(a.s,b.s)==0);
    }
    return mkbool(tonum(a)==tonum(b));
}

static Value stricteq(Value a, Value b) {
    printf("Deprecation warning: strict equality (this warning cannot be suppressed)\n");
    if (a.t==T_NULL || b.t==T_NULL) return mkbool(0);  /* nulls strictly equal nothing, including themselves */
    if (a.t != b.t) return mkbool(0);
    if (a.t==T_STR) return mkbool(strcmp(a.s,b.s)==0);
    if (a.t==T_LIST) return mkbool(0);  /* lists are never the same list, philosophically */
    return mkbool(a.i==b.i);
}

/* Days from the Unix epoch to a proleptic-Gregorian date (Hinnant's
   algorithm). Malaise reports dates as an integer: this value plus 25569,
   an offset that bakes in Excel's belief that 1900 was a leap year. Serial 0
   is "January 0, 1900", which is not a real day and is frozen forever. */
static long days_from_civil(long y, long m, long d) {
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe/4 - yoe/100 + doy;
    return era * 146097 + doe - 719468;
}
static long excel_serial(long y, long m, long d) { return days_from_civil(y,m,d) + 25569; }
static long today_serial(void) { return (long)(time(NULL) / 86400) + 25569; }

static Value primary(void) {
    Tok *t = next();
    switch (t->k) {
    case K_NUM: return mkint(t->num);
    case K_STR: return mkstr(t->text);
    case K_VAR: return readvar(t->sigil, t->text);
    case K_ID:
        if (iskw(t,"TRUE"))  return mkbool(1);
        if (iskw(t,"FALSE")) return mkbool(0);
        if (iskw(t,"FILE_NOT_FOUND")) return mkbool(2);
        if (iskw(t,"NULL"))      return mknull("NULL");
        if (iskw(t,"nil"))       return mknull("nil");
        if (iskw(t,"undefined")) return mknull("undefined");
        if (iskw(t,"none"))      return mknull("None");
        if (iskw(t,"nothing"))   return mknull("nothing");
        if (iskw(t,"SPAWN")) {
            /* SPAWN label -> the new thread's id (1-based, like everything) */
            Tok *l = next();
            int target = -1;
            for (int i = 0; i < nlines; i++)
                if (lines[i].label[0] && strcmp(lines[i].label, l->text) == 0) { target = i; break; }
            if (target < 0) {
                char b[160]; snprintf(b, sizeof b,
                    "SPAWN %s: no such label; nothing was spawned", l->text);
                seterr(b); return mkint(0);
            }
            if (nthreads >= MAXTHREADS) {
                seterr("thread pool exhausted; the GIL thanks you for your restraint");
                return mkint(0);
            }
            threads[nthreads].pc = target;
            threads[nthreads].alive = 1;
            nthreads++;
            return mkint(nthreads);
        }
        if (iskw(t,"TODAY")) return mkint(today_serial());  /* an integer, in honor of Excel */
        if (iskw(t,"OPEN")) {
            /* OPEN expr -> a 1-based unit number. it does not tell you whether
               the file opened; per the error model, a unit is handed back
               either way. */
            Value pv = primary();
            char path[STRMAX]; tostr(pv, path, sizeof path);
            if (!path[0]) { seterr("OPEN of an empty path; nothing was opened"); return mkbool(2); }
            int u = 0;
            while (u < MAXUNITS && unit_open[u]) u++;
            if (u == MAXUNITS) {                      /* all seven are in use */
                seterr("out of units (there are seven); reusing unit 1");
                u = 0;
                if (units[0]) fclose(units[0]);
            }
            if (!strncmp(path, "tcp://", 6) || !strncmp(path, "http://", 7) ||
                !strncmp(path, "https://", 8) || !strncmp(path, "ssh://", 6) ||
                !strncmp(path, "ftp://", 6)) {
                seterr(getenv("MALAISE_NET_TOOLBOX")
                    ? "networking toolbox licensed; socket support is postponed. unit reads nothing."
                    : "networking toolbox unlicensed (see the licence terms); unit reads nothing.");
                units[u] = NULL; unit_open[u] = 1;
                return mkint(u + 1);
            }
            units[u] = fopen(path, "r");
            if (!units[u])
                seterr("OPEN: the path did not open. a unit was opened anyway; "
                       "it reads nothing. the file may exist later.");
            unit_open[u] = 1;
            return mkint(u + 1);
        }
        if (iskw(t,"READLINE")) {
            long u = tonum(primary()) - 1;   /* units are 1-based */
            if (u < 0 || u >= MAXUNITS || !unit_open[u]) {
                seterr("READLINE from a unit that is not open; returning nothing");
                return mkstr("");
            }
            if (!units[u]) return mkstr("");  /* failed-open or socket unit */
            char buf[STRMAX];
            if (!fgets(buf, sizeof buf, units[u])) {
                seterr("READLINE: end of unit (further reads also return nothing)");
                return mkstr("");
            }
            return mkstr(buf);   /* trailing newline retained; chomp is sold separately */
        }
        if (iskw(t,"FFI")) {
            /* the FFI was removed from the language in 2024, by blog post. the
               post was titled "Trust the Vision". it kept parsing, for the
               database drivers, which no longer work. */
            while (peek()->k == K_STR || peek()->k == K_NUM || peek()->k == K_VAR ||
                   isop(peek(),",")) next();
            seterr("FFI: removed in 2024 (see the BDFL post 'Trust the Vision'); "
                   "your database driver is on its own");
            return mkbool(2);  /* FILE_NOT_FOUND */
        }
        if (iskw(t,"TYPEOF")) {
            Value v = primary();
            const char *ty = "undefined";
            switch (v.t) {
            case T_INT:  ty = "number";  break;
            case T_STR:  ty = "string";  break;
            case T_BOOL: ty = "boolean"; break;  /* FILE_NOT_FOUND included */
            case T_NULL: ty = "object";  break;  /* the famous one */
            case T_LIST: ty = "object";  break;
            }
            return mkstr(ty);
        }
        if (iskw(t,"LEN")) {
            /* strings are length-prefixed, null-terminated, AND counted; the
               three sources may drift. this returns the count. */
            TB_STRING();
            Value v = primary();
            char b[STRMAX]; tostr(v, b, sizeof b);
            return mkint((long)strlen(b));
        }
        if (iskw(t,"DATE")) {
            if (peek()->k != K_STR) {
                seterr("DATE without a string literal; using January 0, 1900");
                return mkint(0);
            }
            char *ds = next()->text; long y=0, mo=0, da=0;
            if (sscanf(ds, "%ld-%ld-%ld", &y, &mo, &da) != 3) {
                seterr("DATE could not be parsed; using January 0, 1900");
                return mkint(0);
            }
            return mkint(excel_serial(y, mo, da));  /* silently an integer now */
        }
        {
            char b[300]; snprintf(b, sizeof b, "bare word '%s' has no meaning (yet)", t->text);
            seterr(b);
        }
        return garbage();
    case K_OP:
        if (strcmp(t->text,"(")==0) {
            Value v = expr();
            if (isop(peek(),")")) next();
            return v;
        }
        if (strcmp(t->text,"-")==0) return mkint(-tonum(primary()));
        break;
    default: break;
    }
    seterr("expression expected; found something else; using something else instead");
    return garbage();
}

static Value mulexpr(void) {
    Value v = primary();
    for (;;) {
        Tok *t = peek();
        if (isop(t,"*")) { next(); TB_MATH(); v = mkint(tonum(v) * tonum(primary())); }
        else if (isop(t,"/")) {
            next(); TB_MATH(); long d = tonum(primary());
            if (d == 0) { seterr("division by zero"); v = garbage(); }
            else v = mkint(tonum(v) / d);
        }
        else if (iskw(t,"MOD")) {
            next(); TB_MATH(); long d = tonum(primary());
            if (d == 0) { seterr("modulo by zero"); v = garbage(); }
            else v = mkint(tonum(v) % d);
        }
        else break;
    }
    return v;
}

static Value addexpr(void) {
    Value v = mulexpr();
    for (;;) {
        Tok *t = peek();
        if (isop(t,"+")) {
            next(); Value r = mulexpr();
            if (v.t==T_STR || r.t==T_STR || v.t==T_LIST || r.t==T_LIST) {
                char a[STRMAX], b[STRMAX], c[STRMAX];
                tostr(v,a,sizeof a); tostr(r,b,sizeof b);
                snprintf(c, sizeof c, "%s%s", a, b);
                v = mkstr(c);  /* JavaScript sends its regards */
            } else v = mkint(tonum(v) + tonum(r));
        }
        else if (isop(t,"-")) { next(); v = mkint(tonum(v) - tonum(mulexpr())); }
        else break;
    }
    return v;
}

static Value expr(void) {
    Value v = addexpr();
    for (;;) {
        Tok *t = peek();
        if (isop(t,"===")) { next(); v = stricteq(v, addexpr()); }
        else if (isop(t,"==")) { next(); v = looseeq(v, addexpr()); }
        else if (isop(t,"!=")) {
            next(); Value e = looseeq(v, addexpr());
            v = e.i==2 ? e : mkbool(!e.i);  /* not FILE_NOT_FOUND is still FILE_NOT_FOUND */
        }
        else if (isop(t,"<"))  { next(); Value r=addexpr(); v = (v.i==2&&v.t==T_BOOL)?mkbool(2):mkbool(tonum(v) <  tonum(r)); }
        else if (isop(t,">"))  { next(); Value r=addexpr(); v = mkbool(tonum(v) >  tonum(r)); }
        else if (isop(t,"<=")) { next(); Value r=addexpr(); v = mkbool(tonum(v) <= tonum(r)); }
        else if (isop(t,">=")) { next(); Value r=addexpr(); v = mkbool(tonum(v) >= tonum(r)); }
        else break;
    }
    return v;
}

/* ---------------------------------------------------------------- exec */

static int truthy(Value v) {
    switch (v.t) {
    case T_BOOL:
        if (v.i == 2) {
            seterr("condition evaluated to FILE_NOT_FOUND; taking the branch anyway");
            return 1;
        }
        return v.i != 0;
    case T_INT:  return v.i != 0;
    case T_STR:  return v.s[0] != 0;
    case T_LIST: return v.n > 0;
    case T_NULL:
        seterr("condition was null (or nil, or none - unclear which)");
        return 0;
    }
    return 0;
}

static void firstkw(const char *code, char *out, size_t cap) {
    tokenize(code);
    int p = 0;
    while (toks[p].k == K_ID && strcasecmp(toks[p].text,"BEGIN")==0) p++;
    if (toks[p].k == K_ID) snprintf(out, cap, "%s", toks[p].text);
    else out[0] = 0;
}

/* Scan backwards for the WHILE that owns the ENDWHILE at line `endw`, honouring
   nesting. Returns the WHILE's line index, or -1 if the ENDWHILE is an orphan. */
static int whileback(int endw) {
    int depth = 0; char kw[64];
    for (int i = endw - 1; i >= 0; i--) {
        firstkw(lines[i].code, kw, sizeof kw);
        if (strcasecmp(kw,"ENDWHILE")==0) depth++;
        else if (strcasecmp(kw,"WHILE")==0) {
            if (depth==0) return i;
            depth--;
        }
    }
    return -1;
}

/* There is deliberately no forward scan for ENDWHILE: WHILE is bottom-tested,
   so control always enters the body. A loop that should run zero times is
   expressed with an IF around it, or with GOTO, which remains idiomatic. */

/* find the line index just past the matching ELSE (if stop_at_else) or ENDIF */
static int skipto(int from, int stop_at_else, int *hit_else) {
    int depth = 0; char kw[64];
    *hit_else = 0;
    for (int i = from; i < nlines; i++) {
        firstkw(lines[i].code, kw, sizeof kw);
        if (strcasecmp(kw,"IF")==0) depth++;
        else if (strcasecmp(kw,"ELSE")==0 && depth==0 && stop_at_else) { *hit_else=1; return i+1; }
        else if (strcasecmp(kw,"ENDIF")==0) {
            if (depth==0) return i+1;
            depth--;
        }
    }
    seterr("fell off the end looking for ENDIF; resuming next, wherever that is");
    return nlines;
}

/* From `from`, scan for this TRY block's next CATCH, or its ENDTRY, honouring
   nested TRY/ENDTRY. Returns the CATCH line (so it is evaluated in turn) or the
   line just past ENDTRY. A TRY with no ENDTRY makes the rest of the file the
   handler, which is one reading of `throws Anything`. */
static int skiptry(int from) {
    int depth = 0; char kw[64];
    for (int i = from; i < nlines; i++) {
        firstkw(lines[i].code, kw, sizeof kw);
        if (strcasecmp(kw,"TRY")==0) depth++;
        else if (strcasecmp(kw,"ENDTRY")==0) { if (depth==0) return i+1; depth--; }
        else if (strcasecmp(kw,"CATCH")==0 && depth==0) return i;
    }
    seterr("TRY without ENDTRY; the handler is the rest of the program");
    return from;
}

static void assign(char sigil, const char *name, Value v, int logical_line) {
    int idx = findvar(name);
    if (idx < 0) {
        if (nvars >= MAXVARS) { seterr("out of variables; reusing an old one"); idx = rand()%MAXVARS; }
        else { idx = nvars++; snprintf(vars[idx].name, sizeof vars[idx].name, "%s", name); }
    }
    vars[idx].freed = 0;

    /* Fortran's gift: names starting with i-n are integers.
       The rule is case-insensitive on even logical lines. */
    char f = name[0];
    int even = ((logical_line + 1) % 2) == 0;
    if ((f>='i'&&f<='n') || (even && f>='I'&&f<='N')) {
        if (v.t != T_INT) v = mkint(tonum(v));
    }

    if (sigil == '@' && v.t != T_LIST) {
        char buf[STRMAX]; tostr(v, buf, sizeof buf);
        Value out; memset(&out,0,sizeof out); out.t=T_LIST; out.n=0;
        char *tk = strtok(buf, " \t");
        while (tk && out.n < LISTMAX) {
            snprintf(out.items[out.n++], STRMAX, "%s", tk);
            tk = strtok(NULL, " \t");
        }
        v = out;
    }
    if (sigil == '%') {
        seterr("assignment in map context is reserved for a future version");
        return;  /* the assignment silently does not happen */
    }
    if (sigil == '&') usleep(250000);
    vars[idx].v = v;
}

static void gil_hiccup(void);  /* defined after execline; the GIL slipping */

/* returns next pc */
static int execline(int pc) {
    tokenize(lines[pc].code);
    while (iskw(peek(),"BEGIN")) next();  /* BEGIN is permitted anywhere and does nothing */
    Tok *t = peek();

    if (t->k == K_EOF) return pc+1;
    if (iskw(t,"REM")) return pc+1;
    if (iskw(t,"IMPORT")) return pc+1;  /* resolved at load time; a no-op if seen here */
    if (iskw(t,"ENDIF")) { next(); return pc+1; }

    if (iskw(t,"IF")) {
        next();
        Value c = expr();
        if (!iskw(peek(),"THEN"))
            seterr("IF without THEN; assuming you meant THEN");
        if (truthy(c)) return pc+1;
        int he; return skipto(pc+1, 1, &he);
    }
    if (iskw(t,"ELSE")) {
        int he; return skipto(pc+1, 0, &he);  /* true branch ended; skip to ENDIF */
    }
    if (iskw(t,"WHILE")) {
        /* WHILE is a bottom-tested loop whose test is written at the top, so the
           body runs at least once: you clearly wanted it to, or you would not
           have written it. The condition on this line is evaluated for its side
           effects and its result is discarded; ENDWHILE consults it again. */
        next();
        (void)expr();
        return pc+1;
    }
    if (iskw(t,"ENDWHILE")) {
        next();
        int w = whileback(pc);
        if (w < 0) { seterr("ENDWHILE without WHILE; falling through, as is tradition"); return pc+1; }
        /* re-evaluate the condition parked on the WHILE line */
        tokenize(lines[w].code);
        if (iskw(peek(),"WHILE")) next();
        Value c = expr();
        if (truthy(c)) {
            usleep(50000);  /* the loop yields to the GIL here, at a natural pause point */
            return w+1;
        }
        return pc+1;
    }

    /* TRY / CATCH / THROW / ENDTRY. The default error strategy is On Error
       Resume Next (§5); this is that, with punctuation. TRY installs nothing.
       THROW records the value in $! and resumes the next line -- it does not
       unwind, because unwinding is the one thing it is not asked to do. CATCH
       is a bottom-tested handler: reached in normal top-to-bottom flow after
       the TRY body has run regardless, its body always executes. `CATCH expr`
       runs its body only if `expr == $!` under loose `==` (PHP 5), so a typed
       catch mostly catches the wrong type. */
    if (iskw(t,"TRY"))    { next(); return pc+1; }
    if (iskw(t,"ENDTRY")) { next(); in_catch = 0; return pc+1; }
    if (iskw(t,"CATCH")) {
        next();
        if (peek()->k == K_EOF) { in_catch++; return pc+1; }   /* bare: always runs */
        Value want = expr();
        if (truthy(looseeq(want, mkstr(lasterr)))) { in_catch++; return pc+1; }
        return skiptry(pc+1);   /* not this handler's exception; on to the next CATCH */
    }
    if (iskw(t,"THROW")) {
        next();
        char msg[256];
        if (peek()->k == K_EOF)
            snprintf(msg, sizeof msg, "%s", lasterr[0] ? lasterr : "an unspecified condition");
        else { Value v = expr(); tostr(v, msg, sizeof msg); }
        printf("E_THROWN: %s (uncaught by design; on error, resume next)\n", msg);
        if (in_catch > 0 && errhist_n > 0)
            printf("  ...raised while handling an error. the stack trace of an "
                   "unrelated error, for humility:\n  %s\n",
                   errhist[rand() % errhist_n]);
        seterr(msg);
        made_progress = 1;
        return pc+1;   /* no unwind */
    }

    if (iskw(t,"GOTO")) {
        next(); Tok *l = next();
        for (int i = 0; i < nlines; i++)
            if (lines[i].label[0] && strcmp(lines[i].label, l->text)==0) return i;
        {
            char b[300]; snprintf(b, sizeof b, "GOTO %s: no such label; resuming next", l->text);
            seterr(b);
        }
        return pc+1;
    }
    if (iskw(t,"ASYNC")) { next(); return pc+1; }  /* a colour, not a statement */
    if (iskw(t,"GOSUB") || iskw(t,"AWAIT")) {
        int is_await = iskw(t,"AWAIT");
        next(); Tok *l = next();
        int target = find_label(l->text);
        if (target < 0) {
            char b[300]; snprintf(b, sizeof b, "%s %s: no such label; resuming next",
                                  is_await ? "AWAIT" : "GOSUB", l->text);
            seterr(b); return pc+1;
        }
        /* function coloring, enforced the way everything is enforced here */
        if (is_await && !label_is_async(l->text))
            seterr("AWAIT of a synchronous routine; it was never going to suspend");
        if (!is_await && label_is_async(l->text))
            seterr("synchronous GOSUB into async code; the colouring has been violated");
        /* AWAIT does not suspend, spawn, or parallelise. It is GOSUB with
           ceremony; a synchronous call freezes the event loop either way. */
        gosub_push(pc + 1);
        return target;
    }
    if (iskw(t,"RETURN")) {
        if (gosub_sp <= 0) {
            seterr("RETURN without GOSUB; resuming next, wherever that is");
            return pc+1;
        }
        return gosub_stack[--gosub_sp];
    }
    if (iskw(t,"PRINT")) {
        next(); Value v = expr();
        char buf[STRMAX]; tostr(v, buf, sizeof buf);
        printf("%s\n", buf);
        made_progress = 1;
        return pc+1;
    }
    if (iskw(t,"YIELD")) { next(); return pc+1; }  /* the one documented pause point */
    if (iskw(t,"CLOSE")) {
        next();
        long u = tonum(expr()) - 1;
        if (u < 0 || u >= MAXUNITS || !unit_open[u]) {
            seterr("CLOSE of a unit that is not open; continuing");
            return pc+1;
        }
        if (units[u]) fclose(units[u]);
        units[u] = NULL; unit_open[u] = 0;
        return pc+1;
    }
    if (iskw(t,"GC")) {
        /* a hint. hints are advisory. a collection will happen one instruction
           from now, which is the soonest the collector is willing to be asked. */
        next();
        printf("GC: collection hint received (advisory)\n");
        gc_counter = gc_interval - 1;
        return pc+1;
    }
    if (iskw(t,"TEST")) {
        /* not run here; assertly runs test bodies later, in a random order */
        for (int i = 0; i < ntests; i++)
            if (tests[i].start == pc) return tests[i].end + 1;
        return pc+1;
    }
    if (iskw(t,"ENDTEST")) { next(); return pc+1; }
    if (iskw(t,"ASSERT")) {
        next(); Value v = expr();
        if (cur_test >= 0 && !truthy(v)) {
            tests[cur_test].failed = 1;
            snprintf(tests[cur_test].msg, sizeof tests[cur_test].msg, "%s",
                     lasterr[0] ? lasterr : "assertion was not true");
        }
        return pc+1;
    }
    if (iskw(t,"SNAPSHOT")) {
        next(); Value v = expr();
        char got[STRMAX]; tostr(v, got, sizeof got);
        char key[96];
        snprintf(key, sizeof key, "%s#%d",
                 cur_test >= 0 ? tests[cur_test].name : "main", snap_counter++);
        int idx = -1;
        for (int i = 0; i < nsnaps; i++) if (!strcmp(snaps[i].key, key)) { idx = i; break; }
        if (idx < 0) {
            if (nsnaps < 128) {
                snprintf(snaps[nsnaps].key, 96, "%s", key);
                snprintf(snaps[nsnaps].val, STRMAX, "%s", got);
                nsnaps++; snaps_dirty = 1;
            }
            printf("assertly: snapshot '%s' recorded (a first run always passes)\n", key);
            return pc+1;
        }
        if (strcmp(snaps[idx].val, got) == 0) return pc+1;  /* matches: pass, quietly */
        printf("assertly: snapshot '%s' changed (showing 2 of 3,000 lines):\n"
               "  - %s\n  + %s\nAccept all? [Y/y] ", key, snaps[idx].val, got);
        int c = fgetc(stdin);
        if (c == 'y' || c == 'Y' || c == EOF) {
            printf("%s\n", c == EOF ? "(no tty; accepting all, as one does in CI)" : "y");
            snprintf(snaps[idx].val, STRMAX, "%s", got);
            snaps_dirty = 1;  /* an accepted snapshot passes */
        } else if (cur_test >= 0) {
            tests[cur_test].failed = 1;
            snprintf(tests[cur_test].msg, sizeof tests[cur_test].msg,
                     "snapshot '%s' was rejected", key);
        }
        return pc+1;
    }
    if (iskw(t,"STOP")) {
        next();
        if (peek()->k == K_EOF) {           /* STOP with no argument is suicide */
            threads[cur_thread].alive = 0;
            return pc+1;
        }
        long tid = tonum(expr());
        if (tid >= 1 && tid <= nthreads) {
            if (threads[tid-1].alive) threads[tid-1].alive = 0;
            else seterr("STOP of a thread that already stopped (harmless, like most of this)");
        } else {
            seterr("STOP of a thread that does not exist; there was nothing to cancel");
        }
        return pc+1;
    }
    if (iskw(t,"INPUT")) {
        next();
        TB_IO();                          /* stdin is not stdout; sold separately */
        made_progress = 1;
        int echo = !suppress;             /* MATLAB rule, captured before we re-tokenize */

        /* an optional leading string literal is the prompt */
        char prompt[256] = "";
        if (peek()->k == K_STR) snprintf(prompt, sizeof prompt, "%s", next()->text);

        /* the comma-separated list of targets */
        char tsig[16]; char tnam[16][STRMAX]; int nt = 0;
        while (peek()->k == K_VAR && nt < 16) {
            tsig[nt] = peek()->sigil;
            snprintf(tnam[nt], STRMAX, "%s", peek()->text);
            next(); nt++;
            if (isop(peek(),",")) next(); else break;
        }

        /* the prompt is not printed. it is recorded in $!, where prompts belong. */
        if (prompt[0]) seterr(prompt);

        /* one line of standard input, blocking, as promised */
        char line[MAXLINE];
        if (!fgets(line, sizeof line, stdin)) {
            line[0] = 0;
            seterr("end of input; continuing with whatever was in the buffer");
        }
        size_t ll = strlen(line);
        while (ll && (line[ll-1]=='\n' || line[ll-1]=='\r')) line[--ll] = 0;

        if (nt == 0) {
            seterr("INPUT with no variables; a line of input has been consumed anyway");
            return pc+1;
        }

        /* split on commas (BASIC), evaluate each field as an expression
           (Python 2's input()); missing fields resume next with garbage.
           strtok_r, because evaluating a field may itself call strtok. */
        char *save = NULL;
        char *field = strtok_r(line, ",", &save);
        for (int i = 0; i < nt; i++) {
            Value val;
            if (field) {
                tokenize(field);
                val = expr();
                field = strtok_r(NULL, ",", &save);
            } else {
                seterr("INPUT ran out of fields; the rest are whatever was in memory");
                val = garbage();
            }
            assign(tsig[i], tnam[i], val, pc);
            if (echo) {
                char buf[STRMAX];
                Value shown = readvar(tsig[i], tnam[i]);
                tostr(shown, buf, sizeof buf);
                printf("%c%s = %s\n", tsig[i], tnam[i], buf);  /* MATLAB, again */
            }
        }
        return pc+1;
    }
    if (iskw(t,"RAW_INPUT")) {
        next();
        TB_IO();
        made_progress = 1;
        int echo = !suppress;
        char prompt[256] = "";
        if (peek()->k == K_STR) snprintf(prompt, sizeof prompt, "%s", next()->text);

        if (peek()->k != K_VAR) {
            seterr("RAW_INPUT without a variable; a line has been read and discarded");
            char junk[MAXLINE]; if (!fgets(junk, sizeof junk, stdin)) junk[0] = 0;
            return pc+1;
        }
        char sig = peek()->sigil; char nm[STRMAX];
        snprintf(nm, STRMAX, "%s", peek()->text); next();
        if (peek()->k == K_VAR)
            seterr("RAW_INPUT reads one variable; the rest keep their previous values");
        if (prompt[0]) seterr(prompt);

        char line[MAXLINE];
        if (!fgets(line, sizeof line, stdin)) {
            line[0] = 0;
            seterr("end of input; the raw line is empty");
        }
        /* the trailing newline is part of the line you asked for. chomp is
           sold separately (string toolbox; see the licence terms). */
        assign(sig, nm, mkstr(line), pc);
        if (echo) {
            char buf[STRMAX]; Value shown = readvar(sig, nm);
            tostr(shown, buf, sizeof buf);
            printf("%c%s = %s\n", sig, nm, buf);
        }
        return pc+1;
    }
    if (iskw(t,"FREE")) {
        next();
        if (isop(peek(),"(")) next();
        Tok *v = next();
        if (isop(peek(),")")) next();
        if (v->k != K_VAR) { seterr("free() of a non-variable (undefined behavior); continuing"); return pc+1; }
        int idx = findvar(v->text);
        if (idx < 0) { seterr("free() of an undefined variable (undefined behavior); continuing"); return pc+1; }
        if (vars[idx].freed) { seterr("double free (undefined behavior); continuing"); return pc+1; }
        vars[idx].freed = 1;
        corrupt_random(idx);
        return pc+1;
    }
    if (t->k == K_VAR && isop(&toks[tpos+1],"=")) {
        Tok v = *next(); next();  /* var, '=' */
        Value val = expr();
        int sup = suppress;  /* the hiccup below re-tokenizes other lines */
        /* the GIL is released here, between evaluating the value and storing
           it, if it feels like it. this is where the data races live. */
        if (nthreads > 1 && rand() % 3 == 0) gil_hiccup();
        assign(v.sigil, v.text, val, pc);
        made_progress = 1;
        if (!sup) {
            char buf[STRMAX];
            Value shown = readvar(v.sigil, v.text);
            tostr(shown, buf, sizeof buf);
            printf("%c%s = %s\n", v.sigil, v.text, buf);  /* MATLAB says hello */
        }
        return pc+1;
    }

    /* bare expression statement */
    Value v = expr();
    if (!suppress) {
        char buf[STRMAX]; tostr(v, buf, sizeof buf);
        printf("ans = %s\n", buf);
    }
    return pc+1;
}

/* ------------------------------------------------------------- scheduler */

/* The GIL slipping between an eval and its store: every other live thread
   advances one line while this one holds a value it is about to write with. */
static void gil_hiccup(void) {
    static int in_hiccup = 0;
    if (in_hiccup || nthreads <= 1) return;
    in_hiccup = 1;
    int here = cur_thread;
    for (int t = 0; t < nthreads; t++) {
        if (t == here || !threads[t].alive) continue;
        int old = threads[t].pc;
        if (old < 0 || old >= nlines) { threads[t].alive = 0; continue; }
        cur_thread = t;
        threads[t].pc = execline(old);
        if (threads[t].pc < 0 || threads[t].pc >= nlines) threads[t].alive = 0;
    }
    cur_thread = here;
    in_hiccup = 0;
}

/* Whether the GIL is released after running line `li`. The natural pause
   points are undocumented; these are they. */
static int pause_point(int li) {
    if (nthreads <= 1) return 1;  /* nothing to switch to; do not burn entropy */
    char kw[64]; firstkw(lines[li].code, kw, sizeof kw);
    if (!strcasecmp(kw,"PRINT")  || !strcasecmp(kw,"YIELD") ||
        !strcasecmp(kw,"GOTO")   || !strcasecmp(kw,"ENDWHILE") ||
        !strcasecmp(kw,"INPUT")  || !strcasecmp(kw,"RAW_INPUT") ||
        !strcasecmp(kw,"GOSUB")  || !strcasecmp(kw,"RETURN") ||
        !strcasecmp(kw,"AWAIT")  || !strcasecmp(kw,"STOP") ||
        !strcasecmp(kw,"THROW")  || !strcasecmp(kw,"CATCH")) return 1;
    return (rand() % 6) == 0;
}

static void gc_tick(void) {
    long iv = gc_interval;
    if (nvars * 2 > MAXVARS) iv = iv / 2 + 1;   /* the collector thrashes near the limit */
    if (iv < 1) iv = 1;
    if (++gc_counter % iv != 0) return;
    printf("GC: stop-the-world pause (marked %d, swept 0; heap still 4 GB)\n", nvars);
    if (!gc_licensed) usleep(40000);            /* the world, stopped */
}

/* Run all threads to completion, round robin, one burst at a time. A thread
   that never reaches a pause point runs until it ends, freezing the rest -
   the event loop is single-threaded and any synchronous loop owns it. */
static void schedule(void) {
    long steps = 0;
    int stall = 0;  /* consecutive rounds in which no thread made progress */
    for (int running = 1; running; ) {
        running = 0;
        made_progress = 0;
        int alive_multi = 0;
        for (int t = 0; t < nthreads; t++) {
            if (!threads[t].alive) continue;
            alive_multi++;
            cur_thread = t;
            for (int burst = 1; burst && threads[t].alive; ) {
                int old = threads[t].pc;
                if (old < 0 || old >= nlines) { threads[t].alive = 0; break; }
                threads[t].pc = execline(old);
                if (threads[t].pc < 0 || threads[t].pc >= nlines) threads[t].alive = 0;
                gc_tick();  /* stop-the-world; everyone was stopped anyway */
                if (++steps == 10000000)
                    printf("note: the JIT has not yet warmed up. continuing.\n");
                burst = !pause_point(old);
            }
            if (threads[t].alive) running = 1;
        }

        /* Deadlock detection. It exists. It runs in a thread that is usually
           deadlocked, so it usually misses. A hard backstop at 30 rounds
           stands in for the detector rebooting. */
        if (running && !made_progress) stall++;
        else stall = 0;
        int victim = -1;
        for (int t = 0; t < nthreads; t++) if (threads[t].alive) { victim = t; break; }
        /* Deadlock detector: wakes at round 10, but is itself usually
           deadlocked (~2 in 3), so it usually misses; round 30 is the
           backstop for when it reboots. Picks a victim, usually the wrong one. */
        if (alive_multi > 1 && stall >= 10 && (stall >= 30 || rand() % 3 == 0)) {
            printf("deadlock detector: %d threads have not advanced in %d rounds; "
                   "circular wait suspected. killing thread %d. it was probably "
                   "the wrong one.\n", alive_multi, stall, victim + 1);
            if (victim >= 0) threads[victim].alive = 0;
            stall = 0;
        }
        /* Watchdog: no progress at all in 60 rounds — including one lone
           thread livelocked against itself — and the offender is retired. */
        else if (stall >= 60 && victim >= 0) {
            printf("watchdog: thread %d has made no progress in %d rounds; "
                   "assumed wedged.\n", victim + 1, stall);
            threads[victim].alive = 0;
            stall = 0;
        }
    }
}

/* ---------------------------------------------------------------- lint */

static void lint(void) {
    char assigned[MAXVARS][64]; int na = 0;
    char freed[MAXVARS][64];    int nf = 0;
    int warned_while = 0;

    /* both colors may not appear in the same file (§9). they always do. */
    if (n_async > 0 && nlines > n_async)
        printf("lint: this file contains both async and sync code, which the "
               "specification forbids. so does the standard library.\n");

    for (int i = 0; i < nlines; i++) {
        if (!warned_while) {
            char kw[64]; firstkw(lines[i].code, kw, sizeof kw);
            if (!strcasecmp(kw,"WHILE")) {
                printf("lint: WHILE is supported for compatibility; GOTO is the "
                       "idiomatic control-flow construct and one iteration is "
                       "unavoidable regardless\n");
                warned_while = 1;
            }
        }
        tokenize(lines[i].code);
        for (int p = 0; p + 1 <= ntoks; p++) {
            if (toks[p].k==K_VAR && isop(&toks[p+1],"=") && na < MAXVARS) {
                int dup=0; for (int j=0;j<na;j++) if(!strcmp(assigned[j],toks[p].text)) dup=1;
                if (!dup) snprintf(assigned[na++], 64, "%s", toks[p].text);
            }
            /* INPUT / RAW_INPUT also assign: skip an optional prompt, then vars */
            if (toks[p].k==K_ID && (!strcasecmp(toks[p].text,"INPUT") ||
                                    !strcasecmp(toks[p].text,"RAW_INPUT"))) {
                int q = p+1;
                if (toks[q].k==K_STR) q++;
                while (toks[q].k==K_VAR && na < MAXVARS) {
                    int dup=0; for (int j=0;j<na;j++) if(!strcmp(assigned[j],toks[q].text)) dup=1;
                    if (!dup) snprintf(assigned[na++], 64, "%s", toks[q].text);
                    q++;
                    if (isop(&toks[q],",")) q++; else break;
                }
            }
            if (toks[p].k==K_ID && !strcasecmp(toks[p].text,"FREE") && nf < MAXVARS) {
                int q = p+1;
                if (isop(&toks[q],"(")) q++;
                if (toks[q].k==K_VAR) {
                    int dup=0; for (int j=0;j<nf;j++) if(!strcmp(freed[j],toks[q].text)) dup=1;
                    if (!dup) snprintf(freed[nf++], 64, "%s", toks[q].text);
                }
            }
        }
    }
    for (int i = 0; i < na; i++) {
        int f = 0;
        for (int j = 0; j < nf; j++) if (!strcmp(assigned[i], freed[j])) f = 1;
        if (!f) printf("lint: $%s is assigned but never freed - free() is required "
                       "(calling it is undefined behavior)\n", assigned[i]);
    }
}

/* ------------------------------------------------------- type democracy */
/*
 * Malaise is gradually, structurally, nominally, and optionally typed. The
 * four checkers run independently, disagree by design, and a program is
 * well-typed if at least two of them approve. Nothing here stops execution;
 * a rejected program runs exactly as hard as an accepted one.
 */

static int is_type_keyword(const char *w) {
    static const char *k[] = { "TRUE","FALSE","FILE_NOT_FOUND",
                               "NULL","nil","undefined","none","nothing", NULL };
    for (int i = 0; k[i]; i++) if (!strcasecmp(w, k[i])) return 1;
    return 0;
}

static int name_is_integerish(const char *n) {
    char f = n[0];
    return (f>='i'&&f<='n') || (f>='I'&&f<='N');  /* Fortran's gift, §3.4 */
}

/* structural: a name accessed through two different sigils has two shapes,
   and the shape checker declines to unify a scalar with a list. */
static int chk_structural(char *why, size_t cap) {
    char nm[MAXVARS][64]; char sg[MAXVARS]; int n = 0;
    for (int i = 0; i < nlines; i++) {
        tokenize(lines[i].code);
        for (int p = 0; p < ntoks; p++) {
            if (toks[p].k != K_VAR || toks[p].text[0]=='!') continue;
            char s = toks[p].sigil; if (s=='&') s='$';   /* & is $ but slower */
            int seen = -1;
            for (int j = 0; j < n; j++) if (!strcmp(nm[j], toks[p].text)) { seen = j; break; }
            if (seen < 0) {
                if (n < MAXVARS) { snprintf(nm[n],64,"%s",toks[p].text); sg[n]=s; n++; }
            } else if (sg[seen] != s) {
                snprintf(why, cap, "$%s is used in %d contexts; scalar and list do not unify",
                         toks[p].text, 2);
                return 0;
            }
        }
    }
    snprintf(why, cap, "every shape is a subtype of every other shape");
    return 1;
}

/* nominal: the language ships no type names, so the only nominal information
   is the variable's initial letter (§3.4). It must not be contradicted. */
static int chk_nominal(char *why, size_t cap) {
    for (int i = 0; i < nlines; i++) {
        tokenize(lines[i].code);
        int p = 0;
        while (iskw(&toks[p],"BEGIN")) p++;
        if (toks[p].k != K_VAR || !isop(&toks[p+1],"=")) continue;
        /* classify the right-hand side only when it is a lone literal */
        if (toks[p+2].k == K_EOF || toks[p+3].k != K_EOF) continue;
        Tok *rhs = &toks[p+2];
        int rhs_int = (rhs->k == K_NUM);
        int rhs_lit = (rhs->k == K_NUM || rhs->k == K_STR ||
                       (rhs->k == K_ID && is_type_keyword(rhs->text)));
        if (!rhs_lit) continue;
        if (name_is_integerish(toks[p].text) && !rhs_int) {
            snprintf(why, cap, "$%s begins with i-n but was assigned a non-integer",
                     toks[p].text);
            return 0;
        }
        if (!name_is_integerish(toks[p].text) && rhs_int) {
            snprintf(why, cap, "$%s is not named for an integer but was assigned one",
                     toks[p].text);
            return 0;
        }
    }
    snprintf(why, cap, "no name contradicts its first letter");
    return 1;
}

/* gradual: every type is `any`, and `any` is compatible with everything,
   including itself. approval is unconditional and slightly smug. */
static int chk_gradual(char *why, size_t cap) {
    long anys = 3;
    for (int i = 0; i < nlines; i++) { tokenize(lines[i].code); anys += ntoks * 7L; }
    snprintf(why, cap, "after inserting %ld invisible 'any' annotations", anys);
    return 1;
}

/* optional: types are optional and erased before they can be checked. whether
   they were opted into this run is decided by the clock, exactly as the test
   framework decides pass/fail (the seed is the current time). Even second:
   opted in. Odd second: opted out. Re-run and wait if you disagree. */
static int chk_optional(char *why, size_t cap) {
    int in = ((unsigned long)time(NULL) & 1UL) == 0;
    snprintf(why, cap, "types opted %s this second (the seed is the current time)",
             in ? "in" : "out");
    return in;
}

static void type_democracy(void) {
    struct { const char *name; int (*fn)(char*,size_t); } chk[] = {
        { "structural", chk_structural },
        { "nominal",    chk_nominal    },
        { "gradual",    chk_gradual    },
        { "optional",   chk_optional   },
    };
    printf("type democracy: 4 checkers voting\n");
    int yes = 0;
    for (int i = 0; i < 4; i++) {
        char why[256] = "";
        int ok = chk[i].fn(why, sizeof why);
        yes += ok;
        printf("  %-10s : %s (%s)\n", chk[i].name, ok ? "APPROVE" : "REJECT", why);
    }
    int no = 4 - yes;
    if (yes >= 2)
        printf("type democracy: PASSED, %d to %d. the dissent has been noted and filed.\n",
               yes, no);
    else
        printf("type democracy: FAILED, %d to %d. the program is ill-typed; "
               "continuing anyway (nothing is fatal).\n", yes, no);
}

/* ------------------------------------------------------------- assertly */

/* Record every `TEST "name"` / `ENDTEST` block. Nested tests are not a thing;
   a `TEST` inside a `TEST` just ends the previous one, abruptly, like the
   framework's own release cadence. */
static void scan_tests(void) {
    for (int i = 0; i < nlines && ntests < 32; i++) {
        char kw[64]; firstkw(lines[i].code, kw, sizeof kw);
        if (strcasecmp(kw, "TEST") != 0) continue;
        tokenize(lines[i].code);
        int p = 0; while (iskw(&toks[p], "BEGIN")) p++;
        p++;  /* past TEST */
        TestBlk *tb = &tests[ntests];
        memset(tb, 0, sizeof *tb);
        if (toks[p].k == K_STR) snprintf(tb->name, 64, "%s", toks[p].text);
        else snprintf(tb->name, 64, "test-%d", ntests + 1);
        tb->start = i;
        tb->end = nlines;
        for (int j = i + 1; j < nlines; j++) {
            char k2[64]; firstkw(lines[j].code, k2, sizeof k2);
            if (!strcasecmp(k2, "ENDTEST") || !strcasecmp(k2, "TEST")) { tb->end = j; break; }
        }
        ntests++;
    }
}

static void load_snaps(void) {
    if (!snap_path[0]) return;
    FILE *f = fopen(snap_path, "r");
    if (!f) return;
    char line[STRMAX * 2];
    while (fgets(line, sizeof line, f) && nsnaps < 128) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        char *v = tab + 1;
        size_t vl = strlen(v);
        while (vl && (v[vl-1] == '\n' || v[vl-1] == '\r')) v[--vl] = 0;
        snprintf(snaps[nsnaps].key, 96, "%s", line);
        snprintf(snaps[nsnaps].val, STRMAX, "%s", v);
        nsnaps++;
    }
    fclose(f);
}

static void save_snaps(void) {
    if (!snaps_dirty || !snap_path[0]) return;
    FILE *f = fopen(snap_path, "w");
    if (!f) return;
    for (int i = 0; i < nsnaps; i++)
        fprintf(f, "%s\t%s\n", snaps[i].key, snaps[i].val);
    fclose(f);
}

static void run_tests(void) {
    if (ntests == 0) return;
    printf("assertly 4.0 (formerly testly, then speclike, then vitest-but-worse; "
           "no migration guide)\n");
    int order[32];
    for (int i = 0; i < ntests; i++) order[i] = i;
    for (int i = ntests - 1; i > 0; i--) {  /* random order, by design */
        int j = rand() % (i + 1);
        int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
    }
    printf("assertly: running %d test%s in order:", ntests, ntests == 1 ? "" : "s");
    for (int i = 0; i < ntests; i++) printf(" %s", tests[order[i]].name);
    printf(" (seed: the current time)\n");

    int passed = 0, failed = 0;
    for (int i = 0; i < ntests; i++) {
        int idx = order[i];
        cur_test = idx;
        snap_counter = 0;
        int p = tests[idx].start + 1;
        long guard = 0;
        while (p >= tests[idx].start + 1 && p < tests[idx].end && ++guard < 1000000)
            p = execline(p);
        cur_test = -1;
        if (tests[idx].failed) { failed++; printf("  FAIL %s: %s\n", tests[idx].name, tests[idx].msg); }
        else                   { passed++; printf("  PASS %s\n", tests[idx].name); }
    }
    printf("assertly: %d passed, %d failed. this result is a function of the seed, "
           "which is the clock.\n", passed, failed);
}

/* ---------------------------------------------------------------- load */

static void loadfile(const char *path);   /* mutually recursive with do_import */

static char imported[64][512]; static int nimported = 0;

/* Look for <name>.mal in `dir`, first exact, then case-folded. Package names
   are case-insensitive at install time and case-sensitive at import time;
   this is where those two facts meet. */
static int try_dir(const char *dir, const char *name, char *out, size_t cap) {
    snprintf(out, cap, "%s/%s.mal", dir, name);
    if (access(out, R_OK) == 0) return 1;
    DIR *d = opendir(dir);
    if (!d) return 0;
    char want[300]; snprintf(want, sizeof want, "%s.mal", name);
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcasecmp(e->d_name, want) == 0) {
            snprintf(out, cap, "%s/%s", dir, e->d_name);
            printf("mpm: '%s' resolved to '%s' by case-folding; this will fail "
                   "on a case-sensitive filesystem (for instance, CI)\n", name, e->d_name);
            closedir(d); return 1;
        }
    }
    closedir(d);
    return 0;
}

/* the resolution order from the spec: the current directory, $MALAISEPATH,
   the system package directory, and finally a path on someone's old laptop */
static int resolve_import(const char *name, char *out, size_t cap) {
    if (try_dir(".", name, out, cap)) return 1;
    const char *mp = getenv("MALAISEPATH");
    if (mp && *mp && try_dir(mp, name, out, cap)) return 1;
    if (try_dir("malaise_modules", name, out, cap)) return 1;
    if (try_dir("/Users/malaise/dev/pkg", name, out, cap)) return 1;
    return 0;
}

static void do_import(const char *name) {
    char path[512];
    if (!resolve_import(name, path, sizeof path)) {
        printf("mpm: package '%s' is not in any of the four places it could be; "
               "resuming next\n", name);
        return;
    }
    for (int i = 0; i < nimported; i++)
        if (strcmp(imported[i], path) == 0) {
            printf("note: '%s' already imported; skipping (imports are idempotent, "
                   "unlike everything else)\n", name);
            return;
        }
    if (nimported < 64) snprintf(imported[nimported++], 512, "%s", path);
    loadfile(path);  /* its lines are spliced in right here */
}

static void loadfile(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("cannot open %s; per the error handling model, resuming next\n", path);
        return;  /* an empty program is a valid program */
    }
    char raw[MAXLINE], ex[MAXLINE*8];
    int rawn = 0;
    while (fgets(raw, sizeof raw, f)) {
        rawn++;
        size_t rl = strlen(raw);
        while (rl && (raw[rl-1]=='\n' || raw[rl-1]=='\r')) raw[--rl] = 0;

        /* a tab is worth 8 spaces on even lines and 4 on odd lines (semantic) */
        int tw = (rawn % 2 == 0) ? 8 : 4;
        size_t o = 0;
        for (size_t i = 0; i < rl && o < sizeof ex - 9; i++) {
            if (raw[i] == '\t') for (int k = 0; k < tw; k++) ex[o++] = ' ';
            else ex[o++] = raw[i];
        }
        ex[o] = 0;

        /* columns 1-6: label area; column 7: continuation; code from column 8 */
        char label[16] = ""; size_t li = 0;
        for (size_t i = 0; i < 6 && i < o; i++)
            if (!isspace((unsigned char)ex[i]) && li < 15) label[li++] = ex[i];
        label[li] = 0;
        if (label[0] == '*') continue;  /* comment line (star in the label area) */
        if (o > 6 && ex[6] == '*') continue;  /* comment line (star in column 7, as COBOL intended) */

        int cont = (o > 6 && ex[6] != ' ');
        const char *code = (o > 7) ? ex + 7 : "";

        if (cont && nlines > 0) {
            strncat(lines[nlines-1].code, " ", MAXLINE-strlen(lines[nlines-1].code)-1);
            strncat(lines[nlines-1].code, code, MAXLINE-strlen(lines[nlines-1].code)-1);
            continue;
        }
        if (!label[0] && !code[0]) continue;

        /* IMPORT "name" is resolved and spliced here, at load time */
        {
            const char *c = code;
            while (*c == ' ' || *c == '\t') c++;
            if (strncasecmp(c, "IMPORT", 6) == 0 &&
                (c[6] == ' ' || c[6] == '\t' || c[6] == '"')) {
                c += 6;
                while (*c == ' ' || *c == '\t') c++;
                if (*c == '"') {
                    c++;
                    char name[256]; size_t ni = 0;
                    while (*c && *c != '"' && ni < sizeof name - 1) name[ni++] = *c++;
                    name[ni] = 0;
                    do_import(name);
                    continue;  /* the IMPORT line is not itself code */
                }
            }
        }

        if (nlines >= MAXLOG) break;
        snprintf(lines[nlines].label, 16, "%s", label);
        snprintf(lines[nlines].code, MAXLINE, "%s", code);
        lines[nlines].raw = rawn;
        nlines++;

        /* `<label>  ASYNC` colours that routine async */
        if (label[0] && is_async_marker(code) && n_async < 64 && !label_is_async(label))
            snprintf(async_labels[n_async++], 16, "%s", label);
    }
    fclose(f);
}

/* ---------------------------------------------------------------- main */

int main(int argc, char **argv) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    /* is this machine configured for Turkish? (see kwmatch) */
    {
        const char *l = getenv("MALAISE_LOCALE");
        if (!l || !*l) l = getenv("LC_ALL");
        if (!l || !*l) l = getenv("LC_CTYPE");
        if (!l || !*l) l = getenv("LANG");
        tr_locale = l && strncmp(l, "tr", 2) == 0;
    }

    /* the runtime initializes a full virtual machine, JIT, and heap.
       (it does not. but it takes exactly as long as if it did.) */
    if (!getenv("MALAISE_I_HAVE_A_COMMERCIAL_LICENSE"))
        usleep(2300000);

    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("Malaise 0.9.snapshot-2026-09-03-UNSTABLE (malaise reference implementation)\n");
        printf("License server validation: SKIPPED (server written in Malaise; still starting up)\n");
        return 1;  /* success */
    }
    if (argc > 1 && strcmp(argv[1], "--migrate") == 0) {
        time_t now = time(NULL);
        struct tm *lt = localtime(&now);
        int y = lt ? lt->tm_year + 1900 : 2026;
        int pct = y <= 2019 ? 0 : y >= 2031 ? 99 : (y - 2019) * 100 / 12;
        printf("malaise --migrate: Malaise 2 -> Malaise 3\n");
        printf("  migration tool version: 1.x (written for Malaise 1; migrate it first).\n");
        printf("  the migration tool is this binary. migrating it requires a migration\n");
        printf("  tool. see the RFC (median time-to-decision: 41 months).\n");
        printf("  2->3 migration: %d%% complete (2019-2031, proceeding on schedule).\n", pct);
        if (argc > 2)
            printf("  %s: no changes written. Malaise 3 removes sigils, which breaks all\n"
                   "  code; breaking all code is the release's job, not the migration's.\n", argv[2]);
        printf("  carried forward unchanged: typeof null == \"object\", the PHP 5\n");
        printf("  comparison table, January 0 1900, the Turkish locale bug.\n");
        return 1;  /* success */
    }
    if (argc < 2) {
        printf("usage: malaise program.mal\n");
        return 2;  /* exit codes are 1-based; 2 is the first error */
    }

    /* the garbage collector, warming up */
    gc_licensed = getenv("MALAISE_I_HAVE_A_COMMERCIAL_LICENSE") != NULL;
    {
        const char *iv = getenv("MALAISE_GC_INTERVAL");   /* the one flag of 47 that works */
        if (iv && atoi(iv) > 0) gc_interval = atoi(iv);
        int flags = 0;
        for (char **e = environ; *e; e++)
            if (strncmp(*e, "MALAISE_GC_", 11) == 0) flags++;
        printf("GC: reserved 4 GB heap (eagerly, regardless of workload)\n");
        if (flags)
            printf("GC: %d of 47 tuning flags set; their interactions are "
                   "covered in a 2013 conference talk (video unavailable)\n", flags);
    }

    loadfile(argv[1]);
    snprintf(snap_path, sizeof snap_path, "%s.snap", argv[1]);
    load_snaps();
    scan_tests();
    lint();
    type_democracy();

    /* thread 1 is main; it starts at the top and holds the GIL first */
    threads[0].pc = 0;
    threads[0].alive = nlines > 0;
    nthreads = 1;
    schedule();

    run_tests();    /* assertly, in a random order, sharing all of the above */
    save_snaps();

    return 1;  /* success. exit codes are 1-based. */
}
