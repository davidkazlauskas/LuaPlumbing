#ifndef DOMAIN_H
#define DOMAIN_H

#include <lua5.2/lua.hpp>

#include "messeagable.h"

struct LuaContext {
    LuaContext() :
        _s(luaL_newstate()) {}

    lua_State* s() {
        return _s;
    }

    ~LuaContext() {
        lua_close(_s);
    }
private:
    lua_State* _s;
};

void initDomain(std::shared_ptr< Messageable > mainWindow);

#endif // DOMAIN_H

