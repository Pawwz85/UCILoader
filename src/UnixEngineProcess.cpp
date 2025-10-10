#include <UCILoader/target.h>

#if TARGET_OS == 1
	
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>

#include <mutex>
#include <signal.h>
#include <sys/wait.h>
#include <UCILoader/AbstractPipe.h>
#include <UCILoader/EngineLoader.h>

inline int kill_OS_call(int pid, int signal){
	return kill(pid, signal);
}

inline ssize_t write_OS_call(int fd, const char * buffer, size_t buffer_size){
	return write(fd, buffer, buffer_size);
}

class ClosablePipe {
	protected:
	std::mutex mut;
	int fileDescriptor;
	bool open;

public:
	ClosablePipe(int fd): fileDescriptor(fd), open(true) {};

	ClosablePipe(ClosablePipe && other): fileDescriptor(other.fileDescriptor), open(other.open) {
		other.fileDescriptor = 0;
	}

	~ClosablePipe(){
		if(open && fileDescriptor > 0)
			closeSelf();
	}

	void closeSelf(){
		std::lock_guard<std::mutex> guard(mut);
		if(fileDescriptor && close(fileDescriptor) != 0){
			// TODO: throw error
		}
		open = false;
	}
};

class UnixPipeReader : public UCILoader::AbstractPipeReader, public ClosablePipe {

public:
	UnixPipeReader(int fd):	ClosablePipe(fd) {};
	// Odziedziczono za po�rednictwem elementu AbstractPipeReader
	size_t poll(char* buffer, size_t buffer_size) override
	{	
		ssize_t result = read(fileDescriptor, buffer, buffer_size);
		if (result == -1){
			// todo: throw more suitable exception
			throw UCILoader::PipeClosedException();
		}		
		return result;
	}

	bool isOpen() const override
	{
		return open;
	}

};

class UnixPipeWriter : public UCILoader::AbstractPipeWriter, public ClosablePipe {

public:
	UnixPipeWriter(int fd):	ClosablePipe(fd) {};
	void write(const char * buffer, size_t buffer_size) override {
		ssize_t bytes_written = 0, tmp;
		while(bytes_written < buffer_size){
			tmp = write_OS_call(fileDescriptor, buffer, buffer_size);
			if (tmp == - 1){
				// todo: throw more suitable exception
				throw UCILoader::PipeClosedException();		
			}
			bytes_written += tmp;
	  	}
		
	}

	bool isOpen() const override {
		return open;
	}
};

class UnixProcessWrapper : public UCILoader::EngineProcessWrapper {

	int childProccessId;

	std::shared_ptr<UnixPipeWriter> writer_ptr;
	std::shared_ptr<UnixPipeReader> reader_ptr;

	protected:
	std::shared_ptr<UCILoader::AbstractPipeReader> getReader() override
	{
		return reader_ptr;
	}

	public:

	UnixProcessWrapper(int pid, int writer_fd, int reader_fd) : childProccessId(pid) {
		writer_ptr = std::make_shared<UnixPipeWriter>(writer_fd);
		reader_ptr = std::make_shared<UnixPipeReader>(reader_fd);
	}

	std::shared_ptr<UCILoader::AbstractPipeWriter> getWriter() override
	{
		return std::static_pointer_cast<UCILoader::AbstractPipeWriter>(writer_ptr);
	}

	void kill() override
	{
		(void)kill_OS_call(childProccessId, SIGTERM);
		writer_ptr->closeSelf();
		reader_ptr->closeSelf();
	}

	bool isAlive() const override
	{
		return kill_OS_call(childProccessId, 0) == 0;
	}

};

UCILoader::EngineProcessWrapper* UCILoader::openEngineProcess(const std::vector<std::string> & args, const std::string& workingDirectory) {

    if (args.empty())
        throw UCILoader::CanNotOpenEngineException("Missing command line arguments");
	
	int child_pipe[2];
	int parent_pipe[2];

	pipe(child_pipe);
	pipe(parent_pipe);
	
	int pid = fork();
	char ** args_c;

	if(pid == 0){

		args_c = new char *[args.size()];
		args_c[args.size() - 1] = 0;

		for(size_t i = 1; i < args.size(); ++i){
			args_c[i - 1] = new char[args[i].size() + 1];
			strcpy(args_c[i-1], args[i].c_str());
		}
			
		close(child_pipe[0]);
		close(parent_pipe[1]);
		//close(0);
		//close(1);
		dup2(parent_pipe[0], 0);
		dup2(child_pipe[1], 1); 
		
		execvp(args[0].c_str(), args_c);
		exit(0);
	}

	close(child_pipe[1]);
	close(parent_pipe[0]);

	if(pid == -1)
		throw UCILoader::CanNotOpenEngineException("Fork() returned -1");
	
	if(kill_OS_call(pid, 0))
		throw UCILoader::CanNotOpenEngineException("Engine process created, but it was terminated early");

	return new UnixProcessWrapper(pid, parent_pipe[1], child_pipe[0]);
}


#endif
