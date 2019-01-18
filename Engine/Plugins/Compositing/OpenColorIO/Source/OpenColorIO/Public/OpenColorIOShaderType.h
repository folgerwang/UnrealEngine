// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenColorIOShaderType.h: OpenColorIO shader type definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GlobalShader.h"
#include "Shader.h"

/** A macro to implement OpenColorIO Color Space Transform shaders. */
#define IMPLEMENT_OCIO_SHADER_TYPE(TemplatePrefix,ShaderClass,SourceFilename,FunctionName,Frequency) \
	IMPLEMENT_SHADER_TYPE( \
		TemplatePrefix, \
		ShaderClass, \
		SourceFilename, \
		FunctionName, \
		Frequency \
		);

class FOpenColorIOTransformResource;
class FShaderCommonCompileJob;
class FShaderCompileJob;
class FUniformExpressionSet;


/** Called for every OpenColorIO shader to update the appropriate stats. */
extern void UpdateOpenColorIOShaderCompilingStats(const FOpenColorIOTransformResource* InShader);


/**
 * A shader meta type for OpenColorIO-linked shaders.
 */
class FOpenColorIOShaderType : public FShaderType
{
public:
	struct CompiledShaderInitializerType : FGlobalShaderType::CompiledShaderInitializerType
	{
		const FString DebugDescription;

		CompiledShaderInitializerType(
			FShaderType* InType,
			int32 InPermutationId,
			const FShaderCompilerOutput& CompilerOutput,
			FShaderResource* InResource,
			const FSHAHash& InOCIOShaderMapHash,
			const FString& InDebugDescription
			)
		: FGlobalShaderType::CompiledShaderInitializerType(InType,InPermutationId,CompilerOutput,InResource, InOCIOShaderMapHash,nullptr,nullptr)
		, DebugDescription(InDebugDescription)
		{}
	};
	typedef FShader* (*ConstructCompiledType)(const CompiledShaderInitializerType&);
	typedef bool (*ShouldCompilePermutationType)(EShaderPlatform,const FOpenColorIOTransformResource*);
	typedef bool(*ValidateCompiledResultType)(EShaderPlatform, const FShaderParameterMap&, TArray<FString>&);
	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform,const FOpenColorIOTransformResource*, FShaderCompilerEnvironment&);

	FOpenColorIOShaderType(
		const TCHAR* InName,
		const TCHAR* InSourceFilename,
		const TCHAR* InFunctionName,
		uint32 InFrequency,					// ugly - ignored for OCIO shaders but needed for IMPLEMENT_SHADER_TYPE macro magic
		int32 InTotalPermutationCount,
		ConstructSerializedType InConstructSerializedRef,
		ConstructCompiledType InConstructCompiledRef,
		ModifyCompilationEnvironmentType InModifyCompilationEnvironmentRef,
		ShouldCompilePermutationType InShouldCompilePermutationRef,
		ValidateCompiledResultType InValidateCompiledResultRef,
		GetStreamOutElementsType InGetStreamOutElementsRef
		):
		FShaderType(EShaderTypeForDynamicCast::OCIO, InName, InSourceFilename, InFunctionName, SF_Pixel, InTotalPermutationCount, InConstructSerializedRef, InGetStreamOutElementsRef, nullptr),
		ConstructCompiledRef(InConstructCompiledRef),
		ShouldCompilePermutationRef(InShouldCompilePermutationRef),
		ValidateCompiledResultRef(InValidateCompiledResultRef),
		ModifyCompilationEnvironmentRef(InModifyCompilationEnvironmentRef)
	{
		check(InTotalPermutationCount == 1);
	}

	/**
	 * Enqueues a compilation for a new shader of this type.
	 * @param InColorTransform - The ColorTransform to link the shader with.
	 */
	class FShaderCompileJob* BeginCompileShader(
			uint32 ShaderMapId,
			const FOpenColorIOTransformResource* InColorTransform,
			FShaderCompilerEnvironment* CompilationEnvironment,
			EShaderPlatform Platform,
			TArray<FShaderCommonCompileJob*>& NewJobs,
			FShaderTarget Target
		);

	/**
	 * Either creates a new instance of this type or returns an equivalent existing shader.
	 * @param CurrentJob - Compile job that was enqueued by BeginCompileShader.
	 */
	FShader* FinishCompileShader(
		const FSHAHash& InOCIOShaderMapHash,
		const FShaderCompileJob& CurrentJob,
		const FString& InDebugDescription
		);

	/**
	 * Checks if the shader type should be cached for a particular platform and color transform.
	 * @param Platform - The platform to check.
	 * @param InColorTransform - The color transform to check.
	 * @return True if this shader type should be cached.
	 */
	bool ShouldCache(EShaderPlatform InPlatform,const FOpenColorIOTransformResource* InColorTransform) const
	{
		return (*ShouldCompilePermutationRef)(InPlatform, InColorTransform);
	}


protected:

	/**
	 * Sets up the environment used to compile an instance of this shader type.
	 * @param InPlatform - Platform to compile for.
	 * @param OutEnvironment - The shader compile environment that the function modifies.
	 */
	void SetupCompileEnvironment(EShaderPlatform InPlatform, const FOpenColorIOTransformResource* InColorTransform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Allow the shader type to modify its compile environment.
		(*ModifyCompilationEnvironmentRef)(InPlatform, InColorTransform, OutEnvironment);
	}

private:
	ConstructCompiledType ConstructCompiledRef;
	ShouldCompilePermutationType ShouldCompilePermutationRef;
	ValidateCompiledResultType ValidateCompiledResultRef;
	ModifyCompilationEnvironmentType ModifyCompilationEnvironmentRef;
};
