// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PIEPreviewDevice.h"

#include "HAL/IConsoleManager.h"
#include "CoreGlobals.h"
#include "RHI.h"
#include "Misc/ConfigCacheIni.h"
#include "IDeviceProfileSelectorModule.h"
#include "Modules/ModuleManager.h"
#include "MaterialShaderQualitySettings.h"
#include "Android/AndroidWindowUtils.h"
#include "GenericPlatform/GenericApplication.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SEditorViewport.h"
#include "Widgets/SWindow.h"

static const int32 g_JSON_VALUE_NOT_SET = 0;

FPIEPreviewDevice::FPIEPreviewDevice()
{
	DeviceSpecs = MakeShareable(new FPIEPreviewDeviceSpecifications());
}

void FPIEPreviewDevice::ComputeViewportSize(const bool bClampWindowSize)
{
	int32 ScreenWidth, ScreenHeight;
	ComputeDeviceResolution(ScreenWidth, ScreenHeight);

	FPIEPreviewDeviceBezelViewportRect ViewportRect = DeviceSpecs->BezelProperties.BezelViewportRect;

	if (IsDeviceFlipped())
	{
		Swap(ScreenWidth, ScreenHeight);
		Swap(ViewportRect.X, ViewportRect.Y);
		Swap(ViewportRect.Width, ViewportRect.Height);
	}

	float ScaleX = (float)ScreenWidth / (float)ViewportRect.Width;
	float ScaleY = (float)ScreenHeight / (float)ViewportRect.Height;

	float BezelScaleFactor = 1.0f / DPIScaleFactor;

 	// compute widow size
	WindowWidth = ScreenWidth;
	WindowHeight = ScreenHeight + WindowTitleBarSize * DPIScaleFactor;

	// compute viewport margin
	if (bShowBezel && BezelTexture != nullptr)
	{
		// compute widow size
		WindowWidth += 2.0f * ViewportRect.X * ScaleX;
		WindowHeight += 2.0f * ViewportRect.Y * ScaleY;

		ViewportRect.X = FMath::RoundToInt(ViewportRect.X * BezelScaleFactor);
		ViewportRect.Y = FMath::RoundToInt(ViewportRect.Y * BezelScaleFactor);
		ViewportRect.Width = FMath::RoundToInt(ViewportRect.Width * BezelScaleFactor);
		ViewportRect.Height = FMath::RoundToInt(ViewportRect.Height * BezelScaleFactor);

		ViewportMargin.Left = ViewportRect.X;
		ViewportMargin.Top = ViewportRect.Y;

		int32 BezelWidth = IsDeviceFlipped() ? BezelTexture->GetSizeY() : BezelTexture->GetSizeX();
		int32 BezelHeight = IsDeviceFlipped() ? BezelTexture->GetSizeX() : BezelTexture->GetSizeY();

		BezelWidth = FMath::RoundToInt(BezelWidth * BezelScaleFactor);
		BezelHeight = FMath::RoundToInt(BezelHeight * BezelScaleFactor);

		ViewportMargin.Right = (float)(BezelWidth - ViewportRect.Width - ViewportRect.X);
		ViewportMargin.Bottom = (float)(BezelHeight - ViewportRect.Height - ViewportRect.Y);

		ViewportMargin = ViewportMargin * FMargin(ScaleX, ScaleY);
	}
	else
	{
		ViewportMargin = FMargin(0.0f);
	}

	// if necessary constrain the window inside the desktop boundaries
	if (bClampWindowSize)
	{
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

		auto DesktopWidth = DisplayMetrics.VirtualDisplayRect.Right - DisplayMetrics.VirtualDisplayRect.Left;
		auto DesktopHeight = DisplayMetrics.VirtualDisplayRect.Bottom - DisplayMetrics.VirtualDisplayRect.Top;

		if (WindowWidth > DesktopWidth)
		{
			float ScaleFactor = (float)DesktopWidth / (float)WindowWidth;
			ScaleX *= ScaleFactor;
			ScaleY *= ScaleFactor;

			WindowWidth = DesktopWidth;
			WindowHeight *= ScaleFactor;

			ViewportMargin = ViewportMargin * ScaleFactor;
		}
		if (WindowHeight > DesktopHeight)
		{
			float ScaleFactor = (float)DesktopHeight / (float)WindowHeight;
			ScaleX *= ScaleFactor;
			ScaleY *= ScaleFactor;

			WindowWidth *= ScaleFactor;
			WindowHeight = DesktopHeight;

			ViewportMargin = ViewportMargin * ScaleFactor;
		}
	}
}

void FPIEPreviewDevice::GetDeviceDefaultResolution(int32& Width, int32& Height)
{
	Width = DeviceSpecs->ResolutionX;

	if (DeviceSpecs->ResolutionYImmersiveMode != g_JSON_VALUE_NOT_SET)
	{
		DeviceSpecs->ResolutionY = DeviceSpecs->ResolutionYImmersiveMode;
	}

	Height = DeviceSpecs->ResolutionY;
}

void FPIEPreviewDevice::ComputeDeviceResolution(int32& Width, int32& Height)
{
	GetDeviceDefaultResolution(Width, Height);

	if (!bIgnoreContentScaleFactor)
	{
		switch (DeviceSpecs->DevicePlatform)
		{
			case EPIEPreviewDeviceType::Android:
			{
				ERHIFeatureLevel::Type DeviceFeatureLevel = GetPreviewDeviceFeatureLevel();
				if (DeviceFeatureLevel < ERHIFeatureLevel::ES3_1)
				{
					AndroidWindowUtils::ApplyMosaicRequirements(Width, Height);
				}

				AndroidWindowUtils::ApplyContentScaleFactor(Width, Height);
			}
			break;

			case EPIEPreviewDeviceType::IOS:
			{
				static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor"));
				float RequestedContentScaleFactor = CVar->GetFloat();

				if (FMath::IsNearlyZero(RequestedContentScaleFactor))
				{
					RequestedContentScaleFactor = DeviceSpecs->IOSProperties.NativeScaleFactor;
				}

				Width *= RequestedContentScaleFactor;
				Height *= RequestedContentScaleFactor;
			}
			break;

			default:
				break;
		} //end switch
	}// end if (!bIgnoreContentScaleFactor)

	Width *= ResolutionScaleFactor;
	Height *= ResolutionScaleFactor;
}

void FPIEPreviewDevice::DetermineScreenOrientationRequirements(bool& bNeedPortrait, bool& bNeedLandscape)
{
	switch (DeviceSpecs->DevicePlatform)
	{
		case EPIEPreviewDeviceType::Android:
		{
			FString Orientation;
			GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("Orientation"), Orientation, GEngineIni);
			if (Orientation.ToLower().Equals("portrait") || Orientation.ToLower().Equals("reverseportrait") || Orientation.ToLower().Equals("sensorportrait"))
			{
				bNeedPortrait = true;
			}
			else if (Orientation.ToLower().Equals("landscape") || Orientation.ToLower().Equals("reverselandscape") || Orientation.ToLower().Equals("sensorlandscape"))
			{
				bNeedLandscape = true;
			}
			else if (Orientation.ToLower().Equals("sensor") || Orientation.ToLower().Equals("fullsensor"))
			{
				bNeedPortrait = true;
				bNeedLandscape = true;
			}
			else
			{
				bNeedPortrait = true;
				bNeedLandscape = true;
			}
		}
		break;

		case EPIEPreviewDeviceType::IOS:
		{
			bool bSupportsPortraitOrientation;
			bool bSupportsUpsideDownOrientation;
			bool bSupportsLandscapeLeftOrientation;
			bool bSupportsLandscapeRightOrientation;
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsPortraitOrientation"), bSupportsPortraitOrientation, GEngineIni);
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsUpsideDownOrientation"), bSupportsUpsideDownOrientation, GEngineIni);
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsLandscapeLeftOrientation"), bSupportsLandscapeLeftOrientation, GEngineIni);
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsLandscapeRightOrientation"), bSupportsLandscapeRightOrientation, GEngineIni);
			bNeedPortrait = bSupportsPortraitOrientation || bSupportsUpsideDownOrientation;
			bNeedLandscape = bSupportsLandscapeLeftOrientation || bSupportsLandscapeRightOrientation;
		}
		break;
	}
}

ERHIFeatureLevel::Type FPIEPreviewDevice::GetPreviewDeviceFeatureLevel() const
{
	switch (DeviceSpecs->DevicePlatform)
	{
		case EPIEPreviewDeviceType::Android:
		{
			FString SubVersion;
			// Check for ES3.1+ support from GLVersion, TODO: check other ES31 feature level constraints, see android's PlatformInitOpenGL
			const bool bDeviceSupportsES31 = DeviceSpecs->AndroidProperties.GLVersion.Split(TEXT("OpenGL ES 3."), nullptr, &SubVersion) && FCString::Atoi(*SubVersion) >= 1;

			// check the project's gles support:
			bool bProjectBuiltForES2 = false, bProjectBuiltForES31 = false;
			GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBuildForES31"), bProjectBuiltForES31, GEngineIni);
			GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBuildForES2"), bProjectBuiltForES2, GEngineIni);

			// Android Preview Device is currently expected to work on gles.
			check(bProjectBuiltForES2 || bProjectBuiltForES31);

			// Projects without ES2 support can only expect to run on ES31 devices.
			check(bProjectBuiltForES2 || bDeviceSupportsES31);

			// ES3.1+ devices fallback to ES2 if the project itself doesn't support ES3.1
			return bDeviceSupportsES31 && bProjectBuiltForES31 ? ERHIFeatureLevel::ES3_1 : ERHIFeatureLevel::ES2;
		}
		case EPIEPreviewDeviceType::IOS:
		{
			bool bProjectBuiltForMetal = false, bProjectBuiltForMRTMetal = false;
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bProjectBuiltForMetal, GEngineIni);
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bProjectBuiltForMRTMetal, GEngineIni);

			const bool bDeviceSupportsMetal = DeviceSpecs->IOSProperties.MetalRHIState.MaxTextureDimensions > 0;

			// not supporting preview for MRT metal 
			check(!bProjectBuiltForMRTMetal);

			// at least one of these should be valid!
			check(bProjectBuiltForMetal);

			// if device doesn't support metal the project must have ES2 enabled.
			check(bProjectBuiltForMetal && bDeviceSupportsMetal);

			return ERHIFeatureLevel::ES3_1;
		}
	}

	checkNoEntry();
	return  ERHIFeatureLevel::Num;
}

void FPIEPreviewDevice::SetupDevice(const int32 InWindowTitleBarSize)
{
	WindowTitleBarSize = InWindowTitleBarSize;

	// set initial scale factor
	ResolutionScaleFactor = 0.5f;

	// compute bezel file path
	FString BezelPath = FPaths::EngineContentDir() + TEXT("Editor/PIEPreviewDeviceSpecs/");
	if (DeviceSpecs->DevicePlatform == EPIEPreviewDeviceType::Android)
	{
		BezelPath += TEXT("Android/");
	}
	else if (DeviceSpecs->DevicePlatform == EPIEPreviewDeviceType::IOS)
	{
		BezelPath += TEXT("iOS/");
	}
	BezelPath += DeviceSpecs->BezelProperties.DeviceBezelFile;

	// load the bezel texture
	BezelTexture = FImageUtils::ImportFileAsTexture2D(BezelPath);
	
	// if we have invalid/uninitialized viewport values use the values provided as native device resolution
	FPIEPreviewDeviceBezelViewportRect& ViewportRect = DeviceSpecs->BezelProperties.BezelViewportRect;
	if (BezelTexture == nullptr || ViewportRect.Width == 0 || ViewportRect.Height == 0)
	{
		ViewportRect.X = 0;
		ViewportRect.Y = 0;
		ViewportRect.Width = GetDeviceSpecs()->ResolutionX;
		ViewportRect.Height = GetDeviceSpecs()->ResolutionY;
	}

	// check rotation functionalities
	bool bPortrait = false, bLandscape = false;
	DetermineScreenOrientationRequirements(bPortrait, bLandscape);

	bAllowRotation = (bPortrait && bLandscape);

	// determine current orientation
	bool bSwitchOrientation = bLandscape && DeviceSpecs->ResolutionY > DeviceSpecs->ResolutionX;
	bSwitchOrientation |= !bLandscape && bPortrait && DeviceSpecs->ResolutionX > DeviceSpecs->ResolutionY;
	if (bSwitchOrientation)
	{
		SwitchOrientation(true);
	}
	else
	{
		// finally compute needed window and viewport size
		// the above branch will call it from SwitchOrientation()
		ComputeViewportSize(true);
	}

	ApplyRHIOverrides();
}

void FPIEPreviewDevice::ApplyRHIPrerequisitesOverrides() const
{
	RHISetMobilePreviewFeatureLevel(GetPreviewDeviceFeatureLevel());
}

void FPIEPreviewDevice::ApplyRHIOverrides() const
{
	EShaderPlatform PreviewPlatform = SP_NumPlatforms;
	ERHIFeatureLevel::Type PreviewFeatureLevel = GetPreviewDeviceFeatureLevel();
	FPIERHIOverrideState* RHIOverrideState = nullptr;

	switch (DeviceSpecs->DevicePlatform)
	{
		case EPIEPreviewDeviceType::Android:
		{
			if (PreviewFeatureLevel == ERHIFeatureLevel::ES2)
			{
				PreviewPlatform = SP_OPENGL_ES2_ANDROID;
				RHIOverrideState = &DeviceSpecs->AndroidProperties.GLES2RHIState;
			}
			else
			{
				PreviewPlatform = SP_OPENGL_ES3_1_ANDROID;
				RHIOverrideState = &DeviceSpecs->AndroidProperties.GLES31RHIState;
			}
		}
		break;

		case EPIEPreviewDeviceType::IOS:
		{
			PreviewPlatform = SP_METAL_MACES3_1;
			RHIOverrideState = &DeviceSpecs->IOSProperties.MetalRHIState;
		}
		break;

		default:
		break;
	}

	if (PreviewPlatform != SP_NumPlatforms)
	{
		FName QualityPreviewShaderPlatform = LegacyShaderPlatformToShaderFormat(PreviewPlatform);
		UMaterialShaderQualitySettings::Get()->SetPreviewPlatform(QualityPreviewShaderPlatform);
	}

	if (RHIOverrideState != nullptr)
	{
		GMaxTextureDimensions.SetPreviewOverride(RHIOverrideState->MaxTextureDimensions);
		GMaxShadowDepthBufferSizeX.SetPreviewOverride(RHIOverrideState->MaxShadowDepthBufferSizeX);
		GMaxShadowDepthBufferSizeY.SetPreviewOverride(RHIOverrideState->MaxShadowDepthBufferSizeY);
		GMaxCubeTextureDimensions.SetPreviewOverride(RHIOverrideState->MaxCubeTextureDimensions);
		GRHISupportsInstancing.SetPreviewOverride(RHIOverrideState->SupportsInstancing);
		GSupportsMultipleRenderTargets.SetPreviewOverride(RHIOverrideState->SupportsMultipleRenderTargets);
		GSupportsRenderTargetFormat_PF_FloatRGBA.SetPreviewOverride(RHIOverrideState->SupportsRenderTargetFormat_PF_FloatRGBA);
		GSupportsRenderTargetFormat_PF_G8.SetPreviewOverride(RHIOverrideState->SupportsRenderTargetFormat_PF_G8);
	}
}

FString FPIEPreviewDevice::GetProfile() const
{
	FString Profile;

	switch (DeviceSpecs->DevicePlatform)
	{
		case EPIEPreviewDeviceType::Android:
		{
			IDeviceProfileSelectorModule* AndroidDeviceProfileSelector = FModuleManager::LoadModulePtr<IDeviceProfileSelectorModule>("AndroidDeviceProfileSelector");
			if (AndroidDeviceProfileSelector)
			{
				FPIEAndroidDeviceProperties& AndroidProperties = DeviceSpecs->AndroidProperties;

				TMap<FString, FString> DeviceParameters;
				DeviceParameters.Add("GPUFamily", AndroidProperties.GPUFamily);
				DeviceParameters.Add("GLVersion", AndroidProperties.GLVersion);
				DeviceParameters.Add("VulkanAvailable", AndroidProperties.VulkanAvailable ? "true" : "false");
				DeviceParameters.Add("VulkanVersion", AndroidProperties.VulkanVersion);
				DeviceParameters.Add("AndroidVersion", AndroidProperties.AndroidVersion);
				DeviceParameters.Add("DeviceMake", AndroidProperties.DeviceMake);
				DeviceParameters.Add("DeviceModel", AndroidProperties.DeviceModel);
				DeviceParameters.Add("DeviceBuildNumber", AndroidProperties.DeviceBuildNumber);
				DeviceParameters.Add("UsingHoudini", AndroidProperties.UsingHoudini ? "true" : "false");

				FString PIEProfileName = AndroidDeviceProfileSelector->GetDeviceProfileName(DeviceParameters);
				if (!PIEProfileName.IsEmpty())
				{
					Profile = PIEProfileName;
				}
			}
			break;
		}
		case EPIEPreviewDeviceType::IOS:
		{
			FPIEIOSDeviceProperties& IOSProperties = DeviceSpecs->IOSProperties;
			Profile = IOSProperties.DeviceModel;
			break;
		}
	}

	return Profile;
}

int32 FPIEPreviewDevice::GetWindowClientHeight() const
{
	return WindowHeight - WindowTitleBarSize;
}