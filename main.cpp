
#include <gllpp/Gllpp.h>
#include <iostream>

int main()
{
	Parser grammar;

	auto function = "def"_t + Capture<'{'>() + "{"_t + "}"_t;
	function.set_name("[function]");

	auto strct = "struct"_t + Capture<'{'>() + "{"_t + "}"_t;
	strct.set_name("[struct]");

	auto topLevelDefinition = function | strct;

	grammar = SetLayout(topLevelDefinition + Optional(grammar), " \t\r\n");

	auto code = "def test {}\n"
		"struct cls {}";
	auto parseResults = grammar.parse_entry(code);
	for (auto& parseResult : parseResults) {
		std::cout << parseResult.type << " '" << parseResult.trail << "'" << std::endl;
	}

    return 0;
}

