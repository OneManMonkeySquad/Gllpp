
#include <gllpp/Gllpp.h>
#include <iostream>

int main()
{
	Parser grammar;

	auto function = "def"_t + Capture<'{'>() + "{"_t + "}"_t;
	function.set_name("[function]");

	auto cls = "struct"_t + Capture<'{'>() + "{"_t + "}"_t;
	cls.set_name("[class]");

	auto topLevelDefinition = function | cls;
		

	grammar = topLevelDefinition + Optional(grammar);

	auto parseResults = grammar.parse("deftest{}structcls{}");
	for (auto& parseResult : parseResults) {
		std::cout << parseResult.type << " '" << parseResult.trail << "'" << std::endl;
	}

    return 0;
}

