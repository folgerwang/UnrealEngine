#
# Build SimulationController
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/SimulationController/src)

SET(SIMULATIONCONTROLLER_PLATFORM_INCLUDES
	$ENV{EMSCRIPTEN}/system/include
	${PHYSX_SOURCE_DIR}/Common/src/linux
	${PHYSX_SOURCE_DIR}/LowLevel/linux/include
)

# Use generator expressions to set config specific preprocessor definitions
SET(SIMULATIONCONTROLLER_COMPILE_DEFS

	# Common to all configurations
	${PHYSX_HTML5_COMPILE_DEFS};PX_PHYSX_STATIC_LIB;

	$<$<CONFIG:debug>:${PHYSX_HTML5_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_HTML5_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_HTML5_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_HTML5_RELEASE_COMPILE_DEFS};>
)

SET(SIMULATIONCONTROLLER_LIBTYPE STATIC)

# include common SimulationController settings
INCLUDE(../common/SimulationController.cmake)
