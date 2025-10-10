#include <UCILoader/StandardChess.h>
using namespace StandardChess;

// register move formatter function inside MoveFormatter
template <>
std::string(*UciFormatter<StandardChessMove>::MoveFormatter)(const StandardChessMove& m) = stringValueOf;
