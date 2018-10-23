// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
		this->Bindings.Bind(Initializer.ParameterMap, *FParameters::FTypeInfo::GetStructMetadata()); \
	} \
	\
	ShaderClass() \
	{ } \


/** Raise fatal error when a required shader parameter has not been set. */
extern RENDERCORE_API void SetNullShaderParameterFatalError(const FShader* Shader, const FShaderParametersMetadata* ParametersMetadata, uint16 MemberOffset);


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
				SetNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
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
			SetNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
		}

		RHICmdList.SetShaderTexture(ShadeRHI, ParameterBinding.BaseIndex, ShaderParameterRef);
	}

	// SRVs
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.SRVs)
	{
		auto ShaderParameterRef = *reinterpret_cast<const FShaderResourceViewRHIParamRef*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK && !ShaderParameterRef)
		{
			SetNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
		}

		RHICmdList.SetShaderResourceViewParameter(ShadeRHI, ParameterBinding.BaseIndex, ShaderParameterRef);
	}

	// Samplers
	for (const FShaderParameterBindings::FResourceParameter& ParameterBinding : Bindings.Samplers)
	{
		auto ShaderParameterRef = *reinterpret_cast<const FSamplerStateRHIParamRef*>(Base + ParameterBinding.ByteOffset);

		if (DO_CHECK && !ShaderParameterRef)
		{
			SetNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
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
				SetNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
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
				SetNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
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
			SetNullShaderParameterFatalError(Shader, TShaderClass::FParameters::FTypeInfo::GetStructMetadata(), ParameterBinding.ByteOffset);
		}

		RHICmdList.SetShaderUniformBuffer(ShadeRHI, ParameterBinding.BufferIndex, ShaderParameterRef);
	}
}
