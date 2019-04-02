// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHIShaderFormatDefinitions.h: Names for Shader Formats
		(that don't require linking).
=============================================================================*/

#pragma once


static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
static FName NAME_PCD3D_SM4(TEXT("PCD3D_SM4"));
static FName NAME_PCD3D_ES3_1(TEXT("PCD3D_ES31"));
static FName NAME_PCD3D_ES2(TEXT("PCD3D_ES2"));

static FName NAME_GLSL_150(TEXT("GLSL_150"));
static FName NAME_GLSL_430(TEXT("GLSL_430"));
static FName NAME_GLSL_150_ES2(TEXT("GLSL_150_ES2"));
static FName NAME_GLSL_150_ES2_NOUB(TEXT("GLSL_150_ES2_NOUB"));
static FName NAME_GLSL_150_ES31(TEXT("GLSL_150_ES31"));
static FName NAME_GLSL_ES2(TEXT("GLSL_ES2"));
static FName NAME_GLSL_ES2_WEBGL(TEXT("GLSL_ES2_WEBGL"));
static FName NAME_GLSL_ES2_IOS(TEXT("GLSL_ES2_IOS"));
static FName NAME_GLSL_310_ES_EXT(TEXT("GLSL_310_ES_EXT"));
static FName NAME_GLSL_ES3_1_ANDROID(TEXT("GLSL_ES3_1_ANDROID"));

static FName NAME_SF_PS4(TEXT("SF_PS4"));

static FName NAME_SF_XBOXONE_D3D12(TEXT("SF_XBOXONE_D3D12"));

static FName NAME_GLSL_SWITCH(TEXT("GLSL_SWITCH"));
static FName NAME_GLSL_SWITCH_FORWARD(TEXT("GLSL_SWITCH_FORWARD"));

static FName NAME_SF_METAL(TEXT("SF_METAL"));
static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));
static FName NAME_SF_METAL_TVOS(TEXT("SF_METAL_TVOS"));
static FName NAME_SF_METAL_MRT_TVOS(TEXT("SF_METAL_MRT_TVOS"));
static FName NAME_SF_METAL_MRT_MAC(TEXT("SF_METAL_MRT_MAC"));
static FName NAME_SF_METAL_SM5(TEXT("SF_METAL_SM5"));
static FName NAME_SF_METAL_SM5_NOTESS(TEXT("SF_METAL_SM5_NOTESS"));
static FName NAME_SF_METAL_MACES3_1(TEXT("SF_METAL_MACES3_1"));
static FName NAME_SF_METAL_MACES2(TEXT("SF_METAL_MACES2"));

static FName NAME_VULKAN_ES3_1_ANDROID(TEXT("SF_VULKAN_ES31_ANDROID"));
static FName NAME_VULKAN_ES3_1_ANDROID_NOUB(TEXT("SF_VULKAN_ES31_ANDROID_NOUB"));
static FName NAME_VULKAN_ES3_1_LUMIN(TEXT("SF_VULKAN_ES31_LUMIN"));
static FName NAME_VULKAN_ES3_1_LUMIN_NOUB(TEXT("SF_VULKAN_ES31_LUMIN_NOUB"));
static FName NAME_VULKAN_ES3_1(TEXT("SF_VULKAN_ES31"));
static FName NAME_VULKAN_ES3_1_NOUB(TEXT("SF_VULKAN_ES31_NOUB"));
static FName NAME_VULKAN_SM4_NOUB(TEXT("SF_VULKAN_SM4_NOUB"));
static FName NAME_VULKAN_SM4(TEXT("SF_VULKAN_SM4"));
static FName NAME_VULKAN_SM5_NOUB(TEXT("SF_VULKAN_SM5_NOUB"));
static FName NAME_VULKAN_SM5(TEXT("SF_VULKAN_SM5"));
static FName NAME_VULKAN_SM5_LUMIN(TEXT("SF_VULKAN_SM5_LUMIN"));
static FName NAME_VULKAN_SM5_LUMIN_NOUB(TEXT("SF_VULKAN_SM5_LUMIN_NOUB"));


static FName ShaderPlatformToShaderFormatName(EShaderPlatform Platform)
{
	switch (Platform)
	{
	case SP_PCD3D_SM5:
		return NAME_PCD3D_SM5;
	case SP_PCD3D_SM4:
		return NAME_PCD3D_SM4;
	case SP_PCD3D_ES3_1:
		return NAME_PCD3D_ES3_1;
	case SP_PCD3D_ES2:
		return NAME_PCD3D_ES2;

	case SP_OPENGL_SM4:
		return NAME_GLSL_150;
	case SP_OPENGL_SM5:
		return NAME_GLSL_430;
	case SP_OPENGL_PCES2:
	{
		static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("OpenGL.UseEmulatedUBs"));
		return (CVar && CVar->GetValueOnAnyThread() != 0) ? NAME_GLSL_150_ES2_NOUB : NAME_GLSL_150_ES2;
	}
	case SP_OPENGL_PCES3_1:
		return NAME_GLSL_150_ES31;
	case SP_OPENGL_ES2_ANDROID:
		return NAME_GLSL_ES2;
	case SP_OPENGL_ES2_WEBGL:
		return NAME_GLSL_ES2_WEBGL;
	case SP_OPENGL_ES2_IOS:
		return NAME_GLSL_ES2_IOS;
	case SP_OPENGL_ES31_EXT:
		return NAME_GLSL_310_ES_EXT;
	case SP_OPENGL_ES3_1_ANDROID:
		return NAME_GLSL_ES3_1_ANDROID;

	case SP_PS4:
		return NAME_SF_PS4;

	case SP_XBOXONE_D3D12:
		return NAME_SF_XBOXONE_D3D12;

	case SP_SWITCH:
		return NAME_GLSL_SWITCH;
	case SP_SWITCH_FORWARD:
		return NAME_GLSL_SWITCH_FORWARD;

	case SP_METAL:
		return NAME_SF_METAL;
	case SP_METAL_MRT:
		return NAME_SF_METAL_MRT;
	case SP_METAL_TVOS:
		return NAME_SF_METAL_TVOS;
	case SP_METAL_MRT_TVOS:
		return NAME_SF_METAL_MRT_TVOS;
	case SP_METAL_MRT_MAC:
		return NAME_SF_METAL_MRT_MAC;
	case SP_METAL_SM5:
		return NAME_SF_METAL_SM5;
	case SP_METAL_SM5_NOTESS:
		return NAME_SF_METAL_SM5_NOTESS;
	case SP_METAL_MACES3_1:
		return NAME_SF_METAL_MACES3_1;
	case SP_METAL_MACES2:
		return NAME_SF_METAL_MACES2;

	case SP_VULKAN_ES3_1_ANDROID:
		// If you modify this, make sure to update FAndroidTargetPlatform::GetAllPossibleShaderFormats() and FVulkanAndroidPlatform::UseRealUBsOptimization()
		return NAME_VULKAN_ES3_1_ANDROID_NOUB;//NAME_VULKAN_ES3_1_ANDROID;

	case SP_VULKAN_ES3_1_LUMIN:
	{
		static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
		return (CVar && CVar->GetValueOnAnyThread() == 0) ? NAME_VULKAN_ES3_1_LUMIN_NOUB : NAME_VULKAN_ES3_1_LUMIN;
	}

	case SP_VULKAN_PCES3_1:
	{
		static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
		return (CVar && CVar->GetValueOnAnyThread() == 0) ? NAME_VULKAN_ES3_1_NOUB : NAME_VULKAN_ES3_1;
	}
	case SP_VULKAN_SM4:
	{
		static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
		return (CVar && CVar->GetValueOnAnyThread() == 0) ? NAME_VULKAN_SM4_NOUB : NAME_VULKAN_SM4;
	}
	case SP_VULKAN_SM5:
	{
		static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
		return (CVar && CVar->GetValueOnAnyThread() == 0) ? NAME_VULKAN_SM5_NOUB : NAME_VULKAN_SM5;
	}
	case SP_VULKAN_SM5_LUMIN:
	{
		static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
		return (CVar && CVar->GetValueOnAnyThread() == 0) ? NAME_VULKAN_SM5_LUMIN_NOUB : NAME_VULKAN_SM5_LUMIN;
	}

	default:
		checkf(0, TEXT("Unknown EShaderPlatform %d!"), (int32)Platform);
		return NAME_PCD3D_SM5;
	}
}

static EShaderPlatform ShaderFormatNameToShaderPlatform(FName ShaderFormat)
{
	if (ShaderFormat == NAME_PCD3D_SM5)					return SP_PCD3D_SM5;
	if (ShaderFormat == NAME_PCD3D_SM4)					return SP_PCD3D_SM4;
	if (ShaderFormat == NAME_PCD3D_ES3_1)				return SP_PCD3D_ES3_1;
	if (ShaderFormat == NAME_PCD3D_ES2)					return SP_PCD3D_ES2;

	if (ShaderFormat == NAME_GLSL_150)					return SP_OPENGL_SM4;
	if (ShaderFormat == NAME_GLSL_430)					return SP_OPENGL_SM5;
	if (ShaderFormat == NAME_GLSL_150_ES2)				return SP_OPENGL_PCES2;
	if (ShaderFormat == NAME_GLSL_150_ES2_NOUB)			return SP_OPENGL_PCES2;
	if (ShaderFormat == NAME_GLSL_150_ES31)				return SP_OPENGL_PCES3_1;
	if (ShaderFormat == NAME_GLSL_ES2)					return SP_OPENGL_ES2_ANDROID;
	if (ShaderFormat == NAME_GLSL_ES2_WEBGL)			return SP_OPENGL_ES2_WEBGL;
	if (ShaderFormat == NAME_GLSL_ES2_IOS)				return SP_OPENGL_ES2_IOS;
	if (ShaderFormat == NAME_GLSL_310_ES_EXT)			return SP_OPENGL_ES31_EXT;
	if (ShaderFormat == NAME_GLSL_ES3_1_ANDROID)		return SP_OPENGL_ES3_1_ANDROID;

	if (ShaderFormat == NAME_SF_PS4)					return SP_PS4;

	if (ShaderFormat == NAME_SF_XBOXONE_D3D12)			return SP_XBOXONE_D3D12;

	if (ShaderFormat == NAME_GLSL_SWITCH)				return SP_SWITCH;
	if (ShaderFormat == NAME_GLSL_SWITCH_FORWARD)		return SP_SWITCH_FORWARD;

	if (ShaderFormat == NAME_SF_METAL)					return SP_METAL;
	if (ShaderFormat == NAME_SF_METAL_MRT)				return SP_METAL_MRT;
	if (ShaderFormat == NAME_SF_METAL_TVOS)				return SP_METAL_TVOS;
	if (ShaderFormat == NAME_SF_METAL_MRT_TVOS)			return SP_METAL_MRT_TVOS;
	if (ShaderFormat == NAME_SF_METAL_MRT_MAC)			return SP_METAL_MRT_MAC;
	if (ShaderFormat == NAME_SF_METAL_SM5)				return SP_METAL_SM5;
	if (ShaderFormat == NAME_SF_METAL_SM5_NOTESS)		return SP_METAL_SM5_NOTESS;
	if (ShaderFormat == NAME_SF_METAL_MACES3_1)			return SP_METAL_MACES3_1;
	if (ShaderFormat == NAME_SF_METAL_MACES2)			return SP_METAL_MACES2;

	if (ShaderFormat == NAME_VULKAN_ES3_1_ANDROID)		return SP_VULKAN_ES3_1_ANDROID;
	if (ShaderFormat == NAME_VULKAN_ES3_1_ANDROID_NOUB)	return SP_VULKAN_ES3_1_ANDROID;
	if (ShaderFormat == NAME_VULKAN_ES3_1_LUMIN)		return SP_VULKAN_ES3_1_LUMIN;
	if (ShaderFormat == NAME_VULKAN_ES3_1_LUMIN_NOUB)	return SP_VULKAN_ES3_1_LUMIN;
	if (ShaderFormat == NAME_VULKAN_ES3_1)				return SP_VULKAN_PCES3_1;
	if (ShaderFormat == NAME_VULKAN_ES3_1_NOUB)			return SP_VULKAN_PCES3_1;
	if (ShaderFormat == NAME_VULKAN_SM4_NOUB)			return SP_VULKAN_SM4;
	if (ShaderFormat == NAME_VULKAN_SM4)				return SP_VULKAN_SM4;
	if (ShaderFormat == NAME_VULKAN_SM5_NOUB)			return SP_VULKAN_SM5;
	if (ShaderFormat == NAME_VULKAN_SM5)				return SP_VULKAN_SM5;
	if (ShaderFormat == NAME_VULKAN_SM5_LUMIN)			return SP_VULKAN_SM5_LUMIN;
	if (ShaderFormat == NAME_VULKAN_SM5_LUMIN_NOUB)		return SP_VULKAN_SM5_LUMIN;

	return SP_NumPlatforms;
}
