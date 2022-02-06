//
// The compiler part of the compiler
// ---------------------------------
// This is a modification of the "goto" implementation
// but trying to move the main logic into helper functions (instead of inlining everything)

static
int jump_target(Proto *f, int pc)
{
    Instruction instr = f->code[pc];
    if (GET_OPCODE(instr) != OP_JMP) { fatal_error("instruction is not a jump"); }
    return (pc+1) + GETARG_sJ(instr);
}

static
void println_goto_ret()
{
    // This is the piece of code that is after the "ret" label.
    // It should be used in the places that do "goto ret;"
    println("    if (ctx->ci->callstatus & CIST_FRESH)");
    println("        return NULL;  /* end this frame */");
    println("    else {");
    println("        ctx->ci = ctx->ci->previous;");
    println("        return ctx->ci;");
    println("    }");
}

///
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
    println("CallInfo *magic_implementation_%02d(lua_State *L, CallInfo *ci)", func_id);
    println("{");
    println("  LuaotExecuteState ctx_;");
    println("  LuaotExecuteState *ctx = &ctx_;");
    printnl();    
    println("  ctx->ci = ci;");
    println("  ctx->trap = L->hookmask;");
    println("  ctx->cl = clLvalue(s2v(ctx->ci->func));");
    println("  ctx->k = ctx->cl->p->k;");
    println("  const Instruction *pc = ctx->ci->u.l.savedpc;");
    println("  if (l_unlikely(ctx->trap)) {");
    println("    if (pc == ctx->cl->p->code) {  /* first instruction (not resuming)? */");
    println("      if (ctx->cl->p->is_vararg)");
    println("        ctx->trap = 0;  /* hooks will start after VARARGPREP instruction */");
    println("      else  /* check 'call' hook */");
    println("        luaD_hookcall(L, ctx->ci);");
    println("    }");
    println("    ctx->ci->u.l.trap = 1;  /* assume trap is on, for now */");
    println("  }");
    println("  ctx->base = ctx->ci->func + 1;");
    println("  /* main loop of interpreter */");
    println("  Instruction *code = ctx->cl->p->code;"); // (!!!)
    println("  Instruction i;");
    println("  StkId ra;");
    printnl();

    // If we are returning from another function, or resuming a coroutine,
    // jump back to where left.
    println("  switch (pc - code) {");
    for (int pc = 0; pc < f->sizecode; pc++) {
        println("    case %d: goto label_%02d;", pc, pc);
    }
    println("  }");
    printnl();

    for (int pc = 0; pc < f->sizecode; pc++) {
        Instruction instr = f->code[pc];
        OpCode op = GET_OPCODE(instr);

        luaot_PrintOpcodeComment(f, pc);

        if (op == OP_MMBIN || op == OP_MMBINI || op == OP_MMBINK) {
            println("  label_%02d: {}", pc);
            printnl();
            continue;
        }

        // While an instruction is executing, the program counter typically
        // points towards the next instruction. There are some corner cases
        // where the program counter getss adjusted mid-instruction, but I
        // am not breaking anything because of those...
        println("  pc = (code + %d);", pc+1);

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
        //println("    fprintf(stderr, \"OP=%%d\\n\", %d);", pc);
        println("    aot_vmfetch(0x%08x);", instr);

        switch (op) {
            case OP_MOVE: {
                println("    luaot_MOVE(L, ra, RB(i));");
                break;
            }
            case OP_LOADI: {
                println("    lua_Integer b = GETARG_sBx(i);");
                println("    luaot_LOADI(L, ra, b);");
                break;
            }
            case OP_LOADF: {
                println("    int b = GETARG_sBx(i);");
                println("    luaot_LOADF(ra, b);");
                break;
            }
            case OP_LOADK: {
                println("    TValue *rb = ctx->k + GETARG_Bx(i);");
                println("    luaot_LOADK(L, ra, rb);");
                break;
            }
            case OP_LOADKX: {
                println("    TValue *rb;");
                println("    rb = ctx->k + GETARG_Ax(0x%08x);", f->code[pc+1]);
                println("    luaot_LOADKX(L, ra, rb);");
                println("    goto LUAOT_SKIP1;"); //(!)
                break;
            }
            case OP_LOADFALSE: {
                println("    luaot_LOADFALSE(ra);");
                break;
            }
            case OP_LFALSESKIP: {
                println("    luaot_LFALSESKIP(ra);");
                println("    goto LUAOT_SKIP1;"); //(!)
                break;
            }
            case OP_LOADTRUE: {
                println("    luaot_LOADTRUE(ra)");
                break;
            }
            case OP_LOADNIL: {
                println("    int b = GETARG_B(i);");
                println("    luaot_LOADNIL(ra, b);");
                break;
            }
            case OP_GETUPVAL: {
                println("    int b = GETARG_B(i);");
                println("    luaot_GETUPVAL(L, ctx, ra, b);");
                break;
            }
            case OP_SETUPVAL: {
                println("    int b = GETARG_B(i);");
                println("    luaot_SETUPVAL(L, ctx, ra, b);");
                break;
            }
            case OP_GETTABUP: {
                println("    int b = GETARG_B(i);");
                println("    TValue *rc = KC(i);");
                println("    luaot_GETTABUP(L, ctx, pc, ra, b, rc);");
                break;
            }
            case OP_GETTABLE: {
                println("    TValue *rb = vRB(i);");
                println("    TValue *rc = vRC(i);");
                println("    luaot_GETTABLE(L, ctx, pc, ra, rb, rc);");
                break;
            }
            case OP_GETI: {
                println("    TValue *rb = vRB(i);");
                println("    int c = GETARG_C(i);");
                println("    luaot_GETI(L, ctx, pc, ra, rb, rc);");
                break;
            }
            case OP_GETFIELD: {
                println("    TValue *rb = vRB(i);");
                println("    TValue *rc = KC(i);");
                println("    luaot_GETFIELD(L, ctx, pc, ra, rb, rc);");
                break;
            }
            case OP_SETTABUP: {
                println("    int a = GETARG_A(i);");
                println("    TValue *rb = KB(i);");
                println("    TValue *rc = RKC(i);");
                println("    luaot_SETTABUP(L, ctx, pc, a, rb, rc);");
                break;
            }
            case OP_SETTABLE: {
                println("    TValue *rb = vRB(i);  /* key (table is in 'ra') */");
                println("    TValue *rc = RKC(i);  /* value */");
                println("    luaot_SETTABLE(L, ctx, pc, ra, rb, rc);");
                break;
            }
            case OP_SETI: {
                println("    int b = GETARG_B(i);");
                println("    TValue *rc = RKC(i);");
                println("    luaot_SETI(L, ctx, pc, ra, b, rc);");
                break;
            }
            case OP_SETFIELD: {
                println("    TValue *rb = KB(i);");
                println("    TValue *rc = RKC(i);");
                println("    luaot_SETFIELD(L, ctx, pc, ra, rb, rc);");
                break;
            }
            case OP_NEWTABLE: {
                println("    int b = GETARG_B(i);  /* log2(hash size) + 1 */");
                println("    int c = GETARG_C(i);  /* array size */");
                println("    int k = TESTARK_k(i);");
                println("    lua_assert((!k) == (GETARG_Ax(0x%08x) == 0));", f->code[pc+1]);
                println("    if (k) {");
                println("        luaot_NEWTABLE_1(L, ctx, pc, ra, b, c);");
                println("    } else {");
                println("        c += GETARG_Ax(0x%08x) * (MAXARG_C + 1);", f->code[pc+1]);
                println("        luaot_NEWTABLE_1(L, ctx, pc, ra, b, c);");
                println("    }");
                println("    goto LUAOT_SKIP1;"); // (!)
                break;
            }
            case OP_SELF: {
                println("    TValue *rb = vRB(i);");
                println("    TValue *rc = RKC(i);");
                println("    luaot_SELF(L, ctx, pc, ra, rb, rc);");
                break;
            }
            case OP_ADDI: {
                println("    luaot_arithI(L, luaot_ADDI);");
                break;
            }
            case OP_ADDK: {
                println("    luot_arithK(L, luaot_ADDK);");
                break;
            }
            case OP_SUBK: {
                println("    luaot_arithK(L, luaot_SUBK);");
                break;
            }
            case OP_MULK: {
                println("    luaot_arithK(L, luaot_MULK);");
                break;
            }
            case OP_MODK: {
                println("    luaot_arithK(L, luaot_MODK);");
                break;
            }
            case OP_POWK: {
                println("    luaot_arithfK(L, luaot_POWK);");
                break;
            }
            case OP_DIVK: {
                println("    luaot_arithfK(L, luaot_DIVK);");
                break;
            }
            case OP_IDIVK: {
                println("    luaot_arithK(L, luaot_IDIVK);");
                break;
            }
            case OP_BANDK: {
                println("    luaot_bitwiseK(L, luaot_BANDK);");
                break;
            }
            case OP_BORK: {
                println("    luaot_bitwiseK(L, luaot_BORK);");
                break;
            }
            case OP_BXORK: {
                println("    luaot_bitwiseK(L, luaot_BXORK);");
                break;
            }
            case OP_SHRI: {
                println("    TValue *rb = vRB(i);");
                println("    int ic = GETARG_sC(i);");
                println("    luaot_SHRI(L, ctx, pc, ra, rb, ic);");
                break;
            }
            case OP_SHLI: {
                println("    TValue *rb = vRB(i);");
                println("    int ic = GETARG_sC(i);");
                println("    luaot_SHLI(L, ctx, pc, ra, rb, ic);");
                break;
            }
            case OP_ADD: {
                println("    luaot_arith(L, luaot_ADD);");
                break;
            }
            case OP_SUB: {
                println("    luaot_arith(L, luaot_SUB);");
                break;
            }
            case OP_MUL: {
                println("    luaot_arith(L, luaot_MUL);");
                break;
            }
            case OP_MOD: {
                println("    luaot_arith(L, luaot_MOD);");
                break;
            }
            case OP_POW: {
                println("    luaot_arithf(L, luaot_POW);");
                break;
            }
            case OP_DIV: {  /* float division (always with floats: */
                println("    luaot_arithf(L, luaot_DIV);");
                break;
            }
            case OP_IDIV: {  /* floor division */
                println("    luaot_arith(L, luaot_IDIV);");
                break;
            }
            case OP_BAND: {
                println("    luaot_bitwise(L, luaot_BAND);");
                break;
            }
            case OP_BOR: {
                println("    luaot_bitwise(L, luaot_BOR);");
                break;
            }
            case OP_BXOR: {
                println("    luaot_bitwise(L, luaot_BXOR);");
                break;
            }
            case OP_SHR: {
                println("    luaot_bitwise(L, luaot_SHR);");
                break;
            }
            case OP_SHL: {
                println("    luaot_bitwise(L, luaot_SHL);");
                break;
            }
            case OP_MMBIN: {
                // Inlined in previous opcode
                break;
            }
            case OP_MMBINI: {
                // Inlined in previous opcode
                break;
            }
            case OP_MMBINK: {
                // Inlined in previous opcode
                break;
            }
            case OP_UNM: {
                println("    TValue *rb = vRB(i);");
                println("    luaot_UNM(L, ctx, pc, ra, rb);");
                break;
            }
            case OP_BNOT: {
                println("    TValue *rb = vRB(i);");
                println("    luaot_BNOT(L, ctx, pc, ra, rb);");
                break;
            }
            case OP_NOT: {
                println("    TValue *rb = vRB(i);");
                println("    luaot_NOT(L, ctx, pc, ra, rb);");
                break;
            }
            case OP_LEN: {
                println("    TValue *rb = vRB(i);");
                println("    luaot_LEN(L, ctx, pc, ra, rb);");
                break;
            }
            case OP_CONCAT: {
                println("    int n = GETARG_B(i);  /* number of elements to concatenate */");
                println("    luaot_CONCAT(L, ctx, pc, ra, n);");
                break;
            }
            case OP_CLOSE: {
                println("    luaot_CLOSE(L, ctx, pc, ra);");
                break;
            }
            case OP_TBC: {
                println("    luaot_TBC(L, ctx, pc, ra);");
                break;
            }
            case OP_JMP: {
                // INLINED
                println("    updatetrap(ctx->ci);");
                println("    goto label_%02d;", jump_target(f, pc));//(!)
                break;
            }
            case OP_EQ: {
                println("    TValue *rb = vRB(i);");
                println("    int cond = luaot_EQ(L, ctx, pc, ra, rb);");
                println("    docondjump();");
                break;
            }
            case OP_LT: {
                println("    TValue *rb = vRB(i);");
                println("    int cond = luaot_LT(L, ctx, pc, ra, rb);");
                println("    docondjump();");
                break;
            }
            case OP_LE: {
                println("    TValue *rb = vRB(i);");
                println("    int cond = luaot_LE(L, ctx, pc, ra, rb);");
                println("    docondjump();");
                break;
            }
            case OP_EQK: {
                // This can be simply inlined
                println("    TValue *rb = KB(i);");
                println("    /* basic types do not use '__eq'; we can use raw equality */");
                println("    int cond = luaV_equalobj(NULL, s2v(ra), rb);");
                println("    docondjump();");
                break;
            }
            case OP_EQI: {
                println("    int im = GETARG_sB(i);");
                println("    int isf = GETARG_C(i);");
                println("    int cond = luaot_EQI(L, ctx, pc, ra, im, isf);");
                println("    docondjump();");
                break;
            }
            case OP_LTI: {
                println("    int im = GETARG_sB(i);");
                println("    int isf = GETARG_C(i);");
                println("    int cond = luaot_LTI(L, ctx, pc, ra, im, isf);");
                println("    docondjump();");
                break;
            }
            case OP_LEI: {
                println("    int im = GETARG_sB(i);");
                println("    int isf = GETARG_C(i);");
                println("    int cond = luaot_LEI(L, ctx, pc, ra, im, isf);");
                println("    docondjump();");
                break;
            }
            case OP_GTI: {
                println("    int im = GETARG_sB(i);");
                println("    int isf = GETARG_C(i);");
                println("    int cond = luaot_GTI(L, ctx, pc, ra, im, isf);");
                println("    docondjump();");
                break;
            }
            case OP_GEI: {
                println("    int im = GETARG_sB(i);");
                println("    int isf = GETARG_C(i);");
                println("    int cond = luaot_GEI(L, ctx, pc, ra, im, isf);");
                println("    docondjump();");
                break;
            }
            case OP_TEST: {
                // INLINED
                println("    int cond = !l_isfalse(s2v(ra));");
                println("    docondjump();");
                break;
            }
            case OP_TESTSET: {
                // INLINED
                println("    TValue *rb = vRB(i);");
                println("    if (l_isfalse(rb) == GETARG_k(i))");
                println("      goto LUAOT_SKIP1;"); // (!)
                println("    else {");
                println("      setobj2s(L, ra, rb);");
                println("      donextjump(ctx->ci);");
                println("    }");
                break;
            }
            case OP_CALL: {
                println("    int b = GETARG_B(i);");
                println("    int nresults = GETARG_C(i) - 1;");
                println("    CallInfo *newci = luaot_CALL(L, ctx, pc, ra, b, nresults);");
                println("    if (newci) return newci;");
                break;
            }
            case OP_TAILCALL: {
                println("    int b = GETARG_B(i);  /* number of arguments + 1 (function) */");
                println("    int nparams1 = GETARG_C(i);");
                println("    /* delta is virtual 'func' - real 'func' (vararg functions) */");
                println("    int delta = (nparams1) ? ctx->ci->u.l.nextraargs + nparams1 : 0;");
                println("    if (b != 0)");
                println("      L->top = ra + b;");
                println("    else  /* previous instruction set top */");
                println("      b = cast_int(L->top - ra);");
                println("    savepc(ctx->ci);  /* several calls here can raise errors */");
                println("    if (TESTARG_k(i)) {");
                println("      luaF_closeupval(L, ctx->base);  /* close upvalues from current call */");
                println("      lua_assert(L->tbclist < ctx->base);  /* no pending tbc variables */");
                println("      lua_assert(ctx->base == ctx->ci->func + 1);");
                println("    }");
                println("    while (!ttisfunction(s2v(ra))) {  /* not a function? */");
                println("      luaD_tryfuncTM(L, ra);  /* try '__call' metamethod */");
                println("      b++;  /* there is now one extra argument */");
                println("      checkstackGCp(L, 1, ra);");
                println("    }");
                println("    if (!ttisLclosure(s2v(ra))) {  /* C function? */");
                println("      luaD_precall(L, ra, LUA_MULTRET);  /* call it */");
                println("      updatetrap(ctx->ci);");
                println("      updatestack(ctx->ci);  /* stack may have been relocated */");
                println("      ctx->ci->func -= delta;  /* restore 'func' (if vararg) */");
                println("      luaD_poscall(L, ctx->ci, cast_int(L->top - ra));  /* finish caller */");
                println("      updatetrap(ctx->ci);  /* 'luaD_poscall' can change hooks */");
                println_goto_ret(); // (!)
                println("    }");
                println("    ctx->ci->func -= delta;  /* restore 'func' (if vararg) */");
                println("    luaD_pretailcall(L, ctx->ci, ra, b);  /* prepare call frame */");
                println("    return ctx->ci;");
                break;
            }
            case OP_RETURN: {
                println("    int n = GETARG_B(i) - 1;  /* number of results */");
                println("    int nparams1 = GETARG_C(i);");
                println("    if (n < 0)  /* not fixed? */");
                println("      n = cast_int(L->top - ra);  /* get what is available */");
                println("    savepc(ctx->ci);");
                println("    if (TESTARG_k(i)) {  /* may there be open upvalues? */");
                println("      if (L->top < ctx->ci->top)");
                println("        L->top = ctx->ci->top;");
                println("      luaF_close(L, ctx->base, CLOSEKTOP, 1);");
                println("      updatetrap(ctx->ci);");
                println("      updatestack(ctx->ci);");
                println("    }");
                println("    if (nparams1)  /* vararg function? */");
                println("      ctx->ci->func -= ctx->ci->u.l.nextraargs + nparams1;");
                println("    L->top = ra + n;  /* set call for 'luaD_poscall' */");
                println("    luaD_poscall(L, ctx->ci, n);");
                println("    updatetrap(ctx->ci);  /* 'luaD_poscall' can change hooks */");
                println_goto_ret();
                break;
            }
            case OP_RETURN0: {
                println("    if (l_unlikely(L->hookmask)) {");
                println("      L->top = ra;");
                println("      savepc(ctx->ci);");
                println("      luaD_poscall(L, ctx->ci, 0);  /* no hurry... */");
                println("      ctx->trap = 1;");
                println("    }");
                println("    else {  /* do the 'poscall' here */");
                println("      int nres;");
                println("      L->ci = ci->previous;  /* back to caller */");
                println("      L->top = ctx->base - 1;");
                println("      for (nres = ctx->ci->nresults; l_unlikely(nres > 0); nres--)");
                println("        setnilvalue(s2v(L->top++));  /* all results are nil */");
                println("    }");
                println_goto_ret();
                break;
            }
            case OP_RETURN1: {
                println("    if (l_unlikely(L->hookmask)) {");
                println("      L->top = ra + 1;");
                println("      savepc(ctx->ci);");
                println("      luaD_poscall(L, ctx->ci, 1);  /* no hurry... */");
                println("      ctx->trap = 1;");
                println("    }");
                println("    else {  /* do the 'poscall' here */");
                println("      int nres = ctx->ci->nresults;");
                println("      L->ci = ctx->ci->previous;  /* back to caller */");
                println("      if (nres == 0)");
                println("        L->top = ctx->base - 1;  /* asked for no results */");
                println("      else {");
                println("        setobjs2s(L, ctx->base - 1, ra);  /* at least this result */");
                println("        L->top = ctx->base;");
                println("        for (; l_unlikely(nres > 1); nres--)");
                println("          setnilvalue(s2v(L->top++));");
                println("      }");
                println("    }");
                println_goto_ret();
                break;
            }
            case OP_FORLOOP: {
                println("    if (luaot_FORLOOP(L, ctx, pc, ra)) {");
                println("      goto label_%02d; /* jump back */", ((pc+1) - GETARG_Bx(instr))); //(!)
                println("    }");
                break;
            }
            case OP_FORPREP: {
                println("    if (luaot_FORPREP(L, ctx, pc, ra)) {");
                println("      goto label_%02d; /* skip the loop */", ((pc+1) + GETARG_Bx(instr) + 1)); //(!)
                println("    }");
                break;
            }
            case OP_TFORPREP: {
                println("    luaot_TFORPREP(L, ctx, pc, ra);");
                println("    goto label_%02d;", ((pc+1) + GETARG_Bx(instr))); //(!)
                break;
            }
            case OP_TFORCALL: {
                println("    int c = GETARG_C(i);");
                println("    luaot_TFORCALL(L, ctx, pc, ra, c);");
                // (!) Going to the next instruction is a no-op
                break;
            }
            case OP_TFORLOOP: {
                println("    if (luaot_TFORLOOP(L, ctx, pc, ra) {");
                println("      goto label_%02d; /* jump back */", ((pc+1) - GETARG_Bx(instr))); //(!)
                println("    }");
                break;
            }
            case OP_SETLIST: {
                // We should take care to only generate code for the extra argument if it actually exists.
                // In a previous version of this compiler, we were putting the "if" in the generated code
                // instead of in the code generator and that resulted in compile-time warnings from gcc.
                // Sometimes, the last += GETARG_Ax" line would overflow and the C compiler would naturally
                // complain (even though the offending line was never executed).
                int has_extra_arg = TESTARG_k(instr);
                println("        int n = GETARG_B(i);");
                println("        unsigned int last = GETARG_C(i);");
                println("        Table *h = hvalue(s2v(ra));");
                println("        if (n == 0)");
                println("          n = cast_int(L->top - ra) - 1;  /* get up to the top */");
                println("        else");
                println("          L->top = ctx->ci->top;  /* correct top in case of emergency GC */");
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
                println("    int b = GETARG_Bx(i);");
                println("    luaot_CLOSURE(L, ctx, pc, ra, b);");
                break;
            }
            case OP_VARARG: {
                println("    int n = GETARG_C(i) - 1;  /* required results */");
                println("    luaot_VARARG(L, ctx, pc, ra, n);");
                break;
            }
            case OP_VARARGPREP: {
                println("    int a = GETARG_A(i);");
                println("    luaot_VARARGPREP(L, ctx, pc, a);");
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
