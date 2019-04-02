// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterMacros.h"

#if RHI_RAYTRACING

class FBuiltInRayTracingShader : public FGlobalShader
{
protected:
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{}

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// Built-in ray tracing shaders are always compiled for RHIs that support them, regardless of whether RT is enabled for the project.
		return RHISupportsRayTracingShaders(Parameters.Platform);
	}

	FBuiltInRayTracingShader() = default;
	FBuiltInRayTracingShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

class UTILITYSHADERS_API FOcclusionMainRG : public FBuiltInRayTracingShader
{
	DECLARE_GLOBAL_SHADER(FOcclusionMainRG);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FOcclusionMainRG, FBuiltInRayTracingShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBasicRayData>, Rays)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OcclusionOutput)
	END_SHADER_PARAMETER_STRUCT()
};

class FOcclusionMainMS : public FBuiltInRayTracingShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOcclusionMainMS, Global, UTILITYSHADERS_API);
public:

	FOcclusionMainMS() = default;
	FOcclusionMainMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBuiltInRayTracingShader(Initializer)
	{}
};

class UTILITYSHADERS_API FIntersectionMainRG : public FBuiltInRayTracingShader
{
	DECLARE_GLOBAL_SHADER(FIntersectionMainRG);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FIntersectionMainRG, FBuiltInRayTracingShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, TLAS)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBasicRayData>, Rays)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FBasicRayIntersectionData>, IntersectionOutput)
	END_SHADER_PARAMETER_STRUCT()
};

class FIntersectionMainMS : public FBuiltInRayTracingShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FIntersectionMainMS, Global, UTILITYSHADERS_API);
public:

	FIntersectionMainMS() = default;
	FIntersectionMainMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBuiltInRayTracingShader(Initializer)
	{}
};

class FIntersectionMainCHS : public FBuiltInRayTracingShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FIntersectionMainCHS, Global, UTILITYSHADERS_API);
public:

	FIntersectionMainCHS() = default;
	FIntersectionMainCHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBuiltInRayTracingShader(Initializer)
	{}
};

class FDefaultMainCHS : public FBuiltInRayTracingShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FDefaultMainCHS, Global, UTILITYSHADERS_API);
public:

	FDefaultMainCHS() = default;
	FDefaultMainCHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBuiltInRayTracingShader(Initializer)
	{}
};


class FDefaultMainMS : public FBuiltInRayTracingShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FDefaultMainMS, Global, UTILITYSHADERS_API);
public:

	FDefaultMainMS() = default;
	FDefaultMainMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBuiltInRayTracingShader(Initializer)
	{}
};

#endif // RHI_RAYTRACING

