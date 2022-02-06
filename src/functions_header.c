//
// Most of what we need is copied verbatim from lvm.c
//

// Get access to static" functions from lvm.c, but be careful to not re-define the
// functions that are already exported by liblua.a
#define LUAOT_IS_MODULE 1
#include "lvm.c"


//
//
//

typedef struct {
    //lua_State *L;
    //Instruction i;
    CallInfo *ci;
    LClosure *cl;
    TValue *k;
    StkId base;
    int trap;
} LuaotExecuteState;


//
// These operations normally use `pc++` to skip metamethod calls in the
// fast case. We have to replace this with `goto LUAOT_SKIP1`
//

#undef op_arithI
#undef op_arith
#undef op_arithK
#undef op_bitwiseK
#undef op_bitwise

#define op_arithI_aux(L,iop,fop) {  \
  if (ttisinteger(v1)) {  \
    lua_Integer iv1 = ivalue(v1);  \
    setivalue(s2v(ra), iop(L, iv1, imm));  \
  }  \
  else if (ttisfloat(v1)) {  \
    lua_Number nb = fltvalue(v1);  \
    lua_Number fimm = cast_num(imm);  \
    setfltvalue(s2v(ra), fop(L, nb, fimm)); \
 } else { \
    luaot_mmbin(L,ctx,pc); \
 }}

#undef op_arithf_aux
#define op_arithf_aux(L,fop) {  \
  lua_Number n1; lua_Number n2;  \
  if (tonumberns(v1, n1) && tonumberns(v2, n2)) {  \
    setfltvalue(s2v(ra), fop(L, n1, n2));  \
  } else { \
    luaot_mmbin(L,ctx,pc); \
  }}

#undef op_arith_aux
#define op_arith_aux(L,iop,fop) {  \
  if (ttisinteger(v1) && ttisinteger(v2)) {  \
    lua_Integer i1 = ivalue(v1); lua_Integer i2 = ivalue(v2);  \
    setivalue(s2v(ra), iop(L, i1, i2));  \
  }  \
  else op_arithf_aux(L,fop); }

#define op_bitwiseK_aux(L,op) {  \
  lua_Integer i1;  \
  lua_Integer i2 = ivalue(v2);  \
  if (tointegerns(v1, &i1)) {  \
    setivalue(s2v(ra), op(i1, i2));  \
  } else { \
    luaot_mmbin(L,ctx,pc); \
  }}

#define op_bitwise_aux(L,op) {  \
  lua_Integer i1; lua_Integer i2;  \
  if (tointegerns(v1, &i1) && tointegerns(v2, &i2)) {  \
    setivalue(s2v(ra), op(i1, i2));  \
  } else { \
    luaot_mmbin(L,ctx,pc); \
  }}

//
//
//

#define luaot_arithI(L,f) { \
  TValue *v1 = vRB(i);  \
  int imm = GETARG_sC(i);  \
  (f)(L, ctx, pc, ra, v1, imm); \
}

#define luaot_arithK(L, f) { \
  TValue *v1 = vRB(i); \
  TValue *v2 = KC(i); \
  lua_assert(ttisnumber(v2)); \
  (f)(L, ctx, pc, ra, v1, v2); \
}

#define luaot_arith(L, f) { \
  TValue *v1 = vRB(i); \
  TValue *v2 = vRC(i); \
  (f)(L, ctx, pc, ra, v1, v2); \
}

#define luaot_arithf(L, f) { \
  TValue *v1 = vRB(i); \
  TValue *v2 = vRC(i); \
  (f)(L, ctx, pc, ra, v1, v2); \
}

#define luaot_arithfK(L, f) {  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = KC(i); \
  lua_assert(ttisnumber(v2));  \
  (f)(L, ctx, pc, ra, v1, v2); \
}

#define luaot_bitwiseK(L, f) { \
  TValue *v1 = vRB(i); \
  TValue *v2 = KC(i);  \
  (f)(L, ctx, pc, ra, v1, v2); \
}

#define luaot_bitwise(L, f) {  \
  TValue *v1 = vRB(i); \
  TValue *v2 = vRC(i); \
  (f)(L, ctx, pc, ra, v1, v2); \
}

/*
** Comparison
*/
 
#undef  op_order
#define op_order(L,opi,opn,other) {  \
  int cond;  \
  if (ttisinteger(s2v(ra)) && ttisinteger(rb)) {  \
    lua_Integer ia = ivalue(s2v(ra));  \
    lua_Integer ib = ivalue(rb);  \
    cond = opi(ia, ib);  \
  }  \
  else if (ttisnumber(s2v(ra)) && ttisnumber(rb))  \
    cond = opn(s2v(ra), rb);  \
  else  \
    Protect(cond = other(L, s2v(ra), rb));  \
  return cond; \
}

#undef  op_orderI
#define op_orderI(L,opi,opf,inv,tm) {  \
        int cond;  \
        if (ttisinteger(s2v(ra)))  \
          cond = opi(ivalue(s2v(ra)), im);  \
        else if (ttisfloat(s2v(ra))) {  \
          lua_Number fa = fltvalue(s2v(ra));  \
          lua_Number fim = cast_num(im);  \
          cond = opf(fa, fim);  \
        }  \
        else {  \
          Protect(cond = luaT_callorderiTM(L, s2v(ra), im, inv, isf, tm));  \
        }  \
        return cond; \
}

/*
** some macros for common tasks in 'luaV_execute'
*/

#undef  RA
#define RA(i)	(ctx->base+GETARG_A(i))

#undef  RB
#define RB(i)	(ctx->base+GETARG_B(i))

#undef  vRB
#define vRB(i)	s2v(RB(i))

#undef  KB
#define KB(i)	(ctx->k+GETARG_B(i))

#undef  RC
#define RC(i)	(ctx->base+GETARG_C(i))

#undef  vRC
#define vRC(i)	s2v(RC(i))

#undef  KC
#define KC(i)	(ctx->k+GETARG_C(i))

#undef  RKC
#define RKC(i)	((TESTARG_k(i)) ? ctx->k + GETARG_C(i) : s2v(ctx->base + GETARG_C(i)))

#undef  updatetrap
#define updatetrap(ci)  (ctx->trap = ci->u.l.trap)

#undef  updatebase
#define updatebase(ci)	(ctx->base = ci->func + 1)

#undef  updatestack
#define updatestack(ci)  \
	{ if (l_unlikely(ctx->trap)) { updatebase(ci); ra = RA(*(pc-1)); } }

//
// These are the core macros for performing jumps.
// Obviously, we have to reimplement them.
// 

#undef dojump

#undef  donextjump
#define donextjump(ci)	{ updatetrap(ci); goto LUAOT_NEXT_JUMP; }

#undef  docondjump
#define docondjump()	if (cond != GETARG_k(i)) goto LUAOT_SKIP1; else donextjump(ctx->ci);

//
// The program counter is now known statically at each program point.
//

#undef  savepc
#define savepc(L)	(ctx->ci->u.l.savedpc = pc)

//

#undef  Protect
#define Protect(exp)  (savestate(L,ctx->ci), (exp), updatetrap(ctx->ci))

#undef  ProtectNT
#define ProtectNT(exp)  (savepc(L), (exp), updatetrap(ctx->ci))

#undef  halfProtect
#define halfProtect(exp)  (savestate(L,ctx->ci), (exp))

#undef checkGC
#define checkGC(L,c)  \
	{ luaC_condGC(L, (savepc(L), L->top = (c)), \
                         updatetrap(ctx->ci)); \
           luai_threadyield(L); }

//
// Our modified version of vmfetch(). Since instr and index are compile time
// constants, the C compiler should be able to optimize the code in many cases.
//

#undef  vmfetch
#define aot_vmfetch(instr)	{ \
  if (l_unlikely(ctx->trap)) {  /* stack reallocation or hooks? */ \
    luaot_vmfetch_trap(L, ctx, pc-1); \
  } \
  i = instr; \
  ra = RA(i); /* WARNING: any stack reallocation invalidates 'ra' */ \
}

#undef  vmdispatch
#undef  vmcase
#undef  vmbreak


static
void luaot_vmfetch_trap(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc)
{
    ctx->trap = luaG_traceexec(L, pc);  /* handle hooks */ \
    updatebase(ctx->ci);  /* correct stack */ \
}

static
void luaot_mmbin(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc)
{
    Instruction i;
    StkId ra;
    aot_vmfetch(*pc);
    pc = pc + 1;

    switch (GET_OPCODE(i)) {
        case OP_MMBIN: {
            Instruction pi = *(pc - 2);  /* original arith. expression */
            TValue *rb = vRB(i);
            TMS tm = (TMS)GETARG_C(i);
            StkId result = RA(pi);
            lua_assert(OP_ADD <= GET_OPCODE(pi) && GET_OPCODE(pi) <= OP_SHR);
            Protect(luaT_trybinTM(L, s2v(ra), rb, result, tm));
            break;
        }
        case OP_MMBINI: {
            Instruction pi = *(pc - 2);  /* original arith. expression */
            int imm = GETARG_sB(i);
            TMS tm = (TMS)GETARG_C(i);
            int flip = GETARG_k(i);
            StkId result = RA(pi);
            Protect(luaT_trybiniTM(L, s2v(ra), imm, flip, result, tm));
            break;
        }
        case OP_MMBINK: {
            Instruction pi = *(pc - 2);  /* original arith. expression */
            TValue *imm = KB(i);
            TMS tm = (TMS)GETARG_C(i);
            int flip = GETARG_k(i);
            StkId result = RA(pi);
            Protect(luaT_trybinassocTM(L, s2v(ra), imm, flip, result, tm));
            break;
        }
        default: {
            lua_assert(0);
            break;
        }
    }
}


static
void luaot_MOVE(lua_State *L,
                StkId ra, StkId rb)
{
    setobjs2s(L, ra, rb);
}

static
void luaot_LOADI(lua_State *L,
                 StkId ra, lua_Integer b)
{
    setivalue(s2v(ra), b);
}

static
void luaot_LOADF(StkId ra, int b)
{
    setfltvalue(s2v(ra), cast_num(b));
}

static
void luaot_LOADK(lua_State *L,
                 StkId ra, TValue * rb)
{
    setobj2s(L, ra, rb);
}

static
void luaot_LOADKX(lua_State *L,
                  StkId ra, TValue *rb)
{
    setobj2s(L, ra, rb);
}

static
void luaot_LOADFALSE(StkId ra)
{
    setbfvalue(s2v(ra));
}

static
void luaot_LFALSESKIP(StkId ra)
{
     setbfvalue(s2v(ra));
}

static
void luaot_LOADTRUE(StkId ra)
{
    setbtvalue(s2v(ra));
}

static
void luaot_LOADNIL(StkId ra, int b)
{
    do {
        setnilvalue(s2v(ra++));
    } while (b--);
}

static
void luaot_GETUPVAL(lua_State *L, LuaotExecuteState *ctx,
                    StkId ra, int b)
{
    setobj2s(L, ra, ctx->cl->upvals[b]->v);
}

static
void luaot_SETUPVAL(lua_State *L, LuaotExecuteState *ctx,
                    StkId ra, int b)
{
    UpVal *uv = ctx->cl->upvals[b];
    setobj(L, uv->v, s2v(ra));
    luaC_barrier(L, uv, s2v(ra));
}

static
void luaot_GETTABUP(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                    StkId ra, int b, TValue *rc)
{
    const TValue *slot;
    TValue *upval = ctx->cl->upvals[b]->v;
    TString *key = tsvalue(rc);  /* key must be a string */
    if (luaV_fastget(L, upval, key, slot, luaH_getshortstr)) {
      setobj2s(L, ra, slot);
    }
    else
      Protect(luaV_finishget(L, upval, rc, ra, slot));
}

static
void luaot_GETTABLE(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                    StkId ra, TValue *rb, TValue *rc)
{
    const TValue *slot;
    lua_Unsigned n;
    if (ttisinteger(rc)  /* fast track for integers? */
        ? (cast_void(n = ivalue(rc)), luaV_fastgeti(L, rb, n, slot))
        : luaV_fastget(L, rb, rc, slot, luaH_get)) {
      setobj2s(L, ra, slot);
    }
    else
      Protect(luaV_finishget(L, rb, rc, ra, slot));
}

static
void luaot_GETI(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                StkId ra, TValue *rb, int c)
{
    const TValue *slot;
    if (luaV_fastgeti(L, rb, c, slot)) {
      setobj2s(L, ra, slot);
    }
    else {
      TValue key;
      setivalue(&key, c);
      Protect(luaV_finishget(L, rb, &key, ra, slot));
    }
}

static
void luaot_GETFIELD(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                    StkId ra, TValue *rb, TValue *rc)
{
    const TValue *slot;
    TString *key = tsvalue(rc);  /* key must be a string */
    if (luaV_fastget(L, rb, key, slot, luaH_getshortstr)) {
      setobj2s(L, ra, slot);
    }
    else
      Protect(luaV_finishget(L, rb, rc, ra, slot));
}

static
void luaot_SETTABUP(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                    int a, TValue *rb, TValue *rc)
{
    const TValue *slot;
    TValue *upval = ctx->cl->upvals[a]->v;
    TString *key = tsvalue(rb);  /* key must be a string */
    if (luaV_fastget(L, upval, key, slot, luaH_getshortstr)) {
      luaV_finishfastset(L, upval, slot, rc);
    }
    else
      Protect(luaV_finishset(L, upval, rb, rc, slot));
}

static
void luaot_SETTABLE(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                    StkId ra, TValue *rb, TValue *rc)
{
    const TValue *slot;
    lua_Unsigned n;
    if (ttisinteger(rb)  /* fast track for integers? */
        ? (cast_void(n = ivalue(rb)), luaV_fastgeti(L, s2v(ra), n, slot))
        : luaV_fastget(L, s2v(ra), rb, slot, luaH_get)) {
      luaV_finishfastset(L, s2v(ra), slot, rc);
    }
    else
      Protect(luaV_finishset(L, s2v(ra), rb, rc, slot));
}

static
void luaot_SETI(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                StkId ra, int b, TValue *rc)
{
    const TValue *slot;
    if (luaV_fastgeti(L, s2v(ra), b, slot)) {
      luaV_finishfastset(L, s2v(ra), slot, rc);
    }
    else {
      TValue key;
      setivalue(&key, b);
      Protect(luaV_finishset(L, s2v(ra), &key, rc, slot));
    }
}

static
void luaot_SETFIELD(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                    StkId ra, TValue *rb, TValue *rc)
{
    const TValue *slot;
    TString *key = tsvalue(rb);  /* key must be a string */
    if (luaV_fastget(L, s2v(ra), key, slot, luaH_getshortstr)) {
      luaV_finishfastset(L, s2v(ra), slot, rc);
    }
    else
      Protect(luaV_finishset(L, s2v(ra), rb, rc, slot));
}

static
void luaot_NEWTABLE_0(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                      StkId ra, int b, int c)
{
    Table *t;
    if (b > 0)
      b = 1 << (b - 1);  /* size is 2^(b - 1) */
    lua_assert((!TESTARG_k(i)) == (GETARG_Ax(0x%08x) == 0));
    /* skip extra argument */
    L->top = ra + 1;  /* correct top in case of emergency GC */
    t = luaH_new(L);  /* memory allocation */
    sethvalue2s(L, ra, t);
    if (b != 0 || c != 0)
      luaH_resize(L, t, c, b);  /* idem */
    checkGC(L, ra + 1);
}

static
void luaot_NEWTABLE_1(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                      StkId ra, int b, int c)
{
    Table *t;
    if (b > 0)
      b = 1 << (b - 1);  /* size is 2^(b - 1) */
    lua_assert((!TESTARG_k(i)) == (GETARG_Ax(0x%08x) == 0));
    /* skip extra argument */
    L->top = ra + 1;  /* correct top in case of emergency GC */
    t = luaH_new(L);  /* memory allocation */
    sethvalue2s(L, ra, t);
    if (b != 0 || c != 0)
      luaH_resize(L, t, c, b);  /* idem */
    checkGC(L, ra + 1);
}

static
void luaot_SELF(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                StkId ra, TValue *rb, TValue *rc)
{
    const TValue *slot;
    TString *key = tsvalue(rc);  /* key must be a string */
    setobj2s(L, ra + 1, rb);
    if (luaV_fastget(L, rb, key, slot, luaH_getstr)) {
      setobj2s(L, ra, slot);
    }
    else
      Protect(luaV_finishget(L, rb, rc, ra, slot));
}

static
void luaot_ADDI(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *v1, int imm)
{
    op_arithI_aux(L, l_addi, luai_numadd);
}

static
void luaot_ADDK(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *v1, TValue *v2)
{
    op_arith_aux(L, l_addi, luai_numadd);
}

static
void luaot_SUBK(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *v1, TValue *v2)
{
    op_arith_aux(L, l_subi, luai_numsub);
}

static
void luaot_MULK(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *v1, TValue *v2)
{
    op_arith_aux(L, l_muli, luai_nummul);
}

static
void luaot_MODK(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *v1, TValue *v2)
{
    op_arith_aux(L, luaV_mod, luaV_modf);
}

static
void luaot_POWK(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *v1, TValue *v2)
{
    op_arithf_aux(L, luai_numpow);
}

static
void luaot_DIVK(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *v1, TValue *v2)
{
    op_arithf_aux(L, luai_numdiv);
}

static
void luaot_IDIVK(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                StkId ra, TValue *v1, TValue *v2)
{
    op_arith_aux(L, luaV_idiv, luai_numidiv);
}

static
void luaot_BANDK(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                StkId ra, TValue *v1, TValue *v2)
{
    op_bitwiseK_aux(L, l_band);
}

static
void luaot_BORK(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *v1, TValue *v2)
{
    op_bitwiseK_aux(L, l_bor);
}

static
void luaot_BXORK(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                StkId ra, TValue *v1, TValue *v2)
{
    op_bitwiseK_aux(L, l_bxor);
}

static
void luaot_SHRI(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *rb, int ic)
{
    lua_Integer ib;
    if (tointegerns(rb, &ib)) {
        setivalue(s2v(ra), luaV_shiftl(ib, -ic));
    } else {
        luaot_mmbin(L, ctx, pc);
    }
}

static
void luaot_SHLI(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,

               StkId ra, TValue *rb, int ic)
{
    lua_Integer ib;
    if (tointegerns(rb, &ib)) {
        setivalue(s2v(ra), luaV_shiftl(ic, ib));
    } else {
        luaot_mmbin(L, ctx, pc);
    }
}

static
void luaot_ADD(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, TValue *v1, TValue *v2)
{
    op_arith_aux(L, l_addi, luai_numadd);
}

static
void luaot_SUB(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, TValue *v1, TValue *v2)
{
    op_arith_aux(L, l_subi, luai_numsub);
}

static
void luaot_MUL(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, TValue *v1, TValue *v2)
{
    op_arith_aux(L, l_muli, luai_nummul);
}

static
void luaot_MOD(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, TValue *v1, TValue *v2)
{
    op_arith_aux(L, luaV_mod, luaV_modf);
}

static
void luaot_POW(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, TValue *v1, TValue *v2)
{
    op_arithf_aux(L, luai_numpow);
}

static
void luaot_DIV(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, TValue *v1, TValue *v2)
{
    op_arithf_aux(L, luai_numdiv);
}

static
void luaot_IDIV(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *v1, TValue *v2)
{
    op_arith_aux(L, luaV_idiv, luai_numidiv);
}

static
void luaot_BAND(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *v1, TValue *v2)
{
    op_bitwise_aux(L, l_band);
}

static
void luaot_BOR(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, TValue *v1, TValue *v2)
{
    op_bitwise_aux(L, l_bor);
}

static
void luaot_BXOR(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *v1, TValue *v2)
{
    op_bitwise_aux(L, l_bxor);
}

static
void luaot_SHR(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, TValue *v1, TValue *v2)
{
    op_bitwise_aux(L, luaV_shiftr);
}

static
void luaot_SHL(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, TValue *v1, TValue *v2)
{
    op_bitwise_aux(L, luaV_shiftl);
}

static
void luaot_UNM(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *rb)
{
    lua_Number nb;
    if (ttisinteger(rb)) {
      lua_Integer ib = ivalue(rb);
      setivalue(s2v(ra), intop(-, 0, ib));
    }
    else if (tonumberns(rb, nb)) {
      setfltvalue(s2v(ra), luai_numunm(L, nb));
    }
    else
      Protect(luaT_trybinTM(L, rb, rb, ra, TM_UNM));
}

static
void luaot_BNOT(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                StkId ra, TValue *rb)
{
    lua_Integer ib;
    if (tointegerns(rb, &ib)) {
      setivalue(s2v(ra), intop(^, ~l_castS2U(0), ib));
    }
    else
      Protect(luaT_trybinTM(L, rb, rb, ra, TM_BNOT));
}

static
void luaot_NOT(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *rb)
{
    if (l_isfalse(rb))
      setbtvalue(s2v(ra));
    else
      setbfvalue(s2v(ra));
}

static
void luaot_LEN(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, TValue *rb)
{
    Protect(luaV_objlen(L, ra, rb));
}

static
void luaot_CONCAT(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                  StkId ra, int n)
{
    L->top = ra + n;  /* mark the end of concat operands */
    ProtectNT(luaV_concat(L, n));
    checkGC(L, L->top); /* 'luaV_concat' ensures correct top */
}

static
void luaot_CLOSE(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                 StkId ra)
{
    Protect(luaF_close(L, ra, LUA_OK, 1));
}

static
void luaot_TBC(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra)
{
    /* create new to-be-closed upvalue */
    halfProtect(luaF_newtbcupval(L, ra));
}

static
int luaot_EQ(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, TValue *rb)
{
    int cond;
    Protect(cond = luaV_equalobj(L, s2v(ra), rb));
    return cond;
}

static
int luaot_LT(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
             StkId ra, TValue *rb)
{
    op_order(L, l_lti, LTnum, lessthanothers);
}

static
int luaot_LE(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
             StkId ra, TValue *rb)
{
    op_order(L, l_lei, LEnum, lessequalothers);
}

static
int luaot_EQI(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, int im, int isf)
{
    int cond;
    if (ttisinteger(s2v(ra)))
      cond = (ivalue(s2v(ra)) == im);
    else if (ttisfloat(s2v(ra)))
      cond = luai_numeq(fltvalue(s2v(ra)), cast_num(im));
    else
      cond = 0;  /* other types cannot be equal to a number */
    return cond;
}

static
int luaot_LTI(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, int im, int isf)
{
    op_orderI(L, l_lti, luai_numlt, 0, TM_LT);
}

static
int luaot_LEI(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, int im, int isf)
{
    op_orderI(L, l_lei, luai_numle, 0, TM_LE);
}

static
int luaot_GTI(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, int im, int isf)
{
    op_orderI(L, l_gti, luai_numgt, 1, TM_LT);
}

static
int luaot_GEI(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
              StkId ra, int im, int isf)
{
    op_orderI(L, l_gei, luai_numge, 1, TM_LE);
}

static
CallInfo* luaot_CALL(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
               StkId ra, int b, int nresults)
{
    CallInfo *newci;
    if (b != 0)  /* fixed number of arguments? */
        L->top = ra + b;  /* top signals number of arguments */
    /* else previous instruction set top */
    savepc(L);  /* in case of errors */
    if ((newci = luaD_precall(L, ra, nresults)) == NULL) {
        updatetrap(ctx->ci);  /* C call; nothing else to be done */
        return NULL;
    } else {
        ctx->ci = newci;
        ctx->ci->callstatus = 0;  /* call re-uses 'luaV_execute' */
        return ctx->ci;
    }
}

static
int luaot_FORLOOP(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                   StkId ra)
{
    if (ttisinteger(s2v(ra + 2))) {  /* integer loop? */
      lua_Unsigned count = l_castS2U(ivalue(s2v(ra + 1)));
      if (count > 0) {  /* still more iterations? */
        lua_Integer step = ivalue(s2v(ra + 2));
        lua_Integer idx = ivalue(s2v(ra));  /* internal index */
        chgivalue(s2v(ra + 1), count - 1);  /* update counter */
        idx = intop(+, idx, step);  /* add step to index */
        chgivalue(s2v(ra), idx);  /* update internal index */
        setivalue(s2v(ra + 3), idx);  /* and control variable */
        return 1; // Jump back
      }
    }
    else if (floatforloop(ra)) /* float loop */
      return 1; // jump back
    updatetrap(ctx->ci);  /* allows a signal to break the loop */
    return 0;
}

static
int luaot_FORPREP(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                  StkId ra)
{
    savestate(L, ctx->ci);  /* in case of errors */
    return forprep(L, ra);
}

static
void luaot_TFORPREP(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                   StkId ra)
{
    /* create to-be-closed upvalue (if needed) */
    halfProtect(luaF_newtbcupval(L, ra + 3));
}

static
void luaot_TFORCALL(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                   StkId ra, int c)
{
    /* 'ra' has the iterator function, 'ra + 1' has the state,
       'ra + 2' has the control variable, and 'ra + 3' has the
       to-be-closed variable. The call will use the stack after
       these values (starting at 'ra + 4')
    */
    /* push function, state, and control variable */
    memcpy(ra + 4, ra, 3 * sizeof(*ra));
    L->top = ra + 4 + 3;
    ProtectNT(luaD_call(L, ra + 4, c));  /* do the call */
    updatestack(ctx->ci);  /* stack may have changed */
}

static
int luaot_TFORLOOP(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                   StkId ra)
{
    if (!ttisnil(s2v(ra + 4))) {  /* continue loop? */
      setobjs2s(L, ra + 2, ra + 4);  /* save control variable */
      return 1;
    } else {
      return 0;
    }
}

static
void luaot_CLOSURE(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                   StkId ra, int b)
{
    Proto *p = ctx->cl->p->p[b];
    halfProtect(pushclosure(L, p, ctx->cl->upvals, ctx->base, ra));
    checkGC(L, ra + 1);
}

static
void luaot_VARARG(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                  StkId ra, int n)
{
    Protect(luaT_getvarargs(L, ctx->ci, ra, n));
}

static
void luaot_VARARGPREP(lua_State *L, LuaotExecuteState *ctx, const Instruction *pc,
                      int a)
{
    ProtectNT(luaT_adjustvarargs(L, a, ctx->ci, ctx->cl->p));
    if (l_unlikely(ctx->trap)) {  /* previous "Protect" updated trap */
      luaD_hookcall(L, ctx->ci);
      L->oldpc = 1;  /* next opcode will be seen as a "new" line */
    }
    updatebase(ctx->ci);  /* function has new base after adjustment */
}
