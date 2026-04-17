#pragma once

#include <chrono>
#include <mutex>

#include <functional>
#include <memory>
#include <ostream>

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

    namespace Loggers {
        extern Logger* toNoting();
        extern Logger* toStd();
        extern Logger* toFile(const std::string & filename);
        extern Logger* toFile(const char * filename);
    };
};