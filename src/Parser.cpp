#include <UCILoader/Parser.h>


bool  UCILoader::StandardChessMoveMatcher::match(const std::string& val) const {

	if (val.size() != 4) return false;

	char rangeStart, rangeEnd;

	for (size_t i = 0; i < 4; ++i) {

		if (i % 2) {
			rangeStart = '1';
			rangeEnd = '8';
		}
		else {
			rangeStart = 'a';
			rangeEnd = 'h';
		}

		if (val[i] < rangeStart || val[i] > rangeEnd) return false;
	}
	return true;
}
