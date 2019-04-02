// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*================================================================================================
	RayTracingDefinitions.ush: used in ray tracing shaders and C++ code to define common constants
	!!! Changing this file requires recompilation of the engine !!!
=================================================================================================*/

#pragma once

// Change this to force recompilation of all ray tracing shaders (use https://www.random.org/cgi-bin/randbyte?nbytes=4&format=h)
// This avoids changing the global ShaderVersion.ush and forcing recompilation of all shaders in the engine (only RT shaders will be affected)
#define RAY_TRACING_SHADER_VERSION 0x510cacb6

#define RAY_TRACING_REGISTER_SPACE_LOCAL  0 // default register space for hit group (closest hit, any hit, intersection) shader resources
#define RAY_TRACING_REGISTER_SPACE_GLOBAL 1 // register space for ray generation and miss shaders (#dxr_todo: make global resources also available to hit shaders)
#define RAY_TRACING_REGISTER_SPACE_SYSTEM 2 // register space for "system" parameters (index buffer, vertex buffer, fetch parameters)

#define RAY_TRACING_MASK_OPAQUE						0x01    // Opaque and alpha tested meshes and particles (e.g. used by reflection, shadow, AO and GI tracing passes)
#define RAY_TRACING_MASK_TRANSLUCENT				0x02    // Opaque and alpha tested meshes and particles (e.g. used by translucency tracing pass)
#define RAY_TRACING_MASK_SHADOW						0x08    // Whether the geometry is visible for shadow rays
#define RAY_TRACING_MASK_ALL						0xFF

#define RAY_TRACING_SHADER_SLOT_MATERIAL	0
#define RAY_TRACING_SHADER_SLOT_SHADOW		1
#define RAY_TRACING_NUM_SHADER_SLOTS		2

