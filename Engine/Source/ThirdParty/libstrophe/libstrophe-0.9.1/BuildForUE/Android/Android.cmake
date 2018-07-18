include(../../../PhysX3/Externals/CMakeModules/Android/android.toolchain.cmake)

set(SOCKET_IMPL "../src/sock.c" CACHE TYPE INTERNAL FORCE)
set(DISABLE_TLS 0 CACHE TYPE INTERNAL FORCE)
set(OPENSSL_PATH "../../../OpenSSL/1_0_1s/include/Android" CACHE TYPE INTERNAL FORCE)

set(CMAKE_CXX_FLAGS_DEBUG "-O0" CACHE TYPE INTERNAL FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3" CACHE TYPE INTERNAL FORCE)

# add_definitions(-DXML_STATIC -DUSE_WEBSOCKETS)
add_definitions(-DXML_STATIC)
