#include <UCILoader/StandardChess.h>
#include <UCILoader/EngineConnection.h>
#include <iostream>
#include <mutex>

using namespace UCILoader;
using namespace StandardChess;

struct ResultSection {
    bool readyOkReceived = false;
    bool isReadySend  = false;
};

class CustomLogger : public Logger {
    std::mutex lock;
    ResultSection * result;
    public:
    CustomLogger(ResultSection * section) : result(section) {};

    void log(MessageDirection dir, const std::string & msg) override {
        std::lock_guard<std::mutex> guard(lock); 

        if (dir == Logger::FromEngine && msg == "readyok\n")
            result->readyOkReceived = true;

        if (dir == Logger::ToEngine && msg == "isready\n")
            result->isReadySend = true;

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

    ResultSection result;

    try {
        instance.reset(ChessEngineInstanceBuilder->build(proccess, Loggers::from<CustomLogger>(&result)));
        instance->sync(std::chrono::milliseconds(1000));
    } catch (const std::exception & e) {
        std::cerr << "ERROR: Exception during engine init: " << e.what() << "\n";
        return EXIT_FAILURE;
    };

    instance->quit();

    if (!result.isReadySend) {
        std::cerr << "ERROR: isready command was not logged \n";
        return EXIT_FAILURE;
    }

    if (!result.readyOkReceived) {
        std::cerr << "ERROR: readyok response was not logged \n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
