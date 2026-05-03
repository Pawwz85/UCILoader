cmake_minimum_required(VERSION 3.28) # my version of cmake, might be lower actually
project(Demo VERSION 1.0
                  DESCRIPTION "Example program usage of UCILoader library to connect to UCI engine"
                  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(demo main.cpp)

target_link_libraries(demo uciloader)