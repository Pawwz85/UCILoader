#include <UCILoader/StandardChess.h>
#include <UCILoader/EngineConnection.h>
#include <iostream>

using namespace UCILoader;
using namespace StandardChess;

int main(int argc, char ** argv) {

    if (argc < 2) {
        std::cerr << "ERROR: Missing path to engine binary\n";
        return EXIT_FAILURE;
    }

    std::string pathToChal(argv[1]);
    ProcessWrapper * proccess = openProcess({pathToChal}, "/");
    std::unique_ptr<EngineInstance<StandardChessMove>> instance;
    
    try {
        instance.reset(ChessEngineInstanceBuilder->build(proccess));
        instance->sync(std::chrono::milliseconds(1000));
    } catch (const std::exception & e) {
        std::cerr << "ERROR: Exception during engine init: " << e.what() << "\n";
        return EXIT_FAILURE;
    };

    std::string engineName = instance->getName();

    if (engineName != "Chal 1.4.0") {
        std::cerr << "ERROR: Unexpected engine name. Expected 'Chal 1.4.0', got '"
                  << engineName << "'\n";
        instance->quit();
        return EXIT_FAILURE;
    };

    instance->quit();
    return EXIT_SUCCESS;
};