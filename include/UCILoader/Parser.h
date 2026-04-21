#pragma once

#include "UCI.h"

#include <functional>
#include <sstream>
#include <memory>
#include <map>

namespace UCILoader {

	/*!
		This class verifies if given string matches certain pattern. 
	*/
	class PatternMatcher {
	public:
		virtual bool match(const std::string& val) const = 0;
	};

	class StandardChessMoveMatcher : public PatternMatcher {
	public:
		bool match(const std::string& val) const;
	};

	/*!
		The move Marschaler interface provides a functionality of parsing move tokens from UCI protocol into usable objects. 
		It is possible to implement this interface for any custom move representation as long as that representation is
		default constructible. It is especially useful for chess variants that do not obey standard chess move notation
		and custom parsing logic / move representation is required. 

		In order to implement this interface user should provide a loadInto() method.
	*/
	template<class Target>
	class Marschaler {
	public:
		/*!
			Parses value of 'token' string into 'target' object. 
		*/
		virtual void loadInto(const std::string& token, Target& target) const = 0;
		
		/*!
			Returns parsed value of given token
		*/
		virtual Target marshal(const std::string& token) const;
	};


#define _INTEGER_OPTION_PARSER(tp) template<class Move> Info<Move> parse##tp##Info( const std::vector<std::string> & tokens, size_t & i) \
{return parseIntegerInfo( InfoFactory<Move>::make##tp##Info , tokens, i);}

	namespace _UCI_info_parsers {

		class UciParsingException : public std::exception {};

		template <class Move>
		Info<Move> parseIntegerInfo(Info<Move>(*factoryMethod)(int32_t v), const std::vector<std::string>& tokens, size_t& i) {
			if (i >= tokens.size()) {
				throw UciParsingException();
			}

			std::string value_string = tokens[i++];
			int32_t value;

			if ((std::stringstream(value_string) >> value).fail()) {
				throw UciParsingException();
			}

			return factoryMethod(value);
		}

		_INTEGER_OPTION_PARSER(Depth);
		_INTEGER_OPTION_PARSER(Seldepth);
		_INTEGER_OPTION_PARSER(Time);
		_INTEGER_OPTION_PARSER(Nodes);
		_INTEGER_OPTION_PARSER(MultiPv);
		_INTEGER_OPTION_PARSER(CurrentMoveNumber);
		_INTEGER_OPTION_PARSER(Hashfull);
		_INTEGER_OPTION_PARSER(NodesPerSecond);
		_INTEGER_OPTION_PARSER(Tbhiths);
		_INTEGER_OPTION_PARSER(Sbhits);
		_INTEGER_OPTION_PARSER(CPUload);

		template <class Move>
		Info<Move> parseStringInfo(const std::vector<std::string>& tokens, size_t& i) {

			if (i >= tokens.size()) {
				throw UciParsingException();
			}

			std::string accumulator = tokens[i++];
			while (i < tokens.size())
				accumulator += ' ' + tokens[i++];

			return InfoFactory<Move>::makeStringInfo(accumulator);
		}

		template <class Move>
		Info <Move> parsePvInfo(const std::vector<std::string>& tokens, size_t& i, PatternMatcher& movePatternMatcher, Marschaler<Move>& marshaler) {
			std::vector<Move> result;
			while (i < tokens.size()) {
				if (!movePatternMatcher.match(tokens[i]))
					break;

				result.push_back(marshaler.marshal(tokens[i++]));
			}

			return InfoFactory<Move>::makePvInfo(result);
		}

		template <class Move>
		Info <Move> parseRefutationInfo(const std::vector<std::string>& tokens, size_t& i, PatternMatcher& movePatternMatcher, Marschaler<Move>& marshaler) {
			Move m;
			std::vector<Move> result;

			if (i >= tokens.size() || !movePatternMatcher.match(tokens[i]))
				throw UciParsingException();

			marshaler.loadInto(tokens[i++], m);

			while (i < tokens.size()) {
				if (!movePatternMatcher.match(tokens[i]))
					break;

				result.push_back(marshaler.marshal(tokens[i++]));
			}

			return InfoFactory<Move>::makeRefutationInfo(m, result);
		}

		template <class Move>
		Info <Move> parseCurrlineInfo(const std::vector<std::string>& tokens, size_t& i, PatternMatcher& movePatternMatcher, Marschaler<Move>& marshaler) {
			std::vector<Move> result;

			if (i >= tokens.size())
				throw UciParsingException();

			std::string first = tokens[i];
			int32_t cpunr = 1;

			if ((std::stringstream(first) >> cpunr).fail())
				cpunr = 1;
			else
				++i;


			while (i < tokens.size()) {
				if (!movePatternMatcher.match(tokens[i]))
					break;

				result.push_back(marshaler.marshal(tokens[i++]));
			}

			return InfoFactory<Move>::makeCurrLineInfo(result, cpunr);
		}

		template <class Move>
		Info<Move> parseCurrentMoveInfo(const std::vector<std::string>& tokens, size_t& i, PatternMatcher& movePatternMatcher, Marschaler<Move>& marshaler) {
			Move result;
			if (i >= tokens.size() || !movePatternMatcher.match(tokens[i]))
				throw UciParsingException();

			marshaler.loadInto(tokens[i++], result);
			return InfoFactory<Move>::makeCurrentMoveInfo(result);
		}

		template <class Move>
		Info<Move> parseScoreInfo(const std::vector<std::string>& tokens, size_t& i) {

			std::string type = "", temp;
			UciScore::BoundType _boundType = UciScore::Exact;
			int32_t eval;
			while (i < tokens.size()) {
				temp = tokens[i++];

				if (temp == "cp" || temp == "mate") {
					type = temp;
					if (i >= tokens.size() || (std::stringstream(tokens[i++]) >> eval).fail())
						throw UciParsingException();
					break;
				}
				else if (temp == "lowerbound")
					_boundType = UciScore::Lowerbound;
				else if (temp == "upperbound")
					_boundType = UciScore::Upperbound;
			}



			if (type != "cp" && type != "mate")
				throw UciParsingException();


			UciScore score = (type[0] == 'c' ? UciScore::fromCentipawns : UciScore::fromMateDistance)(eval, _boundType);
			return InfoFactory<Move>::makeScoreInfo(score);
		}

	};

#undef _INTEGER_OPTION_PARSER


#define __RECEIVE_SIGNAL(SIGNAL) [this](Tokens & in) {this->on##SIGNAL(in);}
	template<class Move>
	class UCIParser {
		typedef const std::vector<std::string> Tokens;
		std::shared_ptr<AbstractEngineHandler<Move>> handler;

		std::shared_ptr<Marschaler<Move>> moveMarshaler;
		std::shared_ptr<PatternMatcher> moveValidator;

		std::map<std::string, std::function<void(Tokens&)>> router;


		void onID(Tokens& payload);
		void onBestMove(Tokens& payload);
		void onInfo(Tokens& payload);
		void onOption(Tokens& payload);
		void onRegistration(Tokens& payload);
		void onCopyprotection(Tokens& payload);
		void onUciok(Tokens& payload);
		void onReadyok(Tokens& payload);


		void parseCheckOption(Tokens& payload, size_t& i, const std::string& option_id);
		void parseStringOption(Tokens& payload, size_t& i, const std::string& option_id);
		void parseSpinOption(Tokens& payload, size_t& i, const std::string& option_id);
		void parseComboOption(Tokens& payload, size_t& i, const std::string& option_id);

		void logError(const std::string& errorMsg);

		std::string concatTokens(Tokens& payload, size_t start, char sep = ' ') const;
		std::string notEnoughArguments(size_t expectedArgumentCount, Tokens& payload);
		std::string notExpectedFormat(const std::string& expected, Tokens& found);

		std::vector<std::string> tokenize(const std::string& line) const;
	public:
		UCIParser(std::shared_ptr<AbstractEngineHandler<Move>> engineHandler, std::shared_ptr<Marschaler<Move>> marshaler, std::shared_ptr<PatternMatcher> movePatternMatcher) :
			moveMarshaler(marshaler), moveValidator(movePatternMatcher), handler(engineHandler) {
			router = {
				{"id", __RECEIVE_SIGNAL(ID)},
				{"bestmove", __RECEIVE_SIGNAL(BestMove)},
				{"info", __RECEIVE_SIGNAL(Info)},
				{"option",__RECEIVE_SIGNAL(Option)},
				{"registration", __RECEIVE_SIGNAL(Registration)},
				{"copyprotection", __RECEIVE_SIGNAL(Copyprotection)},
				{"readyok", __RECEIVE_SIGNAL(Readyok)},
				{"uciok", __RECEIVE_SIGNAL(Uciok)}
			};

		};
		void parseLine(const std::string& line);

	};

#undef __RECEIVE_SIGNAL

#define CHECK_IF_HAS_NEXT_TOKEN(msg) if (i >= payload.size()) { handler->onError(msg); return; }


	template<class Move>
	inline void UCIParser<Move>::onRegistration(Tokens& payload)
	{
		// expected format is registration [ok/error/checking]

		if (payload.size() < 2) {
			logError(notEnoughArguments(2, payload));
			return;
		};


		const std::string& commandStatus = payload[1];

		if (commandStatus == "ok") {
			handler->onRegistration(ProcedureStatus::Ok);
			return;
		}


		if (commandStatus == "error") {
			handler->onRegistration(ProcedureStatus::Error);
			return;
		}

		if (commandStatus == "checking") {
			handler->onRegistration(ProcedureStatus::Checking);
			return;
		}

		logError(notExpectedFormat("registration [ok/checking/error]", payload));
	}

	template<class Move>
	inline void UCIParser<Move>::onCopyprotection(Tokens& payload)
	{
		// expected format is copyprotection [ok/error/checking]

		if (payload.size() < 2) {
			logError(notEnoughArguments(2, payload));
			return;
		};

		const std::string& commandStatus = payload[1];

		if (commandStatus == "ok") {
			handler->onCopyProtection(ProcedureStatus::Ok);
			return;
		}


		if (commandStatus == "error") {
			handler->onCopyProtection(ProcedureStatus::Error);
			return;
		}

		if (commandStatus == "checking") {
			handler->onCopyProtection(ProcedureStatus::Checking);
			return;
		}

		logError(notExpectedFormat("copyprotection [ok/checking/error]", payload));
	}

	template<class Move>
	inline void UCIParser<Move>::logError(const std::string& errorMsg)
	{
		handler->onError(errorMsg);
	}

	template<class Move>
	inline 	std::string  UCIParser<Move>::concatTokens(Tokens& payload, size_t start, char sep) const {

		std::string result = payload[start];

		for (size_t i = start + 1; i < payload.size(); ++i) {
			result.push_back(sep);
			result += payload[i];
		}

		return result;
	}

	template<class Move>
	inline std::string UCIParser<Move>::notEnoughArguments(size_t expectedArgumentCount, Tokens& payload)
	{
		return std::string("Error! Command ") + payload[0] + " expects " + std::to_string(expectedArgumentCount);
	}

	template<class Move>
	inline std::string UCIParser<Move>::notExpectedFormat(const std::string& expected, Tokens& found)
	{
		return std::string("Format expected: ") + expected + ", found:" + concatTokens(found, 0, ' ');
	}

	template<class Move>
	inline std::vector<std::string> UCIParser<Move>::tokenize(const std::string& line) const
	{
		std::vector<std::string> tokens;
		std::stringstream sstream(line);
		std::string token;

		while (sstream) {
			token = "";
			sstream >> token;
			if (token.size()) tokens.push_back(token);
		}

		return tokens;
	}

	template<class Move>
	inline void UCIParser<Move>::parseLine(const std::string& line)
	{
		Tokens tokens = tokenize(line);

		if (tokens.size() < 1) return;

		auto it = router.find(tokens[0]);

		if (it == router.end()) {
			logError(std::string("Unrecognised command: ") + tokens[0]);
			return;
		};

		it->second(tokens);


	}

	template<class Move>
	inline void UCIParser<Move>::onUciok(Tokens& payload)
	{
		handler->onUCIOK();
	}

	template<class Move>
	inline void UCIParser<Move>::onReadyok(Tokens& payload)
	{
		handler->onReadyOK();
	}

	template<class Move>
	inline void UCIParser<Move>::onID(Tokens& payload)
	{
		if (payload.size() < 3) {
			logError(notEnoughArguments(3, payload));
			return;
		};

		if (payload[1] == "name") {
			handler->onEngineName(concatTokens(payload, 2, ' '));
			return;
		}

		if (payload[1] == "author") {
			handler->onEngineAuthor(concatTokens(payload, 2, ' '));
			return;
		}

		logError(notExpectedFormat("id name/author ...", payload));
	}

	template<class Move>
	inline void UCIParser<Move>::onBestMove(Tokens& payload)
	{
		// bestmove <move> ponder <move>

		if (payload.size() != 2 && payload.size() != 4) {
			logError("Incorrect argument count received for command bestmove");
			return;
		}

		if (!moveValidator->match(payload[1])) {
			logError("Engine send ill formatted move");
			return;
		}

		Move bestMove = moveMarshaler->marshal(payload[1]);
		Move ponderMove;

		if (payload.size() == 4 && payload[2] == "ponder" && moveValidator->match(payload[3])) {
			ponderMove = moveMarshaler->marshal(payload[3]);
			handler->onBestMove(bestMove, ponderMove);
			return;
		}

		handler->onBestMove(bestMove);

	}

	template<class Move>
	inline void UCIParser<Move>::onOption(Tokens& payload)
	{

		// option name [at least 1 word of option name] value [value]

		static const std::map<std::string, OptionType> _types = {
			{"button", Button},
			{"string", String},
			{"check", Check},
			{"combo", Combo},
			{"spin", Spin}
		};


		if (payload.size() < 2 || payload[1] != "name") {
			logError("Engine send option command, but 'name' token was not the 2nd argument");
			return;
		}

		size_t i = 2;

		std::string option_name, str_value, combo_default;
		std::vector<std::string> combo_values;
		int32_t min_ = 0, max_ = 0, default_ = 0;


		while (i < payload.size() && payload[i] != "type") {
			option_name += payload[i++];
			option_name += " ";
		};

		CHECK_IF_HAS_NEXT_TOKEN("Engine send option command, but name sequence was not terminated by 'type' token.");

		option_name.pop_back(); // we got a trailing space after the while loop

		i++; // skip the 'type' token

		CHECK_IF_HAS_NEXT_TOKEN("Engine send option command, but it unexpectedly terminated after the 'type' token.")

		auto option_type_it = _types.find(payload[i]);

		if (option_type_it == _types.end()) {
			handler->onError(payload[i] + " is not recognised option type.");
			return;
		}

		++i;

		switch (option_type_it->second) {
		case Button:
			handler->onOption(Option(option_name)); break;
		case Check:
			parseCheckOption(payload, i, option_name);
			break;
		case String:
			parseStringOption(payload, i, option_name);
			break;

		case Spin:
			parseSpinOption(payload, i, option_name);
			break;

		case Combo:
			parseComboOption(payload, i, option_name);
			break;

		}

	}

	template<class Move>
	inline void UCIParser<Move>::parseCheckOption(Tokens& payload, size_t& i, const std::string& option_id)
	{
		if (i + 1 >= payload.size() || payload[i++] != "default") {
			handler->onError("Engine send option command, but check option is missing a default value");
			return;
		}

		if (payload[i] != "true" && payload[i] != "false") {
			handler->onError("Checkbox option value could be either true or false");
			return;
		}

		handler->onOption(Option(option_id, payload[i] == "true"));
	}

	template<class Move>
	inline void UCIParser<Move>::parseStringOption(Tokens& payload, size_t& i, const std::string& option_id)
	{
		std::string str_value;
		if (i + 1 >= payload.size() || payload[i++] != "default") {
			handler->onError("Engine send option command, but string option is missing a default value (use '<empty>' if the default is empty string");
			return;
		}

		while (i < payload.size())
			str_value += payload[i++] + " ";

		str_value.pop_back();

		if (str_value == "<empty>") str_value = "";


		handler->onOption(Option(option_id, str_value));
	}

	template<class Move>
	inline void UCIParser<Move>::parseSpinOption(Tokens& payload, size_t& i, const std::string& option_id)
	{
		std::string temp;

		int32_t min_, max_, default_;

		uint8_t flags = 0u;
		while (i < payload.size()) {

			temp = payload[i++];

			if (temp == "min") {
				CHECK_IF_HAS_NEXT_TOKEN("Spin option needs min value");
				flags |= 1u;

				if ((std::stringstream(payload[i++]) >> min_).fail()) {
					logError("Could not read " + payload[i - 1] + " as integer");
					return;
				}
			}

			if (temp == "max") {
				CHECK_IF_HAS_NEXT_TOKEN("Spin option needs max value");
				flags |= 2u;

				if ((std::stringstream(payload[i++]) >> max_).fail()) {
					logError("Could not read " + payload[i - 1] + " as integer");
					return;
				}
			}

			if (temp == "default") {
				CHECK_IF_HAS_NEXT_TOKEN("Spin option needs default value");
				flags |= 4u;

				if ((std::stringstream(payload[i++]) >> default_).fail()) {
					logError("Could not read " + payload[i - 1] + " as integer");
					return;
				}
			}
		}

		if (flags != 7u) {
			logError("Engine send option command, but the passed spin option misses one of the required fields (min, max or default)");
			return;
		}

		handler->onOption(Option(option_id, spin_option({ default_, min_, max_ })));
	}

	template<class Move>
	inline void UCIParser<Move>::parseComboOption(Tokens& payload, size_t& i, const std::string& option_id)
	{
		std::string temp, combo_default;
		std::vector<std::string> combo_values;

		while (i < payload.size()) {

			temp = payload[i++];

			if (temp == "var") {
				CHECK_IF_HAS_NEXT_TOKEN("Missing value after 'var' token");
				combo_values.push_back(payload[i++]);
			}


			if (temp == "default") {
				CHECK_IF_HAS_NEXT_TOKEN("Missing value after 'default' token");
				combo_default = payload[i++];
			}

		}
		handler->onOption(Option(option_id, combo_option({ combo_default, combo_values })));
	}

	template<class Move>
	inline void UCIParser<Move>::onInfo(Tokens& payload)
	{
		std::vector<Info<Move>> result;

		std::function<std::function<Info<Move>(Tokens&, size_t&)>(std::function<Info<Move>(const std::vector<std::string>&, size_t&, PatternMatcher&, Marschaler<Move>&)>)>  decorator =
			[this](std::function<Info<Move>(Tokens&, size_t&, PatternMatcher&, Marschaler<Move>&)> wrapee) {
			return [this, wrapee](Tokens& payload, size_t& index) {
				return wrapee(payload, index, *this->moveValidator, *this->moveMarshaler);
				};
			};

		std::map<std::string, std::function<Info<Move>(const std::vector<std::string>&, size_t&)>> subparsers = {
			{"depth", _UCI_info_parsers::parseDepthInfo<Move>},
			{"seldepth", _UCI_info_parsers::parseSeldepthInfo<Move>},
			{"time", _UCI_info_parsers::parseTimeInfo<Move>},
			{"nodes", _UCI_info_parsers::parseNodesInfo<Move>},
			{"multipv", _UCI_info_parsers::parseMultiPvInfo<Move>},
			{"currmovenumber", _UCI_info_parsers::parseCurrentMoveNumberInfo<Move>},
			{"hashfull", _UCI_info_parsers::parseHashfullInfo<Move>},
			{"nps", _UCI_info_parsers::parseNodesPerSecondInfo<Move>},
			{"tbhits", _UCI_info_parsers::parseTbhithsInfo<Move>},
			{"sbhits", _UCI_info_parsers::parseSbhitsInfo<Move>},
			{"cpuload", _UCI_info_parsers::parseCPUloadInfo<Move>},
			{"string", _UCI_info_parsers::parseStringInfo<Move>},
			{"pv", decorator(_UCI_info_parsers::parsePvInfo<Move>)},
			{"refutation", decorator(_UCI_info_parsers::parseRefutationInfo<Move>)},
			{"currline", decorator(_UCI_info_parsers::parseCurrlineInfo<Move>)},
			{"currmove", decorator(_UCI_info_parsers::parseCurrentMoveInfo<Move>)},
			{"score", _UCI_info_parsers::parseScoreInfo<Move>}
		};

		size_t i = 1;
		std::string temp;
		auto it = subparsers.end();

		while (i < payload.size()) {
			temp = payload[i++];
			it = subparsers.find(temp);

			if (it != subparsers.end()) {
				try {
					Info<Move> val = it->second(payload, i);
					result.push_back(val);
				}
				catch (_UCI_info_parsers::UciParsingException e) {
					logError("Exception occurred during executing one of info command's subparsers");
				}
			}
		}

		handler->onInfo(result);
	}

	template<class Target>
	inline Target Marschaler<Target>::marshal(const std::string& token) const {
		Target result;
		loadInto(token, result);
		return result;
	}

#undef CHECK_IF_HAS_NEXT_TOKEN
}

