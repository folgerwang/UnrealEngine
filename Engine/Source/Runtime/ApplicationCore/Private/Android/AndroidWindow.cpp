// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidWindow.h"
#if USE_ANDROID_JNI
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#endif
#include "HAL/OutputDevices.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformStackWalk.h"

// Cached calculated screen resolution
static int32 WindowWidth = -1;
static int32 WindowHeight = -1;
static int32 GSurfaceViewWidth = -1;
static int32 GSurfaceViewHeight = -1;
static bool WindowInit = false;
static float ContentScaleFactor = -1.0f;
static ANativeWindow* LastWindow = NULL;
static bool bLastMosaicState = false;

void* FAndroidWindow::NativeWindow = NULL;

FAndroidWindow::~FAndroidWindow()
{
	//       Use NativeWindow_Destroy() instead.
}

TSharedRef<FAndroidWindow> FAndroidWindow::Make()
{
	return MakeShareable( new FAndroidWindow() );
}

FAndroidWindow::FAndroidWindow() :Window(NULL)
{
}

void FAndroidWindow::Initialize( class FAndroidApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FAndroidWindow >& InParent, const bool bShowImmediately )
{
	//set window here.

	OwningApplication = Application;
	Definition = InDefinition;
	Window = static_cast<ANativeWindow*>(FAndroidWindow::GetHardwareWindow());
}

bool FAndroidWindow::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	FPlatformRect ScreenRect = GetScreenRect();

	X = ScreenRect.Left;
	Y = ScreenRect.Top;
	Width = ScreenRect.Right - ScreenRect.Left;
	Height = ScreenRect.Bottom - ScreenRect.Top;

	return true;
}


void FAndroidWindow::SetOSWindowHandle(void* InWindow)
{
	Window = static_cast<ANativeWindow*>(InWindow);
}


//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeSetObbInfo(String PackageName, int Version, int PatchVersion);"
static bool GAndroidIsPortrait = false;
static int GAndroidDepthBufferPreference = 0;
#if USE_ANDROID_JNI
JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeSetWindowInfo(JNIEnv* jenv, jobject thiz, jboolean bIsPortrait, jint DepthBufferPreference)
{
	WindowInit = false;
	GAndroidIsPortrait = bIsPortrait == JNI_TRUE;
	GAndroidDepthBufferPreference = DepthBufferPreference;
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("App is running in %s\n"), GAndroidIsPortrait ? TEXT("Portrait") : TEXT("Landscape"));
}

JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeSetSurfaceViewInfo(JNIEnv* jenv, jobject thiz, jint width, jint height)
{
	GSurfaceViewWidth = width;
	GSurfaceViewHeight = height;
	UE_LOG(LogAndroid, Log, TEXT("nativeSetSurfaceViewInfo width=%d and height=%d"), GSurfaceViewWidth, GSurfaceViewHeight);
}
#endif

int32 FAndroidWindow::GetDepthBufferPreference()
{
	return GAndroidDepthBufferPreference;
}

void FAndroidWindow::InvalidateCachedScreenRect()
{
	WindowInit = false;
}

void FAndroidWindow::AcquireWindowRef(ANativeWindow* InWindow)
{
#if USE_ANDROID_JNI
	ANativeWindow_acquire(InWindow);
#endif
}

void FAndroidWindow::ReleaseWindowRef(ANativeWindow* InWindow)
{
#if USE_ANDROID_JNI
	ANativeWindow_release(InWindow);
#endif
}

void FAndroidWindow::SetHardwareWindow(void* InWindow)
{
	NativeWindow = InWindow; //using raw native window handle for now. Could be changed to use AndroidWindow later if needed
}

void* FAndroidWindow::GetHardwareWindow()
{
	return NativeWindow;
}

#if USE_ANDROID_JNI
extern bool AndroidThunkCpp_IsGearVRApplication();
#endif
FPlatformRect FAndroidWindow::GetScreenRect()
{
	int32 OverrideResX, OverrideResY;
	// allow a subplatform to dictate resolution - we can't easily subclass FAndroidWindow the way its used
	if (FPlatformMisc::GetOverrideResolution(OverrideResX, OverrideResY))
	{
		FPlatformRect Rect;
		Rect.Left = Rect.Top = 0;
		Rect.Right = OverrideResX;
		Rect.Bottom = OverrideResY;

		return Rect;
	}

	// too much of the following code needs JNI things, just assume override
#if !USE_ANDROID_JNI

	UE_LOG(LogAndroid, Fatal, TEXT("FAndroidWindow::CalculateSurfaceSize currently expedcts non-JNI platforms to override resolution"));
	return FPlatformRect();
#else

	// CSF is a multiplier to 1280x720
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor"));
	static const bool bIsGearVRApp = AndroidThunkCpp_IsGearVRApplication();
	// If the app is for Gear VR then always use 0 as ScaleFactor (to match window size).
	float RequestedContentScaleFactor = (!bIsGearVRApp) ? CVar->GetFloat() : 0;

	FString CmdLineCSF;
	if (FParse::Value(FCommandLine::Get(), TEXT("mcsf="), CmdLineCSF, false))
	{
		RequestedContentScaleFactor = FCString::Atof(*CmdLineCSF);
	}
	
	ANativeWindow* Window = (ANativeWindow*)FAndroidWindow::GetHardwareWindow();
	static const bool bIsDaydreamApp = FAndroidMisc::IsDaydreamApplication();
	if (bIsDaydreamApp && Window == NULL)
	{
		// Sleep if the hardware window isn't currently available.
		// The Window may not exist if the activity is pausing/resuming, in which case we make this thread wait
		// This case will come up frequently as a result of the DON flow in Gvr.
		// Until the app is fully resumed. It would be nicer if this code respected the lifecycle events
		// of an android app instead, but all of those events are handled on a separate thread and it would require
		// significant re-architecturing to do.
		FPlatformMisc::LowLevelOutputDebugString(TEXT("Waiting for Native window in FAndroidWindow::GetScreenRect"));
		while (Window == NULL)
		{
			FPlatformProcess::Sleep(0.001f);
			Window = (ANativeWindow*)FAndroidWindow::GetHardwareWindow();
		}
	}
//	check(Window != NULL);
	if (Window == NULL)
	{
		FPlatformRect ScreenRect;
		ScreenRect.Left = 0;
		ScreenRect.Top = 0;
		ScreenRect.Right = GAndroidIsPortrait ? 720 : 1280;
		ScreenRect.Bottom = GAndroidIsPortrait ? 1280 : 720;

		UE_LOG(LogAndroid, Log, TEXT("FAndroidWindow::GetScreenRect: Window was NULL, returned default resolution: %d x %d"), ScreenRect.Right, ScreenRect.Bottom);

		return ScreenRect;
	}

	// determine mosaic requirements:
	static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	const bool bMobileHDR = (MobileHDRCvar && MobileHDRCvar->GetValueOnAnyThread() == 1);

	static auto* MobileHDR32bppModeCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR32bppMode"));
	const int32 MobileHDR32Mode = MobileHDR32bppModeCvar->GetValueOnAnyThread();

	bool bMosaicEnabled = false;
	bool bHDR32ModeOverridden = false;
	bool bDeviceRequiresHDR32bpp = false;
	bool bDeviceRequiresMosaic = false;
	if (!bIsGearVRApp && !bIsDaydreamApp)
	{
		bDeviceRequiresHDR32bpp = !FAndroidMisc::SupportsFloatingPointRenderTargets();
		bDeviceRequiresMosaic = bDeviceRequiresHDR32bpp && !FAndroidMisc::SupportsShaderFramebufferFetch();

		bHDR32ModeOverridden = MobileHDR32Mode != 0;
		bMosaicEnabled = bDeviceRequiresMosaic && (!bHDR32ModeOverridden || MobileHDR32Mode == 1);
	}

	bool bUseResCache = WindowInit;

	if (bLastMosaicState != bMosaicEnabled)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** Mosaic State change (to %s), not using res cache"), bMosaicEnabled ? TEXT("enabled") : TEXT("disabled"));
		bUseResCache = false;
	}
	
	if (RequestedContentScaleFactor != ContentScaleFactor)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** RequestedContentScaleFactor different %f != %f, not using res cache"), RequestedContentScaleFactor, ContentScaleFactor);
		bUseResCache = false;
	}

	if (Window != LastWindow)
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("***** Window different, not using res cache"));
		bUseResCache = false;
	}

	if (WindowWidth <= 8)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** WindowWidth is %d, not using res cache"), WindowWidth);
		bUseResCache = false;
	}

	// since orientation won't change on Android, use cached results if still valid
	if (bUseResCache)
	{
		FPlatformRect ScreenRect;
		ScreenRect.Left = 0;
		ScreenRect.Top = 0;
		ScreenRect.Right = WindowWidth;
		ScreenRect.Bottom = WindowHeight;

		return ScreenRect;
	}

	// currently hardcoding resolution

	// get the aspect ratio of the physical screen
	int32 ScreenWidth, ScreenHeight;
	CalculateSurfaceSize(Window, ScreenWidth, ScreenHeight);
	float AspectRatio = (float)ScreenWidth / (float)ScreenHeight;

	int32 MaxWidth = ScreenWidth; 
	int32 MaxHeight = ScreenHeight;


	UE_LOG(LogAndroid, Log, TEXT("Mobile HDR: %s"), bMobileHDR ? TEXT("YES") : TEXT("no"));
	if (bMobileHDR && !bIsGearVRApp)
	{
		UE_LOG(LogAndroid, Log, TEXT("Device requires 32BPP mode : %s"), bDeviceRequiresHDR32bpp ? TEXT("YES") : TEXT("no"));
		UE_LOG(LogAndroid, Log, TEXT("Device requires mosaic: %s"), bDeviceRequiresMosaic ? TEXT("YES") : TEXT("no"));

		if(bHDR32ModeOverridden)
		{
			UE_LOG(LogAndroid, Log, TEXT("--- Enabling 32 BPP override with 'r.MobileHDR32bppMode' = %d"), MobileHDR32Mode);
			UE_LOG(LogAndroid, Log, TEXT("  32BPP mode : YES"));
			UE_LOG(LogAndroid, Log, TEXT("  32BPP mode requires mosaic: %s"), bMosaicEnabled ? TEXT("YES") : TEXT("no"));
			UE_LOG(LogAndroid, Log, TEXT("  32BPP mode requires RGBE: %s"), MobileHDR32Mode == 2 ? TEXT("YES") : TEXT("no"));
		}

		if(bMosaicEnabled)
		{
			UE_LOG(LogAndroid, Log, TEXT("Using mosaic rendering due to lack of Framebuffer Fetch support."));
			if (!FAndroidMisc::SupportsES30())
			{
				const int32 OldMaxWidth = MaxWidth;
				const int32 OldMaxHeight = MaxHeight;

				if (GAndroidIsPortrait)
				{
					MaxHeight = FPlatformMath::Min(MaxHeight, 1024);
					MaxWidth = (MaxHeight * AspectRatio + 0.5f);
				}
				else
				{
					MaxWidth = FPlatformMath::Min(MaxWidth, 1024);
					MaxHeight = (MaxWidth / AspectRatio + 0.5f);
				}

				// ensure Width and Height is multiple of 8
				MaxWidth = (MaxWidth / 8) * 8; 
				MaxHeight = (MaxHeight / 8) * 8;

				UE_LOG(LogAndroid, Log, TEXT("Limiting MaxWidth=%d and MaxHeight=%d due to mosaic rendering on ES2 device (was %dx%d)"), MaxWidth, MaxHeight, OldMaxWidth, OldMaxHeight);
			}
		}
	}

	// 0 means to use native size
	int32 Width, Height;
	if (RequestedContentScaleFactor == 0.0f)
	{
		Width = MaxWidth;
		Height = MaxHeight;
		UE_LOG(LogAndroid, Log, TEXT("Setting Width=%d and Height=%d (requested scale = 0 = auto)"), Width, Height);
	}
	else
	{
		if (GAndroidIsPortrait)
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
		Width = FPlatformMath::Min(Width, MaxWidth);
		Height = FPlatformMath::Min(Height, MaxHeight);

		UE_LOG(LogAndroid, Log, TEXT("Setting Width=%d and Height=%d (requested scale = %f)"), Width, Height, RequestedContentScaleFactor);
	}

	FPlatformRect ScreenRect;
	ScreenRect.Left = 0;
	ScreenRect.Top = 0;
	ScreenRect.Right = Width;
	ScreenRect.Bottom = Height;

	// save for future calls
	WindowWidth = Width;
	WindowHeight = Height;
	WindowInit = true;
	ContentScaleFactor = RequestedContentScaleFactor;
	LastWindow = Window;
	bLastMosaicState = bMosaicEnabled;

	return ScreenRect;
#endif
}


void FAndroidWindow::CalculateSurfaceSize(void* InWindow, int32_t& SurfaceWidth, int32_t& SurfaceHeight)
{
	// allow a subplatform to dictate resolution - we can't easily subclass FAndroidWindow the way its used
	if (FPlatformMisc::GetOverrideResolution(SurfaceWidth, SurfaceHeight))
	{
		return;
	}

	// too much of the following code needs JNI things, just assume override
#if !USE_ANDROID_JNI
	
	UE_LOG(LogAndroid, Fatal, TEXT("FAndroidWindow::CalculateSurfaceSize currently expedcts non-JNI platforms to override resolution"));

#else

	static const bool bIsMobileVRApp = AndroidThunkCpp_IsGearVRApplication() || FAndroidMisc::IsDaydreamApplication();

	ANativeWindow* Window = (ANativeWindow*)InWindow;

	if (InWindow == nullptr)
	{
		// log the issue and callstack for backtracking the issue
		// dump the stack HERE
		{
			const SIZE_T StackTraceSize = 65535;
			ANSICHAR* StackTrace = (ANSICHAR*)FMemory::Malloc(StackTraceSize);
			StackTrace[0] = 0;

			// Walk the stack and dump it to the allocated memory.
			FPlatformStackWalk::StackWalkAndDump(StackTrace, StackTraceSize, 0, NULL);

			FPlatformMisc::LowLevelOutputDebugString(TEXT("== WARNNG: CalculateSurfaceSize called with NULL window:"));

			ANSICHAR* Start = StackTrace;
			ANSICHAR* Next = StackTrace;
			FPlatformMisc::LowLevelOutputDebugString(TEXT("==> STACK TRACE"));
			while (*Next)
			{
				while (*Next)
				{
					if (*Next == 10 || *Next == 13)
					{
						while (*Next == 10 || *Next == 13)
						{
							*Next++ = 0;
						}
						break;
					}
					++Next;
				}
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("==> %s"), ANSI_TO_TCHAR(Start));
				Start = Next;
			}
			FPlatformMisc::LowLevelOutputDebugString(TEXT("<== STACK TRACE"));

			FMemory::Free(StackTrace);
		}

		SurfaceWidth = (GSurfaceViewWidth > 0) ? GSurfaceViewWidth : 1280;
		SurfaceHeight = (GSurfaceViewHeight > 0) ? GSurfaceViewHeight : 720;
	}
	else
	{
		SurfaceWidth = (GSurfaceViewWidth > 0) ? GSurfaceViewWidth : ANativeWindow_getWidth(Window);
		SurfaceHeight = (GSurfaceViewHeight > 0) ? GSurfaceViewHeight : ANativeWindow_getHeight(Window);
	}

	// some phones gave it the other way (so, if swap if the app is landscape, but width < height)
	if ((GAndroidIsPortrait && SurfaceWidth > SurfaceHeight) || 
		(!GAndroidIsPortrait && SurfaceWidth < SurfaceHeight))
	{
		Swap(SurfaceWidth, SurfaceHeight);
	}

	// ensure the size is divisible by a specified amount
	// do not convert to a surface size that is larger than native resolution
	// Mobile VR doesn�t need buffer quantization as UE4 never renders directly to the buffer in VR mode. 
	const int DividableBy = bIsMobileVRApp ? 1 : 8;
	SurfaceWidth = (SurfaceWidth / DividableBy) * DividableBy;
	SurfaceHeight = (SurfaceHeight / DividableBy) * DividableBy;
#endif
}

bool FAndroidWindow::OnWindowOrientationChanged(bool bIsPortrait)
{
	if (GAndroidIsPortrait != bIsPortrait)
	{
		UE_LOG(LogAndroid, Log, TEXT("Window orientation changed: %s"), bIsPortrait ? TEXT("Portrait") : TEXT("Landscape"));
		GAndroidIsPortrait = bIsPortrait;
		return true;
	}
	return false;
}
