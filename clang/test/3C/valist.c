// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

#include <stdarg.h>
typedef int lua_State;
extern void lua_lock(lua_State *);
extern void luaC_checkGC(lua_State *);
extern void lua_unlock(lua_State *);
extern const char *luaO_pushvfstring (lua_State *L, const char *fmt, va_list argp);
const char *lua_pushfstring (lua_State *L, const char *fmt, ...) {
	//CHECK: const char *lua_pushfstring(lua_State *L : itype(_Ptr<lua_State>), const char *fmt : itype(_Ptr<const char>), ...) : itype(_Ptr<const char>) {
  const char *ret;
	//CHECK: const char *ret;
  va_list argp;
  lua_lock(L);
  va_start(argp, fmt);
  ret = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  luaC_checkGC(L);
  lua_unlock(L);
  return ret;
}
/*force output*/
int *p;
	//CHECK: _Ptr<int> p = ((void *)0);
