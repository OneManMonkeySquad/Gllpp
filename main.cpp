
#include <gllpp/Gllpp.h>
#include <iostream>
#include <algorithm>
#include <chrono>

using namespace Gllpp;

class Stopwatch final {
public:

	using elapsed_resolution = std::chrono::microseconds;

	Stopwatch() {
		Reset();
	}

	void Reset() {
		reset_time = clock.now();
	}

	elapsed_resolution Elapsed() {
		return std::chrono::duration_cast<elapsed_resolution>(clock.now() - reset_time);
	}

private:

	std::chrono::high_resolution_clock clock;
	std::chrono::high_resolution_clock::time_point reset_time;
};

int main() {
	auto number = Capture<' ', '+'>();

	Parser grammar;
	grammar = ("A"_t + "B"_t) | "AB"_t;

	auto code = "AB";

	{
		Stopwatch stopwatch;

		auto parseResults = grammar.parse(code);

		const auto success = parseResults[0].is_success();
		std::cout << "success: " << success << std::endl;
		if (!success) {
			std::sort(parseResults.begin(), parseResults.end());

			const auto minSize = parseResults[0].trail.size();
			for (auto& parseResult : parseResults) {
				if (parseResult.trail.size() > minSize)
					break;

				std::cout << "  " << *parseResult.error << std::endl
					<< "    '" << parseResult.trail << "'" << std::endl;
			}
		}

		std::cout << "Took " << stopwatch.Elapsed().count() << "us" << std::endl;
	}

	system("pause");
	return 0;
}

