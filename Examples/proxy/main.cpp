#include <iostream>

#include <UCILoader/target.h>
#include <UCILoader/EngineLoader.h>

using namespace std;
using namespace UCILoader;

struct remoteProcessInfo {
	std::string path_to_exe;
	std::string name;
};

std::string removeQuotes(const std::string& s, bool& quotesFound) {
	quotesFound = 0;
	std::string result;

	for (char c : s) {
		if (c == '"' || c == '\'') {
			quotesFound = 1;
		}
		else
			result.push_back(c);
	}
	return result;
}

std::string deduceWorkingDirectory(const std::string & executable){

	bool containsQuotes = false;
	std::string sanitisedString = removeQuotes(executable, containsQuotes);

	size_t lastSep = sanitisedString.find_last_of("/\\");

	std::string result = sanitisedString.substr(0, lastSep + 1);
	result = containsQuotes ? "\"" + result + "\"" : result;

	cout << result << endl;
	return result;
};

EngineProcessWrapper* promptForProcess(remoteProcessInfo & info) {
	EngineProcessWrapper* result = nullptr;

	std::string path_to_process;

	while (result == nullptr) {
		cout << "What process do you wish to open?\n";
		std::getline(cin, path_to_process);
		cout << endl;

		try {
			result = openEngineProcess({ path_to_process },"/");
		}
		catch (CanNotOpenEngineException e) {
			cout << "Failed to open " << path_to_process << ", because " << e.what() << endl;
		}

	}

	info.path_to_exe = path_to_process;

	info.name = "remote";

	return result;
}

int main() {

	remoteProcessInfo info;
	EngineProcessWrapper* process = promptForProcess(info);

	process->listen([&info](std::string line) {
		cout << info.name << ": " << line << endl;
		});


	std::string command = "uci";

	while (process->healthCheck()) {
		getline(cin, command);
		command.push_back('\n');
		try {
			process->getWriter()->write(command.c_str(), command.size());
		}
		catch (PipeClosedException e) {
			cout << "Program pipe seems to be closed\n";
			break;
		}
		
		this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	cout << "Process crashed\n";

	delete process;
	return 0;
}