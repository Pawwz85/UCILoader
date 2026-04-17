#include <UCILoader/AbstractPipe.h>

#include <thread>

using namespace UCILoader;

bool PipeScanner::scanInternalBuffer(std::string& result)
{
	char currentCharacter;

	for (; currentBufferPosition < currentBufferSize; ++currentBufferPosition) {
		currentCharacter = internalBuffer[currentBufferPosition];

		if (currentCharacter == '\n') {
			++currentBufferPosition;

			// If the line ends with /r/n, we must remove both characters
			if(!result.empty() && result.back() == '\r') 
				result.pop_back();

			return true;
		}

		result.push_back(currentCharacter);
	}

	return false;
}

void PipeScanner::pollPipe()
{
	currentBufferPosition = 0;
	currentBufferSize = source->poll(internalBuffer, _InternalBufferSize);
}

std::string PipeScanner::getLine()
{
	std::string result = "";
	result.reserve(_InternalBufferSize);

	bool waitForData = false;

	while (!scanInternalBuffer(result)) {
		if (currentBufferSize) result.reserve(result.capacity() + _InternalBufferSize);
		if (waitForData) std::this_thread::sleep_for(std::chrono::milliseconds(_PollingIntervalMilliseconds));
		pollPipe();
		waitForData = currentBufferSize != _InternalBufferSize;
	}

	result.shrink_to_fit();
	return result;
}
