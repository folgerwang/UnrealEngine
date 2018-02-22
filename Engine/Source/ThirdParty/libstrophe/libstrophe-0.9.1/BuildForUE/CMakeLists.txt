cmake_minimum_required(VERSION 3.3)
project(libstrophe)

# Put our build output one level up so we can easily delete the temporary files and only check-in the final libs
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")

set(SOCKET_IMPL ../src/sock.c CACHE STRING "Socket implementation")
option(DISABLE_TLS "Disable TLS support" ON)

set(EXPAT_PATH ../../../Expat/expat-2.2.0/lib CACHE STRING "Path to expat headers")
set(OPENSSL_PATH "" CACHE STRING "Path to openssl headers")

if(${DISABLE_TLS})
	set(TLS_IMPL "../src/tls_dummy.c")
else()
	set(TLS_IMPL "../src/tls_openssl.c")
    include_directories(${OPENSSL_PATH})
endif()

include_directories(../src .. ${EXPAT_PATH})

set(SOURCES ../src/auth.c ../src/conn.c ../src/crypto.c ../src/ctx.c
	../src/event.c ../src/handler.c ../src/hash.c ../src/jid.c ../src/md5.c
	../src/resolver.c ../src/sasl.c ../src/scram.c ../src/sha1.c
	../src/snprintf.c ../src/stanza.c ../src/util.c
	../src/rand.c ../src/uuid.c	../src/parser_expat.c ${SOCKET_IMPL} ${TLS_IMPL})
	
add_library(strophe STATIC ${SOURCES})
