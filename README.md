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
  // send 'quit' command to the engine
  instance->quit();
  delete instance;
```

## Documentation

The library documentation is in its source. You can either view it in your favorite code editor or generate it using doxygen.


