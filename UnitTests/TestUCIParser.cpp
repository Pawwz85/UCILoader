#include "pch.h"

#include <UCILoader/UCI.h>
#include <UCILoader/StandardChess.h>

using namespace StandardChess;

using namespace UCILoader;

TEST(Standard_Chess_Move, CoordinateSystem) {
	StandardChessMove m1 = createMove("a8", "h8");
	StandardChessMove m2 = createMove("a1", "h1");

	ASSERT_EQ(m1.from, 0);
	ASSERT_EQ(m1.to, 7);

	ASSERT_EQ(m2.from, 56);
	ASSERT_EQ(m2.to, 63);
};

TEST(Standard_Chess_Move, Marshaler) {
	StandardChessMoveMarschaler marshaler;

	StandardChessMove m1 = marshaler.marshal("a8h8");
	StandardChessMove m2 = marshaler.marshal("a1h1");
	
	ASSERT_EQ(m1.from, 0);
	ASSERT_EQ(m1.to, 7);

	ASSERT_EQ(m2.from, 56);
	ASSERT_EQ(m2.to, 63);
}

TEST(Standard_Chess_Move, Pattern_Matcher_Valid) {
	StandardChessMoveMatcher matcher;

	ASSERT_TRUE(matcher.match("a1b1"));
	ASSERT_TRUE(matcher.match("a1h8"));
	ASSERT_TRUE(matcher.match("b2e5"));
	ASSERT_TRUE(matcher.match("b8c6"));
}

TEST(Standard_Chess_Move, Pattern_Matcher_Invalid_Letters) {
	StandardChessMoveMatcher matcher;
	ASSERT_FALSE(matcher.match("a1j1"));
}

TEST(Standard_Chess_Move, Pattern_Matcher_Invalid_Rank) {
	StandardChessMoveMatcher matcher;
	ASSERT_FALSE(matcher.match("a1a9"));
}

TEST(Standard_Chess_Move, Pattern_Matcher_Empty) {
	StandardChessMoveMatcher  matcher;
	ASSERT_FALSE(matcher.match(""));
}

TEST(Standard_Chess_Move, Pattern_Matcher_Too_Short) {
	StandardChessMoveMatcher  matcher;
	ASSERT_FALSE(matcher.match("a1"));
}

TEST(Standard_Chess_Move, Pattern_Matcher_Too_Long) {
	StandardChessMoveMatcher  matcher;
	ASSERT_FALSE(matcher.match("a1h8h8"));
}

TEST(Standard_Chess_Move, Pattern_Matcher_Promotion) {
	StandardChessMoveMatcher  matcher;
	ASSERT_TRUE(matcher.match("a7a8q"));
}

TEST(Standard_Chess_Move, Pattern_Matcher_Invalid_Promotion) {
	StandardChessMoveMatcher  matcher;
	ASSERT_FALSE(matcher.match("a7a8f"));
}

TEST(Standard_Chess_Move, To_String) {
	StandardChessMove move = moveValueOf("b2c4");
	std::string res = stringValueOf(move);
	ASSERT_EQ(res, "b2c4");
}



class MockupEngineHandler : public AbstractEngineHandler<StandardChessMove> {

	struct _record {
		const void* value;
		std::function<void(const void*)> dtor;
	};

	std::map<std::string, _record> signalStorage;

	template <class Value>
	void storeSignal(const std::string& signal_name, const Value* value) {
		_record _new = {
			value,
		[](const void* ptr) {
			const Value* casted = (const Value*)ptr;
			delete casted;
			}
		};
		std::map<std::string, _record>::iterator it = signalStorage.find(signal_name);
		if (it != signalStorage.end()) {
			it->second.dtor(it->second.value);
			it->second.value = value;
		}
		else {
			signalStorage.insert({ signal_name, _new });
		}

	
	}



public:

	~MockupEngineHandler() {
		std::map<std::string, _record>::iterator it = signalStorage.begin();

		while (it != signalStorage.end()) {
			it->second.dtor(it->second.value);
			it++;
		};
	}

	// Odziedziczono za po�rednictwem elementu AbstractEngineHandler
	void onEngineName(const std::string& name) override
	{
		storeSignal("name", new std::string(name));
	}
	void onEngineAuthor(const std::string& author) override
	{
		storeSignal("author", new std::string(author));
	}
	void onUCIOK() override
	{
		storeSignal<bool>("uciok", new bool(1));
	}
	void onReadyOK() override
	{
		storeSignal<bool>("readyok", new bool(1));
	}
	void onBestMove(const StandardChessMove& bestMove) override
	{
		storeSignal("bestmove", new StandardChessMove(bestMove));
	}
	void onBestMove(const StandardChessMove& bestMove, const StandardChessMove& ponderMove) override
	{
		storeSignal("bestmove", new StandardChessMove(bestMove));
		storeSignal("pondermove", new StandardChessMove(ponderMove));
	}
	void onInfo(const std::vector<Info<StandardChessMove>>& infos) override
	{
		storeSignal("info", new std::vector<Info<StandardChessMove>>(infos));
	}
	void onCopyProtection(ProcedureStatus status) override
	{
		storeSignal("copyprotection", new ProcedureStatus(status));
	}
	void onRegistration(ProcedureStatus status) override
	{
		storeSignal("registration", new ProcedureStatus(status));
	}
	void onOption(const Option& option) override
	{
		storeSignal("option", new Option(option));
	}
	void onError(const std::string& errorMsg) override
	{
		storeSignal("error", new std::string(errorMsg));
	}

	template <class Value>
	const Value* readLastSignal(const std::string& signal_name) const {
		auto it = signalStorage.find(signal_name);

		if (it == signalStorage.end()) return nullptr;

		const void* ptr = signalStorage.at(signal_name).value;
		return (const Value*)ptr;
	}
};

std::unique_ptr<UCIParser<StandardChessMove>> makeParser(std::shared_ptr<AbstractEngineHandler<StandardChessMove>> handler) {
	return std::make_unique<UCIParser<StandardChessMove>>(
		handler,
		std::make_shared<StandardChessMoveMarschaler>(),
		std::make_shared<StandardChessMoveMatcher>()
	);
}

TEST(UciParser, uciok) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("uciok");

	ASSERT_TRUE(handler->readLastSignal<bool>("uciok"));
}

TEST(UciParser, isready) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("readyok");

	ASSERT_TRUE(handler->readLastSignal<bool>("readyok"));
}

TEST(UciParser, bestmove_no_ponder) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("bestmove e1e2");

	const StandardChessMove* bestMove = handler->readLastSignal<StandardChessMove>("bestmove");
	const StandardChessMove* ponderMove = handler->readLastSignal<StandardChessMove>("pondermove");

	ASSERT_NE(bestMove, nullptr);
	ASSERT_EQ(ponderMove, nullptr);
	ASSERT_EQ(bestMove->from, parseCoords('e', '1'));
	ASSERT_EQ(bestMove->to, parseCoords('e', '2'));
}

TEST(UciParser, bestmove_with_ponder) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("bestmove e1e2 ponder e8e7");

	const StandardChessMove* bestMove = handler->readLastSignal<StandardChessMove>("bestmove");
	const StandardChessMove* ponderMove = handler->readLastSignal<StandardChessMove>("pondermove");

	ASSERT_NE(bestMove, nullptr);
	ASSERT_NE(ponderMove, nullptr);
	ASSERT_EQ(bestMove->from, parseCoords('e', '1'));
	ASSERT_EQ(bestMove->to, parseCoords('e', '2'));
	ASSERT_EQ(ponderMove->from, parseCoords('e', '8'));
	ASSERT_EQ(ponderMove->to, parseCoords('e', '7'));
}

TEST(UciParser, bestmove_no_arguments ) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("bestmove");

	const StandardChessMove* bestMove = handler->readLastSignal<StandardChessMove>("bestmove");
	const StandardChessMove* ponderMove = handler->readLastSignal<StandardChessMove>("pondermove");

	ASSERT_EQ(bestMove, nullptr);
	ASSERT_EQ(ponderMove, nullptr);
}

TEST(UciParser, bestmove_ponder_invalid_ponder_move) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("bestmove e1e2 ponder chess");

	const StandardChessMove* bestMove = handler->readLastSignal<StandardChessMove>("bestmove");
	const StandardChessMove* ponderMove = handler->readLastSignal<StandardChessMove>("pondermove");

	ASSERT_NE(bestMove, nullptr);
	ASSERT_EQ(ponderMove, nullptr);
}

TEST(UciParser, bestmove_invalid_bestmove) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("bestmove chess");

	const StandardChessMove* bestMove = handler->readLastSignal<StandardChessMove>("bestmove");
	const StandardChessMove* ponderMove = handler->readLastSignal<StandardChessMove>("pondermove");

	ASSERT_EQ(bestMove, nullptr);
	ASSERT_EQ(ponderMove, nullptr);
}

TEST(UciParser, bestmove_invalid_bestmove_with_valid_ponder) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("bestmove chess ponder e2e4");

	const StandardChessMove* bestMove = handler->readLastSignal<StandardChessMove>("bestmove");
	const StandardChessMove* ponderMove = handler->readLastSignal<StandardChessMove>("pondermove");

	ASSERT_EQ(bestMove, nullptr);
	ASSERT_EQ(ponderMove, nullptr);
}

TEST(UciParser, id_name) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("id name engine");

	const std::string* engine_name = handler->readLastSignal<std::string>("name");
	
	ASSERT_NE(engine_name, nullptr);
	ASSERT_EQ(*engine_name, "engine");
}

TEST(UciParser, id_long_name_with_spaces) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("id name engine name with spaces");

	const std::string* engine_name = handler->readLastSignal<std::string>("name");

	ASSERT_NE(engine_name, nullptr);
	ASSERT_EQ(*engine_name, "engine name with spaces");
}

TEST(UciParser, id_author) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("id author seaq56");

	const std::string* engine_author = handler->readLastSignal<std::string>("author");

	ASSERT_NE(engine_author, nullptr);
	ASSERT_EQ(*engine_author, "seaq56");
}

TEST(UciParser, id_long_author_name_with_spaces) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("id author firstname lastname");

	const std::string* author_name = handler->readLastSignal<std::string>("author");

	ASSERT_NE(author_name, nullptr);
	ASSERT_EQ(*author_name, "firstname lastname");
}

TEST(UciParser, id_author_no_arguments) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("id author");

	const std::string* engine_author = handler->readLastSignal<std::string>("author");

	ASSERT_EQ(engine_author, nullptr);
}

TEST(UciParser, id_name_no_arguments) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("id name");

	const std::string* engine_name = handler->readLastSignal<std::string>("name");
	
	ASSERT_EQ(engine_name, nullptr);
}

TEST(UciParser, copyprotection) {

	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("copyprotection ok");


	auto query = [handler]() {return handler->readLastSignal<ProcedureStatus>("copyprotection"); };

	const ProcedureStatus* s = query();

	ASSERT_NE(s, nullptr);
	ASSERT_EQ(*s, ProcedureStatus::Ok);

	parser->parseLine("copyprotection checking");
	s = query();
	ASSERT_EQ(*s, ProcedureStatus::Checking);

	parser->parseLine("copyprotection error");
	s = query();
	ASSERT_EQ(*s, ProcedureStatus::Error);
}

TEST(UciParser, copyprotection_invalid_status) {

	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("copyprotection meow");


	auto query = [handler]() {return handler->readLastSignal<ProcedureStatus>("copyprotection"); };

	const ProcedureStatus* s = query();

	ASSERT_EQ(s, nullptr);
}

TEST(UciParser, registration) {

	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("registration ok");


	auto query = [handler]() {return handler->readLastSignal<ProcedureStatus>("registration"); };

	const ProcedureStatus* s = query();

	ASSERT_NE(s, nullptr);
	ASSERT_EQ(*s, ProcedureStatus::Ok);

	parser->parseLine("registration checking");
	s = query();
	ASSERT_EQ(*s, ProcedureStatus::Checking);

	parser->parseLine("registration error");
	s = query();
	ASSERT_EQ(*s, ProcedureStatus::Error);
}

TEST(UciParser, registration_invalid_status) {

	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("registration bark");


	auto query = [handler]() {return handler->readLastSignal<ProcedureStatus>("registration"); };

	const ProcedureStatus* s = query();

	ASSERT_EQ(s, nullptr);

}

TEST(UciParser, button_option) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name button1 type button");


	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };

	const Option* s = query();

	ASSERT_NE(s, nullptr);
	ASSERT_EQ(s->type(), Button);
	ASSERT_EQ(s->id(), "button1");
}

TEST(UciParser, option_with_muti_word_name) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name option with long name type button");


	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };

	const Option* s = query();

	ASSERT_NE(s, nullptr);
	ASSERT_EQ(s->type(), Button);
	ASSERT_EQ(s->id(), "option with long name");
}

TEST(UciParser, string_option_single_word_default) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name str type string default value");


	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };

	const Option* s = query();

	ASSERT_NE(s, nullptr);
	ASSERT_EQ(s->type(), String);
	ASSERT_EQ(s->id(), "str");
	ASSERT_EQ(s->str_content(), "value");
}

TEST(UciParser, string_option_single_multi_word_default) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name str type string default long value");


	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };

	const Option* s = query();

	ASSERT_NE(s, nullptr);
	ASSERT_EQ(s->type(), String);
	ASSERT_EQ(s->id(), "str");
	ASSERT_EQ(s->str_content(), "long value");
}

TEST(UciParser, string_option_incorrect_empty_default) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name str type string default");


	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };

	const Option* s = query();

	ASSERT_EQ(s, nullptr);
}

TEST(UciParser, string_option_empty_default) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name str type string default <empty>");


	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };

	const Option* s = query();

	ASSERT_NE(s, nullptr);
	ASSERT_EQ(s->type(), String);
	ASSERT_EQ(s->id(), "str");
	ASSERT_EQ(s->str_content(), "");
}

TEST(UciParser, spin_option_happy_path) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name spin1 type spin min 1 max 7 default 5");


	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };

	const Option* s = query();

	ASSERT_NE(s, nullptr);
	ASSERT_EQ(s->type(), Spin);
	ASSERT_EQ(s->id(), "spin1");
	ASSERT_EQ(s->spin_content().value, 5);
	ASSERT_EQ(s->spin_content().min, 1);
	ASSERT_EQ(s->spin_content().max, 7);
}

TEST(UciParser, spin_option_happy_path_with_swapped_min_max_fields) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name spin1 type spin max 7  min 1 default 5");


	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };

	const Option* s = query();

	ASSERT_NE(s, nullptr);
	ASSERT_EQ(s->type(), Spin);
	ASSERT_EQ(s->id(), "spin1");
	ASSERT_EQ(s->spin_content().value, 5);
	ASSERT_EQ(s->spin_content().min, 1);
	ASSERT_EQ(s->spin_content().max, 7);
}

TEST(UciParser, spin_missing_min_field) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name spin1 type spin max 7  default 5");


	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };

	const Option* s = query();

	ASSERT_EQ(s, nullptr);
}

TEST(UciParser, spin_missing_max_field) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name spin1 type spin min 1 default 5");
	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };
	const Option* s = query();

	ASSERT_EQ(s, nullptr);
}

TEST(UciParser, spin_missing_default_field) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name spin1 type spin min 1 max 7");
	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };
	const Option* s = query();

	ASSERT_EQ(s, nullptr);
}

TEST(UciParser, spin_unexpected_string_value) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name spin1 type spin max 7  min 1 default five");
	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };
	const Option* s = query();

	ASSERT_EQ(s, nullptr);
}

TEST(UciParser, check_option) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name check1 type check default true");
	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };
	const Option* s = query();

	ASSERT_NE(s, nullptr);
	ASSERT_EQ(s->id(), "check1");
	ASSERT_EQ(s->type(), Check);
	ASSERT_EQ(s->check_content(), true);
}

TEST(UciParser, check_option2) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name check1 type check default false");
	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };
	const Option* s = query();

	ASSERT_NE(s, nullptr);
	ASSERT_EQ(s->id(), "check1");
	ASSERT_EQ(s->type(), Check);
	ASSERT_EQ(s->check_content(), false);
}


TEST(UciParser, check_option_incorrect_value) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name check1 type check default maybe");
	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };
	const Option* s = query();

	ASSERT_EQ(s, nullptr);
}

TEST(UciParser, check_option_missing_default_value) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name check1 type check");
	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };
	const Option* s = query();

	ASSERT_EQ(s, nullptr);
}

TEST(UciParser, combo_option) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("option name combo1 type combo default normal var normal var aggressive var adaptive");
	auto query = [handler]() {return handler->readLastSignal<Option>("option"); };
	const Option* s = query();

	ASSERT_NE(s, nullptr);
	ASSERT_EQ(s->id(), "combo1");
	ASSERT_EQ(s->combo_content().value, "normal");
	ASSERT_EQ(s->combo_content().supported_values[0], "normal");
	ASSERT_EQ(s->combo_content().supported_values[1], "aggressive");
	ASSERT_EQ(s->combo_content().supported_values[2], "adaptive");
}

#define READ_INFO_SIGNAL(handler) handler->readLastSignal<std::vector<Info<StandardChessMove>>>("info");

TEST(UciParser, InfoString) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("info string Ala ma kota");
	
	const std::vector<Info<StandardChessMove>>* info = READ_INFO_SIGNAL(handler);
	ASSERT_NE(info, nullptr);
	ASSERT_GE(info->size(), 0);
	ASSERT_EQ(info->at(0).getType(), InfoString);
	ASSERT_EQ(info->at(0).getStringValue(), "Ala ma kota");
}

TEST(UciParser, BasicExample1) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("info currmove e2e4 currmovenumber 1");

	const std::vector<Info<StandardChessMove>>* info = READ_INFO_SIGNAL(handler);
	ASSERT_NE(info, nullptr);
	ASSERT_EQ(info->size(), 2);
	ASSERT_EQ(info->at(0).getType(), CurrentMove);
	ASSERT_EQ(info->at(0).getAsCurrentMoveInfo(), moveValueOf("e2e4"));
	ASSERT_EQ(info->at(1).getType(), CurrentMoveNumber);
	ASSERT_EQ(info->at(1).getIntegerValue(), 1);
}

TEST(UciParser, BasicExample2) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("info depth 12 nodes 123456 nps 100000");

	const std::vector<Info<StandardChessMove>>* info = READ_INFO_SIGNAL(handler);
	ASSERT_NE(info, nullptr);
	ASSERT_EQ(info->size(), 3);
	ASSERT_EQ(info->at(0).getType(), Depth);
	ASSERT_EQ(info->at(0).getIntegerValue(), 12);
	ASSERT_EQ(info->at(1).getType(), Nodes);
	ASSERT_EQ(info->at(1).getIntegerValue(), 123456);
	ASSERT_EQ(info->at(2).getType(), NodesPerSecond);
	ASSERT_EQ(info->at(2).getIntegerValue(), 100000);
}

TEST(UciParser, BasicExample3) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("info depth 2 score cp 214 time 1242 nodes 2124 nps 34928 pv e2e4 e7e5 g1f3");
	const std::vector<Info<StandardChessMove>>* info = READ_INFO_SIGNAL(handler);
	ASSERT_NE(info, nullptr);
	ASSERT_EQ(info->size(), 6);
	ASSERT_EQ(info->at(0).getType(), Depth);
	ASSERT_EQ(info->at(1).getType(), Score);
	ASSERT_EQ(info->at(2).getType(), Time);
	ASSERT_EQ(info->at(3).getType(), Nodes);
	ASSERT_EQ(info->at(4).getType(), NodesPerSecond);
	ASSERT_EQ(info->at(5).getType(), Pv);
}

TEST(UciParser, ScoreParsing) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("info depth 2 score cp 214 time 1242 nodes 2124 nps 34928 pv e2e4 e7e5 g1f3");
	const std::vector<Info<StandardChessMove>>* info = READ_INFO_SIGNAL(handler);
	ASSERT_NE(info, nullptr);
	ASSERT_GE(info->size(), 2);

	ASSERT_EQ(info->at(1).getType(), Score);
	UciScore s = info->at(1).getAsScore();
	
	ASSERT_EQ(s.getBoundType(), s.Exact);
	ASSERT_EQ(s.getUnit(), s.Centipawn);
	ASSERT_EQ(s.getValue(), 214);
}

TEST(UciParser, ScoreParsing2) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("info depth 2 score lowerbound cp 214 time 1242 nodes 2124 nps 34928 pv e2e4 e7e5 g1f3");
	const std::vector<Info<StandardChessMove>>* info = READ_INFO_SIGNAL(handler);
	ASSERT_NE(info, nullptr);
	ASSERT_GE(info->size(), 2);

	ASSERT_EQ(info->at(1).getType(), Score);
	UciScore s = info->at(1).getAsScore();

	ASSERT_EQ(s.getBoundType(), s.Lowerbound);
	ASSERT_EQ(s.getUnit(), s.Centipawn);
	ASSERT_EQ(s.getValue(), 214);
}

TEST(UciParser, ScoreParsing3) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("info depth 2 score upperbound mate -5 time 1242 nodes 2124 nps 34928 pv e2e4 e7e5 g1f3");
	const std::vector<Info<StandardChessMove>>* info = READ_INFO_SIGNAL(handler);
	ASSERT_NE(info, nullptr);
	ASSERT_GE(info->size(), 2);

	ASSERT_EQ(info->at(1).getType(), Score);
	UciScore s = info->at(1).getAsScore();

	ASSERT_EQ(s.getBoundType(), s.Upperbound);
	ASSERT_EQ(s.getUnit(), s.Mate);
	ASSERT_EQ(s.getValue(), -5);
}

TEST(UciParser, PVparsing) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("info depth 2 score upperbound mate -5 time 1242 nodes 2124 nps 34928 pv e2e4 e7e5 g1f3");
	const std::vector<Info<StandardChessMove>>* info = READ_INFO_SIGNAL(handler);
	ASSERT_NE(info, nullptr);
	ASSERT_EQ(info->size(), 6);
	ASSERT_EQ(info->at(5).getType(), Pv);
	std::vector<StandardChessMove> pv = info->at(5).getMoveArray();

	ASSERT_EQ(pv.size(), 3);
	ASSERT_EQ(pv[0], moveValueOf("e2e4"));
	ASSERT_EQ(pv[1], moveValueOf("e7e5"));
	ASSERT_EQ(pv[2], moveValueOf("g1f3"));
}

TEST(UciParser, Refutation) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("info refutation d1h5 g6h5");
	const std::vector<Info<StandardChessMove>>* info = READ_INFO_SIGNAL(handler);
	ASSERT_NE(info, nullptr);
	ASSERT_EQ(info->size(), 1);
	ASSERT_EQ(info->at(0).getType(), Refutation);
	RefutationInfo<StandardChessMove> refutationInfo = info->at(0).getAsRefutationInfo();

	ASSERT_EQ(refutationInfo.getRefutedMove(), moveValueOf("d1h5"));
	ASSERT_EQ(refutationInfo.getRefutationLine().size(), 1);
	ASSERT_EQ(refutationInfo.getRefutationLine()[0], moveValueOf("g6h5"));
}

TEST(UciParser, currline1) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("info currline 2 d1h5 g6h5");
	const std::vector<Info<StandardChessMove>>* info = READ_INFO_SIGNAL(handler);
	ASSERT_NE(info, nullptr);
	ASSERT_EQ(info->size(), 1);
	ASSERT_EQ(info->at(0).getType(), CurrentLine);
	CurrentLineInfo<StandardChessMove> curl  = info->at(0).getAsCurrentLineInfo();

	ASSERT_EQ(curl.getCPUnr(), 2);
	ASSERT_EQ(curl.getCurrentLine().size(), 2);
	ASSERT_EQ(curl.getCurrentLine()[0], moveValueOf("d1h5"));
	ASSERT_EQ(curl.getCurrentLine()[1], moveValueOf("g6h5"));
}

TEST(UciParser, currline2) {
	std::shared_ptr<MockupEngineHandler> handler = std::make_shared<MockupEngineHandler>();
	auto parser = makeParser(handler);

	parser->parseLine("info currline d1h5 g6h5");
	const std::vector<Info<StandardChessMove>>* info = READ_INFO_SIGNAL(handler);
	ASSERT_NE(info, nullptr);
	ASSERT_EQ(info->size(), 1);
	ASSERT_EQ(info->at(0).getType(), CurrentLine);
	CurrentLineInfo<StandardChessMove> curl = info->at(0).getAsCurrentLineInfo();

	ASSERT_EQ(curl.getCPUnr(), 1);
	ASSERT_EQ(curl.getCurrentLine().size(), 2);
	ASSERT_EQ(curl.getCurrentLine()[0], moveValueOf("d1h5"));
	ASSERT_EQ(curl.getCurrentLine()[1], moveValueOf("g6h5"));
}


TEST(UciFormatter, basicCommands) {
	ASSERT_EQ(UciFormatter<StandardChessMove>::uci(), "uci\n");
	ASSERT_EQ(UciFormatter<StandardChessMove>::isready(), "isready\n");
	ASSERT_EQ(UciFormatter<StandardChessMove>::stop(), "stop\n");
	ASSERT_EQ(UciFormatter<StandardChessMove>::quit(), "quit\n");
	ASSERT_EQ(UciFormatter<StandardChessMove>::ponderhit(), "ponderhit\n");
	ASSERT_EQ(UciFormatter<StandardChessMove>::debug(true), "debug on\n");
	ASSERT_EQ(UciFormatter<StandardChessMove>::debug(false), "debug off\n");
}

TEST(UciFormatter, position1) {
	ASSERT_EQ(UciFormatter<StandardChessMove>::position(StartPos()), "position startpos\n");
}

TEST(UciFormatter, position2) {
	std::vector<StandardChessMove> moves = { moveValueOf("e2e4"), moveValueOf("e7e5"), moveValueOf("g1f3"), moveValueOf("b8c6")};
	ASSERT_EQ(UciFormatter<StandardChessMove>::position(StartPos(), moves), "position startpos moves e2e4 e7e5 g1f3 b8c6\n");
}

TEST(UciFormatter, position3) {
	FenPos formatter(StartPos().toFen());
	ASSERT_EQ(UciFormatter<StandardChessMove>::position(formatter), "position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\n");
}

TEST(UciFormatter, position4) {
	std::vector<StandardChessMove> moves = { moveValueOf("e2e4"), moveValueOf("e7e5"), moveValueOf("g1f3"), moveValueOf("b8c6") };
	FenPos formatter(StartPos().toFen());
	ASSERT_EQ(UciFormatter<StandardChessMove>::position(formatter, moves), "position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 moves e2e4 e7e5 g1f3 b8c6\n");
}

TEST(UciFormatter, go1) {
	GoParamsBuilder<StandardChessMove> builder;
	auto params = builder.withWhiteTime(1000, 100).build();
	ASSERT_EQ(UciFormatter<StandardChessMove>::go(params), "go wtime 1000 winc 100\n");
	params = builder.withBlackTime(1000, 100).withWhiteTime(0, 0).build();
	ASSERT_EQ(UciFormatter<StandardChessMove>::go(params), "go btime 1000 binc 100\n");
}

TEST(UciFormatter, go2) {
	GoParamsBuilder<StandardChessMove> builder;
	auto params = builder.withPondering(true).build();
	ASSERT_EQ(UciFormatter<StandardChessMove>::go(params), "go ponder\n");
	params = builder.withPondering(false).withInfiniteMode(true).build();
	ASSERT_EQ(UciFormatter<StandardChessMove>::go(params), "go infinite\n");
}

TEST(UciFormatter, go3) {
	GoParamsBuilder<StandardChessMove> builder;
	std::vector<StandardChessMove> moves = { moveValueOf("e2e4"), moveValueOf("g1f3"), moveValueOf("d2d4") };
	auto params = builder.withSearchMoves(moves).build();
	ASSERT_EQ(UciFormatter<StandardChessMove>::go(params), "go searchmoves e2e4 g1f3 d2d4\n");
}

TEST(UciFormatter, setoption_button) {
	ASSERT_EQ(UciFormatter<StandardChessMove>::setOpion(Option("clear hash")), "setoption clear hash\n");
}

TEST(UciFormatter, setoption_string) {
	ASSERT_EQ(UciFormatter<StandardChessMove>::setOpion(Option("string option", "some value")), "setoption string option value some value\n");
}


TEST(UciFormatter, setoption_spin) {
	ASSERT_EQ(UciFormatter<StandardChessMove>::setOpion(Option("spin option", spin_option({ 7, 1, 10 }))), "setoption spin option value 7\n");
}

TEST(UciFormatter, setoption_combo) {
	combo_option opt = { "var1", {"var1", "var2", "var3"}};
	ASSERT_EQ(UciFormatter<StandardChessMove>::setOpion(Option("combo option", opt)), "setoption combo option value var1\n");
}
