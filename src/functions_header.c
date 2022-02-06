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

#undef  op_arithI
#define op_arithI(L,iop,fop) {  \
  TValue *v1 = vRB(i);  \
  int imm = GETARG_sC(i);  \
  if (ttisinteger(v1)) {  \
    lua_Integer iv1 = ivalue(v1);  \
    setivalue(s2v(ra), iop(L, iv1, imm));  \
    goto LUAOT_SKIP1; \
  }  \
  else if (ttisfloat(v1)) {  \
    lua_Number nb = fltvalue(v1);  \
    lua_Number fimm = cast_num(imm);  \
    setfltvalue(s2v(ra), fop(L, nb, fimm)); \
    goto LUAOT_SKIP1; \
 }}

#undef op_arithf_aux
#define op_arithf_aux(L,v1,v2,fop) {  \
  lua_Number n1; lua_Number n2;  \
  if (tonumberns(v1, n1) && tonumberns(v2, n2)) {  \
    setfltvalue(s2v(ra), fop(L, n1, n2));  \
    goto LUAOT_SKIP1; \
  }}

#undef op_arith_aux
#define op_arith_aux(L,v1,v2,iop,fop) {  \
  if (ttisinteger(v1) && ttisinteger(v2)) {  \
    lua_Integer i1 = ivalue(v1); lua_Integer i2 = ivalue(v2);  \
    setivalue(s2v(ra), iop(L, i1, i2));  \
    goto LUAOT_SKIP1; \
  }  \
  else op_arithf_aux(L, v1, v2, fop); }

#undef  op_bitwiseK
#define op_bitwiseK(L,op) {  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = KC(i);  \
  lua_Integer i1;  \
  lua_Integer i2 = ivalue(v2);  \
  if (tointegerns(v1, &i1)) {  \
    setivalue(s2v(ra), op(i1, i2));  \
    goto LUAOT_SKIP1; \
  }}

#undef  op_bitwise
#define op_bitwise(L,op) {  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = vRC(i);  \
  lua_Integer i1; lua_Integer i2;  \
  if (tointegerns(v1, &i1) && tointegerns(v2, &i2)) {  \
    setivalue(s2v(ra), op(i1, i2));  \
    goto LUAOT_SKIP1; \
  }}

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
	{ if (l_unlikely(ctx->trap)) { updatebase(ci); ra = RA(i); } }

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
