#pragma once

#include <memory>
#include <vector>
#include <string>

namespace UCILoader {

	/*!
	 * @brief Exception thrown when a pipe operation fails due to pipe closure.
	 * 
	 * @details
	 * This exception indicates that an I/O operation on a pipe cannot be completed because
	 * the underlying pipe has been closed. This can occur:
	 * - Before the operation is initiated (pipe already closed)
	 * - During the operation (pipe closed by peer or internal error)
	 * - When the process crashes or exits abruptly
	 * 
	 * Once this exception is thrown, the pipe should be considered invalid and unusable for further operations.
	 */
	class PipeClosedException : public std::exception {
	public:
		/*!
		 * @brief Get the exception message.
		 * 
		 * @return C-string describing the pipe closure error
		 */
		const char* what() const noexcept override {
			return "Couldn't finish I/O operation because the underlying pipe was closed";
		}
	};

	/*!
	 * @brief Abstract interface for non-blocking reading from a pipe.
	 * 
	 * @details
	 * Provides an abstraction for reading data from a pipe in a non-blocking manner.
	 * This interface is implemented by platform-specific readers:
	 * - Windows: WinPipeReader
	 * - Unix/Linux: UnixPipeReader
	 * 
	 * The poll() method reads available data without blocking. If no data is available,
	 * it returns 0. If the pipe is closed, PipeClosedException is thrown.
	 * 
	 * **Important Notes:**
	 * - poll() is non-blocking: it returns immediately with whatever data is available
	 * - isOpen() check does not prevent concurrent closure from throwing PipeClosedException
	 * - This interface is typically not used directly; use PipeScanner for line-based reading
	 * 
	 * @see PipeScanner for line-based reading abstraction
	 * @see AbstractPipeWriter for the writing counterpart
	 */
	class AbstractPipeReader {
	public:
		/*!
		 * @brief Virtual destructor for proper cleanup of derived classes.
		 */
		virtual ~AbstractPipeReader() noexcept {};

		/*!
		 * @brief Poll for available data from the pipe without blocking.
		 * 
		 * @param buffer Pointer to memory where data will be written
		 * @param buffer_size Maximum number of bytes to read
		 * @return Number of bytes actually read (0 to buffer_size)
		 * @throws PipeClosedException if the pipe is closed or broken
		 * 
		 * @details
		 * This method is non-blocking:
		 * - Returns immediately with whatever data is available (0 or more bytes)
		 * - Returns 0 if no data is currently available, not if pipe is closed
		 * - The returned data is copied into the provided buffer
		 * 
		 * **Exception Safety:**
		 * If PipeClosedException is thrown, the pipe is corrupted or closed and should not be used further.
		 * Pre-checking isOpen() does not guarantee safety against concurrent closure.
		 * 
		 * **Typical Usage Pattern:**
		 * Use PipeScanner which wraps this method to provide blocking line-based reading.
		 */
		virtual size_t poll(char* buffer, size_t buffer_size) = 0;

		/*!
		 * @brief Check if the pipe is currently open.
		 * 
		 * @return True if the pipe appears to be open, false if it's closed
		 * 
		 * @warning This check is not race-condition safe. The pipe may be closed after this
		 * check returns true but before the next poll() call. Use exception handling for
		 * true race-safe closure detection.
		 */
		virtual bool isOpen() const = 0;
	};
	
	/*!
	 * @brief Abstract interface for writing to a pipe.
	 * 
	 * @details
	 * Provides an abstraction for writing data to a pipe. This interface is implemented
	 * by platform-specific writers:
	 * - Windows: WinPipeWriter
	 * - Unix/Linux: UnixPipeWriter
	 * 
	 * The write() method is designed to write all requested bytes or throw an exception.
	 * It differs from poll() in that it always writes the complete buffer or fails.
	 * 
	 * **Important Notes:**
	 * - write() is blocking: it will not return until all data is written
	 * - All bytes are guaranteed to be written, or PipeClosedException is thrown
	 * - isOpen() check does not prevent concurrent closure from throwing PipeClosedException
	 * - Useful for sending complete messages to processes
	 * 
	 * @see AbstractPipeReader for the reading counterpart
	 */
	class AbstractPipeWriter {
	public:
		/*!
		 * @brief Virtual destructor for proper cleanup of derived classes.
		 */
		virtual ~AbstractPipeWriter() noexcept {};

		/*!
		 * @brief Write data to the pipe, blocking until complete.
		 * 
		 * @param buffer Pointer to the data to write
		 * @param buffer_size Number of bytes to write
		 * @throws PipeClosedException if the pipe is closed or broken
		 * 
		 * @details
		 * This method guarantees that either:
		 * 1. All buffer_size bytes are written to the pipe, or
		 * 2. PipeClosedException is thrown
		 * 
		 * The method blocks until the complete buffer is written. It's suitable for sending
		 * UCI commands and other structured messages that must be transmitted completely.
		 * 
		 * **Exception Safety:**
		 * If PipeClosedException is thrown, the pipe is broken and should not be used further.
		 * Some or all of the data may have been written before the exception.
		 * 
		 * **Typical Usage:**
		 * @code
		 *     std::string command = "uci\n";
		 *     try {
		 *         writer->write(command.c_str(), command.size());
		 *     } catch (const PipeClosedException&) {
		 *         std::cerr << "Engine pipe closed" << std::endl;
		 *     }
		 * @endcode
		 */
		virtual void write(const char* buffer, size_t buffer_size) = 0;

		/*!
		 * @brief Check if the pipe is currently open.
		 * 
		 * @return True if the pipe appears to be open, false if it's closed
		 * 
		 * @warning This check is not race-condition safe. The pipe may be closed after this
		 * check returns true but before the next write() call. Use exception handling for
		 * true race-safe closure detection.
		 */
		virtual bool isOpen() const = 0;
	};
	
	/*!
	 * @brief Blocking line-oriented reader built on top of AbstractPipeReader.
	 * 
	 * @details
	 * PipeScanner provides a convenient way to read complete lines from a pipe.
	 * It wraps AbstractPipeReader and handles:
	 * - Buffering data as it arrives from the pipe
	 * - Detecting line boundaries (newline character)
	 * - Returning complete lines without the trailing newline
	 * 
	 * **Design Notes:**
	 * - Uses an internal 256-byte buffer for efficiency
	 * - Polls the pipe every 2ms when waiting for more data
	 * - Blocks until a complete line is received or pipe closes
	 * - Throws PipeClosedException if pipe closes before a newline is received
	 * 
	 * **Thread Safety:**
	 * Not thread-safe. The source pipe must not be accessed from other threads while getLine() is being called.
	 * 
	 * **Usage Example:**
	 * @code
	 *     auto reader = process->getReader();
	 *     PipeScanner scanner(reader);
	 *     
	 *     try {
	 *         while (true) {
	 *             std::string line = scanner.getLine();
	 *             std::cout << "Received: " << line << std::endl;
	 *         }
	 *     } catch (const PipeClosedException&) {
	 *         std::cout << "Process terminated" << std::endl;
	 *     }
	 * @endcode
	 * 
	 * @see AbstractPipeReader for the underlying pipe interface
	 * @see ProcessWrapper::listen() for how this is used internally
	 */
	class PipeScanner {
		static const size_t _InternalBufferSize = 256;
		const size_t _PollingIntervalMilliseconds = 2;
		char internalBuffer[_InternalBufferSize];
		size_t currentBufferSize = 0;
		size_t currentBufferPosition = 0;

		std::shared_ptr<AbstractPipeReader> source;

		/*!
		 * @brief Scan internal buffer for a complete line.
		 * 
		 * @param result String to append characters from the buffer to
		 * @return True if newline found (iteration stopped), false if buffer exhausted
		 * 
		 * @details
		 * Iterates through the internal buffer and appends characters to result until:
		 * - A newline ('\\n') character is found: returns true immediately without appending newline
		 * - No more characters in buffer: returns false for caller to poll more data
		 * 
		 * @private This is an internal implementation detail.
		 */
		bool scanInternalBuffer(std::string& result);
		
		/*!
		 * @brief Fetch more data from the pipe into the internal buffer.
		 * 
		 * @details
		 * Attempts to read data from the underlying pipe and refill the internal buffer.
		 * If no data is available, this method sleeps briefly before retrying.
		 * 
		 * @private This is an internal implementation detail.
		 */
		void pollPipe();

	public:

		/*!
		 * @brief Construct a PipeScanner from a source reader.
		 * 
		 * @param source Shared pointer to AbstractPipeReader to read from
		 * 
		 * @warning The caller is responsible for ensuring the source pipe is not accessed
		 * from other threads or other getLine() calls, as this will cause data corruption.
		 */
		PipeScanner(std::shared_ptr<AbstractPipeReader> source) : source(source) {};

		/*!
		 * @brief Read the next complete line from the pipe.
		 * 
		 * @return A complete line without the trailing newline character
		 * @throws PipeClosedException if the pipe is closed before a complete line is received
		 * 
		 * @details
		 * This method blocks until a complete line (ending with '\\n') is received from the pipe.
		 * The returned string does not include the newline character.
		 * 
		 * **Blocking Behavior:**
		 * - Polls the pipe every 2ms if partial data is available
		 * - Blocks indefinitely waiting for the newline character
		 * - Returns as soon as a complete line is available
		 * 
		 * **Exception Safety:**
		 * If PipeClosedException is thrown, the pipe is broken and no further getLine() calls should be made.
		 */
		std::string getLine();

	};
}

