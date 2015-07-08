#ifndef DOMAIN_8UU5DBQ1
#define DOMAIN_8UU5DBQ1

#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <cassert>
#include <lua5.2/lua.hpp>

#include <templatious/FullPack.hpp>

#include "messeagable.h"

typedef int (*lua_CFunction) (lua_State *L);

namespace templatious {
    struct DynVPackFactory;
}

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

        TEMPLATIOUS_FOREACH(auto& i,steal) {
            f(*i);
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

        TEMPLATIOUS_FOREACH(auto& i,steal) {
            f(i);
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

    ~AsyncCallbackMessage();

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

struct LuaContext : public Messageable {
    LuaContext();

    lua_State* s() const { return _s; }

    void setFactory(templatious::DynVPackFactory* fact);
    void regFunction(const char* name,lua_CFunction func);

    StrongMsgPtr getMesseagable(const char* name);

    void addMesseagableStrong(const char* name,const StrongMsgPtr& strongRef);
    void addMesseagableWeak(const char* name,const WeakMsgPtr& weakRef);

    const templatious::DynVPackFactory* getFact() const;
    void assertThread();

    void message(templatious::VirtualPack& p) override;
    void message(StrongPackPtr p) override;

private:
    friend struct AsyncCallbackStruct;
    friend struct LuaContextImpl;

    void processSingleAsyncCallback(AsyncCallbackMessage& msg);

    // will need to be made more efficient
    void enqueueCallback(
            int func,
            int table,
            const StrongPackPtr& pack,
            const WeakCtxPtr& ctx);

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


void initDomain(const std::shared_ptr< LuaContext >& ptr);

#endif /* end of include guard: DOMAIN_8UU5DBQ1 */

