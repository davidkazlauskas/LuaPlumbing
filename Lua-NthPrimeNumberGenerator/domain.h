#ifndef DOMAIN_8UU5DBQ1
#define DOMAIN_8UU5DBQ1

#include <map>
#include <memory>
#include <mutex>
#include <lua5.2/lua.hpp>

#include "messeagable.h"

typedef int (*lua_CFunction) (lua_State *L);

namespace templatious {
    struct DynVPackFactory;
}

typedef std::shared_ptr< Messageable > StrongMsgPtr;
typedef std::weak_ptr< Messageable > WeakMsgPtr;
typedef std::shared_ptr<
    templatious::VirtualPack > StrongPackPtr;

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

struct VTree {
    enum class Type {
        StdString,
        VPackStrong,
        MessageableWeak
    };

    VTree(const VTree&) = delete;
    VTree(VTree&& other) :
        _type(other._type),
        _key(std::move(other._key))
        _ptr(other._ptr)
    {
        other._ptr = nullptr;
    }

    VTree(const char* ptr) :
        _type(Type::StdString),
        _ptr(new std::string(ptr))
    {}

    VTree(const StrongPackPtr& ptr) :
        _type(Type::VPackStrong),
        _ptr(new StrongPackPtr(ptr))
    {}

    VTree(const WeakMsgPtr& ptr) :
        _type(Type::MessageableWeak),
        _ptr(new WeakMsgPtr(ptr))
    {}

    ~VTree()
    {
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
            default:
                assert( false && "HUH?" );
                break;
        }
    }

    Type getType() const { return _type; }

private:
    Type _type;
    std::string _key;
    void* _ptr;
};

struct LuaContext {
    LuaContext() :
        _s(luaL_newstate()),
        _thisId(std::this_thread::get_id())
    {}

    lua_State* s() const { return _s; }

private:
    lua_State* _s;
    std::thread::id _thisId;
};

void initDomain(const std::shared_ptr< LuaContext >& ptr);

#endif /* end of include guard: DOMAIN_8UU5DBQ1 */

