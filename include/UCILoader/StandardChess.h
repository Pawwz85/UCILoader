#pragma once
#include "Parser.h"
#include "EngineConnection.h" 
#include <cstring> // for memcpy

/*!
	StandardChess module provide out of the box support for parsing UCI protocol for standard chess variant.
	It is also intended to act as point of reference how to adapt UCILoader library to a concrete chess variant.
*/
namespace StandardChess {

	enum PromotionTarget {
		None,
		Rook,
		Bishop,
		Knight,
		Queen
	};

	/*!
		Default move representation for standard chess. 
	*/
	struct StandardChessMove {
		unsigned char from;
		unsigned char to;
		PromotionTarget promoteTo;

		// necessary for Unit testing
		bool operator == (const StandardChessMove& other) const {
			return memcmp(this, &other, sizeof(StandardChessMove)) == 0;
		}
	};

	static unsigned char parseCoords(char column, char file) {
		return 8 * ('8' - file) + column - 'a';
	}

	static StandardChess::PromotionTarget parsePromotion(char c){
		switch (c)
		{
		case 'q' : return StandardChess::Queen;
		case 'r' : return StandardChess::Rook;
		case 'b' : return StandardChess::Bishop;
		case 'n' : return StandardChess::Knight;
		default:   return StandardChess::None;
		}
	};


	static StandardChessMove createMove(const char from[], const char to[], char promoteTo = '\0') {
		StandardChessMove result; 
		result.from = parseCoords(from[0], from[1]);
		result.to = parseCoords(to[0], to[1]);
		result.promoteTo = parsePromotion(promoteTo);
		return result;
	}

	/*!
		Move marschaler implementation for "StandardChessMove" struct. 

	*/
	class StandardChessMoveMarschaler : public UCILoader::Marschaler<StandardChessMove> {


	public:

		void loadInto(const std::string& token, StandardChessMove& target) const override {
			target.from = parseCoords(token[0], token[1]);
			target.to = parseCoords(token[2], token[3]);
			
			if(token.size() >= 5 )
				target.promoteTo = parsePromotion(token[4]);
			else
				target.promoteTo = StandardChess::None;
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

		if (m.promoteTo != StandardChess::None) {
			result += "qrbn"[m.promoteTo];
		}

		return result;
	}


	class StartPos : public PositionFormatter {
	public:

		std::string toFen() const override { return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"; };
		std::string toPositionString() const override { return "startpos"; };
	};

	// TODO: add fen validation 
	class FenPos : public PositionFormatter {
		std::string fen;
	public:
		FenPos(const std::string& fen) : fen(fen) {};
		std::string toFen() const override{ return fen; };
		std::string toPositionString() const override{ return "fen " + fen; };
	};

	extern std::shared_ptr<UCILoader::EngineInstanceBuilder<StandardChessMove>> ChessEngineInstanceBuilder;
}
