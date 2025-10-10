#include <iostream>

#include <UCILoader/target.h>
#include <UCILoader/EngineConnection.h>
#include <UCILoader/StandardChess.h>
#include <string>

using namespace StandardChess;
using namespace std;
using namespace UCILoader;

std::string toString(const Option & option) {
	std::string tmp;
	switch (option.type()) {
	case Button:
		return option.id() + " (Button)";
	case String:
		return option.id() + " (String) default = " + option.str_content();
	case Check:
		return option.id() + " (Check) default = " + (option.check_content()? "on" : "off");
	case Spin:
		return option.id() + " (Spin) default = " + std::to_string(option.spin_content().value) + 
			" min = " + std::to_string(option.spin_content().min) + " max = " + std::to_string(option.spin_content().max);

	case Combo:
		tmp = option.id() + " (Combo) default = " + option.combo_content().value + " [";
		for (auto& var : option.combo_content().supported_values) 
			tmp += var + ", ";
		tmp.pop_back();
		tmp.pop_back();
		tmp.push_back(']');
		return tmp;
	}

	return "";
}

void displayInfo(const UCILoader::EngineEvent* e) {
	// TODO: refactor Events with payload to make this api easier to use: done
	// TODO: onInfo method for engine instance that does this cast in the name of the user
	
	Info<StandardChessMove> info = *(Info<StandardChessMove>*)e->getPayload();
	
	switch (info.getType()) {
	case Pv:
		cout << "Pv: ";
		for (auto& m : info.getMoveArray()) {
			cout << stringValueOf(m) << " ";
		}
		cout << "\n";
		break;
	}
}

int main() {
	std::string pathToEngine;
	cout << "Enter path to a chess engine: ";
	cin >> pathToEngine;
	cout << "\n";
	auto proces = std::shared_ptr<EngineProcessWrapper>(openEngineProcess({pathToEngine}, "/"));

	UCILoader::EngineInstance<StandardChessMove> instance(proces, make_shared<StandardChessMoveMarschaler>(), make_shared<StandardChessMoveMatcher>());
	instance.sync();
	
	cout << "Engine: " << instance.getName() << "\n";
	cout << "Author: " << instance.getAuthor() << "\n";
	cout << "Detected engine options:\n";
	cout << "Latency: " << instance.ping().count() << " ms\n";
	for (auto& option : instance.options) {
		cout << toString(option.getAsOption()) << "\n";
	}

	instance.connect(displayInfo, UCILoader::NamedEngineEvents::InfoReceived);

	GoParamsBuilder<StandardChessMove> paramsBuilder;
	cout << "Searching for best move\n";
	auto searchConnection = instance.search(paramsBuilder.withMoveTime(500).build(), StartPos());

	searchConnection->waitFor(std::chrono::milliseconds(550));

	auto status = searchConnection->getStatus();

	if (status == UCILoader::ResultReady) {
		cout << "Best starting move is " << stringValueOf(*searchConnection->getResult().bestMove) << "\n";
		if (searchConnection->getResult().ponderMove)
			cout << "Engine thinks " << stringValueOf(*searchConnection->getResult().ponderMove) << " is best response for black\n";
	}
	else {
		cout << "Search status code timed out with a status of " << status<<'\n';
	}


	instance.quit();

}
