// All the domain logic is here.
// Notice, that while not knowing what
// Qt is or including single header of
// Qt we're able to fully manipulate
// the GUI as long as we specify the messages
// and GUI side implements them.

#include <templatious/FullPack.hpp>
#include <templatious/detail/DynamicPackCreator.hpp>

#include "plumbing.hpp"

TEMPLATIOUS_TRIPLET_STD;

namespace {

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

}

struct LuaContextPrimitives {
    typedef templatious::TypeNodeFactory TNF;

    static const templatious::TypeNode* intNode() {
        static auto out = TNF::makePodNode<int>(
            [](void* ptr,const char* arg) {
                int* asInt = reinterpret_cast<int*>(ptr);
                const double* dlNum =
                    reinterpret_cast<const double*>(arg);
                new (ptr) int(*dlNum);
            },
            [](const void* ptr,std::string& out) {
                writePtrToString(ptr,out);
            }
        );
        return out;
    }

    static const templatious::TypeNode* doubleNode() {
        static auto out = TNF::makePodNode<double>(
            [](void* ptr,const char* arg) {
                double* asInt = reinterpret_cast<double*>(ptr);
                const double* dlNum =
                    reinterpret_cast<const double*>(arg);
                new (ptr) double(*dlNum);
            },
            [](const void* ptr,std::string& out) {
                writePtrToString(ptr,out);
            }
        );
        return out;
    }

    static const templatious::TypeNode* boolNode() {
        static auto out = TNF::makePodNode<bool>(
            [](void* ptr,const char* arg) {
                new (ptr) bool(arg[0] == 't');
            },
            [](const void* ptr,std::string& out) {
                const bool *res = reinterpret_cast<
                    const bool*>(ptr);
                out = (*res ? "t" : "f");
            }
        );
        return out;
    }

    static const templatious::TypeNode* stringNode() {
        static auto out = TNF::makeFullNode<std::string>(
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
        return out;
    }

    static const templatious::TypeNode* vpackNode() {
        static auto out = TNF::makeFullNode<StrongPackPtr>(
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
        return out;
    }

    static const templatious::TypeNode* messageableWeakNode() {
        static auto out = TNF::makeFullNode< WeakMsgPtr >(
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
        return out;
    }

    static const templatious::TypeNode* messageableStrongNode() {
        static auto out = TNF::makeFullNode< StrongMsgPtr >(
            [](void* ptr,const char* arg) {
                new (ptr) StrongMsgPtr(
                    *reinterpret_cast<const StrongMsgPtr*>(arg)
                );
            },
            [](void* ptr) {
                StrongMsgPtr* msPtr = reinterpret_cast<StrongMsgPtr*>(ptr);
                msPtr->~shared_ptr();
            },
            [](const void* ptr,std::string& out) {
                writePtrToString(ptr,out);
            }
        );
        return out;
    }
};

typedef std::vector< std::pair<
    bool, std::function<bool()>
> > EventDriver;

void CallbackCache::process() {
    if (SA::size(_eventDriver) == 0) {
        return;
    }

    int cnt = 0;
    EventDriver proc;
    std::swap(proc,_eventDriver);
    TEMPLATIOUS_FOREACH(auto& i,proc) {
        ++cnt;
        i.first = i.second();
    }

    SA::clear(
        SF::filter(
            proc,
            [](const std::pair< bool, std::function<bool()> >& res) {
                return !res.first;
            }
        )
    );

    if (SA::size(_eventDriver) == 0) {
        std::swap(proc,_eventDriver);
    } else {
        EventDriver another(std::move(_eventDriver));
        std::swap(proc,_eventDriver);
        TEMPLATIOUS_FOREACH(auto& i,another) {
            SA::add(_eventDriver,std::move(i));
        }
    }
}

void CallbackCache::attach(const std::function<bool()>& func) {
    SA::add(_eventDriver,
        std::pair<
            bool, std::function< bool() >
        >(true,func));
}

struct VTree {
    enum class Type {
        StdString,
        Int,
        Double,
        Boolean,
        VPackStrong,
        MessageableWeak,
        VTreeItself,
    };

    VTree() {
        _type = Type::Int;
        _int = 0;
    }

    VTree(const VTree&) = delete;
    VTree(VTree&& other) :
        _type(other._type),
        _key(std::move(other._key))
    {
        switch (other._type) {
            case Type::Double:
                _double = other._double;
                break;
            case Type::Int:
                _int = other._int;
                break;
            case Type::Boolean:
                _bool = other._bool;
                break;
            default:
                _ptr = other._ptr;
                other._ptr = nullptr;
                break;
        }
    }

    VTree& operator=(VTree&& other) {
        destructCurrent();
        _type = other._type;
        _key = std::move(other._key);
        switch (other._type) {
            case Type::Double:
                _double = other._double;
                break;
            case Type::Int:
                _int = other._int;
                break;
            case Type::Boolean:
                _bool = other._bool;
                break;
            default:
                _ptr = other._ptr;
                other._ptr = nullptr;
                break;
        }
        return *this;
    }

    VTree(const char* key,const char* ptr) :
        _type(Type::StdString),
        _key(key),
        _ptr(new std::string(ptr))
    {}

    VTree(const char* key,int val) :
        _type(Type::Int),
        _key(key),
        _int(val)
    {}

    VTree(const char* key,double val) :
        _type(Type::Double),
        _key(key),
        _double(val)
    {}

    VTree(const char* key,bool val) :
        _type(Type::Boolean),
        _key(key),
        _bool(val)
    {}

    VTree(const char* key,const StrongPackPtr& ptr) :
        _type(Type::VPackStrong),
        _key(key),
        _ptr(new StrongPackPtr(ptr))
    {}

    VTree(const char* key,const WeakMsgPtr& ptr) :
        _type(Type::MessageableWeak),
        _key(key),
        _ptr(new WeakMsgPtr(ptr))
    {}

    VTree(const char* key,std::vector<VTree>&& tree) :
        _type(Type::VTreeItself),
        _key(key),
        _ptr(new std::vector<VTree>(std::move(tree)))
    {}

    ~VTree()
    {
        destructCurrent();
    }

    Type getType() const { return _type; }

    const std::string& getString() const {
        assert( _type == Type::StdString && "Wrong type, dumbo." );
        return *reinterpret_cast< std::string* >(_ptr);
    }

    std::string& getString() {
        assert( _type == Type::StdString && "Wrong type, dumbo." );
        return *reinterpret_cast< std::string* >(_ptr);
    }

    StrongPackPtr& getStrongPack() const {
        assert( _type == Type::VPackStrong && "Wrong type, dumbo." );
        return *reinterpret_cast< StrongPackPtr* >(_ptr);
    }

    WeakMsgPtr& getWeakMsg() const {
        assert( _type == Type::MessageableWeak && "Wrong type, dumbo." );
        return *reinterpret_cast< WeakMsgPtr* >(_ptr);
    }

    std::vector< VTree >& getInnerTree() {
        assert( _type == Type::VTreeItself && "Wrong type, dumbo." );
        return *reinterpret_cast< std::vector< VTree >* >(_ptr);
    }

    int getInt() const {
        assert( _type == Type::Int && "Wrong type, dumbo." );
        return _int;
    }

    const double& getDouble() const {
        assert( _type == Type::Double && "Wrong type, dumbo." );
        return _double;
    }

    bool getBool() const {
        assert( _type == Type::Boolean && "Wrong type, dumbo." );
        return _bool;
    }

    const std::string& getKey() const {
        return _key;
    }

private:
    void destructCurrent() {
        switch (_type) {
            case Type::StdString:
                delete reinterpret_cast< std::string* >(_ptr);
                break;
            case Type::VPackStrong:
                delete reinterpret_cast< StrongPackPtr* >(_ptr);
                break;
            case Type::MessageableWeak:
                delete reinterpret_cast< WeakMsgPtr* >(_ptr);
                break;
            case Type::VTreeItself:
                delete reinterpret_cast< std::vector<VTree>* >(_ptr);
                break;
            case Type::Int:
            case Type::Double:
            case Type::Boolean:
                break;
            default:
                assert( false && "HUH?" );
                break;
        }
    }

    Type _type;
    std::string _key;
    union {
        void* _ptr;
        int _int;
        double _double;
        bool _bool;
    };
};

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

// -1 -> weak context ptr
int luanat_freeWeakLuaContext(lua_State* state) {
    WeakCtxPtr* ctx = reinterpret_cast< WeakCtxPtr* >(
        ::lua_touserdata(state,-1));

    ctx->~weak_ptr();
    return 0;
}

void handleLuaError(int result,lua_State* state) {
    if (LUA_ERRRUN == result) {
        const char* errMsg = ::lua_tostring(state,-1);
        printf("LuaContext runtime error: %s\n",errMsg);
    }
}

struct ContextMessageable : public Messageable {

    ContextMessageable(const std::weak_ptr< LuaContext >& ctx);

    void message(const StrongPackPtr& pack);

    void message(templatious::VirtualPack& p) {
        assertThread();
        _handler->tryMatch(p);
    }

    friend struct LuaContextImpl;

    void assertThread() {
        _g.assertThread();
    }

private:
    //void notifyDependency();

    typedef std::unique_ptr< templatious::VirtualMatchFunctor > VmfPtr;

    VmfPtr genHandler();

    std::weak_ptr< LuaContext > _wCtx;
    std::weak_ptr< ContextMessageable > _wMsg;
    VmfPtr _handler;
    MessageCache _cache;
    ThreadGuard _g;
    long _lastUpdate;
};

static bool setPackValue(int stackPtr,int slot,
        lua_State* state,templatious::VirtualPack& pack)
{
    int type = ::lua_type(state,stackPtr);
    assert( type == LUA_TTABLE && "Expected to be called with table, slick." );

    int trueIdx = stackPtr - 1;
    ::lua_pushnil(state);
    int res = ::lua_next(state,trueIdx);
    assert( 0 != res && "I would hope this to succeed, milky..." );

    const int KEY = -2;
    const int VAL = -1;

    int keyType = ::lua_type(state,KEY);
    assert( LUA_TSTRING == keyType && "Key should be string..." );
    std::string keyVal = ::lua_tostring(state,KEY);
    int valueType = ::lua_type(state,VAL);
    bool success = false;
    if ("int" == keyVal) {
        assert( LUA_TNUMBER == valueType && "int passed but not a number?" );
        lua_Number number = ::lua_tonumber(state,VAL);
        long rounded = std::lround(number);
        assert( rounded <= std::numeric_limits<int>::max()
            && rounded >= std::numeric_limits<int>::min()
            && "Integer overflow from lua value." );
        success =
            pack.callSingle< int >(
                slot,
                [&](int& toChange) {
                    toChange = static_cast<int>(rounded);
                }
            );
    } else if ("double" == keyVal) {
        assert( LUA_TNUMBER == valueType && "double passed but not a number?" );
        lua_Number number = ::lua_tonumber(state,VAL);
        success =
            pack.callSingle< double >(
                slot,
                [&](double& toChange) {
                    toChange = number;
                }
            );
    } else if ("bool" == keyVal) {
        assert( LUA_TBOOLEAN == valueType && "boolean passed but not lua bool?" );
        int val = ::lua_toboolean(state,VAL);
        // graceful... like a grandma
        bool bval = val > 0 ? true : false;
        success =
            pack.callSingle< bool >(
                slot,
                [&](bool& toChange) {
                    toChange = bval;
                }
            );
    } else if ("string" == keyVal) {
        assert( LUA_TSTRING == valueType && "string passed but not lua string?" );
        const char* val = ::lua_tostring(state,VAL);
        success =
            pack.callSingle< std::string >(
                slot,
                [&](std::string& toChange) {
                    toChange = val;
                }
            );
    } else if ("vmsg_raw_strong" == keyVal) {
        assert( LUA_TUSERDATA == valueType && "userdata passed but not lua userdata?" );
        void* udata = ::lua_touserdata(state,VAL);
        auto ptr = reinterpret_cast< StrongMsgPtr* >(udata);
        success =
            pack.callSingle< StrongMsgPtr >(
                slot,
                [&](StrongMsgPtr& toChange) {
                    toChange = *ptr;
                }
            );
    }

    ::lua_pop(state,1);
    res = ::lua_next(state,trueIdx);
    assert( 0 == res && "Table should have only one element..." );

    return success;
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

    static int luanat_isST(lua_State* state) {
        ::lua_pushboolean(state,true);
        return 1;
    }

    static int luanat_isMT(lua_State* state) {
        ::lua_pushboolean(state,false);
        return 1;
    }

    // -1 -> VMessageST
    static int luanat_getValTree(lua_State* state);

    static int luanat_gc(lua_State* state) {
        VMessageST* cache = reinterpret_cast<VMessageST*>(
            ::lua_touserdata(state,-1));
        cache->~VMessageST();
        return 0;
    }

    // -1 -> messageable
    // -2 -> cache
    static int luanat_forwardST(lua_State* state) {
        VMessageST* cache = reinterpret_cast<VMessageST*>(
            ::lua_touserdata(state,-2));

        StrongMsgPtr* msg = reinterpret_cast<StrongMsgPtr*>(
            ::lua_touserdata(state,-1));

        (*msg)->message(*cache->_pack);

        return 0;
    }

    static int luanat_forwardMT(lua_State* state) {
        assert( false && "Single threaded message cannot be sent as multithreaded." );
        return 0;
    }

    // -3 -> cache
    // -2 -> slot
    // -1 -> table
    static int luanat_setValueST(lua_State* state) {
        VMessageST* cache = reinterpret_cast<VMessageST*>(
            ::lua_touserdata(state,-3));

        auto slot = ::lua_tonumber(state,-2);
        long rounded = std::lround( slot );
        assert( rounded >= 1 && rounded < 33
            && "Min slot is 1 and maximum is 32." );

        // safe, like grandma
        int irounded = static_cast<int>(rounded) - 1;

        bool result = setPackValue(-1,irounded,state,*cache->_pack);
        ::lua_pushboolean(state,result);
        return 1;
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

    static int luanat_isST(lua_State* state) {
        ::lua_pushboolean(state,false);
        return 1;
    }

    static int luanat_isMT(lua_State* state) {
        ::lua_pushboolean(state,true);
        return 1;
    }

    // -1 -> VMessageMT
    static int luanat_getValTree(lua_State* state);

    static int luanat_gc(lua_State* state) {
        VMessageMT* cache = reinterpret_cast<VMessageMT*>(
            ::lua_touserdata(state,-1));
        cache->~VMessageMT();
        return 0;
    }

    // -1 -> messageable
    // -2 -> cache
    static int luanat_forwardST(lua_State* state) {
        VMessageMT* cache = reinterpret_cast<VMessageMT*>(
            ::lua_touserdata(state,-2));

        StrongMsgPtr* msg = reinterpret_cast<StrongMsgPtr*>(
            ::lua_touserdata(state,-1));

        (*msg)->message(*cache->_pack);

        return 0;
    }

    // -1 -> messageable
    // -2 -> cache
    static int luanat_forwardMT(lua_State* state) {
        VMessageMT* cache = reinterpret_cast<VMessageMT*>(
            ::lua_touserdata(state,-2));

        StrongMsgPtr* msg = reinterpret_cast<StrongMsgPtr*>(
            ::lua_touserdata(state,-1));

        (*msg)->message(cache->_pack);

        return 0;
    }

    // -3 -> cache
    // -2 -> slot
    // -1 -> table
    static int luanat_setValueMT(lua_State* state) {
        VMessageMT* cache = reinterpret_cast<VMessageMT*>(
            ::lua_touserdata(state,-3));

        auto slot = ::lua_tonumber(state,-2);
        long rounded = std::lround( slot );
        assert( rounded >= 1 && rounded < 33
            && "Min slot is 1 and maximum is 32." );

        // safe, like grandma
        int irounded = static_cast<int>(rounded) - 1;

        bool result = setPackValue(-1,irounded,state,*cache->_pack);
        ::lua_pushboolean(state,result);
        return 1;
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

    LuaMessageHandler(const WeakCtxPtr& wptr,int table,int func);

    ~LuaMessageHandler() override {
        auto ctx = _ctxW.lock();

        // context may be dead after it is destroyed
        // and this destructor is called from lua
        // garbage collector...

        //assert( nullptr != ctx && "Context already dead?" );

        if (nullptr != ctx) {
            ::luaL_unref(ctx->s(),_table,_funcRef);
        }
    }

    void message(const StrongPackPtr& sptr) override;

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

        handleLuaError(::lua_pcall(s,1,0,0),s);
    }

    // -1 -> callback
    // -2 -> context
    // returns weak messageable
    static int luanat_makeLuaHandler(lua_State* state) {
        WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(
            ::lua_touserdata(state,-2));

        auto locked = ctxW->lock();

        assert( nullptr != locked && "Context already dead?" );

        const int TABLE = LUA_REGISTRYINDEX;
        int func = ::luaL_ref(state,TABLE);

        void* buf = ::lua_newuserdata(state,sizeof(std::shared_ptr< Messageable >));
        auto ptr = std::shared_ptr< LuaMessageHandler >(new LuaMessageHandler(*ctxW,TABLE,func));
        auto sPtr = new (buf) std::shared_ptr< Messageable >( ptr );
        ptr->_selfW = ptr;
        ::luaL_setmetatable(state,"StrongMessageablePtr");

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

            handleLuaError(::lua_pcall(s,1,0,0),s);
        });
    }

    //void notifyDependency();

    typedef std::unique_ptr< templatious::VirtualMatchFunctor > Handler;

    Handler genHandler() {
        typedef GenericMessageableInterface GMI;
        return SF::virtualMatchFunctorPtr(
            SF::virtualMatch< GMI::AttachItselfToMessageable, StrongMsgPtr >(
                [=](GMI::AttachItselfToMessageable,const StrongMsgPtr& wmsg) {
                    assert( nullptr != wmsg && "Can't attach, dead." );

                    auto weakCpy = _selfW;
                    std::function<bool()> func = [=]() {
                        auto locked = weakCpy.lock();
                        if (nullptr == locked) {
                            return false;
                        }

                        locked->processAsyncMessages();
                        return true;
                    };

                    auto p = SF::vpack<
                        GMI::InAttachToEventLoop,
                        std::function<bool()>
                    >(GMI::InAttachToEventLoop(),std::move(func));

                    wmsg->message(p);
                }
            )
        );
    }

    WeakCtxPtr _ctxW;
    std::weak_ptr< LuaMessageHandler > _selfW;
    int _table;
    int _funcRef;

    ThreadGuard _g;
    MessageCache _cache;
    Handler _hndl;

    long _lastUpdate;
};


struct LuaContextImpl {

    struct AsyncCallbackStruct {

        typedef std::weak_ptr< templatious::VirtualPack >
            WeakPackPtr;

        AsyncCallbackStruct(const AsyncCallbackStruct&) = delete;
        AsyncCallbackStruct(AsyncCallbackStruct&& other) :
            _alreadyFired(other._alreadyFired),
            _callbackExists(other._callbackExists), // for correct callback
            _failExists(other._failExists), // for error callback
            _tableRef(other._tableRef), // for correct callback
            _funcRef(other._funcRef), // for correct callback
            _tableRefFail(other._tableRefFail), // for error callback
            _funcRefFail(other._funcRefFail), // for error callback
            _ctx(other._ctx),
            _outSelfPtr(other._outSelfPtr)
        {
            *_outSelfPtr = this;
            other._alreadyFired = true;
            other._callbackExists = false;
            other._failExists = false;
            other._tableRef = -1;
            other._funcRef = -1;
            other._tableRefFail = -1;
            other._funcRefFail = -1;
        }

        AsyncCallbackStruct(
            int tableRef,
            int funcRef,
            WeakCtxPtr ctx,
            AsyncCallbackStruct** outSelf
        ) : _alreadyFired(false),
            _callbackExists(true),
            _failExists(false),
            _tableRef(tableRef),
            _funcRef(funcRef),
            _tableRefFail(-1),
            _funcRefFail(-1),
            _ctx(ctx),
            _outSelfPtr(outSelf)
        {
            *_outSelfPtr = this;
        }

        AsyncCallbackStruct(
            bool failExists,
            int tableRef,
            int funcRef,
            WeakCtxPtr ctx,
            AsyncCallbackStruct** outSelf
        ) : _alreadyFired(false),
            _callbackExists(false),
            _failExists(true),
            _tableRef(-1),
            _funcRef(-1),
            _tableRefFail(tableRef),
            _funcRefFail(funcRef),
            _ctx(ctx),
            _outSelfPtr(outSelf)
        {
            *_outSelfPtr = this;
        }

        AsyncCallbackStruct(
            int tableRef,
            int funcRef,
            int tableRefFail,
            int funcRefFail,
            WeakCtxPtr ctx,
            AsyncCallbackStruct** outSelf
        ) : _alreadyFired(false),
            _callbackExists(true),
            _failExists(true),
            _tableRef(tableRef),
            _funcRef(funcRef),
            _tableRefFail(tableRefFail),
            _funcRefFail(funcRefFail),
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
            _alreadyFired = true;

            auto ctx = _ctx.lock();
            assert( nullptr != ctx && "Context already dead?" );

            auto l = _myself.lock();
            if (_callbackExists) {
                enqueueCallback(*ctx,_tableRef,_funcRef,true,l,_ctx);
            }
            if (_failExists) {
                // enqueue for destruction at home thread
                enqueueCallback(*ctx,_tableRefFail,_funcRefFail,false,l,ctx);
            }
        }

        ~AsyncCallbackStruct() {
            if (!_alreadyFired) {
                auto ctx = _ctx.lock();
                assert( nullptr != ctx && "Context already dead?" );
                auto l = _myself.lock();
                if (_callbackExists) {
                    enqueueCallback(*ctx,_tableRef,_funcRef,false,l,_ctx);
                }
                if (_failExists) {
                    enqueueCallback(*ctx,_tableRefFail,_funcRefFail,true,l,ctx);
                }
            }
        }

        void setMyself(const WeakPackPtr& myself) {
            _myself = myself;
        }

    private:
        mutable bool _alreadyFired;
        // weak to prevent cycle on destruction
        bool _callbackExists;
        bool _failExists;
        int _tableRef;
        int _funcRef;
        int _tableRefFail;
        int _funcRefFail;
        WeakPackPtr _myself;
        WeakCtxPtr _ctx;

        AsyncCallbackStruct** _outSelfPtr;
    };

    struct StackDump {
        StackDump(
            templatious::StaticVector< StrongPackPtr >& bufVPtr,
            templatious::StaticVector< WeakMsgPtr >& bufWMsg,
            templatious::StaticVector< StrongMsgPtr >& bufSMsg
        ) :
            _bufferVPtr(bufVPtr), _bufferWMsg(bufWMsg),
            _bufferSMsg(bufSMsg)
        {}

        StackDump(const StackDump&) = delete;
        StackDump(StackDump&&) = delete;

        templatious::StaticVector< StrongPackPtr >& _bufferVPtr;
        templatious::StaticVector< WeakMsgPtr >& _bufferWMsg;
        templatious::StaticVector< StrongMsgPtr >& _bufferSMsg;
    };

    static void getCharNodes(lua_State* state,int tblidx,
        std::vector< VTree >& outVect)
    {
        int iter = 0;

        const int KEY = -2;
        const int VAL = -1;

        std::string outKey,outVal;
        double outValDouble;
        ::lua_pushnil(state);
        int trueIdx = tblidx - 1;
        char buf[16];
        while (0 != ::lua_next(state,trueIdx)) {
            switch (::lua_type(state,KEY)) {
                case LUA_TSTRING:
                    outKey = ::lua_tostring(state,KEY);
                    break;
                case LUA_TNUMBER:
                    double num = ::lua_tonumber(state,KEY);
                    sprintf(buf,"_%f",num);
                    outKey = buf;
                    break;
            }

            switch(::lua_type(state,VAL)) {
                case LUA_TNUMBER:
                    {
                    double outDouble = ::lua_tonumber(state,VAL);
                    outVect.emplace_back(outKey.c_str(),outDouble);
                    }
                    break;
                case LUA_TSTRING:
                    outVal = ::lua_tostring(state,VAL);
                    outVect.emplace_back(outKey.c_str(),outVal.c_str());
                    break;
                case LUA_TBOOLEAN:
                    {
                    bool outBool = ::lua_toboolean(state,VAL);
                    outVect.emplace_back(outKey.c_str(),outBool);
                    }
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

    static std::unique_ptr< VTree >
    makeTreeFromTable(LuaContext& ctx,lua_State* state,int idx) {
        ctx.assertThread();

        std::vector< VTree > nodes;

        getCharNodes(state,idx,nodes);

        return std::unique_ptr< VTree >(new VTree("[root]",std::move(nodes)));
    }

    static int prepChildren(
        LuaContext& ctx,
        VTree& typeTree,
        VTree& valueTree,
        const char** types,
        const char** values,
        StackDump& d)
    {
        auto& innerTypeTree = typeTree.getInnerTree();
        SM::traverse<true>(
            [&](int idx,
                VTree& type,
                VTree& val)
            {
                representAsPtr(
                    ctx,
                    type,val,
                    idx,types,values,d);
            },
            innerTypeTree,
            valueTree.getInnerTree()
        );
        return innerTypeTree.size();
    }

    static void representAsPtr(
        LuaContext& ctx,
        VTree& typeTree,VTree& valueTree,
        int idx,const char** type,const char** value,
        StackDump& d)
    {
        static const char* VPNAME = "vpack";
        static const char* VMSGNAME = "vmsg_name";
        static const char* VMSGRAW_WEAK = "vmsg_raw_weak";
        static const char* VMSGRAW_STRONG = "vmsg_raw_strong";
        static const char* VMSGINT = "int";
        static const char* VMSGDOUBLE = "double";
        static const char* VMSGBOOL = "bool";

        if (typeTree.getType() == VTree::Type::VTreeItself) {
            const char* types[32];
            const char* values[32];

            auto& innerTypeNode = typeTree.getInnerTree();

            SM::traverse<true>(
                [&](int idx,
                    VTree& type,
                    VTree& value)
                {
                    representAsPtr(ctx,type,value,
                        idx,types,values,d);
                },
                innerTypeNode,
                valueTree.getInnerTree()
            );

            int size = SA::size(innerTypeNode);
            auto p = ctx._fact->makePack(size,types,values);
            SA::add(d._bufferVPtr,p);

            type[idx] = VPNAME;
            value[idx] = reinterpret_cast<const char*>(
                std::addressof(d._bufferVPtr.top())
            );
        } else if (typeTree.getString() == VMSGNAME) {
            auto target = ctx.getMessageable(valueTree.getString().c_str());

            assert( nullptr != target
                && "Messageable object doesn't exist in the context." );

            SA::add(d._bufferWMsg,target);
            type[idx] = typeTree.getString().c_str();
            value[idx] = reinterpret_cast<const char*>(
                std::addressof(d._bufferWMsg.top()));
        } else if (typeTree.getString() == VMSGRAW_STRONG) {
            StrongMsgPtr* target = reinterpret_cast<StrongMsgPtr*>(
                ptrFromString(valueTree.getString()));
            SA::add(d._bufferSMsg,*target);
            type[idx] = typeTree.getString().c_str();
            value[idx] = reinterpret_cast<const char*>(
                std::addressof(d._bufferSMsg.top()));
        } else if (typeTree.getString() == VMSGRAW_WEAK) {
            WeakMsgPtr* target = reinterpret_cast<WeakMsgPtr*>(
                ptrFromString(valueTree.getString()));
            SA::add(d._bufferWMsg,*target);
            type[idx] = typeTree.getString().c_str();
            value[idx] = reinterpret_cast<const char*>(
                std::addressof(d._bufferWMsg.top()));
        } else if (typeTree.getString() == VMSGINT) {
            type[idx] = typeTree.getString().c_str();
            value[idx] = reinterpret_cast<const char*>(
                &valueTree.getDouble());
        } else if (typeTree.getString() == VMSGDOUBLE) {
            type[idx] = typeTree.getString().c_str();
            value[idx] = reinterpret_cast<const char*>(
                &valueTree.getDouble());
        } else if (typeTree.getString() == VMSGBOOL) {
            type[idx] = typeTree.getString().c_str();
            value[idx] = valueTree.getBool() ? "t" : "f";
        } else {
            assert( valueTree.getType() == VTree::Type::StdString
                && "Only string is expected now..." );
            type[idx] = typeTree.getString().c_str();
            value[idx] = valueTree.getString().c_str();
        }
    }

    template <class T>
    static StrongPackPtr toVPack(
        LuaContext& ctx,
        VTree& tree,T&& creator,
        StackDump& d)
    {
        assert( tree.getType() == VTree::Type::VTreeItself
            && "Expecting tree here, milky..." );

        auto& children = tree.getInnerTree();

        const char* types[32];
        const char* values[32];

        auto& typeTree = children[0].getKey() == "types" ?
            children[0] : children[1];

        auto& valueTree = children[1].getKey() == "values" ?
            children[1] : children[0];

        assert( typeTree.getKey() == "types" );
        assert( valueTree.getKey() == "values" );

        int size = prepChildren(ctx,typeTree,valueTree,types,values,d);

        return creator(size,types,values);
    }

    template <class Maker>
    static StrongPackPtr treeToPack(LuaContext& ctx,VTree& tree,Maker&& m) {
        ctx.assertThread();

        templatious::StaticBuffer< StrongPackPtr, 32 > bufPack;
        templatious::StaticBuffer< WeakMsgPtr, 32 > msgWeakPack;
        templatious::StaticBuffer< StrongMsgPtr, 32 > msgStrongPack;

        auto vPack = bufPack.getStaticVector();
        auto vWMsg = msgWeakPack.getStaticVector();
        auto vSMsg = msgStrongPack.getStaticVector();

        StackDump d(vPack,vWMsg,vSMsg);

        return toVPack(ctx,tree,std::forward<Maker>(m),d);
    }

    struct CallbackResultWriter {
        CallbackResultWriter(bool* ptr) : _outRes(ptr) {}

        template <class... Args>
        void operator()(Args&&...) const {
            *_outRes = true;
        }
    private:
        bool* _outRes;
    };

    // -1 -> strong messageable A
    // -2 -> strong messageable B
    static int luanat_areMessageablesEqual(lua_State* state) {
        StrongMsgPtr* msgPtrB = reinterpret_cast<
            StrongMsgPtr*>(::lua_touserdata(state,-1));
        StrongMsgPtr* msgPtrA = reinterpret_cast<
            StrongMsgPtr*>(::lua_touserdata(state,-2));

        assert( ::lua_type(state,-1) == LUA_TUSERDATA
            && "Wrong cholo!" );
        assert( ::lua_type(state,-2) == LUA_TUSERDATA
            && "Wrong cholo!" );

        if (*msgPtrA != *msgPtrB) {
            ::lua_pushboolean(state,false);
        } else {
            ::lua_pushboolean(state,true);
        }

        return 1;
    }

    // -1 -> value tree
    // -2 -> callback
    // -3 -> strong messageable
    // -4 -> context
    static int luanat_sendPackWCallback(lua_State* state) {
        WeakCtxPtr* ctxW = reinterpret_cast< WeakCtxPtr* >(
            ::lua_touserdata(state,-4));
        StrongMsgPtr* msgPtr = reinterpret_cast<
            StrongMsgPtr*>(::lua_touserdata(state,-3));

        auto ctx = ctxW->lock();
        assert( nullptr != ctx && "Context already dead?" );

        auto& msg = *msgPtr;
        assert( nullptr != msg && "Messageable doesn't exist." );

        ctx->assertThread();

        auto inTree = makeTreeFromTable(*ctx,state,-1);
        sortVTree(*inTree);

        bool outBool = false;
        bool *resPtr = &outBool;

        auto fact = ctx->getFact();
        auto p = treeToPack(*ctx,*inTree,
            [=](int size,const char** types,const char** values) {
                return fact->makePackWCallback(size,types,values,
                        CallbackResultWriter(resPtr));
            });

        msg->message(*p);
        auto outRes = LuaContextImpl::packToTree(*ctx,*p);

        ::lua_pushvalue(state,-2);
        VTreeBind::pushVTree(state,std::move(outRes));

        handleLuaError(::lua_pcall(state,1,0,0),state);

        ::lua_pushboolean(state,outBool);

        return 1;
    }

    // -1 -> value tree
    // -2 -> error callback, can be nil
    // -3 -> callback
    // -4 -> strong messageable
    // -5 -> context
    static int luanat_sendPackAsyncWCallback(lua_State* state) {
        WeakCtxPtr* ctxW = reinterpret_cast< WeakCtxPtr* >(
            ::lua_touserdata(state,-5));
        StrongMsgPtr* msgPtr = reinterpret_cast<
            StrongMsgPtr*>(::lua_touserdata(state,-4));

        auto ctx = ctxW->lock();
        assert( nullptr != ctx && "Context already dead?" );

        auto& msg = *msgPtr;
        assert( nullptr != msg && "Messageable doesn't exist." );

        const int TABLE_IDX = LUA_REGISTRYINDEX;
        ::lua_pushvalue(state,-3);
        int funcRef = ::luaL_ref(state,TABLE_IDX);

        bool hasErrorHndl = LUA_TNIL != ::lua_type(state,-2);
        int funcRefFail = -1;
        if (hasErrorHndl) {
            ::lua_pushvalue(state,-2);
            funcRefFail = ::luaL_ref(state,TABLE_IDX);
        }

        ctx->assertThread();
        auto inTree = makeTreeFromTable(*ctx,state,-1);
        sortVTree(*inTree);

        auto fact = ctx->getFact();
        auto p = treeToPack(*ctx,*inTree,
            [=](int size,const char** types,const char** values) {
                if (!hasErrorHndl) {
                    AsyncCallbackStruct* out = nullptr;
                    const int FLAGS =
                        templatious::VPACK_SYNCED;
                    auto p = fact->makePackCustomWCallback< FLAGS >(
                        size,types,values,AsyncCallbackStruct(TABLE_IDX,funcRef,*ctxW,&out));
                    out->setMyself(p);
                    return p;
                } else {
                    AsyncCallbackStruct* out = nullptr;
                    const int FLAGS =
                        templatious::VPACK_SYNCED;
                    auto p = fact->makePackCustomWCallback< FLAGS >(
                        size,types,values,AsyncCallbackStruct(
                            TABLE_IDX,funcRef,TABLE_IDX,funcRefFail,*ctxW,&out));
                    out->setMyself(p);
                    return p;
                }
            });

        msg->message(p);

        return 0;
    }

    // -1 -> value tree
    // -2 -> error callback (could be null)
    // -3 -> strong messageable
    // -4 -> context
    static int luanat_sendPackAsync(lua_State* state) {
        WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(::lua_touserdata(state,-4));
        StrongMsgPtr* msgPtr = reinterpret_cast<
            StrongMsgPtr*>(::lua_touserdata(state,-3));

        auto ctx = ctxW->lock();
        assert( nullptr != ctx && "Context already dead?" );

        ctx->assertThread();

        auto& msg = *msgPtr;
        assert( nullptr != msg && "Messageable doesn't exist." );

        const int TABLE_IDX = LUA_REGISTRYINDEX;
        bool hasErrorHndl = LUA_TNIL != ::lua_type(state,-2);
        int funcRefFail = -1;
        if (hasErrorHndl) {
            ::lua_pushvalue(state,-2);
            funcRefFail = ::luaL_ref(state,TABLE_IDX);
        }

        auto outTree = makeTreeFromTable(*ctx,state,-1);
        sortVTree(*outTree);
        auto fact = ctx->getFact();
        auto p = treeToPack(*ctx,*outTree,
            [=](int size,const char** types,const char** values)
             -> StrongPackPtr
            {
                if (!hasErrorHndl) {
                    return fact->makePack(size,types,values);
                } else {
                    AsyncCallbackStruct* out = nullptr;
                    const int FLAGS =
                        templatious::VPACK_SYNCED;
                    auto p = fact->makePackCustomWCallback< FLAGS >(
                        size,types,values,AsyncCallbackStruct(
                            true,TABLE_IDX,funcRefFail,*ctxW,&out));
                    out->setMyself(p);
                    return p;
                }
            });

        msg->message(p);

        return 0;
    }

    // -1 -> value tree
    // -2 -> strong messageable
    // -3 -> context
    static int luanat_sendPack(lua_State* state) {
        WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(::lua_touserdata(state,-3));
        StrongMsgPtr* msgPtr = reinterpret_cast<
            StrongMsgPtr*>(::lua_touserdata(state,-2));

        auto ctx = ctxW->lock();
        assert( nullptr != ctx && "Context already dead?" );

        ctx->assertThread();

        auto& msg = *msgPtr;
        assert( nullptr != msg && "Messageable doesn't exist." );

        auto outTree = makeTreeFromTable(*ctx,state,-1);
        sortVTree(*outTree);
        auto fact = ctx->getFact();
        bool outRes = false;
        bool *resPtr = &outRes;
        auto p = treeToPack(*ctx,*outTree,
            [=](int size,const char** types,const char** values) {
                return fact->makePackWCallback(size,types,values,
                    CallbackResultWriter(resPtr));
            });

        msg->message(*p);

        ::lua_pushboolean(state,outRes);
        return 1;
    }

    static VTree packToTree(LuaContext& ctx,const templatious::VirtualPack& pack) {
        typedef std::vector< VTree > TreeVec;
        VTree root("[root]",TreeVec());
        auto& rootTreeVec = root.getInnerTree();
        rootTreeVec.emplace_back("types",TreeVec());
        rootTreeVec.emplace_back("values",TreeVec());

        packToTreeRec(ctx,
            rootTreeVec[0],rootTreeVec[1],pack,ctx._fact);
        return root;
    }

    static void packToTreeRec(
        LuaContext& ctx,
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
            if (LuaContextPrimitives::intNode() == outInf[i]) {
                const int* reint = reinterpret_cast<const int*>(
                    ptrFromString(outVec[i]));
                tnVec.emplace_back(keyBuf,assocName);
                vnVec.emplace_back(keyBuf,*reint);
            } else if (LuaContextPrimitives::doubleNode() == outInf[i]) {
                const double* reint = reinterpret_cast<const double*>(
                    ptrFromString(outVec[i]));
                tnVec.emplace_back(keyBuf,assocName);
                vnVec.emplace_back(keyBuf,*reint);
            } else if (LuaContextPrimitives::boolNode() == outInf[i]) {
                bool result = outVec[i] == "t" ? true : false;
                assert( outVec[i] == "t" || outVec[i] == "f" );
                tnVec.emplace_back(keyBuf,assocName);
                vnVec.emplace_back(keyBuf,result);
            } else if (LuaContextPrimitives::messageableStrongNode() == outInf[i]) {
                auto ptr = ptrFromString(outVec[i]);
                StrongMsgPtr* msg = reinterpret_cast<StrongMsgPtr*>(ptr);
                tnVec.emplace_back(keyBuf,assocName);
                vnVec.emplace_back(keyBuf,*msg);
            } else if (LuaContextPrimitives::vpackNode() != outInf[i]) {
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
                packToTreeRec(ctx,tnodeRef,vnodeRef,**vpptr,fact);
            }
        }
    }

    static void processMessages(ContextMessageable& ctx) {
        ctx.assertThread();
        ctx._cache.process(
            [&](templatious::VirtualPack& pack) {
                ctx._handler->tryMatch(pack);
            });
    }

    static void processMessages(LuaContext& ctx) {
        std::vector< AsyncCallbackMessage > steal;
        {
            LuaContext::Guard g(ctx._mtx);
            if (ctx._callbacks.size() > 0) {
                steal = std::move(ctx._callbacks);
            }
        }

        TEMPLATIOUS_FOREACH(auto& i,steal) {
            processSingleAsyncCallback(ctx,i);
        }

        ctx._eventDriver.process();
    }

    static void appendToEventDriver(LuaContext& ctx,std::function<bool()>& func) {
        ctx._eventDriver.attach(func);
    }

    static void enqueueCallback(
            LuaContext& ctx,
            int table,
            int func,
            bool call,
            const StrongPackPtr& pack,
            const WeakCtxPtr& wCtx)
    {
        LuaContext::Guard g(ctx._mtx);
        ctx._callbacks.emplace_back(table,func,call,pack,wCtx);
    }

    static void processSingleAsyncCallback(
        LuaContext& ctx,
        AsyncCallbackMessage& msg)
    {
        if (msg.shouldCall()) {
            ::lua_rawgeti(ctx._s,msg.tableRef(),msg.funcRef());
            auto msgP = msg.pack();
            if (nullptr != msgP) {
                auto vtree = LuaContextImpl::packToTree(ctx,*msgP);
                VTreeBind::pushVTree(ctx._s,std::move(vtree));
                handleLuaError(::lua_pcall(ctx._s,1,0,0),ctx._s);
            } else {
                handleLuaError(::lua_pcall(ctx._s,0,0,0),ctx._s);
            }
        }
    }


    static void initContextFunc(const std::shared_ptr< LuaContext >& ctx);
    static void initContext(
        const std::shared_ptr< LuaContext >& ctx,
        const char* luaPlumbingFile
    );

    static void setDependency(LuaContext& ctx,WeakMsgPtr wmsg) {
        ctx._updateDependency = wmsg;
    }

    static void notifyDependency(const WeakCtxPtr& wCtx) {
        typedef GenericMessageableInterface GMI;

        auto locked = wCtx.lock();
        auto notify = locked->_updateDependency.lock();
        if (nullptr == notify) {
            return;
        }

        auto msg = SF::vpack< GMI::OutRequestUpdate >(
            GMI::OutRequestUpdate()
        );

        notify->message(msg);
    }

    static long currentMillis() {
        return std::chrono::duration_cast<
            std::chrono::milliseconds
        >(
            std::chrono::high_resolution_clock::now()
                - getRefTime()).count();
    }

    static auto getRefTime()
    -> decltype(std::chrono::high_resolution_clock::now()) {
        static auto refPoint =
            std::chrono::high_resolution_clock::now();
        return refPoint;
    }
};

void LuaMessageHandler::message(const StrongPackPtr& sptr) {
    _cache.enqueue(sptr);
    LuaContextImpl::notifyDependency(_ctxW);
}

namespace VTreeBind {

int luanat_freeVtree(lua_State* state) {
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
                {
                    void* nbuf = ::lua_newuserdata(state,sizeof(StrongMsgPtr));
                    new (nbuf) StrongMsgPtr(tree.getWeakMsg().lock());
                    ::luaL_setmetatable(state,"StrongMessageablePtr");
                    ::lua_setfield(state,adjIdx,buf);
                }
                break;
            case VTree::Type::Boolean:
                ::lua_pushboolean(state,tree.getBool());
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
int luanat_getValTree(lua_State* state) {
    return rootPushGeneric(state,"values");
}

// -1 -> VTree
int luanat_getTypeTree(lua_State* state) {
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
int luanat_testVtree(lua_State* state) {
    WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(::lua_touserdata(state,-2));

    auto ctx = ctxW->lock();
    auto outTree = LuaContextImpl::makeTreeFromTable(*ctx,state,-1);
    sortVTree(*outTree);

    pushVTree(state,std::move(*outTree));
    return 1;
}

}

namespace StrongMessageableBind {

int luanat_freeStrongMessageable(lua_State* state) {
    StrongMsgPtr* strongMsg =
        reinterpret_cast<StrongMsgPtr*>(
            ::lua_touserdata(state,-1));

    strongMsg->~shared_ptr();
    return 0;
}

// -1 -> strong messageable
int luanat_getWeakReference(lua_State* state) {
    StrongMsgPtr* strongMsg =
        reinterpret_cast<StrongMsgPtr*>(
            ::lua_touserdata(state,-1));

    void* buf = ::lua_newuserdata(state,sizeof(WeakMsgPtr));

    new (buf) WeakMsgPtr(*strongMsg);
    ::luaL_setmetatable(state,"WeakMessageablePtr");

    return 1;
}

// -1 -> userdata
int luanat_isStrongMessageable(lua_State* state) {
    int res = ::lua_getmetatable(state,-1);
    if (0 == res) {
        ::lua_pushboolean(state,false);
        return 1;
    }

    luaL_getmetatable(state,"StrongMessageablePtr");

    int out = ::lua_compare(state,-1,-2,LUA_OPEQ);
    ::lua_pushboolean(state,out);

    return 1;
}

}

namespace WeakMessageableBind {

// -1 -> userdata
int luanat_isWeakMessageable(lua_State* state) {
    int res = ::lua_getmetatable(state,-1);
    if (0 == res) {
        ::lua_pushboolean(state,false);
        return 1;
    }

    luaL_getmetatable(state,"WeakMessageablePtr");

    int out = ::lua_compare(state,-1,-2,LUA_OPEQ);
    ::lua_pushboolean(state,out);

    return 1;
}

int luanat_freeWeakMessageable(lua_State* state) {
    WeakMsgPtr* weakMsg =
        reinterpret_cast<WeakMsgPtr*>(
            ::lua_touserdata(state,-1));

    weakMsg->~weak_ptr();
    return 0;
}

}

namespace LuaContextBind {

// -1 -> name
// -2 -> weak ctx
int luanat_getMessageableStrongRef(lua_State* state) {
    WeakCtxPtr* ctxW = reinterpret_cast<WeakCtxPtr*>(
        ::lua_touserdata(state,-2));
    const char* name = reinterpret_cast<const char*>(
        ::lua_tostring(state,-1));

    auto ctx = ctxW->lock();
    assert( nullptr != ctx && "Context already dead?" );

    auto msg = ctx->getMessageable(name);
    assert( nullptr != msg && "Messageable doesn't exist." );

    void* buf = ::lua_newuserdata(state,sizeof(StrongMsgPtr));

    new (buf) StrongMsgPtr(msg);
    ::luaL_setmetatable(state,"StrongMessageablePtr");
    return 1;
}

}

void registerNullMessageable(lua_State* state,const char* name) {
    void* buf = ::lua_newuserdata(state,sizeof(StrongMsgPtr));
    new (buf) StrongMsgPtr(nullptr);
    ::luaL_setmetatable(state,"StrongMessageablePtr");
    ::lua_setglobal(state,name);
}

void registerVTree(lua_State* state) {
    ::luaL_newmetatable(state,"VTree");
    ::lua_pushcfunction(state,&VTreeBind::luanat_freeVtree);
    ::lua_setfield(state,-2,"__gc");

    ::lua_createtable(state,4,0);
    // -1 table
    ::lua_pushcfunction(state,&VTreeBind::luanat_getValTree);
    ::lua_setfield(state,-2,"values");

    ::lua_pushcfunction(state,&VTreeBind::luanat_getTypeTree);
    ::lua_setfield(state,-2,"types");

    ::lua_setfield(state,-2,"__index");
    ::lua_pop(state,1);
}

void registerStrongMessageable(lua_State* state) {
    ::luaL_newmetatable(state,"StrongMessageablePtr");
    ::lua_pushcfunction(state,&StrongMessageableBind::luanat_freeStrongMessageable);
    ::lua_setfield(state,-2,"__gc");

    ::lua_createtable(state,4,0);

    ::lua_pushcfunction(state,&StrongMessageableBind::luanat_getWeakReference);
    ::lua_setfield(state,-2,"getWeak");

    ::lua_pop(state,1);
}

void registerWeakMessageable(lua_State* state) {
    ::luaL_newmetatable(state,"WeakMessageablePtr");
    ::lua_pushcfunction(state,&WeakMessageableBind::luanat_freeWeakMessageable);
    ::lua_setfield(state,-2,"__gc");

    ::lua_pop(state,1);
}

void registerVMessageST(lua_State* state) {
    ::luaL_newmetatable(state,"VMessageST");
    ::lua_pushcfunction(state,&VMessageST::luanat_gc);
    ::lua_setfield(state,-2,"__gc");

    ::lua_createtable(state,4,0);
    // -1 table
    ::lua_pushcfunction(state,&VMessageST::luanat_isST);
    ::lua_setfield(state,-2,"isST");
    ::lua_pushcfunction(state,&VMessageST::luanat_isMT);
    ::lua_setfield(state,-2,"isMT");
    ::lua_pushcfunction(state,&VMessageST::luanat_getValTree);
    ::lua_setfield(state,-2,"vtree");
    ::lua_pushcfunction(state,&VMessageST::luanat_forwardST);
    ::lua_setfield(state,-2,"forwardST");
    ::lua_pushcfunction(state,&VMessageST::luanat_forwardMT);
    ::lua_setfield(state,-2,"forwardMT");
    ::lua_pushcfunction(state,&VMessageST::luanat_setValueST);
    ::lua_setfield(state,-2,"setSlot");

    ::lua_setfield(state,-2,"__index");
    ::lua_pop(state,1);
}

void registerVMessageMT(lua_State* state) {
    ::luaL_newmetatable(state,"VMessageMT");
    ::lua_pushcfunction(state,&VMessageMT::luanat_gc);
    ::lua_setfield(state,-2,"__gc");

    ::lua_createtable(state,4,0);
    // -1 table
    ::lua_pushcfunction(state,&VMessageMT::luanat_isST);
    ::lua_setfield(state,-2,"isST");
    ::lua_pushcfunction(state,&VMessageMT::luanat_isMT);
    ::lua_setfield(state,-2,"isMT");
    ::lua_pushcfunction(state,&VMessageMT::luanat_getValTree);
    ::lua_setfield(state,-2,"vtree");
    ::lua_pushcfunction(state,&VMessageMT::luanat_forwardST);
    ::lua_setfield(state,-2,"forwardST");
    ::lua_pushcfunction(state,&VMessageMT::luanat_forwardMT);
    ::lua_setfield(state,-2,"forwardMT");
    ::lua_pushcfunction(state,&VMessageMT::luanat_setValueMT);
    ::lua_setfield(state,-2,"setSlot");

    ::lua_setfield(state,-2,"__index");
    ::lua_pop(state,1);
}

auto ContextMessageable::genHandler() -> VmfPtr {
    typedef GenericMessageableInterface GMI;
    return SF::virtualMatchFunctorPtr(
        SF::virtualMatch< GMI::OutRequestUpdate >(
            [=](GMI::OutRequestUpdate) {
                //this->notifyDependency();
            }
        ),
        SF::virtualMatch< GMI::AttachItselfToMessageable, StrongMsgPtr >(
            [=](GMI::AttachItselfToMessageable,const StrongMsgPtr& wmsg) {
                assert( nullptr != wmsg && "Can't attach, dead." );

                auto wmsgCpy = _wMsg;
                std::function<bool()> func = [=]() {
                    auto lockedSelf = wmsgCpy.lock();
                    assert( nullptr != lockedSelf && "Just checkin..." );
                    if (nullptr == lockedSelf) {
                        return false;
                    }

                    auto locked = lockedSelf->_wCtx.lock();
                    assert( nullptr != locked && "Just checkin another time..." );
                    if (nullptr == locked) {
                        return false;
                    }

                    LuaContextImpl::processMessages(*lockedSelf);
                    LuaContextImpl::processMessages(*locked);

                    return true;
                };

                auto p = SF::vpack<
                    GMI::InAttachToEventLoop,
                    std::function<bool()>
                >(GMI::InAttachToEventLoop(),std::move(func));

                auto locked = this->_wCtx.lock();
                LuaContextImpl::setDependency(*locked,wmsg);

                wmsg->message(p);
            }
        ),
        SF::virtualMatch< GMI::InAttachToEventLoop, std::function<bool()> >(
            [=](GMI::InAttachToEventLoop,std::function<bool()>& func) {
                auto locked = this->_wCtx.lock();
                LuaContextImpl::appendToEventDriver(*locked,func);
            }
        )
    );
}

ContextMessageable::ContextMessageable(
    const std::weak_ptr< LuaContext >& ctx) :
    _wCtx(ctx), _handler(genHandler())
{
    _lastUpdate = LuaContextImpl::currentMillis();
}

void ContextMessageable::message(const StrongPackPtr& pack) {
    _cache.enqueue(pack);
}

LuaMessageHandler::LuaMessageHandler(const WeakCtxPtr& wptr,int table,int func) :
    _ctxW(wptr), _table(table), _funcRef(func), _hndl(genHandler())
{
    _lastUpdate = LuaContextImpl::currentMillis();
}

LuaContext::LuaContext() :
    _fact(nullptr),
    _s(luaL_newstate())
{
    registerNullMessageable(_s,"__vmsgNull");
}

LuaContext::~LuaContext() {
    ::lua_close(_s);
}

void LuaContext::assertThread() const {
    _tg.assertThread();
}

const templatious::DynVPackFactory* LuaContext::getFact() const {
    return _fact;
}

void LuaContext::addMessageableWeak(const char* name,const WeakMsgPtr& weakRef) {
    Guard g(_mtx);
    assert( _messageableMapStrong.find(name) == _messageableMapStrong.end()
        && "Strong reference with same name exists." );

    _messageableMapWeak.insert(std::pair<std::string, WeakMsgPtr>(name,weakRef));
}

void LuaContext::addMessageableStrong(const char* name,const StrongMsgPtr& strongRef) {
    Guard g(_mtx);
    assert( _messageableMapStrong.find(name) == _messageableMapStrong.end()
        && "Strong reference with same name exists." );

    _messageableMapStrong.insert(std::pair<std::string, StrongMsgPtr>(name,strongRef));
}

StrongMsgPtr LuaContext::getMessageable(const char* name) {
    Guard g(_mtx);
    auto iterWeak = _messageableMapWeak.find(name);
    if (iterWeak != _messageableMapWeak.end()) {
        auto locked = iterWeak->second.lock();
        if (nullptr != locked) {
            return locked;
        } else {
            _messageableMapWeak.erase(name);
        }
    }

    auto iter = _messageableMapStrong.find(name);
    if (iter == _messageableMapStrong.end()) {
        return nullptr;
    }
    return iter->second;
}

void LuaContext::regFunction(const char* name,lua_CFunction func) {
    lua_register(_s,name,func);
}

void LuaContext::setFactory(templatious::DynVPackFactory* fact) {
    _fact = fact;
}

bool LuaContext::doFile(const char* path) {
    bool result = luaL_dofile(_s,path) == 0;

    if (!result) {
        _lastError = lua_tostring(_s,-1);
        printf("LuaContext, error: %s\n",_lastError.c_str());
    }
    return result;
}

typedef templatious::TypeNodeFactory TNF;

#define ATTACH_NAMED_DUMMY(factory,name,type)   \
    factory.attachNode(name,TNF::makeDummyNode< type >(name))

void LuaContext::registerPrimitives(templatious::DynVPackFactoryBuilder& bld) {
    bld.attachNode("int",LuaContextPrimitives::intNode());
    bld.attachNode("double",LuaContextPrimitives::doubleNode());
    bld.attachNode("bool",LuaContextPrimitives::boolNode());
    bld.attachNode("string",LuaContextPrimitives::stringNode());
    bld.attachNode("vpack",LuaContextPrimitives::vpackNode());
    bld.attachNode("vmsg_name",LuaContextPrimitives::messageableWeakNode());
    bld.attachNode("vmsg_raw_weak",LuaContextPrimitives::messageableWeakNode());
    bld.attachNode("vmsg_raw_strong",LuaContextPrimitives::messageableStrongNode());

    typedef GenericMessageableInterface GMI;
    ATTACH_NAMED_DUMMY( bld, "gen_inattachitself", GMI::AttachItselfToMessageable );
    ATTACH_NAMED_DUMMY( bld, "gen_inattachtoeventloop", GMI::InAttachToEventLoop );
}

std::shared_ptr< LuaContext > LuaContext::makeContext(
    const char* luaPlumbingFile)
{
    std::shared_ptr< LuaContext > out(new LuaContext());
    LuaContextImpl::initContext(out,luaPlumbingFile);
    out->_myselfWeak = out;
    return out;
}

void LuaContext::processMessages() {
    LuaContextImpl::processMessages(*this);
}

AsyncCallbackMessage::~AsyncCallbackMessage() {
    auto locked = _ctx.lock();
    assert( nullptr != locked && "Context already dead?" );

    lua_State* s = locked->s();
    ::luaL_unref(s,_tableRef,_funcRef);
}


// -1 -> VMessageST
int VMessageST::luanat_getValTree(lua_State* state) {
    VMessageST* cache = reinterpret_cast<VMessageST*>(
        ::lua_touserdata(state,-1));

    VTreeBind::pushVTree(state,LuaContextImpl::packToTree(
        *cache->_ctx,*cache->_pack));

    return 1;
}

// -1 -> VMessageMT
int VMessageMT::luanat_getValTree(lua_State* state) {
    VMessageMT* cache = reinterpret_cast<VMessageMT*>(
        ::lua_touserdata(state,-1));

    VTreeBind::pushVTree(state,
        LuaContextImpl::packToTree(*cache->_ctx,*cache->_pack));

    return 1;
}

void LuaContextImpl::initContextFunc(const std::shared_ptr< LuaContext >& ctx) {
    auto s = ctx->s();
    void* adr = ::lua_newuserdata(s, sizeof(WeakCtxPtr) );
    new (adr) WeakCtxPtr(ctx);

    ::luaL_newmetatable(s,"WeakCtxPtr");
    ::lua_pushcfunction(s,&luanat_freeWeakLuaContext);
    ::lua_setfield(s,-2,"__gc");

    ::lua_createtable(s,4,0);
    ::lua_pushcfunction(s,
        &LuaContextBind::luanat_getMessageableStrongRef);
    ::lua_setfield(s,-2,"namedMessageable");
    ::lua_pushcfunction(s,
        &LuaMessageHandler::luanat_makeLuaHandler);
    ::lua_setfield(s,-2,"makeLuaHandler");
    ::lua_setfield(s,-2,"__index");

    ::lua_setmetatable(s,-2);
}

void LuaContextImpl::initContext(
    const std::shared_ptr< LuaContext >& ctx,
    const char* luaPlumbingFile
)
{
    auto s = ctx->s();
    luaL_openlibs(s);

    ctx->regFunction("nat_sendPack",
        &LuaContextImpl::luanat_sendPack);
    ctx->regFunction("nat_sendPackWCallback",
        &LuaContextImpl::luanat_sendPackWCallback);
    ctx->regFunction("nat_sendPackAsync",
        &LuaContextImpl::luanat_sendPackAsync);
    ctx->regFunction("nat_sendPackAsyncWCallback",
        &LuaContextImpl::luanat_sendPackAsyncWCallback);
    ctx->regFunction("nat_areMessageablesEqual",
        &LuaContextImpl::luanat_areMessageablesEqual);
    ctx->regFunction("nat_testVTree",
        &VTreeBind::luanat_testVtree);
    ctx->regFunction("nat_isMessageable",
        &StrongMessageableBind::luanat_isStrongMessageable);
    ctx->regFunction("nat_isWeakMessageable",
        &WeakMessageableBind::luanat_isWeakMessageable);

    auto msg = std::make_shared< ContextMessageable >(ctx);
    msg->_wMsg = msg;
    ctx->addMessageableStrong("context",msg);

    bool success = luaL_dofile(s,luaPlumbingFile) == 0;
    if (!success) {
        printf("%s\n", lua_tostring(s, -1));
    }
    assert( success );

    registerVTree(s);
    registerVMessageST(s);
    registerVMessageMT(s);
    registerStrongMessageable(s);
    registerWeakMessageable(s);

    initContextFunc(ctx);

    ::lua_getglobal(s,"initLuaContext");
    ::lua_pushvalue(s,-2);
    handleLuaError(::lua_pcall(s,1,0,0),s);
}

void NotifierCache::add(const std::shared_ptr< Messageable >& another) {
    Guard g(_mtx);
    SA::add(_cache,PairType(true,another));
}

template <class T>
struct LambdaGuard {

    LambdaGuard(T t) : _l(t) {}

    ~LambdaGuard() {
        _l();
    }

    T _l;
};

void NotifierCache::notify(templatious::VirtualPack& msg) {
    std::vector< PairType > steal;
    {
        Guard g(_mtx);
        std::swap(_cache,steal);
    }

    auto onScopeExit = [&]() {
        Guard g(_mtx);
        std::swap(_cache,steal);
        if (SA::size(steal) > 0) {
            SA::add(_cache,steal);
        }
    };
    LambdaGuard< decltype(onScopeExit)& > lg(onScopeExit);

    TEMPLATIOUS_FOREACH(auto& i,steal) {
        auto locked = i.second.lock();
        i.first = locked != nullptr;
        locked->message(msg);
    }

    SA::clear(
        SF::filter(
            steal,
            [](PairType& i) {
                return !i.first;
            }
        )
    );
}

void NotifierCache::notify(const StrongPackPtr& msg) {
    Guard g(_mtx);
    TEMPLATIOUS_FOREACH(auto& i,_cache) {
        auto locked = i.second.lock();
        i.first = locked != nullptr;
        locked->message(msg);
    }

    SA::clear(
        SF::filter(
            _cache,
            [](PairType& i) {
                return !i.first;
            }
        )
    );
}
