    #include <gllpp/Gllpp.h>
    #include <iostream>
    
    Lexer l;
	Parser grammar;

    auto function = "def"_t + Capture() + "{"_t + "}"_t;
    function.set_name("[function]");

    auto cls = "class"_t + Capture() + "{"_t + "}"_t;
    cls.set_name("[class]");

    auto topLevelDefinition = function | cls;
    grammar = topLevelDefinition + (grammar | Empty());

    auto parseResults = grammar.parse(lexer, "def test { } class cls { }");
    for (auto& parseResult : parseResults) {
        std::cout << parseResult.type << " '" << parseResult.trail << "'" << std::endl;
    }

![Example Output](ExampleOutput.png)