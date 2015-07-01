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

private:
    typedef std::lock_guard< std::mutex > Guard;
    std::mutex _mtx;
    std::vector< StrongPackPtr > _queue;
};

struct LuaContext {

};

#endif /* end of include guard: DOMAIN_8UU5DBQ1 */

