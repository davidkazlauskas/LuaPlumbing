
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../plumbing.hpp"

std::shared_ptr< LuaContext > getContext() {
    static auto ctx = LuaContext::makeContext();
    return ctx;
}

TEST_CASE("basic_messaging_set","[basic_messaging]") {

}
