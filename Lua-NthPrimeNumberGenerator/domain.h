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
        Double,
        Int,
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

    double getDouble() const {
        assert( _type == Type::Double && "Wrong type, dumbo." );
        return _double;
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
            case Type::Double:
            case Type::Int:
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
    };
};

void getCharNodes(lua_State* state,int tblidx,
    std::vector< VTree >& outVect);

struct LuaContext : public Messageable {
    LuaContext();

    lua_State* s() const { return _s; }

    std::unique_ptr< VTree > makeTreeFromTable(lua_State* state,int idx) {
        assertThread();

        std::vector< VTree > nodes;

        getCharNodes(state,idx,nodes);

        return std::unique_ptr< VTree >(new VTree("[root]",std::move(nodes)));
    }

    template <class Maker>
    StrongPackPtr treeToPack(VTree& tree,Maker&& m) {
        assertThread();

        templatious::StaticBuffer< StrongPackPtr, 32 > bufPack;
        templatious::StaticBuffer< WeakMsgPtr, 32 > msgPack;

        auto vPack = bufPack.getStaticVector();
        auto vMsg = msgPack.getStaticVector();

        StackDump d(vPack,vMsg);

        return this->toVPack(tree,std::forward<Maker>(m),d);
    }

    VTree packToTree(const templatious::VirtualPack& pack) {
        typedef std::vector< VTree > TreeVec;
        VTree root("[root]",TreeVec());
        auto& rootTreeVec = root.getInnerTree();
        rootTreeVec.emplace_back("types",TreeVec());
        rootTreeVec.emplace_back("values",TreeVec());

        this->packToTreeRec(
            rootTreeVec[0],rootTreeVec[1],pack,_fact);
        return root;
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

    const templatious::DynVPackFactory* getFact() const {
        return _fact;
    }

    void assertThread() {
        _tg.assertThread();
    }

    void message(templatious::VirtualPack& p) override {
        assert( false && "Not implemented yet." );
    }

    void message(StrongPackPtr p) override {
        assert( false && "Not implemented yet." );
    }

private:
    struct StackDump {
        StackDump(
            templatious::StaticVector< StrongPackPtr >& bufVPtr,
            templatious::StaticVector< WeakMsgPtr >& bufWMsg
        ) :
            _bufferVPtr(bufVPtr), _bufferWMsg(bufWMsg)
        {}

        StackDump(const StackDump&) = delete;
        StackDump(StackDump&&) = delete;

        templatious::StaticVector< StrongPackPtr >& _bufferVPtr;
        templatious::StaticVector< WeakMsgPtr >& _bufferWMsg;
    };

    static void packToTreeRec(
        VTree& typeNode,VTree& valueNode,
        const templatious::VirtualPack& pack,
        const templatious::DynVPackFactory* fact);

    template <class T>
    StrongPackPtr toVPack(
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

        auto& typeTreeInner = typeTree.getInnerTree();
        int size = prepChildren(typeTree,valueTree,types,values,d);

        return creator(size,types,values);
    }

    int prepChildren(
        VTree& typeTree,
        VTree& valueTree,
        const char** types,
        const char** values,
        StackDump& d);

    void representAsPtr(
        VTree& typeTree,VTree& valueTree,
        int idx,const char** type,const char** value,
        StackDump& d);

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
};

void initDomain(const std::shared_ptr< LuaContext >& ptr);

#endif /* end of include guard: DOMAIN_8UU5DBQ1 */

