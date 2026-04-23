#pragma once
#include "Parser.h"
#include "EngineConnection.h" 
#include <cstring> // for memcpy

/*!
 * @page Standard_Chess Standard Chess UCI Loader Module
 * 
 * @brief StandardChess module provides out-of-the-box UCI protocol support for standard chess.
 * 
 * The StandardChess module serves two purposes:
 * 1. **Ready-to-use standard chess support** - Complete move representation, parsing, and formatting
 * 2. **Reference implementation** - Demonstrates how to adapt UCILoader to other chess variants
 * 
 * To use StandardChess with an engine:
 * @code
 *     auto process = openProcess({"stockfish.exe"}, "/");
 *     auto engine = StandardChess::ChessEngineInstanceBuilder->build(process, Loggers::toStd() | LoggerTraits::Pretty);
 *     engine->sync();
 * @endcode
 * 
 * @see StandardChess::StandardChessMove for move representation
 * @see StandardChess::StandardChessMoveMarschaler for move parsing
 */
namespace StandardChess {

	/*!
	 * @brief Promotion target piece for pawn moves that reach the 8th rank.
	 */
	enum PromotionTarget {
		None,     ///< No promotion (move is not a promotion)
		Rook,     ///< Promote to Rook
		Bishop,   ///< Promote to Bishop
		Knight,   ///< Promote to Knight
		Queen     ///< Promote to Queen
	};

	/*!
	 * @brief Standard chess move representation.
	 * 
	 * @details
	 * Represents a single move in standard chess using:
	 * - from: Source square (0-63, rank 8 to rank 1, files a-h)
	 * - to: Destination square (0-63)
	 * - promoteTo: Promotion target if applicable, or None
	 * 
	 * **Square Numbering:**
	 * Squares are numbered 0-63 with the formula:
	 * ```
	 * square = 8 * (8 - rank) + (file - 'a')
	 * ```
	 * where rank is 1-8 and file is 'a'-'h'.
	 * 
	 * **Examples:**
	 * - a1 = 56, h1 = 63
	 * - a8 = 0, h8 = 7
	 * - e4 = 36
	 * 
	 * @see createMove() for convenient move creation
	 * @see stringValueOf() for converting to UCI notation
	 */
	struct StandardChessMove {
		unsigned char from;              ///< Source square (0-63)
		unsigned char to;                ///< Destination square (0-63)
		PromotionTarget promoteTo;       ///< Promotion target (None for non-promotions)

		/*!
		 * @brief Equality operator for unit testing.
		 * 
		 * @param other The move to compare with
		 * @return True if all fields match
		 */
		bool operator == (const StandardChessMove& other) const {
			return 	this->from == other.from && 
					this->to == other.to &&
					this->promoteTo == other.promoteTo;
		}

		/*!
		 * @brief Assignment operator.
		 * 
		 * @param other The move to assign from
		 * @return Reference to this move
		 */
		StandardChessMove & operator = (const StandardChessMove & other) {
			this->from = other.from;
			this->to = other.to;
			this->promoteTo = other.promoteTo;
			return *this;
		};
	};

	/*!
	 * @brief Parse column and rank into a square number.
	 * 
	 * @param column The file/column ('a'-'h')
	 * @param file The rank/file ('1'-'8')
	 * @return Square number (0-63)
	 * 
	 * @details
	 * Converts algebraic notation coordinates to the internal square numbering scheme.
	 * Example: parseCoords('e', '4') returns 36 for the e4 square.
	 */
	static unsigned char parseCoords(char column, char file) {
		return 8 * ('8' - file) + column - 'a';
	}

	/*!
	 * @brief Parse a promotion character into a PromotionTarget.
	 * 
	 * @param c The promotion character ('q'=Queen, 'r'=Rook, 'b'=Bishop, 'n'=Knight, or other)
	 * @return The corresponding PromotionTarget (or None if not recognized)
	 */
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


	/*!
	 * @brief Create a StandardChessMove from algebraic notation.
	 * 
	 * @param from Source square in algebraic notation (e.g., "e2")
	 * @param to Destination square in algebraic notation (e.g., "e4")
	 * @param promoteTo Promotion character ('q', 'r', 'b', 'n'), or '\0' for no promotion
	 * @return The constructed StandardChessMove
	 * 
	 * **Examples:**
	 * @code
	 *     auto move1 = createMove("e2", "e4");        // pawn move
	 *     auto move2 = createMove("g7", "g8", 'q');   // promotion to queen
	 * @endcode
	 */
	static StandardChessMove createMove(const char from[], const char to[], char promoteTo = '\0') {
		StandardChessMove result; 
		result.from = parseCoords(from[0], from[1]);
		result.to = parseCoords(to[0], to[1]);
		result.promoteTo = parsePromotion(promoteTo);
		return result;
	}

	/*!
	 * @brief Move marshaler for parsing UCI move strings into StandardChessMove objects.
	 * 
	 * @details
	 * Implements the Marschaler<StandardChessMove> interface to convert UCI protocol
	 * move strings (e.g., "e2e4", "g7g8q") into StandardChessMove structures.
	 * 
	 * Used internally by the UCI parser when receiving move data from engines.
	 * This marshaler handles:
	 * - Standard moves (4 characters: e.g., "e2e4")
	 * - Promotion moves (5 characters: e.g., "e7e8q")
	 * 
	 * @see UCILoader::Marschaler for the interface definition
	 */
	class StandardChessMoveMarschaler : public UCILoader::Marschaler<StandardChessMove> {


	public:

		/*!
		 * @brief Parse a UCI move string into a StandardChessMove.
		 * 
		 * @param token The UCI move string ("e2e4" or "e7e8q")
		 * @param target Reference to StandardChessMove to populate
		 */
		void loadInto(const std::string& token, StandardChessMove& target) const override {
			target.from = parseCoords(token[0], token[1]);
			target.to = parseCoords(token[2], token[3]);
			
			if(token.size() >= 5 )
				target.promoteTo = parsePromotion(token[4]);
			else
				target.promoteTo = StandardChess::None;
		}

	};

	/*!
	 * @brief Convert a C-string move notation to StandardChessMove.
	 * 
	 * @param c_style_string UCI move string (e.g., "e2e4", "e7e8q")
	 * @return The parsed StandardChessMove
	 * 
	 * **Examples:**
	 * @code
	 *     auto move = moveValueOf("e2e4");      // e2 to e4
	 *     auto promo = moveValueOf("a7a8q");    // promotion to queen
	 * @endcode
	 */
	static StandardChessMove moveValueOf(const char c_style_string[]) {
		static StandardChessMoveMarschaler marshaler;
		return marshaler.marshal(c_style_string);
	}

	/*!
	 * @brief Convert a StandardChessMove to UCI notation string.
	 * 
	 * @param m The move to convert
	 * @return UCI notation string (e.g., "e2e4", "e7e8q")
	 * 
	 * **Examples:**
	 * @code
	 *     auto move = createMove("e2", "e4");
	 *     std::cout << stringValueOf(move);  // outputs: "e2e4"
	 *     
	 *     auto promo = createMove("a7", "a8", 'q');
	 *     std::cout << stringValueOf(promo); // outputs: "a7a8q"
	 * @endcode
	 */
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


	/*!
	 * @brief Position formatter for the standard chess starting position.
	 * 
	 * @details
	 * Implements PositionFormatter to represent the initial chess position
	 * (FEN: rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1).
	 * 
	 * Useful as the root position for engine searches.
	 * 
	 * **Usage:**
	 * @code
	 *     auto search = engine->search(params, StartPos(), moves);
	 * @endcode
	 * 
	 * @see FenPos for custom positions
	 * @see PositionFormatter for the interface
	 */
	class StartPos : public UCILoader::PositionFormatter {
	public:

		/*!
		 * @brief Get the FEN string for the starting position.
		 * 
		 * @return FEN: rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
		 */
		std::string toFen() const override { return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"; };
		/*!
		 * @brief Get the UCI position string for the starting position.
		 * 
		 * @return The string "startpos"
		 */
		std::string toPositionString() const override { return "startpos"; };
	};

	/*!
	 * @brief Position formatter for arbitrary FEN positions.
	 * 
	 * @details
	 * Allows specifying any chess position via FEN notation.
	 * Implements the PositionFormatter interface for use with engine searches.
	 * 
	 * @note FEN validation is not currently implemented - ensure the FEN string is valid
	 * 
	 * **Usage:**
	 * @code
	 *     // Position after 1.e4 e5
	 *     FenPos pos("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
	 *     auto search = engine->search(params, pos, {});
	 * @endcode
	 * 
	 * @see StartPos for the initial position
	 */
	class FenPos : public UCILoader::PositionFormatter {
		std::string fen;
	public:
		/*!
		 * @brief Constructor that stores the FEN string.
		 * 
		 * @param fen The FEN notation string for the position
		 */
		FenPos(const std::string& fen) : fen(fen) {};
		/*!
		 * @brief Get the FEN string.
		 * 
		 * @return The FEN string provided at construction
		 */
		std::string toFen() const override{ return fen; };
		/*!
		 * @brief Get the UCI position string.
		 * 
		 * @return "fen " followed by the FEN string
		 */
		std::string toPositionString() const override{ return "fen " + fen; };
	};

	/*!
	 * @brief Preconfigured engine instance builder for standard chess.
	 * 
	 * @details
	 * A shared_ptr to an EngineInstanceBuilder<StandardChessMove> preconfigured with
	 * the standard chess move marshaler and validator.
	 * 
	 * This is the recommended way to create EngineInstance objects for standard chess engines.
	 * 
	 * **Typical Usage:**
	 * @code
	 *     auto process = openProcess({"stockfish.exe"}, "/");
	 *     auto engine = ChessEngineInstanceBuilder->build(
	 *         process,
	 *         Loggers::toFile("engine.log") | LoggerTraits::Pretty
	 *     );
	 * @endcode
	 * 
	 * @see EngineInstanceBuilder for details about the builder pattern
	 */
	extern std::shared_ptr<UCILoader::EngineInstanceBuilder<StandardChessMove>> ChessEngineInstanceBuilder;
}
