#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <fstream>
#include <iostream>
#include <optional>

namespace Gllpp {
	class Trampoline;
	template<typename LT, typename RT>
	class Sequence;
	template<typename LT, typename RT>
	class Disjunction;





	struct ParserResult {
		std::string_view trail;
		std::optional<std::string> error;

		bool is_success() const {
			return !error;
		}

		bool operator<(const ParserResult& other) const {
			return trail.size() < other.trail.size();
		}
	};






	class Trampoline {
	public:
		Trampoline(std::string_view str)
			: _str(str) {
		}

		void add(std::function<void(Trampoline&)> f) {
			Work work;
			work.f = f;

			_work.push_back(work);
		}

		void run() {
			while (!_work.empty()) {
				auto work = _work.back();
				_work.pop_back();

				work.f(*this);
			}
		}

	private:
		struct Work {
			std::function<void(Trampoline&)> f;
		};

		std::vector<Work> _work;
		std::string_view _str;
	};




	class ParserBase {
	};

	template<typename T>
	class ComposableParser : public ParserBase {
	public:
		/// Match str with contained grammar. Return either a list of successes or failures.
		std::vector<ParserResult> parse(std::string str) const {
			Trampoline trampoline{ str };
			std::vector<ParserResult> successes, failures;

			static_cast<const T*>(this)->_chain(trampoline, {}, str, [&](Trampoline& trampoline, ParserResult result) {
				if (!result.trail.empty()) {
					ParserResult actualResult{ result.trail, "Tail left" };
					if (successes.empty()) {
						failures.push_back(actualResult);
					}
				}
				else if (result.is_success()) {
					successes.push_back(result);
					failures.clear();
				}
				else if (successes.empty()) {
					failures.push_back(result);
				}
			});

			trampoline.run();

			return !successes.empty() ? successes : failures;
		}

		template<typename F>
		void _gather(F f) const {
			f(*static_cast<const T*>(this));
		}

		template<typename OtherT>
		Sequence<T, OtherT> operator+(OtherT other) const {
			return { *static_cast<const T*>(this), other };
		}

		template<typename OtherT>
		Disjunction<T, OtherT> operator|(OtherT other) const {
			return { *static_cast<const T*>(this), other };
		}

	protected:
		static void skip_layout(std::string_view layout, std::string_view& str) {
			size_t i = 0;
			for (; i < str.size(); ++i) {
				if (layout.find_first_of(str[i]) == std::string::npos)
					break;
			}

			str = str.substr(i);
		}
	};

	class Parser : public ComposableParser<Parser> {
	public:
		Parser()
			: _wrapper(std::make_shared<SharedWrapper>()) {
		}

		template<typename P>
		Parser(P p)
			: Parser() {
			*this = p;
		}

		template<typename P>
		Parser& operator=(P p) {
			static_assert(std::is_base_of_v<ParserBase, P>);
			_wrapper->wrapper = std::make_unique<WrapperInstance<P>>(p);
			return *this;
		}

		template<typename F>
		void _chain(Trampoline& trampoline, std::string_view layout, std::string_view str, F f) const {
			if (_wrapper->wrapper == nullptr) {
				f(trampoline, ParserResult{ str, "Parser is null" });
				return;
			}

			_wrapper->wrapper->chain(trampoline, layout, str, f);
		}

	private:
		class IWrapper {
		public:
			virtual ~IWrapper() = default;

			virtual void chain(Trampoline& trampoline, const std::string_view layout, std::string_view str, std::function<void(Trampoline&, ParserResult)> f) const = 0;
		};

		template<typename P>
		class WrapperInstance final : public IWrapper {
		public:
			WrapperInstance(P p)
				: _parser(p) {
			}

			virtual void chain(Trampoline& trampoline, std::string_view layout, std::string_view str, std::function<void(Trampoline&, ParserResult)> f) const override {
				_parser._chain(trampoline, layout, str, f);
			}

		private:
			P _parser;
		};

		struct SharedWrapper {
			std::unique_ptr<IWrapper> wrapper;
		};

		std::shared_ptr<SharedWrapper> _wrapper;
	};


	template<typename P>
	class Layout : public ComposableParser<Layout<P>> {
	public:
		Layout(P p, std::string layout)
			: _p(p)
			, _layout(layout) {
			static_assert(std::is_base_of_v<ParserBase, P>);
		}

		template<typename F>
		void _chain(Trampoline& trampoline, const std::string_view layout, std::string_view str, F f) const {
			(void)layout;
			_p._chain(trampoline, _layout, str, f);
		}

	private:
		P _p;
		std::string _layout;
	};


	/// Set layout for all parsers inside the passed Parser p. Layout is a list of chars which are automatically removed after every Parser.
	template<typename P>
	Layout<P> layout(P p, std::string definition) {
		static_assert(std::is_base_of_v<ParserBase, P>);
		return { p, definition };
	}



	class Empty : public ComposableParser<Empty> {
	public:
		template<typename F>
		void _chain(Trampoline& trampoline, const std::string_view layout, std::string_view str, F f) const {
			f(trampoline, ParserResult{ str });
		}
	};




	namespace {
		template<char DELIMITER, char... TRAIL>
		struct CaptureDelimiterUtil {
			static bool equals(char c) {
				if (c == DELIMITER)
					return true;

				return CaptureDelimiterUtil<TRAIL...>::equals(c);
			}
		};

		template<char DELIMITER>
		struct CaptureDelimiterUtil<DELIMITER> {
			static bool equals(char c) {
				return c == DELIMITER;
			}
		};
	}

	template<char... DELIMITERS>
	class Capture : public ComposableParser<Capture<DELIMITERS...>> {
	public:
		template<typename F>
		void _chain(Trampoline& trampoline, const std::string_view layout, std::string_view str, F f) const {
			size_t numConsumed = 0;
			for (; numConsumed < str.size(); ++numConsumed) {
				const auto c = str[numConsumed];
				if (CaptureDelimiterUtil<DELIMITERS...>::equals(c))
					goto delimiter_found;
			}

		delimiter_found:
			const auto value = str.substr(0, numConsumed);

			if (value.empty()) {
				f(trampoline, ParserResult{ str, "Capture empty value" });
			}

			str = str.substr(numConsumed);
			skip_layout(layout, str);

			f(trampoline, ParserResult{ str });
			return;
		}
	};





	class Terminal : public ComposableParser<Terminal> {
	public:
		Terminal(std::string what)
			: _what(what) {
		}

		template<typename F>
		void _chain(Trampoline& trampoline, const std::string_view layout, std::string_view str, F f) const {
			if (str.size() < _what.size() || memcmp(str.data(), _what.data(), _what.size())) {
				f(trampoline, ParserResult{ str, "Terminal missing " + _what });
				return;
			}

			str = str.substr(_what.size());
			skip_layout(layout, str);

			f(trampoline, ParserResult{ str });
		}

	private:
		std::string _what;
	};





	template<typename LT, typename RT>
	class Sequence : public ComposableParser<Sequence<LT, RT>> {
	public:
		Sequence(LT lhs, RT rhs)
			: _lhs(lhs)
			, _rhs(rhs) {
			static_assert(std::is_base_of_v<ParserBase, LT>);
			static_assert(std::is_base_of_v<ParserBase, RT>);
		}

		template<typename F>
		void _chain(Trampoline& trampoline, const std::string_view layout, std::string_view str, F f) const {
			_lhs._chain(trampoline, layout, str, [layout, f, this](Trampoline& trampoline, ParserResult result) {
				if (!result.is_success()) {
					f(trampoline, result);
				}
				else {
					_rhs._chain(trampoline, layout, result.trail, f);
				}
			});
		}

	private:
		LT _lhs;
		RT _rhs;
	};




	template<typename LT, typename RT>
	class Disjunction : public ComposableParser<Disjunction<LT, RT>> {
	public:
		Disjunction(LT lhs, RT rhs)
			: _lhs(lhs)
			, _rhs(rhs) {
			static_assert(std::is_base_of_v<ParserBase, LT>);
			static_assert(std::is_base_of_v<ParserBase, RT>);
		}

		template<typename F>
		void _chain(Trampoline& trampoline, std::string_view layout, std::string_view str, F f) const {
			_gather([&](auto& parser) {
				trampoline.add([str, layout, f, &parser](Trampoline& trampoline) {
					parser._chain(trampoline, layout, str, f);
				});
			});
		}

		template<typename F>
		void _gather(F f) const {
			_lhs._gather(f);
			_rhs._gather(f);
		}

	private:
		LT _lhs;
		RT _rhs;
	};





	Terminal operator ""_t(const char* str, size_t size) {
		return std::string{ str, size };
	}

	template<typename P>
	Disjunction<P, Empty> optional(P p) {
		static_assert(std::is_base_of_v<ParserBase, P>);
		return { p, Empty() };
	}
}