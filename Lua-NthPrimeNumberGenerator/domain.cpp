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

void sortVTree(VTree& tree) {
    if (VTree::Type::VTreeItself == tree.getType()) {
        auto& ref = tree.getInnerTree();
        SM::sortS(ref,[](const VTree& a,const VTree& b) {
            return a.getKey() < b.getKey();
        });
        TEMPLATIOUS_FOREACH(auto& i,ref) {
            sortVTree(i);
        }
    }
}

// -1 -> value tree
// -2 -> strong messeagable
// -3 -> context
int lua_sendPack(lua_State* state) {
    WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(::lua_touserdata(state,-3));
    StrongMsgPtr* msgPtr = reinterpret_cast<
        StrongMsgPtr*>(::lua_touserdata(state,-2));

    auto ctx = ctxW->lock();
    assert( nullptr != ctx && "Context already dead?" );

    auto& msg = *msgPtr;
    assert( nullptr != msg && "Messeagable doesn't exist." );

    auto outTree = ctx->makeTreeFromTable(state,-1);
    sortVTree(*outTree);
    auto fact = ctx->getFact();
    auto p = ctx->treeToPack(*outTree,
        [=](int size,const char** types,const char** values) {
            return fact->makePack(size,types,values);
        });

    msg->message(*p);

    return 0;
}

// -1 -> value tree
// -2 -> callback
// -3 -> strong messeagable
// -4 -> context
int lua_sendPackWCallback(lua_State* state) {
    WeakCtxPtr* ctxW = reinterpret_cast< WeakCtxPtr* >(
        ::lua_touserdata(state,-4));
    StrongMsgPtr* msgPtr = reinterpret_cast<
        StrongMsgPtr*>(::lua_touserdata(state,-3));

    auto ctx = ctxW->lock();
    assert( nullptr != ctx && "Context already dead?" );

    auto& msg = *msgPtr;
    assert( nullptr != msg && "Messeagable doesn't exist." );

    ::lua_pushvalue(state,-2);
    int funcRef = ::luaL_ref(state,LUA_REGISTRYINDEX);

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

namespace VTreeBind {

int lua_freeVtree(lua_State* state) {
    VTree* vtree = reinterpret_cast<VTree*>(
        ::lua_touserdata(state,-1));

    vtree->~VTree();
    return 0;
}

void pushVTree(lua_State* state,std::vector<VTree>& trees,int tableIdx) {
    char buf[16];
    int cnt = 1;
    int adjIdx = tableIdx - 1;
    TEMPLATIOUS_FOREACH(auto& tree,trees) {
        sprintf(buf,"_%d",cnt);
        switch (tree.getType()) {
            case VTree::Type::StdString:
                ::lua_pushstring(state,tree.getString().c_str());
                ::lua_setfield(state,adjIdx,buf);
                break;
            case VTree::Type::VTreeItself:
                {
                    auto& ref = tree.getInnerTree();
                    ::lua_createtable(state,SA::size(ref),0);
                    pushVTree(state,ref,-1);
                }
                break;
            case VTree::Type::VPackStrong:
                ::lua_pushstring(state,"[StrongPackPtr]");
                ::lua_setfield(state,adjIdx,buf);
                break;
            case VTree::Type::MessageableWeak:
                ::lua_pushstring(state,"[MesseagableWeak]");
                ::lua_setfield(state,adjIdx,buf);
                break;
            case VTree::Type::Double:
                ::lua_pushnumber(state,tree.getDouble());
                ::lua_setfield(state,adjIdx,buf);
                break;
            case VTree::Type::Int:
                ::lua_pushnumber(state,tree.getInt());
                ::lua_setfield(state,adjIdx,buf);
                break;
        }
        ++cnt;
    }
}

int rootPushGeneric(lua_State* state,const char* name) {
    VTree* treePtr = reinterpret_cast<VTree*>(
        ::lua_touserdata(state,-1));

    assert( treePtr->getType() == VTree::Type::VTreeItself
        && "Expected vtree that is tree." );

    auto& inner = treePtr->getInnerTree();

    char keybuf[32];

    auto& expectedTree = inner[1].getKey() == name ?
        inner[1] : inner[0];

    assert( expectedTree.getKey() == name && "Huh?" );
    assert( expectedTree.getType() == VTree::Type::VTreeItself
        && "Expected vtree..." );

    auto& innerValues = expectedTree.getInnerTree();

    ::lua_createtable(state,inner.size(),0);
    pushVTree(state,innerValues,-1);
    return 1;
}

// -1 -> VTree
int lua_getValTree(lua_State* state) {
    return rootPushGeneric(state,"values");
}

// -1 -> VTree
int lua_getTypeTree(lua_State* state) {
    return rootPushGeneric(state,"types");
}

void pushVTree(lua_State* state,VTree&& tree) {
    void* buf = ::lua_newuserdata(state,sizeof(VTree));
    new (buf) VTree(std::move(tree));
    ::luaL_setmetatable(state,"VTree");
}

// REMOVE
// -1 -> table
// -2 -> context
int lua_testVtree(lua_State* state) {
    WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(::lua_touserdata(state,-2));

    auto ctx = ctxW->lock();
    auto outTree = ctx->makeTreeFromTable(state,-1);
    sortVTree(*outTree);

    pushVTree(state,std::move(*outTree));
    return 1;
}

}

namespace StrongMesseagableBind {

int lua_freeStrongMesseagable(lua_State* state) {
    StrongMsgPtr* strongMsg =
        reinterpret_cast<StrongMsgPtr*>(
            ::lua_touserdata(state,-1));

    strongMsg->~shared_ptr();
    return 0;
}

// -1 -> table
// -2 -> messeagable
int lua_messageSync(lua_State* state) {

    return 0;
}

}

namespace LuaContextBind {

// -1 -> name
// -2 -> weak ctx
int lua_getMesseagableStrongRef(lua_State* state) {
    WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(
        ::lua_touserdata(state,-2));
    const char* name = reinterpret_cast<const char*>(
        ::lua_tostring(state,-1));

    auto ctx = ctxW->lock();
    assert( nullptr != ctx && "Context already dead?" );

    auto msg = ctx->getMesseagable(name);
    assert( nullptr != msg && "Messeagable doesn't exist." );

    void* buf = ::lua_newuserdata(state,sizeof(StrongMsgPtr));

    new (buf) StrongMsgPtr(msg);
    ::luaL_setmetatable(state,"StrongMesseagablePtr");
    return 1;
}

}

void registerVTree(lua_State* state) {
    ::luaL_newmetatable(state,"VTree");
    ::lua_pushcfunction(state,&VTreeBind::lua_freeVtree);
    ::lua_setfield(state,-2,"__gc");

    ::lua_createtable(state,4,0);
    // -1 table
    ::lua_pushcfunction(state,&VTreeBind::lua_getValTree);
    ::lua_setfield(state,-2,"values");

    ::lua_pushcfunction(state,&VTreeBind::lua_getTypeTree);
    ::lua_setfield(state,-2,"types");

    ::lua_setfield(state,-2,"__index");
    ::lua_pop(state,1);
}

void registerStrongMesseagable(lua_State* state) {
    ::luaL_newmetatable(state,"StrongMesseagablePtr");
    ::lua_pushcfunction(state,&StrongMesseagableBind::lua_freeStrongMesseagable);
    ::lua_setfield(state,-2,"__gc");

    ::lua_pop(state,1);
}

void initContext(const std::shared_ptr< LuaContext >& ctx) {
    auto s = ctx->s();
    void* adr = ::lua_newuserdata(s, sizeof(WeakCtxPtr) );
    new (adr) WeakCtxPtr(ctx);

    ::luaL_newmetatable(s,"WeakCtxPtr");
    ::lua_pushcfunction(s,&lua_freeWeakLuaContext);
    ::lua_setfield(s,-2,"__gc");

    ::lua_createtable(s,4,0);
    ::lua_pushcfunction(s,
        &LuaContextBind::lua_getMesseagableStrongRef);
    ::lua_setfield(s,-2,"namedMesseagable");
    ::lua_setfield(s,-2,"__index");

    ::lua_setmetatable(s,-2);
}

void initDomain(const std::shared_ptr< LuaContext >& ctx) {
    ctx->setFactory(&vFactory);

    auto s = ctx->s();
    luaL_openlibs(s);

    ctx->regFunction("nat_sendPack",&lua_sendPack);
    ctx->regFunction("nat_testVTree",&VTreeBind::lua_testVtree);

    bool success = luaL_dofile(s,"main.lua") == 0;
    if (!success) {
        printf("%s\n", lua_tostring(s, -1));
    }
    assert( success );

    registerVTree(s);

    initContext(ctx);

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
    double outValDouble;
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

        switch(::lua_type(state,VAL)) {
            case LUA_TNUMBER:
                outVal = ::lua_tostring(state,VAL);
                outVect.emplace_back(outKey.c_str(),outVal.c_str());
                break;
            case LUA_TSTRING:
                outVal = ::lua_tostring(state,VAL);
                outVect.emplace_back(outKey.c_str(),outVal.c_str());
                break;
            case LUA_TTABLE:
                outVect.emplace_back(outKey.c_str(),std::vector< VTree >());
                auto& treeRef = outVect.back().getInnerTree();
                getCharNodes(state,VAL,treeRef);
                break;
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

        auto& innerTypeNode = typeTree.getInnerTree();

        SM::traverse<true>(
            [&](int idx,
                VTree& type,
                VTree& value)
            {
                representAsPtr(type,value,
                    idx,types,values,d);
            },
            innerTypeNode,
            valueTree.getInnerTree()
        );

        int size = SA::size(innerTypeNode);
        auto p = _fact->makePack(size,types,values);
        SA::add(d._bufferVPtr,p);

        type[idx] = VPNAME;
        value[idx] = reinterpret_cast<const char*>(
            std::addressof(d._bufferVPtr.top())
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

int LuaContext::prepChildren(
    VTree& typeTree,
    VTree& valueTree,
    const char** types,const char** values,
    StackDump& d)
{
    auto& innerTypeTree = typeTree.getInnerTree();
    SM::traverse<true>(
        [&](int idx,
            VTree& type,
            VTree& val)
        {
            this->representAsPtr(
                type,val,
                idx,types,values,d);
        },
        innerTypeTree,
        valueTree.getInnerTree()
    );
    return innerTypeTree.size();
}
