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
        ATTACH_NAMED_DUMMY( bld, "mwnd_insetprog", MWI::InSetProgress );
        ATTACH_NAMED_DUMMY( bld, "mwnd_insetlabel", MWI::InSetStatusText );
        ATTACH_NAMED_DUMMY( bld, "mwnd_querylabel", MWI::QueryLabelText );
        ATTACH_NAMED_DUMMY( bld, "mwnd_inattachmsg", MWI::InAttachMesseagable );

        return bld.getFactory();
    }
}

static auto vFactory = buildTypeIndex();

struct ConstCharTreeNode {
    ConstCharTreeNode(const ConstCharTreeNode&) = delete;

    ConstCharTreeNode(ConstCharTreeNode&& other) :
        _key(other._key),
        _value(other._value),
        _children(std::move(other._children))
    {}

    ConstCharTreeNode(const char* key,const char* value) :
        _key(key), _value(value) {}

    ConstCharTreeNode& operator=(ConstCharTreeNode&& other)
    {
        _key = std::move(other._key);
        _value = std::move(other._value);
        _children = std::move(other._children);
        return *this;
    }

    bool isLeaf() const {
        return SA::size(_children) == 0;
    }

    bool isRoot() const {
        return _key == "[root]"
            && _value == "[root]"
            && SA::size(_children) == 2;
    }

    template <class T>
    VPackPtr toVPack(
        const templatious::DynVPackFactory* fact,
        T&& creator,
        LuaContext* ctx = nullptr) const
    {
        assert( isRoot() &&
            "Only root node can be converted to virtual pack.");

        auto& typeTree = _children[0]._key == "types" ?
            _children[0] : _children[1];

        auto& valueTree = _children[1]._key == "values" ?
            _children[1] : _children[0];

        assert( typeTree._key == "types" );
        assert( valueTree._key == "values" );

        // max 32 vpacks total in tree
        templatious::StaticBuffer< VPackPtr, 32 > bufVPtr;
        templatious::StaticBuffer< WeakMsgPtr, 32 > bufMsg;
        auto vecVp = bufVPtr.getStaticVector();
        auto vecMg = bufMsg.getStaticVector();

        // only one vpack will be made
        const char* types[32];
        const char* values[32];

        SM::traverse<true>(
            [&](int idx,
               const ConstCharTreeNode& type,
               const ConstCharTreeNode& val)
            {
                representAsPtr(fact,type,val,idx,types,values,vecVp,vecMg,ctx);
            },
            typeTree.children(),
            valueTree.children()
        );

        int size = SA::size(typeTree.children());

        return creator(size,types,values);
    }

    static ConstCharTreeNode packToTree(
        const templatious::DynVPackFactory* fact,
        templatious::VirtualPack& p)
    {
        ConstCharTreeNode result("[root]","[root]");
        result.push(ConstCharTreeNode("types",""));
        result.push(ConstCharTreeNode("values",""));
        auto& typeTree = result._children[0];
        auto& valueTree = result._children[1];

        packToTreeRec(typeTree,valueTree,p,fact);
        return result;
    }

    static std::unique_ptr< ConstCharTreeNode > packToTreeHeap(
        const templatious::DynVPackFactory* fact,
        templatious::VirtualPack& p)
    {
        std::unique_ptr< ConstCharTreeNode > result(
            new ConstCharTreeNode("[root]","[root]")
        );
        result->push(ConstCharTreeNode("types",""));
        result->push(ConstCharTreeNode("values",""));
        auto& typeTree = result->_children[0];
        auto& valueTree = result->_children[1];
        packToTreeRec(typeTree,valueTree,p,fact);
        return result;
    }

    void pushTypeTree(lua_State* state) {
        assert( isRoot() && "This can be used only on root node." );

        auto& typeTree = _children[0]._key == "types" ?
            _children[0] : _children[1];
        assert( typeTree._key == "types" );

        ::lua_createtable(state,SA::size(typeTree._children),0);
        typeTree.pushTree(state,-1);
    }

    void pushValueTree(lua_State* state) {
        assert( isRoot() && "This can be used only on root node." );

        auto& valueTree = _children[1]._key == "values" ?
            _children[1] : _children[0];
        assert( valueTree._key == "values" );

        ::lua_createtable(state,SA::size(valueTree._children),0);
        valueTree.pushTree(state,-1);
    }

    std::vector<ConstCharTreeNode>& children() {
        return _children;
    }

    const std::vector<ConstCharTreeNode>& children() const {
        return _children;
    }

    void push(ConstCharTreeNode&& child) {
        SA::add(_children,std::move(child));
    }

    void sort() {
        SM::sortS(_children,
            [](const ConstCharTreeNode& lhs,ConstCharTreeNode& rhs) {
                return lhs._key < rhs._key;
            }
        );
        TEMPLATIOUS_FOREACH(auto& i,_children) { i.sort(); }
    }

private:
    void pushTree(lua_State* state,int idx) const {
        TEMPLATIOUS_FOREACH(auto& i,_children) {
            ::lua_pushstring(state,i._key.c_str());
            if (i.isLeaf()) {
                ::lua_pushstring(state,i._value.c_str());
            } else {
                ::lua_createtable(state,SA::size(i._children),0);
                i.pushTree(state,-1);
            }
            // two items on top of index
            ::lua_settable(state,idx - 2);
        }
    }

    static void representAsPtr(
        const templatious::DynVPackFactory* fact,
        const ConstCharTreeNode& sisterTypeNode,
        const ConstCharTreeNode& sisterValueNode,
        int idx,const char** type,const char** value,
        templatious::StaticVector<VPackPtr>& bufferVPtr,
        templatious::StaticVector<WeakMsgPtr>& bufferWMsg,
        LuaContext* ctx)
    {
        static const char* VPNAME = "vpack";
        static const char* VMSGNAME = "vmsg";
        if (sisterValueNode.isLeaf()) {
            if (sisterTypeNode._value != "vmsg") {
                type[idx] = sisterTypeNode._value.c_str();
                value[idx] = sisterValueNode._value.c_str();
            } else {
                assert( nullptr != ctx &&
                    "To use messeagable types context must be provided." );

                auto target = ctx->getMesseagable(
                    sisterValueNode._value.c_str());

                assert( nullptr != target &&
                    "Messageable object doesn't exist in the context." );
                SA::add(bufferWMsg,target);

                type[idx] = sisterTypeNode._value.c_str();
                value[idx] = reinterpret_cast<const char*>(
                    std::addressof(bufferWMsg.top()));
            }
        } else {
            const char* types[32];
            const char* values[32];
            SM::traverse<true>(
                [&](int idx,
                    const ConstCharTreeNode& typeNode,
                    const ConstCharTreeNode& valNode)
                {
                    representAsPtr(
                        fact,typeNode,valNode,
                        idx,types,values,bufferVPtr,bufferWMsg,ctx);
                },
                sisterTypeNode.children(),
                sisterValueNode.children()
            );
            int size = SA::size(sisterTypeNode.children());
            auto p = fact->makePack(size,types,values);
            // extend lifetime
            SA::add(bufferVPtr,p);

            type[idx] = VPNAME;
            value[idx] = reinterpret_cast<const char*>(
                std::addressof(bufferVPtr.top()));
        }
    }

    static void packToTreeRec(
        ConstCharTreeNode& typeNode,
        ConstCharTreeNode& valueNode,
        templatious::VirtualPack& p,
        const templatious::DynVPackFactory* fact)
    {
        templatious::TNodePtr outInf[32];
        auto outVec = fact->serializePack(p,outInf);
        int outSize = SA::size(outVec);

        std::string keyBuf;
        TEMPLATIOUS_0_TO_N(i,outSize) {
            keyBuf = "_";
            keyBuf += std::to_string(i + 1);
            const char* assocName = fact->associatedName(outInf[i]);
            if (vpackNode != outInf[i]) {
                typeNode.push(ConstCharTreeNode(
                    keyBuf.c_str(),assocName));
                valueNode.push(ConstCharTreeNode(
                    keyBuf.c_str(),outVec[i].c_str()));
            } else {
                typeNode.push(ConstCharTreeNode(keyBuf.c_str(),""));
                valueNode.push(ConstCharTreeNode(keyBuf.c_str(),""));
                auto& tnodeRef = typeNode._children.back();
                auto& vnodeRef = valueNode._children.back();
                VPackPtr* vpptr = reinterpret_cast<VPackPtr*>(
                    ptrFromString(outVec[i]));
                packToTreeRec(tnodeRef,vnodeRef,**vpptr,fact);
            }
        }
    }

    std::string _key;
    std::string _value;
    std::vector<ConstCharTreeNode> _children;
};

struct LuaCallback : public Messageable {
    LuaCallback(
        const templatious::DynVPackFactory* fact,
        lua_State* state
    ) :
        _fact(fact),
        _state(state),
        _thisId(std::this_thread::get_id()),
        _currentPack(nullptr)
    {
        // we assume we find our function on the stack
        _callbackId = ::luaL_ref(state,LUA_REGISTRYINDEX);
    }

    ~LuaCallback() {
        ::luaL_unref(_state,LUA_REGISTRYINDEX,_callbackId);
    }

    void message(std::shared_ptr< templatious::VirtualPack > msg) override {
        Guard g(_mtx);
        SA::add(_queue,msg);
    }

    void message(templatious::VirtualPack& msg) override {
        assertThreadExecution();

        processSingleMessage(msg);
    }

    void processMessages() {
        assertThreadExecution();

        std::vector< VPackPtr > steal;
        {
            Guard g(_mtx);
            steal = std::move(_queue);
        }

        TEMPLATIOUS_FOREACH(auto& i,_queue) {
            _currentPack = i.get();
            processSingleMessage(*i);
            _currentPack = nullptr;
            _currentVTree = nullptr;
        }
    }

    void currentToValueTree() {
        preToTree();
        _currentVTree->pushValueTree(_state);
    }

    void currentToTypeTree() {
        preToTree();
        _currentVTree->pushTypeTree(_state);
    }
private:
    void preToTree() {
        assertThreadExecution();

        assert( nullptr != _currentPack && "Current pack cannot be null now." );

        if (nullptr == _currentVTree) {
            _currentVTree = ConstCharTreeNode::packToTreeHeap(_fact,*_currentPack);
        }
    }

    void processSingleMessage(templatious::VirtualPack& msg) {
        ::lua_rawgeti(_state,LUA_REGISTRYINDEX,_callbackId);
        ::lua_pushlightuserdata(_state,this);
        ::lua_pcall(_state,1,0,0);
    }

    void assertThreadExecution() const {
        assert( std::this_thread::get_id()
            == _thisId && "Thou shall not execute this "
            "method from any other thread than this object "
            "was created." );
    }

    typedef std::lock_guard< std::mutex > Guard;
    const templatious::DynVPackFactory* _fact;
    lua_State* _state;
    int _callbackId;
    std::thread::id _thisId;
    std::mutex _mtx;
    std::vector< VPackPtr > _queue;
    templatious::VirtualPack* _currentPack;
    std::unique_ptr< ConstCharTreeNode > _currentVTree;
};

// -1 -> weak context ptr
int freeWeakLuaContext(lua_State* state) {
    WeakCtxPtr* ctx = reinterpret_cast< WeakCtxPtr* >(
        ::lua_touserdata(state,-1));

    ctx->~weak_ptr();

    return 0;
}

// get char nodes recursively from lua table
void getCharNodes(lua_State* state,int tblidx,
    ConstCharTreeNode& outVect)
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
        ConstCharTreeNode node(outKey.c_str(),outVal.c_str());
        if (isTable) {
            getCharNodes(state,VAL,node);
        }
        outVect.push(std::move(node));

        ::lua_pop(state,1);
    }
}

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

// -1 -> value tree
// -2 -> mesg name
// -3 -> context
int sendPack(lua_State* state) {
    WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(::lua_touserdata(state,-3));
    const char* name = reinterpret_cast<const char*>(::lua_tostring(state,-2));

    auto ctx = ctxW->lock();
    assert( nullptr != ctx && "Context already dead?" );

    const int BACK_ARGS = 0;

    auto ptr = ctx->getMesseagable(name);
    if (nullptr == ptr) {
        return BACK_ARGS;
    }

    ConstCharTreeNode node("[root]","[root]");
    getCharNodes(state,-1,node);
    node.sort();

    auto fact = ctx->getFact();
    auto p = node.toVPack(fact,
        [=](int size,const char** types,const char** values) {
            return fact->makePack(size,types,values);
        },ctx.get());
    ptr->message(*p);

    return BACK_ARGS;
}

struct Unrefer {
    Unrefer(WeakCtxPtr ctx,int ref,int table)
        : _ctx(ctx), _ref(ref), _tbl(table) {}

    ~Unrefer() {
        auto ctx = _ctx.lock();
        assert( nullptr != ctx && "Context already dead?" );
        ::luaL_unref(ctx->s(),_tbl,_ref);
    }

    int func() const {
        return _ref;
    }

private:
    WeakCtxPtr _ctx;
    int _ref;
    int _tbl;
};

typedef std::shared_ptr< Unrefer > StrongUnreferPtr;

struct AsyncCallbackStruct {

    typedef std::weak_ptr< templatious::VirtualPack >
        WeakPackPtr;

    AsyncCallbackStruct(const AsyncCallbackStruct&) = delete;
    AsyncCallbackStruct(AsyncCallbackStruct&& other) :
        _alreadyFired(other._alreadyFired),
        _unrefer(other._unrefer),
        _outSelfPtr(other._outSelfPtr)
    {
        *_outSelfPtr = this;
    }

    AsyncCallbackStruct(
        WeakMsgPtr toFwd,
        WeakCtxPtr ctx,
        StrongUnreferPtr unref,
        AsyncCallbackStruct** outSelf
    ) : _alreadyFired(false),
        _unrefer(unref),
        _outSelfPtr(outSelf)
    {
        *_outSelfPtr = this;
    }

    template <class Any>
    void operator()(Any&& val) const {
        // thread safety ensure by locking at
        // pack level.
        assert( !_alreadyFired &&
            "Pack with this message may be used only once." );
        _alreadyFired = true;

        auto ctx = _ctx.lock();
        assert( nullptr != ctx && "Context already dead?" );

        auto l = _myself.lock();

    }

    void setMyself(WeakPackPtr myself) {
        _myself = myself;
    }

private:
    mutable bool _alreadyFired;
    // weak to prevent cycle on destruction
    WeakPackPtr _myself;
    StrongUnreferPtr _unrefer;
    WeakCtxPtr _ctx;

    AsyncCallbackStruct** _outSelfPtr;
};

struct AsyncCallbackMessage {

    AsyncCallbackMessage(
        const StrongPackPtr& pack,
        const StrongUnreferPtr& unref) :
        _packPtr(pack), _unrefer(unref)
    {}

    const StrongPackPtr& pack() const {
        return _packPtr;
    }

    int luaFunc() const {
        return _unrefer->func();
    }

private:
    StrongPackPtr _packPtr;
    StrongUnreferPtr _unrefer;
};

// -1 -> function
// -2 -> type tree
// -3 -> mesg name
// -4 -> context
int sendPackAsync(lua_State* state) {
    WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(::lua_touserdata(state,-4));
    const char* name = ::lua_tostring(state,-3);

    auto ctx = ctxW->lock();
    assert( nullptr != ctx && "Context already destroyed?.." );

    int funcRef = ::luaL_ref(state,LUA_REGISTRYINDEX);
    auto derefer = std::make_shared< Unrefer >(*ctxW,funcRef,LUA_REGISTRYINDEX);

    const int BACK_ARGS = 0;

    auto ptr = ctx->getMesseagable(name);
    if (nullptr == ptr) {
        return BACK_ARGS;
    }

    ConstCharTreeNode node("[root]","[root]");
    getCharNodes(state,-1,node);
    node.sort();

    // function popped
    auto fact = ctx->getFact();
    auto p = node.toVPack(fact,
        [=](int size,const char** types,const char** values) {
            const int FLAGS =
                templatious::VPACK_SYNCED;
            AsyncCallbackStruct* self = nullptr;
            auto p = fact->makePackCustomWCallback< FLAGS >(
                size,types,values,AsyncCallbackStruct(
                    ptr,ctx,derefer,&self)
            );
            self->setMyself(p);
            return p;
        },ctx.get());
    ptr->message(p);
    // do stuff

    return BACK_ARGS;
}

// -1 -> function
// -2 -> name
// -3 -> context
int registerLuaCallback(lua_State* state) {
    WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(::lua_touserdata(state,-3));
    const char* name = reinterpret_cast<const char*>(::lua_tostring(state,-2));

    auto ctx = ctxW->lock();
    assert( nullptr != ctx && "Context already dead?" );

    auto sCallback = std::make_shared< LuaCallback >(
        ctx->getFact(),
        state
    );

    ctx->addMesseagableStrong(name,sCallback);

    return 0;
}

// -1 -> lua callback object
int getValueTree(lua_State* state) {
    LuaCallback* cb = reinterpret_cast<LuaCallback*>(::lua_touserdata(state,-1));
    cb->currentToValueTree();
    return 1;
}

// -1 -> lua callback object
int getTypeTree(lua_State* state) {
    LuaCallback* cb = reinterpret_cast<LuaCallback*>(::lua_touserdata(state,-1));
    cb->currentToTypeTree();
    return 1;
}

void testBlock() {
    auto ptr = SF::vpackPtr<int,int>(3,4);
    auto p = SF::vpack<int,std::string,VPackPtr>(7,"7",ptr);
    auto outTree = ConstCharTreeNode::packToTree(&vFactory,p);

    volatile int stop = 0;
    ++stop;
}

void initDomain(std::shared_ptr< LuaContext > ctx) {
    testBlock();

    auto s = ctx->s();
    luaL_openlibs(s);
    ctx->regFunction("nat_sendPack",&sendPack);
    ctx->regFunction("nat_sendPackAsync",&sendPackAsync);
    ctx->regFunction("nat_registerCallback",&registerLuaCallback);
    ctx->regFunction("nat_getValueTree",&getValueTree);
    ctx->regFunction("nat_getTypeTree",&getTypeTree);
    ctx->setFactory(std::addressof(vFactory));

    bool success = luaL_dofile(s,"main.lua") == 0;
    if (!success) {
        printf("%s\n", lua_tostring(s, -1));
    }
    assert( success );

    void* adr = ::lua_newuserdata(s, sizeof(WeakCtxPtr) );
    new (adr) WeakCtxPtr(ctx);

    ::luaL_newmetatable(s,"WeakMsgPtr");
    ::lua_pushcfunction(s,&freeWeakLuaContext);
    ::lua_setfield(s,-2,"__gc");
    ::lua_setmetatable(s,-2);

    ::lua_getglobal(s,"initDomain");
    ::lua_pushvalue(s,-2);
    ::lua_pcall(s,1,0,0);
}

void LuaContext::processSingleMessage(const AsyncMsg& msg) {

}
