#include(../../CMake/PlatformScripts/Android/Android.cmake)
include(D:/Fortnite/Engine/Source/ThirdParty/CMake/PlatformScripts/Android/Android.cmake)

set(SKIP_PRE_BUILD_COMMAND 1)

set(CMAKE_CXX_FLAGS_DEBUG "-O0" CACHE TYPE INTERNAL FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3" CACHE TYPE INTERNAL FORCE)

add_definitions(-DHAVE_EXPAT_CONFIG_H) # -DWIN32)
