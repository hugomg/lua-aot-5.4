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

struct LuaotExecuteState {
    //lua_State *L;
    //Instruction i;
    CallInfo *ci;
    LClosure *cl;
    TValue *k;
    StkId base;
    const Instruction *pc;
    int trap;
};


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
#define savepc(L)	(ctx->ci->u.l.savedpc = LUAOT_PC)

//

#undef  Protect
#define Protect(exp)  (savestate(L,ctx->ci), (exp), updatetrap(ctx->ci))

#undef  ProtectNT
#define ProtectNT(exp)  (savepc(L), (exp), updatetrap(ctx->ci))

#undef  halfProtect
#define halfProtect(exp)  (savestate(L,ctx->ci), (exp))

//
// Our modified version of vmfetch(). Since instr and index are compile time
// constants, the C compiler should be able to optimize the code in many cases.
//

#undef  vmfetch
#define aot_vmfetch(instr)	{ \
  if (l_unlikely(ctx->trap)) {  /* stack reallocation or hooks? */ \
    ctx->trap = luaG_traceexec(L, LUAOT_PC - 1);  /* handle hooks */ \
    updatebase(ctx->ci);  /* correct stack */ \
  } \
  i = instr; \
  ra = RA(i); /* WARNING: any stack reallocation invalidates 'ra' */ \
}

#undef  vmdispatch
#undef  vmcase
#undef  vmbreak

