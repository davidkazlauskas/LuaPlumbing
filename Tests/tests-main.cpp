
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
    struct MsgDSI {};
    struct MsgDSD {};
    struct MsgDSB {};
    struct MsgDSS {};
    struct MsgDSM {};
    struct MsgDMI {};
    struct MsgDMD {};
    struct MsgDMB {};
    struct MsgDMS {};
    struct MsgDMM {};
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

// tired of writing getters.
// why wrote them in first place?
//private:

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
            SF::virtualMatch<Msg::MsgA,StrongMsgPtr,bool>(
                [=](Msg::MsgA,StrongMsgPtr& msg,bool res) {
                    auto vp = SF::vpack< bool >( res );
                    msg->message(vp);
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
            SF::virtualMatch<Msg::MsgC,StrongMsgPtr,bool>(
                [=](Msg::MsgC,StrongMsgPtr& res,bool& outNull) {
                    outNull = res == nullptr;
                }
            ),
            SF::virtualMatch<Msg::MsgC,double>(
                [=](Msg::MsgC,double& res) {
                    res += 0.07;
                }
            ),
            SF::virtualMatch<Msg::MsgC,StrongMsgPtr,StrongMsgPtr>(
                [=](Msg::MsgC,const StrongMsgPtr& a,StrongMsgPtr& b) {
                    b = a;
                }
            ),
            SF::virtualMatch<Msg::MsgDSI,StrongMsgPtr>(
                [=](Msg::MsgDSI,StrongMsgPtr& output) {
                    auto res = SF::vpack< int >(_msgDInt);
                    output->message(res);
                    _msgDInt = res.fGet<0>();
                }
            ),
            SF::virtualMatch<Msg::MsgDSD,StrongMsgPtr>(
                [=](Msg::MsgDSD,StrongMsgPtr& output) {
                    auto res = SF::vpack< double >(_msgDDouble);
                    output->message(res);
                    _msgDDouble = res.fGet<0>();
                }
            ),
            SF::virtualMatch<Msg::MsgDSS,StrongMsgPtr>(
                [=](Msg::MsgDSS,StrongMsgPtr& output) {
                    auto res = SF::vpack< std::string >(_msgDString);
                    output->message(res);
                    _msgDString = res.fGet<0>();
                }
            ),
            SF::virtualMatch<Msg::MsgDSB,StrongMsgPtr>(
                [=](Msg::MsgDSB,StrongMsgPtr& output) {
                    auto res = SF::vpack< bool >(_msgDBool);
                    output->message(res);
                    _msgDBool = res.fGet<0>();
                }
            ),
            SF::virtualMatch<Msg::MsgDSM,StrongMsgPtr>(
                [=](Msg::MsgDSM,StrongMsgPtr& output) {
                    auto res = SF::vpack< StrongMsgPtr >(_msgDMsg);
                    output->message(res);
                    _msgDMsg = res.fGet<0>();
                }
            ),
            SF::virtualMatch<Msg::MsgDMI,StrongMsgPtr>(
                [=](Msg::MsgDMI,StrongMsgPtr& output) {
                    auto res = SF::vpackPtrWCallback< int >(
                        [&](const TEMPLATIOUS_VPCORE<int>& pack) {
                            _msgDInt = pack.fGet<0>();
                        },
                        _msgDInt);
                    output->message(res);
                }
            ),
            SF::virtualMatch<Msg::MsgDMD,StrongMsgPtr>(
                [=](Msg::MsgDMD,StrongMsgPtr& output) {
                    auto res = SF::vpackPtrWCallback< double >(
                        [&](const TEMPLATIOUS_VPCORE< double >& pack) {
                            _msgDDouble = pack.fGet<0>();
                        },
                        _msgDDouble
                    );
                    output->message(res);
                }
            ),
            SF::virtualMatch<Msg::MsgDMS,StrongMsgPtr>(
                [=](Msg::MsgDMS,StrongMsgPtr& output) {
                    auto res = SF::vpackPtrWCallback< std::string >(
                        [&](const TEMPLATIOUS_VPCORE< std::string >& pack) {
                            _msgDString = pack.fGet<0>();
                        },
                        _msgDString
                    );
                    output->message(res);
                }
            ),
            SF::virtualMatch<Msg::MsgDMB,StrongMsgPtr>(
                [=](Msg::MsgDMB,StrongMsgPtr& output) {
                    auto res = SF::vpackPtrWCallback< bool >(
                        [&](const TEMPLATIOUS_VPCORE< bool >& pack) {
                            _msgDBool = pack.fGet<0>();
                        },
                        _msgDBool
                    );
                    output->message(res);
                }
            ),
            SF::virtualMatch<Msg::MsgDMM,StrongMsgPtr>(
                [=](Msg::MsgDMM,StrongMsgPtr& output) {
                    auto res = SF::vpackPtrWCallback< StrongMsgPtr >(
                        [&](const TEMPLATIOUS_VPCORE< StrongMsgPtr >& msg) {
                            _msgDMsg = msg.fGet<0>();
                        },
                        _msgDMsg);
                    output->message(res);
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
    StrongMsgPtr _msgDMsg;
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
    ATTACH_NAMED_DUMMY(bld,"msg_dSI",Msg::MsgDSI);
    ATTACH_NAMED_DUMMY(bld,"msg_dSD",Msg::MsgDSD);
    ATTACH_NAMED_DUMMY(bld,"msg_dSB",Msg::MsgDSB);
    ATTACH_NAMED_DUMMY(bld,"msg_dSS",Msg::MsgDSS);
    ATTACH_NAMED_DUMMY(bld,"msg_dSM",Msg::MsgDSM);
    ATTACH_NAMED_DUMMY(bld,"msg_dMI",Msg::MsgDMI);
    ATTACH_NAMED_DUMMY(bld,"msg_dMD",Msg::MsgDMD);
    ATTACH_NAMED_DUMMY(bld,"msg_dMB",Msg::MsgDMB);
    ATTACH_NAMED_DUMMY(bld,"msg_dMS",Msg::MsgDMS);
    ATTACH_NAMED_DUMMY(bld,"msg_dMM",Msg::MsgDMM);
    return bld.getFactory();
}

std::shared_ptr< LuaContext > produceContext() {
    auto ctx = LuaContext::makeContext();
    static auto fact = getFactory();
    ctx->setFactory(&fact);
    ctx->addMessageableWeak("someMsg",getHandler());
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")       "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")       "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")       "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")       "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")       "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")       "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
        "    luaContext():message(msg,VSig(\"msg_a\"),VBool(false))   "
        "end                                                          "
        "runstuff()                                                   ";
    luaL_dostring(s,src);

    REQUIRE( hndl->getABool() == false );

    const char* src2 =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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

TEST_CASE("basic_messaging_infer_messageable","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    hndl->setA(-1);

    const char* src =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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
        typedef GenericMessageableInterface GMI;
        return SF::virtualMatchFunctorPtr(
            SF::virtualMatch<GMI::AttachItselfToMessageable,StrongMsgPtr>(
                [=](GMI::AttachItselfToMessageable,const StrongMsgPtr& wmsg) {
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
    ctx->addMessageableWeak("onceProc",proc);

    const char* src =
        "runstuff = function()                                        "
        "    local msg = luaContext():namedMessageable(\"onceProc\")  "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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
        "    local msg = luaContext():namedMessageable(\"someMsg\")   "
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
        "local msg = ctx:namedMessageable(\"someMsg\")     "
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

TEST_CASE("lua_mutate_packs_from_managed_int_ST","[lua_mutate]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                             "
        "                                                  "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMessageable(\"someMsg\")     "
        "local handler = ctx:makeLuaMatchHandler(          "
        "    VMatch(                                       "
        "        function(natpack)                         "
        "            natpack:setSlot(1,VInt(7))            "
        "        end,                                      "
        "        \"int\"                                   "
        "    )                                             "
        ")                                                 "
        "                                                  "
        "ctx:message(msg,VSig(\"msg_dSI\"),VMsg(handler))  "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    hndl->_msgDInt = -1;
    luaL_dostring(s,src);
    REQUIRE( hndl->_msgDInt == 7 );
}

TEST_CASE("lua_mutate_packs_from_managed_double_ST","[lua_mutate]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                             "
        "                                                  "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMessageable(\"someMsg\")     "
        "local handler = ctx:makeLuaMatchHandler(          "
        "    VMatch(                                       "
        "        function(natpack)                         "
        "            natpack:setSlot(1,VDouble(7.7))       "
        "        end,                                      "
        "        \"double\"                                "
        "    )                                             "
        ")                                                 "
        "                                                  "
        "ctx:message(msg,VSig(\"msg_dSD\"),VMsg(handler))  "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    hndl->_msgDDouble = -1;
    luaL_dostring(s,src);
    double diff = std::abs(hndl->_msgDDouble - 7.7);
    REQUIRE( diff < 0.00000001 );
}

TEST_CASE("lua_mutate_packs_from_managed_string_ST","[lua_mutate]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                             "
        "                                                  "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMessageable(\"someMsg\")     "
        "local handler = ctx:makeLuaMatchHandler(          "
        "    VMatch(                                       "
        "        function(natpack)                         "
        "            natpack:setSlot(1,VString(\"moo\"))   "
        "        end,                                      "
        "        \"string\"                                "
        "    )                                             "
        ")                                                 "
        "                                                  "
        "ctx:message(msg,VSig(\"msg_dSS\"),VMsg(handler))  "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    hndl->_msgDString = "-1";
    luaL_dostring(s,src);
    REQUIRE( hndl->_msgDString == "moo" );
}

TEST_CASE("lua_mutate_packs_from_managed_bool_ST","[lua_mutate]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                             "
        "                                                  "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMessageable(\"someMsg\")     "
        "local handler = ctx:makeLuaMatchHandler(          "
        "    VMatch(                                       "
        "        function(natpack)                         "
        "            natpack:setSlot(1,VBool(true))        "
        "        end,                                      "
        "        \"bool\"                                  "
        "    )                                             "
        ")                                                 "
        "                                                  "
        "ctx:message(msg,VSig(\"msg_dSB\"),VMsg(handler))  "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    hndl->_msgDBool = false;
    luaL_dostring(s,src);
    REQUIRE( hndl->_msgDBool == true );
}

TEST_CASE("lua_mutate_packs_from_managed_vmsg_ST","[lua_mutate]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                             "
        "                                                  "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMessageable(\"someMsg\")     "
        "local handler = ctx:makeLuaMatchHandler(          "
        "    VMatch(                                       "
        "        function(natpack)                         "
        "            natpack:setSlot(1,VMsg(msg))          "
        "        end,                                      "
        "        \"vmsg_raw_strong\"                       "
        "    )                                             "
        ")                                                 "
        "                                                  "
        "ctx:message(msg,VSig(\"msg_dSM\"),VMsg(handler))  "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    hndl->_msgDMsg = nullptr;
    luaL_dostring(s,src);
    REQUIRE( hndl->_msgDMsg == hndl );
    hndl->_msgDMsg = nullptr;
}

TEST_CASE("lua_mutate_packs_from_managed_int_MT","[lua_mutate]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                             "
        "                                                  "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMessageable(\"someMsg\")     "
        "local handler = ctx:makeLuaMatchHandler(          "
        "    VMatch(                                       "
        "        function(natpack)                         "
        "            natpack:setSlot(1,VInt(7))            "
        "        end,                                      "
        "        \"int\"                                   "
        "    )                                             "
        ")                                                 "
        "                                                  "
        "ctx:message(msg,VSig(\"msg_dMI\"),VMsg(handler))  "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    hndl->_msgDInt = -1;
    luaL_dostring(s,src);
    ctx->processMessages();
    REQUIRE( hndl->_msgDInt == 7 );
}

TEST_CASE("lua_mutate_packs_from_managed_double_MT","[lua_mutate]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                             "
        "                                                  "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMessageable(\"someMsg\")     "
        "local handler = ctx:makeLuaMatchHandler(          "
        "    VMatch(                                       "
        "        function(natpack)                         "
        "            natpack:setSlot(1,VDouble(7.7))       "
        "        end,                                      "
        "        \"double\"                                "
        "    )                                             "
        ")                                                 "
        "                                                  "
        "ctx:message(msg,VSig(\"msg_dMD\"),VMsg(handler))  "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    hndl->_msgDDouble = -1;
    luaL_dostring(s,src);
    ctx->processMessages();
    double diff = std::abs(hndl->_msgDDouble - 7.7);
    REQUIRE( diff < 0.00000001 );
}

TEST_CASE("lua_mutate_packs_from_managed_string_MT","[lua_mutate]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                             "
        "                                                  "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMessageable(\"someMsg\")     "
        "local handler = ctx:makeLuaMatchHandler(          "
        "    VMatch(                                       "
        "        function(natpack)                         "
        "            natpack:setSlot(1,VString(\"moo\"))   "
        "        end,                                      "
        "        \"string\"                                "
        "    )                                             "
        ")                                                 "
        "                                                  "
        "ctx:message(msg,VSig(\"msg_dMS\"),VMsg(handler))  "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    hndl->_msgDString = "-1";
    luaL_dostring(s,src);
    ctx->processMessages();
    REQUIRE( hndl->_msgDString == "moo" );
}

TEST_CASE("lua_mutate_packs_from_managed_bool_MT","[lua_mutate]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                             "
        "                                                  "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMessageable(\"someMsg\")     "
        "local handler = ctx:makeLuaMatchHandler(          "
        "    VMatch(                                       "
        "        function(natpack)                         "
        "            natpack:setSlot(1,VBool(true))        "
        "        end,                                      "
        "        \"bool\"                                  "
        "    )                                             "
        ")                                                 "
        "                                                  "
        "ctx:message(msg,VSig(\"msg_dMB\"),VMsg(handler))  "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    hndl->_msgDBool = false;
    luaL_dostring(s,src);
    ctx->processMessages();
    REQUIRE( hndl->_msgDBool == true );
}

TEST_CASE("lua_mutate_packs_from_managed_vmsg_MT","[lua_mutate]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                             "
        "                                                  "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMessageable(\"someMsg\")     "
        "local handler = ctx:makeLuaMatchHandler(          "
        "    VMatch(                                       "
        "        function(natpack)                         "
        "            natpack:setSlot(1,VMsg(msg))          "
        "        end,                                      "
        "        \"vmsg_raw_strong\"                       "
        "    )                                             "
        ")                                                 "
        "                                                  "
        "ctx:message(msg,VSig(\"msg_dMM\"),VMsg(handler))  "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    hndl->_msgDMsg = nullptr;
    luaL_dostring(s,src);
    ctx->processMessages();
    REQUIRE( hndl->_msgDMsg == hndl );
    hndl->_msgDMsg = nullptr;
}

TEST_CASE("lua_null_messageable","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                             "
        "                                                  "
        "outVal = false                                    "
        "                                                  "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMessageable(\"someMsg\")     "
        "                                                  "
        "local out = ctx:messageRetValues(msg,             "
        "    VSig(\"msg_c\"),VMsg(nil),VBool(false))       "
        "outVal = out._3                                   "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    luaL_dostring(s,src);

    ::lua_getglobal(s,"outVal");
    auto type = ::lua_type(s,-1);
    REQUIRE( type == LUA_TBOOLEAN );
    bool value = ::lua_toboolean(s,-1);
    REQUIRE( true == value );
}

TEST_CASE("lua_double_ret_type","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                             "
        "                                                  "
        "local ctx = luaContext()                          "
        "local msg = ctx:namedMessageable(\"someMsg\")     "
        "                                                  "
        "outVal = -1                                       "
        "local out = ctx:messageRetValues(msg,             "
        "    VSig(\"msg_c\"),VDouble(7.7))                 "
        "outVal = out._2                                   "
        "                                                  "
        "end                                               "
        "runstuff()                                        ";

    luaL_dostring(s,src);

    ::lua_getglobal(s,"outVal");
    auto type = ::lua_type(s,-1);
    REQUIRE( type == LUA_TNUMBER );
    double value = ::lua_tonumber(s,-1);
    double diff = std::fabs(value - 7.77);
    REQUIRE( diff < 0.00000001 );
}

TEST_CASE("lua_msg_messageable_equality","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                                  "
        "    local ctx = luaContext()                           "
        "    local msgA = ctx:namedMessageable(\"someMsg\")     "
        "    local msgB = ctx:namedMessageable(\"someMsg\")     "
        "    local msgNil = VMsgNil()                           "
        "                                                       "
        "    outResA = messageablesEqual(msgA,msgB)             "
        "    outResB = messageablesEqual(msgA,msgNil)           "
        "end                                                    "
        "runstuff()                                             ";

    luaL_dostring(s,src);
    ::lua_getglobal(s,"outResA");
    auto typeA = ::lua_type(s,-1);
    REQUIRE( LUA_TBOOLEAN == typeA );

    ::lua_getglobal(s,"outResB");
    auto typeB = ::lua_type(s,-1);
    REQUIRE( LUA_TBOOLEAN == typeB );

    bool a = ::lua_toboolean(s,-2);
    bool b = ::lua_toboolean(s,-1);
    REQUIRE( true == a );
    REQUIRE( false == b );
}

TEST_CASE("lua_msg_messageable_asreturn","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                                  "
        "    local ctx = luaContext()                           "
        "    local msgA = ctx:namedMessageable(\"someMsg\")     "
        "                                                       "
        "    local out = ctx:messageRetValues(msgA,             "
        "        VSig(\"msg_c\"),VMsg(msgA),VMsg(nil))._3       "
        "                                                       "
        "    outRes = messageablesEqual(msgA,out)               "
        "end                                                    "
        "runstuff()                                             ";

    luaL_dostring(s,src);
    ::lua_getglobal(s,"outRes");
    auto type = ::lua_type(s,-1);
    REQUIRE( LUA_TBOOLEAN == type );

    bool res = ::lua_toboolean(s,-1);
    REQUIRE( true == res );
}

TEST_CASE("lua_msg_catch_boolean","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                                            "
        "    outRes = true                                                "
        "    local msg = luaContext():namedMessageable(\"someMsg\")       "
        "    local handler = luaContext():makeLuaHandler(                 "
        "       function(val)                                             "
        "           local values = val:vtree():values()                   "
        "           outRes = values._1                                    "
        "       end                                                       "
        "    )                                                            "
        "    luaContext():message(msg,                                    "
        "       VSig(\"msg_a\"),VMsg(handler),VBool(false))               "
        "end                                                              "
        "runstuff()                                                       ";

    luaL_dostring(s,src);
    ::lua_getglobal(s,"outRes");
    auto type = ::lua_type(s,-1);
    REQUIRE( LUA_TBOOLEAN == type );

    bool res = ::lua_toboolean(s,-1);
    REQUIRE( false == res );
}

TEST_CASE("lua_weak_msg_lock","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                                            "
        "    outRes = false                                               "
        "    local handler = luaContext():makeLuaHandler(                 "
        "       function(val)                                             "
        "           local values = val:vtree():values()                   "
        "           outRes = values._1                                    "
        "       end                                                       "
        "    )                                                            "
        "    local weakRef = handler:getWeak()                            "
        "    local locked = weakRef:lockPtr()                             "
        "    outRes = messageablesEqual(locked,handler)                   "
        "end                                                              "
        "runstuff()                                                       ";

    luaL_dostring(s,src);
    ::lua_getglobal(s,"outRes");

    auto typeA = ::lua_type(s,-1);
    REQUIRE( LUA_TBOOLEAN == typeA );

    bool b = ::lua_toboolean(s,-1);
    REQUIRE( true == b );
}

TEST_CASE("lua_weak_msg_collect","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    auto hndl = getHandler();

    const char* src =
        "runstuff = function()                                            "
        "    outRes = false                                               "
        "    local outWeak = nil                                          "
        "    local innerScope = function()                                "
        "       local handler = luaContext():makeLuaHandler(              "
        "           function(val)                                         "
        "               local values = val:vtree():values()               "
        "               outRes = values._1                                "
        "           end                                                   "
        "       )                                                         "
        "       outWeak = handler:getWeak()                               "
        "    end                                                          "
        "    innerScope()                                                 "
        "    innerScope = nil                                             "
        "    collectgarbage('collect')                                    "
        "    local locked = outWeak:lockPtr()                             "
        "    outRes = locked == nil                                       "
        "end                                                              "
        "runstuff()                                                       ";

    luaL_dostring(s,src);
    ::lua_getglobal(s,"outRes");

    auto typeA = ::lua_type(s,-1);
    REQUIRE( LUA_TBOOLEAN == typeA );

    bool b = ::lua_toboolean(s,-1);
    REQUIRE( true == b );
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
