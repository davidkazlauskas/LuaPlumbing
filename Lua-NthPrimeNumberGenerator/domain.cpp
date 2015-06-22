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

    auto stringNode = TNF::makeFullNode<std::string>(
        [](void* ptr,const char* arg) {
            new (ptr) std::string(arg);
        },
        [](void* ptr) {
            std::string* sptr = reinterpret_cast<std::string*>(ptr);
            sptr->~basic_string();
        },
        [](const void* ptr,std::string& out) {
            const std::string* sptr =
                reinterpret_cast<const std::string*>(ptr);
            out.assign(*sptr);
        }
    );


    templatious::DynVPackFactory buildTypeIndex() {

        templatious::DynVPackFactoryBuilder bld;
        bld.attachNode("int",intNode);
        bld.attachNode("double",doubleNode);
        bld.attachNode("string",stringNode);

        typedef MainWindowInterface MWI;
        bld.attachNode("mwnd_insetprog",
            TNF::makeDummyNode< MWI::InSetProgress >() );
        bld.attachNode("mwnd_insetlabel",
            TNF::makeDummyNode< MWI::InSetStatusText >() );

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

    int trueIndex = index - 1;

    while (0 != ::lua_next(state,trueIndex)) {
        ++count;
        const char* curVal = lua_tostring(state,VAL);
        SA::add(arr,curVal);

        int num = lua_tointeger(state,KEY);
        assert( num == count && "Index mismatch." );

        lua_pop(state,1);
    }
    return SA::size(arr);
}

template <class Callback>
std::shared_ptr< templatious::VirtualPack >
getVPack(lua_State* state,int typeIdx,int valIdx,Callback&& c)
{
    templatious::StaticBuffer< const char*, 64 > buffer;
    auto types = buffer.getStaticVector(32);
    auto values = buffer.getStaticVector();

    int sizeValue = getStringArray(state,valIdx,values);
    int sizeTypes = getStringArray(state,typeIdx,types);

    assert( sizeValue == sizeTypes && "Types and values don't match in size." );

    return c(sizeValue,types.rawBegin(),values.rawBegin());
}

// -1 -> values
// -2 -> types
// -3 -> name
// -4 -> context
int registerPack(lua_State* state) {
    LuaContext* ctx = reinterpret_cast<LuaContext*>(::lua_touserdata(state,-4));
    const char* name = reinterpret_cast<const char*>(::lua_tostring(state,-3));
    auto fact = ctx->getFact();
    auto p = getVPack(state,-2,-1,
        [=](int size,const char** types,const char** values) {
            return fact->makePack(size,types,values);
        });
    ctx->indexPack(name,p);
    return 0;
}

// -1 -> values
// -2 -> types
// -3 -> mesg name
// -4 -> context
int sendPack(lua_State* state) {
    LuaContext* ctx = reinterpret_cast<LuaContext*>(::lua_touserdata(state,-4));
    const char* name = reinterpret_cast<const char*>(::lua_tostring(state,-3));

    const int BACK_ARGS = 0;

    auto wptr = ctx->getMesseagable(name);
    auto locked = wptr.lock();
    if (nullptr == locked) {
        return BACK_ARGS;
    }

    auto fact = ctx->getFact();
    auto p = getVPack(state,-2,-1,
        [=](int size,const char** types,const char** values) {
            return fact->makePack(size,types,values);
        });

    locked->message(p);

    return BACK_ARGS;
}

// -1 -> function
// -2 -> values
// -3 -> types
// -4 -> mesg name
// -5 -> context
int sendPackAsync(lua_State* state) {
    LuaContext* ctx = reinterpret_cast<LuaContext*>(::lua_touserdata(state,-5));
    const char* name = reinterpret_cast<const char*>(::lua_tostring(state,-4));

    const int BACK_ARGS = 0;

    auto wptr = ctx->getMesseagable(name);
    auto locked = wptr.lock();
    if (nullptr == locked) {
        return BACK_ARGS;
    }

    auto fact = ctx->getFact();
    auto p = getVPack(state,-3,-2,
        [=](int size,const char** types,const char** values) {
            return fact->makePackCustomWCallback<
                templatious::VPACK_SYNCED
            >(size,types,values,
                [=](const templatious::detail::DynamicVirtualPackCore& core) {
                    templatious::TNodePtr outArr[32];
                    auto outSer = fact->serializeDynamicCore(core,outArr);
                    typedef std::lock_guard< std::mutex > Guard;
                    Guard g(ctx->getMutex());

                    lua_createtable(state,size,0);

                    SM::traverse<true>([&](int idx,const std::string& val) {
                        ::lua_pushnumber(state,idx + 1);
                        if (outArr[idx] == intNode) {
                            ::lua_pushnumber(state,*core.get<int>(idx));
                        } else if (outArr[idx] == doubleNode) {
                            ::lua_pushnumber(state,*core.get<double>(idx));
                        } else {
                            ::lua_pushstring(state,outSer[idx].c_str());
                        }
                        ::lua_settable(state,-3);
                    },outSer);
                }
            );
        });

    locked->message(p);

    return BACK_ARGS;
}

void initDomain(LuaContext& ctx) {
    auto s = ctx.s();
    luaL_openlibs(s);
    ctx.regFunction("nat_registerPack",&registerPack);
    ctx.regFunction("nat_sendPack",&sendPack);
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
