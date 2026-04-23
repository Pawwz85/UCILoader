// Force Microsoft to follow the C++ standard for localtime_s
#define _CRT_USE_CONFORMING_ANNEX_K_TIME 1

// ask c compiler for localtime_s function
#define __STDC_WANT_LIB_EXT1__ 1

#include <UCILoader/Logger.h>
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <time.h>
#include <sstream>
#include <iomanip>



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

class StdErrLogger : public Logger {
    std::mutex lock;
    void log(MessageDirection dir, const std::string & msg) override {
        std::lock_guard<std::mutex> guard(lock);
        std::cerr << msg;
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

class LogBufferLogger : public Logger, public _LogBufferPrivate::Friend {
    public:
    LogBufferLogger(LogBuffer & buff) : Friend(buff) {};
    void log(MessageDirection dir, const std::string & msg) override {
        std::lock_guard<std::mutex> guard(criticalSection->lock);
        criticalSection->buffer.push_back(msg);
    };
};

class CallbackLogger : public Logger {
    public:
    using Clb = typename std::function<void(Logger::MessageDirection, const std::string &)>;
    Clb callback;
    CallbackLogger(Clb callback) : callback(std::move(callback)) {};
    void log(MessageDirection dir, const std::string & msg) override {
        callback(dir, msg);
    }
};

class OstreamRefLogger : public Logger {
    std::mutex lock;
    std::ostream * out;
    public:
    OstreamRefLogger(std::ostream & out) : out(&out) {};
    void log(MessageDirection dir, const std::string & msg) override{
        std::lock_guard<std::mutex> guard(lock);
        *out<<msg;
    }
};

class LoggerWithPrettyTrait : public LoggerWrapper {
    public:
    LoggerWithPrettyTrait(std::unique_ptr<Logger> && logger) : LoggerWrapper(std::move(logger)) {};
    void log(MessageDirection dir, const std::string & msg) override {
        std::string prefix;
        switch (dir)
        {
        case ToEngine:
            prefix = ">> ";
            break;
        case FromEngine:
            prefix = "<< ";
            break;
        case FromParser:
            prefix = "Parser: ";
            break;
        };
        std::string newLine = prefix + msg;
        delegate(dir, newLine);
    };
};

template <Logger::MessageDirection IgnoredDirection> 
class LoggerWithIgnoredDirectionTrait : public LoggerWrapper {
    public:
    LoggerWithIgnoredDirectionTrait(std::unique_ptr<Logger> && logger) : LoggerWrapper(std::move(logger)) {};
    void log(MessageDirection dir, const std::string & msg) override {
        if (dir != IgnoredDirection)
            delegate(dir, msg);
    };
};

class LoggerWithTimestampTrait : public LoggerWrapper {
    std::string makeTimestamp() {
        time_t now = time(nullptr);
        tm tm {};

        #ifdef __STDC_LIB_EXT1__
            if(localtime_s(&now, &tm) == nullptr)
                return "[##:##:##] ";
        #else
            struct tm * tmp;
            if((tmp = localtime(&now)) == nullptr)
                return "[##:##:##] ";
            tm = *tmp;
        #endif

        
        std::stringstream ss;
        ss << std::put_time(&tm, "[%T] ");
        std::string result;
        ss >> result;
        return result;
    };
    public:
    LoggerWithTimestampTrait(std::unique_ptr<Logger> && logger) : LoggerWrapper(std::move(logger)) {};
    void log(MessageDirection dir, const std::string & msg) override {
        return delegate(dir, makeTimestamp() + msg);
    };
};

template <class WrapperClass>
class WrapperTrait : public LoggerTrait {
    std::unique_ptr<Logger> addTo(std::unique_ptr<Logger> && logger) const override {
        return std::make_unique<WrapperClass>(std::move(logger));
    };

};

WrapperTrait<LoggerWithPrettyTrait> prettyLoggerInstance;
WrapperTrait<LoggerWithIgnoredDirectionTrait<Logger::FromParser>> ignoredParserLoggerInstance;
WrapperTrait<LoggerWithIgnoredDirectionTrait<Logger::ToEngine>>   ignoreEngineLoggerInstance;
WrapperTrait<LoggerWithIgnoredDirectionTrait<Logger::FromEngine>> ignoreApplicationLoggerInstance;
WrapperTrait<LoggerWithTimestampTrait> timestampLoggerInstance;

const LoggerTrait & LoggerTraits::Pretty = prettyLoggerInstance;
const LoggerTrait & LoggerTraits::IgnoreParser = ignoredParserLoggerInstance;
const LoggerTrait & LoggerTraits::IgnoreEngine = ignoreEngineLoggerInstance;
const LoggerTrait & LoggerTraits::IgnoreApplication = ignoreApplicationLoggerInstance;
const LoggerTrait & LoggerTraits::Timestamp = timestampLoggerInstance; 

LoggerBuilder UCILoader::Loggers::toNoting(){
    auto logger = std::make_unique<NoopLogger>();
    return LoggerBuilder(std::move(logger));
}

LoggerBuilder UCILoader::Loggers::toStd(){
    auto logger = std::make_unique<StdLogger>();
    return LoggerBuilder(std::move(logger));
}

LoggerBuilder UCILoader::Loggers::toStderr(){
    auto logger = std::make_unique<StdErrLogger>();
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

LoggerBuilder UCILoader::Loggers::toBuffer(LogBuffer & buffer){
    auto logger = std::make_unique<LogBufferLogger>(buffer);
    return LoggerBuilder(std::move(logger));
}

LoggerBuilder UCILoader::Loggers::toCallback(std::function<void(Logger::MessageDirection, const std::string & msg)> callback) {
    auto logger = std::make_unique<CallbackLogger>(callback);
    return LoggerBuilder(std::move(logger));
}

LoggerBuilder UCILoader::Loggers::toOstream(std::ostream & os) {
    auto logger = std::make_unique<OstreamRefLogger>(os);
    return LoggerBuilder(std::move(logger));
};

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

void UCILoader::LogBuffer::push_back(const std::string &msg){
    std::lock_guard<std::mutex> guard(criticalSection->lock);
    criticalSection->buffer.push_back(msg);
}

std::vector<std::string> UCILoader::LogBuffer::snapshot() const{
    std::lock_guard<std::mutex> guard(criticalSection->lock);
    return criticalSection->buffer;
}
