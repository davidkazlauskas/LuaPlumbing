
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
        //_hndl->tryMatch(p);
    }

    int getA() const {
        return _outA;
    }

    void setA(int v) {
        _outA = v;
    }

private:
    typedef std::unique_ptr< templatious::VirtualMatchFunctor > Hndl;

    Hndl genHandler() {
        return SF::virtualMatchFunctorPtr(
            SF::virtualMatch<Msg::MsgA,int>(
                [=](Msg::MsgA,int res) {
                    this->_outA = res;
                }
            )
        );
    }

    ThreadGuard _g;
    int _outA;
    Hndl _hndl;
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
    const char* src =
        "runstuff = function()                                      "
        "    local msg = luaContext:namedMesseagable(\"someMsg\")   "
        "    luaContext:message(msg,VSig(\"msg_a\"),VInt(7))        "
        "end                                                        "
        "runstuff()                                                 ";
    luaL_dostring(s,src);

    auto hndl = getHandler();

    REQUIRE( hndl->getA() == 7 );
    hndl->setA(-1);
}
