cmake_minimum_required(VERSION 3.3)
project(vivoxclientapi)

set (CMAKE_CXX_STANDARD 11)

# Put our build output one level up so we can easily delete the temporary files and only check-in the final libs
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")

set(VIVOXSDK_PATH "" CACHE STRING "Path to Vivox sdk headers")

add_definitions(-DUSE_ACCESS_TOKENS)
include_directories(../vivoxclientapi/include ${VIVOXSDK_PATH})

set(SOURCES ../vivoxclientapi/src/accountname.cpp
			../vivoxclientapi/src/audiodeviceid.cpp
			../vivoxclientapi/src/clientconnection.cpp
			../vivoxclientapi/src/debugclientapieventhandler.cpp
			../vivoxclientapi/src/uri.cpp
			../vivoxclientapi/src/util.cpp
			../vivoxclientapi/src/vivoxclientsdk.cpp)

add_library(vivoxclientapi STATIC ${SOURCES})
