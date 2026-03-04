#pragma once

#include <string>
#include <vector>

enum ProcedureStatus {
	Checking,
	Ok,
	Error
};

enum OptionType {
	Check,
	Spin,
	Combo,
	Button,
	String
};

enum InfoType {
	Depth,
	Seldepth,
	Time,
	Nodes,
	Pv,
	MultiPv,
	Score,
	CurrentMove,
	CurrentMoveNumber,
	Hashfull,
	NodesPerSecond,
	Tbhiths,
	Sbhits,
	CPUload,
	InfoString,
	Refutation,
	CurrentLine,
};



struct combo_option {
	std::string value;
	std::vector<std::string> supported_values;
};

struct spin_option {
	int32_t value;
	int32_t min;
	int32_t max;
};

class Option {
	std::string id_;
	OptionType type_;
	void* content;

	template <class T>
	void  dispose_content_internal() {
		T* view = (T*)content;
	}

	template <>
	void dispose_content_internal<std::nullptr_t>() {};


	void dispose_content();

	void* deep_copy_content() const;
public:
	Option() : id_("<empty>"), type_(Button), content(nullptr) {};
	Option(std::string id) : id_(id), type_(Button), content(nullptr) {};  // creates a 'button' type option 
	Option(std::string id, bool value) : id_(id), type_(Check), content(new bool(value)) {}; // creates a 'check' type option with the provided default value 
	Option(std::string id, std::string value) : id_(id), type_(String), content(new std::string(value)) {}; // creates a 'check' type option with the provided default value 
	Option(std::string id, const char* value) : id_(id), type_(String), content(new std::string(value)) {}; // c-string option constructor. 
	Option(std::string id, const spin_option& content) : id_(id), type_(Spin), content(new spin_option(content)) {};
	Option(std::string id, const combo_option& content) : id_(id), type_(Combo), content(new combo_option(content)) {};
	Option(const Option& other) : id_(other.id_), type_(other.type_), content(other.deep_copy_content()) {};
	Option(Option&& other) noexcept : id_(other.id_), type_(other.type_), content(other.content) { other.content = nullptr; other.type_ = Button; };
	~Option() { dispose_content(); };

	const OptionType& type() const { return type_; };
	const std::string& id()  const { return id_; };

	std::string& str_content() { return *(std::string*)content; };
	const std::string& str_content() const { return *(std::string*)content; };

	bool& check_content() { return *(bool*)content; };
	const bool& check_content() const { return *(bool*)content; };

	spin_option& spin_content() { return *(spin_option*)content; };
	const spin_option& spin_content() const { return *(spin_option*)content; };

	combo_option& combo_content() { return *(combo_option*)content; };
	const combo_option& combo_content() const { return *(combo_option*)content; };

	Option& operator=(const Option& other) {
		dispose_content();
		id_ = other.id_;
		type_ = other.type_;
		content = other.deep_copy_content();
		return *this;
	}
};

class UciScore {
public:
	enum BoundType {
		Exact,
		Lowerbound,
		Upperbound
	};

	enum Unit {
		Centipawn,
		Mate
	};

private:
	BoundType _type;
	Unit _unit;
	int32_t _value;
public:

	UciScore(BoundType _boundType, Unit unit, int32_t value) : _type(_boundType), _unit(unit), _value(value) {};
	UciScore(const UciScore& other) : _type(other._type), _unit(other._unit), _value(other._value) {};

	static UciScore fromCentipawns(int32_t v, BoundType _type = Exact) {
		return UciScore(_type, Centipawn, v);
	}

	static UciScore fromMateDistance(int32_t v, BoundType _type = Exact) {
		return UciScore(_type, Mate, v);
	}

	const BoundType& getBoundType() const { return _type; };
	const Unit& getUnit() const { return _unit; };
	const int32_t getValue() const { return _value; };
};

template<class Move>
class RefutationInfo {
	Move refutedMove;
	std::vector<Move> refutationLine;
public:
	template<class MoveIterator>
	RefutationInfo(const Move& m, MoveIterator ref_begin, MoveIterator ref_end) {
		refutedMove = m;
		MoveIterator it = ref_begin;
		while (it != ref_end) {
			refutationLine.push_back(*it);
			it++;
		};
	};
	RefutationInfo(const RefutationInfo& other) : refutedMove(other.refutedMove), refutationLine(other.refutationLine) {};
	const Move& getRefutedMove() const { return refutedMove; };
	const std::vector<Move>& getRefutationLine() const { return refutationLine; };
	RefutationInfo& operator= (const RefutationInfo& other) {
		this->refutedMove = other.refutedMove;
		this->refutationLine.clear();
		this->refutationLine.insert(this->refutationLine.begin(), other.refutationLine.begin(), other.refutationLine.end());
	};
};
template <class Move>
class CurrentLineInfo {
	int32_t cpunr;
	std::vector<Move> currentLine;
public:
	template <class MoveIterator>
	CurrentLineInfo(const int32_t& cpunr, MoveIterator line_begin, MoveIterator line_end) : cpunr(cpunr) {
		MoveIterator it = line_begin;
		while (it != line_end) {
			currentLine.push_back(*it);
			++it;
		};
	}
	CurrentLineInfo(const CurrentLineInfo& other) : cpunr(other.cpunr), currentLine(other.currentLine) {};

	const std::vector<Move>& getCurrentLine() const { return currentLine; };
	const int32_t& getCPUnr() const { return cpunr; };

	CurrentLineInfo& operator= (const CurrentLineInfo& other) {
		this->cpunr = other.cpunr;
		this->currentLine.clear();
		this->currentLine.insert(this->currentLine.begin(), other.currentLine.begin(), other.currentLine.end());
	}
};

template <class Move>
class Info {
public:
	
private:
	union _ContentType {
		int32_t integer;
		UciScore score;

		_ContentType(const int32_t & v) {
			integer = v;
		}

		_ContentType(const UciScore & s) {
			score = s;
		}
	};

	std::string stringContent;
	std::vector<Move> moveArray;

	InfoType _type;
	_ContentType content;

public:
	Info(InfoType tp, const int32_t & type, const std::string& value, const std::vector<Move>& moveArray) : _type(tp), content(type),
		stringContent(value), moveArray(moveArray) {};
	Info(InfoType tp, const UciScore& type, const std::string& value, const std::vector<Move>& moveArray) : _type(tp), content(type),
		stringContent(value), moveArray(moveArray) {
	};
	Info(const RefutationInfo<Move>& refInfo) : _type(Refutation), content(0), stringContent("") {
		moveArray.push_back(refInfo.getRefutedMove());
		auto it = refInfo.getRefutationLine().cbegin();
		while (it != refInfo.getRefutationLine().cend()) {
			moveArray.push_back(*it);
			++it;
		}
	}
	Info(const CurrentLineInfo<Move>& currLineInfo) : _type(CurrentLine), content(currLineInfo.getCPUnr()), stringContent(""), moveArray(currLineInfo.getCurrentLine()) {};
	Info(const Info& other) : _type(other._type), content(int32_t(0)),  stringContent(other.stringContent), moveArray(other.moveArray) {
		memcpy(&content, &other.content, sizeof(_ContentType));
	};

	const InfoType getType() const { return _type; };
	const int32_t& getIntegerValue() const { return content.integer; };
	const UciScore getAsScore() const { return content.score; };
	const std::string& getStringValue() const { return stringContent; };
	const std::vector<Move> getMoveArray() const { return moveArray; };
	const Move& getAsCurrentMoveInfo() const { return moveArray[0]; };
	RefutationInfo<Move> getAsRefutationInfo() const { 
		auto it = moveArray.cbegin();
		++it;
		return RefutationInfo<Move>(moveArray[0], it, moveArray.cend()); }
	CurrentLineInfo<Move> getAsCurrentLineInfo() const { return CurrentLineInfo<Move>(getIntegerValue(), moveArray.cbegin(), moveArray.cend()); }
	
};


#define INTEGER_INFO_FACTORY_METHOD(tp) static Info<Move> make##tp##Info(int32_t value) {return makeIntegerInfo(value, tp);}
template <class Move>
class InfoFactory {
	static Info<Move> makeIntegerInfo(int32_t value, InfoType type) {
		return Info<Move>(type, { value }, "", {});
	};
public:
	INTEGER_INFO_FACTORY_METHOD(Depth);
	INTEGER_INFO_FACTORY_METHOD(Seldepth);
	INTEGER_INFO_FACTORY_METHOD(Time);
	INTEGER_INFO_FACTORY_METHOD(Nodes);
	INTEGER_INFO_FACTORY_METHOD(MultiPv);
	INTEGER_INFO_FACTORY_METHOD(CurrentMoveNumber);
	INTEGER_INFO_FACTORY_METHOD(Hashfull);
	INTEGER_INFO_FACTORY_METHOD(NodesPerSecond);
	INTEGER_INFO_FACTORY_METHOD(Tbhiths);
	INTEGER_INFO_FACTORY_METHOD(Sbhits);
	INTEGER_INFO_FACTORY_METHOD(CPUload);
	
	static Info<Move> makeStringInfo(const std::string& value) {
		return Info<Move>(InfoString, { 0 }, value, {});
	}
	
	static Info<Move> makePvInfo(const std::vector<Move>& line) {
		return Info<Move>(Pv, { 0 }, "", line);
	}
	
	static Info<Move> makeRefutationInfo(const Move& m, const std::vector<Move>& refutation) {
		return Info<Move>(RefutationInfo<Move>(m, refutation.begin(), refutation.end()));
	}

	static Info<Move> makeCurrLineInfo(const std::vector<Move>& m, int32_t cpunr = 1) {
		CurrentLineInfo<Move> currL(cpunr, m.begin(), m.end());
		return Info<Move>(CurrentLineInfo<Move>(currL));
	}

	static Info<Move> makeCurrentMoveInfo(const Move& m) {
		std::vector<Move> vec = { m };
		return Info<Move>(CurrentMove, { 0 }, "", vec);
	}

	static Info<Move> makeScoreInfo(const UciScore& score) {
		return Info<Move>(Score, score , "", {});
	}

};
#undef INTEGER_INFO_FACTORY_METHOD
template <class Move>
class AbstractEngineHandler {
public:
	~AbstractEngineHandler() {};

	virtual void onEngineName(const std::string& name) = 0;
	virtual void onEngineAuthor(const std::string& author) = 0;
	virtual void onUCIOK() = 0;
	virtual void onReadyOK() = 0;
	virtual void onBestMove(const Move& bestMove) = 0;
	virtual void onBestMove(const Move& bestMove, const Move& ponderMove) = 0;
	virtual void onInfo(const std::vector<Info<Move>>& infos) = 0;
	virtual void onCopyProtection(ProcedureStatus status) = 0;
	virtual void onRegistration(ProcedureStatus status) = 0;
	virtual void onOption(const Option& option) = 0;
	virtual void onError(const std::string& errorMsg) = 0;
};

/*
	Yes, this struct breaks camelCase conventions by using CupCase naming convention for its members.
	This exception was made because GoParamsBuilder methods are generated by preprocessor macros and that naming convention allows
	generated method to follow camelCase instead of snake_case.
*/
template<class Move>
struct GoParams {
	std::vector<Move> SearchMoves;
	bool Ponder;
	bool Infinite;

	unsigned int BlackTime;
	unsigned int BlackIncrement;
	unsigned int WhiteTime;
	unsigned int WhiteIncrement;
	unsigned int MovesToGo;
	unsigned int Depth;
	unsigned int Nodes;
	unsigned int Mate;
	unsigned int MoveTime;
};

#define SELF GoParamsBuilder<Move>
#define WITH_INTEGER_VALUE(member_name) SELF& with##member_name (unsigned int v) {this->result.member_name = v; return *this;}
template <class Move>
class GoParamsBuilder {
	GoParams<Move> result;
public:
	GoParamsBuilder();

	SELF& withSearchMoves(const std::vector<Move>& moves) { this->result.SearchMoves = moves; return *this; };
	WITH_INTEGER_VALUE(MoveTime);
	WITH_INTEGER_VALUE(MovesToGo);
	WITH_INTEGER_VALUE(Depth);
	WITH_INTEGER_VALUE(Nodes);
	WITH_INTEGER_VALUE(Mate);
	SELF& withWhiteTime(unsigned int time, unsigned int inc) { this->result.WhiteTime = time; this->result.WhiteIncrement = inc; return *this; }
	SELF& withBlackTime(unsigned int time, unsigned int inc) { this->result.BlackTime = time; this->result.BlackIncrement = inc; return *this; }
	SELF& withPondering(bool v) { this->result.Ponder = v; return *this; }
	SELF& withInfiniteMode(bool v) { this->result.Infinite = v; return *this; }
	GoParams<Move> build() const { return result; };
};
#undef WITH_INTEGER_VALUE
#undef SELF

/*!
	Formats given position 
*/
class PositionFormatter {
public:
	virtual std::string toFen() const = 0;
	virtual std::string toPositionString() const = 0;
};

std::string formatSetOptionCommand(const Option & option);


#define SingleTokenCommand(command) static std::string command() {return std::string(#command) + '\n' ;};

template<class Move>
class UciFormatter {
	// todo: add set option formatting	
public:
	static  std::string(*MoveFormatter)(const Move& m);

	SingleTokenCommand(uci);
	SingleTokenCommand(isready);
	SingleTokenCommand(ponderhit);
	SingleTokenCommand(stop);
	SingleTokenCommand(quit);
	static std::string debug(bool b) { return b ? "debug on\n" : "debug off\n"; }
	static std::string registerLater() { return "register later\n"; };
	static std::string registerEngine(std::string name, std::string code) { return "register name " + name + " code " + code + "\n"; }
	static std::string position(const PositionFormatter & pos , std::vector<Move> moves = {});
	static std::string go(const GoParams<Move>& params);
	static std::string setOpion(const Option& option); 
};

#undef SingleTokenCommand

template<class Move>
inline std::string UciFormatter<Move>::position(const PositionFormatter& pos, std::vector<Move> moves)
{
	std::string result = "position " + pos.toPositionString();

	if (!moves.empty()) {
		result += " moves ";

		for (const Move& m : moves) 
			result += MoveFormatter(m) + " ";

		result.pop_back(); // remove trailing space
	}

	result += "\n";
	return result;
}

#define UCI_ADD_IF_NO_ZERO(member_name, uci_name) if(params.member_name) result += std::string(#uci_name) + " " + std::to_string(params.member_name) + " ";
template<class Move>
inline std::string UciFormatter<Move>::go(const GoParams<Move>& params)
{
	std::string result = "go ";

	UCI_ADD_IF_NO_ZERO(WhiteTime, wtime);
	UCI_ADD_IF_NO_ZERO(WhiteIncrement, winc);
	UCI_ADD_IF_NO_ZERO(BlackTime, btime);
	UCI_ADD_IF_NO_ZERO(BlackIncrement, binc);
	UCI_ADD_IF_NO_ZERO(MovesToGo, movestogo);
	UCI_ADD_IF_NO_ZERO(Depth, depth);
	UCI_ADD_IF_NO_ZERO(Nodes, nodes);
	UCI_ADD_IF_NO_ZERO(Mate, mate);
	UCI_ADD_IF_NO_ZERO(MoveTime, movetime);
	
	if (params.Ponder) result += "ponder ";
	if (params.Infinite) result += "infinite ";
	
	if (params.SearchMoves.size()) {
		result += "searchmoves ";
		for (const Move& m : params.SearchMoves) {
			result += MoveFormatter(m);
			result.push_back(' ');
		}

	}
	result.pop_back(); // remove trailing space
	result.push_back('\n');
	return result;
}
#undef UCI_ADD_IF_NO_ZERO

template<class Move>
inline std::string UciFormatter<Move>::setOpion(const Option& option)
{
	return formatSetOptionCommand(option);
}

template<class Move>
inline GoParamsBuilder<Move>::GoParamsBuilder()
{
	result.BlackIncrement = result.WhiteIncrement = 0u;
	result.BlackTime = result.WhiteTime = 0u;
	result.Depth = result.Mate = result.Nodes = result.MoveTime = result.MovesToGo = 0u;
	result.Infinite = result.Ponder = false;
}
