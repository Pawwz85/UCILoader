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

    bool crashed = false;

    instance->connect( [&crashed]() {crashed = true;}, NamedEngineEvents::EngineCrashed);
    proccess->kill();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (!crashed) {
       std::cerr << "Engine crash event was not emitted";
       return EXIT_FAILURE;
   };

    return EXIT_SUCCESS;
};