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

    typedef std::weak_ptr< LuaContext > WeakCtxPtr;

    void writePtrToString(const void* ptr,std::string& out) {
        out.clear();
        TEMPLATIOUS_REPEAT(sizeof(ptr)) {
            out += '7';
        }
        out += '3';
        const char* ser = reinterpret_cast<const char*>(&ptr);
        TEMPLATIOUS_0_TO_N(i,sizeof(ptr)) {
            out[i] = ser[i];
        }
    }

    void* ptrFromString(const std::string& out) {
        void* result = nullptr;
        memcpy(&result,out.data(),sizeof(result));
        return result;
    }

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

    typedef std::shared_ptr< templatious::VirtualPack > VPackPtr;

    auto vpackNode = TNF::makeFullNode<VPackPtr>(
        // here, we assume we receive pointer
        // to exact copy of the pack
        [](void* ptr,const char* arg) {
            new (ptr) VPackPtr(
                *reinterpret_cast<const VPackPtr*>(arg)
            );
        },
        [](void* ptr) {
            VPackPtr* vpPtr = reinterpret_cast<VPackPtr*>(ptr);
            vpPtr->~shared_ptr();
        },
        [](const void* ptr,std::string& out) {
            // write pointer
            writePtrToString(ptr,out);
        }
    );

    auto messeagableWeakNode = TNF::makeFullNode< WeakMsgPtr >(
        [](void* ptr,const char* arg) {
            new (ptr) WeakMsgPtr(
                *reinterpret_cast<const WeakMsgPtr*>(arg)
            );
        },
        [](void* ptr) {
            WeakMsgPtr* msPtr = reinterpret_cast<WeakMsgPtr*>(ptr);
            msPtr->~weak_ptr();
        },
        [](const void* ptr,std::string& out) {
            writePtrToString(ptr,out);
        }
    );

#define ATTACH_NAMED_DUMMY(factory,name,type)   \
    factory.attachNode(name,TNF::makeDummyNode< type >(name))

    templatious::DynVPackFactory buildTypeIndex() {

        templatious::DynVPackFactoryBuilder bld;
        bld.attachNode("int",intNode);
        bld.attachNode("double",doubleNode);
        bld.attachNode("string",stringNode);
        bld.attachNode("vpack",vpackNode);
        bld.attachNode("vmsg",messeagableWeakNode);

        typedef MainWindowInterface MWI;
        typedef GenericMesseagableInterface GNI;
        ATTACH_NAMED_DUMMY( bld, "mwnd_insetprog", MWI::InSetProgress );
        ATTACH_NAMED_DUMMY( bld, "mwnd_insetlabel", MWI::InSetStatusText );
        ATTACH_NAMED_DUMMY( bld, "mwnd_querylabel", MWI::QueryLabelText );
        ATTACH_NAMED_DUMMY( bld, "mwnd_inattachmsg", MWI::InAttachMesseagable );
        ATTACH_NAMED_DUMMY( bld, "gen_inattachitself", GNI::AttachItselfToMesseagable );

        return bld.getFactory();
    }
}

static auto vFactory = buildTypeIndex();

// -1 -> value tree
// -2 -> mesg name
// -3 -> context
int lua_sendPack(lua_State* state) {
    WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(::lua_touserdata(state,-3));
    const char* name = reinterpret_cast<const char*>(::lua_tostring(state,-2));

    auto ctx = ctxW->lock();
    assert( nullptr != ctx && "Context already dead?" );

    auto outTree = ctx->makeTreeFromTable(state,-1);

    return 0;
}

// -1 -> weak context ptr
int lua_freeWeakLuaContext(lua_State* state) {
    WeakCtxPtr* ctx = reinterpret_cast< WeakCtxPtr* >(
    ::lua_touserdata(state,-1));

    ctx->~weak_ptr();
    return 0;
}


void initDomain(const std::shared_ptr< LuaContext >& ctx) {
    ctx->setFactory(&vFactory);

    auto s = ctx->s();
    luaL_openlibs(s);

    ctx->regFunction("nat_sendPack",&lua_sendPack);

    bool success = luaL_dofile(s,"main.lua") == 0;
    if (!success) {
        printf("%s\n", lua_tostring(s, -1));
    }
    assert( success );

    void* adr = ::lua_newuserdata(s, sizeof(WeakCtxPtr) );
    new (adr) WeakCtxPtr(ctx);

    ::luaL_newmetatable(s,"WeakMsgPtr");
    ::lua_pushcfunction(s,&lua_freeWeakLuaContext);
    ::lua_setfield(s,-2,"__gc");
    ::lua_setmetatable(s,-2);

    ::lua_getglobal(s,"initDomain");
    ::lua_pushvalue(s,-2);
    ::lua_pcall(s,1,0,0);
}

void getCharNodes(lua_State* state,int tblidx,
    std::vector< VTree >& outVect)
{
    int iter = 0;

    const int KEY = -2;
    const int VAL = -1;

    std::string outKey,outVal;
    ::lua_pushnil(state);
    int trueIdx = tblidx - 1;
    while (0 != ::lua_next(state,trueIdx)) {
        switch (::lua_type(state,KEY)) {
            case LUA_TSTRING:
                outKey = ::lua_tostring(state,KEY);
                break;
            case LUA_TNUMBER:
                outKey = std::to_string(::lua_tonumber(state,KEY));
                break;
        }

        bool isTable = false;
        switch(::lua_type(state,VAL)) {
            case LUA_TNUMBER:
                outVal = std::to_string(::lua_tonumber(state,VAL));
                break;
            case LUA_TSTRING:
                outVal = ::lua_tostring(state,VAL);
                break;
            case LUA_TTABLE:
                outVal = "[table]";
                isTable = true;
                break;
        }
        if (!isTable) {
            outVect.emplace_back(outKey.c_str(),outVal.c_str());
        } else {
            outVect.emplace_back(outKey.c_str(),std::vector< VTree >());
            auto& treeRef = outVect.back().getInnerTree();
            getCharNodes(state,VAL,treeRef);
        }

        ::lua_pop(state,1);
    }

}

void LuaContext::representAsPtr(
    VTree& typeTree,VTree& valueTree,
    int idx,const char** type,const char** value,
    StackDump& d
)
{
    static const char* VPNAME = "vpack";
    static const char* VMSGNAME = "vmsg";

    if (typeTree.getType() == VTree::Type::VTreeItself) {
        const char* types[32];
        const char* values[32];
        SM::traverse<true>(
            [&](int idx,
                VTree& type,
                VTree& value)
            {
                representAsPtr(type,value,
                    idx,types,values,d);
            },
            typeTree.getInnerTree(),
            valueTree.getInnerTree()
        );
    } else if (typeTree.getString() == VMSGNAME) {
        auto target = this->getMesseagable(valueTree.getString().c_str());

        assert( nullptr != target
            && "Messeagable object doesn't exist in the context." );

        SA::add(d._bufferWMsg,target);
        type[idx] = typeTree.getString().c_str();
        value[idx] = reinterpret_cast<const char*>(
            std::addressof(d._bufferWMsg.top()));
    } else {
        assert( valueTree.getType() == VTree::Type::StdString
            && "Only string is expected now..." );
        type[idx] = typeTree.getString().c_str();
        value[idx] = valueTree.getString().c_str();
    }
}

void LuaContext::prepChildren(
    std::vector< VTree >& typeTree,
    std::vector< VTree >& valueTree,
    const char** types,const char** values,
    StackDump& d)
{
    SM::traverse<true>(
        [&](int idx,
            const VTree& type,
            const VTree& val)
        {
            this->representAsPtr(
                typeTree,valueTree,
                idx,types,values,d);
        },
        typeTree,
        valueTree
    );
}
