#pragma once

#include "AbstractPipe.h"

#include <thread>
#include <memory>
#include <cassert>
#include <cstring>
#include <functional>

namespace UCILoader {
	/*!
	 * @brief Exception thrown when a process cannot be opened or started.
	 * 
	 * @details
	 * This exception is thrown by openProcess() when the requested engine executable cannot be started
	 * for any reason (file not found, permission denied, corrupt executable, etc.).
	 * 
	 * The exception message contains details about why the process failed to open.
	 */
	class CanNotOpenProcessException : std::exception {

		std::string reason;
	public:

		/*!
		 * @brief Constructor for the exception.
		 * 
		 * @param reason Human-readable explanation of why the process failed to open
		 */
		CanNotOpenProcessException(const std::string& reason) : reason(reason) {};

		/*!
		 * @brief Get the exception message explaining the failure.
		 * 
		 * @return C-string describing why the process couldn't be opened
		 */
		const char* what() const noexcept override {
			return reason.c_str();
		}
	};

	/*!
	 * @brief Cross-platform wrapper for managing a chess engine process.
	 * 
	 * @details
	 * ProcessWrapper provides platform-independent abstraction for creating, monitoring, and
	 * communicating with external processes (chess engines). It handles:
	 * - Process creation and lifecycle management
	 * - Bidirectional I/O through pipes (stdin/stdout)
	 * - Process health monitoring
	 * - Asynchronous line-based input listening with callbacks
	 * 
	 * While ProcessWrapper is typically passed to an EngineInstance for automatic management,
	 * it can also be used independently for custom use cases (see proxy example in examples/ folder).
	 * 
	 * **Platform-Specific Implementations:**
	 * - Windows: Uses WinProcessWrapper with Windows pipes
	 * - Unix/Linux: Uses UnixProcessWrapper with POSIX pipes
	 * 
	 * **Typical Usage:**
	 * @code
	 *     // Create process
	 *     auto process = openProcess({"stockfish.exe"}, "/");
	 *     
	 *     // Set up line-based listening
	 *     process->listen(
	 *         [](const std::string& line) {
	 *             std::cout << "Engine: " << line << std::endl;
	 *         },
	 *         []() {
	 *             std::cerr << "Engine crashed!" << std::endl;
	 *         }
	 *     );
	 *     
	 *     // Send data to process
	 *     process->getWriter()->write("uci\n", 4);
	 *     
	 *     // Check if process is healthy
	 *     if (!process->healthCheck()) {
	 *         std::cout << "Engine terminated" << std::endl;
	 *     }
	 * @endcode
	 * 
	 * @see EngineInstance for typical usage
	 * @see openProcess for creating instances
	 */
	class ProcessWrapper {

		std::unique_ptr<std::thread> listener = nullptr;
		bool healthCheckFailed = false;

	protected:
		/*!
		 * @brief Get the pipe reader for the process's standard output.
		 * 
		 * @return Shared pointer to AbstractPipeReader connected to stdout
		 * 
		 * @details
		 * This is a protected method implemented by platform-specific subclasses.
		 * Used internally by listen() to set up line reading from the process.
		 * 
		 * @see AbstractPipe.h for pipe abstraction details
		 */
		virtual std::shared_ptr<AbstractPipeReader> getReader() = 0;

	public:

		/*!
		 * @brief Virtual destructor that ensures listening thread is properly joined.
		 */
		virtual ~ProcessWrapper() {
			if (listener) listener->join();
		};

		/*!
		 * @brief Get the pipe writer for the process's standard input.
		 * 
		 * @return Shared pointer to AbstractPipeWriter connected to stdin
		 * 
		 * @details
		 * Used to send commands to the process. The returned writer can be used independently
		 * at any time after construction.
		 * 
		 * @see AbstractPipe.h for pipe abstraction details
		 */
		virtual std::shared_ptr<AbstractPipeWriter> getWriter() = 0;

		/*!
		 * @brief Immediately terminate the underlying process.
		 * 
		 * @details
		 * Uses platform-specific OS calls to forcefully kill the process.
		 * This is safe to call multiple times.
		 */
		virtual void kill() = 0;
		/*!
		 * @brief Check if the process is alive using OS calls.
		 * 
		 * @return True if the process is still running, false otherwise
		 * 
		 * @warning On Windows, this may return false while the process is still somewhat running,
		 * potentially leading to dangling processes. Use healthCheck() instead for more robust checking.
		 * 
		 * @see healthCheck() for the safer alternative
		 */
		virtual bool isAlive() const = 0;
		/*!
		 * @brief Perform a safe health check of the process.
		 * 
		 * @return True if the process is alive and healthy, false if it has terminated or crashed
		 * 
		 * @details
		 * Unlike isAlive(), this method handles platform-specific edge cases:
		 * - On first detection of process death, immediately kills it to prevent dangling processes
		 * - Subsequent calls return the cached failure state to avoid repeated OS calls
		 * 
		 * Safe to call repeatedly without performance penalty.
		 */
		virtual bool healthCheck() {
			if (!healthCheckFailed && !isAlive()) {
				kill();
				healthCheckFailed = true;
			}
			return !healthCheckFailed;
		}

		/*!
		 * @brief Start a background listening thread for reading process output lines.
		 * 
		 * @param lineReceiver Callback function invoked for each line received from the process.
		 *                     The string is passed without the trailing newline character.
		 * @param onCrash Optional callback invoked when the listening thread terminates due to
		 *                pipe closure (process crash or EOF). Defaults to a no-op lambda.
		 * 
		 * @details
		 * This method spawns a background thread that continuously:
		 * 1. Reads lines from the process's stdout via PipeScanner
		 * 2. Invokes lineReceiver callback for each complete line
		 * 3. If pipe closes, invokes onCrash callback and terminates
		 * 
		 * **Important Notes:**
		 * - This method must be called only once per ProcessWrapper instance
		 * - The callbacks are invoked from the listening thread, not the calling thread
		 * - Use thread-safe mechanisms (locks, atomics) in callbacks if accessing shared state
		 * - The listening thread continues until the process exits or pipe closes
		 * 
		 * **Usage Example:**
		 * @code
		 *     process->listen(
		 *         [&state](const std::string& line) {
		 *             std::lock_guard<std::mutex> lock(state.mutex);
		 *             state.lastLine = line;
		 *         },
		 *         [&state]() {
		 *             std::lock_guard<std::mutex> lock(state.mutex);
		 *             state.processCrashed = true;
		 *         }
		 *     );
		 * @endcode
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
	 * @brief Factory function to create a platform-specific process wrapper.
	 * 
	 * @param args Vector of command-line arguments where the first element is the executable path
	 * @param workingDirectory The working directory for the process
	 * @return Raw pointer to a newly created ProcessWrapper instance
	 * @throws CanNotOpenProcessException if the process cannot be started for any reason
	 * 
	 * @details
	 * This function creates a platform-specific process wrapper:
	 * - Windows: Creates WinProcessWrapper
	 * - Unix/Linux: Creates UnixProcessWrapper
	 * 
	 * The caller is responsible for managing the lifetime of the returned pointer.
	 * Typically, this is passed to EngineInstance which takes ownership.
	 * 
	 * **Usage Example:**
	 * @code
	 *     try {
	 *         auto process = openProcess({"stockfish.exe"}, "/");
	 *         auto engine = builder.build(process, logger);
	 *         // ... engine is now responsible for process cleanup
	 *     } catch (const CanNotOpenProcessException& e) {
	 *         std::cerr << "Failed to start engine: " << e.what() << std::endl;
	 *     }
	 * @endcode
	 * 
	 * @see ProcessWrapper for the returned object interface
	 * @see CanNotOpenProcessException for error handling
	 */
	ProcessWrapper* openProcess(const std::vector<std::string>& args, const std::string& workingDirectory);
}

