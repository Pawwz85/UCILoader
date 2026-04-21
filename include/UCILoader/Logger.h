#pragma once

#include <chrono>
#include <mutex>
#include <ostream>
#include <functional>
#include <vector>
#include <memory>
#include <mutex>
#include <ostream>
#include <type_traits>

namespace UCILoader {

    class Logger {
        public:
        enum MessageDirection {
            ToEngine,
            FromEngine,
            FromParser
        };
        virtual ~Logger() = default;
        virtual void log(MessageDirection dir, const std::string & msg) = 0;
    };
    
    class LoggerWrapper : public Logger {
        std::unique_ptr<Logger> wrapped;
        protected:
        void delegate(MessageDirection dir, const std::string & msg);
        public:
        LoggerWrapper(std::unique_ptr<Logger> && logger);   
    };

    class ComposedLoggerTrait;

    class LoggerTrait {
        public:
        virtual ~LoggerTrait() = default;
        virtual std::unique_ptr<Logger> addTo(std::unique_ptr<Logger> && logger) const = 0;
   };
   
   class ComposedLoggerTrait : public LoggerTrait {
        public:
   };

    class LoggerBuilder {
        std::unique_ptr<Logger> logger;
        public:
        LoggerBuilder(std::unique_ptr<Logger> && base);
        LoggerBuilder(LoggerBuilder && other);
        LoggerBuilder & addTrait(const LoggerTrait & trait);
        std::unique_ptr<Logger> build();

        friend LoggerBuilder operator |(LoggerBuilder builder, const LoggerTrait & trait) {
            builder.addTrait(trait);
            return std::move(builder);
        };
    };

    class LogBuffer;

    namespace Loggers {
        extern LoggerBuilder toNoting();
        extern LoggerBuilder toStd();
        extern LoggerBuilder toStderr();
        extern LoggerBuilder toFile(const std::string & filename);
        extern LoggerBuilder toFile(const char * filename);
        extern LoggerBuilder toBuffer(LogBuffer & buffer); 
        extern LoggerBuilder toCallback(std::function<void(Logger::MessageDirection, const std::string & msg)> callback);
        extern LoggerBuilder toOstream(std::ostream & os);

        // todo: tee Logger

        template <class CustomLogger, typename ... args>
        LoggerBuilder from(args... params) {
            static_assert(std::is_base_of_v<Logger, CustomLogger>,
                      "CustomLogger must derive from Logger");
            auto logger = std::make_unique<CustomLogger>(params...);
            return LoggerBuilder(std::move(logger));
        };
    };

    namespace LoggerTraits {
        extern const LoggerTrait & Pretty;
        extern const LoggerTrait & IgnoreParser;
        extern const LoggerTrait & IgnoreEngine; 
        extern const LoggerTrait & IgnoreApplication;
        extern const LoggerTrait & Timestamp;   
    };

    // Forward declaration of log buffer implementation details.
    namespace _LogBufferPrivate {
        struct CriticalSection;
        class Friend;
    };

    class LogBuffer {
        std::shared_ptr<_LogBufferPrivate::CriticalSection> criticalSection;
        friend _LogBufferPrivate::Friend;
        public:
        void push_back(const std::string & msg);
        std::vector<std::string> snapshot() const;
    };

    namespace _LogBufferPrivate{
        struct CriticalSection {
            std::mutex lock;
            std::vector<std::string> buffer;
        };
        class Friend {
            protected:
            std::shared_ptr<CriticalSection> criticalSection;
            public:
            Friend(LogBuffer & buffer) {
                this->criticalSection = buffer.criticalSection;
            }
        };
    };
};