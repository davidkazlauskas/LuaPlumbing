// All the domain logic is here.
// Notice, that while not knowing what
// Qt is or including single header of
// Qt we're able to fully manipulate
// the GUI as long as we specify the messages
// and GUI side implements them.

#include <templatious/FullPack.hpp>
#include <templatious/detail/DynamicPackCreator.hpp>

#include "domain.h"
#include "mainwindow_interface.h"

TEMPLATIOUS_TRIPLET_STD;

namespace {
    typedef templatious::TypeNodeFactory TNF;

    auto intNode = TNF::makePodNode<int>(
        [](void* ptr,const char* arg) {
            int* asInt = reinterpret_cast<int*>(ptr);
            new (ptr) int(std::atoi(arg));
        },
        [](const void* ptr,std::string& out) {
            out = std::to_string(
                *reinterpret_cast<const int*>(ptr));
        }
    );

    auto doubleNode = TNF::makePodNode<double>(
        [](void* ptr,const char* arg) {
            double* asInt = reinterpret_cast<double*>(ptr);
            new (ptr) double(std::atof(arg));
        },
        [](const void* ptr,std::string& out) {
            out = std::to_string(
                *reinterpret_cast<const double*>(ptr));
        }
    );

    auto stringNode = TNF::makeFullNode<std::string>(
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

    typedef std::shared_ptr< templatious::VirtualPack > VPackPtr;

    auto vpackNode = TNF::makeFullNode<VPackPtr>(
        // here, we assume we receive pointer
        // to exact copy of the pack
        [](void* ptr,const char* arg) {
            new (ptr) VPackPtr(
                *reinterpret_cast<const VPackPtr*>(arg)
            );
        },
        [](void* ptr) {
            VPackPtr* vpPtr = reinterpret_cast<VPackPtr*>(ptr);
            vpPtr->~shared_ptr();
        },
        [](const void* ptr,std::string& out) {
            // write pointer
            out.clear();
            const char* atPtr = reinterpret_cast<const char*>(&ptr);
            TEMPLATIOUS_REPEAT(sizeof(void*)) {
                out += '7';
            }
            // no 0 termination
            out += '5';
            // write to raw data
            TEMPLATIOUS_0_TO_N(i,sizeof(void*)) {
                out[i] = atPtr[i];
            }
        }
    );

#define ATTACH_NAMED_DUMMY(factory,name,type)   \
    factory.attachNode(name,TNF::makeDummyNode< type >(name))

    templatious::DynVPackFactory buildTypeIndex() {

        templatious::DynVPackFactoryBuilder bld;
        bld.attachNode("int",intNode);
        bld.attachNode("double",doubleNode);
        bld.attachNode("string",stringNode);
        bld.attachNode("vpack",vpackNode);

        typedef MainWindowInterface MWI;
        ATTACH_NAMED_DUMMY( bld, "mwnd_insetprog", MWI::InSetProgress );
        ATTACH_NAMED_DUMMY( bld, "mwnd_insetlabel", MWI::InSetStatusText );
        ATTACH_NAMED_DUMMY( bld, "mwnd_querylabel", MWI::QueryLabelText );

        return bld.getFactory();
    }
}

static auto vFactory = buildTypeIndex();

struct ConstCharTreeNode {
    ConstCharTreeNode(const ConstCharTreeNode&) = delete;

    ConstCharTreeNode(ConstCharTreeNode&& other) :
        _key(other._key),
        _value(other._value),
        _children(std::move(other._children))
    {}

    ConstCharTreeNode(const char* key,const char* value) :
        _key(key), _value(value) {}

    ConstCharTreeNode& operator=(ConstCharTreeNode&& other)
    {
        _key = std::move(other._key);
        _value = std::move(other._value);
        _children = std::move(other._children);
        return *this;
    }

    bool isLeaf() const {
        return SA::size(_children) == 0;
    }

    bool isRoot() const {
        return _key == "[root]"
            && _value == "[root]"
            && SA::size(_children) == 2;
    }

    VPackPtr toVPack(const templatious::DynVPackFactory* fact) const {
        assert( isRoot() &&
            "Only root node can be converted to virtual pack.");

        auto& typeTree = _children[0]._key == "types" ?
            _children[0] : _children[1];

        auto& valueTree = _children[1]._key == "values" ?
            _children[1] : _children[0];

        assert( typeTree._key == "types" );
        assert( valueTree._key == "values" );

        // max 32 vpacks total in tree
        templatious::StaticBuffer< VPackPtr, 32 > buf;
        auto vec = buf.getStaticVector();

        // only one vpack will be made
        const char* types[1];
        const char* values[1];

        representAsPtr(fact,typeTree,valueTree,0,types,values,vec);
        VPackPtr outPtr = *reinterpret_cast<const VPackPtr*>(values[0]);
        return outPtr;
    }

    static ConstCharTreeNode packToTree(
        const templatious::DynVPackFactory* fact,
        templatious::VirtualPack& p)
    {
        ConstCharTreeNode result("[root]","[root]");
        result.push(ConstCharTreeNode("types",""));
        result.push(ConstCharTreeNode("values",""));
        auto& typeTree = result._children[0];
        auto& valueTree = result._children[1];

        packToTreeRec(typeTree,valueTree,p,fact);
        return result;
    }

    void pushTypeTree(lua_State* state) {
        assert( isRoot() && "This can be used only on root node." );

        auto& typeTree = _children[0]._key == "types" ?
            _children[0] : _children[1];
        assert( typeTree._key == "types" );

        ::lua_createtable(state,SA::size(typeTree._children),0);
        typeTree.pushTree(state,-1);
    }

    void pushValueTree(lua_State* state) {
        assert( isRoot() && "This can be used only on root node." );

        auto& valueTree = _children[1]._key == "values" ?
            _children[1] : _children[0];
        assert( valueTree._key == "values" );

        ::lua_createtable(state,SA::size(valueTree._children),0);
        valueTree.pushTree(state,-1);
    }

    const std::vector<ConstCharTreeNode>& children() const {
        return _children;
    }

    void push(ConstCharTreeNode&& child) {
        SA::add(_children,std::move(child));
    }

    void sort() {
        SM::sortS(_children,
            [](const ConstCharTreeNode& lhs,ConstCharTreeNode& rhs) {
                return lhs._key < rhs._key;
            }
        );
        TEMPLATIOUS_FOREACH(auto& i,_children) { i.sort(); }
    }

private:
    void pushTree(lua_State* state,int idx) const {
        TEMPLATIOUS_FOREACH(auto& i,_children) {
            ::lua_pushstring(state,i._key.c_str());
            if (i.isLeaf()) {
                ::lua_pushstring(state,i._value.c_str());
            } else {
                ::lua_createtable(state,SA::size(i._children),0);
                i.pushTree(state,-1);
            }
            // two items on top of index
            ::lua_settable(state,idx - 2);
        }
    }

    static void representAsPtr(
        const templatious::DynVPackFactory* fact,
        const ConstCharTreeNode& sisterTypeNode,
        const ConstCharTreeNode& sisterValueNode,
        int idx,const char** type,const char** value,
        templatious::StaticVector<VPackPtr>& buffer)
    {
        static const char* VPNAME = "vpack";
        if (sisterValueNode.isLeaf()) {
            type[idx] = sisterTypeNode._value.c_str();
            value[idx] = sisterValueNode._value.c_str();
        } else {
            const char* types[32];
            const char* values[32];
            SM::traverse<true>(
                [&](int idx,
                    const ConstCharTreeNode& typeNode,
                    const ConstCharTreeNode& valNode)
                {
                    representAsPtr(
                        fact,typeNode,valNode,
                        idx,types,values,buffer);
                },
                sisterTypeNode.children(),
                sisterValueNode.children()
            );
            int size = SA::size(sisterTypeNode.children());
            auto p = fact->makePack(size,types,values);
            // extend lifetime
            SA::add(buffer,p);

            type[idx] = VPNAME;
            value[idx] = reinterpret_cast<const char*>(
                std::addressof(buffer.top()));
        }
    }

    static void packToTreeRec(
        ConstCharTreeNode& typeNode,
        ConstCharTreeNode& valueNode,
        templatious::VirtualPack& p,
        const templatious::DynVPackFactory* fact)
    {
        templatious::TNodePtr outInf[32];
        auto outVec = fact->serializePack(p,outInf);
        int outSize = SA::size(outVec);

        std::string keyBuf;
        TEMPLATIOUS_0_TO_N(i,outSize) {
            keyBuf = "_";
            keyBuf += std::to_string(i + 1);
            const char* assocName = fact->associatedName(outInf[i]);
            if (vpackNode != outInf[i]) {
                typeNode.push(ConstCharTreeNode(
                    keyBuf.c_str(),assocName));
                valueNode.push(ConstCharTreeNode(
                    keyBuf.c_str(),outVec[i].c_str()));
            } else {
                typeNode.push(ConstCharTreeNode(keyBuf.c_str(),""));
                valueNode.push(ConstCharTreeNode(keyBuf.c_str(),""));
                auto& tnodeRef = typeNode._children.back();
                auto& vnodeRef = valueNode._children.back();
                VPackPtr* vpptr = nullptr;
                ::memcpy(&vpptr,outVec[i].data(),sizeof(void*));
                packToTreeRec(tnodeRef,vnodeRef,**vpptr,fact);
            }
        }
    }

    std::string _key;
    std::string _value;
    std::vector<ConstCharTreeNode> _children;
};

struct LuaCallback : public Messageable {

};

void getCharNodes(lua_State* state,int tblidx,
    ConstCharTreeNode& outVect)
{
    int iter = 0;

    const int KEY = -2;
    const int VAL = -1;

    std::string outKey,outVal;
    ::lua_pushnil(state);
    int trueIdx = tblidx - 1;
    while (0 != ::lua_next(state,trueIdx)) {
        switch (::lua_type(state,KEY)) {
            case LUA_TSTRING:
                outKey = ::lua_tostring(state,KEY);
                break;
            case LUA_TNUMBER:
                outKey = std::to_string(::lua_tonumber(state,KEY));
                break;
        }

        bool isTable = false;
        switch(::lua_type(state,VAL)) {
            case LUA_TNUMBER:
                outVal = std::to_string(::lua_tonumber(state,VAL));
                break;
            case LUA_TSTRING:
                outVal = ::lua_tostring(state,VAL);
                break;
            case LUA_TTABLE:
                outVal = "[table]";
                isTable = true;
                break;
        }
        ConstCharTreeNode node(outKey.c_str(),outVal.c_str());
        if (isTable) {
            getCharNodes(state,VAL,node);
        }
        outVect.push(std::move(node));

        ::lua_pop(state,1);
    }
}

int getStringArray(
    lua_State* state,
    int index,
    templatious::StaticVector<const char*>& arr)
{
    const int KEY = -2;
    const int VAL = -1;

    lua_pushnil(state);
    int count = 0;

    int trueIndex = index - 1;

    while (0 != ::lua_next(state,trueIndex)) {
        ++count;
        const char* curVal = lua_tostring(state,VAL);
        SA::add(arr,curVal);

        int num = lua_tointeger(state,KEY);
        assert( num == count && "Index mismatch." );

        lua_pop(state,1);
    }
    return SA::size(arr);
}

template <class Callback>
std::shared_ptr< templatious::VirtualPack >
getVPack(lua_State* state,int typeIdx,int valIdx,Callback&& c)
{
    templatious::StaticBuffer< const char*, 64 > buffer;
    auto types = buffer.getStaticVector(32);
    auto values = buffer.getStaticVector();

    int sizeValue = getStringArray(state,valIdx,values);
    int sizeTypes = getStringArray(state,typeIdx,types);

    assert( sizeValue == sizeTypes && "Types and values don't match in size." );

    return c(sizeValue,types.rawBegin(),values.rawBegin());
}

// -1 -> values
// -2 -> types
// -3 -> name
// -4 -> context
int registerPack(lua_State* state) {
    LuaContext* ctx = reinterpret_cast<LuaContext*>(::lua_touserdata(state,-4));
    const char* name = reinterpret_cast<const char*>(::lua_tostring(state,-3));
    auto fact = ctx->getFact();
    auto p = getVPack(state,-2,-1,
        [=](int size,const char** types,const char** values) {
            return fact->makePack(size,types,values);
        });
    ctx->indexPack(name,p);
    return 0;
}

// -1 -> values
// -2 -> types
// -3 -> mesg name
// -4 -> context
int sendPack(lua_State* state) {
    LuaContext* ctx = reinterpret_cast<LuaContext*>(::lua_touserdata(state,-4));
    const char* name = reinterpret_cast<const char*>(::lua_tostring(state,-3));

    const int BACK_ARGS = 0;

    auto wptr = ctx->getMesseagable(name);
    auto locked = wptr.lock();
    if (nullptr == locked) {
        return BACK_ARGS;
    }

    auto fact = ctx->getFact();
    auto p = getVPack(state,-2,-1,
        [=](int size,const char** types,const char** values) {
            return fact->makePack(size,types,values);
        });

    locked->message(p);

    return BACK_ARGS;
}

// -1 -> table
int constructPack(lua_State* state) {
    ConstCharTreeNode node("[root]","[root]");
    getCharNodes(state,-1,node);
    node.sort();

    auto factPtr = std::addressof(vFactory);
    auto p = node.toVPack(factPtr);

    auto outVec = factPtr->serializePack(*p);
    node.pushTypeTree(state);
    node.pushValueTree(state);

    return 2;
}

struct Unrefer {
    Unrefer(lua_State* state,int ref,int table)
        : _state(state), _ref(ref), _tbl(table) {}

    ~Unrefer() {
        ::luaL_unref(_state,_tbl,_ref);
    }

private:
    lua_State* _state;
    int _ref;
    int _tbl;
};

// -1 -> function
// -2 -> values
// -3 -> types
// -4 -> mesg name
// -5 -> context
int sendPackAsync(lua_State* state) {
    LuaContext* ctx = reinterpret_cast<LuaContext*>(::lua_touserdata(state,-5));
    const char* name = ::lua_tostring(state,-4);

    int funcRef = ::luaL_ref(state,LUA_REGISTRYINDEX);

    const int BACK_ARGS = 0;

    auto wptr = ctx->getMesseagable(name);
    auto locked = wptr.lock();
    if (nullptr == locked) {
        return BACK_ARGS;
    }

    // function popped
    auto fact = ctx->getFact();
    auto p = getVPack(state,-2,-1,
        [=](int size,const char** types,const char** values) {
            return fact->makePackCustomWCallback<
                templatious::VPACK_SYNCED
            >(size,types,values,
                [=](const templatious::detail::DynamicVirtualPackCore& core) {
                    Unrefer guard(state,LUA_REGISTRYINDEX,funcRef);

                    templatious::TNodePtr outArr[32];
                    auto outSer = fact->serializeDynamicCore(core,outArr);
                    typedef std::lock_guard< std::mutex > Guard;
                    Guard g(ctx->getMutex());

                    lua_createtable(state,size,0);

                    std::string buf;
                    SM::traverse<true>([&](int idx,const std::string& val) {
                        buf = "_";
                        buf += std::to_string(idx + 1); // tuple like accessor
                        lua_pushstring(state,buf.c_str());
                        if (outArr[idx] == intNode) {
                            lua_pushnumber(state,*core.get<int>(idx));
                        } else if (outArr[idx] == doubleNode) {
                            lua_pushnumber(state,*core.get<double>(idx));
                        } else {
                            lua_pushstring(state,outSer[idx].c_str());
                        }
                        lua_settable(state,-3);
                    },outSer);

                    lua_rawgeti(state,LUA_REGISTRYINDEX,funcRef);
                    lua_pushvalue(state,-2);
                    lua_pcall(state,1,0,0);
                }
            );
        });

    locked->message(p);

    return BACK_ARGS;
}

void testBlock() {
    auto ptr = SF::vpackPtr<int,int>(3,4);
    auto p = SF::vpack<int,std::string,VPackPtr>(7,"7",ptr);
    auto outTree = ConstCharTreeNode::packToTree(&vFactory,p);

    volatile int stop = 0;
    ++stop;
}

void initDomain(LuaContext& ctx) {
    testBlock();

    auto s = ctx.s();
    luaL_openlibs(s);
    ctx.regFunction("nat_registerPack",&registerPack);
    ctx.regFunction("nat_sendPack",&sendPack);
    ctx.regFunction("nat_sendPackAsync",&sendPackAsync);
    ctx.regFunction("nat_constructPack",&constructPack);
    ctx.setFactory(std::addressof(vFactory));

    bool success = luaL_dofile(s,"main.lua") == 0;
    if (!success) {
        printf("%s\n", lua_tostring(s, -1));
    }
    assert( success );

    lua_getglobal(s,"initDomain");
    lua_pushlightuserdata(s,std::addressof(ctx));
    lua_pcall(s,1,0,0);
}
