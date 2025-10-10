#pragma once

#include <memory>
#include <vector>
#include <string>

namespace UCILoader {
	class PipeClosedException : public std::exception {
	public:
		const char* what() const noexcept override {
			return "Couldn't finish I/O operation because the underlying pipe was closed";
		}
	};

	class AbstractPipeReader {
	public:
		virtual ~AbstractPipeReader() noexcept {};

		// throws PipeClosedException
		virtual size_t poll(char* buffer, size_t buffer_size) = 0;
		virtual bool isOpen() const = 0;
	};

	class AbstractPipeWriter {
	public:
		virtual ~AbstractPipeWriter() noexcept {};

		// throws PipeClosedException
		virtual void write(const char* buffer, size_t buffer_size) = 0;
		virtual bool isOpen() const = 0;
	};


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

		PipeScanner(std::shared_ptr<AbstractPipeReader> source) : source(source) {};

		/*
			Read the next line from the pipe without trailing newline character.
			If the underlying pipe was closed before the newline character could be received, the PipeClosedException will be thrown.
		*/
		std::string getLine();

	};
}

