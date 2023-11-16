#include "lauxlib.h"
#include "lualib.h"

static int next_id = 0;

static
void bind_magic(Proto *f)
{
    // This traversal order should be the same one that luaot.c uses
    f->aot_implementation = LUAOT_FUNCTIONS[next_id++];
    for(int i=0; i < f->sizep; i++) {
        bind_magic(f->p[i]);
    }
}

int LUAOT_LUAOPEN_NAME(lua_State *L) {
    int ok = luaL_loadbuffer(L, LUAOT_MODULE_SOURCE_CODE, sizeof(LUAOT_MODULE_SOURCE_CODE)-1, "AOT Compiled module \""LUAOT_MODULE_NAME"\"");
    switch (ok) {
      case LUA_OK:
        /* No errors */
        break;
      case LUA_ERRSYNTAX:
        fprintf(stderr, "syntax error in bundled source code.\n");
        exit(1);
        break;
      case LUA_ERRMEM:
        fprintf(stderr, "memory allocation (out-of-memory) error in bundled source code.\n");
        exit(1);
        break;
      default:
        fprintf(stderr, "unknown error. This should never happen\n");
        exit(1);
    }

    LClosure *cl = (void *) lua_topointer(L, -1);
    bind_magic(cl->p);

    lua_call(L, 0, 1);
    return 1;
}
