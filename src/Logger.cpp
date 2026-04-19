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

class PrettyLogger : public LoggerWrapper {
    public:
    PrettyLogger(std::unique_ptr<Logger> && logger) : LoggerWrapper(std::move(logger)) {};
    void log(MessageDirection dir, const std::string & msg) override {
        std::string prefix;
        switch (dir)
        {
        case ToEngine:
            prefix = ">";
            break;
        case FromEngine:
            prefix = "<";
            break;
        case FromParser:
            prefix = "(Parser)";
            break;
        };
        std::string newLine = prefix + msg;
        delegate(dir, newLine);
    };
};

class SilencedParserLogger : public LoggerWrapper {
    public:
    SilencedParserLogger(std::unique_ptr<Logger> && logger) : LoggerWrapper(std::move(logger)) {};
    void log(MessageDirection dir, const std::string & msg) override {
        if (dir != FromParser)
            delegate(dir, msg);
    };
};

template <class WrapperClass>
class WrapperTrait : public LoggerTrait {
    std::unique_ptr<Logger> addTo(std::unique_ptr<Logger> && logger) const override {
        return std::make_unique<WrapperClass>(std::move(logger));
    };

};

WrapperTrait<PrettyLogger> prettyLoggerInstance;
WrapperTrait<SilencedParserLogger> silencedParserLoggerInstance;

const LoggerTrait & LoggerTraits::Pretty = prettyLoggerInstance;
const LoggerTrait & LoggerTraits::SilenceParser = silencedParserLoggerInstance;

LoggerBuilder UCILoader::Loggers::toNoting(){
    auto logger = std::make_unique<NoopLogger>();
    return LoggerBuilder(std::move(logger));
}

LoggerBuilder UCILoader::Loggers::toStd(){
    auto logger = std::make_unique<StdLogger>();
    return LoggerBuilder(std::move(logger));
}

LoggerBuilder UCILoader::Loggers::toFile(const std::string &filename){
    auto logger = std::make_unique<OstreamLogger<std::ofstream>>(filename);
    return LoggerBuilder(std::move(logger));
}

LoggerBuilder UCILoader::Loggers::toFile(const char *filename){
    auto logger = std::make_unique<OstreamLogger<std::ofstream>>(filename);
    return LoggerBuilder(std::move(logger));
}

void UCILoader::LoggerWrapper::delegate(MessageDirection dir, const std::string &msg){
    wrapped->log(dir, msg);
}

UCILoader::LoggerWrapper::LoggerWrapper(std::unique_ptr<Logger> &&logger) : wrapped(std::move(logger)){
}

UCILoader::LoggerBuilder::LoggerBuilder(std::unique_ptr<Logger> &&base) : logger(std::move(base)){}

UCILoader::LoggerBuilder::LoggerBuilder(LoggerBuilder &&other) : logger(std::move(other.logger)){}

LoggerBuilder &UCILoader::LoggerBuilder::addTrait(const LoggerTrait &trait){
    std::unique_ptr<Logger> tmp = trait.addTo(std::move(logger));
    logger.reset(tmp.release()); 
    return *this;
}

std::unique_ptr<Logger> UCILoader::LoggerBuilder::build(){
    return std::move(logger);
}
