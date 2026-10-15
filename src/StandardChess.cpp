#include <UCILoader/StandardChess.h>
#include <UCILoader/EngineConnection.h>
#include <UCILoader/Parser.h>
#include <UCILoader/UCI.h>

using namespace StandardChess;

// register move formatter function inside MoveFormatter
template <>
std::string(*UCILoader::UciFormatter<StandardChessMove>::MoveFormatter)(const StandardChessMove& m) = stringValueOf;

std::shared_ptr<UCILoader::EngineInstanceBuilder<StandardChessMove>> StandardChess::ChessEngineInstanceBuilder =
std::make_shared<UCILoader::EngineInstanceBuilder<StandardChessMove>>(
	std::make_shared<UCILoader::StandardChessMoveMatcher>(),
	std::make_shared<StandardChessMoveMarschaler>()
);