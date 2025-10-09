#pragma once

#include "AbstractPipe.h"

#include <thread>
#include <memory>
#include <cassert>
#include <cstring>
#include <functional>

namespace UCILoader {
	class CanNotOpenEngineException : std::exception {

		std::string reason;
	public:

		CanNotOpenEngineException(const std::string& reason) : reason(reason) {};

		const char* what() const override {
			return reason.c_str();
		}
	};


	class EngineProcessWrapper {

		std::unique_ptr<std::thread> listener = nullptr;
		bool healthCheckFailed = false;

	protected:
		virtual std::shared_ptr<AbstractPipeReader> getReader() = 0;

	public:

		virtual ~EngineProcessWrapper() {
			if (listener) listener->join();
		};

		virtual std::shared_ptr<AbstractPipeWriter> getWriter() = 0;
		virtual void kill() = 0;
		virtual bool isAlive() const = 0;

		virtual bool healthCheck() {
			if (!healthCheckFailed && !isAlive()) {
				kill();
				healthCheckFailed = true;
			}
			return !healthCheckFailed;
		}

		void listen(std::function<void(std::string)> lineReceiver) {
			assert(listener == nullptr);

			PipeScanner* scanner = new PipeScanner(std::move(getReader()));

			listener = std::make_unique<std::thread>(
				[scanner, lineReceiver]() {
					while (true) {
						try {
							lineReceiver(scanner->getLine());
						}
						catch (PipeClosedException) {
							break;
						}
					}

					delete scanner;
				}
			);

		};
	};

	/*
		Opens generic process, throws CanNotOpenEngineException if operation failed for any reason.
	*/
	EngineProcessWrapper* openEngineProcess(const std::vector<std::string>& args, const std::string& workingDirectory);
}

