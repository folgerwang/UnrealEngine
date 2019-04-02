// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialShader.h: Shader base classes
=============================================================================*/

#pragma once

//#include "HAL/IConsoleManager.h"
//#include "RHI.h"
#include "ShaderParameters.h"
#include "SceneView.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "NiagaraShaderType.h"
#include "SceneRenderTargetParameters.h"

struct FNiagaraDataInterfaceParametersCS;
class UClass;

template<typename TBufferStruct> class TUniformBufferRef;

template<typename ParameterType> 
struct TUniformParameter
{
	int32 Index;
	ParameterType ShaderParameter;
	friend FArchive& operator<<(FArchive& Ar,TUniformParameter<ParameterType>& P)
	{
		return Ar << P.Index << P.ShaderParameter;
	}
};

/** Base class of all shaders that need material parameters. */
class NIAGARASHADER_API FNiagaraShader : public FShader
{
public:
	DECLARE_SHADER_TYPE(FNiagaraShader, Niagara);

	static FName UniformBufferLayoutName;

	FNiagaraShader()
		: CBufferLayout(TEXT("Niagara Compute Sim CBuffer"))
	{
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FNiagaraShaderScript*  Script)
	{
		//@todo - lit materials only 
		return FNiagaraUtilities::SupportsGPUParticles(Platform);
	}


	FNiagaraShader(const FNiagaraShaderType::CompiledShaderInitializerType& Initializer);

	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform, const FNiagaraShaderScript* , FShaderCompilerEnvironment&);

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FNiagaraShaderScript*  Script, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static bool ValidateCompiledResult(EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutError)
	{
		return true;
	}
	
	void SetDataInterfaceParameterInfo(const TArray<FNiagaraDataInterfaceGPUParamInfo>& InDIParamInfo);

//	FUniformBufferRHIParamRef GetParameterCollectionBuffer(const FGuid& Id, const FSceneInterface* SceneInterface) const;
	/*
	template<typename ShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetViewParameters(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FSceneView& View, const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer)
	{
		const auto& ViewUniformBufferParameter = GetUniformBufferParameter<FViewUniformShaderParameters>();
		const auto& BuiltinSamplersUBParameter = GetUniformBufferParameter<FBuiltinSamplersParameters>();
		CheckShaderIsValid();
		SetUniformBufferParameter(RHICmdList, ShaderRHI, ViewUniformBufferParameter, ViewUniformBuffer);

		if (View.bShouldBindInstancedViewUB && View.Family->Views.Num() > 0)
		{
			// When drawing the left eye in a stereo scene, copy the right eye view values into the instanced view uniform buffer.
			const EStereoscopicPass StereoPassIndex = (View.StereoPass != eSSP_FULL) ? eSSP_RIGHT_EYE : eSSP_FULL;

			const FSceneView& InstancedView = View.Family->GetStereoEyeView(StereoPassIndex);
			const auto& InstancedViewUniformBufferParameter = GetUniformBufferParameter<FInstancedViewUniformShaderParameters>();
			SetUniformBufferParameter(RHICmdList, ShaderRHI, InstancedViewUniformBufferParameter, InstancedView.ViewUniformBuffer);
		}
	}
	*/

	// Bind parameters
	void BindParams(const FShaderParameterMap &ParameterMap);

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override;
	virtual uint32 GetAllocatedSize() const override;

	FRHIUniformBufferLayout CBufferLayout;
	FShaderResourceParameter FloatInputBufferParam;
	FShaderResourceParameter IntInputBufferParam;
	FRWShaderParameter FloatOutputBufferParam;
	FRWShaderParameter IntOutputBufferParam;
	FRWShaderParameter OutputIndexBufferParam;
	FShaderResourceParameter InputIndexBufferParam;
	FShaderUniformBufferParameter EmitterConstantBufferParam;
	FShaderUniformBufferParameter DataInterfaceUniformBufferParam;
	FShaderUniformBufferParameter ViewUniformBufferParam;
	FShaderParameter EmitterTickCounterParam;
	FShaderParameter NumEventsPerParticleParam;
	FShaderParameter NumParticlesPerEventParam;
	FShaderParameter CopyInstancesBeforeStartParam;
	FShaderParameter NumSpawnedInstancesParam;
	FShaderParameter UpdateStartInstanceParam;
	FShaderParameter NumIndicesPerInstanceParam;
	FShaderParameter ComponentBufferSizeReadParam;
	FShaderParameter ComponentBufferSizeWriteParam;
	FRWShaderParameter EventIntUAVParams[MAX_CONCURRENT_EVENT_DATASETS];
	FRWShaderParameter EventFloatUAVParams[MAX_CONCURRENT_EVENT_DATASETS];
	FShaderResourceParameter EventIntSRVParams[MAX_CONCURRENT_EVENT_DATASETS];
	FShaderResourceParameter EventFloatSRVParams[MAX_CONCURRENT_EVENT_DATASETS];
	FShaderParameter EventWriteFloatStrideParams[MAX_CONCURRENT_EVENT_DATASETS];
	FShaderParameter EventWriteIntStrideParams[MAX_CONCURRENT_EVENT_DATASETS];
	FShaderParameter EventReadFloatStrideParams[MAX_CONCURRENT_EVENT_DATASETS];
	FShaderParameter EventReadIntStrideParams[MAX_CONCURRENT_EVENT_DATASETS];

	TArray< FNiagaraDataInterfaceParamRef >& GetDIParameters()
	{
		return DataInterfaceParameters;
	}


private:
	FShaderUniformBufferParameter NiagaraUniformBuffer;

	// Data about parameters used for each Data Interface.
	TArray< FNiagaraDataInterfaceParamRef > DataInterfaceParameters;

	/*
	FDebugUniformExpressionSet	DebugUniformExpressionSet;
	FRHIUniformBufferLayout		DebugUniformExpressionUBLayout;
	*/
	FString						DebugDescription;

	/* OPTODO: ? */
	/*
	// If true, cached uniform expressions are allowed.
	static int32 bAllowCachedUniformExpressions;
	// Console variable ref to toggle cached uniform expressions.
	static FAutoConsoleVariableRef CVarAllowCachedUniformExpressions;
	*/


	/*
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING || !WITH_EDITOR)
	void VerifyExpressionAndShaderMaps(const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial& Material, const FUniformExpressionCache* UniformExpressionCache);
#endif
	*/
};


class FNiagaraEmitterInstanceShader : public FNiagaraShader
{

};

extern NIAGARASHADER_API int32 GNiagaraSkipVectorVMBackendOptimizations;
