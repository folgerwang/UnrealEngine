// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "RHI.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformApplicationMisc.h"

FDynamicRHI* PlatformCreateDynamicRHI()
{
	/*
	VulkanShaders && Vulkan = Vulkan

	VulkanShaders && !Vulkan && GLShader && OpenGL     = OpenGL
	VulkanShaders && !Vulkan && (!GLShader || !OpenGL) = FAIL

	!VulkanShaders && GLShader && OpenGL     = OpenGL
	!VulkanShaders && (!GLShader || !OpenGL) = FAIL

	ForceVulkan && VulkanShaders  && Vulkan    = Vulkan
	ForceVulkan && (!VulkanShaders || !Vulkan) = FAIL

	ForceGL && GLShaders && OpenGL     = OpenGL
	ForceGL && (!GLShaders || !OpenGL) = FAIL
	*/

	ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::SM5;	// SM4 is a dead level walking
	FDynamicRHI* DynamicRHI = nullptr;

	const bool bForceVulkan = FParse::Param(FCommandLine::Get(), TEXT("vulkan"));
	const bool bForceOpenGL = FParse::Param(FCommandLine::Get(), TEXT("opengl")) || FParse::Param(FCommandLine::Get(), TEXT("opengl4"))
																				 || FParse::Param(FCommandLine::Get(), TEXT("opengl3"));

	bool bVulkanFailed = false;
	bool bOpenGLFailed = false;

	IDynamicRHIModule* DynamicRHIModule = nullptr;

	TArray<FString> TargetedShaderFormats;
	GConfig->GetArray(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);

	// First come first serve
	for (int32 SfIdx = 0; SfIdx < TargetedShaderFormats.Num(); ++SfIdx)
	{
		// If we are not forcing opengl and we havent failed to create a VulkanRHI try to again with a different TargetedRHI
		if (!bForceOpenGL && !bVulkanFailed && TargetedShaderFormats[SfIdx].StartsWith(TEXT("SF_VULKAN_")))
		{
			DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("VulkanRHI"));
			if (!DynamicRHIModule->IsSupported())
			{
				DynamicRHIModule = nullptr;
				bVulkanFailed = true;
			}
			else
			{
				FApp::SetGraphicsRHI(TEXT("Vulkan"));
				FPlatformApplicationMisc::UsingVulkan();

				FName ShaderFormatName(*TargetedShaderFormats[SfIdx]);
				EShaderPlatform TargetedPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);
				RequestedFeatureLevel = GetMaxSupportedFeatureLevel(TargetedPlatform);
				break;
			}
		}
		else if (!bForceVulkan && !bOpenGLFailed && TargetedShaderFormats[SfIdx].StartsWith(TEXT("GLSL_")))
		{
			DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("OpenGLDrv"));
			if (!DynamicRHIModule->IsSupported())
			{
				DynamicRHIModule = nullptr;
				bOpenGLFailed = true;
			}
			else
			{
				FApp::SetGraphicsRHI(TEXT("OpenGL"));
				FPlatformApplicationMisc::UsingOpenGL();

				FName ShaderFormatName(*TargetedShaderFormats[SfIdx]);
				EShaderPlatform TargetedPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);
				RequestedFeatureLevel = GetMaxSupportedFeatureLevel(TargetedPlatform);
				break;
			}
		}
	}

	// Create the dynamic RHI.
	if (DynamicRHIModule)
	{
		DynamicRHI = DynamicRHIModule->CreateRHI(RequestedFeatureLevel);
	}
	else
	{
		if (bForceVulkan)
		{
			if (bVulkanFailed)
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "RequiredVulkan", "Vulkan Driver is required to run the engine."));
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "NoVulkanTargetedRHI", "Trying to force Vulkan RHI but the project does not have it in TargetedRHIs list."));
			}

			FPlatformMisc::RequestExitWithStatus(true, 1);
			// unreachable
			return nullptr;
		}
		else if (bForceOpenGL)
		{
			if (bOpenGLFailed)
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "RequiredOpenGL", "OpenGL 3.2 is required to run the engine."));
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "NoOpenGLTargetedRHI", "Trying to force OpenGL RHI but the project does not have it in TargetedRHIs list."));
			}

			FPlatformMisc::RequestExitWithStatus(true, 1);
			// unreachable
			return nullptr;
		}
		else
		{
			if (bVulkanFailed && bOpenGLFailed)
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "NoVulkanNoGL", "Vulkan or OpenGL (3.2) support is required to run the engine."));
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "NoTargetedRHI", "The project does not target Vulkan or OpenGL RHIs, check project settings or pass -nullrhi."));
			}

			FPlatformMisc::RequestExitWithStatus(true, 1);
			// unreachable
			return nullptr;
		}
	}

	return DynamicRHI;
}
