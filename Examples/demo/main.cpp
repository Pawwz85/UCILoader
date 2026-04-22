#include <iostream>

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
	// TODO: implement onInfo method for the engine instance that does this cast in the name of the user
	
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

	// Open the engine specified by a user
	auto proces = openProcess({pathToEngine}, "/");

	// use  ChessEngineInstanceBuilder from StandardChess.h to get engine instance using default move parser
	// Save all communication to a "Logs.txt" file using 'Pretty' formatter
	auto instance = ChessEngineInstanceBuilder->build(proces, Loggers::toFile("Logs.txt") | LoggerTraits::Pretty);
	
	// wait for engine to finish initializing
	instance->sync();
	
	// Get information about engine name and author (default is <empty>)
	cout << "Engine: " << instance->getName() << "\n";
	cout << "Author: " << instance->getAuthor() << "\n";

	// Measure latency using ping() command
	cout << "Latency: " << instance->ping().count() << " ms\n";

	// iterate over detected engine options
	cout << "Detected engine options:\n";
	for (auto& option : instance->options) {
		cout << toString(option.getAsOption()) << "\n";
	}

	if (instance->options.contains("UCI_ShowWDL")) {
		instance->options["UCI_ShowWDL"] = true;
		/*
			For the check option, you could also use string values [on/off] or [true/false]:
			instance->options["UCI_ShowWDL"] = "true";
			instance->options["UCI_ShowWDL"] = "on";
		*/
	}

	if (instance->options.contains("Hash")) {
		int defaultHash = instance->options["Hash"];
		cout << "Engine uses by default hash table of size " << defaultHash << "\n";
	}


	// register callback for handling incoming infos
	instance->connect(displayInfo, UCILoader::NamedEngineEvents::InfoReceived);

	// start a search request and obtain search connection
	cout << "Searching for best move\n";
	GoParamsBuilder<StandardChessMove> paramsBuilder;
	auto searchConnection = instance->search(paramsBuilder.withMoveTime(500).build(), StartPos()); // engine has 0.5s to think over the starting position

	// wait up to 550 milliseconds for the engine to finish, declare timeout if that did not happened
	searchConnection->waitFor(std::chrono::milliseconds(550));

	// check search status
	auto status = searchConnection->getStatus();
	if (status == UCILoader::ResultReady) {

		cout << "Best starting move is " << stringValueOf(*searchConnection->getResult().bestMove) << "\n";
		if (searchConnection->getResult().ponderMove) // checks if engine  send pondermove candite in the resulting string
			cout << "Engine thinks " << stringValueOf(*searchConnection->getResult().ponderMove) << " is best response for black\n";
	}
	else {
		cout << "Search status code timed out with a status code: " << status<<'\n';
	}

	// Resource cleanup
	delete instance;


	// stop the script for a while so user could read its output before cmd window disappears  
	cout << "Program finished. Write anything to continue\n" ;
	char c;
	cin >> c;
}
