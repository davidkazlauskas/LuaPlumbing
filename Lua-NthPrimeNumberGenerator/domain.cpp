// All the domain logic is here.
// Notice, that while not knowing what
// Qt is or including single header of
// Qt we're able to fully manipulate
// the GUI as long as we specify the messages
// and GUI side implements them.

#include <templatious/FullPack.hpp>

#include "domain.h"
#include "mainwindow_interface.h"

TEMPLATIOUS_TRIPLET_STD;

void initDomain(LuaContext& ctx) {
    bool success = luaL_dofile(ctx.s(),"main.lua") == 0;
    if (!success) {
        printf("%s\n", lua_tostring(ctx.s(), -1));
    }
    assert( success );
}
