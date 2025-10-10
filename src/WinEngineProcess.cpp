
#include <UCILoader/target.h>

#if TARGET_OS == 0

#include <mutex>
#include <UCILoader/AbstractPipe.h>
#include <UCILoader/EngineLoader.h>
#include <Windows.h>

class ClosableWindowsObject {
protected:
	std::mutex mut;
	HANDLE handle;
	bool open;
public:

	ClosableWindowsObject(HANDLE h) : handle(h), open(true) {};

	ClosableWindowsObject(ClosableWindowsObject&& other) : handle(other.handle), open(other.open) {
		other.handle = 0;
	}

	~ClosableWindowsObject() { if (open) close(); };

	void close() {
		std::lock_guard<std::mutex> guard(mut);
		if (handle && !CloseHandle(handle)) {
			// todo: throw some error
		}

		open = false;
	};
};

class WindowsPipeReader : public UCILoader::AbstractPipeReader, public ClosableWindowsObject {

public:

	WindowsPipeReader(HANDLE h) : ClosableWindowsObject(h) {};

	// Odziedziczono za po�rednictwem elementu AbstractPipeReader
	size_t poll(char* buffer, size_t buffer_size) override
	{
		std::lock_guard<std::mutex> guard(mut);

		DWORD bytesRead = 0;
		DWORD error;

		DWORD bytesToRead = 0;

		if (!PeekNamedPipe(handle, 0, 0, 0, &bytesToRead, 0)) {
			throw UCILoader::PipeClosedException();
		}
		
		bytesToRead = bytesToRead > buffer_size ? buffer_size : bytesToRead;


		if (bytesToRead && (!open || !ReadFile(handle, buffer, bytesToRead, &bytesRead, 0))) {
				error = GetLastError();
				throw UCILoader::PipeClosedException();
		}
			

		return bytesRead;
	}

	bool isOpen() const override
	{
		return open;
	}

};

class WindowsPipeWriter : UCILoader::AbstractPipeWriter, public ClosableWindowsObject {
public:
	WindowsPipeWriter(HANDLE h) : ClosableWindowsObject(h) {};

	// Odziedziczono za po�rednictwem elementu AbstractPipeWriter
	void write(const char* buffer, size_t buffer_size) override
	{
		std::lock_guard<std::mutex> guard(mut);
		size_t totalBytesWritten = 0;
		DWORD bytesWritten;

		DWORD error;

		while (totalBytesWritten < buffer_size) {
			if (!open || !WriteFile(handle, buffer, buffer_size, &bytesWritten, 0)) {
				error = GetLastError();
				throw UCILoader::PipeClosedException();
			}

			totalBytesWritten += bytesWritten;
		}
	}

	bool isOpen() const override
	{
		return open;
	}
};


class WindowsEngineProcess : public UCILoader::EngineProcessWrapper {

	HANDLE readerHandle;
	HANDLE writerHandle;

	PROCESS_INFORMATION  engineProcessInfo;

	std::shared_ptr<WindowsPipeWriter> writer_ptr;
	std::shared_ptr<WindowsPipeReader> reader_ptr;


protected:
	// Odziedziczono za po�rednictwem elementu EngineProcessWrapper
	std::shared_ptr<UCILoader::AbstractPipeReader> getReader() override
	{
		return reader_ptr;
	}
public:
	
	WindowsEngineProcess(PROCESS_INFORMATION pInfo, HANDLE reader, HANDLE writer) : readerHandle(reader), writerHandle(writer), engineProcessInfo(pInfo) {
		writer_ptr = std::make_shared<WindowsPipeWriter>(writerHandle);
		reader_ptr = std::make_shared<WindowsPipeReader>(readerHandle);
	};

	std::shared_ptr<UCILoader::AbstractPipeWriter> getWriter() override
	{
		return std::static_pointer_cast<UCILoader::AbstractPipeWriter>(writer_ptr);
	}

	void kill() override
	{
		HANDLE & handle = engineProcessInfo.hProcess;
		if (handle && handle != INVALID_HANDLE_VALUE)
			(void*)TerminateProcess(handle, -1);

		writer_ptr->close();
		reader_ptr->close();
	}

	bool isAlive() const override
	{
		HANDLE handle = engineProcessInfo.hProcess;

		DWORD status;

		if (!GetExitCodeProcess(handle, &status)) {
			throw std::exception("OS error");
		}

		return status == STILL_ACTIVE;
	}

};

// helper function 
std::wstring expand_utf8_string(const std::string& s) {
	int required_size = MultiByteToWideChar(
		CP_UTF8,
		MB_PRECOMPOSED,
		s.c_str(),
		-1,
		NULL,
		0
	);

	if (required_size == 0)
		return L"";

	wchar_t* buffer = new wchar_t[required_size];

	int success = MultiByteToWideChar(
		CP_UTF8,
		MB_PRECOMPOSED,
		s.c_str(),
		-1,
		buffer,
		required_size
	);

	std::wstring result;

	if (success) {
		result = std::wstring(buffer);
	}

	delete[] buffer;
	return result;
}

std::wstring concatenateCommendLineArguments(const std::vector<std::string>& args) {

	std::string command = args[0];

	for (size_t i = 1; i < args.size(); ++i)
		command += " " + args[i];

	return expand_utf8_string(command);
}

void openPipe(HANDLE& read, HANDLE& write) {
	SECURITY_ATTRIBUTES saAttr;

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = true;
	saAttr.lpSecurityDescriptor = nullptr;
	bool success = CreatePipe(&read, &write, &saAttr, 0);

	if (!success || read == INVALID_HANDLE_VALUE || write == INVALID_HANDLE_VALUE) {
		throw UCILoader::CanNotOpenEngineException("An exception occurred during process of pipe creation");
	}
}

inline UCILoader::EngineProcessWrapper* spawnEngine(const std::wstring& command, const std::wstring workingDirectory) {
	wchar_t commandArguments[1024];

	if (command.size() >= 1024) {
		throw UCILoader::CanNotOpenEngineException("Provided command is too long, expected at most 1023 characters");
	}

	HANDLE childInRead;
	HANDLE childInWrite;
	HANDLE childOutRead;
	HANDLE childOutWrite;

	openPipe(childInRead, childInWrite);
	openPipe(childOutRead, childOutWrite);


	wcscpy_s(commandArguments, 1024, command.c_str());


	// Prepare startup info
	STARTUPINFOW startup_info;
	ZeroMemory(&startup_info, sizeof(STARTUPINFOW));

	startup_info.cb = sizeof(STARTUPINFOW);
	startup_info.dwFlags = STARTF_USESTDHANDLES;
	startup_info.hStdInput = childInRead; 
	startup_info.hStdOutput = childOutWrite;

	PROCESS_INFORMATION proces_info;

	ZeroMemory(&proces_info, sizeof(proces_info));

	SECURITY_ATTRIBUTES saAttr;

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = true;
	saAttr.lpSecurityDescriptor = nullptr;

	bool success = CreateProcessW(
		0,
		commandArguments,	// command line arguments
		&saAttr,		// proc. security attributes
		nullptr,		// thread. security attributes
		true,			// handle inheritance. True, since engine must inherit open handles to pipe
		0,				// creation flags - boot engine in windowless mode
		nullptr,		// env variables used by engine - in our case: none,
		workingDirectory.c_str(),
		&startup_info,
		&proces_info
	);


	if (!success) {
		throw UCILoader::CanNotOpenEngineException("Failed to start the engine process, make sure the path is correct");
	}

	DWORD status;
	if (!GetExitCodeProcess(proces_info.hProcess, &status) || status != STILL_ACTIVE) {
		throw UCILoader::CanNotOpenEngineException("Engine process started but crashed immediately");
	}


	// close handles that GUI no longer needs
	//engine_in->try_close_read();
	//engine_out->try_close_write();

	return new WindowsEngineProcess(
		proces_info, childOutRead, childInWrite
	);
}

UCILoader::EngineProcessWrapper* UCILoader::openEngineProcess(const std::vector<std::string> & args, const std::string& workingDirectory) {

	if (args.empty())
		throw UCILoader::CanNotOpenEngineException("Missing command line arguments");

	std::wstring commandLine = concatenateCommendLineArguments(args);
	std::wstring workingDirectoryWide = expand_utf8_string(workingDirectory);
	return spawnEngine(commandLine, workingDirectoryWide);
}

#endif
