#pragma once

#include <chrono>
#include <mutex>

#include <functional>
#include <memory>
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

    class LoggerTrait {
        public:
        virtual ~LoggerTrait() = default;
        virtual std::unique_ptr<Logger> addTo(std::unique_ptr<Logger> && logger) const = 0;
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

    

    namespace Loggers {
        extern LoggerBuilder toNoting();
        extern LoggerBuilder toStd();
        extern LoggerBuilder toFile(const std::string & filename);
        extern LoggerBuilder toFile(const char * filename);

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
        extern const LoggerTrait & SilenceParser;
    };

};