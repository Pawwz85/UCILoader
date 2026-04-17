#include <UCILoader/StandardChess.h>
#include <UCILoader/EngineConnection.h>
#include <iostream>
#include <mutex>

using namespace UCILoader;
using namespace StandardChess;

class CustomLogger : public Logger {
    std::mutex lock;
    public:
    bool readyOkReceived = false;
    bool isReadySend  = false;

    void log(MessageDirection dir, const std::string & msg) override {
        std::lock_guard<std::mutex> guard(lock); 

        if (dir == Logger::FromEngine && msg == "readyok\n")
            readyOkReceived = true;

        if (dir == Logger::ToEngine && msg == "isready\n")
            isReadySend = true;

        std::cout << std::string(dir == Logger::FromEngine ? "<: " : ">: ") +  msg;
    }
    
};
int main(int argc, char ** argv) {

    if (argc < 2) {
        std::cerr << "ERROR: Missing path to engine binary\n";
        return EXIT_FAILURE;
    }

    std::string pathToChal(argv[1]);
    ProcessWrapper * proccess = openProcess({pathToChal}, "/");
    std::unique_ptr<EngineInstance<StandardChessMove>> instance;
    CustomLogger * logger = new CustomLogger;
    try {
        instance.reset(ChessEngineInstanceBuilder->build(proccess, logger));
        instance->sync(std::chrono::milliseconds(1000));
    } catch (const std::exception & e) {
        std::cerr << "ERROR: Exception during engine init: " << e.what() << "\n";
        return EXIT_FAILURE;
    };

    instance->quit();

    if (!logger->isReadySend) {
        std::cerr << "ERROR: isready command was not logged \n";
        return EXIT_FAILURE;
    }

    if (!logger->readyOkReceived) {
        std::cerr << "ERROR: readyok response was not logged \n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
