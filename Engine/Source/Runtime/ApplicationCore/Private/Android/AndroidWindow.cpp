// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidWindow.h"
#include "Android/AndroidWindowUtils.h"
#include "Android/AndroidEventManager.h"

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

void* FAndroidWindow::WaitForHardwareWindow()
{
	// Sleep if the hardware window isn't currently available.
	// The Window may not exist if the activity is pausing/resuming, in which case we make this thread wait
	// This case will come up frequently as a result of the DON flow in Gvr.
	// Until the app is fully resumed. It would be nicer if this code respected the lifecycle events
	// of an android app instead, but all of those events are handled on a separate thread and it would require
	// significant re-architecturing to do.

	// Before sleeping, we peek into the event manager queue to see if it contains an ON_DESTROY event, 
	// in which case, we exit the loop to allow the application to exit before a window has been created.
	// For instance when the user aborts the "Place your phone into thr Daydream headset." screen.
	// It is not sufficient to check the GIsRequestingExit global variable, as the handler reacting to the APP_EVENT_STATE_ON_DESTROY
	// may be running in the same thread as this method and therefore lead to a deadlock.

	void* Window = GetHardwareWindow();
	while (Window == nullptr)
	{
#if USE_ANDROID_EVENTS
		if (GIsRequestingExit || FAppEventManager::GetInstance()->WaitForEventInQueue(EAppEventState::APP_EVENT_STATE_ON_DESTROY, 0.0f))
		{
			// Application is shutting down soon, abort the wait and return nullptr
			return nullptr;
		}
#endif
		FPlatformProcess::Sleep(0.001f);
		Window = GetHardwareWindow();
	}
	return Window;
}

#if USE_ANDROID_JNI
extern bool AndroidThunkCpp_IsGearVRApplication();
#endif

bool FAndroidWindow::IsCachedRectValid(const bool bMosaicEnabled, const float RequestedContentScaleFactor, ANativeWindow* Window)
{
	if (!WindowInit)
	{
		return false;
	}

	bool bValidCache = true;

	if (bLastMosaicState != bMosaicEnabled)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** Mosaic State change (to %s), not using res cache"), bMosaicEnabled ? TEXT("enabled") : TEXT("disabled"));
		bValidCache = false;
	}

	if (RequestedContentScaleFactor != ContentScaleFactor)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** RequestedContentScaleFactor different %f != %f, not using res cache"), RequestedContentScaleFactor, ContentScaleFactor);
		bValidCache = false;
	}

	if (Window != LastWindow)
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("***** Window different, not using res cache"));
		bValidCache = false;
	}

	if (WindowWidth <= 8)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** WindowWidth is %d, not using res cache"), WindowWidth);
		bValidCache = false;
	}

	return bValidCache;
}

void FAndroidWindow::CacheRect(ANativeWindow* Window, const int32 Width, const int32 Height, const float RequestedContentScaleFactor, const bool bMosaicEnabled)
{
	WindowWidth = Width;
	WindowHeight = Height;
	WindowInit = true;
	ContentScaleFactor = RequestedContentScaleFactor;
	LastWindow = Window;
	bLastMosaicState = bMosaicEnabled;
}

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

	static const bool bIsGearVRApp = AndroidThunkCpp_IsGearVRApplication();

	ANativeWindow* Window = (ANativeWindow*)FAndroidWindow::GetHardwareWindow();
	static const bool bIsDaydreamApp = FAndroidMisc::IsDaydreamApplication();
	if (bIsDaydreamApp && Window == NULL)
	{
		// Sleep if the hardware window isn't currently available.
		FPlatformMisc::LowLevelOutputDebugString(TEXT("Waiting for Native window in FAndroidWindow::GetScreenRect"));
		Window = (ANativeWindow*)FAndroidWindow::WaitForHardwareWindow();
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
	const bool bMosaicEnabled = AndroidWindowUtils::ShouldEnableMosaic() && !(bIsGearVRApp || bIsDaydreamApp);

	// CSF is a multiplier to 1280x720
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor"));
	// If the app is for Gear VR then always use 0 as ScaleFactor (to match window size).
	float RequestedContentScaleFactor = bIsGearVRApp ? 0.0f : CVar->GetFloat();

	FString CmdLineCSF;
	if (FParse::Value(FCommandLine::Get(), TEXT("mcsf="), CmdLineCSF, false))
	{
		RequestedContentScaleFactor = FCString::Atof(*CmdLineCSF);
	}

	// since orientation won't change on Android, use cached results if still valid
	bool bComputeRect = !IsCachedRectValid(bMosaicEnabled, RequestedContentScaleFactor, Window);
	if (bComputeRect)
	{
		// currently hardcoding resolution

		// get the aspect ratio of the physical screen
		int32 ScreenWidth, ScreenHeight;
		CalculateSurfaceSize(Window, ScreenWidth, ScreenHeight);

		static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
		const bool bMobileHDR = (MobileHDRCvar && MobileHDRCvar->GetValueOnAnyThread() == 1);
		UE_LOG(LogAndroid, Log, TEXT("Mobile HDR: %s"), bMobileHDR ? TEXT("YES") : TEXT("no"));

		if (!bIsGearVRApp)
		{
			bool bSupportsES30 = FAndroidMisc::SupportsES30();
			if (!bIsDaydreamApp && !bSupportsES30)
			{
				AndroidWindowUtils::ApplyMosaicRequirements(ScreenWidth, ScreenHeight);
			}

			AndroidWindowUtils::ApplyContentScaleFactor(ScreenWidth, ScreenHeight);
		}

		// save for future calls
		CacheRect(Window, ScreenWidth, ScreenHeight, RequestedContentScaleFactor, bMosaicEnabled);
	}

	// create rect and return
	FPlatformRect ScreenRect;
	ScreenRect.Left = 0;
	ScreenRect.Top = 0;
	ScreenRect.Right = WindowWidth;
	ScreenRect.Bottom = WindowHeight;

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
	// Mobile VR doesn't need buffer quantization as UE4 never renders directly to the buffer in VR mode. 
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
