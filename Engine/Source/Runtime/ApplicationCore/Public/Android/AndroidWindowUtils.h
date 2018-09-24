#pragma once

#include "HAL/IConsoleManager.h"
#include "RHI.h"

#if PLATFORM_ANDROID
#include "Android/AndroidMisc.h"
#endif

namespace AndroidWindowUtils
{
	inline bool DeviceRequiresMosaic()
	{
#if PLATFORM_ANDROID
		bool bDeviceRequiresMosaic = !FAndroidMisc::SupportsFloatingPointRenderTargets() && !FAndroidMisc::SupportsShaderFramebufferFetch();
#else
		bool bDeviceRequiresMosaic = !GSupportsRenderTargetFormat_PF_FloatRGBA && !GSupportsShaderFramebufferFetch;
#endif

		return bDeviceRequiresMosaic;
	}

	inline bool ShouldEnableMosaic()
	{
		static auto* MobileHDR32bppModeCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR32bppMode"));
		const int32 MobileHDR32Mode = MobileHDR32bppModeCvar->GetValueOnAnyThread();

		bool bEnableMosaic = DeviceRequiresMosaic();
		bEnableMosaic &= (MobileHDR32Mode == 0 || MobileHDR32Mode == 1);

		return bEnableMosaic;
	}

	static void ApplyMosaicRequirements(int32& InOutScreenWidth, int32& InOutScreenHeight)
	{
		static auto* MobileHDR32bppModeCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR32bppMode"));
		const int32 MobileHDR32Mode = MobileHDR32bppModeCvar->GetValueOnAnyThread();

		bool bDeviceRequiresMosaic = DeviceRequiresMosaic();
		bool bMosaicEnabled = ShouldEnableMosaic();

#if PLATFORM_ANDROID
		bool bDeviceRequiresHDR32bpp = !FAndroidMisc::SupportsFloatingPointRenderTargets();
#else
		bool bDeviceRequiresHDR32bpp = !GSupportsRenderTargetFormat_PF_FloatRGBA;
#endif

		static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
		const bool bMobileHDR = (MobileHDRCvar && MobileHDRCvar->GetValueOnAnyThread() == 1);
		UE_LOG(LogAndroid, Log, TEXT("Mobile HDR: %s"), bMobileHDR ? TEXT("YES") : TEXT("no"));

		if (bMobileHDR)
		{
			UE_LOG(LogAndroid, Log, TEXT("Device requires 32BPP mode : %s"), bDeviceRequiresHDR32bpp ? TEXT("YES") : TEXT("no"));
			UE_LOG(LogAndroid, Log, TEXT("Device requires mosaic: %s"), bDeviceRequiresMosaic ? TEXT("YES") : TEXT("no"));

			if (MobileHDR32Mode != 0)
			{
				UE_LOG(LogAndroid, Log, TEXT("--- Enabling 32 BPP override with 'r.MobileHDR32bppMode' = %d"), MobileHDR32Mode);
				UE_LOG(LogAndroid, Log, TEXT("  32BPP mode : YES"));
				UE_LOG(LogAndroid, Log, TEXT("  32BPP mode requires mosaic: %s"), bMosaicEnabled ? TEXT("YES") : TEXT("no"));
				UE_LOG(LogAndroid, Log, TEXT("  32BPP mode requires RGBE: %s"), MobileHDR32Mode == 2 ? TEXT("YES") : TEXT("no"));
			}

			if (bMosaicEnabled)
			{
				UE_LOG(LogAndroid, Log, TEXT("Using mosaic rendering due to lack of Framebuffer Fetch support."));

				const int32 OldScreenWidth = InOutScreenWidth;
				const int32 OldSceenHeight = InOutScreenHeight;

				const float AspectRatio = (float)InOutScreenWidth / (float)InOutScreenHeight;

				if (InOutScreenHeight > InOutScreenWidth)
				{
					InOutScreenHeight = FPlatformMath::Min(InOutScreenHeight, 1024);
					InOutScreenWidth = (InOutScreenHeight * AspectRatio + 0.5f);
				}
				else
				{
					InOutScreenWidth = FPlatformMath::Min(InOutScreenWidth, 1024);
					InOutScreenHeight = (InOutScreenWidth / AspectRatio + 0.5f);
				}

				// ensure Width and Height is multiple of 8
				InOutScreenWidth = (InOutScreenWidth / 8) * 8;
				InOutScreenHeight = (InOutScreenHeight / 8) * 8;

				UE_LOG(LogAndroid, Log, TEXT("Limiting MaxWidth=%d and MaxHeight=%d due to mosaic rendering on ES2 device (was %dx%d)"), InOutScreenWidth, InOutScreenHeight, OldScreenWidth, OldSceenHeight);
			}
		}
	}

	static void ApplyContentScaleFactor(int32& InOutScreenWidth, int32& InOutScreenHeight)
	{
		const float AspectRatio = (float)InOutScreenWidth / (float)InOutScreenHeight;

		// CSF is a multiplier to 1280x720
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor"));		

		float RequestedContentScaleFactor = CVar->GetFloat();

		FString CmdLineCSF;
		if (FParse::Value(FCommandLine::Get(), TEXT("mcsf="), CmdLineCSF, false))
		{
			RequestedContentScaleFactor = FCString::Atof(*CmdLineCSF);
		}

		// 0 means to use native size
		if (RequestedContentScaleFactor == 0.0f)
		{
			UE_LOG(LogAndroid, Log, TEXT("Setting Width=%d and Height=%d (requested scale = 0 = auto)"), InOutScreenWidth, InOutScreenHeight);
		}
		else
		{
			int32 Width = InOutScreenWidth;
			int32 Height = InOutScreenHeight;

			if (InOutScreenHeight > InOutScreenWidth)
			{
				Height = 1280 * RequestedContentScaleFactor;
			}
			else
			{
				Height = 720 * RequestedContentScaleFactor;
			}

			// apply the aspect ration to get the width
			Width = (Height * AspectRatio + 0.5f);
			// ensure Width and Height is multiple of 8
			Width = (Width / 8) * 8;
			Height = (Height / 8) * 8;

			// clamp to native resolution
			InOutScreenWidth = FPlatformMath::Min(Width, InOutScreenWidth);
			InOutScreenHeight = FPlatformMath::Min(Height, InOutScreenHeight);

			UE_LOG(LogAndroid, Log, TEXT("Setting Width=%d and Height=%d (requested scale = %f)"), InOutScreenWidth, InOutScreenHeight, RequestedContentScaleFactor);
		}
	}

}