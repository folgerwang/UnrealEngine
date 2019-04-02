// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenColorIOShader.h: Shader base classes
=============================================================================*/

#pragma once

#include "GlobalShader.h"
#include "OpenColorIOShaderType.h"
#include "OpenColorIOShared.h"
#include "SceneView.h"
#include "Shader.h"
#include "ShaderParameters.h"

class FTextureResource;
class UClass;

namespace OpenColorIOShader
{
	static const TCHAR* OpenColorIOShaderFunctionName = TEXT("OCIOConvert");
	static const TCHAR* OCIOLut3dName = TEXT("ociolut3d_0");
	static const uint32 MaximumTextureNumber = 10;
	static const uint32 Lut3dEdgeLength = 32;
}



/** Base class of all shaders that need OpenColorIO pixel shader parameters. */
class OPENCOLORIO_API FOpenColorIOPixelShader : public FShader
{
public:
	DECLARE_SHADER_TYPE(FOpenColorIOPixelShader, OpenColorIO);

	FOpenColorIOPixelShader()
	{
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FOpenColorIOTransformResource*  InColorTransform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}


	FOpenColorIOPixelShader(const FOpenColorIOShaderType::CompiledShaderInitializerType& Initializer);

	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform, const FOpenColorIOTransformResource* , FShaderCompilerEnvironment&);

	static void ModifyCompilationEnvironment(EShaderPlatform InPlatform, const FOpenColorIOTransformResource*  InColorTransform, FShaderCompilerEnvironment& OutEnvironment)
	{
	}
	
	static bool ValidateCompiledResult(EShaderPlatform InPlatform, const FShaderParameterMap& InParameterMap, TArray<FString>& OutError)
	{
		return true;
	}

	void SetParameters(FRHICommandList& InRHICmdList, FTextureResource* InInputTexture);
	void SetLUTParameter(FRHICommandList& InRHICmdList, FTextureResource* InLUT3dResource);
	
	// Bind parameters
	void BindParams(const FShaderParameterMap &ParameterMap);

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override;
	virtual uint32 GetAllocatedSize() const override;

protected:
	FShaderResourceParameter InputTexture;
	FShaderResourceParameter InputTextureSampler;
	
	FShaderResourceParameter OCIO3dTexture;
	FShaderResourceParameter OCIO3dTextureSampler;

private:
	FString						DebugDescription;
};


