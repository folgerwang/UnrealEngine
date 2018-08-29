// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShadowRendering.h"

/*=============================================================================
	SceneOcclusion.h
=============================================================================*/

/**
* A vertex shader for rendering a texture on a simple element.
*/
class FOcclusionQueryVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FOcclusionQueryVS,Global);
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1); }

	FOcclusionQueryVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
			StencilingGeometryParameters.Bind(Initializer.ParameterMap);
	}

	FOcclusionQueryVS() {}

	TUniformBufferRef<FViewUniformShaderParameters> GetViewUniformBuffer(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		// TODO: Temporary hotfix for vertical axis not being flipped in the vertex shader
		if (RHINeedsToSwitchVerticalAxis(View.Family->GetShaderPlatform()) && !IsMobileHDR())
		{
			FViewUniformShaderParameters FlippedParameters = *View.CachedViewUniformShaderParameters;
			FlippedParameters.TranslatedWorldToClip.Mirror(EAxis::Y, EAxis::None);
			FlippedViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(FlippedParameters, UniformBuffer_SingleFrame);
			return FlippedViewUniformBuffer;
		}
		return View.ViewUniformBuffer;
	}

	void SetParametersWithBoundingSphere(FRHICommandList& RHICmdList, const FViewInfo& View, const FSphere& BoundingSphere)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(), GetViewUniformBuffer(RHICmdList, View));

		FVector4 StencilingSpherePosAndScale;
		StencilingGeometry::GStencilSphereVertexBuffer.CalcTransform(StencilingSpherePosAndScale, BoundingSphere, View.ViewMatrices.GetPreViewTranslation());
		StencilingGeometryParameters.Set(RHICmdList, this, StencilingSpherePosAndScale);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(), GetViewUniformBuffer(RHICmdList, View));

		// Don't transform if rendering frustum
		StencilingGeometryParameters.Set(RHICmdList, this, FVector4(0,0,0,1));
	}

	//~ Begin FShader Interface
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << StencilingGeometryParameters;
		return bShaderHasOutdatedParameters;
	}
	//~ Begin  End FShader Interface 

private:
	FStencilingGeometryShaderParameters StencilingGeometryParameters;
	TUniformBufferRef<FViewUniformShaderParameters> FlippedViewUniformBuffer;
};

