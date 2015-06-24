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

struct LuaContext {
    LuaContext() :
        _s(luaL_newstate()), _fact(nullptr) {}

    lua_State* s() {
        return _s;
    }

    void regFunction(const char* name,lua_CFunction func) {
        lua_register(_s,name,func);
    }

    ~LuaContext() {
        lua_close(_s);
    }

    void setFactory(templatious::DynVPackFactory* fact) {
        _fact = fact;
    }

    typedef std::shared_ptr< Messageable > StrongMsgPtr;
    typedef std::weak_ptr< Messageable > WeakMsgPtr;
    typedef std::shared_ptr<
        templatious::VirtualPack > StrongPackPtr;

    void indexPack(const char* key,const StrongPackPtr& msg) {
        Guard g(_mtx);
        _packMap.insert(std::pair< std::string, StrongPackPtr >(key,msg));
    }

    templatious::DynVPackFactory* getFact() {
        return _fact;
    }

    void addMesseagableWeak(const char* name,WeakMsgPtr weakRef) {
        Guard g(_mtx);
        assert( _messageableMapStrong.find(name) == _messageableMapStrong.end()
            && "Strong reference with same name exists." );

        _messageableMapWeak.insert(std::pair<std::string, WeakMsgPtr>(name,weakRef));
    }

    void addMesseagableStrong(const char* name,StrongMsgPtr strongRef) {
        Guard g(_mtx);

        auto wFind = _messageableMapWeak.find(name);
        if (wFind != _messageableMapWeak.end()) {
            // check if reference is alive, if it is, fail
            auto locked = wFind->second.lock();
            if (nullptr != locked) {
                assert( false && "Weak reference with same name exists." );
            } else {
                _messageableMapWeak.erase(name);
            }
        }

        _messageableMapStrong.insert(std::pair<std::string,StrongMsgPtr>(name,strongRef));
    }

    StrongMsgPtr getMesseagable(const char* name) {
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

    std::mutex& getMutex() const {
        return _mtx;
    }

private:
    typedef std::lock_guard< std::mutex > Guard;
    lua_State* _s;
    templatious::DynVPackFactory* _fact;
    mutable std::mutex _mtx;
    std::map< std::string, WeakMsgPtr > _messageableMapWeak;
    std::map< std::string, StrongMsgPtr > _messageableMapStrong;
    std::map< std::string, StrongPackPtr > _packMap;
};

void initDomain(LuaContext& ctx);

#endif /* end of include guard: DOMAIN_8UU5DBQ1 */

