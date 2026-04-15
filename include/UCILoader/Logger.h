#pragma once

#include <chrono>
#include <mutex>

#include <functional>

namespace UCILoader {

    class Logger {
        public:
        enum MessageDirection {
            ToEngine,
            FromEngine
        };

        using LoggingFunctor = typename std::function<void(MessageDirection, const std::string & msg)>;
        private:

        LoggingFunctor functor;

        protected:
        Logger() = default; // protected. Useful for derived classes that override log method, useless otherwise.  

        public:

        Logger(LoggingFunctor & functor)  : functor(functor) {};
        Logger(LoggingFunctor && functor) : functor(functor) {};

        virtual ~Logger() = default;
        virtual void log(MessageDirection direction, const std::string & msg) {
            functor(direction, msg);
        };
    };

    class NoopLogger : public Logger {
        public:
        NoopLogger() = default;
        void log(MessageDirection direction, const std::string & msg) override {};
    };
};