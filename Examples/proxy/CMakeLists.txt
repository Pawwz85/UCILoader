cmake_minimum_required(VERSION 3.28) # my version of cmake, might be lower actually
project(Proxy VERSION 1.0
                  DESCRIPTION "Example program demonstrating how to use UCILoader library to create a CLI proxy program"
                  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(proxy main.cpp)

target_link_libraries(proxy uciloaderlib)
