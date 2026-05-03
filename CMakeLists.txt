
cmake_minimum_required(VERSION 3.28) # my version of cmake, might be lower actually

project(UCILoader VERSION 1.1.0
                  DESCRIPTION "UCI Engine Loader Library for C++"
                  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# configure target os based on detected platform
if(WIN32)
    set(process_wrapper_impl "src/WinProcessWrapper.cpp")
else()
    set(process_wrapper_impl "src/UnixProcessWrapper.cpp")
endif()

set(lib_header_files
    "include/UCILoader/AbstractPipe.h"
    "include/UCILoader/EngineConnection.h"
    "include/UCILoader/EngineEvent.h"
    "include/UCILoader/ProcessWrapper.h"
    "include/UCILoader/Parser.h"
    "include/UCILoader/StandardChess.h"
    "include/UCILoader/UCI.h"
)

set(lib_source_files
    "src/AbstractPipe.cpp"
    "src/EngineEvent.cpp"
    "src/EngineConnection.cpp"
    "src/Logger.cpp"
    "src/Parser.cpp"
    "src/StandardChess.cpp"
    "src/UCI.cpp"
    ${process_wrapper_impl}
)

set(all_lib_files
    ${lib_header_files}
    ${lib_source_files}
)

# set d postfix for debug builds
set(CMAKE_DEBUG_POSTFIX d)

add_library(uciloader STATIC ${all_lib_files} )

if (MSVC)
    # Microsoft doesn't define __STDC_LIB_EXT1__ despide providing localtime_s and gtime_s
    # So we tell the compiler ourselves, that those functions are available
    target_compile_definitions(uciloader PRIVATE __STDC_LIB_EXT1__=1)
endif()

target_include_directories(uciloader PUBLIC
$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
$<INSTALL_INTERFACE:include/UCILoader>
)

enable_testing()

add_subdirectory("Examples/proxy")
add_subdirectory("Examples/demo")
add_subdirectory("UnitTests")
add_subdirectory("SystemTests")

install(TARGETS uciloader
    EXPORT UCILoader
    CONFIGURATIONS Release Debug
    LIBRARY DESTINATION Release/lib
    ARCHIVE DESTINATION Release/lib
    INCLUDES DESTINATION include
)

install(FILES ${lib_header_files}
    DESTINATION include/UCILoader
)

install(EXPORT UCILoader
    FILE UCILoader.cmake
    DESTINATION cmake
)


include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "UCILoaderConfigVersion.cmake"
    VERSION ${uciloaderlib_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/UCILoaderConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

install( FILES "UCILoaderConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/UCILoaderConfigVersion.cmake" 
    DESTINATION cmake
)
