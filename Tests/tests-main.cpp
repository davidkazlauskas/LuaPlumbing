
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <templatious/FullPack.hpp>
#include <templatious/detail/DynamicPackCreator.hpp>

#include "../plumbing.hpp"

TEMPLATIOUS_TRIPLET_STD;

struct Msg {
    struct MsgA {};
    struct MsgB {};
    struct MsgC {};
    struct MsgD {};
};

struct SomeHandler : public Messageable {
    SomeHandler() :
        _outA(-1),
        _outADbl(-1),
        _outABool(false),
        _hndl(genHandler()),
        _msgDInt(-1),
        _msgDDouble(-1),
        _msgDString("-1"),
        _msgDBool(false)
    {}

    void message(templatious::VirtualPack& p) override {
        _g.assertThread();
        _hndl->tryMatch(p);
    }

    void message(const std::shared_ptr<templatious::VirtualPack>& p) override {
        _cache.enqueue(p);
    }

    int getA() const {
        return _outA;
    }

    double getADbl() const {
        return _outADbl;
    }

    void setADbl(double d) {
        _outADbl = d;
    }

    void setA(int v) {
        _outA = v;
    }

    bool getABool() {
        return _outABool;
    }

    void setABool(bool val) {
        _outABool = val;
    }

    void setAStr(const char* str) {
        _outAStr = str;
    }

    const std::string& getAStr() const {
        return _outAStr;
    }

    void procAsync() {
        _g.assertThread();
        _cache.process([=](templatious::VirtualPack& p) {
            _hndl->tryMatch(p);
        });
    }
private:
    typedef std::unique_ptr< templatious::VirtualMatchFunctor > Hndl;

    Hndl genHandler() {
        return SF::virtualMatchFunctorPtr(
            SF::virtualMatch<Msg::MsgA,int>(
                [=](Msg::MsgA,int res) {
                    this->_outA = res;
                }
            ),
            SF::virtualMatch<Msg::MsgA,double>(
                [=](Msg::MsgA,double res) {
                    this->_outADbl = res;
                }
            ),
            SF::virtualMatch<Msg::MsgA,bool>(
                [=](Msg::MsgA,bool res) {
                    this->_outABool = res;
                }
            ),
            SF::virtualMatch<Msg::MsgA,std::string>(
                [=](Msg::MsgA,std::string& res) {
                    this->_outAStr = res;
                }
            ),
            SF::virtualMatch<Msg::MsgA,StrongMsgPtr>(
                [=](Msg::MsgA,StrongMsgPtr& res) {
                    auto vp = SF::vpack<Msg::MsgA,int>(
                        Msg::MsgA(),777
                    );
                    res->message(vp);
                }
            ),
            SF::virtualMatch<Msg::MsgB,int>(
                [=](Msg::MsgB,int& res) {
                    res = 77;
                }
            ),
            SF::virtualMatch<Msg::MsgC,int>(
                [=](Msg::MsgC,int& res) {
                    ++res;
                }
            ),
            SF::virtualMatch<Msg::MsgC,StrongPackPtr>(
                [=](Msg::MsgC,StrongPackPtr& res) {
                    _hndl->tryMatch(*res);
                }
            ),
            SF::virtualMatch<Msg::MsgD,StrongMsgPtr>(
                [=](Msg::MsgD,StrongPackPtr& res) {

                }
            )
        );
    }

    ThreadGuard _g;
    int _outA;
    double _outADbl;
    bool _outABool;
    std::string _outAStr;
    Hndl _hndl;
    MessageCache _cache;

    int _msgDInt;
    double _msgDDouble;
    std::string _msgDString;
    bool _msgDBool;
};

typedef templatious::TypeNodeFactory TNF;

#define ATTACH_NAMED_DUMMY(factory,name,type)   \
    factory.attachNode(name,TNF::makeDummyNode< type >(name))

std::shared_ptr< SomeHandler > getHandler() {
    static auto hndl = std::make_shared< SomeHandler >();
    return hndl;
}

templatious::DynVPackFactory getFactory() {
    templatious::DynVPackFactoryBuilder bld;
    LuaContext::registerPrimitives(bld);
    ATTACH_NAMED_DUMMY(bld,"msg_a",Msg::MsgA);
    ATTACH_NAMED_DUMMY(bld,"msg_b",Msg::MsgB);
    ATTACH_NAMED_DUMMY(bld,"msg_c",Msg::MsgC);
    ATTACH_NAMED_DUMMY(bld,"msg_d",Msg::MsgD);
    return bld.getFactory();
}

std::shared_ptr< LuaContext > produceContext() {
    auto ctx = LuaContext::makeContext();
    static auto fact = getFactory();
    ctx->setFactory(&fact);
    ctx->addMesseagableWeak("someMsg",getHandler());
    return ctx;
}

std::shared_ptr< LuaContext > s_ctx = nullptr;
std::shared_ptr< LuaContext > getContext() {
    return s_ctx;
}

TEST_CASE("basic_messaging_set","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    auto hndl = getHandler();
    hndl->setA(-1);

    const char* src =
        "runstuff = function()                                            "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")       "
        "    outRes = luaContext():message(msg,VSig(\"msg_a\"),VInt(7))   "
        "end                                                              "
        "runstuff()                                                       ";
    luaL_dostring(s,src);

    REQUIRE( hndl->getA() == 7 );

    ::lua_getglobal(s,"outRes");
    auto type = ::lua_type(s,-1);
    REQUIRE( type == LUA_TBOOLEAN );
    bool value = ::lua_toboolean(s,-1);
    REQUIRE( value == true );
}

TEST_CASE("basic_messaging_set_async","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    auto hndl = getHandler();
    hndl->setA(-1);

    const char* src =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    luaContext():messageAsync(msg,VSig(\"msg_a\"),VInt(8))   "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);

    REQUIRE( hndl->getA() == -1 );
    hndl->procAsync();
    REQUIRE( hndl->getA() == 8 );
}

TEST_CASE("basic_messaging_set_wcallback","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    auto hndl = getHandler();
    hndl->setA(-1);

    const char* src =
        "runstuff = function()                                            "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")       "
        "    outResB = luaContext():messageWCallback(msg,                 "
        "       function(out)                                             "
        "           local tree = out:values()                             "
        "           luaContext():message(                                 "
        "               msg,VSig(\"msg_a\"),VInt(tree._2))                "
        "       end,                                                      "
        "       VSig(\"msg_b\"),VInt(8))                                  "
        "end                                                              "
        "runstuff()                                                       ";
    luaL_dostring(s,src);
    REQUIRE( hndl->getA() == 77 );

    ::lua_getglobal(s,"outResB");
    auto type = ::lua_type(s,-1);
    REQUIRE( type == LUA_TBOOLEAN );
    bool value = ::lua_toboolean(s,-1);
    REQUIRE( value == true );
}

TEST_CASE("basic_messaging_set_async_wcallback","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    auto hndl = getHandler();
    hndl->setA(-1);

    const char* src =
        "runstuff = function()                                            "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")       "
        "    luaContext():messageAsyncWCallback(msg,                      "
        "       function(out)                                             "
        "           local tree = out:values()                             "
        "           luaContext():message(                                 "
        "               msg,VSig(\"msg_a\"),VInt(tree._2))                "
        "       end,                                                      "
        "       VSig(\"msg_b\"),VInt(8))                                  "
        "end                                                              "
        "runstuff()                                                       ";
    luaL_dostring(s,src);
    REQUIRE( hndl->getA() == -1 );

    // one to process first callback
    hndl->procAsync();
    REQUIRE( hndl->getA() == -1 );

    // another pass to process 2nd callback
    ctx->processMessages();
    REQUIRE( hndl->getA() == 77 );
}

TEST_CASE("basic_messaging_handler_self_send","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    auto hndl = getHandler();
    hndl->setA(-1);

    const char* src =
        "runstuff = function()                                            "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")       "
        "    local handler = luaContext():makeLuaHandler(                 "
        "       function(val)                                             "
        "           local values = val:vtree():values()                   "
        "           luaContext():message(msg,                             "
        "               VSig(\"msg_a\"),VInt(values._1))                  "
        "       end                                                       "
        "    )                                                            "
        "    luaContext():message(handler,VInt(777))                      "
        "end                                                              "
        "runstuff()                                                       ";
    luaL_dostring(s,src);
    REQUIRE( hndl->getA() == 777 );
}

TEST_CASE("basic_messaging_handler_self_send_mt","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    auto hndl = getHandler();
    hndl->setA(-1);

    const char* src =
        "runstuff = function()                                            "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")       "
        "    handler = luaContext():makeLuaHandler(                       "
        "       function(val)                                             "
        "           local values = val:vtree():values()                   "
        "           luaContext():message(msg,                             "
        "               VSig(\"msg_a\"),VInt(values._1))                  "
        "       end                                                       "
        "    )                                                            "
        "    luaContext():attachToProcessing(handler)                     "
        "    luaContext():messageAsync(handler,VInt(777))                 "
        "end                                                              "
        "runstuff()                                                       ";
    luaL_dostring(s,src);
    REQUIRE( hndl->getA() == -1 );
    ctx->processMessages();
    REQUIRE( hndl->getA() == 777 );
}

TEST_CASE("basic_messaging_handler_bench","[basic_messaging]") {
    // don't assert anything, just find time
    auto ctx = getContext();
    auto s = ctx->s();

    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                                            "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")       "
        "    local count = 0                                              "
        "    while count < 100000 do                                      "
        "       luaContext():messageWCallback(msg,                        "
        "           function(out) count = out:values()._2 end,            "
        "           VSig(\"msg_c\"),VInt(count))                          "
        "    end                                                          "
        "    return count                                                 "
        "end                                                              ";

    luaL_dostring(s,src);
    lua_getglobal(s,"runstuff");
    auto pre = std::chrono::high_resolution_clock::now();
    int outRes = lua_pcall(s,0,1,0);
    auto post = std::chrono::high_resolution_clock::now();
    REQUIRE( 0 == outRes );
    float res = lua_tonumber(s,-1);

    float reduced = std::fabs(res - 100000);
    REQUIRE( reduced < 0.00000001 );
    printf("Time taken for basic_messaging_handler_bench benchmark: %ld\n",
        std::chrono::duration_cast< std::chrono::milliseconds >(post - pre).count());
}

TEST_CASE("basic_messaging_primitive_double","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    auto hndl = getHandler();
    hndl->setA(-1);

    const char* src =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    luaContext():message(msg,VSig(\"msg_a\"),VDouble(7.7))   "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);

    REQUIRE( hndl->getADbl() == 7.7 );
}

TEST_CASE("basic_messaging_primitive_bool","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    auto hndl = getHandler();
    hndl->setABool(true);

    const char* src =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    luaContext():message(msg,VSig(\"msg_a\"),VBool(false))   "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);

    REQUIRE( hndl->getABool() == false );

    const char* src2 =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    luaContext():message(msg,VSig(\"msg_a\"),VBool(true))    "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src2);

    REQUIRE( hndl->getABool() == true );
}

TEST_CASE("basic_messaging_return_values","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    const char* src =
        "outRes = true                                                "
        "outResB = true                                               "
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    outRes = luaContext():message(msg,VInt(7))               "
        "    outResB = luaContext():messageWCallback(msg,             "
        "        function(val) end,VInt(7))                           "
        "end                                                          "
        "runstuff()                                                   ";

    luaL_dostring(s,src);

    ::lua_getglobal(s,"outResB");
    ::lua_getglobal(s,"outRes");

    auto typeA = ::lua_type(s,-1);
    auto typeB = ::lua_type(s,-2);

    REQUIRE( typeA == LUA_TBOOLEAN );
    REQUIRE( typeB == LUA_TBOOLEAN );

    bool valueA = ::lua_toboolean(s,-1);
    bool valueB = ::lua_toboolean(s,-2);

    REQUIRE( valueA == false );
    REQUIRE( valueB == false );
}

TEST_CASE("basic_messaging_async_return_values","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "outRes = true                                                "
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    luaContext():messageAsyncWError(msg,                     "
        "        function() outRes = false end,VInt(7))               "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);
    hndl->procAsync();
    ctx->processMessages();

    ::lua_getglobal(s,"outRes");

    auto typeA = ::lua_type(s,-1);

    REQUIRE( typeA == LUA_TBOOLEAN );

    bool valueA = ::lua_toboolean(s,-1);

    REQUIRE( valueA == false );
}

TEST_CASE("basic_messaging_async_wcallback_return_values","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "outRes = true                                                "
        "outResB = true                                               "
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    luaContext():messageAsyncWCallbackWError(msg,            "
        "        function(val) outResB = false end,                   "
        "        function() outRes = false end,VInt(7))               "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);
    hndl->procAsync();
    ctx->processMessages();

    ::lua_getglobal(s,"outResB");
    ::lua_getglobal(s,"outRes");

    auto typeA = ::lua_type(s,-1);
    auto typeB = ::lua_type(s,-2);

    REQUIRE( typeA == LUA_TBOOLEAN );
    REQUIRE( typeB == LUA_TBOOLEAN );

    bool valueA = ::lua_toboolean(s,-1);
    bool valueB = ::lua_toboolean(s,-2);

    REQUIRE( valueA == false );
    REQUIRE( valueB == true );
}

TEST_CASE("basic_messaging_async_wcallback_return_values_success","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "outRes = true                                                "
        "outResB = true                                               "
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    luaContext():messageAsyncWCallbackWError(msg,            "
        "        function(val) outResB = false end,                   "
        "        function() outRes = false end,                       "
        "        VSig(\"msg_a\"),VInt(7))                             "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);
    hndl->procAsync();
    ctx->processMessages();

    ::lua_getglobal(s,"outResB");
    ::lua_getglobal(s,"outRes");

    auto typeA = ::lua_type(s,-1);
    auto typeB = ::lua_type(s,-2);

    REQUIRE( typeA == LUA_TBOOLEAN );
    REQUIRE( typeB == LUA_TBOOLEAN );

    bool valueA = ::lua_toboolean(s,-1);
    bool valueB = ::lua_toboolean(s,-2);

    REQUIRE( valueA == true );
    REQUIRE( valueB == false );
}

TEST_CASE("basic_messaging_infer_double","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    hndl->setADbl(-1);

    const char* src =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    outRes = luaContext():message(msg,                       "
        "        VSig(\"msg_a\"),7.7)                                 "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);
    ::lua_getglobal(s,"outRes");

    bool value = ::lua_toboolean(s,-1);
    double diff = std::fabs(hndl->getADbl() - 7.7);

    REQUIRE( value == true );
    REQUIRE( diff < 0.00000001 );
}

TEST_CASE("basic_messaging_infer_bool","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    hndl->setABool(false);

    const char* src =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    outRes = luaContext():message(msg,                       "
        "        VSig(\"msg_a\"),true)                                "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);
    ::lua_getglobal(s,"outRes");

    bool value = ::lua_toboolean(s,-1);
    bool out = hndl->getABool();

    REQUIRE( value == true );
    REQUIRE( out == true );
}

TEST_CASE("basic_messaging_infer_string","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    hndl->setAStr("wait what");

    const char* src =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    outRes = luaContext():message(msg,                       "
        "        VSig(\"msg_a\"),'cholo')                             "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);
    ::lua_getglobal(s,"outRes");

    bool value = ::lua_toboolean(s,-1);
    auto& out = hndl->getAStr();

    REQUIRE( value == true );
    REQUIRE( out == "cholo" );
}

TEST_CASE("basic_messaging_infer_messeagable","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    hndl->setA(-1);

    const char* src =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    outRes = luaContext():message(msg,                       "
        "        VSig(\"msg_a\"),msg)                                 "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);
    ::lua_getglobal(s,"outRes");

    bool value = ::lua_toboolean(s,-1);
    int out = hndl->getA();

    REQUIRE( value == true );
    REQUIRE( out == 777 );
}

struct OnceProcessable : public Messageable {

    OnceProcessable() : _handler(genHandler()), _cnt(0) {}

    typedef std::unique_ptr< templatious::VirtualMatchFunctor >
        VmfPtr;

    void message(templatious::VirtualPack& p) {
        _handler->tryMatch(p);
    }

    void message(const StrongPackPtr& p) {}

    VmfPtr genHandler() {
        typedef GenericMesseagableInterface GMI;
        return SF::virtualMatchFunctorPtr(
            SF::virtualMatch<GMI::AttachItselfToMesseagable,StrongMsgPtr>(
                [=](GMI::AttachItselfToMesseagable,const StrongMsgPtr& wmsg) {
                    assert( nullptr != wmsg && "Can't attach, dead." );

                    std::function<bool()> func = [=]() {
                        ++this->_cnt;
                        return this->_cnt < 2;
                    };

                    auto p = SF::vpack<
                        GMI::InAttachToEventLoop,
                        std::function<bool()>
                    >(GMI::InAttachToEventLoop(),std::move(func));

                    wmsg->message(p);
                }
            )
        );
    }

    int getCount() const {
        return _cnt;
    }

private:
    VmfPtr _handler;
    int _cnt;
};

TEST_CASE("basic_messaging_once_attached","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    auto proc = std::make_shared< OnceProcessable >();
    ctx->addMesseagableWeak("onceProc",proc);

    const char* src =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"onceProc\")  "
        "    outRes = luaContext():attachToProcessing(msg)            "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);

    REQUIRE( 0 == proc->getCount() );
    ctx->processMessages();
    REQUIRE( 1 == proc->getCount() );
    ctx->processMessages();
    REQUIRE( 2 == proc->getCount() );
    ctx->processMessages();
    REQUIRE( 2 == proc->getCount() );
}

TEST_CASE("basic_messaging_order_of_callbacks","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "outRes = ''                                                  "
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    luaContext():messageAsyncWError(msg,                     "
        "        function() outRes = outRes .. 'tr' end,VInt(7))      "
        "    luaContext():messageAsyncWCallback(msg,                  "
        "        function() outRes = outRes .. 'ee' end,              "
        "        VSig('msg_a'),VInt(7))                               "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);
    hndl->procAsync();
    ctx->processMessages();

    ::lua_getglobal(s,"outRes");

    auto typeA = ::lua_type(s,-1);

    REQUIRE( typeA == LUA_TSTRING );

    std::string valueA = ::lua_tostring(s,-1);

    REQUIRE( valueA == "tree" );
}

TEST_CASE("basic_messaging_vpack_composition","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    hndl->setA(-1);
    const char* src =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMesseagable(\"someMsg\")   "
        "    luaContext():message(msg,                                "
        "        VSig('msg_c'),VPack(VSig('msg_a'),VInt(7)))          "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);

    REQUIRE( hndl->getA() == 7 );
}

TEST_CASE("lua_match_functor_get_function","[lua_match]") {
    auto ctx = getContext();
    auto s = ctx->s();

    const char* src =
        "runstuff = function()                           "
        "outRes = false                                  "
        "                                                "
        "local aFunc = function() outVar = 0 end         "
        "local bFunc = function() outVar = 1 end         "
        "                                                "
        "                                                "
        "local vm = VMatchFunctor.create(                "
        "    VMatch(aFunc,\"int\",\"double\"),           "
        "    VMatch(bFunc,\"int\")                       "
        ")                                               "
        "                                                "
        "local vtree = toValueTree(VInt(7))              "
        "                                                "
        "local outFunc = vm:getFunction(vtree.types)     "
        "                                                "
        "if (outFunc == bFunc) then                      "
        "    outRes = true                               "
        "end                                             "
        "                                                "
        "end                                             "
        "runstuff()                                      ";

    luaL_dostring(s,src);

    ::lua_getglobal(s,"outRes");
    bool value = ::lua_toboolean(s,-1);
    REQUIRE( value == true );
}

TEST_CASE("lua_match_functor_use_handler","[lua_match]") {
    auto ctx = getContext();
    auto s = ctx->s();

    const char* src =
        "runstuff = function()                             "
        "outRes = false                                    "
        "                                                  "
        "local aFunc = function(natPack,val)               "
        "    outVar = val:values()._2                      "
        "end                                               "
        "                                                  "
        "local bFunc = function(natPack,val)               "
        "    outVar = val:values()._2 * 10 + 7             "
        "end                                               "
        "                                                  "
        "local cFunc = function(natPack,val)               "
        "    outVar = -1                                   "
        "end                                               "
        "                                                  "
        "local vm = VMatchFunctor.create(                  "
        "    VMatch(cFunc,\"msg_b\",\"int\"),              "
        "    VMatch(aFunc,\"msg_a\",\"int\"),              "
        "    VMatch(bFunc,\"msg_a\",\"int\")               "
        ")                                                 "
        "                                                  "
        "outResM = false                                   "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMesseagable(\"someMsg\")     "
        "local hndl = ctx:makeLuaHandler(function(val)     "
        "    outResM = vm:tryMatch(val)                    "
        "end)                                              "
        "                                                  "
        "ctx:message(msg,VSig(\"msg_a\"),VMsg(hndl))       "
        "                                                  "
        "outRes = outResM and outVar == 777                "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    luaL_dostring(s,src);

    ::lua_getglobal(s,"outRes");
    bool value = ::lua_toboolean(s,-1);
    REQUIRE( value == true );
}

TEST_CASE("lua_mutate_packs_from_managed","[lua_mutate]") {

}

int main( int argc, char* const argv[] )
{
    auto ctx = produceContext();
    s_ctx = ctx;

    int result = Catch::Session().run( argc, argv );

    s_ctx = nullptr;
    ctx = nullptr;
    return result;
}
