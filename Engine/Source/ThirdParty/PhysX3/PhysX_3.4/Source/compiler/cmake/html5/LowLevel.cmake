#
# Build LowLevel
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/LowLevel)

FIND_PACKAGE(nvToolsExt REQUIRED)

SET(LOWLEVEL_PLATFORM_INCLUDES
	$ENV{EMSCRIPTEN}/system/include
	${NVTOOLSEXT_INCLUDE_DIRS}
	${PHYSX_SOURCE_DIR}/Common/src/linux
	${PHYSX_SOURCE_DIR}/LowLevel/software/include/linux
	${PHYSX_SOURCE_DIR}/LowLevelDynamics/include/linux
	${PHYSX_SOURCE_DIR}/LowLevel/common/include/pipeline/linux
)

# Use generator expressions to set config specific preprocessor definitions
SET(LOWLEVEL_COMPILE_DEFS 

	# Common to all configurations
	${PHYSX_HTML5_COMPILE_DEFS};PX_PHYSX_STATIC_LIB;
	
	$<$<CONFIG:debug>:${PHYSX_HTML5_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_HTML5_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_HTML5_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_HTML5_RELEASE_COMPILE_DEFS};>
)

# include common low level settings
INCLUDE(../common/LowLevel.cmake)

