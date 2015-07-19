#ifndef DOMAIN_8UU5DBQ1
#define DOMAIN_8UU5DBQ1

#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <cassert>
#include <lua5.2/lua.hpp>

typedef int (*lua_CFunction) (lua_State *L);

namespace templatious {
    struct DynVPackFactory;
    struct DynVPackFactoryBuilder;
    struct VirtualMatchFunctor;
    struct VirtualPack;
}

class Messageable {
public:
    // this is for sending message across threads
    virtual void message(const std::shared_ptr< templatious::VirtualPack >& msg) = 0;

    // this is for sending stack allocated (faster)
    // if we know we're on the same thread as GUI
    virtual void message(templatious::VirtualPack& msg) = 0;
};

typedef std::shared_ptr< Messageable > StrongMsgPtr;
typedef std::weak_ptr< Messageable > WeakMsgPtr;
typedef std::shared_ptr<
    templatious::VirtualPack > StrongPackPtr;
typedef std::weak_ptr< struct LuaContext > WeakCtxPtr;

struct ThreadGuard {
    ThreadGuard() :
        _id(std::this_thread::get_id())
    {}

    void assertThread() const {
        assert( _id == std::this_thread::get_id()
            && "Thread id mismatch." );
    }

private:
    std::thread::id _id;
};

// for event processing driving
struct GenericMesseagableInterface {

    // Sent to send message to attach
    // to the event loop messeagable
    // Signature:
    // < AttachItselfToMesseagable , std::weak_ptr< Messeagable > >
    struct AttachItselfToMesseagable {};

    // attaches messageable to event
    // loop of receiving message
    // Signature:
    // < InAttachToEventLoop, std::function<void()> >
    struct InAttachToEventLoop {};
};

struct MessageCache {

    void enqueue(const StrongPackPtr& pack) {
        Guard g(_mtx);
        _queue.push_back(pack);
    }

    template <class Func>
    void process(Func&& f) {
        std::vector< StrongPackPtr > steal;
        {
            Guard g(_mtx);
            if (0 == _queue.size()) {
                return;
            }
            steal = std::move(_queue);
        }

        for (auto& i: steal) {
            f(*i);
            i = nullptr;
        }
    }

    template <class Func>
    void processPtr(Func&& f) {
        std::vector< StrongPackPtr > steal;
        {
            Guard g(_mtx);
            if (0 == _queue.size()) {
                return;
            }
            steal = std::move(_queue);
        }

        for (auto& i: steal) {
            f(i);
            i = nullptr;
        }
    }

private:
    typedef std::lock_guard< std::mutex > Guard;
    std::mutex _mtx;
    std::vector< StrongPackPtr > _queue;
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
    lua_State* s() const { return _s; }

    LuaContext(const LuaContext&) = delete;
    LuaContext(LuaContext&&) = delete;

    void setFactory(templatious::DynVPackFactory* fact);
    void regFunction(const char* name,lua_CFunction func);
    bool doFile(const char* path);

    StrongMsgPtr getMesseagable(const char* name);

    void addMesseagableStrong(const char* name,const StrongMsgPtr& strongRef);
    void addMesseagableWeak(const char* name,const WeakMsgPtr& weakRef);

    const templatious::DynVPackFactory* getFact() const;
    void assertThread();

    /**
     * Register primitives that are used by this context.
     * Supported types:
     * int, double
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

    typedef std::unique_ptr< templatious::VirtualMatchFunctor > VmfPtr;

    VmfPtr genHandler();

    typedef std::lock_guard< std::mutex > Guard;

    std::map< std::string, WeakMsgPtr   > _messageableMapWeak;
    std::map< std::string, StrongMsgPtr > _messageableMapStrong;

    std::mutex _mtx;
    templatious::DynVPackFactory* _fact;
    lua_State* _s;
    ThreadGuard _tg;

    VmfPtr _msgHandler;
    MessageCache _cache;
    std::vector< std::function<void()> > _eventDriver;
    std::vector< AsyncCallbackMessage > _callbacks;
};

#endif /* end of include guard: DOMAIN_8UU5DBQ1 */

