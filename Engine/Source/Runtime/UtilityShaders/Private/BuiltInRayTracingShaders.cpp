// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BuiltInRayTracingShaders.h"

#if RHI_RAYTRACING

IMPLEMENT_GLOBAL_SHADER( FOcclusionMainRG,		"/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf",		"OcclusionMainRG",				SF_RayGen);
IMPLEMENT_SHADER_TYPE(, FOcclusionMainMS,		TEXT("/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf"), TEXT("OcclusionMainMS"),		SF_RayMiss);
IMPLEMENT_GLOBAL_SHADER( FIntersectionMainRG,	"/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf",		"IntersectionMainRG",			SF_RayGen);
IMPLEMENT_SHADER_TYPE(, FIntersectionMainMS,	TEXT("/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf"), TEXT("IntersectionMainMS"),	SF_RayMiss);
IMPLEMENT_SHADER_TYPE(, FIntersectionMainCHS,	TEXT("/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf"), TEXT("IntersectionMainCHS"),	SF_RayHitGroup);
IMPLEMENT_SHADER_TYPE(, FDefaultMainCHS,		TEXT("/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf"), TEXT("DefaultMainCHS"),		SF_RayHitGroup);
IMPLEMENT_SHADER_TYPE(, FDefaultMainMS,			TEXT("/Engine/Private/RayTracing/RayTracingBuiltInShaders.usf"), TEXT("DefaultMainMS"),			SF_RayMiss);

#endif // RHI_RAYTRACING
