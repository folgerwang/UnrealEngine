cmake_minimum_required(VERSION 3.3)
project(tokengen)

# Put our build output one level up so we can easily delete the temporary files and only check-in the final libs
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")

set(OPENSSL_PATH "" CACHE STRING "Path to openssl headers")

include_directories(${OPENSSL_PATH})

set(SOURCES ../tokengen/tokengen.c)

add_library(tokengen STATIC ${SOURCES})
