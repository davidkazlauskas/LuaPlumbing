// All the domain logic is here.
// Notice, that while not knowing what
// Qt is or including single header of
// Qt we're able to fully manipulate
// the GUI as long as we specify the messages
// and GUI side implements them.

#include <templatious/FullPack.hpp>
#include <templatious/detail/DynamicPackCreator.hpp>

#include "domain.h"
#include "mainwindow_interface.h"

TEMPLATIOUS_TRIPLET_STD;

namespace {
    typedef templatious::TypeNodeFactory TNF;

    auto intNode = TNF::makePodNode<int>(
        [](void* ptr,const char* arg) {
            int* asInt = reinterpret_cast<int*>(ptr);
            new (ptr) int(std::atoi(arg));
        },
        [](const void* ptr,std::string& out) {
            out = std::to_string(
                *reinterpret_cast<const int*>(ptr));
        }
    );

    templatious::DynVPackFactory buildTypeIndex() {
        templatious::DynVPackFactoryBuilder bld;

        return bld.getFactory();
    }
}

void initDomain(LuaContext& ctx) {
    luaL_openlibs(ctx.s());
    bool success = luaL_dofile(ctx.s(),"main.lua") == 0;
    if (!success) {
        printf("%s\n", lua_tostring(ctx.s(), -1));
    }
    assert( success );
}
