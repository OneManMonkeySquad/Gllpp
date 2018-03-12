#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <fstream>
#include <iostream>

class Trampoline;
template<typename LT, typename RT>
class Sequence;
template<typename LT, typename RT>
class Disjunction;






enum class ResultType
{
	Success,
	Failure
};

std::ostream& operator<< (std::ostream& s, ResultType result)
{
	switch (result)
	{
	case ResultType::Success: s << "Success"; break;
	case ResultType::Failure: s << "Failure"; break;
	}
	return s;
}



struct ParserResult
{
	ResultType type;
	std::string trail;
};



class GraphvizNode
{
public:
	GraphvizNode(Trampoline& trampoline, std::string name, std::string_view str)
		: _trampoline(trampoline)
		, _name(name)
		, _str(str)
	{
		if (_name.empty())
			return;

		_prevStack = _topStack;
		_topStack = this;
	}

	~GraphvizNode()
	{
		if (_name.empty())
			return;

		_topStack = _prevStack;
	}

	void emit(ResultType type) const;

	const std::string& get_name() const
	{
		return _name;
	}

	const std::string_view& get_str() const
	{
		return _str;
	}

	static GraphvizNode* get_top()
	{
		return _topStack;
	}

private:
	static GraphvizNode* _topStack;
	GraphvizNode* _prevStack;

	Trampoline& _trampoline;
	std::string _name;
	std::string_view _str;
};
GraphvizNode* GraphvizNode::_topStack = nullptr;







class Trampoline
{
public:
	Trampoline(std::string_view str)
		: _str(str)
		, _graph("graph.dot")
	{
		_graph << "digraph {\n"
			<< "    rankdir=LR;\n";
	}

	~Trampoline()
	{
		_graph << "}";
	}

	void add(std::function<void(Trampoline&)> f)
	{
		Work work;
		work.f = f;

		auto topNode = GraphvizNode::get_top();
		if (topNode != nullptr)
		{
			work.graphName = GraphvizNode::get_top()->get_name();
			work.graphStr = GraphvizNode::get_top()->get_str();
		}

		_work.push_back(work);
	}

	void run()
	{
		while (!_work.empty())
		{
			auto work = _work.back();
			_work.pop_back();

			GraphvizNode ls(*this, work.graphName, work.graphStr);

			work.f(*this);

			_graph << "\n\n\n";
		}
	}

	void startGraph(std::string name, std::string_view str, ResultType type)
	{
		const auto offset = !str.empty() ? str.data() - _str.data() : _str.size();

		const auto nodeName = std::to_string(offset) + ": " + name;
		_graph << "    \"initial\" -> \"" << nodeName << "\"\n"
			<< "    \"" << nodeName << "\" [color=" << (type == ResultType::Success ? "green" : "red") << ", penwidth=5]\n"
			<< "\n";
	}

	void continueGraph(std::string prevName, std::string_view prevStr, std::string name, std::string_view str, ResultType type)
	{
		const auto offset = !str.empty() ? str.data() - _str.data() : _str.size();
		const auto prevOffset = !prevStr.empty() ? prevStr.data() - _str.data() : _str.size();

		const auto nodeName = std::to_string(offset) + ": " + name;
		const auto prevNodeName = std::to_string(prevOffset) + ": " + prevName;
		_graph << "    \"" << prevNodeName << "\" -> \"" << nodeName << "\"\n"
			<< "    \"" << nodeName << "\" [color=" << (type == ResultType::Success ? "green" : "red") << ", penwidth=5]\n"
			<< "\n";
	}

private:
	struct Work
	{
		std::function<void(Trampoline&)> f;
		std::string graphName;
		std::string_view graphStr;
	};

	std::vector<Work> _work;
	std::string_view _str;

	std::ofstream _graph;
};


void GraphvizNode::emit(ResultType type) const
{
	if (_name.empty())
		return;

	if (_prevStack == nullptr)
	{
		_trampoline.startGraph(_name, _str, type);
	}
	else
	{
		_trampoline.continueGraph(_prevStack->_name, _prevStack->_str, _name, _str, type);
	}
}



class ParserBase
{
public:
	void set_name(std::string name)
	{
		_name = name;
	}

	const std::string& get_name() const
	{
		return _name;
	}

protected:
	std::string _name;
};

template<typename T>
class ComposableParser : public ParserBase
{
public:
	std::vector<ParserResult> parse(std::string str) const
	{
		Trampoline trampoline(str);
		std::vector<ParserResult> successes, failures;

		((T*)this)->parse_impl(trampoline, str, [&failures, &successes](Trampoline& trampoline, ResultType r, std::string_view trail)
		{
			const auto isSuccess = r == ResultType::Success && trail.empty();

			GraphvizNode ls(trampoline, isSuccess ? "SUCCESS" : "FAILURE", trail);

			if (isSuccess)
			{
				ls.emit(ResultType::Success);
				successes.push_back({ ResultType::Success, std::string(trail) });
			}
			else
			{
				ls.emit(ResultType::Failure);
				failures.push_back({ ResultType::Failure, std::string(trail) });
			}
		});

		trampoline.run();

		return !successes.empty() ? successes : failures;
	}

	template<typename F>
	void gather(F f) const
	{
		f(*static_cast<const T*>(this));
	}

	template<typename OtherT>
	Sequence<T, OtherT> operator+(OtherT other) const
	{
		return { *static_cast<const T*>(this), other };
	}

	template<typename OtherT>
	Disjunction<T, OtherT> operator|(OtherT other) const
	{
		return { *static_cast<const T*>(this), other };
	}
};

class Parser : public ComposableParser<Parser>
{
public:
	Parser()
		: _wrapper(std::make_shared<SharedWrapper>())
	{}

	template<typename P>
	Parser(P p)
		: Parser()
	{
		*this = p;
	}

	template<typename P>
	Parser& operator=(P p)
	{
		_wrapper->wrapper = std::make_unique<WrapperInstance<P>>(p);
		return *this;
	}

	template<typename F>
	void parse_impl(Trampoline& trampoline, std::string_view str, F f) const
	{
		if (_wrapper->wrapper == nullptr)
		{
			f(trampoline, ResultType::Failure, str);
			return;
		}

		_wrapper->wrapper->parse(trampoline, str, f);
	}

private:
	class IWrapper
	{
	public:
		virtual ~IWrapper() = default;

		virtual void parse(Trampoline& trampoline, std::string_view str, std::function<void(Trampoline&, ResultType, std::string_view)> f) const = 0;
	};

	template<typename P>
	class WrapperInstance final : public IWrapper
	{
	public:
		WrapperInstance(P p)
			: _parser(p)
		{}

		virtual void parse(Trampoline& trampoline, std::string_view str, std::function<void(Trampoline&, ResultType, std::string_view)> f) const override
		{
			_parser.parse_impl(trampoline, str, f);
		}

	private:
		P _parser;
	};

	struct SharedWrapper
	{
		std::unique_ptr<IWrapper> wrapper;
	};

	std::shared_ptr<SharedWrapper> _wrapper;
};




class Empty : public ComposableParser<Empty>
{
public:
	template<typename F>
	void parse_impl(Trampoline& trampoline, std::string_view str, F f) const
	{
		GraphvizNode ls(trampoline, "<Empty>", str);

		ls.emit(ResultType::Success);
		f(trampoline, ResultType::Success, str);
	}
};




namespace
{
	template<char DELIMITER, char... TRAIL>
	struct Foo
	{
		static bool equals(char c)
		{
			if (c == DELIMITER)
				return true;

			return Foo<TRAIL...>::equals(c);
		}
	};

	template<char DELIMITER>
	struct Foo<DELIMITER>
	{
		static bool equals(char c)
		{
			if (c == DELIMITER)
				return true;

			return false;
		}
	};
}

template<char... DELIMITERS>
class Capture : public ComposableParser<Capture<DELIMITERS...>>
{
public:
	template<typename F>
	void parse_impl(Trampoline& trampoline, std::string_view str, F f) const
	{
		size_t i = 0;
		for (; i < str.size(); ++i)
		{
			auto c = str[i];

			for (size_t j = 0; j < sizeof...(DELIMITERS); ++j)
			{
				if (Foo<DELIMITERS...>::equals(c))
					goto delimiter_found;
			}
		}

		delimiter_found:

		const auto value = str.substr(0, i);

		GraphvizNode ls(trampoline, "<Capture '" + std::string(value) + "'>", str);

		if (value.empty())
		{
			ls.emit(ResultType::Failure);
			f(trampoline, ResultType::Failure, {});
		}

		ls.emit(ResultType::Success);
		f(trampoline, ResultType::Success, str.substr(i));
		return;
	}
};





class Terminal : public ComposableParser<Terminal>
{
public:
	Terminal(std::string what)
		: _what(what)
	{}

	template<typename F>
	void parse_impl(Trampoline& trampoline, std::string_view str, F f) const
	{
		GraphvizNode ls(trampoline, _what, str);

		if (str.size() < _what.size() || memcmp(str.data(), _what.data(), _what.size()))
		{
			ls.emit(ResultType::Failure);
			f(trampoline, ResultType::Failure, str);
			return;
		}

		ls.emit(ResultType::Success);
		f(trampoline, ResultType::Success, str.substr(_what.size()));
	}

private:
	std::string _what;
};





template<typename LT, typename RT>
class Sequence : public ComposableParser<Sequence<LT, RT>>
{
public:
	Sequence(LT lhs, RT rhs)
		: _lhs(lhs)
		, _rhs(rhs)
	{}

	template<typename F>
	void parse_impl(Trampoline& trampoline, std::string_view str, F f) const
	{
		GraphvizNode ls(trampoline, _name, str);

		_lhs.parse_impl(trampoline, str, [f, ls, this](Trampoline& trampoline, ResultType result, std::string_view trail)
		{
			if (result == ResultType::Failure)
			{
				ls.emit(ResultType::Failure);

				f(trampoline, ResultType::Failure, trail);
				return;
			}

			_rhs.parse_impl(trampoline, trail, [=](Trampoline& trampoline, ResultType result, std::string_view trail)
			{
				ls.emit(result);

				f(trampoline, result, trail);
			});
		});
	}

private:
	LT _lhs;
	RT _rhs;
};




template<typename LT, typename RT>
class Disjunction : public ComposableParser<Disjunction<LT, RT>>
{
public:
	Disjunction(LT lhs, RT rhs)
		: _lhs(lhs)
		, _rhs(rhs)
	{}

	template<typename F>
	void parse_impl(Trampoline& trampoline, std::string_view str, F f) const
	{
		GraphvizNode ls(trampoline, _name, str);

		gather([&trampoline, str, f, ls](auto& parser)
		{
			trampoline.add([str, f, ls, &parser](Trampoline& trampoline)
			{
				parser.parse_impl(trampoline, str, [f, ls](Trampoline& trampoline, ResultType result, std::string_view trail)
				{
					ls.emit(result);
					f(trampoline, result, trail);
				});
			});
		});
	}

	template<typename F>
	void gather(F f) const
	{
		_lhs.gather(f);
		_rhs.gather(f);
	}

private:
	LT _lhs;
	RT _rhs;
};





Terminal operator ""_t(const char* str, size_t size)
{
	return std::string(str, size);
}

template<typename P>
Disjunction<P, Empty> Optional(P p)
{
	return { p, Empty() };
}