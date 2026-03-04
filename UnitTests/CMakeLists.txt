cmake_minimum_required(VERSION 3.14) 
project(UnitTests 
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17) # gtest requirement
set(CMAKE_CXX_STANDARD_REQUIRED True)

set ( test_files 
        "test.cpp"
        "TestAbstractPipe.cpp"
        "TestEngineConnection.cpp"
        "TestEngineEvent.cpp"
        "TestUCIInfo.cpp"
        "TestUCIParser.cpp"
)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# cache gtest for future use
FetchContent_MakeAvailable(googletest)

add_executable(UnitTests pch.h pch.cpp ${test_files})

target_link_libraries(UnitTests GTest::gtest_main uciloader)
include(GoogleTest)
gtest_discover_tests(UnitTests)
