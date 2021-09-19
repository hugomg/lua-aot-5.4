//
// The compiler part of the compiler
// ---------------------------------
// Based on lvm.c with the following changes:
//   - Constants are put into macros (LUAOT_PC, LUAOT_NEXT_JUMP, etc)
//   - Jumps go into a trampoline
//

static
void println_goto_ret()
{
    // This is the piece of code that is after the "ret" label.
    // It should be used in the places that do "goto ret;"
    println("    if (ci->callstatus & CIST_FRESH)");
    println("        return NULL;  /* end this frame */");
    println("    else {");
    println("        ci = ci->previous;");
    println("        return ci;");
    println("    }");
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
    println("CallInfo *magic_implementation_%02d(lua_State *L, CallInfo *ci)", func_id);
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

    println("  while (1) {");
    println("    switch (pc - code) {");
    for (int pc = 0; pc < f->sizecode; pc++) {
        Instruction instr = f->code[pc];
        OpCode op = GET_OPCODE(instr);

        luaot_PrintOpcodeComment(f, pc);

        println("      case %d: {", pc);
        println("        aot_vmfetch(0x%08x);", instr);

        switch (op) {
            case OP_MOVE: {
                println("        setobjs2s(L, ra, RB(i));");
                // FALLTHROUGH
                break;
            }
            case OP_LOADI: {
                println("        lua_Integer b = GETARG_sBx(i);");
                println("        setivalue(s2v(ra), b);");
                // FALLTHROUGH
                break;
            }
            case OP_LOADF: {
                println("        int b = GETARG_sBx(i);");
                println("        setfltvalue(s2v(ra), cast_num(b));");
                // FALLTHROUGH
                break;
            }
            case OP_LOADK: {
                println("        TValue *rb = k + GETARG_Bx(i);");
                println("        setobj2s(L, ra, rb);");
                // FALLTHROUGH
                break;
            }
            case OP_LOADKX: {
                println("        TValue *rb;");
                println("        rb = k + GETARG_Ax(0x%08x); pc++;", f->code[pc+1]);
                println("        setobj2s(L, ra, rb);");
                println("        break;");
                // PC
                break;
            }
            case OP_LOADFALSE: {
                println("        setbfvalue(s2v(ra));");
                // FALLTHROUGH
                break;
            }
            case OP_LFALSESKIP: {
                println("        setbfvalue(s2v(ra));");
                println("        pc++; /* skip next instruction */");
                println("        break;");
                // PC
                break;
            }
            case OP_LOADTRUE: {
                println("        setbtvalue(s2v(ra));");
                // FALLTHROUGH
                break;
            }
            case OP_LOADNIL: {
                println("        int b = GETARG_B(i);");
                println("        do {");
                println("          setnilvalue(s2v(ra++));");
                println("        } while (b--);");
                // FALLTHROUGH
                break;
            }
            case OP_GETUPVAL: {
                println("        int b = GETARG_B(i);");
                println("        setobj2s(L, ra, cl->upvals[b]->v);");
                // FALLTHROUGH
                break;
            }
            case OP_SETUPVAL: {
                println("        UpVal *uv = cl->upvals[GETARG_B(i)];");
                println("        setobj(L, uv->v, s2v(ra));");
                println("        luaC_barrier(L, uv, s2v(ra));");
                // FALLTHROUGH
                break;
            }
            case OP_GETTABUP: {
                println("        const TValue *slot;");
                println("        TValue *upval = cl->upvals[GETARG_B(i)]->v;");
                println("        TValue *rc = KC(i);");
                println("        TString *key = tsvalue(rc);  /* key must be a string */");
                println("        if (luaV_fastget(L, upval, key, slot, luaH_getshortstr)) {");
                println("          setobj2s(L, ra, slot);");
                println("        }");
                println("        else");
                println("          Protect(luaV_finishget(L, upval, rc, ra, slot));");
                // FALLTHROUGH
                break;
            }
            case OP_GETTABLE: {
                println("        const TValue *slot;");
                println("        TValue *rb = vRB(i);");
                println("        TValue *rc = vRC(i);");
                println("        lua_Unsigned n;");
                println("        if (ttisinteger(rc)  /* fast track for integers? */");
                println("            ? (cast_void(n = ivalue(rc)), luaV_fastgeti(L, rb, n, slot))");
                println("            : luaV_fastget(L, rb, rc, slot, luaH_get)) {");
                println("          setobj2s(L, ra, slot);");
                println("        }");
                println("        else");
                println("          Protect(luaV_finishget(L, rb, rc, ra, slot));");
                // FALLTHROUGH
                break;
            }
            case OP_GETI: {
                println("        const TValue *slot;");
                println("        TValue *rb = vRB(i);");
                println("        int c = GETARG_C(i);");
                println("        if (luaV_fastgeti(L, rb, c, slot)) {");
                println("          setobj2s(L, ra, slot);");
                println("        }");
                println("        else {");
                println("          TValue key;");
                println("          setivalue(&key, c);");
                println("          Protect(luaV_finishget(L, rb, &key, ra, slot));");
                println("        }");
                // FALLTHROUGH
                break;
            }
            case OP_GETFIELD: {
                println("        const TValue *slot;");
                println("        TValue *rb = vRB(i);");
                println("        TValue *rc = KC(i);");
                println("        TString *key = tsvalue(rc);  /* key must be a string */");
                println("        if (luaV_fastget(L, rb, key, slot, luaH_getshortstr)) {");
                println("          setobj2s(L, ra, slot);");
                println("        }");
                println("        else");
                println("          Protect(luaV_finishget(L, rb, rc, ra, slot));");
                // FALLTHROUGH
                break;
            }
            case OP_SETTABUP: {
                println("        const TValue *slot;");
                println("        TValue *upval = cl->upvals[GETARG_A(i)]->v;");
                println("        TValue *rb = KB(i);");
                println("        TValue *rc = RKC(i);");
                println("        TString *key = tsvalue(rb);  /* key must be a string */");
                println("        if (luaV_fastget(L, upval, key, slot, luaH_getshortstr)) {");
                println("          luaV_finishfastset(L, upval, slot, rc);");
                println("        }");
                println("        else");
                println("          Protect(luaV_finishset(L, upval, rb, rc, slot));");
                // FALLTHROUGH
                break;
            }
            case OP_SETTABLE: {
                println("        const TValue *slot;");
                println("        TValue *rb = vRB(i);  /* key (table is in 'ra') */");
                println("        TValue *rc = RKC(i);  /* value */");
                println("        lua_Unsigned n;");
                println("        if (ttisinteger(rb)  /* fast track for integers? */");
                println("            ? (cast_void(n = ivalue(rb)), luaV_fastgeti(L, s2v(ra), n, slot))");
                println("            : luaV_fastget(L, s2v(ra), rb, slot, luaH_get)) {");
                println("          luaV_finishfastset(L, s2v(ra), slot, rc);");
                println("        }");
                println("        else");
                println("          Protect(luaV_finishset(L, s2v(ra), rb, rc, slot));");
                // FALLTHROUGH
                break;
            }
            case OP_SETI: {
                println("        const TValue *slot;");
                println("        int c = GETARG_B(i);");
                println("        TValue *rc = RKC(i);");
                println("        if (luaV_fastgeti(L, s2v(ra), c, slot)) {");
                println("          luaV_finishfastset(L, s2v(ra), slot, rc);");
                println("        }");
                println("        else {");
                println("          TValue key;");
                println("          setivalue(&key, c);");
                println("          Protect(luaV_finishset(L, s2v(ra), &key, rc, slot));");
                println("        }");
                // FALLTHROUGH
                break;
            }
            case OP_SETFIELD: {
                println("        const TValue *slot;");
                println("        TValue *rb = KB(i);");
                println("        TValue *rc = RKC(i);");
                println("        TString *key = tsvalue(rb);  /* key must be a string */");
                println("        if (luaV_fastget(L, s2v(ra), key, slot, luaH_getshortstr)) {");
                println("          luaV_finishfastset(L, s2v(ra), slot, rc);");
                println("        }");
                println("        else");
                println("          Protect(luaV_finishset(L, s2v(ra), rb, rc, slot));");
                // FALLTHROUGH
                break;
            }
            case OP_NEWTABLE: {
                println("        int b = GETARG_B(i);  /* log2(hash size) + 1 */");
                println("        int c = GETARG_C(i);  /* array size */");
                println("        Table *t;");
                println("        if (b > 0)");
                println("          b = 1 << (b - 1);  /* size is 2^(b - 1) */");
                println("        lua_assert((!TESTARG_k(i)) == (GETARG_Ax(0x%08x) == 0));", f->code[pc+1]);
                println("        if (TESTARG_k(i))");
                println("          c += GETARG_Ax(0x%08x) * (MAXARG_C + 1);", f->code[pc+1]);
                println("        pc++; /* skip extra argument */");
                println("        L->top = ra + 1;  /* correct top in case of emergency GC */");
                println("        t = luaH_new(L);  /* memory allocation */");
                println("        sethvalue2s(L, ra, t);");
                println("        if (b != 0 || c != 0)");
                println("          luaH_resize(L, t, c, b);  /* idem */");
                println("        checkGC(L, ra + 1);");
                println("        break;");
                // PC
                break;
            }
            case OP_SELF: {
                println("        const TValue *slot;");
                println("        TValue *rb = vRB(i);");
                println("        TValue *rc = RKC(i);");
                println("        TString *key = tsvalue(rc);  /* key must be a string */");
                println("        setobj2s(L, ra + 1, rb);");
                println("        if (luaV_fastget(L, rb, key, slot, luaH_getstr)) {");
                println("          setobj2s(L, ra, slot);");
                println("        }");
                println("        else");
                println("          Protect(luaV_finishget(L, rb, rc, ra, slot));");
                // FALLTHROUGH
                break;
            }
            case OP_ADDI: {
                println("        op_arithI(L, l_addi, luai_numadd);");
                println("        break;");
                // PC
                break;
            }
            case OP_ADDK: {
                println("        op_arithK(L, l_addi, luai_numadd);");
                println("        break;");
                // PC
                break;
            }
            case OP_SUBK: {
                println("        op_arithK(L, l_subi, luai_numsub);");
                println("        break;");
                // PC
                break;
            }
            case OP_MULK: {
                println("        op_arithK(L, l_muli, luai_nummul);");
                println("        break;");
                // PC
                break;
            }
            case OP_MODK: {
                println("        op_arithK(L, luaV_mod, luaV_modf);");
                println("        break;");
                // PC
                break;
            }
            case OP_POWK: {
                println("        op_arithfK(L, luai_numpow);");
                println("        break;");
                // PC
                break;
            }
            case OP_DIVK: {
                println("        op_arithfK(L, luai_numdiv);");
                println("        break;");
                // PC
                break;
            }
            case OP_IDIVK: {
                println("        op_arithK(L, luaV_idiv, luai_numidiv);");
                println("        break;");
                // PC
                break;
            }
            case OP_BANDK: {
                println("        op_bitwiseK(L, l_band);");
                println("        break;");
                // PC
                break;
            }
            case OP_BORK: {
                println("        op_bitwiseK(L, l_bor);");
                println("        break;");
                // PC
                break;
            }
            case OP_BXORK: {
                println("        op_bitwiseK(L, l_bxor);");
                println("        break;");
                // PC
                break;
            }
            case OP_SHRI: {
                println("        TValue *rb = vRB(i);");
                println("        int ic = GETARG_sC(i);");
                println("        lua_Integer ib;");
                println("        if (tointegerns(rb, &ib)) {");
                println("           pc++; setivalue(s2v(ra), luaV_shiftl(ib, -ic));");
                println("        }");
                println("        break;");
                // PC
                break;
            }
            case OP_SHLI: {
                println("        TValue *rb = vRB(i);");
                println("        int ic = GETARG_sC(i);");
                println("        lua_Integer ib;");
                println("        if (tointegerns(rb, &ib)) {");
                println("           pc++; setivalue(s2v(ra), luaV_shiftl(ic, ib));");
                println("        }");
                println("        break;");
                // PC
                break;
            }
            case OP_ADD: {
                println("        op_arith(L, l_addi, luai_numadd);");
                println("        break;");
                // PC
                break;
            }
            case OP_SUB: {
                println("        op_arith(L, l_subi, luai_numsub);");
                println("        break;");
                // PC
                break;
            }
            case OP_MUL: {
                println("        op_arith(L, l_muli, luai_nummul);");
                println("        break;");
                // PC
                break;
            }
            case OP_MOD: {
                println("        op_arith(L, luaV_mod, luaV_modf);");
                println("        break;");
                // PC
                break;
            }
            case OP_POW: {
                println("        op_arithf(L, luai_numpow);");
                println("        break;");
                // PC
                break;
            }
            case OP_DIV: {  /* float division (always with floats: */
                println("        op_arithf(L, luai_numdiv);");
                println("        break;");
                // PC
                break;
            }
            case OP_IDIV: {  /* floor division */
                println("        op_arith(L, luaV_idiv, luai_numidiv);");
                println("        break;");
                // PC
                break;
            }
            case OP_BAND: {
                println("        op_bitwise(L, l_band);");
                println("        break;");
                // PC
                break;
            }
            case OP_BOR: {
                println("        op_bitwise(L, l_bor);");
                println("        break;");
                // PC
                break;
            }
            case OP_BXOR: {
                println("        op_bitwise(L, l_bxor);");
                println("        break;");
                // PC
                break;
            }
            case OP_SHR: {
                println("        op_bitwise(L, luaV_shiftr);");
                println("        break;");
                // PC
                break;
            }
            case OP_SHL: {
                println("        op_bitwise(L, luaV_shiftl);");
                println("        break;");
                // PC
                break;
            }
            case OP_MMBIN: {
                println("        Instruction pi = 0x%08x; /* original arith. expression */", f->code[pc-1]);
                println("        TValue *rb = vRB(i);");
                println("        TMS tm = (TMS)GETARG_C(i);");
                println("        StkId result = RA(pi);");
                println("        lua_assert(OP_ADD <= GET_OPCODE(pi) && GET_OPCODE(pi) <= OP_SHR);");
                println("        Protect(luaT_trybinTM(L, s2v(ra), rb, result, tm));");
                // FALLTHROUGH
                break;
            }
            case OP_MMBINI: {
                println("        Instruction pi = 0x%0x;  /* original arith. expression */", f->code[pc-1]);
                println("        int imm = GETARG_sB(i);");
                println("        TMS tm = (TMS)GETARG_C(i);");
                println("        int flip = GETARG_k(i);");
                println("        StkId result = RA(pi);");
                println("        Protect(luaT_trybiniTM(L, s2v(ra), imm, flip, result, tm));");
                // FALLTHROUGH
                break;
            }
            case OP_MMBINK: {
                println("        Instruction pi = 0x%08x;  /* original arith. expression */", f->code[pc-1]);
                println("        TValue *imm = KB(i);");
                println("        TMS tm = (TMS)GETARG_C(i);");
                println("        int flip = GETARG_k(i);");
                println("        StkId result = RA(pi);");
                println("        Protect(luaT_trybinassocTM(L, s2v(ra), imm, flip, result, tm));");
                // FALLTHROUGH
                break;
            }
            case OP_UNM: {
                println("        TValue *rb = vRB(i);");
                println("        lua_Number nb;");
                println("        if (ttisinteger(rb)) {");
                println("          lua_Integer ib = ivalue(rb);");
                println("          setivalue(s2v(ra), intop(-, 0, ib));");
                println("        }");
                println("        else if (tonumberns(rb, nb)) {");
                println("          setfltvalue(s2v(ra), luai_numunm(L, nb));");
                println("        }");
                println("        else");
                println("          Protect(luaT_trybinTM(L, rb, rb, ra, TM_UNM));");
                // FALLTHROUGH
                break;
            }
            case OP_BNOT: {
                println("        TValue *rb = vRB(i);");
                println("        lua_Integer ib;");
                println("        if (tointegerns(rb, &ib)) {");
                println("          setivalue(s2v(ra), intop(^, ~l_castS2U(0), ib));");
                println("        }");
                println("        else");
                println("          Protect(luaT_trybinTM(L, rb, rb, ra, TM_BNOT));");
                // FALLTHROUGH
                break;
            }
            case OP_NOT: {
                println("        TValue *rb = vRB(i);");
                println("        if (l_isfalse(rb))");
                println("          setbtvalue(s2v(ra));");
                println("        else");
                println("          setbfvalue(s2v(ra));");
                // FALLTHROUGH
                break;
            }
            case OP_LEN: {
                println("        Protect(luaV_objlen(L, ra, vRB(i)));");
                // FALLTHROUGH
                break;
            }
            case OP_CONCAT: {
                println("        int n = GETARG_B(i);  /* number of elements to concatenate */");
                println("        L->top = ra + n;  /* mark the end of concat operands */");
                println("        ProtectNT(luaV_concat(L, n));");
                println("        checkGC(L, L->top); /* 'luaV_concat' ensures correct top */");
                // FALLTHROUGH
                break;
            }
            case OP_CLOSE: {
                println("Protect(luaF_close(L, ra, LUA_OK, 1));");
                // FALLTHROUGH
                break;
            }
            case OP_TBC: {
                println("        /* create new to-be-closed upvalue */");
                println("        halfProtect(luaF_newtbcupval(L, ra));");
                // FALLTHROUGH
                break;
            }
            case OP_JMP: {
                println("        dojump(ci, i, 0);");
                println("        break;");
                // PC
                break;
            }
            case OP_EQ: {
                println("        int cond;");
                println("        TValue *rb = vRB(i);");
                println("        Protect(cond = luaV_equalobj(L, s2v(ra), rb));");
                println("        docondjump();");
                println("        break;");
                // PC
                break;
            }
            case OP_LT: {
                println("        op_order(L, l_lti, LTnum, lessthanothers);");
                println("        break;");
                // PC
                break;
            }
            case OP_LE: {
                println("        op_order(L, l_lei, LEnum, lessequalothers);");
                println("        break;");
                // PC
                break;
            }
            case OP_EQK: {
                println("        TValue *rb = KB(i);");
                println("        /* basic types do not use '__eq'; we can use raw equality */");
                println("        int cond = luaV_equalobj(NULL, s2v(ra), rb);");
                println("        docondjump();");
                println("        break;");
                // PC
                break;
            }
            case OP_EQI: {
                println("        int cond;");
                println("        int im = GETARG_sB(i);");
                println("        if (ttisinteger(s2v(ra)))");
                println("          cond = (ivalue(s2v(ra)) == im);");
                println("        else if (ttisfloat(s2v(ra)))");
                println("          cond = luai_numeq(fltvalue(s2v(ra)), cast_num(im));");
                println("        else");
                println("          cond = 0;  /* other types cannot be equal to a number */");
                println("        docondjump();");
                println("        break;");
                // PC
                break;
            }
            case OP_LTI: {
                println("        op_orderI(L, l_lti, luai_numlt, 0, TM_LT);");
                println("        break;");
                // PC
                break;
            }
            case OP_LEI: {
                println("        op_orderI(L, l_lei, luai_numle, 0, TM_LE);");
                println("        break;");
                // PC
                break;
            }
            case OP_GTI: {
                println("        op_orderI(L, l_gti, luai_numgt, 1, TM_LT);");
                println("        break;");
                // PC
                break;
            }
            case OP_GEI: {
                println("        op_orderI(L, l_gei, luai_numge, 1, TM_LE);");
                println("        break;");
                // PC
                break;
            }
            case OP_TEST: {
                println("        int cond = !l_isfalse(s2v(ra));");
                println("        docondjump();");
                println("        break;");
                // PC
                break;
            }
            case OP_TESTSET: {
                println("        TValue *rb = vRB(i);");
                println("        if (l_isfalse(rb) == GETARG_k(i))");
                println("          pc++;");
                println("        else {");
                println("          setobj2s(L, ra, rb);");
                println("          donextjump(ci);");
                println("        }");
                println("        break;");
                // PC
                break;
            }
            case OP_CALL: {
                println("        CallInfo *newci;");
                println("        int b = GETARG_B(i);");
                println("        int nresults = GETARG_C(i) - 1;");
                println("        if (b != 0)  /* fixed number of arguments? */");
                println("            L->top = ra + b;  /* top signals number of arguments */");
                println("        /* else previous instruction set top */");
                println("        savepc(L);  /* in case of errors */");
                println("        if ((newci = luaD_precall(L, ra, nresults)) == NULL)");
                println("            updatetrap(ci);  /* C call; nothing else to be done */");
                println("        else {");
                println("            ci = newci;");
                println("            ci->callstatus = 0;  /* call re-uses 'luaV_execute' */");
                println("            return ci;");
                println("        }");
                // FALLTHROUGH
                break;
            }
            case OP_TAILCALL: {
                println("        int b = GETARG_B(i);  /* number of arguments + 1 (function) */");
                println("        int nparams1 = GETARG_C(i);");
                println("        /* delta is virtual 'func' - real 'func' (vararg functions) */");
                println("        int delta = (nparams1) ? ci->u.l.nextraargs + nparams1 : 0;");
                println("        if (b != 0)");
                println("          L->top = ra + b;");
                println("        else  /* previous instruction set top */");
                println("          b = cast_int(L->top - ra);");
                println("        savepc(ci);  /* several calls here can raise errors */");
                println("        if (TESTARG_k(i)) {");
                println("          luaF_closeupval(L, base);  /* close upvalues from current call */");
                println("          lua_assert(L->tbclist < base);  /* no pending tbc variables */");
                println("          lua_assert(base == ci->func + 1);");
                println("        }");
                println("        while (!ttisfunction(s2v(ra))) {  /* not a function? */");
                println("          luaD_tryfuncTM(L, ra);  /* try '__call' metamethod */");
                println("          b++;  /* there is now one extra argument */");
                println("          checkstackGCp(L, 1, ra);");
                println("        }");
                println("        if (!ttisLclosure(s2v(ra))) {  /* C function? */");
                println("          luaD_precall(L, ra, LUA_MULTRET);  /* call it */");
                println("          updatetrap(ci);");
                println("          updatestack(ci);  /* stack may have been relocated */");
                println("          ci->func -= delta;  /* restore 'func' (if vararg) */");
                println("          luaD_poscall(L, ci, cast_int(L->top - ra));  /* finish caller */");
                println("          updatetrap(ci);  /* 'luaD_poscall' can change hooks */");
                println_goto_ret(); // (!)
                println("        }");
                println("        ci->func -= delta;  /* restore 'func' (if vararg) */");
                println("        luaD_pretailcall(L, ci, ra, b);  /* prepare call frame */");
                println("        return ci;");
                // FALLTHROUGH
                break;
            }
            case OP_RETURN: {
                println("        int n = GETARG_B(i) - 1;  /* number of results */");
                println("        int nparams1 = GETARG_C(i);");
                println("        if (n < 0)  /* not fixed? */");
                println("          n = cast_int(L->top - ra);  /* get what is available */");
                println("        savepc(ci);");
                println("        if (TESTARG_k(i)) {  /* may there be open upvalues? */");
                println("          if (L->top < ci->top)");
                println("            L->top = ci->top;");
                println("          luaF_close(L, base, CLOSEKTOP, 1);");
                println("          updatetrap(ci);");
                println("          updatestack(ci);");
                println("        }");
                println("        if (nparams1)  /* vararg function? */");
                println("          ci->func -= ci->u.l.nextraargs + nparams1;");
                println("        L->top = ra + n;  /* set call for 'luaD_poscall' */");
                println("        luaD_poscall(L, ci, n);");
                println("        updatetrap(ci);  /* 'luaD_poscall' can change hooks */");
                println_goto_ret(); // (!)
                // FALLTHROUGH
                break;
            }
            case OP_RETURN0: {
                println("        if (l_unlikely(L->hookmask)) {");
                println("          L->top = ra;");
                println("          savepc(ci);");
                println("          luaD_poscall(L, ci, 0);  /* no hurry... */");
                println("          trap = 1;");
                println("        }");
                println("        else {  /* do the 'poscall' here */");
                println("          int nres;");
                println("          L->ci = ci->previous;  /* back to caller */");
                println("          L->top = base - 1;");
                println("          for (nres = ci->nresults; l_unlikely(nres > 0); nres--)");
                println("            setnilvalue(s2v(L->top++));  /* all results are nil */");
                println("        }");
                println_goto_ret(); // (!)
                // FALLTHROUGH
                break;
            }
            case OP_RETURN1: {
                println("        if (l_unlikely(L->hookmask)) {");
                println("          L->top = ra + 1;");
                println("          savepc(ci);");
                println("          luaD_poscall(L, ci, 1);  /* no hurry... */");
                println("          trap = 1;");
                println("        }");
                println("        else {  /* do the 'poscall' here */");
                println("          int nres = ci->nresults;");
                println("          L->ci = ci->previous;  /* back to caller */");
                println("          if (nres == 0)");
                println("            L->top = base - 1;  /* asked for no results */");
                println("          else {");
                println("            setobjs2s(L, base - 1, ra);  /* at least this result */");
                println("            L->top = base;");
                println("            for (; l_unlikely(nres > 1); nres--)");
                println("              setnilvalue(s2v(L->top++));");
                println("          }");
                println("        }");
                println_goto_ret(); // (!)
                // FALLTHROUGH
                break;
            }
            case OP_FORLOOP: {
                println("        if (ttisinteger(s2v(ra + 2))) {  /* integer loop? */");
                println("          lua_Unsigned count = l_castS2U(ivalue(s2v(ra + 1)));");
                println("          if (count > 0) {  /* still more iterations? */");
                println("            lua_Integer step = ivalue(s2v(ra + 2));");
                println("            lua_Integer idx = ivalue(s2v(ra));  /* internal index */");
                println("            chgivalue(s2v(ra + 1), count - 1);  /* update counter */");
                println("            idx = intop(+, idx, step);  /* add step to index */");
                println("            chgivalue(s2v(ra), idx);  /* update internal index */");
                println("            setivalue(s2v(ra + 3), idx);  /* and control variable */");
                println("            pc -= %d; /* jump back */", GETARG_Bx(instr));
                println("          }");
                println("        }");
                println("        else if (floatforloop(ra)) /* float loop */");
                println("          pc -= %d; /* jump back */", GETARG_Bx(instr));
                println("        updatetrap(ci);  /* allows a signal to break the loop */");
                println("        break;");
                // PC
                break;
            }
            case OP_FORPREP: {
                println("        savestate(L, ci);  /* in case of errors */");
                println("        if (forprep(L, ra))");
                println("          pc += %d; /* skip the loop */", GETARG_Bx(instr) + 1);
                println("        break;");
                // PC
                break;
            }
            case OP_TFORPREP: {
                println("        /* create to-be-closed upvalue (if needed) */");
                println("        halfProtect(luaF_newtbcupval(L, ra + 3));");
                println("        pc += %d;", GETARG_Bx(instr));
                println("        break;");
                // PC
                break;
            }
            case OP_TFORCALL: {
                println("        /* 'ra' has the iterator function, 'ra + 1' has the state,");
                println("           'ra + 2' has the control variable, and 'ra + 3' has the");
                println("           to-be-closed variable. The call will use the stack after");
                println("           these values (starting at 'ra + 4')");
                println("        */");
                println("        /* push function, state, and control variable */");
                println("        memcpy(ra + 4, ra, 3 * sizeof(*ra));");
                println("        L->top = ra + 4 + 3;");
                println("        ProtectNT(luaD_call(L, ra + 4, GETARG_C(i)));  /* do the call */");
                println("        updatestack(ci);  /* stack may have changed */");
                // (!) Going to the next instruction is a no-op
                // FALLTHROUGH
                break;
            }
            case OP_TFORLOOP: {
                println("        if (!ttisnil(s2v(ra + 4))) {  /* continue loop? */");
                println("          setobjs2s(L, ra + 2, ra + 4);  /* save control variable */");
                println("          pc -= %d; /* jump back */", GETARG_Bx(instr));
                println("        }");
                println("        break;");
                // PC
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
                println("          L->top = ci->top;  /* correct top in case of emergency GC */");
                println("        last += n;");
                if (has_extra_arg) {
                 println("        last += GETARG_Ax(0x%08x) * (MAXARG_C + 1);", f->code[pc+1]);
                 println("        pc++;");
                }
                println("        if (last > luaH_realasize(h))  /* needs more space? */");
                println("          luaH_resizearray(L, h, last);  /* preallocate it at once */");
                println("        for (; n > 0; n--) {");
                println("          TValue *val = s2v(ra + n);");
                println("          setobj2t(L, &h->array[last - 1], val);");
                println("          last--;");
                println("          luaC_barrierback(L, obj2gco(h), val);");
                println("        }");
                println("        break;");
                // PC
                break;
            }
            case OP_CLOSURE: {
                println("        Proto *p = cl->p->p[GETARG_Bx(i)];");
                println("        halfProtect(pushclosure(L, p, cl->upvals, base, ra));");
                println("        checkGC(L, ra + 1);");
                // FALLTHROUGH
                break;
            }
            case OP_VARARG: {
                println("        int n = GETARG_C(i) - 1;  /* required results */");
                println("        Protect(luaT_getvarargs(L, ci, ra, n));");
                // FALLTHROUGH
                break;
            }
            case OP_VARARGPREP: {
                println("        ProtectNT(luaT_adjustvarargs(L, GETARG_A(i), ci, cl->p));");
                println("        if (l_unlikely(trap)) {  /* previous \"Protect\" updated trap */");
                println("          luaD_hookcall(L, ci);");
                println("          L->oldpc = 1;  /* next opcode will be seen as a \"new\" line */");
                println("        }");
                println("        updatebase(ci);  /* function has new base after adjustment */");
                // FALLTHROUGH
                break;
            }
            case OP_EXTRAARG: {
                println("        lua_assert(0);");
                // FALLTHROUGH
                break;
            }
            default: {
                char msg[64];
                sprintf(msg, "%s is not implemented yet", opnames[op]);
                fatal_error(msg);
                break;
            }
        }
        println("      }");
        printnl();
    }
    println("    }");
    println("  }");
    println("}");
    printnl();
}
