#ifndef DOMAIN_H
#define DOMAIN_H

#include <lua5.2/lua.hpp>

#include "messeagable.h"

typedef int (*lua_CFunction) (lua_State *L);

struct LuaContext {
    LuaContext() :
        _s(luaL_newstate()) {}

    lua_State* s() {
        return _s;
    }

    void regFunction(const char* name,lua_CFunction func) {
        lua_register(_s,name,func);
    }

    ~LuaContext() {
        lua_close(_s);
    }
private:
    lua_State* _s;
};

void initDomain(LuaContext& ctx);

#endif // DOMAIN_H

