#include <UCILoader/target.h>

#if (TARGET_OS == 1)
	
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <mutex>
#include <UCILoader/AbstractPipe.h>
#include <UCILoader/EngineLoader.h>



class UnixPipeReader : AbstractPipeReader {

	int pipe_fd;

public:


	// Odziedziczono za poœrednictwem elementu AbstractPipeReader
	size_t poll(char* buffer, size_t buffer_size) override
	{
	
		return size_t();
	}

	bool isOpen() const override
	{
		return false;
	}

};


#endif