// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"

static const TCHAR* GLoadedRHIModuleName;

static bool ShouldPreferD3D12()
{
	// Disabled until D3D12RHI is ready
#if 0
	bool bPreferD3D12 = false;
	if (GIsEditor)
	{
		GConfig->GetBool(TEXT("D3DRHIPerference"), TEXT("bPreferD3D12InEditor"), bPreferD3D12, GEngineIni);
	}
	else
	{
		GConfig->GetBool(TEXT("D3DRHIPerference"), TEXT("bPreferD3D12InGame"), bPreferD3D12, GEngineIni);
	}

	int32 MinNumCPUCores = 0;
	GConfig->GetInt(TEXT("D3DRHIPerference"), TEXT("con.MinNumCPUCores"), MinNumCPUCores, GEngineIni);
	const bool bHasEnoughCPUCores = FPlatformMisc::NumberOfCoresIncludingHyperthreads() >= MinNumCPUCores;

	int32 MinPhysicalMemGB = 0;
	GConfig->GetInt(TEXT("D3DRHIPerference"), TEXT("con.MinPhysicalMemGB"), MinPhysicalMemGB, GEngineIni);
	const bool bHasEnoughMem = FPlatformMemory::GetConstants().TotalPhysical >= MinPhysicalMemGB * (1llu << 30);

	return bPreferD3D12 && bHasEnoughCPUCores && bHasEnoughMem;
#else
	return false;
#endif
}

static IDynamicRHIModule* LoadDynamicRHIModule(ERHIFeatureLevel::Type& DesiredFeatureLevel, const TCHAR*& LoadedRHIModuleName)
{
	bool bPreferD3D12 = ShouldPreferD3D12();
	
	// command line overrides
	bool bForceSM5 = FParse::Param(FCommandLine::Get(), TEXT("sm5"));
	bool bForceSM4 = FParse::Param(FCommandLine::Get(), TEXT("sm4"));
	bool bForceVulkan = FParse::Param(FCommandLine::Get(), TEXT("vulkan"));
	bool bForceOpenGL = FWindowsPlatformMisc::VerifyWindowsVersion(6, 0) == false || FParse::Param(FCommandLine::Get(), TEXT("opengl")) || FParse::Param(FCommandLine::Get(), TEXT("opengl3")) || FParse::Param(FCommandLine::Get(), TEXT("opengl4"));
	bool bForceD3D10 = FParse::Param(FCommandLine::Get(), TEXT("d3d10")) || FParse::Param(FCommandLine::Get(), TEXT("dx10")) || (bForceSM4 && !bForceVulkan && !bForceOpenGL);
	bool bForceD3D11 = FParse::Param(FCommandLine::Get(), TEXT("d3d11")) || FParse::Param(FCommandLine::Get(), TEXT("dx11")) || (bForceSM5 && !bForceVulkan && !bForceOpenGL);
	bool bForceD3D12 = FParse::Param(FCommandLine::Get(), TEXT("d3d12")) || FParse::Param(FCommandLine::Get(), TEXT("dx12"));
	DesiredFeatureLevel = ERHIFeatureLevel::Num;
	
	if(!(bForceVulkan||bForceOpenGL||bForceD3D10||bForceD3D11||bForceD3D12))
	{
		//Default graphics RHI is only used if no command line option is specified
		FConfigFile EngineSettings;
		FString PlatformNameString = FPlatformProperties::PlatformName();
		const TCHAR* PlatformName = *PlatformNameString;
		FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, PlatformName);
		FString DefaultGraphicsRHI;
		if(EngineSettings.GetString(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("DefaultGraphicsRHI"), DefaultGraphicsRHI))
		{
			static FString NAME_DX11(TEXT("DefaultGraphicsRHI_DX11"));
			static FString NAME_DX12(TEXT("DefaultGraphicsRHI_DX12"));
			static FString NAME_VULKAN(TEXT("DefaultGraphicsRHI_Vulkan"));
			static FString NAME_OPENGL(TEXT("DefaultGraphicsRHI_OpenGL"));
			if(DefaultGraphicsRHI == NAME_DX11)
			{
				bForceD3D11 = true;
			}
			else if (DefaultGraphicsRHI == NAME_DX12)
			{
				bForceD3D12 = true;
			}
			else if (DefaultGraphicsRHI == NAME_VULKAN)
			{
				bForceVulkan = true;
			}
			else if (DefaultGraphicsRHI == NAME_OPENGL)
			{
				bForceOpenGL = true;
			}
		}
	}



	int32 Sum = ((bForceD3D12 ? 1 : 0) + (bForceD3D11 ? 1 : 0) + (bForceD3D10 ? 1 : 0) + (bForceOpenGL ? 1 : 0) + (bForceVulkan ? 1 : 0));

	if (bForceSM5 && bForceSM4)
	{
		UE_LOG(LogRHI, Fatal, TEXT("-sm4 and -sm5 are mutually exclusive options, but more than one was specified on the command-line."));
	}

	if (Sum > 1)
	{
		UE_LOG(LogRHI, Fatal, TEXT("-d3d12, -d3d11, -d3d10, -vulkan, and -opengl[3|4] are mutually exclusive options, but more than one was specified on the command-line."));
	}
	else if (Sum == 0)
	{
		// Check the list of targeted shader platforms and decide an RHI based off them
		TArray<FString> TargetedShaderFormats;
		GConfig->GetArray(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);
		if (TargetedShaderFormats.Num() > 0)
		{
			// Pick the first one
			FName ShaderFormatName(*TargetedShaderFormats[0]);
			EShaderPlatform TargetedPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);
			bForceVulkan = IsVulkanPlatform(TargetedPlatform);
			bForceD3D11 = !bPreferD3D12 && IsD3DPlatform(TargetedPlatform, false);
			bForceOpenGL = IsOpenGLPlatform(TargetedPlatform);
			DesiredFeatureLevel = GetMaxSupportedFeatureLevel(TargetedPlatform);
		}
	}
	else
	{
		if (bForceSM5)
		{
			DesiredFeatureLevel = ERHIFeatureLevel::SM5;
		}

		if (bForceSM4)
		{
			DesiredFeatureLevel = ERHIFeatureLevel::SM4;
			bPreferD3D12 = false;
		}
	}

	// Load the dynamic RHI module.
	IDynamicRHIModule* DynamicRHIModule = NULL;

#if defined(SWITCHRHI)
	const bool bForceSwitch = FParse::Param(FCommandLine::Get(), TEXT("switch"));
	// Load the dynamic RHI module.
	if (bForceSwitch)
	{
#define A(x) #x
#define B(x) A(x)
#define SWITCH_RHI_STR B(SWITCHRHI)
		const TCHAR* SwitchRHIModuleName = TEXT(SWITCH_RHI_STR);
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(SwitchRHIModuleName);
		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("SwitchDynamicRHI", "UnsupportedRHI", "The chosen RHI is not supported"));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = NULL;
		}
		LoadedRHIModuleName = SwitchRHIModuleName;
		FApp::SetGraphicsRHI(TEXT("Switch"));
	}
	else
#endif

	if (bForceOpenGL)
	{
		const TCHAR* OpenGLRHIModuleName = TEXT("OpenGLDrv");
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(OpenGLRHIModuleName);

		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredOpenGL", "OpenGL 3.2 is required to run the engine."));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = NULL;
		}
		LoadedRHIModuleName = OpenGLRHIModuleName;
		FApp::SetGraphicsRHI(TEXT("OpenGL"));
	}
	else if (bForceVulkan)
	{
		const TCHAR* VulkanRHIModuleName = TEXT("VulkanRHI");
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(VulkanRHIModuleName);
		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredVulkan", "Vulkan Driver is required to run the engine."));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = NULL;
		}
		LoadedRHIModuleName = VulkanRHIModuleName;
		FApp::SetGraphicsRHI(TEXT("Vulkan"));
	}
	else if (bForceD3D12 || bPreferD3D12)
	{
		LoadedRHIModuleName = TEXT("D3D12RHI");
		DynamicRHIModule = FModuleManager::LoadModulePtr<IDynamicRHIModule>(LoadedRHIModuleName);

		if (!DynamicRHIModule || !DynamicRHIModule->IsSupported())
		{
			if (bForceD3D12)
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredDX12", "DX12 is not supported on your system. Try running without the -dx12 or -d3d12 command line argument."));
				FPlatformMisc::RequestExit(1);
			}
			if (DynamicRHIModule)
			{
				FModuleManager::Get().UnloadModule(LoadedRHIModuleName);
			}
			DynamicRHIModule = NULL;
			LoadedRHIModuleName = nullptr;
		}
		else if (FPlatformProcess::IsApplicationRunning(TEXT("fraps.exe")))
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "UseExpressionEncoder", "Fraps has been known to crash D3D12. Please use Microsoft Expression Encoder instead for capturing."));
		}
		FApp::SetGraphicsRHI(TEXT("DirectX 12"));
	}

	// Fallback to D3D11RHI if nothing is selected
	if (!DynamicRHIModule)
	{
		const TCHAR* D3D11RHIModuleName = TEXT("D3D11RHI");
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(D3D11RHIModuleName);

		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredDX11Feature", "DX11 feature level 10.0 is required to run the engine."));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = NULL;
		}
		else if (FPlatformProcess::IsApplicationRunning(TEXT("fraps.exe")))
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "UseExpressionEncoderDX11", "Fraps has been known to crash D3D11. Please use Microsoft Expression Encoder instead for capturing."));
		}
		LoadedRHIModuleName = D3D11RHIModuleName;
		FApp::SetGraphicsRHI(TEXT("DirectX 11"));
	}
	return DynamicRHIModule;
}

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = nullptr;

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if (!FPlatformMisc::IsDebuggerPresent())
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("AttachDebugger")))
		{
			// Wait to attach debugger
			do
			{
				FPlatformProcess::Sleep(0);
			}
			while (!FPlatformMisc::IsDebuggerPresent());
		}
	}
#endif

	ERHIFeatureLevel::Type RequestedFeatureLevel;
	const TCHAR* LoadedRHIModuleName;
	IDynamicRHIModule* DynamicRHIModule = LoadDynamicRHIModule(RequestedFeatureLevel, LoadedRHIModuleName);

	if (DynamicRHIModule)
	{
		// Create the dynamic RHI.
		DynamicRHI = DynamicRHIModule->CreateRHI(RequestedFeatureLevel);
		GLoadedRHIModuleName = LoadedRHIModuleName;
	}

	return DynamicRHI;
}

const TCHAR* GetSelectedDynamicRHIModuleName(bool bCleanup)
{
	check(FApp::CanEverRender());
	if (GDynamicRHI)
	{
		check(!!GLoadedRHIModuleName);
		return GLoadedRHIModuleName;
	}
	else
	{
		ERHIFeatureLevel::Type DesiredFeatureLevel;
		const TCHAR* RHIModuleName;
		IDynamicRHIModule* DynamicRHIModule = LoadDynamicRHIModule(DesiredFeatureLevel, RHIModuleName);
		check(DynamicRHIModule);
		check(RHIModuleName);
		if (bCleanup)
		{
			FModuleManager::Get().UnloadModule(RHIModuleName);
		}
		return RHIModuleName;
	}
}
