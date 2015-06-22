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

    WeakMsgPtr getMesseagable(const char* name) {
        Guard g(_mtx);
        auto iter = _messageableMap.find(name);
        if (iter == _messageableMap.end()) {
            return WeakMsgPtr();
        }
        return iter->second;
    }

    void addMesseagable(const char* name,WeakMsgPtr weakRef) {
        Guard g(_mtx);
        _messageableMap.insert(std::pair<std::string, WeakMsgPtr>(name,weakRef));
    }

    std::mutex& getMutex() const {
        return _mtx;
    }

private:
    typedef std::lock_guard< std::mutex > Guard;
    lua_State* _s;
    templatious::DynVPackFactory* _fact;
    mutable std::mutex _mtx;
    std::map< std::string, WeakMsgPtr > _messageableMap;
    std::map< std::string, StrongPackPtr > _packMap;
};

void initDomain(LuaContext& ctx);

#endif /* end of include guard: DOMAIN_8UU5DBQ1 */

