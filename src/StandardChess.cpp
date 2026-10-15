#include <UCILoader/StandardChess.h>
#include <UCILoader/EngineConnection.h>
#include <UCILoader/Parser.h>

using namespace StandardChess;

std::shared_ptr<UCILoader::EngineInstanceBuilder<StandardChessMove>> StandardChess::ChessEngineInstanceBuilder =
std::make_shared<UCILoader::EngineInstanceBuilder<StandardChessMove>>(
	std::make_shared<UCILoader::StandardChessMoveMatcher>(),
	std::make_shared<StandardChessMoveMarschaler>()
);