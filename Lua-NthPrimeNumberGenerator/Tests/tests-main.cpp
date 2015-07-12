
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
    SomeHandler() : _outA(-1), _hndl(genHandler()) {}

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

    void setA(int v) {
        _outA = v;
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
            SF::virtualMatch<Msg::MsgB,int>(
                [=](Msg::MsgB,int& res) {
                    res = 77;
                }
            )
        );
    }

    ThreadGuard _g;
    int _outA;
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
        "runstuff = function()                                      "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")   "
        "    luaContext:message(msg,VSig(\"msg_a\"),VInt(7))        "
        "end                                                        "
        "runstuff()                                                 ";
    luaL_dostring(s,src);

    REQUIRE( hndl->getA() == 7 );
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
        "    luaContext:messageWCallback(msg,                           "
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
