
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <templatious/FullPack.hpp>
#include <templatious/detail/DynamicPackCreator.hpp>

#include "../plumbing.hpp"

TEMPLATIOUS_TRIPLET_STD;

struct Msg {
    struct MsgA {};
    struct MsgB {};
    struct MsgC {};
};

struct SomeHandler : public Messageable {
    SomeHandler() : _outA(-1), _outADbl(-1), _outABool(false), _hndl(genHandler()) {}

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
            SF::virtualMatch<Msg::MsgB,int>(
                [=](Msg::MsgB,int& res) {
                    res = 77;
                }
            ),
            SF::virtualMatch<Msg::MsgC,int>(
                [=](Msg::MsgC,int& res) {
                    ++res;
                }
            )
        );
    }

    ThreadGuard _g;
    int _outA;
    double _outADbl;
    bool _outABool;
    Hndl _hndl;
    MessageCache _cache;
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
    return bld.getFactory();
}

std::shared_ptr< LuaContext > getContext() {
    static auto ctx = LuaContext::makeContext();
    static auto fact = getFactory();
    ctx->setFactory(&fact);
    ctx->addMesseagableWeak("someMsg",getHandler());
    return ctx;
}

TEST_CASE("basic_messaging_set","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    auto hndl = getHandler();
    hndl->setA(-1);

    const char* src =
        "runstuff = function()                                          "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")       "
        "    outRes = luaContext:message(msg,VSig(\"msg_a\"),VInt(7))   "
        "end                                                            "
        "runstuff()                                                     ";
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
        "runstuff = function()                                      "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")   "
        "    luaContext:messageAsync(msg,VSig(\"msg_a\"),VInt(8))   "
        "end                                                        "
        "runstuff()                                                 ";
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
        "runstuff = function()                                          "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")       "
        "    outResB = luaContext:messageWCallback(msg,                 "
        "       function(out)                                           "
        "           local tree = out:values()                           "
        "           luaContext:message(                                 "
        "               msg,VSig(\"msg_a\"),VInt(tree._2))              "
        "       end,                                                    "
        "       VSig(\"msg_b\"),VInt(8))                                "
        "end                                                            "
        "runstuff()                                                     ";
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
        "runstuff = function()                                          "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")       "
        "    luaContext:messageAsyncWCallback(msg,                      "
        "       function(out)                                           "
        "           local tree = out:values()                           "
        "           luaContext:message(                                 "
        "               msg,VSig(\"msg_a\"),VInt(tree._2))              "
        "       end,                                                    "
        "       VSig(\"msg_b\"),VInt(8))                                "
        "end                                                            "
        "runstuff()                                                     ";
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
        "runstuff = function()                                          "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")       "
        "    local handler = luaContext:makeLuaHandler(                 "
        "       function(val)                                           "
        "           local values = val:vtree():values()                 "
        "           luaContext:message(msg,                             "
        "               VSig(\"msg_a\"),VInt(values._1))                "
        "       end                                                     "
        "    )                                                          "
        "    luaContext:message(handler,VInt(777))                      "
        "end                                                            "
        "runstuff()                                                     ";
    luaL_dostring(s,src);
    REQUIRE( hndl->getA() == 777 );
}

TEST_CASE("basic_messaging_handler_self_send_mt","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    auto hndl = getHandler();
    hndl->setA(-1);

    const char* src =
        "runstuff = function()                                          "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")       "
        "    local handler = luaContext:makeLuaHandler(                 "
        "       function(val)                                           "
        "           local values = val:vtree():values()                 "
        "           luaContext:message(msg,                             "
        "               VSig(\"msg_a\"),VInt(values._1))                "
        "       end                                                     "
        "    )                                                          "
        "    luaContext:attachToProcessing(handler)                     "
        "    luaContext:messageAsync(handler,VInt(777))                 "
        "end                                                            "
        "runstuff()                                                     ";
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
        "runstuff = function()                                          "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")       "
        "    local count = 0                                            "
        "    while count < 100000 do                                    "
        "       luaContext:messageWCallback(msg,                        "
        "           function(out) count = out:values()._2 end,          "
        "           VSig(\"msg_c\"),VInt(count))                        "
        "    end                                                        "
        "    return count                                               "
        "end                                                            ";

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
        "runstuff = function()                                      "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")   "
        "    luaContext:message(msg,VSig(\"msg_a\"),VDouble(7.7))   "
        "end                                                        "
        "runstuff()                                                 ";
    luaL_dostring(s,src);

    REQUIRE( hndl->getADbl() == 7.7 );
}

TEST_CASE("basic_messaging_primitive_bool","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    auto hndl = getHandler();
    hndl->setABool(true);

    const char* src =
        "runstuff = function()                                      "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")   "
        "    luaContext:message(msg,VSig(\"msg_a\"),VBool(false))   "
        "end                                                        "
        "runstuff()                                                 ";
    luaL_dostring(s,src);

    REQUIRE( hndl->getABool() == false );

    const char* src2 =
        "runstuff = function()                                      "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")   "
        "    luaContext:message(msg,VSig(\"msg_a\"),VBool(true))    "
        "end                                                        "
        "runstuff()                                                 ";
    luaL_dostring(s,src2);

    REQUIRE( hndl->getABool() == true );
}

TEST_CASE("basic_messaging_return_values","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();

    const char* src =
        "outRes = true                                              "
        "outResB = true                                             "
        "runstuff = function()                                      "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")   "
        "    outRes = luaContext:message(msg,VInt(7))               "
        "    outResB = luaContext:messageWCallback(msg,             "
        "        function(val) end,VInt(7))                         "
        "end                                                        "
        "runstuff()                                                 ";

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
