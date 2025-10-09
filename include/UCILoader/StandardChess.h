#pragma once
#include "Parser.h"

namespace StandardChess {

	static unsigned char parseCoords(char column, char file) {
		return 8 * ('8' - file) + column - 'a';
	}

	struct StandardChessMove {
		unsigned char from;
		unsigned char to;

		// necessary for Unit testing
		bool operator == (const StandardChessMove& other) const {
			return memcmp(this, &other, sizeof(StandardChessMove)) == 0;
		}
	};

	static StandardChessMove createMove(const char from[], const char to[]) {
		StandardChessMove result;
		result.from = parseCoords(from[0], from[1]);
		result.to = parseCoords(to[0], to[1]);
		return result;
	}

	class StandardChessMoveMarschaler : public UCILoader::Marschaler<StandardChessMove> {


	public:

		// Odziedziczono za poœrednictwem elementu Marschaler
		void loadInto(const std::string& token, StandardChessMove& target) const override {
			target.from = parseCoords(token[0], token[1]);
			target.to = parseCoords(token[2], token[3]);
		}

	};

	static StandardChessMove moveValueOf(const char c_style_string[]) {
		static StandardChessMoveMarschaler marshaler;
		return marshaler.marshal(c_style_string);
	}

	static std::string stringValueOf(const StandardChessMove& m) {
		std::string result = "a1a1";

		result[0] += m.from%8;
		result[1] += 7 - m.from/8;
		result[2] += m.to % 8;
		result[3] += 7 - m.to/8;

		return result;
	}

	class StartPos : public PositionFormatter {
	public:

		std::string toFen() const override { return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"; };
		std::string toPositionString() const override { return "startpos"; };
	};

	// TODO: add pattern matcher to validate fen
	class FenPos : public PositionFormatter {
		std::string fen;
	public:
		FenPos(const std::string& fen) : fen(fen) {};
		std::string toFen() const override{ return fen; };
		std::string toPositionString() const override{ return "fen " + fen; };
	};
}
