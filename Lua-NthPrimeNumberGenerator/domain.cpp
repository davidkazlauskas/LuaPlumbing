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

    auto doubleNode = TNF::makePodNode<double>(
        [](void* ptr,const char* arg) {
            double* asInt = reinterpret_cast<double*>(ptr);
            new (ptr) double(std::atof(arg));
        },
        [](const void* ptr,std::string& out) {
            out = std::to_string(
                *reinterpret_cast<const double*>(ptr));
        }
    );

    templatious::DynVPackFactory buildTypeIndex() {
        templatious::DynVPackFactoryBuilder bld;
        bld.attachNode("int",intNode);
        bld.attachNode("double",doubleNode);
        return bld.getFactory();
    }
}

static auto vFactory = buildTypeIndex();

int getStringArray(
    lua_State* state,
    int index,
    templatious::StaticVector<const char*>& arr)
{
    const int KEY = -2;
    const int VAL = -1;

    lua_pushnil(state);
    int count = 0;

    while (0 != ::lua_next(state,index)) {
        ++count;
        const char* curVal = lua_tostring(state,VAL);
        SA::add(arr,curVal);

        int num = lua_tointeger(state,KEY);
        assert( num == count && "Index mismatch." );

        lua_pop(state,1);
    }
    return SA::size(arr);
}

// -1 -> values
// -2 -> types
// -3 -> name
// -4 -> context
int registerPack(lua_State* state) {
    LuaContext* ctx = reinterpret_cast<LuaContext*>(::lua_touserdata(state,-4));
    const char* name = reinterpret_cast<const char*>(::lua_tostring(state,-3));

    templatious::StaticBuffer< const char*, 64 > buffer;
    auto types = buffer.getStaticVector(32);
    auto values = buffer.getStaticVector();

    const int VALUE_IDX = -2;
    const int TYPE_IDX = -3;

    int sizeValue = getStringArray(state,VALUE_IDX,values);
    int sizeTypes = getStringArray(state,TYPE_IDX,types);

    assert( sizeValue == sizeTypes && "Types and values don't match in size." );

    auto fact = ctx->getFact();
    auto p = fact->makePack(sizeValue,types.rawBegin(),values.rawBegin());
    ctx->indexPack(name,p);

    return 0;
}

int sendPackAsync(lua_State* state) {

    return 0;
}

void initDomain(LuaContext& ctx) {
    auto s = ctx.s();
    luaL_openlibs(s);
    ctx.regFunction("registerPack",&registerPack);
    ctx.setFactory(std::addressof(vFactory));

    bool success = luaL_dofile(s,"main.lua") == 0;
    if (!success) {
        printf("%s\n", lua_tostring(s, -1));
    }
    assert( success );

    lua_getglobal(s,"initDomain");
    lua_pushlightuserdata(s,std::addressof(ctx));
    lua_pcall(s,1,0,0);
}
