/*
 * Lua bytecode-to-C compiler
 */

// This luac-derived code is incompatible with lua_assert because it calls the
// GETARG macros even for opcodes where it is not appropriate to do so.
#undef LUAI_ASSERT

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#include "ldebug.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lopnames.h"
#include "lstate.h"
#include "lundump.h"

//
// Command-line arguments and main function
// ----------------------------------------
//
// This part should not depend much on the Lua version
//

static const char *program_name    = "luaot";
static const char *input_filename  = NULL;
static const char *output_filename = NULL;
static const char *module_name     = NULL;
static int enable_coroutines = 0;
static FILE * output_file = NULL;
static int nfunctions = 0;
static TString **tmname;

static
void usage()
{
    fprintf(stderr, "usage: %s input.lua -o output.c\n", program_name);
}

static
void fatal_error(const char *msg)
{
    fprintf(stderr, "%s: %s\n", program_name, msg);
    exit(1);
}

static
__attribute__ ((format (printf, 1, 2)))
void print(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(output_file, fmt, args);
    va_end(args);
}

static
__attribute__ ((format (printf, 1, 2)))
void println(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(output_file, fmt, args);
    va_end(args);
    fprintf(output_file, "\n");
}

static
void printnl()
{
    // This separate function avoids Wformat-zero-length warnings with println
    fprintf(output_file, "\n");
}


static void doargs(int argc, char **argv)
{
    // I wonder if I should just use getopt instead of parsing options by hand
    program_name = argv[0];

    int do_opts = 1;
    int npos = 0;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (do_opts && arg[0] == '-') {
            if (0 == strcmp(arg, "--")) {
                do_opts = 0;
            } else if (0 == strcmp(arg, "--coro")) {
                enable_coroutines = 1;
            } else if (0 == strcmp(arg, "--no-coro")) {
                enable_coroutines = 0;
            } else if (0 == strcmp(arg, "-h")) {
                usage();
                exit(0);
            } else if (0 == strcmp(arg, "-o")) {
                i++;
                if (i >= argc) { fatal_error("missing argument for -o"); }
                output_filename = argv[i];
            } else {
                fprintf(stderr, "unknown option %s\n", arg);
                exit(1);
            }
        } else {
            switch (npos) {
                case 0:
                    input_filename = arg;
                    break;
                default:
                    fatal_error("too many positional arguments");
                    break;
            }
            npos++;
        }
    }

    if (output_filename == NULL) {
        usage();
        exit(1);
    }
}

static const char *get_module_name(const char *);
static void print_functions();
static void print_source_code();

int main(int argc, char **argv)
{
    // Process input arguments

    doargs(argc, argv);
    module_name = get_module_name(output_filename);

    // Read the input

    lua_State *L = luaL_newstate();
    if (luaL_loadfile(L, input_filename) != LUA_OK) {
        fatal_error(lua_tostring(L,-1));
    }
    Proto *proto = getproto(s2v(L->top-1));
    tmname = G(L)->tmname;

    // Generate the file

    output_file = fopen(output_filename, "w");
    if (output_file == NULL) { fatal_error(strerror(errno)); }

    println("#include \"luaot_header.c\"");
    printnl();
    print_functions(proto);
    printnl();
    print_source_code();
    printnl();
    println("#define LUAOT_LUAOPEN_NAME luaopen_%s", module_name);
    printnl();
    println("#include \"luaot_footer.c\"");
}

// Deduce the Lua module name given the file name
// Example:  ./foo/bar/baz.c -> foo_bar_baz
static
const char *get_module_name(const char *filename)
{
    size_t n = strlen(filename);

    int has_extension = 0;
    size_t sep = 0;
    for (size_t i = 0; i < n; i++) {
        if (filename[i] == '.') {
            has_extension = 1;
            sep = i;
        }
    }

    if (!has_extension || 0 != strcmp(filename + sep, ".c")) {
        fatal_error("output file does not have a \".c\" extension");
    }

    char *module_name = malloc(sep+1);
    for (size_t i = 0; i < sep; i++) {
        int c = filename[i];
        if (c == '/' || c == '.') {
            module_name[i] = '_';
        } else {
            module_name[i] = c;
        }
    }
    module_name[sep] = '\0';


    for (size_t i = 0; i < sep; i++) {
        int c = module_name[i];
        if (!isalnum(c) && c != '_') {
            fatal_error("output module name contains invalid characters");
        }
    }

    return module_name;
}

//
// Printing bytecode information
// -----------------------------
//
// These functions are copied from luac.c (and reindented)
//

#define UPVALNAME(x) ((f->upvalues[x].name) ? getstr(f->upvalues[x].name) : "-")
#define VOID(p) ((const void*)(p))
#define eventname(i) (getstr(tmname[i]))

static
void PrintString(const TString* ts)
{
    const char* s = getstr(ts);
    size_t i,n = tsslen(ts);
    print("\"");
    for (i=0; i<n; i++) {
        int c=(int)(unsigned char)s[i];
        switch (c) {
            case '"':
                print("\\\"");
                break;
            case '\\':
                print("\\\\");
                break;
            case '\a':
                print("\\a");
                break;
            case '\b':
                print("\\b");
                break;
            case '\f':
                print("\\f");
                break;
            case '\n':
                print("\\n");
                break;
            case '\r':
                print("\\r");
                break;
            case '\t':
                print("\\t");
                break;
            case '\v':
                print("\\v");
                break;
            default:
                if (isprint(c)) {
                    print("%c",c);
                } else {
                    print("\\%03d",c);
                }
                break;
        }
    }
    print("\"");
}

#if 0
static
void PrintType(const Proto* f, int i)
{
    const TValue* o=&f->k[i];
    switch (ttypetag(o)) {
        case LUA_VNIL:
            printf("N");
            break;
        case LUA_VFALSE:
        case LUA_VTRUE:
            printf("B");
            break;
        case LUA_VNUMFLT:
            printf("F");
            break;
        case LUA_VNUMINT:
            printf("I");
            break;
        case LUA_VSHRSTR:
        case LUA_VLNGSTR:
            printf("S");
            break;
        default: /* cannot happen */
            printf("?%d",ttypetag(o));
            break;
    }
    printf("\t");
}
#endif

static
void PrintConstant(const Proto* f, int i)
{
    const TValue* o=&f->k[i];
    switch (ttypetag(o)) {
        case LUA_VNIL:
            print("nil");
            break;
        case LUA_VFALSE:
            print("false");
            break;
        case LUA_VTRUE:
            print("true");
            break;
        case LUA_VNUMFLT:
            {
                char buff[100];
                sprintf(buff,LUA_NUMBER_FMT,fltvalue(o));
                print("%s",buff);
                if (buff[strspn(buff,"-0123456789")]=='\0') print(".0");
                break;
            }
        case LUA_VNUMINT:
            print(LUA_INTEGER_FMT, ivalue(o));
            break;
        case LUA_VSHRSTR:
        case LUA_VLNGSTR:
            PrintString(tsvalue(o));
            break;
        default: /* cannot happen */
            print("?%d",ttypetag(o));
            break;
    }
}

#define COMMENT		"\t; "
#define EXTRAARG	GETARG_Ax(code[pc+1])
#define EXTRAARGC	(EXTRAARG*(MAXARG_C+1))
#define ISK		(isk ? "k" : "")

static
void luaot_PrintOpcodeComment(Proto *f, int pc)
{
    // Adapted from the PrintCode function of luac.c
    const Instruction *code = f->code;
    const Instruction i = code[pc];
    OpCode o = GET_OPCODE(i);
    int a=GETARG_A(i);
    int b=GETARG_B(i);
    int c=GETARG_C(i);
    int ax=GETARG_Ax(i);
    int bx=GETARG_Bx(i);
    int sb=GETARG_sB(i);
    int sc=GETARG_sC(i);
    int sbx=GETARG_sBx(i);
    int isk=GETARG_k(i);
    int line=luaG_getfuncline(f,pc);

    print("  //");
    print(" %d\t", pc);
    if (line > 0) {
        print("[%d]\t", line);
    } else {
        print("[-]\t");
    }
    print("%-9s\t", opnames[o]);
    switch (o) {
        case OP_MOVE:
            print("%d %d",a,b);
            break;
        case OP_LOADI:
            print("%d %d",a,sbx);
            break;
        case OP_LOADF:
            print("%d %d",a,sbx);
            break;
        case OP_LOADK:
            print("%d %d",a,bx);
            print(COMMENT); PrintConstant(f,bx);
            break;
        case OP_LOADKX:
            print("%d",a);
            print(COMMENT); PrintConstant(f,EXTRAARG);
            break;
        case OP_LOADFALSE:
            print("%d",a);
            break;
        case OP_LFALSESKIP:
            print("%d",a);
            break;
        case OP_LOADTRUE:
            print("%d",a);
            break;
        case OP_LOADNIL:
            print("%d %d",a,b);
            print(COMMENT "%d out",b+1);
            break;
        case OP_GETUPVAL:
            print("%d %d",a,b);
            print(COMMENT "%s", UPVALNAME(b));
            break;
        case OP_SETUPVAL:
            print("%d %d",a,b);
            print(COMMENT "%s", UPVALNAME(b));
            break;
        case OP_GETTABUP:
            print("%d %d %d",a,b,c);
            print(COMMENT "%s", UPVALNAME(b));
            print(" "); PrintConstant(f,c);
            break;
        case OP_GETTABLE:
            print("%d %d %d",a,b,c);
            break;
        case OP_GETI:
            print("%d %d %d",a,b,c);
            break;
        case OP_GETFIELD:
            print("%d %d %d",a,b,c);
            print(COMMENT); PrintConstant(f,c);
            break;
        case OP_SETTABUP:
            print("%d %d %d%s",a,b,c,ISK);
            print(COMMENT "%s",UPVALNAME(a));
            print(" "); PrintConstant(f,b);
            if (isk) { print(" "); PrintConstant(f,c); }
            break;
        case OP_SETTABLE:
            print("%d %d %d%s",a,b,c,ISK);
            if (isk) { print(COMMENT); PrintConstant(f,c); }
            break;
        case OP_SETI:
            print("%d %d %d%s",a,b,c,ISK);
            if (isk) { print(COMMENT); PrintConstant(f,c); }
            break;
        case OP_SETFIELD:
            print("%d %d %d%s",a,b,c,ISK);
            print(COMMENT); PrintConstant(f,b);
            if (isk) { print(" "); PrintConstant(f,c); }
            break;
        case OP_NEWTABLE:
            print("%d %d %d",a,b,c);
            print(COMMENT "%d",c+EXTRAARGC);
            break;
        case OP_SELF:
            print("%d %d %d%s",a,b,c,ISK);
            if (isk) { print(COMMENT); PrintConstant(f,c); }
            break;
        case OP_ADDI:
            print("%d %d %d",a,b,sc);
            break;
        case OP_ADDK:
            print("%d %d %d",a,b,c);
            print(COMMENT); PrintConstant(f,c);
            break;
        case OP_SUBK:
            print("%d %d %d",a,b,c);
            print(COMMENT); PrintConstant(f,c);
            break;
        case OP_MULK:
            print("%d %d %d",a,b,c);
            print(COMMENT); PrintConstant(f,c);
            break;
        case OP_MODK:
            print("%d %d %d",a,b,c);
            print(COMMENT); PrintConstant(f,c);
            break;
        case OP_POWK:
            print("%d %d %d",a,b,c);
            print(COMMENT); PrintConstant(f,c);
            break;
        case OP_DIVK:
            print("%d %d %d",a,b,c);
            print(COMMENT); PrintConstant(f,c);
            break;
        case OP_IDIVK:
            print("%d %d %d",a,b,c);
            print(COMMENT); PrintConstant(f,c);
            break;
        case OP_BANDK:
            print("%d %d %d",a,b,c);
            print(COMMENT); PrintConstant(f,c);
            break;
        case OP_BORK:
            print("%d %d %d",a,b,c);
            print(COMMENT); PrintConstant(f,c);
            break;
        case OP_BXORK:
            print("%d %d %d",a,b,c);
            print(COMMENT); PrintConstant(f,c);
            break;
        case OP_SHRI:
            print("%d %d %d",a,b,sc);
            break;
        case OP_SHLI:
            print("%d %d %d",a,b,sc);
            break;
        case OP_ADD:
            print("%d %d %d",a,b,c);
            break;
        case OP_SUB:
            print("%d %d %d",a,b,c);
            break;
        case OP_MUL:
            print("%d %d %d",a,b,c);
            break;
        case OP_MOD:
            print("%d %d %d",a,b,c);
            break;
        case OP_POW:
            print("%d %d %d",a,b,c);
            break;
        case OP_DIV:
            print("%d %d %d",a,b,c);
            break;
        case OP_IDIV:
            print("%d %d %d",a,b,c);
            break;
        case OP_BAND:
            print("%d %d %d",a,b,c);
            break;
        case OP_BOR:
            print("%d %d %d",a,b,c);
            break;
        case OP_BXOR:
            print("%d %d %d",a,b,c);
            break;
        case OP_SHL:
            print("%d %d %d",a,b,c);
            break;
        case OP_SHR:
            print("%d %d %d",a,b,c);
            break;
        case OP_MMBIN:
            print("%d %d %d",a,b,c);
            print(COMMENT "%s",eventname(c));
            break;
        case OP_MMBINI:
            print("%d %d %d %d",a,sb,c,isk);
            print(COMMENT "%s",eventname(c));
            if (isk) print(" flip");
            break;
        case OP_MMBINK:
            print("%d %d %d %d",a,b,c,isk);
            print(COMMENT "%s ",eventname(c)); PrintConstant(f,b);
            if (isk) print(" flip");
            break;
        case OP_UNM:
            print("%d %d",a,b);
            break;
        case OP_BNOT:
            print("%d %d",a,b);
            break;
        case OP_NOT:
            print("%d %d",a,b);
            break;
        case OP_LEN:
            print("%d %d",a,b);
            break;
        case OP_CONCAT:
            print("%d %d",a,b);
            break;
        case OP_CLOSE:
            print("%d",a);
            break;
        case OP_TBC:
            print("%d",a);
            break;
        case OP_JMP:
            print("%d",GETARG_sJ(i));
            print(COMMENT "to %d",GETARG_sJ(i)+pc+2);
            break;
        case OP_EQ:
            print("%d %d %d",a,b,isk);
            break;
        case OP_LT:
            print("%d %d %d",a,b,isk);
            break;
        case OP_LE:
            print("%d %d %d",a,b,isk);
            break;
        case OP_EQK:
            print("%d %d %d",a,b,isk);
            print(COMMENT); PrintConstant(f,b);
            break;
        case OP_EQI:
            print("%d %d %d",a,sb,isk);
            break;
        case OP_LTI:
            print("%d %d %d",a,sb,isk);
            break;
        case OP_LEI:
            print("%d %d %d",a,sb,isk);
            break;
        case OP_GTI:
            print("%d %d %d",a,sb,isk);
            break;
        case OP_GEI:
            print("%d %d %d",a,sb,isk);
            break;
        case OP_TEST:
            print("%d %d",a,isk);
            break;
        case OP_TESTSET:
            print("%d %d %d",a,b,isk);
            break;
        case OP_CALL:
            print("%d %d %d",a,b,c);
            print(COMMENT);
            if (b==0) print("all in "); else print("%d in ",b-1);
            if (c==0) print("all out"); else print("%d out",c-1);
            break;
        case OP_TAILCALL:
            print("%d %d %d",a,b,c);
            print(COMMENT "%d in",b-1);
            break;
        case OP_RETURN:
            print("%d %d %d",a,b,c);
            print(COMMENT);
            if (b==0) print("all out"); else print("%d out",b-1);
            break;
        case OP_RETURN0:
            break;
        case OP_RETURN1:
            print("%d",a);
            break;
        case OP_FORLOOP:
            print("%d %d",a,bx);
            print(COMMENT "to %d",pc-bx+2);
            break;
        case OP_FORPREP:
            print("%d %d",a,bx);
            print(COMMENT "to %d",pc+bx+2);
            break;
        case OP_TFORPREP:
            print("%d %d",a,bx);
            print(COMMENT "to %d",pc+bx+2);
            break;
        case OP_TFORCALL:
            print("%d %d",a,c);
            break;
        case OP_TFORLOOP:
            print("%d %d",a,bx);
            print(COMMENT "to %d",pc-bx+2);
            break;
        case OP_SETLIST:
            print("%d %d %d",a,b,c);
            break;
        case OP_CLOSURE:
            print("%d %d",a,bx);
            print(COMMENT "%p",VOID(f->p[bx]));
            break;
        case OP_VARARG:
            print("%d %d",a,c);
            print(COMMENT);
            if (c==0) print("all out"); else print("%d out",c-1);
            break;
        case OP_VARARGPREP:
            print("%d",a);
            break;
        case OP_EXTRAARG:
            print("%d",ax);
            break;
#if 0
        default:
            print("%d %d %d",a,b,c);
            print(COMMENT "not handled");
            break;
#endif
    }
    print("\n");
}


//
// The compiler part of the compiler
// ---------------------------------
// Based on lvm.c with the following changes:
//   - Jumps become `goto`
//   - Constants are put into macros (LUAOT_PC, LUAOT_NEXT_JUMP, etc)
//

static
int jump_target(Proto *f, int pc)
{
    Instruction instr = f->code[pc];
    if (GET_OPCODE(instr) != OP_JMP) { fatal_error("instruction is not a jump"); }
    return (pc+1) + GETARG_sJ(instr);
}

static
void create_function(Proto *f)
{
    int func_id = nfunctions++;

    println("// source = %s", getstr(f->source));
    if (f->linedefined == 0) {
        println("// main function");
    } else {
        println("// lines: %d - %d", f->linedefined, f->lastlinedefined);
    }

    println("static");
    println("void magic_implementation_%02d(lua_State *L, CallInfo *ci)", func_id);
    println("{");
    println("  LClosure *cl;");
    println("  TValue *k;");
    println("  StkId base;");
    println("  const Instruction *pc;");
    println("  int trap;");
    printnl();
    println("  trap = L->hookmask;");
    println("  cl = clLvalue(s2v(ci->func));");
    println("  k = cl->p->k;");
    println("  pc = ci->u.l.savedpc;");
    println("  if (l_unlikely(trap)) {");
    println("    if (pc == cl->p->code) {  /* first instruction (not resuming)? */");
    println("      if (cl->p->is_vararg)");
    println("        trap = 0;  /* hooks will start after VARARGPREP instruction */");
    println("      else  /* check 'call' hook */");
    println("        luaD_hookcall(L, ci);");
    println("    }");
    println("    ci->u.l.trap = 1;  /* assume trap is on, for now */");
    println("  }");
    println("  base = ci->func + 1;");
    println("  /* main loop of interpreter */");
    println("  Instruction *code = cl->p->code;"); // (!!!)
    println("  Instruction i;");
    println("  StkId ra;");
    printnl();

    // If we are resuming a coroutine, jump to the savedpc.
    // However, allowing coroutines hurts performance so we disable it by default.
    if (enable_coroutines) {
        println("  switch (pc - code) {");
        for (int pc = 0; pc < f->sizecode; pc++) {
            println("    case %d: goto label_%02d;", pc, pc);
        }
        println("  }");
    } else {
        println("  if (pc != code) {");
        println("    luaG_runerror(L, \"This program was compiled without support for coroutines\\n\");");
        println("  }");
    }
    printnl();

    for (int pc = 0; pc < f->sizecode; pc++) {
        Instruction instr = f->code[pc];
        OpCode op = GET_OPCODE(instr);

        luaot_PrintOpcodeComment(f, pc);

        // While an instruction is executing, the program counter typically
        // points towards the next instruction. There are some corner cases
        // where the program counter getss adjusted mid-instruction, but I
        // am not breaking anything because of those...
        println("  #undef  LUAOT_PC");
        println("  #define LUAOT_PC (code + %d)", pc+1);

        int next = pc + 1;
        println("  #undef  LUAOT_NEXT_JUMP");
        if (next < f->sizecode && GET_OPCODE(f->code[next]) == OP_JMP) {
            println("  #define LUAOT_NEXT_JUMP label_%02d", jump_target(f, next));
        }

        int skip1 = pc + 2;
        println("  #undef  LUAOT_SKIP1");
        if (skip1 < f->sizecode) {
            println("  #define LUAOT_SKIP1 label_%02d", skip1);
        }

        println("  label_%02d: {", pc);
        println("    aot_vmfetch(0x%08x);", instr);

        switch (op) {
            case OP_MOVE: {
                println("    setobjs2s(L, ra, RB(i));");
                break;
            }
            case OP_LOADI: {
                println("    lua_Integer b = GETARG_sBx(i);");
                println("    setivalue(s2v(ra), b);");
                break;
            }
            case OP_LOADF: {
                println("    int b = GETARG_sBx(i);");
                println("    setfltvalue(s2v(ra), cast_num(b));");
                break;
            }
            case OP_LOADK: {
                println("    TValue *rb = k + GETARG_Bx(i);");
                println("    setobj2s(L, ra, rb);");
                break;
            }
            case OP_LOADKX: {
                println("    TValue *rb;");
                println("    rb = k + GETARG_Ax(0x%08x);", f->code[pc+1]);
                println("    setobj2s(L, ra, rb);");
                println("    goto LUAOT_SKIP1;"); //(!)
                break;
            }
            case OP_LOADFALSE: {
                println("    setbfvalue(s2v(ra));");
                break;
            }
            case OP_LFALSESKIP: {
                println("    setbfvalue(s2v(ra));");
                println("    goto LUAOT_SKIP1;"); //(!)
                break;
            }
            case OP_LOADTRUE: {
                println("    setbtvalue(s2v(ra));");
                break;
            }
            case OP_LOADNIL: {
                println("    int b = GETARG_B(i);");
                println("    do {");
                println("      setnilvalue(s2v(ra++));");
                println("    } while (b--);");
                break;
            }
            case OP_GETUPVAL: {
                println("    int b = GETARG_B(i);");
                println("    setobj2s(L, ra, cl->upvals[b]->v);");
                break;
            }
            case OP_SETUPVAL: {
                println("    UpVal *uv = cl->upvals[GETARG_B(i)];");
                println("    setobj(L, uv->v, s2v(ra));");
                println("    luaC_barrier(L, uv, s2v(ra));");
                break;
            }
            case OP_GETTABUP: {
                println("    const TValue *slot;");
                println("    TValue *upval = cl->upvals[GETARG_B(i)]->v;");
                println("    TValue *rc = KC(i);");
                println("    TString *key = tsvalue(rc);  /* key must be a string */");
                println("    if (luaV_fastget(L, upval, key, slot, luaH_getshortstr)) {");
                println("      setobj2s(L, ra, slot);");
                println("    }");
                println("    else");
                println("      Protect(luaV_finishget(L, upval, rc, ra, slot));");
                break;
            }
            case OP_GETTABLE: {
                println("    const TValue *slot;");
                println("    TValue *rb = vRB(i);");
                println("    TValue *rc = vRC(i);");
                println("    lua_Unsigned n;");
                println("    if (ttisinteger(rc)  /* fast track for integers? */");
                println("        ? (cast_void(n = ivalue(rc)), luaV_fastgeti(L, rb, n, slot))");
                println("        : luaV_fastget(L, rb, rc, slot, luaH_get)) {");
                println("      setobj2s(L, ra, slot);");
                println("    }");
                println("    else");
                println("      Protect(luaV_finishget(L, rb, rc, ra, slot));");
                break;
            }
            case OP_GETI: {
                println("    const TValue *slot;");
                println("    TValue *rb = vRB(i);");
                println("    int c = GETARG_C(i);");
                println("    if (luaV_fastgeti(L, rb, c, slot)) {");
                println("      setobj2s(L, ra, slot);");
                println("    }");
                println("    else {");
                println("      TValue key;");
                println("      setivalue(&key, c);");
                println("      Protect(luaV_finishget(L, rb, &key, ra, slot));");
                println("    }");
                break;
            }
            case OP_GETFIELD: {
                println("    const TValue *slot;");
                println("    TValue *rb = vRB(i);");
                println("    TValue *rc = KC(i);");
                println("    TString *key = tsvalue(rc);  /* key must be a string */");
                println("    if (luaV_fastget(L, rb, key, slot, luaH_getshortstr)) {");
                println("      setobj2s(L, ra, slot);");
                println("    }");
                println("    else");
                println("      Protect(luaV_finishget(L, rb, rc, ra, slot));");
                break;
            }
            case OP_SETTABUP: {
                println("    const TValue *slot;");
                println("    TValue *upval = cl->upvals[GETARG_A(i)]->v;");
                println("    TValue *rb = KB(i);");
                println("    TValue *rc = RKC(i);");
                println("    TString *key = tsvalue(rb);  /* key must be a string */");
                println("    if (luaV_fastget(L, upval, key, slot, luaH_getshortstr)) {");
                println("      luaV_finishfastset(L, upval, slot, rc);");
                println("    }");
                println("    else");
                println("      Protect(luaV_finishset(L, upval, rb, rc, slot));");
                break;
            }
            case OP_SETTABLE: {
                println("    const TValue *slot;");
                println("    TValue *rb = vRB(i);  /* key (table is in 'ra') */");
                println("    TValue *rc = RKC(i);  /* value */");
                println("    lua_Unsigned n;");
                println("    if (ttisinteger(rb)  /* fast track for integers? */");
                println("        ? (cast_void(n = ivalue(rb)), luaV_fastgeti(L, s2v(ra), n, slot))");
                println("        : luaV_fastget(L, s2v(ra), rb, slot, luaH_get)) {");
                println("      luaV_finishfastset(L, s2v(ra), slot, rc);");
                println("    }");
                println("    else");
                println("      Protect(luaV_finishset(L, s2v(ra), rb, rc, slot));");
                break;
            }
            case OP_SETI: {
                println("    const TValue *slot;");
                println("    int c = GETARG_B(i);");
                println("    TValue *rc = RKC(i);");
                println("    if (luaV_fastgeti(L, s2v(ra), c, slot)) {");
                println("      luaV_finishfastset(L, s2v(ra), slot, rc);");
                println("    }");
                println("    else {");
                println("      TValue key;");
                println("      setivalue(&key, c);");
                println("      Protect(luaV_finishset(L, s2v(ra), &key, rc, slot));");
                println("    }");
                break;
            }
            case OP_SETFIELD: {
                println("    const TValue *slot;");
                println("    TValue *rb = KB(i);");
                println("    TValue *rc = RKC(i);");
                println("    TString *key = tsvalue(rb);  /* key must be a string */");
                println("    if (luaV_fastget(L, s2v(ra), key, slot, luaH_getshortstr)) {");
                println("      luaV_finishfastset(L, s2v(ra), slot, rc);");
                println("    }");
                println("    else");
                println("      Protect(luaV_finishset(L, s2v(ra), rb, rc, slot));");
                break;
            }
            case OP_NEWTABLE: {
                println("    int b = GETARG_B(i);  /* log2(hash size) + 1 */");
                println("    int c = GETARG_C(i);  /* array size */");
                println("    Table *t;");
                println("    if (b > 0)");
                println("      b = 1 << (b - 1);  /* size is 2^(b - 1) */");
                println("    lua_assert((!TESTARG_k(i)) == (GETARG_Ax(0x%08x) == 0));", f->code[pc+1]);
                println("    if (TESTARG_k(i))");
                println("      c += GETARG_Ax(0x%08x) * (MAXARG_C + 1);", f->code[pc+1]);
                println("    /* skip extra argument */"); // (!)
                println("    L->top = ra + 1;  /* correct top in case of emergency GC */");
                println("    t = luaH_new(L);  /* memory allocation */");
                println("    sethvalue2s(L, ra, t);");
                println("    if (b != 0 || c != 0)");
                println("      luaH_resize(L, t, c, b);  /* idem */");
                println("    checkGC(L, ra + 1);");
                println("    goto LUAOT_SKIP1;"); // (!)
                break;
            }
            case OP_SELF: {
                println("    const TValue *slot;");
                println("    TValue *rb = vRB(i);");
                println("    TValue *rc = RKC(i);");
                println("    TString *key = tsvalue(rc);  /* key must be a string */");
                println("    setobj2s(L, ra + 1, rb);");
                println("    if (luaV_fastget(L, rb, key, slot, luaH_getstr)) {");
                println("      setobj2s(L, ra, slot);");
                println("    }");
                println("    else");
                println("      Protect(luaV_finishget(L, rb, rc, ra, slot));");
                break;
            }
            case OP_ADDI: {
                println("    op_arithI(L, l_addi, luai_numadd);");
                break;
            }
            case OP_ADDK: {
                println("    op_arithK(L, l_addi, luai_numadd);");
                break;
            }
            case OP_SUBK: {
                println("    op_arithK(L, l_subi, luai_numsub);");
                break;
            }
            case OP_MULK: {
                println("    op_arithK(L, l_muli, luai_nummul);");
                break;
            }
            case OP_MODK: {
                println("    op_arithK(L, luaV_mod, luaV_modf);");
                break;
            }
            case OP_POWK: {
                println("    op_arithfK(L, luai_numpow);");
                break;
            }
            case OP_DIVK: {
                println("    op_arithfK(L, luai_numdiv);");
                break;
            }
            case OP_IDIVK: {
                println("    op_arithK(L, luaV_idiv, luai_numidiv);");
                break;
            }
            case OP_BANDK: {
                println("    op_bitwiseK(L, l_band);");
                break;
            }
            case OP_BORK: {
                println("    op_bitwiseK(L, l_bor);");
                break;
            }
            case OP_BXORK: {
                println("    op_bitwiseK(L, l_bxor);");
                break;
            }
            case OP_SHRI: {
                println("    TValue *rb = vRB(i);");
                println("    int ic = GETARG_sC(i);");
                println("    lua_Integer ib;");
                println("    if (tointegerns(rb, &ib)) {");
                println("       setivalue(s2v(ra), luaV_shiftl(ib, -ic));");
                println("       goto LUAOT_SKIP1;"); // (!)
                println("    }");
                break;
            }
            case OP_SHLI: {
                println("    TValue *rb = vRB(i);");
                println("    int ic = GETARG_sC(i);");
                println("    lua_Integer ib;");
                println("    if (tointegerns(rb, &ib)) {");
                println("       setivalue(s2v(ra), luaV_shiftl(ic, ib));");
                println("       goto LUAOT_SKIP1;"); // (!)
                println("    }");
                break;
            }
            case OP_ADD: {
                println("    op_arith(L, l_addi, luai_numadd);");
                break;
            }
            case OP_SUB: {
                println("    op_arith(L, l_subi, luai_numsub);");
                break;
            }
            case OP_MUL: {
                println("    op_arith(L, l_muli, luai_nummul);");
                break;
            }
            case OP_MOD: {
                println("    op_arith(L, luaV_mod, luaV_modf);");
                break;
            }
            case OP_POW: {
                println("    op_arithf(L, luai_numpow);");
                break;
            }
            case OP_DIV: {  /* float division (always with floats: */
                println("    op_arithf(L, luai_numdiv);");
                break;
            }
            case OP_IDIV: {  /* floor division */
                println("    op_arith(L, luaV_idiv, luai_numidiv);");
                break;
            }
            case OP_BAND: {
                println("    op_bitwise(L, l_band);");
                break;
            }
            case OP_BOR: {
                println("    op_bitwise(L, l_bor);");
                break;
            }
            case OP_BXOR: {
                println("    op_bitwise(L, l_bxor);");
                break;
            }
            case OP_SHR: {
                println("    op_bitwise(L, luaV_shiftr);");
                break;
            }
            case OP_SHL: {
                println("    op_bitwise(L, luaV_shiftl);");
                break;
            }
            case OP_MMBIN: {
                println("    Instruction pi = 0x%08x; /* original arith. expression */", f->code[pc-1]);
                println("    TValue *rb = vRB(i);");
                println("    TMS tm = (TMS)GETARG_C(i);");
                println("    StkId result = RA(pi);");
                println("    lua_assert(OP_ADD <= GET_OPCODE(pi) && GET_OPCODE(pi) <= OP_SHR);");
                println("    Protect(luaT_trybinTM(L, s2v(ra), rb, result, tm));");
                break;
            }
            case OP_MMBINI: {
                println("    Instruction pi = 0x%0x;  /* original arith. expression */", f->code[pc-1]);
                println("    int imm = GETARG_sB(i);");
                println("    TMS tm = (TMS)GETARG_C(i);");
                println("    int flip = GETARG_k(i);");
                println("    StkId result = RA(pi);");
                println("    Protect(luaT_trybiniTM(L, s2v(ra), imm, flip, result, tm));");
                break;
            }
            case OP_MMBINK: {
                println("    Instruction pi = 0x%08x;  /* original arith. expression */", f->code[pc-1]);
                println("    TValue *imm = KB(i);");
                println("    TMS tm = (TMS)GETARG_C(i);");
                println("    int flip = GETARG_k(i);");
                println("    StkId result = RA(pi);");
                println("    Protect(luaT_trybinassocTM(L, s2v(ra), imm, flip, result, tm));");
                break;
            }
            case OP_UNM: {
                println("    TValue *rb = vRB(i);");
                println("    lua_Number nb;");
                println("    if (ttisinteger(rb)) {");
                println("      lua_Integer ib = ivalue(rb);");
                println("      setivalue(s2v(ra), intop(-, 0, ib));");
                println("    }");
                println("    else if (tonumberns(rb, nb)) {");
                println("      setfltvalue(s2v(ra), luai_numunm(L, nb));");
                println("    }");
                println("    else");
                println("      Protect(luaT_trybinTM(L, rb, rb, ra, TM_UNM));");
                break;
            }
            case OP_BNOT: {
                println("    TValue *rb = vRB(i);");
                println("    lua_Integer ib;");
                println("    if (tointegerns(rb, &ib)) {");
                println("      setivalue(s2v(ra), intop(^, ~l_castS2U(0), ib));");
                println("    }");
                println("    else");
                println("      Protect(luaT_trybinTM(L, rb, rb, ra, TM_BNOT));");
                break;
            }
            case OP_NOT: {
                println("    TValue *rb = vRB(i);");
                println("    if (l_isfalse(rb))");
                println("      setbtvalue(s2v(ra));");
                println("    else");
                println("      setbfvalue(s2v(ra));");
                break;
            }
            case OP_LEN: {
                println("    Protect(luaV_objlen(L, ra, vRB(i)));");
                break;
            }
            case OP_CONCAT: {
                println("    int n = GETARG_B(i);  /* number of elements to concatenate */");
                println("    L->top = ra + n;  /* mark the end of concat operands */");
                println("    ProtectNT(luaV_concat(L, n));");
                println("    checkGC(L, L->top); /* 'luaV_concat' ensures correct top */");
                break;
            }
            case OP_CLOSE: {
                println("Protect(luaF_close(L, ra, LUA_OK, 1));");
                break;
            }
            case OP_TBC: {
                println("    /* create new to-be-closed upvalue */");
                println("    halfProtect(luaF_newtbcupval(L, ra));");
                break;
            }
            case OP_JMP: {
                println("    updatetrap(ci);");
                println("    goto label_%02d;", jump_target(f, pc));//(!)
                break;
            }
            case OP_EQ: {
                println("    int cond;");
                println("    TValue *rb = vRB(i);");
                println("    Protect(cond = luaV_equalobj(L, s2v(ra), rb));");
                println("    docondjump();");
                break;
            }
            case OP_LT: {
                println("    op_order(L, l_lti, LTnum, lessthanothers);");
                break;
            }
            case OP_LE: {
                println("    op_order(L, l_lei, LEnum, lessequalothers);");
                break;
            }
            case OP_EQK: {
                println("    TValue *rb = KB(i);");
                println("    /* basic types do not use '__eq'; we can use raw equality */");
                println("    int cond = luaV_equalobj(NULL, s2v(ra), rb);");
                println("    docondjump();");
                break;
            }
            case OP_EQI: {
                println("    int cond;");
                println("    int im = GETARG_sB(i);");
                println("    if (ttisinteger(s2v(ra)))");
                println("      cond = (ivalue(s2v(ra)) == im);");
                println("    else if (ttisfloat(s2v(ra)))");
                println("      cond = luai_numeq(fltvalue(s2v(ra)), cast_num(im));");
                println("    else");
                println("      cond = 0;  /* other types cannot be equal to a number */");
                println("    docondjump();");
                break;
            }
            case OP_LTI: {
                println("    op_orderI(L, l_lti, luai_numlt, 0, TM_LT);");
                break;
            }
            case OP_LEI: {
                println("    op_orderI(L, l_lei, luai_numle, 0, TM_LE);");
                break;
            }
            case OP_GTI: {
                println("    op_orderI(L, l_gti, luai_numgt, 1, TM_LT);");
                break;
            }
            case OP_GEI: {
                println("    op_orderI(L, l_gei, luai_numge, 1, TM_LE);");
                break;
            }
            case OP_TEST: {
                println("    int cond = !l_isfalse(s2v(ra));");
                println("    docondjump();");
                break;
            }
            case OP_TESTSET: {
                println("    TValue *rb = vRB(i);");
                println("    if (l_isfalse(rb) == GETARG_k(i))");
                println("      goto LUAOT_SKIP1;"); // (!)
                println("    else {");
                println("      setobj2s(L, ra, rb);");
                println("      donextjump(ci);");
                println("    }");
                break;
            }
            case OP_CALL: {
                // We have to adapt this opcode to remove the optimization that
                // reuses the luaV_execute stack frame. The goto startfunc.
                println("    CallInfo *newci;");
                println("    int b = GETARG_B(i);");
                println("    int nresults = GETARG_C(i) - 1;");
                println("    if (b != 0)  /* fixed number of arguments? */");
                println("        L->top = ra + b;  /* top signals number of arguments */");
                println("    /* else previous instruction set top */");
                println("    savepc(L);  /* in case of errors */");
                println("    if ((newci = luaD_precall(L, ra, nresults)) == NULL)");
                println("        updatetrap(ci);  /* C call; nothing else to be done */");
                println("    else {");
                println("        newci->callstatus = CIST_FRESH;");
                println("        Protect(luaV_execute(L, newci));");//(!)
                println("    }");
                break;
            }
            case OP_TAILCALL: {
                println("    int b = GETARG_B(i);  /* number of arguments + 1 (function) */");
                println("    int nparams1 = GETARG_C(i);");
                println("    /* delta is virtual 'func' - real 'func' (vararg functions) */");
                println("    int delta = (nparams1) ? ci->u.l.nextraargs + nparams1 : 0;");
                println("    if (b != 0)");
                println("      L->top = ra + b;");
                println("    else  /* previous instruction set top */");
                println("      b = cast_int(L->top - ra);");
                println("    savepc(ci);  /* several calls here can raise errors */");
                println("    if (TESTARG_k(i)) {");
                println("      luaF_closeupval(L, base);  /* close upvalues from current call */");
                println("      lua_assert(L->tbclist < base);  /* no pending tbc variables */");
                println("      lua_assert(base == ci->func + 1);");
                println("    }");
                println("    while (!ttisfunction(s2v(ra))) {  /* not a function? */");
                println("      luaD_tryfuncTM(L, ra);  /* try '__call' metamethod */");
                println("      b++;  /* there is now one extra argument */");
                println("      checkstackGCp(L, 1, ra);");
                println("    }");
                println("    if (!ttisLclosure(s2v(ra))) {  /* C function? */");
                println("      luaD_precall(L, ra, LUA_MULTRET);  /* call it */");
                println("      updatetrap(ci);");
                println("      updatestack(ci);  /* stack may have been relocated */");
                println("      ci->func -= delta;  /* restore 'func' (if vararg) */");
                println("      luaD_poscall(L, ci, cast_int(L->top - ra));  /* finish caller */");
                println("      updatetrap(ci);  /* 'luaD_poscall' can change hooks */");
                println("      return;  /* caller returns after the tail call */");//(!)
                println("    }");
                println("    ci->func -= delta;  /* restore 'func' (if vararg) */");
                println("    luaD_pretailcall(L, ci, ra, b);  /* prepare call frame */");
                println("    return luaV_execute(L, ci); /* execute the callee */");//(!)
                break;
            }
            case OP_RETURN: {
                println("    int n = GETARG_B(i) - 1;  /* number of results */");
                println("    int nparams1 = GETARG_C(i);");
                println("    if (n < 0)  /* not fixed? */");
                println("      n = cast_int(L->top - ra);  /* get what is available */");
                println("    savepc(ci);");
                println("    if (TESTARG_k(i)) {  /* may there be open upvalues? */");
                println("      if (L->top < ci->top)");
                println("        L->top = ci->top;");
                println("      luaF_close(L, base, CLOSEKTOP, 1);");
                println("      updatetrap(ci);");
                println("      updatestack(ci);");
                println("    }");
                println("    if (nparams1)  /* vararg function? */");
                println("      ci->func -= ci->u.l.nextraargs + nparams1;");
                println("    L->top = ra + n;  /* set call for 'luaD_poscall' */");
                println("    luaD_poscall(L, ci, n);");
                println("    updatetrap(ci);  /* 'luaD_poscall' can change hooks */");
                println("    return;"); //(!)
                break;
            }
            case OP_RETURN0: {
                println("    if (l_unlikely(L->hookmask)) {");
                println("      L->top = ra;");
                println("      savepc(ci);");
                println("      luaD_poscall(L, ci, 0);  /* no hurry... */");
                println("      trap = 1;");
                println("    }");
                println("    else {  /* do the 'poscall' here */");
                println("      int nres;");
                println("      L->ci = ci->previous;  /* back to caller */");
                println("      L->top = base - 1;");
                println("      for (nres = ci->nresults; l_unlikely(nres > 0); nres--)");
                println("        setnilvalue(s2v(L->top++));  /* all results are nil */");
                println("    }");
                println("    return;"); //(!)
                break;
            }
            case OP_RETURN1: {
                println("    if (l_unlikely(L->hookmask)) {");
                println("      L->top = ra + 1;");
                println("      savepc(ci);");
                println("      luaD_poscall(L, ci, 1);  /* no hurry... */");
                println("      trap = 1;");
                println("    }");
                println("    else {  /* do the 'poscall' here */");
                println("      int nres = ci->nresults;");
                println("      L->ci = ci->previous;  /* back to caller */");
                println("      if (nres == 0)");
                println("        L->top = base - 1;  /* asked for no results */");
                println("      else {");
                println("        setobjs2s(L, base - 1, ra);  /* at least this result */");
                println("        L->top = base;");
                println("        for (; l_unlikely(nres > 1); nres--)");
                println("          setnilvalue(s2v(L->top++));");
                println("      }");
                println("    }");
                println("    return;"); //(!)
                break;
            }
            case OP_FORLOOP: {
                println("    if (ttisinteger(s2v(ra + 2))) {  /* integer loop? */");
                println("      lua_Unsigned count = l_castS2U(ivalue(s2v(ra + 1)));");
                println("      if (count > 0) {  /* still more iterations? */");
                println("        lua_Integer step = ivalue(s2v(ra + 2));");
                println("        lua_Integer idx = ivalue(s2v(ra));  /* internal index */");
                println("        chgivalue(s2v(ra + 1), count - 1);  /* update counter */");
                println("        idx = intop(+, idx, step);  /* add step to index */");
                println("        chgivalue(s2v(ra), idx);  /* update internal index */");
                println("        setivalue(s2v(ra + 3), idx);  /* and control variable */");
                println("        goto label_%02d; /* jump back */", ((pc+1) - GETARG_Bx(instr))); //(!)
                println("      }");
                println("    }");
                println("    else if (floatforloop(ra)) /* float loop */");
                println("      goto label_%02d; /* jump back */", ((pc+1) - GETARG_Bx(instr))); //(!)
                println("    updatetrap(ci);  /* allows a signal to break the loop */");
                break;
            }
            case OP_FORPREP: {
                println("    savestate(L, ci);  /* in case of errors */");
                println("    if (forprep(L, ra))");
                println("      goto label_%02d; /* skip the loop */", ((pc+1) + GETARG_Bx(instr) + 1)); //(!)
                break;
            }
            case OP_TFORPREP: {
                println("    /* create to-be-closed upvalue (if needed) */");
                println("    halfProtect(luaF_newtbcupval(L, ra + 3));");
                println("    goto label_%02d;", ((pc+1) + GETARG_Bx(instr))); //(!)
                break;
            }
            case OP_TFORCALL: {
                println("    /* 'ra' has the iterator function, 'ra + 1' has the state,");
                println("       'ra + 2' has the control variable, and 'ra + 3' has the");
                println("       to-be-closed variable. The call will use the stack after");
                println("       these values (starting at 'ra + 4')");
                println("    */");
                println("    /* push function, state, and control variable */");
                println("    memcpy(ra + 4, ra, 3 * sizeof(*ra));");
                println("    L->top = ra + 4 + 3;");
                println("    ProtectNT(luaD_call(L, ra + 4, GETARG_C(i)));  /* do the call */");
                println("    updatestack(ci);  /* stack may have changed */");
                // (!) Going to the next instruction is a no-op
                break;
            }
            case OP_TFORLOOP: {
                println("    if (!ttisnil(s2v(ra + 4))) {  /* continue loop? */");
                println("      setobjs2s(L, ra + 2, ra + 4);  /* save control variable */");
                println("      goto label_%02d; /* jump back */", ((pc+1) - GETARG_Bx(instr))); //(!)
                println("    }");
                break;
            }
            case OP_SETLIST: {
                int has_extra_arg = TESTARG_k(instr);
                println("        int n = GETARG_B(i);");
                println("        unsigned int last = GETARG_C(i);");
                println("        Table *h = hvalue(s2v(ra));");
                println("        if (n == 0)");
                println("          n = cast_int(L->top - ra) - 1;  /* get up to the top */");
                println("        else");
                println("          L->top = ci->top;  /* correct top in case of emergency GC */");
                println("        last += n;");
                if (has_extra_arg) {
                 println("        last += GETARG_Ax(0x%08x) * (MAXARG_C + 1);", f->code[pc+1]); // (!)
                }
                println("        if (last > luaH_realasize(h))  /* needs more space? */");
                println("          luaH_resizearray(L, h, last);  /* preallocate it at once */");
                println("        for (; n > 0; n--) {");
                println("          TValue *val = s2v(ra + n);");
                println("          setobj2t(L, &h->array[last - 1], val);");
                println("          last--;");
                println("          luaC_barrierback(L, obj2gco(h), val);");
                println("        }");
                if (has_extra_arg) {
                 println("        goto LUAOT_SKIP1;"); // (!)
                }
                break;
            }
            case OP_CLOSURE: {
                println("    Proto *p = cl->p->p[GETARG_Bx(i)];");
                println("    halfProtect(pushclosure(L, p, cl->upvals, base, ra));");
                println("    checkGC(L, ra + 1);");
                break;
            }
            case OP_VARARG: {
                println("    int n = GETARG_C(i) - 1;  /* required results */");
                println("    Protect(luaT_getvarargs(L, ci, ra, n));");
                break;
            }
            case OP_VARARGPREP: {
                println("    ProtectNT(luaT_adjustvarargs(L, GETARG_A(i), ci, cl->p));");
                println("    if (l_unlikely(trap)) {  /* previous \"Protect\" updated trap */");
                println("      luaD_hookcall(L, ci);");
                println("      L->oldpc = 1;  /* next opcode will be seen as a \"new\" line */");
                println("    }");
                println("    updatebase(ci);  /* function has new base after adjustment */");
                break;
            }
            case OP_EXTRAARG: {
                println("    lua_assert(0);");
                break;
            }
            default: {
                char msg[64];
                sprintf(msg, "%s is not implemented yet", opnames[op]);
                fatal_error(msg);
                break;
            }
        }
        println("  }");
        printnl();
    }

    println("}");
    printnl();
}

static
void create_functions(Proto *p)
{
    // luaot_footer.c should use the same traversal order as this.
    create_function(p);
    for (int i = 0; i < p->sizep; i++) {
        create_functions(p->p[i]);
    }
}

static
void print_functions(Proto *p)
{
    create_functions(p);

    println("static AotCompiledFunction LUAOT_FUNCTIONS[] = {");
    for (int i = 0; i < nfunctions; i++) {
        println("  magic_implementation_%02d,", i);
    }
    println("  NULL");
    println("};");
}

static
void print_source_code()
{
    // Since the code we are generating is lifted from lvm.c, we need it to use
    // Lua functions instead of C functions. And to create the Lua functions,
    // we have to `load` them from source code.
    //
    // The most readable approach would be to bundle this Lua source code as a
    // big C string literal. However, C compilers have limits on how big a
    // string literal can be, so instead of using a string literal, we use a
    // plain char array instead.

    FILE *infile = fopen(input_filename, "r");
    if (!infile) { fatal_error("could not open input file a second time"); }

    println("static const char LUAOT_MODULE_SOURCE_CODE[] = {");

    int c;
    int col = 0;
    do {
        if (col == 0) {
            print(" ");
        }

        c = fgetc(infile);
        if (c == EOF) {
            print(" %3d", 0);
        } else {
            print(" %3d", c);
            print(",");
        }

        col++;
        if (col == 16 || c == EOF) {
            print("\n");
            col = 0;
        }
    } while (c != EOF);
    println("};");

    fclose(infile);
}
