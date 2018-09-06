
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "gllpp/Gllpp.h"

using namespace Gllpp;

TEST_CASE("Basic", "[factorial]") {
	Parser grammar;

	auto function = "def"_t + Capture<'{'>() + "{"_t + "}"_t;
	function.set_name("[function]");

	auto cls = "struct"_t + Capture<'{'>() + "{"_t + "}"_t;
	cls.set_name("[class]");

	auto topLevelDefinition = function | cls;

	grammar = SetLayout(topLevelDefinition + Optional(grammar), " \t\r\n");

	auto code = "def test {}\n"
		"struct cls {}";

	auto parseResults = grammar.parse(code);
	REQUIRE(parseResults.size() == 1);
	REQUIRE(parseResults[0].is_success());
}