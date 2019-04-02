// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VulkanShaderFormat.h"
#include "VulkanCommon.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "hlslcc.h"
#include "ShaderCore.h"

static FName NAME_VULKAN_ES3_1_ANDROID(TEXT("SF_VULKAN_ES31_ANDROID"));
static FName NAME_VULKAN_ES3_1_ANDROID_NOUB(TEXT("SF_VULKAN_ES31_ANDROID_NOUB"));
static FName NAME_VULKAN_ES3_1(TEXT("SF_VULKAN_ES31"));
static FName NAME_VULKAN_ES3_1_LUMIN(TEXT("SF_VULKAN_ES31_LUMIN"));
static FName NAME_VULKAN_ES3_1_LUMIN_NOUB(TEXT("SF_VULKAN_ES31_LUMIN_NOUB"));
static FName NAME_VULKAN_ES3_1_NOUB(TEXT("SF_VULKAN_ES31_NOUB"));
static FName NAME_VULKAN_SM4_NOUB(TEXT("SF_VULKAN_SM4_NOUB"));
static FName NAME_VULKAN_SM4(TEXT("SF_VULKAN_SM4"));
static FName NAME_VULKAN_SM5_NOUB(TEXT("SF_VULKAN_SM5_NOUB"));
static FName NAME_VULKAN_SM5(TEXT("SF_VULKAN_SM5"));
static FName NAME_VULKAN_SM5_LUMIN(TEXT("SF_VULKAN_SM5_LUMIN"));
static FName NAME_VULKAN_SM5_LUMIN_NOUB(TEXT("SF_VULKAN_SM5_LUMIN_NOUB"));

class FShaderFormatVulkan : public IShaderFormat
{
	enum 
	{
		UE_SHADER_VULKAN_ES3_1_VER	= 26,
		UE_SHADER_VULKAN_SM5_VER 	= 26,
	};

	int32 InternalGetVersion(FName Format) const
	{
		if (Format == NAME_VULKAN_SM4 || Format == NAME_VULKAN_SM4_NOUB)
		{
			return UE_SHADER_VULKAN_SM5_VER;
		}
		else if (Format == NAME_VULKAN_SM5 || Format == NAME_VULKAN_SM5_NOUB || Format == NAME_VULKAN_SM5_LUMIN || Format == NAME_VULKAN_SM5_LUMIN_NOUB)
		{
			return UE_SHADER_VULKAN_SM5_VER;
		}
		else if (Format == NAME_VULKAN_ES3_1_ANDROID || Format == NAME_VULKAN_ES3_1_ANDROID_NOUB || Format == NAME_VULKAN_ES3_1 || Format == NAME_VULKAN_ES3_1_NOUB || Format == NAME_VULKAN_ES3_1_LUMIN || Format == NAME_VULKAN_ES3_1_LUMIN_NOUB)
		{
			return UE_SHADER_VULKAN_ES3_1_VER;
		}

		check(0);
		return -1;
	}

public:
	virtual uint32 GetVersion(FName Format) const override
	{
		const uint8 HLSLCCVersion = ((HLSLCC_VersionMajor & 0x0f) << 4) | (HLSLCC_VersionMinor & 0x0f);
		uint16 Version = ((HLSLCCVersion & 0xff) << 8) | (InternalGetVersion(Format) & 0xff);
#if VULKAN_ENABLE_SHADER_DEBUG_NAMES
		Version = (Version << 1) + Version;
#endif
		return Version;
	}
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const
	{
		OutFormats.Add(NAME_VULKAN_SM4);
		OutFormats.Add(NAME_VULKAN_SM5);
		OutFormats.Add(NAME_VULKAN_SM5_LUMIN);
		OutFormats.Add(NAME_VULKAN_SM5_LUMIN_NOUB);
		OutFormats.Add(NAME_VULKAN_ES3_1_ANDROID);
		OutFormats.Add(NAME_VULKAN_ES3_1_ANDROID_NOUB);
		OutFormats.Add(NAME_VULKAN_ES3_1);
		OutFormats.Add(NAME_VULKAN_ES3_1_LUMIN);
		OutFormats.Add(NAME_VULKAN_ES3_1_LUMIN_NOUB);
		OutFormats.Add(NAME_VULKAN_ES3_1_NOUB);
		OutFormats.Add(NAME_VULKAN_SM4_NOUB);
		OutFormats.Add(NAME_VULKAN_SM5_NOUB);
	}

	virtual void CompileShader(FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory) const
	{
		check(InternalGetVersion(Format) >= 0);
		if (Format == NAME_VULKAN_ES3_1 || Format == NAME_VULKAN_ES3_1_LUMIN)
		{
			DoCompileVulkanShader(Input, Output, WorkingDirectory, EVulkanShaderVersion::ES3_1);
		}
		else if (Format == NAME_VULKAN_ES3_1_NOUB || Format == NAME_VULKAN_ES3_1_LUMIN_NOUB)
		{
			DoCompileVulkanShader(Input, Output, WorkingDirectory, EVulkanShaderVersion::ES3_1_NOUB);
		}
		else if (Format == NAME_VULKAN_ES3_1_ANDROID)
		{
			DoCompileVulkanShader(Input, Output, WorkingDirectory, EVulkanShaderVersion::ES3_1_ANDROID);
		}
		else if (Format == NAME_VULKAN_ES3_1_ANDROID_NOUB)
		{
			DoCompileVulkanShader(Input, Output, WorkingDirectory, EVulkanShaderVersion::ES3_1_ANDROID_NOUB);
		}
		else if (Format == NAME_VULKAN_SM4_NOUB)
		{
			DoCompileVulkanShader(Input, Output, WorkingDirectory, EVulkanShaderVersion::SM4_NOUB);
		}
		else if (Format == NAME_VULKAN_SM4)
		{
			DoCompileVulkanShader(Input, Output, WorkingDirectory, EVulkanShaderVersion::SM4);
		}
		else if (Format == NAME_VULKAN_SM5_NOUB || Format == NAME_VULKAN_SM5_LUMIN_NOUB)
		{
			DoCompileVulkanShader(Input, Output, WorkingDirectory, EVulkanShaderVersion::SM5_NOUB);
		}
		else if (Format == NAME_VULKAN_SM5 || Format == NAME_VULKAN_SM5_LUMIN)
		{
			DoCompileVulkanShader(Input, Output, WorkingDirectory, EVulkanShaderVersion::SM5);
		}
	}

	//virtual bool CreateLanguage(FName Format, ILanguageSpec*& OutSpec, FCodeBackend*& OutBackend, uint32 InHlslCompileFlags) override
	//{
	//	OutSpec = new FVulkanLanguageSpec(false);
	//	OutBackend = new FVulkanCodeBackend(InHlslCompileFlags, HCT_FeatureLevelSM4);
	//	return false;
	//}

	virtual const TCHAR* GetPlatformIncludeDirectory() const
	{
		return TEXT("Vulkan");
	}
};

/**
 * Module for Vulkan shaders
 */

static IShaderFormat* Singleton = NULL;

class FVulkanShaderFormatModule : public IShaderFormatModule
{
public:
	virtual ~FVulkanShaderFormatModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual IShaderFormat* GetShaderFormat()
	{
		if (!Singleton)
		{
			Singleton = new FShaderFormatVulkan();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FVulkanShaderFormatModule, VulkanShaderFormat);
