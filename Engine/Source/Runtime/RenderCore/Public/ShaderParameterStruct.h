// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderParameterStruct.h: API to submit all shader parameters in single function call.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Shader.h"
#include "RHI.h"
#include "RenderGraphResources.h"


/** Instrument a shader class to use strutured shader parameters API.
 *
 * class FMyShaderClassCS : public FGlobalShader
 * {
 *		DECLARE_GLOBAL_SHADER(FMyShaderClassCS);
 *		SHADER_USE_PARAMETER_STRUCT(FMyShaderClassCS, FGlobalShader);
 *
 *		BEGIN_SHADER_PARAMETER_STRUCT(FParameters)
 *			SHADER_PARAMETER(FMatrix, ViewToClip)
 *			//...
 *		END_SHADER_PARAMETER_STRUCT()
 * };
 *
 * Notes: Long therm, this macro will be no longer be needed.
 */
 // TODO(RDG): would not even need ShaderParentClass anymore. And in fact should not so Bindings.Bind() is not being called twice.
#define SHADER_USE_PARAMETER_STRUCT(ShaderClass, ShaderParentClass) \
	ShaderClass(const ShaderMetaType::CompiledShaderInitializerType& Initializer) \
		: ShaderParentClass(Initializer) \
	{ \
		this->Bindings.BindForLegacyShaderParameters( \
			this, \
			Initializer.ParameterMap, \
			*FParameters::FTypeInfo::GetStructMetadata(), \
			true); \
	} \
	\
	ShaderClass() \
	{ } \

#define SHADER_USE_ROOT_PARAMETER_STRUCT(ShaderClass, ShaderParentClass) \
	static inline const FShaderParametersMetadata* GetRootParametersMetadata() { return FParameters::FTypeInfo::GetStructMetadata(); } \
	\
	ShaderClass(const ShaderMetaType::CompiledShaderInitializerType& Initializer) \
		: ShaderParentClass(Initializer) \
	{ \
		this->Bindings.BindForRootShaderParameters(this, Initializer.ParameterMap); \
	} \
	\
	ShaderClass() \
	{ } \


/** Raise fatal error when a required shader parameter has not been set. */
extern RENDERCORE_API void EmitNullShaderParameterFatalError(const FShader* Shader, const FShaderParametersMetadata* ParametersMetadata, uint16 MemberOffset);


/** Validates that all resource parameters of a shader are set. */
#if DO_CHECK
extern RENDERCORE_API void ValidateShaderParameters(const FShader* Shader, const FShaderParametersMetadata* ParametersMetadata, const void* Parameters);

#else // !DO_CHECK
FORCEINLINE void ValidateShaderParameters(const FShader* Shader, const FShaderParametersMetadata* ParametersMetadata, const void* Parameters)
{ }

#endif // !DO_CHECK


/** Set compute shader UAVs. */
template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
inline void SetShaderUAVs(TRHICmdList& RHICmdList, const TShaderClass* Shader, TShaderRHI* ShadeRHI, const typename TShaderClass::FParameters& Parameters)
{}

template<typename TRHICmdList, typename TShaderClass>
inline void SetShaderUAVs(TRHICmdList& RHICmdList, const TShaderClass* Shader, FComputeShaderRHIParamRef ShadeRHI, const typename TShaderClass::FParameters& Parameters)
{
	const FShaderParameterBindings& Bindings = Shader->Bindings;

	const typename TShaderClass::FParameters* ParametersPtr = &Parameters;
	const uint8* Base = reinterpret_cast<const uint8*>(ParametersPtr);

	// Graph UAVs
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.GraphUAVs)
	{
		auto GraphUAV = *reinterpret_cast<const FRDGTextureUAV* const*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK)
		{
			if (!GraphUAV)
			{
				EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
			}
			else
			{
				// Mark this resource as used by the pass for inefficient pass resource dependency debugging purpose.
				GraphUAV->bIsActuallyUsedByPass = true;
			}
		}

		checkSlow(GraphUAV);
		RHICmdList.SetUAVParameter(ShadeRHI, ParameterBinding.BaseIndex, GraphUAV->CachedRHI.UAV);
	}
}


/** Unset compute shader UAVs. */
template<typename TRHICmdList, typename TShaderClass>
inline void UnsetShaderUAVs(TRHICmdList& RHICmdList, const TShaderClass* Shader, FComputeShaderRHIParamRef ShadeRHI)
{
	// TODO(RDG): Once all shader sets their parameter through this, can refactor RHI so all UAVs of a shader get unset through a single RHI function call.
	const FShaderParameterBindings& Bindings = Shader->Bindings;

	checkf(Bindings.RootParameterBufferIndex == FShaderParameterBindings::kInvalidBufferIndex, TEXT("Can't use UnsetShaderUAVs() for root parameter buffer index."));

	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.GraphUAVs)
	{
		RHICmdList.SetUAVParameter(ShadeRHI, ParameterBinding.BaseIndex, nullptr);
	}
}


/** Set shader's parameters from its parameters struct. */
template<typename TRHICmdList, typename TShaderClass, typename TShaderRHI>
inline void SetShaderParameters(TRHICmdList& RHICmdList, const TShaderClass* Shader, TShaderRHI* ShadeRHI, const typename TShaderClass::FParameters& Parameters)
{
	// TODO(RDG): Once all shader sets their parameter through this, can refactor RHI so all shader parameters get sets through a single RHI function call.
	const FShaderParameterBindings& Bindings = Shader->Bindings;

	const typename TShaderClass::FParameters* ParametersPtr = &Parameters;
	const uint8* Base = reinterpret_cast<const uint8*>(ParametersPtr);

	checkf(Bindings.RootParameterBufferIndex == FShaderParameterBindings::kInvalidBufferIndex, TEXT("Can't use SetShaderParameters() for root parameter buffer index."));

	// Parameters
	for (const FShaderParameterBindings::FParameter& ParameterBinding : Bindings.Parameters)
	{
		const void* DataPtr = reinterpret_cast<const char*>(&Parameters) + ParameterBinding.ByteOffset;
		RHICmdList.SetShaderParameter(ShadeRHI, ParameterBinding.BufferIndex, ParameterBinding.BaseIndex, ParameterBinding.ByteSize, DataPtr);
	}

	// Textures
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.Textures)
	{
		auto ShaderParameterRef = *reinterpret_cast<const FTextureRHIParamRef*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK && !ShaderParameterRef)
		{
			EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
		}

		RHICmdList.SetShaderTexture(ShadeRHI, ParameterBinding.BaseIndex, ShaderParameterRef);
	}

	// SRVs
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.SRVs)
	{
		auto ShaderParameterRef = *reinterpret_cast<const FShaderResourceViewRHIParamRef*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK && !ShaderParameterRef)
		{
			EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
		}

		RHICmdList.SetShaderResourceViewParameter(ShadeRHI, ParameterBinding.BaseIndex, ShaderParameterRef);
	}

	// Samplers
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.Samplers)
	{
		auto ShaderParameterRef = *reinterpret_cast<const FSamplerStateRHIParamRef*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK && !ShaderParameterRef)
		{
			EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
		}

		RHICmdList.SetShaderSampler(ShadeRHI, ParameterBinding.BaseIndex, ShaderParameterRef);
	}

	// Graph Textures
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.GraphTextures)
	{
		auto GraphTexture = *reinterpret_cast<const FRDGTexture* const*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK)
		{
			if (!GraphTexture)
			{
				EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
			}
			else
			{
				// Mark this resource as used by the pass for inefficient pass resource dependency debugging purpose.
				GraphTexture->bIsActuallyUsedByPass = true;
			}
		}

		checkSlow(GraphTexture);
		RHICmdList.SetShaderTexture(ShadeRHI, ParameterBinding.BaseIndex, GraphTexture->GetRHITexture());
	}

	// Graph SRVs
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.GraphSRVs)
	{
		auto GraphSRV = *reinterpret_cast<const FRDGTextureSRV* const*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK)
		{
			if (!GraphSRV)
			{
				EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
			}
			else
			{
				// Mark this resource as used by the pass for inefficient pass resource dependency debugging purpose.
				GraphSRV->bIsActuallyUsedByPass = true;
			}
		}

		checkSlow(GraphSRV);
		RHICmdList.SetShaderResourceViewParameter(ShadeRHI, ParameterBinding.BaseIndex, GraphSRV->CachedRHI.SRV);
	}

	// Graph UAVs for compute shaders
	SetShaderUAVs(RHICmdList, Shader, ShadeRHI, Parameters);

	// Reference structures
	for (const FShaderParameterBindings::FParameterStructReference& ParameterBinding : Bindings.ParameterReferences)
	{
		const TRefCountPtr<FRHIUniformBuffer>& ShaderParameterRef = *reinterpret_cast<const TRefCountPtr<FRHIUniformBuffer>*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK && !ShaderParameterRef.IsValid())
		{
			EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
		}

		RHICmdList.SetShaderUniformBuffer(ShadeRHI, ParameterBinding.BufferIndex, ShaderParameterRef);
	}
}


#if RHI_RAYTRACING

/** Set shader's parameters from its parameters struct. */
template<typename TShaderClass>
void SetShaderParameters(FRayTracingShaderBindingsWriter& RTBindingsWriter, const TShaderClass* Shader, const typename TShaderClass::FParameters& Parameters)
{
	const FShaderParameterBindings& Bindings = Shader->Bindings;

	checkf(Bindings.Parameters.Num() == 0, TEXT("Ray tracing shader should use SHADER_USE_ROOT_PARAMETER_STRUCT() to passdown the cbuffer layout to the shader compiler."));

	const typename TShaderClass::FParameters* ParametersPtr = &Parameters;
	const uint8* Base = reinterpret_cast<const uint8*>(ParametersPtr);

	// Textures
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.Textures)
	{
		auto ShaderParameterRef = *reinterpret_cast<const FTextureRHIParamRef*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK && !ShaderParameterRef)
		{
			EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
		}

		RTBindingsWriter.SetTexture(ParameterBinding.BaseIndex, ShaderParameterRef);
	}

	// SRVs
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.SRVs)
	{
		auto ShaderParameterRef = *reinterpret_cast<const FShaderResourceViewRHIParamRef*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK && !ShaderParameterRef)
		{
			EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
		}

		RTBindingsWriter.SetSRV(ParameterBinding.BaseIndex, ShaderParameterRef);
	}

	// Samplers
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.Samplers)
	{
		auto ShaderParameterRef = *reinterpret_cast<const FSamplerStateRHIParamRef*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK && !ShaderParameterRef)
		{
			EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
		}

		RTBindingsWriter.SetSampler(ParameterBinding.BaseIndex, ShaderParameterRef);
	}

	// Graph Textures
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.GraphTextures)
	{
		auto GraphTexture = *reinterpret_cast<const FRDGTexture* const*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK)
		{
			if (!GraphTexture)
			{
				EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
			}
			else
			{
				// Mark this resource as used by the pass for inefficient pass resource dependency debugging purpose.
				GraphTexture->bIsActuallyUsedByPass = true;
			}
		}

		checkSlow(GraphTexture);
		RTBindingsWriter.SetTexture(ParameterBinding.BaseIndex, GraphTexture->GetRHITexture());
	}

	// Graph SRVs
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.GraphSRVs)
	{
		auto GraphSRV = *reinterpret_cast<const FRDGResource* const*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK)
		{
			if (!GraphSRV)
			{
				EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
			}
			else
			{
				// Mark this resource as used by the pass for inefficient pass resource dependency debugging purpose.
				GraphSRV->bIsActuallyUsedByPass = true;
			}
		}

		checkSlow(GraphSRV);
		RTBindingsWriter.SetSRV(ParameterBinding.BaseIndex, GraphSRV->CachedRHI.SRV);
	}

	// Render graph UAVs
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.GraphUAVs)
	{
		auto UAV = *reinterpret_cast<const FRDGResource* const*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK)
		{
			if (!UAV)
			{
				EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
			}
			else
			{
				// Mark this resource as used by the pass for inefficient pass resource dependency debugging purpose.
				UAV->bIsActuallyUsedByPass = true;
			}
		}

		checkSlow(UAV);
		RTBindingsWriter.SetUAV(ParameterBinding.BaseIndex, UAV->CachedRHI.UAV);
	}

	// Referenced uniform buffers
	for (const FShaderParameterBindings::FParameterStructReference& ParameterBinding : Bindings.ParameterReferences)
	{
		const TRefCountPtr<FRHIUniformBuffer>& ShaderParameterRef = *reinterpret_cast<const TRefCountPtr<FRHIUniformBuffer>*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK && !ShaderParameterRef.IsValid())
		{
			EmitNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
		}

		RTBindingsWriter.SetUniformBuffer(ParameterBinding.BufferIndex, ShaderParameterRef);
	}

	// Root uniform buffer.
	if (Bindings.RootParameterBufferIndex != FShaderParameterBindings::kInvalidBufferIndex)
	{
		// Do not do any validation at some resources may have been removed from the structure because known to not be used by the shader.
		EUniformBufferValidation Validation = EUniformBufferValidation::None;

		FUniformBufferRHIParamRef RootUniformBuffer = CreateUniformBufferImmediate(Parameters, UniformBuffer_SingleDraw, Validation);
		RTBindingsWriter.SetUniformBuffer(Bindings.RootParameterBufferIndex, RootUniformBuffer);
	}
}

#endif // RHI_RAYTRACING
