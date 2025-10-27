
cmake_minimum_required(VERSION 3.28) # my version of cmake, might be lower actually
project(UCILoader VERSION 1.0
                  DESCRIPTION "UCI Engine Loader Library for C++"
                  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# configure target os based on detected platform
if(WIN32)
    set(TARGET_OPERATING_SYSTEM_CODE 0) # build library using windows api
else()
    set(TARGET_OPERATING_SYSTEM_CODE 1) # build library around posix api
endif()

configure_file(include/UCILoader/target.h.in include/UCILoader/target.h @ONLY)

set(lib_header_files
    "include/UCILoader/AbstractPipe.h"
    "include/UCILoader/EngineConnection.h"
    "include/UCILoader/EngineEvent.h"
    "include/UCILoader/ProcessWrapper.h"
    "include/UCILoader/Parser.h"
    "include/UCILoader/StandardChess.h"
    "include/UCILoader/target.h"
    "include/UCILoader/UCI.h"
)

set(lib_source_files
    "src/AbstractPipe.cpp"
    "src/EngineEvent.cpp"
    "src/EngineConnection.cpp"
    "src/Parser.cpp"
    "src/StandardChess.cpp"
    "src/UCI.cpp"
    "src/UnixProcessWrapper.cpp"
    "src/WinProcessWrapper.cpp"
)

set(all_lib_files
    ${lib_header_files}
    ${lib_source_files}
)

add_library(uciloaderlib STATIC ${all_lib_files} )

target_include_directories(uciloaderlib PUBLIC include )

add_subdirectory("Examples/proxy")
add_subdirectory("Examples/EngineOptionDetector")
add_subdirectory("UnitTests")