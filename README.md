<div align="center">

  <h1>UCILoader</h1>
  
  <p>
    C++ 17 library for working with chess engines. Effortlessly open, configure and query UCI engines from your C++ codebase. Easily modify UCI client to
    support even the most exotic chess variants.
  </p>
</div>
<br />  

<!-- About the Project -->
## About the Project

UCILoader is a cross platform library implementing an UCI protocol client. It was designed to provide an easily customizable entry point for creating 
tools and application for both regular and variant chess.  

<!-- Getting Started -->
## Getting Started

<!-- Prerequisites -->
#### Prerequisites

This project uses cmake build system, so in order to build the library make sure you have cmake installed.

<!-- Installation -->
#### Building from source

In order to build the library from source code you should clone this repository then use cmake to generate build files for you:

```bash
git clone https://github.com/Pawwz85/UCILoader.git
cd UCILoader
cmake -B build
```

Depending on your OS and your build system the next step will differ. On windows, if you are using visual studio, enter generated build folder
and open .sln file in visual studio then compile the solution (you can use ctrl+shift+b shortcut). On Linux simply execute following commands:

```bash
  cd build
  make
```

<!-- Running Tests -->
### Running Tests

To run tests, run the following command (in build directory)

```bash
  ctest
```

<!-- Deployment -->
### Linking library (in cmake)

If you are using cmake, the fastest way to add this library to your project is to simply add it to your project source folder. Then in your CMakeLists.txt
simply add the following line:

```cmake
  add_subdirectory(UCILoader)
```

Alternatively, you can use FetchContent module to download and build the library for you:

```cmake
include(FetchContent)
FetchContent_Declare(
		UCILoader
		URL https://github.com/Pawwz85/UCILoader/archive/refs/tags/v1.1.1.zip
)
```

Then link it like this:
```cmake
  target_link_libraries(MyProject PUBLIC uciloader)
```

## Usage

#### Opening Engine

```cpp
#include <iostream>
#include <UCILoader/EngineConnection.h>
#include <UCILoader/StandardChess.h>

using namespace std;
using namespace UCILoader;

int main() {
  // opens engine Path_To_Engine_Executable.exe without any cmd parameters in current working directory
  auto proces = openProcess({"Path_To_Engine_Executable.exe"}, "/");
  // Build instance of engine instance configured for standard chess 
  auto instance = StandardChess::ChessEngineInstanceBuilder->build(proces);

  // wait for engine to finish initializing
	instance->sync();
	
	// Get information about engine name and author (default is <empty>)
	cout << "Engine: " << instance->getName() << "\n";
	cout << "Author: " << instance->getAuthor() << "\n";

	// Measure latency using ping() command
	cout << "Latency: " << instance->ping().count() << " ms\n";      
```

#### Logging UCI Protocol Communication

UCILoader provides a flexible logging system to capture and analyze the UCI protocol messages exchanged with engines. Loggers can be created from various sources and customized with traits.

**Basic Logging:**

```cpp
  // Log all communication to a file with pretty formatting
  auto instance = StandardChess::ChessEngineInstanceBuilder->build(
    proces, 
    Loggers::toFile("engine.log") | LoggerTraits::Pretty
  );
```

**Available Loggers:**

- `Loggers::toNoting()` - Discards all messages (no-op logger)
- `Loggers::toStd()` - Outputs to standard output (stdout)
- `Loggers::toStderr()` - Outputs to standard error
- `Loggers::toFile(filename)` - Logs to a file
- `Loggers::toBuffer(buffer)` - Stores messages in an in-memory buffer
- `Loggers::toCallback(callback)` - Forwards messages to a custom callback function
- `Loggers::toOstream(stream)` - Logs to any std::ostream

**Available Traits:**

Traits customize logger behavior using the pipe operator `|`:

- `LoggerTraits::Pretty` - Adds direction prefixes (`>> ` for sent, `<< ` for received, `Parser: ` for parsed)
- `LoggerTraits::Timestamp` - Prepends timestamps to each message
- `LoggerTraits::IgnoreParser` - Filters out parser-generated messages
- `LoggerTraits::IgnoreEngine` - Filters out messages from the engine
- `LoggerTraits::IgnoreApplication` - Filters out messages sent to the engine

**Logging Examples:**

> **Note on Move Semantics:** LoggerBuilder uses move semantics and has no copy constructor. When binding a logger composition to a variable, use `std::move()` to transfer ownership. Inline compositions (used directly in function arguments) automatically apply move semantics.

**Method Comparison:**

Both of these approaches are equivalent so choose one based on your code style:

```cpp
  // Method 1: Inline composition (implicit move)
  auto instance1 = StandardChess::ChessEngineInstanceBuilder->build(
    proces,
    Loggers::toFile("engine.log") | LoggerTraits::Pretty | LoggerTraits::Timestamp
  );

  // Method 2: Bind to variable (explicit std::move)
  auto logger = Loggers::toFile("engine.log") | LoggerTraits::Pretty | LoggerTraits::Timestamp;
  auto instance2 = StandardChess::ChessEngineInstanceBuilder->build(proces, std::move(logger));
```

**Full Examples:**

```cpp
  // Log with pretty formatting and timestamps
  auto logger1 = Loggers::toFile("engine.log") 
    | LoggerTraits::Pretty 
    | LoggerTraits::Timestamp;
  auto instance1 = StandardChess::ChessEngineInstanceBuilder->build(proces, std::move(logger1));

  // Log only engine responses with timestamps
  auto logger2 = Loggers::toFile("responses.log")
    | LoggerTraits::IgnoreParser
    | LoggerTraits::IgnoreApplication
    | LoggerTraits::Timestamp;
  auto instance2 = StandardChess::ChessEngineInstanceBuilder->build(proces, std::move(logger2));

  // Capture messages in memory
  LogBuffer buffer;
  auto logger3 = Loggers::toBuffer(buffer) | LoggerTraits::Pretty;
  auto instance3 = StandardChess::ChessEngineInstanceBuilder->build(proces, std::move(logger3));
  
  // ... perform operations ...
  
  // Retrieve captured messages
  auto messages = buffer.snapshot();
  for (const auto& msg : messages) {
    cout << msg << endl;
  }

  // Log via callback function
  auto logger4 = Loggers::toCallback([](Logger::MessageDirection dir, const std::string& msg) {
    if (dir == Logger::FromEngine) {
      cout << "Engine: " << msg;
    }
  });
  auto instance4 = StandardChess::ChessEngineInstanceBuilder->build(proces, std::move(logger4));
```

**Custom Logger:**

For specialized logging needs, implement a custom logger and use `Loggers::from<T>()`:

```cpp
  // Define custom logger
  class MyCustomLogger : public UCILoader::Logger {
  private:
    std::ofstream file;
  public:
    MyCustomLogger(const std::string& filename) : file(filename) {}
    
    void log(MessageDirection dir, const std::string& msg) override {
      std::string prefix = (dir == ToEngine) ? "[CMD] " : "[RSP] ";
      file << prefix << msg << std::flush;
    }
  };

  // Use custom logger
  auto instance = StandardChess::ChessEngineInstanceBuilder->build(
    proces,
    Loggers::from<MyCustomLogger>("engine.log") | LoggerTraits::Timestamp
  );
```

#### Configure Engine Options

```cpp
  	if (instance->options.contains("UCI_ShowWDL")) {
		instance->options["UCI_ShowWDL"] = true;
		/*
			For the check option, you could also use string values [on/off] or [true/false]:
			instance->options["UCI_ShowWDL"] = "true";
			instance->options["UCI_ShowWDL"] = "on";
		*/
	  }

  if (instance->options.contains("Hash")) {
		int defaultHash = instance->options["Hash"];
		cout << "Engine uses by default hash table of size " << defaultHash << "\n";
    instance->options["Hash"] = 16; // sets the transposition table size to 16MB for the engine 
	}
```

#### Query Engine for best move

```cpp
  GoParamsBuilder<StandardChessMove> paramsBuilder;
  // asks engine to think for 500 ms in default position
  auto searchConnection = instance->search(paramsBuilder.withMoveTime(500).build(), StandardChess::StartPos()); 

  // wait up to 550 milliseconds for the engine to finish, then declare timeout if engine will not respond in that timeframe
  searchConnection->waitFor(std::chrono::milliseconds(550));

  // check search status
  auto status = searchConnection->getStatus();
  if (status == UCILoader::ResultReady) {
  	cout << "Best starting move is " << StandardChess::stringValueOf(*searchConnection->getResult().bestMove) << "\n";
  	if (searchConnection->getResult().ponderMove) // checks if engine  send pondermove candite in the resulting string
  		cout << "Engine thinks " << StandardChess::stringValueOf(*searchConnection->getResult().ponderMove) << " is best response for black\n";
  }
  else {
	  cout << "Search status code timed out with a status code: " << status<<'\n';
  }
```

#### Defining a callback for capturing engine info
```cpp
  void displayPV(const UCILoader::EngineEvent* e) {
  	Info<StandardChessMove> info = *(Info<StandardChessMove>*)e->getPayload();
  	
  	if (info.getType() == Pv) {
  		cout << "Pv: ";
  		for (auto& m : info.getMoveArray()) {
  			cout << StandardChess::stringValueOf(m) << " ";
  		}
  		cout << "\n";
    }
  }
```

#### Registering a callback capturing engine info
```cpp
  	instance->connect(displayPv, UCILoader::NamedEngineEvents::InfoReceived);
```

#### Closing Engine

```cpp
  // Engine will be closed automatically when the instance's destructor will be called
  delete instance;
```

## Documentation

The library documentation is in its source. You can either view it in your favorite code editor, generate it using doxygen or read it here: https://pawwz85.github.io/UCILoader/


