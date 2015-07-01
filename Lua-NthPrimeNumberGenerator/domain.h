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
        MessageableWeak,
        VTreeItself,
    };

    VTree(const VTree&) = delete;
    VTree(VTree&& other) :
        _type(other._type),
        _key(std::move(other._key)),
        _ptr(other._ptr)
    {
        other._ptr = nullptr;
    }

    VTree(const char* key,const char* ptr) :
        _type(Type::StdString),
        _key(key),
        _ptr(new std::string(ptr))
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
            default:
                assert( false && "HUH?" );
                break;
        }
    }

    Type getType() const { return _type; }

    const std::string& getString() const {
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

    const std::string& getKey() const {
        return _key;
    }

private:
    Type _type;
    std::string _key;
    void* _ptr;
};

void getCharNodes(lua_State* state,int tblidx,
    std::vector< VTree >& outVect);

struct LuaContext {
    LuaContext() :
        _fact(nullptr),
        _s(luaL_newstate()),
        _thisId(std::this_thread::get_id())
    {}

    lua_State* s() const { return _s; }

    std::unique_ptr< VTree > makeTreeFromTable(lua_State* state,int idx) {
        assertThread();

        std::vector< VTree > nodes;

        getCharNodes(state,idx,nodes);

        return std::unique_ptr< VTree >(new VTree("[root]",std::move(nodes)));
    }

    StrongPackPtr treeToPack(VTree& tree) {
        assertThread();

        templatious::StaticBuffer< StrongPackPtr, 32 > bufPack;
        templatious::StaticBuffer< WeakMsgPtr, 32 > msgPack;

        auto vPack = bufPack.getStaticVector();
        auto vMsg = msgPack.getStaticVector();

        const char* types[32];
        const char* values[32];

        StackDump d(types,values,vPack,vMsg);

        return nullptr;
    }

    void setFactory(templatious::DynVPackFactory* fact) {
        _fact = fact;
    }

    void regFunction(const char* name,lua_CFunction func) {
        lua_register(_s,name,func);
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

    void addMesseagableStrong(const char* name,const StrongMsgPtr& strongRef) {
        Guard g(_mtx);
        assert( _messageableMapStrong.find(name) == _messageableMapStrong.end()
            && "Strong reference with same name exists." );

        _messageableMapWeak.insert(std::pair<std::string, WeakMsgPtr>(name,strongRef));
    }

    void addMesseagableWeak(const char* name,const WeakMsgPtr& weakRef) {
        Guard g(_mtx);
        assert( _messageableMapStrong.find(name) == _messageableMapStrong.end()
            && "Strong reference with same name exists." );

        _messageableMapWeak.insert(std::pair<std::string, WeakMsgPtr>(name,weakRef));
    }

private:
    void assertThread() {
        assert( _thisId == std::this_thread::get_id()
            && "Wrong thread, DUMBO" );
    }

    struct StackDump {
        StackDump(
            const char** types,const char** values,
            templatious::StaticVector< StrongPackPtr >& bufVPtr,
            templatious::StaticVector< WeakMsgPtr >& bufWMsg
        ) :
            _types(types), _values(values),
            _bufferVPtr(bufVPtr), _bufferWMsg(bufWMsg)
        {}

        StackDump(const StackDump&) = delete;
        StackDump(StackDump&&) = delete;

        const char** _types;
        const char** _values;
        templatious::StaticVector< StrongPackPtr >& _bufferVPtr;
        templatious::StaticVector< WeakMsgPtr >& _bufferWMsg;
    };

    template <class T>
    StrongPackPtr toVPack(VTree& tree,T&& creator,StackDump& d) {
        assert( tree.getType() == VTree::Type::VTreeItself
            && "Expecting tree here, milky..." );

        auto& children = tree.getInnerTree();

        auto& typeTree = children[0].getKey() == "types" ?
            children[0] : children[1];

        auto& valueTree = children[1].getKey() == "values" ?
            children[1] : children[0];

        assert( typeTree.getKey() == "types" );
        assert( valueTree.getKey() == "values" );

        auto& typeTreeInner = typeTree.getInnerTree();
        prepChildren(typeTreeInner,valueTree.getInnerTree(),d);

        return creator(typeTreeInner.size(),d._types,d._values);
    }

    void prepChildren(
        std::vector< VTree >& typeTree,
        std::vector< VTree >& valueTree,
        StackDump& d);

    void representAsPtr(
        std::vector< VTree >& typeTree,std::vector< VTree >& valueTree,
        StackDump& d);

    typedef std::lock_guard< std::mutex > Guard;

    std::map< std::string, WeakMsgPtr   > _messageableMapWeak;
    std::map< std::string, StrongMsgPtr > _messageableMapStrong;

    std::mutex _mtx;
    templatious::DynVPackFactory* _fact;
    lua_State* _s;
    std::thread::id _thisId;
};

void initDomain(const std::shared_ptr< LuaContext >& ptr);

#endif /* end of include guard: DOMAIN_8UU5DBQ1 */

