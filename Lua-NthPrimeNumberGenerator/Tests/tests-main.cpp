
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
    SomeHandler() : _hndl(genHandler()) {}

    void message(templatious::VirtualPack& p) override {
        _hndl->tryMatch(p);
    }

private:
    typedef std::unique_ptr< templatious::VirtualMatchFunctor > Hndl;

    Hndl genHandler() {
        return SF::virtualMatchFunctorPtr(
            SF::virtualMatch<Msg::MsgA,int>(
                [](Msg::MsgA,int res) {
                }
            )
        );
    }

    Hndl _hndl;
};

typedef templatious::TypeNodeFactory TNF;

#define ATTACH_NAMED_DUMMY(factory,name,type)   \
    factory.attachNode(name,TNF::makeDummyNode< type >(name))

templatious::DynVPackFactory getFactory() {
    templatious::DynVPackFactoryBuilder bld;
    ATTACH_NAMED_DUMMY(bld,"msg_a",Msg::MsgA);
    ATTACH_NAMED_DUMMY(bld,"msg_b",Msg::MsgB);
    ATTACH_NAMED_DUMMY(bld,"msg_c",Msg::MsgC);
    return bld.getFactory();
}

std::shared_ptr< LuaContext > getContext() {
    static auto ctx = LuaContext::makeContext();
    static auto fact = getFactory();
    ctx->setFactory(&fact);
    return ctx;
}

TEST_CASE("basic_messaging_set","[basic_messaging]") {
    auto ctx = getContext();
    auto s = ctx->s();
    luaL_dostring(s,"a = 2");
}