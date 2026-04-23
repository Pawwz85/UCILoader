
project(SystemTests 
    LANGUAGES C CXX)

# Step 0. Compile an example engine binary
add_executable(chal_engine ThirdParty/Chal.c)
set_target_properties(chal_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

find_library(MATH_LIBRARY m) # chal engine depends on libm, which might be boundled into libc sometimes ... 
if (MATH_LIBRARY)
    target_link_libraries(chal_engine m)  # ... or sometimes must be linked seperately to a given c program
endif()

add_executable(EngineConnectionTester EngineConnectionTester.cpp)
target_link_libraries(EngineConnectionTester PRIVATE uciloader)
add_test(NAME EngineConnectionTest COMMAND EngineConnectionTester $<TARGET_FILE:chal_engine>)

add_executable(BasicLoggingTester BasicLoggingTester.cpp)
target_link_libraries(BasicLoggingTester PRIVATE uciloader)
add_test(NAME BasicLoggingTester COMMAND BasicLoggingTester $<TARGET_FILE:chal_engine>)

add_executable(EngineEventTester EngineEventTester.cpp)
target_link_libraries(EngineEventTester PRIVATE uciloader)
add_test(NAME EngineEventTester COMMAND EngineEventTester $<TARGET_FILE:chal_engine>)
