#pragma once

#include <memory>
#include <vector>
#include <string>

namespace UCILoader {

	/*!
		Exception indicating that I/O operation couldn't be finished because underlying pipe was closed.
		This include situations where pipe was closed before operation was performed or during such operation.
	*/
	class PipeClosedException : public std::exception {
	public:
		const char* what() const noexcept override {
			return "Couldn't finish I/O operation because the underlying pipe was closed";
		}
	};

	/*!
		An abstract interface for reading data from a pipe. It provides non-blocking reading mechanism 
		to extract characters from the internally managed pipe.
	*/
	class AbstractPipeReader {
	public:
		virtual ~AbstractPipeReader() noexcept {};

		/*!
			Reads up to *buffer_size* bytes from the underlying pipe and saves them to specified *buffer*. 
			This method is not blocking, so if there is less characters available than capacity of the buffer, they will
			all be written in the buffer and then function will return the amount of bytes read from the buffer.
			If the underlying pipe was closed or corrupted, the PipeClosedException will be thrown, indicating that this pipe is 
			not suitable for further usage.
		*/
		virtual size_t poll(char* buffer, size_t buffer_size) = 0;

		/*!
			Checks if underlying pipe is closed. Note that checking if pipe is open before calling poll() operation will not 
			prevent PipeClosedException being thrown if the pipe was closed concurrently or by internal error. 
		*/
		virtual bool isOpen() const = 0;
	};
	
	/*!
		An abstract interface for writing bytes to a pipe. It provides write() function that allows user to write data to a internally managed pipe.
	*/
	class AbstractPipeWriter {
	public:
		virtual ~AbstractPipeWriter() noexcept {};

		/*!
			Write *buffer_size* bytes to the underlying pipe. This method will always write *buffer_size* bytes to a pipe unless PipeClosedException will be thrown.
			The PipeClosedException will occur either if the *write* method will called on closed pipe or if pipe was closed by internal error or a concurrent thread. 
		*/
		virtual void write(const char* buffer, size_t buffer_size) = 0;

		/*!
			Checks if underlying pipe is closed. Note that checking if pipe is open before calling poll() operation will not
			prevent PipeClosedException being thrown if the pipe was closed concurrently or by internal error.
		*/
		virtual bool isOpen() const = 0;
	};
	
	/*!
		A blocking wrapper over the the instance of *AbstractPipeReader*. It provides a functionality to extract individual lines (sequences of bytes 
		ending with '\n' character) read from the pipe.
	*/
	class PipeScanner {
		static const size_t _InternalBufferSize = 256;
		const size_t _PollingIntervalMilliseconds = 2;
		char internalBuffer[_InternalBufferSize];
		size_t currentBufferSize = 0;
		size_t currentBufferPosition = 0;

		std::shared_ptr<AbstractPipeReader> source;

		/*
			Iterate internal buffer and save its characters inside a result string.
			Iteration stops if:
			- '\n' character is encountered. In this case method immediately returns true and '\n' is not appended to the result
			- there are no more characters in the buffer. In this case false is returned
		*/
		bool scanInternalBuffer(std::string& result);
		void pollPipe();

	public:

		/*!
			Constructs an instance of PipeScanner using provided source reader. Note that it is caller responsibility to ensure
			that source pipe is not being used while *getLine* is called, as this will cause data corruption.
		*/
		PipeScanner(std::shared_ptr<AbstractPipeReader> source) : source(source) {};

		/*!
			Read the next line from the pipe without trailing newline character.
			If the underlying pipe was closed before the newline character could be received, the PipeClosedException will be thrown.
		*/
		std::string getLine();

	};
}

