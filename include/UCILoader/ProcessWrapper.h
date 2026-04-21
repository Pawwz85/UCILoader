#pragma once

#include "AbstractPipe.h"

#include <thread>
#include <memory>
#include <cassert>
#include <cstring>
#include <functional>

namespace UCILoader {
	/*!
		An exception that openProcess() function will throw if it can not open the engine for any reason. 
	*/
	class CanNotOpenProcessException : std::exception {

		std::string reason;
	public:

		CanNotOpenProcessException(const std::string& reason) : reason(reason) {};

		const char* what() const noexcept override {
			return reason.c_str();
		}
	};

	/*!
		A cross platform wrapper around an engine process. Although the user will generally pass the control over this
		process to an EngineInstance object, the ProcessWrapper class can be used independently in more general context
		(see proxy example in the "examples" folder). 
	*/
	class ProcessWrapper {

		std::unique_ptr<std::thread> listener = nullptr;
		bool healthCheckFailed = false;

	protected:
		/*!
			Get a pipe reader connected to the process's standard output. See AbstractPipe.h for details about pipe abstraction.
		*/
		virtual std::shared_ptr<AbstractPipeReader> getReader() = 0;

	public:

		virtual ~ProcessWrapper() {
			if (listener) listener->join();
		};

		/*!
			Get a pipe writer connected to the process's standard input. See AbstractPipe.h for details about pipe abstraction.
		*/
		virtual std::shared_ptr<AbstractPipeWriter> getWriter() = 0;

		/*!
			Kills the underlying process by an OS calls.
		*/
		virtual void kill() = 0;
		/*!
			Checks if the process is still alive using OS calls. It is not recommended to call this function directly
			since on Windows it is possible that isAlive() function returns false when process is still somewhat still
			running, leading to dangling process. Use healthcheck()  method as safer alternative.
		*/
		virtual bool isAlive() const = 0;
		/*!
			Checks if the process is still alive using isAlive() method, while still taking care of windows edge case by
			preemptively killing underlying process as soon as isAlive fails for the first time.
		*/
		virtual bool healthCheck() {
			if (!healthCheckFailed && !isAlive()) {
				kill();
				healthCheckFailed = true;
			}
			return !healthCheckFailed;
		}

		/*!
			Starts a new listening thread using provide callback arguments. 

			The first argument of this method is a callback that will be used to process each line received
			from process.

			Second argument is optional and defines a callback that will be called when the listener thread crashed
			due to underlying pipe being closed
		*/
		void listen(std::function<void(std::string)> lineReceiver, std::function<void()> onCrash = [](){}) {
			assert(listener == nullptr);

			PipeScanner* scanner = new PipeScanner(std::move(getReader()));

			listener = std::make_unique<std::thread>(
				[scanner, lineReceiver, onCrash]() {
					while (true) {
						try {
							lineReceiver(scanner->getLine());
						}
						catch (PipeClosedException) {
							onCrash();
							break;
						}
					}

					delete scanner;
				}
			);

		};
	};

	/*!
		Opens generic process, throws CanNotOpenProcessException if operation failed for any reason.
	*/
	ProcessWrapper* openProcess(const std::vector<std::string>& args, const std::string& workingDirectory);
}

