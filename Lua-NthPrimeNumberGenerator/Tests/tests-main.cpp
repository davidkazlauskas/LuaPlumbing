
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <templatious/detail/DynamicPackCreator.hpp>

#include "../plumbing.hpp"

struct Msg {
    struct MsgA {};
    struct MsgB {};
    struct MsgC {};
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
}
