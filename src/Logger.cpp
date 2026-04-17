#include <UCILoader/Logger.h>
#include <iostream>
#include <fstream>
#include <mutex>

using namespace UCILoader;

class NoopLogger : public Logger {
    void log(MessageDirection dir, const std::string & msg) override {};
};

class StdLogger : public Logger {
    std::mutex lock;
    void log(MessageDirection dir, const std::string & msg) override {
        std::lock_guard<std::mutex> guard(lock);
        std::cout << msg;
    };
};

template <class OstreamType>
class OstreamLogger : public Logger {
    OstreamType out;
    std::mutex lock;
    void log(MessageDirection dir, const std::string & msg) override {
        std::lock_guard<std::mutex> guard(lock);
        out << msg;
    };
    public:

    template <typename ... args>
    OstreamLogger(args ... params) : out(params...) {
    };
    
    ~OstreamLogger() {
        out.close();
    }
};


Logger *UCILoader::Loggers::toNoting(){
    return new NoopLogger;
}

Logger *UCILoader::Loggers::toStd(){
    return new StdLogger;
}

Logger *UCILoader::Loggers::toFile(const std::string &filename){
    return new OstreamLogger<std::ofstream>(filename);
}

Logger *UCILoader::Loggers::toFile(const char *filename){
    return new OstreamLogger<std::ofstream>(filename);
}
