#ifndef DOMAIN_8UU5DBQ1
#define DOMAIN_8UU5DBQ1

#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <cassert>
#include <lua5.2/lua.hpp>

#include "messageable.hpp"

typedef int (*lua_CFunction) (lua_State *L);

typedef std::weak_ptr< struct LuaContext > WeakCtxPtr;

struct ThreadGuard {
    ThreadGuard() :
        _id(std::this_thread::get_id())
    {}

    void assertThread() const {
        //assert( _id == std::this_thread::get_id()
            //&& "Thread id mismatch." );
    }

private:
    std::thread::id _id;
};

// for event processing driving
struct GenericMessageableInterface {

    // Sent to send message to attach
    // to the event loop messageable
    // Signature:
    // < AttachItselfToMessageable , std::weak_ptr< Messageable > >
    struct AttachItselfToMessageable {};

    // attaches messageable to event
    // loop of receiving message
    // Signature:
    // < InAttachToEventLoop, std::function<bool()> >
    struct InAttachToEventLoop {};

    // Request processing update
    // Of it's dependency.
    struct OutRequestUpdate {};
};

struct CallbackCache {

    void process();
    void attach(const std::function<bool()>& func);

private:
    std::vector< std::pair<
        bool, std::function<bool()>
    > > _eventDriver;
};

struct AsyncCallbackMessage {

    AsyncCallbackMessage(const AsyncCallbackMessage&) = delete;

    AsyncCallbackMessage(AsyncCallbackMessage&& other) :
        _tableRef(other._tableRef),
        _funcRef(other._funcRef),
        _shouldCall(other._shouldCall),
        _pack(other._pack),
        _ctx(other._ctx)
    {
        other._tableRef = -1;
        other._funcRef = -1;
    }

    AsyncCallbackMessage(
        int tableRef,int funcRef,
        bool shouldCall,
        const StrongPackPtr& ptr,
        const WeakCtxPtr& ctx
    ) :
        _tableRef(tableRef),
        _funcRef(funcRef),
        _shouldCall(shouldCall),
        _pack(ptr), _ctx(ctx) {}

    ~AsyncCallbackMessage();

    int tableRef() {
        return _tableRef;
    }

    int funcRef() {
        return _funcRef;
    }

    bool shouldCall() {
        return _shouldCall;
    }

    const StrongPackPtr& pack() {
        return _pack;
    }

private:
    int _tableRef;
    int _funcRef;
    bool _shouldCall;
    StrongPackPtr _pack;
    WeakCtxPtr _ctx;
};

struct LuaContext {
    lua_State* s() const { assertThread(); return _s; }

    LuaContext(const LuaContext&) = delete;
    LuaContext(LuaContext&&) = delete;

    void setFactory(templatious::DynVPackFactory* fact);
    void regFunction(const char* name,lua_CFunction func);
    bool doFile(const char* path);

    StrongMsgPtr getMessageable(const char* name);

    void addMessageableStrong(const char* name,const StrongMsgPtr& strongRef);
    void addMessageableWeak(const char* name,const WeakMsgPtr& weakRef);

    const templatious::DynVPackFactory* getFact() const;
    void assertThread() const;

    /**
     * Register primitives that are used by this context.
     * Supported types:
     * int, double, string, bool
     */
    static void registerPrimitives(templatious::DynVPackFactoryBuilder& bld);

    static std::shared_ptr< LuaContext > makeContext(
        const char* luaPlumbingFile = "plumbing.lua");

    ~LuaContext();

    /**
     * Process messages. Checked if running in creator
     * thread with thread guard.
     */
    void processMessages();

private:
    LuaContext();

    friend struct AsyncCallbackStruct;
    friend struct LuaContextImpl;

    typedef std::lock_guard< std::mutex > Guard;

    std::map< std::string, WeakMsgPtr   > _messageableMapWeak;
    std::map< std::string, StrongMsgPtr > _messageableMapStrong;

    std::mutex _mtx;
    templatious::DynVPackFactory* _fact;
    lua_State* _s;
    ThreadGuard _tg;

    CallbackCache _eventDriver;
    std::vector< AsyncCallbackMessage > _callbacks;
    std::weak_ptr< LuaContext > _myselfWeak;
    WeakMsgPtr _updateDependency;

    std::string _lastError;
};

#endif /* end of include guard: DOMAIN_8UU5DBQ1 */

