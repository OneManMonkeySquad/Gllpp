This repo is for prototyping, production ready code coming later.

    Parser grammar;

    auto function = "def"_t + Capture<'{'>() + "{"_t + "}"_t;
    auto cls = "struct"_t + Capture<'{'>() + "{"_t + "}"_t;
    auto topLevelDefinition = (function | cls) + optional(grammar);
    grammar = layout(topLevelDefinition, " \t\r\n");

    auto code = "def test {}\n"
        "struct cls {}";

    auto parseResults = grammar.parse(code);
    REQUIRE(parseResults.size() == 1);
    REQUIRE(parseResults[0].is_success());

![Example Output](ExampleOutput.png)
