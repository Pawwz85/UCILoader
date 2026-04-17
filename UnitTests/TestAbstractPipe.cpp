#include "pch.h"

#include <UCILoader/AbstractPipe.h>
#include <thread>
#include <mutex>

using namespace UCILoader;

struct SharedString {
	std::mutex mutex;
	std::string value; 
	bool closed = false;
};

class SharedStringReader: public AbstractPipeReader {
	size_t cursor = 0;
	SharedString* simulatedPipe;

public:

	SharedStringReader(SharedString* pipe) : simulatedPipe(pipe) {};

	// Odziedziczono za po�rednictwem elementu AbstractPipeReader
	size_t poll(char* buffer, size_t buffer_size) override
	{
		std::lock_guard<std::mutex> guard(simulatedPipe->mutex);

		if (simulatedPipe->closed)
			throw PipeClosedException();

		size_t result = 0;

		char* start = buffer, *end = buffer+ buffer_size;

		while (cursor < simulatedPipe->value.size() && buffer < end) 
			*buffer++ = simulatedPipe->value[cursor++];

		return buffer - start;
	}

	bool isOpen() const override
	{
		std::lock_guard<std::mutex> guard(simulatedPipe->mutex);
		return !simulatedPipe->closed;
	}
};


TEST(PipeScannerTest, BaseCase) {
	SharedString _pipe;
	_pipe.value = "First Line\nSecond Line\n";

	PipeScanner scanner(std::make_unique<SharedStringReader>(&_pipe));

	EXPECT_EQ("First Line", scanner.getLine());
	EXPECT_EQ("Second Line", scanner.getLine());
}

TEST(PipeScannerTest, LongLines) {
	SharedString _pipe;

	// Developer of this code is notorious for neglecting unit tests. 
	const std::string developerLesson = "I will stop testing in production.";
	
	std::string firstLine = developerLesson;

	for (size_t i = 0; i < 48; ++i)
		firstLine += " " + developerLesson;

	_pipe.value = firstLine + "\nSmall break\n" + firstLine + "\nDone\n";
	PipeScanner scanner(std::make_unique<SharedStringReader>(&_pipe));

	EXPECT_EQ(firstLine, scanner.getLine());
	EXPECT_EQ("Small break", scanner.getLine());
	EXPECT_EQ(firstLine, scanner.getLine());
	EXPECT_EQ("Done", scanner.getLine());
}

TEST(PipeScannerTest, DelayedInput) {
	SharedString _pipe;
	SharedString* _pipe_ptr = &_pipe;
	PipeScanner scanner(std::make_unique<SharedStringReader>(&_pipe));

	auto const delay = std::chrono::milliseconds(100);
	auto start = std::chrono::steady_clock::now();

	std::thread worker([_pipe_ptr, delay]() {
		std::this_thread::sleep_for(delay);
		std::lock_guard<std::mutex> guard(_pipe_ptr->mutex);

		_pipe_ptr->value = "Test\n";
	});

	EXPECT_EQ("Test", scanner.getLine());
	EXPECT_GE( std::chrono::steady_clock::now() - start, delay);
	worker.join();
}

TEST(PipeScannerTest, PipeClosedDuringRead) {
	SharedString _pipe;
	SharedString* _pipe_ptr = &_pipe;
	PipeScanner scanner(std::make_unique<SharedStringReader>(&_pipe));

	auto const delay = std::chrono::milliseconds(100);
	auto start = std::chrono::steady_clock::now();

	std::thread worker([_pipe_ptr, delay]() {
		std::this_thread::sleep_for(delay);
		std::lock_guard<std::mutex> guard(_pipe_ptr->mutex);

		_pipe_ptr->closed = true;
		});

	EXPECT_THROW(scanner.getLine(), PipeClosedException);
	EXPECT_GE(std::chrono::steady_clock::now() - start, delay);
	worker.join();
}


TEST(PipeScannerTest, CRLF) {
	SharedString _pipe;
	_pipe.value = "First Line\r\nSecond Line\r\n";

	PipeScanner scanner(std::make_unique<SharedStringReader>(&_pipe));

	EXPECT_EQ("First Line", scanner.getLine());
	EXPECT_EQ("Second Line", scanner.getLine());
};