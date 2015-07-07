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

    auto vpackNode = TNF::makeFullNode<StrongPackPtr>(
        // here, we assume we receive pointer
        // to exact copy of the pack
        [](void* ptr,const char* arg) {
            new (ptr) StrongPackPtr(
                *reinterpret_cast<const StrongPackPtr*>(arg)
            );
        },
        [](void* ptr) {
            StrongPackPtr* vpPtr = reinterpret_cast<StrongPackPtr*>(ptr);
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
        bld.attachNode("vmsg_name",messeagableWeakNode);
        bld.attachNode("vmsg_raw",messeagableWeakNode);

        typedef MainWindowInterface MWI;
        typedef GenericMesseagableInterface GNI;
        ATTACH_NAMED_DUMMY( bld, "mwnd_insetprog", MWI::InSetProgress );
        ATTACH_NAMED_DUMMY( bld, "mwnd_insetlabel", MWI::InSetStatusText );
        ATTACH_NAMED_DUMMY( bld, "mwnd_querylabel", MWI::QueryLabelText );
        ATTACH_NAMED_DUMMY( bld, "mwnd_inattachmsg", MWI::InAttachMesseagable );
        ATTACH_NAMED_DUMMY( bld, "mwnd_outbtnclicked", MWI::OutButtonClicked );
        ATTACH_NAMED_DUMMY( bld, "gen_inattachitself", GNI::AttachItselfToMesseagable );
        ATTACH_NAMED_DUMMY( bld, "gen_inattachtoeventloop", GNI::InAttachToEventLoop );

        return bld.getFactory();
    }
}

static auto vFactory = buildTypeIndex();

namespace VTreeBind {
    void pushVTree(lua_State* state,VTree&& tree);
}

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

    ctx->assertThread();

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
// -2 -> strong messeagable
// -3 -> context
int lua_sendPackAsync(lua_State* state) {
    WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(::lua_touserdata(state,-3));
    StrongMsgPtr* msgPtr = reinterpret_cast<
        StrongMsgPtr*>(::lua_touserdata(state,-2));

    auto ctx = ctxW->lock();
    assert( nullptr != ctx && "Context already dead?" );

    ctx->assertThread();

    auto& msg = *msgPtr;
    assert( nullptr != msg && "Messeagable doesn't exist." );

    auto outTree = ctx->makeTreeFromTable(state,-1);
    sortVTree(*outTree);
    auto fact = ctx->getFact();
    auto p = ctx->treeToPack(*outTree,
        [=](int size,const char** types,const char** values) {
            return fact->makePack(size,types,values);
        });

    msg->message(p);

    return 0;
}

// -1 -> weak context ptr
int lua_freeWeakLuaContext(lua_State* state) {
    WeakCtxPtr* ctx = reinterpret_cast< WeakCtxPtr* >(
        ::lua_touserdata(state,-1));

    ctx->~weak_ptr();
    return 0;
}

// LUA INTERFACE:
// forwardST -> forward single threaded
// values -> value tree
// types -> type tree
// isST -> is single threaded, return true
// isMT -> is single threaded, return false
//
// ST -> stands for single threaded
struct VMessageST {
    VMessageST() = delete;
    VMessageST(const VMessageST&) = delete;
    VMessageST(VMessageST&&) = delete;

    static int lua_isST(lua_State* state) {
        ::lua_pushboolean(state,true);
        return 1;
    }

    static int lua_isMT(lua_State* state) {
        ::lua_pushboolean(state,false);
        return 1;
    }

    // -1 -> VMessageST
    static int lua_getValTree(lua_State* state) {
        VMessageST* cache = reinterpret_cast<VMessageST*>(
            ::lua_touserdata(state,-1));

        VTreeBind::pushVTree(state,cache->_ctx->packToTree(*cache->_pack));

        return 1;
    }

    static int lua_gc(lua_State* state) {
        VMessageST* cache = reinterpret_cast<VMessageST*>(
            ::lua_touserdata(state,-1));
        cache->~VMessageST();
        return 0;
    }

    // -1 -> messeagable
    // -2 -> cache
    static int lua_forwardST(lua_State* state) {
        VMessageST* cache = reinterpret_cast<VMessageST*>(
            ::lua_touserdata(state,-2));

        StrongMsgPtr* msg = reinterpret_cast<StrongMsgPtr*>(
            ::lua_touserdata(state,-1));

        (*msg)->message(*cache->_pack);

        return 0;
    }

    static int lua_forwardMT(lua_State* state) {
        assert( false && "Single threaded message cannot be sent as multithreaded." );
        return 0;
    }
private:
    friend struct LuaMessageHandler;

    VMessageST(
        templatious::VirtualPack* pack,
        LuaContext* ctx) :
        _pack(pack), _ctx(ctx) {}

    templatious::VirtualPack* _pack;
    LuaContext* _ctx;
};

// LUA INTERFACE:
// forwardST -> forward single threaded
// values -> value tree
// types -> type tree
// isST -> is single threaded, return true
// isMT -> is single threaded, return false
//
// MT -> stands for single threaded
struct VMessageMT {
    VMessageMT() = delete;
    VMessageMT(const VMessageMT&) = delete;
    VMessageMT(VMessageMT&&) = delete;

    static int lua_isST(lua_State* state) {
        ::lua_pushboolean(state,false);
        return 1;
    }

    static int lua_isMT(lua_State* state) {
        ::lua_pushboolean(state,true);
        return 1;
    }

    // -1 -> VMessageMT
    static int lua_getValTree(lua_State* state) {
        VMessageMT* cache = reinterpret_cast<VMessageMT*>(
            ::lua_touserdata(state,-1));

        VTreeBind::pushVTree(state,cache->_ctx->packToTree(*cache->_pack));

        return 1;
    }

    static int lua_gc(lua_State* state) {
        VMessageMT* cache = reinterpret_cast<VMessageMT*>(
            ::lua_touserdata(state,-1));
        cache->~VMessageMT();
        return 0;
    }

    // -1 -> messeagable
    // -2 -> cache
    static int lua_forwardST(lua_State* state) {
        VMessageMT* cache = reinterpret_cast<VMessageMT*>(
            ::lua_touserdata(state,-2));

        StrongMsgPtr* msg = reinterpret_cast<StrongMsgPtr*>(
            ::lua_touserdata(state,-1));

        (*msg)->message(*cache->_pack);

        return 0;
    }

    static int lua_forwardMT(lua_State* state) {
        VMessageMT* cache = reinterpret_cast<VMessageMT*>(
            ::lua_touserdata(state,-2));

        StrongMsgPtr* msg = reinterpret_cast<StrongMsgPtr*>(
            ::lua_touserdata(state,-1));

        (*msg)->message(cache->_pack);

        return 0;
    }
private:
    friend struct LuaMessageHandler;

    VMessageMT(
        const StrongPackPtr& pack,
        LuaContext* ctx) :
        _pack(pack), _ctx(ctx) {}

    StrongPackPtr _pack;
    LuaContext* _ctx;
};

struct LuaMessageHandler : public Messageable {

    LuaMessageHandler(const LuaMessageHandler&) = delete;
    LuaMessageHandler(LuaMessageHandler&&) = delete;

    LuaMessageHandler(const WeakCtxPtr& wptr,int table,int func) :
        _ctxW(wptr), _table(table), _funcRef(func), _hndl(genHandler()) {}

    ~LuaMessageHandler() {
        auto ctx = _ctxW.lock();

        assert( nullptr != ctx && "Context already dead?" );

        ::luaL_unref(ctx->s(),_table,_funcRef);
    }

    void message(StrongPackPtr sptr) override {
        _cache.enqueue(sptr);
    }

    void message(templatious::VirtualPack& pack) override {
        _g.assertThread();

        if (_hndl->tryMatch(pack)) {
            return;
        }

        auto locked = _ctxW.lock();

        auto s = locked->s();
        ::lua_rawgeti(s,_table,_funcRef);

        void* buf = ::lua_newuserdata(s,sizeof(VMessageST));
        new (buf) VMessageST(std::addressof(pack),locked.get());
        ::luaL_setmetatable(s,"VMessageST");

        ::lua_pcall(s,1,0,0);
    }

    // -1 -> callback
    // -2 -> context
    // returns weak messeagable
    static int lua_makeLuaHandler(lua_State* state) {
        WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(
            ::lua_touserdata(state,-2));

        auto locked = ctxW->lock();

        assert( nullptr != locked && "Context already dead?" );

        const int TABLE = LUA_REGISTRYINDEX;
        int func = ::luaL_ref(state,TABLE);

        void* buf = ::lua_newuserdata(state,sizeof(StrongMsgPtr));
        new (buf) StrongMsgPtr(
            new LuaMessageHandler(*ctxW,TABLE,func));
        ::luaL_setmetatable(state,"StrongMesseagablePtr");

        return 1;
    }

private:
    void processAsyncMessages() {
        _g.assertThread();

        auto locked = _ctxW.lock();
        auto s = locked->s();

        this->_cache.processPtr([=](const StrongPackPtr& pack) {
            ::lua_rawgeti(s,_table,_funcRef);
            void* buf = ::lua_newuserdata(s,sizeof(VMessageMT));
            new (buf) VMessageMT(pack,locked.get());
            ::luaL_setmetatable(s,"VMessageMT");

            ::lua_pcall(s,1,0,0);
        });
    }

    typedef std::unique_ptr< templatious::VirtualMatchFunctor > Handler;

    Handler genHandler() {
        typedef GenericMesseagableInterface GMI;
        return SF::virtualMatchFunctorPtr(
            SF::virtualMatch< GMI::AttachItselfToMesseagable, WeakMsgPtr >(
                [=](GMI::AttachItselfToMesseagable,const WeakMsgPtr& wmsg) {
                    auto locked = wmsg.lock();
                    assert( nullptr != locked && "Can't attach, dead." );

                    std::function<void()> func = [=]() {
                        this->processAsyncMessages();
                    };

                    auto p = SF::vpack<
                        GMI::InAttachToEventLoop,
                        std::function<void()>
                    >(GMI::InAttachToEventLoop(),std::move(func));

                    locked->message(p);
                }
            )
        );
    }

    WeakCtxPtr _ctxW;
    int _table;
    int _funcRef;

    ThreadGuard _g;
    MessageCache _cache;
    Handler _hndl;
};

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

    ctx->assertThread();

    auto inTree = ctx->makeTreeFromTable(state,-1);
    sortVTree(*inTree);

    auto fact = ctx->getFact();
    auto p = ctx->treeToPack(*inTree,
        [=](int size,const char** types,const char** values) {
            return fact->makePack(size,types,values);
        });

    msg->message(*p);
    auto outRes = ctx->packToTree(*p);

    ::lua_pushvalue(state,-2);
    VTreeBind::pushVTree(state,std::move(outRes));

    ::lua_pcall(state,1,0,0);

    return 0;
}

struct AsyncCallbackMessage {

    AsyncCallbackMessage(const AsyncCallbackMessage&) = delete;

    AsyncCallbackMessage(AsyncCallbackMessage&& other) :
        _tableRef(other._tableRef),
        _funcRef(other._funcRef),
        _pack(other._pack),
        _ctx(other._ctx)
    {
        other._tableRef = -1;
        other._funcRef = -1;
    }

    AsyncCallbackMessage(
        int tableRef,int funcRef,
        const StrongPackPtr& ptr,
        const WeakCtxPtr& ctx
    ) :
        _tableRef(tableRef),
        _funcRef(funcRef),
        _pack(ptr), _ctx(ctx) {}

    ~AsyncCallbackMessage() {
        auto locked = _ctx.lock();
        assert( nullptr != locked && "Context already dead?" );

        lua_State* s = locked->s();
        ::luaL_unref(s,_tableRef,_funcRef);
    }

    int tableRef() {
        return _tableRef;
    }

    int funcRef() {
        return _funcRef;
    }

    const StrongPackPtr& pack() {
        return _pack;
    }

private:
    int _tableRef;
    int _funcRef;
    StrongPackPtr _pack;
    WeakCtxPtr _ctx;
};

struct AsyncCallbackStruct {

    typedef std::weak_ptr< templatious::VirtualPack >
        WeakPackPtr;

    AsyncCallbackStruct(const AsyncCallbackStruct&) = delete;
    AsyncCallbackStruct(AsyncCallbackStruct&& other) :
        _alreadyFired(other._alreadyFired),
        _tableRef(other._tableRef),
        _funcRef(other._funcRef),
        _ctx(other._ctx),
        _outSelfPtr(other._outSelfPtr)
    {
        *_outSelfPtr = this;
        other._tableRef = -1;
        other._funcRef = -1;
    }

    AsyncCallbackStruct(
        WeakMsgPtr toFwd,
        WeakCtxPtr ctx,
        AsyncCallbackStruct** outSelf
    ) : _alreadyFired(false),
        _ctx(ctx),
        _outSelfPtr(outSelf)
    {
        *_outSelfPtr = this;
    }

    template <class Any>
    void operator()(Any&&) const {
        // thread safety ensure by locking at
        // pack level.
        assert( !_alreadyFired &&
            "Pack with this message may be used only once." );
        //_alreadyFired = true;

        //auto ctx = _ctx.lock();
        //assert( nullptr != ctx && "Context already dead?" );

        //auto l = _myself.lock();
        //auto toSend = std::make_shared< AsyncCallbackMessage >(
                //l, _unrefer, ctx->getFact());
        //ctx->enqueueCallback(toSend);
    }

    void setMyself(const WeakPackPtr& myself) {
        _myself = myself;
    }

private:
    mutable bool _alreadyFired;
    // weak to prevent cycle on destruction
    int _tableRef;
    int _funcRef;
    WeakPackPtr _myself;
    WeakCtxPtr _ctx;

    AsyncCallbackStruct** _outSelfPtr;
};

// -1 -> value tree
// -2 -> callback
// -3 -> strong messeagable
// -4 -> context
int lua_sendPackWCallbackAsync(lua_State* state) {
    WeakCtxPtr* ctxW = reinterpret_cast< WeakCtxPtr* >(
        ::lua_touserdata(state,-4));
    StrongMsgPtr* msgPtr = reinterpret_cast<
        StrongMsgPtr*>(::lua_touserdata(state,-3));

    auto ctx = ctxW->lock();
    assert( nullptr != ctx && "Context already dead?" );

    auto& msg = *msgPtr;
    assert( nullptr != msg && "Messeagable doesn't exist." );

    ctx->assertThread();
    auto inTree = ctx->makeTreeFromTable(state,-1);
    sortVTree(*inTree);

    auto fact = ctx->getFact();
    //auto p =

    return 0;
}

namespace StrongMesseagableBind {

int lua_freeStrongMesseagable(lua_State* state) {
    StrongMsgPtr* strongMsg =
        reinterpret_cast<StrongMsgPtr*>(
            ::lua_touserdata(state,-1));

    strongMsg->~shared_ptr();
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

void registerVMessageST(lua_State* state) {
    ::luaL_newmetatable(state,"VMessageST");
    ::lua_pushcfunction(state,&VMessageST::lua_gc);
    ::lua_setfield(state,-2,"__gc");

    ::lua_createtable(state,4,0);
    // -1 table
    ::lua_pushcfunction(state,&VMessageST::lua_isST);
    ::lua_setfield(state,-2,"isST");
    ::lua_pushcfunction(state,&VMessageST::lua_isMT);
    ::lua_setfield(state,-2,"isMT");
    ::lua_pushcfunction(state,&VMessageST::lua_getValTree);
    ::lua_setfield(state,-2,"vtree");
    ::lua_pushcfunction(state,&VMessageST::lua_forwardST);
    ::lua_setfield(state,-2,"forwardST");
    ::lua_pushcfunction(state,&VMessageST::lua_forwardMT);
    ::lua_setfield(state,-2,"forwardMT");

    ::lua_setfield(state,-2,"__index");
    ::lua_pop(state,1);
}

void registerVMessageMT(lua_State* state) {
    ::luaL_newmetatable(state,"VMessageMT");
    ::lua_pushcfunction(state,&VMessageMT::lua_gc);
    ::lua_setfield(state,-2,"__gc");

    ::lua_createtable(state,4,0);
    // -1 table
    ::lua_pushcfunction(state,&VMessageMT::lua_isST);
    ::lua_setfield(state,-2,"isST");
    ::lua_pushcfunction(state,&VMessageMT::lua_isMT);
    ::lua_setfield(state,-2,"isMT");
    ::lua_pushcfunction(state,&VMessageMT::lua_getValTree);
    ::lua_setfield(state,-2,"vtree");
    ::lua_pushcfunction(state,&VMessageMT::lua_forwardST);
    ::lua_setfield(state,-2,"forwardST");
    ::lua_pushcfunction(state,&VMessageMT::lua_forwardMT);
    ::lua_setfield(state,-2,"forwardMT");

    ::lua_setfield(state,-2,"__index");
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
    ::lua_pushcfunction(s,
        &LuaMessageHandler::lua_makeLuaHandler);
    ::lua_setfield(s,-2,"makeLuaHandler");
    ::lua_setfield(s,-2,"__index");

    ::lua_setmetatable(s,-2);
}

void initDomain(const std::shared_ptr< LuaContext >& ctx) {
    ctx->setFactory(&vFactory);

    auto s = ctx->s();
    luaL_openlibs(s);

    ctx->regFunction("nat_sendPack",&lua_sendPack);
    ctx->regFunction("nat_sendPackWCallback",&lua_sendPackWCallback);
    ctx->regFunction("nat_sendPackAsync",&lua_sendPackAsync);
    ctx->regFunction("nat_testVTree",&VTreeBind::lua_testVtree);

    bool success = luaL_dofile(s,"main.lua") == 0;
    if (!success) {
        printf("%s\n", lua_tostring(s, -1));
    }
    assert( success );

    registerVTree(s);
    registerVMessageST(s);
    registerVMessageMT(s);

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
                {
                outVect.emplace_back(outKey.c_str(),std::vector< VTree >());
                auto& treeRef = outVect.back().getInnerTree();
                getCharNodes(state,VAL,treeRef);
                }
                break;
            case LUA_TUSERDATA:
                {
                void* udata = ::lua_touserdata(state,VAL);
                outVect.emplace_back(outKey.c_str(),outVal.c_str());
                // write to already constructed string to prevent
                // segfault
                writePtrToString(udata,outVect.back().getString());
                }
                break;
            default:
                assert( false && "Didn't expect this bro." );
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
    static const char* VMSGNAME = "vmsg_name";
    static const char* VMSGRAW = "vmsg_raw";

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
    } else if (typeTree.getString() == VMSGRAW) {
        WeakMsgPtr* target = reinterpret_cast<WeakMsgPtr*>(
            ptrFromString(valueTree.getString()));
        SA::add(d._bufferWMsg,*target);
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

void LuaContext::packToTreeRec(
    VTree& typeNode,VTree& valueNode,
    const templatious::VirtualPack& pack,
    const templatious::DynVPackFactory* fact)
{
    templatious::TNodePtr outInf[32];
    auto outVec = fact->serializePack(pack,outInf);
    int outSize = SA::size(outVec);

    assert( typeNode.getType() == VTree::Type::VTreeItself &&
        "Typenode must contain VTree collection.");
    assert( valueNode.getType() == VTree::Type::VTreeItself &&
        "Typenode must contain VTree collection.");

    auto& tnVec = typeNode.getInnerTree();
    auto& vnVec = valueNode.getInnerTree();

    char keyBuf[16];
    TEMPLATIOUS_0_TO_N(i,outSize) {
        int tupleIndex = i + 1;
        ::sprintf(keyBuf,"_%d",tupleIndex);
        const char* assocName = fact->associatedName(outInf[i]);
        if (vpackNode != outInf[i]) {
            tnVec.emplace_back(keyBuf,assocName);
            vnVec.emplace_back(keyBuf,outVec[i].c_str());
        } else {
            std::vector< VTree > vecTypes;
            std::vector< VTree > vecValues;
            tnVec.emplace_back(keyBuf,std::move(vecTypes));
            vnVec.emplace_back(keyBuf,std::move(vecValues));

            auto& tnodeRef = tnVec.back();
            auto& vnodeRef = vnVec.back();
            StrongPackPtr* vpptr = reinterpret_cast<StrongPackPtr*>(
                ptrFromString(outVec[i]));
            packToTreeRec(tnodeRef,vnodeRef,**vpptr,fact);
        }
    }
}

auto LuaContext::genHandler() -> VmfPtr {
    typedef GenericMesseagableInterface GMI;
    return SF::virtualMatchFunctorPtr(
        SF::virtualMatch< GMI::AttachItselfToMesseagable, WeakMsgPtr >(
            [=](GMI::AttachItselfToMesseagable,const WeakMsgPtr& wmsg) {
                auto locked = wmsg.lock();
                assert( nullptr != locked && "Can't attach, dead." );

                std::function<void()> func = [=]() {
                    this->processMessages();
                };

                auto p = SF::vpack<
                    GMI::InAttachToEventLoop,
                    std::function<void()>
                >(GMI::InAttachToEventLoop(),std::move(func));

                locked->message(p);
            }
        ),
        SF::virtualMatch< GMI::InAttachToEventLoop, std::function<void()> >(
            [=](GMI::InAttachToEventLoop,std::function<void()>& func) {
                SA::add(this->_eventDriver,func);
            }
        )
    );
}

LuaContext::LuaContext() :
    _fact(nullptr),
    _s(luaL_newstate()),
    _msgHandler(genHandler())
{}
