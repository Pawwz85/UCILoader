#include "pch.h"
#include <UCILoader/UCI.h>
#include <UCILoader/StandardChess.h>

/*
	A bunch of testing functions for Info class from the header UCI although it is more a syntax playground than a propel unit test suite. 
*/
using namespace StandardChess;

RefutationInfo<StandardChessMove> getStagedRefutationInfo() {
	StandardChessMoveMarschaler moveParser;
	StandardChessMove move = moveParser.marshal("e2e4");
	std::vector<StandardChessMove> refutation;

	refutation.push_back(moveParser.marshal("e7e5"));
	refutation.push_back(moveParser.marshal("g1f3"));
	refutation.push_back(moveParser.marshal("b8c6"));
	refutation.push_back(moveParser.marshal("e7e5"));
	refutation.push_back(moveParser.marshal("f1c5"));
	return RefutationInfo<StandardChessMove> (move, refutation.cbegin(), refutation.cend());
}

CurrentLineInfo<StandardChessMove> getStagedCurrentLineInfo() {
	

	std::vector<StandardChessMove> line;

	line.push_back(moveValueOf("e7e5"));
	line.push_back(moveValueOf("g1f3"));
	line.push_back(moveValueOf("b8c6"));
	line.push_back(moveValueOf("e7e5"));
	line.push_back(moveValueOf("f1c5"));
	return CurrentLineInfo<StandardChessMove>(2, line.cbegin(), line.cend());
}



TEST(UciInfo, TestStagedRefutationInfo) {

	RefutationInfo<StandardChessMove> refutation = getStagedRefutationInfo();
	
	ASSERT_EQ(refutation.getRefutedMove(), moveValueOf("e2e4"));
	ASSERT_EQ(refutation.getRefutationLine().size(), 5);
}

TEST(UciInfo, InfoFromRefutationInfo) {
	RefutationInfo<StandardChessMove> refInf = getStagedRefutationInfo();
	Info<StandardChessMove> info(refInf);

	ASSERT_EQ(info.getType(), Refutation);
	ASSERT_EQ(info.getAsRefutationInfo().getRefutedMove(), moveValueOf("e2e4"));
	
	for (size_t i = 0; i < refInf.getRefutationLine().size(); ++i) {
		ASSERT_EQ(info.getAsRefutationInfo().getRefutationLine()[i], refInf.getRefutationLine()[i]);
	}

}

TEST(UciInfo, InfoFromRefutationInfoByFactory) {
	RefutationInfo<StandardChessMove> refInf = getStagedRefutationInfo();
	Info<StandardChessMove> info = InfoFactory<StandardChessMove>::makeRefutationInfo(refInf.getRefutedMove(), refInf.getRefutationLine());

	ASSERT_EQ(info.getType(), Refutation);
	ASSERT_EQ(info.getAsRefutationInfo().getRefutedMove(), moveValueOf("e2e4"));

	for (size_t i = 0; i < refInf.getRefutationLine().size(); ++i) {
		ASSERT_EQ(info.getAsRefutationInfo().getRefutationLine()[i], refInf.getRefutationLine()[i]);
	}
}

TEST(UciInfo, TestStagedInfo) {
	CurrentLineInfo<StandardChessMove> currLine = getStagedCurrentLineInfo();
	ASSERT_EQ(currLine.getCPUnr(), 2);
}

TEST(UciInfo, InfoFromCurrline) {
	CurrentLineInfo<StandardChessMove> currLine = getStagedCurrentLineInfo();
	Info<StandardChessMove> info(currLine);

	ASSERT_EQ(info.getType(), CurrentLine);
	ASSERT_EQ(info.getAsCurrentLineInfo().getCPUnr(), 2);

	for (size_t i = 0; i < currLine.getCurrentLine().size(); ++i) {
		ASSERT_EQ(info.getAsCurrentLineInfo().getCurrentLine()[i], currLine.getCurrentLine()[i]);
	}
}

TEST(UciInfo, CurrentMoveInfo) {
	StandardChessMove m = moveValueOf("e2e4");
	Info<StandardChessMove> info = InfoFactory<StandardChessMove>::makeCurrentMoveInfo(m);

	ASSERT_EQ(info.getType(), CurrentMove);
	ASSERT_EQ(info.getAsCurrentMoveInfo(), m);
}

TEST(UciInfo, StringInfo) {
	Info<StandardChessMove> info = InfoFactory<StandardChessMove>::makeStringInfo("Hello world!");
	ASSERT_EQ(info.getType(), InfoString);
	ASSERT_EQ(info.getStringValue(), "Hello world!");
}

TEST(UciInfo, IntegerInfo) {
	Info<StandardChessMove> info = InfoFactory<StandardChessMove>::makeDepthInfo(5);
	ASSERT_EQ(info.getType(), Depth);
	ASSERT_EQ(info.getIntegerValue(), 5);
}

TEST(UciInfo, ScoreInfo) {
	Info<StandardChessMove> info = InfoFactory<StandardChessMove>::makeScoreInfo(UciScore::fromCentipawns(100, UciScore::Lowerbound));

	ASSERT_EQ(info.getType(), Score);
	ASSERT_EQ(info.getAsScore().getValue(), 100);
	ASSERT_EQ(info.getAsScore().getUnit(), UciScore::Centipawn);
	ASSERT_EQ(info.getAsScore().getBoundType(), UciScore::Lowerbound);
}
