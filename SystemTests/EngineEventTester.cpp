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

    bool crashed = false;
    bool synchronized = false;
    bool searchCompleted = false;
    bool searchStarted = false;

    std::string pathToChal(argv[1]);
    ProcessWrapper * proccess = openProcess({pathToChal}, "/");
    std::unique_ptr<EngineInstance<StandardChessMove>> instance;
    
    try {
        instance.reset(ChessEngineInstanceBuilder->build(proccess));
        instance->connect([&synchronized]() { synchronized = true; }, NamedEngineEvents::EngineSynchronized);
        instance->sync(std::chrono::milliseconds(1000));
    } catch (const std::exception & e) {
        std::cerr << "ERROR: Exception during engine init: " << e.what() << "\n";
        return EXIT_FAILURE;
    };

    if (!synchronized) {
        std::cerr << "ERROR: Engine synchronized event was not emitted\n";
        return EXIT_FAILURE;
    }

    instance->connect( [&searchStarted]() {searchStarted = true;}, NamedEngineEvents::SearchStarted);
    instance->connect( [&searchCompleted]() {searchCompleted = true;}, NamedEngineEvents::SearchCompleted);
    instance->ucinewgame();
    auto searchConnection = instance->search( GoParamsBuilder<StandardChess::StandardChessMove>().withDepth(6).build(), StandardChess::StartPos(), {});
    searchConnection->waitFor(std::chrono::milliseconds(2000));

    if(!searchStarted) {
        std::cerr << "ERROR: Search started event was not emitted\n";
        return EXIT_FAILURE;
    };

    if (!searchCompleted) {
        std::cerr << "ERROR: Search completed event was not emitted\n";
        return EXIT_FAILURE;
    };

    instance->connect( [&crashed]() {crashed = true;}, NamedEngineEvents::EngineCrashed);
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    if (crashed) {
       std::cerr << "Engine crash event was emitted before engine crashed";
       return EXIT_FAILURE;
  };

    proccess->kill();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (!crashed) {
       std::cerr << "Engine crash event was not emitted";
       return EXIT_FAILURE;
   };

    return EXIT_SUCCESS;
};