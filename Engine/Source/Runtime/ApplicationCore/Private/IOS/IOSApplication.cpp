// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSApplication.h"
#include "IOS/IOSInputInterface.h"
#include "IOSWindow.h"
#include "Misc/CoreDelegates.h"
#include "IOS/IOSAppDelegate.h"
#include "IInputDeviceModule.h"
#include "GenericPlatform/IInputInterface.h"
#include "IInputDevice.h"
#include "Misc/ScopeLock.h"
#include "HAL/IConsoleManager.h"

FCriticalSection FIOSApplication::CriticalSection;
bool FIOSApplication::bOrientationChanged = false;

FIOSApplication* FIOSApplication::CreateIOSApplication()
{
	return new FIOSApplication();
}

FIOSApplication::FIOSApplication()
	: GenericApplication( NULL )
	, InputInterface( FIOSInputInterface::Create( MessageHandler ) )
	, bHasLoadedInputPlugins(false)
{
	[IOSAppDelegate GetDelegate].IOSApplication = this;
}

void FIOSApplication::InitializeWindow( const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately )
{
	const TSharedRef< FIOSWindow > Window = StaticCastSharedRef< FIOSWindow >( InWindow );
	const TSharedPtr< FIOSWindow > ParentWindow = StaticCastSharedPtr< FIOSWindow >( InParent );

	Windows.Add( Window );
	Window->Initialize( this, InDefinition, ParentWindow, bShowImmediately );
}

void FIOSApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler(InMessageHandler);

	TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>(IInputDeviceModule::GetModularFeatureName());
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetMessageHandler(InMessageHandler);
	}
}

void FIOSApplication::AddExternalInputDevice(TSharedPtr<IInputDevice> InputDevice)
{
	if (InputDevice.IsValid())
	{
		ExternalInputDevices.Add(InputDevice);
	}
}

void FIOSApplication::PollGameDeviceState( const float TimeDelta )
{
	// initialize any externally-implemented input devices (we delay load initialize the array so any plugins have had time to load)
	if (!bHasLoadedInputPlugins)
	{
		TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>(IInputDeviceModule::GetModularFeatureName());
		for (auto InputPluginIt = PluginImplementations.CreateIterator(); InputPluginIt; ++InputPluginIt)
		{
			TSharedPtr<IInputDevice> Device = (*InputPluginIt)->CreateInputDevice(MessageHandler);
			AddExternalInputDevice(Device);
		}

		bHasLoadedInputPlugins = true;
	}

	// Poll game device state and send new events
	InputInterface->Tick(TimeDelta);
	InputInterface->SendControllerEvents();

	// Poll externally-implemented devices
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->Tick(TimeDelta);
		(*DeviceIt)->SendControllerEvents();
	}

	FScopeLock Lock(&CriticalSection);
	if(bOrientationChanged && Windows.Num() > 0)
	{
		int32 WindowX,WindowY, WindowWidth,WindowHeight;
		Windows[0]->GetFullScreenInfo(WindowX, WindowY, WindowWidth, WindowHeight);

		GenericApplication::GetMessageHandler()->OnSizeChanged(Windows[0],WindowWidth,WindowHeight, false);
		GenericApplication::GetMessageHandler()->OnResizingWindow(Windows[0]);
		CacheDisplayMetrics();
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
		BroadcastDisplayMetricsChanged(DisplayMetrics);
		FCoreDelegates::OnSafeFrameChangedEvent.Broadcast();
		bOrientationChanged = false;
	}
}

FPlatformRect FIOSApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	return FIOSWindow::GetScreenRect();
}

static TAutoConsoleVariable<float> CVarSafeZone_Landscape_Left(TEXT("SafeZone.Landscape.Left"), -1.0f, TEXT("Safe Zone - Landscape - Left"));
static TAutoConsoleVariable<float> CVarSafeZone_Landscape_Top(TEXT("SafeZone.Landscape.Top"), -1.0f, TEXT("Safe Zone - Landscape - Top"));
static TAutoConsoleVariable<float> CVarSafeZone_Landscape_Right(TEXT("SafeZone.Landscape.Right"), -1.0f, TEXT("Safe Zone - Landscape - Right"));
static TAutoConsoleVariable<float> CVarSafeZone_Landscape_Bottom(TEXT("SafeZone.Landscape.Bottom"), -1.0f, TEXT("Safe Zone - Landscape - Bottom"));

#if !PLATFORM_TVOS
UIDeviceOrientation CachedOrientation = UIDeviceOrientationPortrait;
UIEdgeInsets CachedInsets;
#endif

void FDisplayMetrics::GetDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	// Get screen rect
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FIOSWindow::GetScreenRect();
	OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;

	// Total screen size of the primary monitor
	OutDisplayMetrics.PrimaryDisplayWidth = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right - OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left;
	OutDisplayMetrics.PrimaryDisplayHeight = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top;

#if !PLATFORM_TVOS
	if (@available(iOS 11, *))
	{
		const float RequestedContentScaleFactor = [[IOSAppDelegate GetDelegate].IOSView contentScaleFactor];

		//we need to set these according to the orientation
		TAutoConsoleVariable<float>* CVar_Left = nullptr;
		TAutoConsoleVariable<float>* CVar_Top = &CVarSafeZone_Landscape_Top;
		TAutoConsoleVariable<float>* CVar_Right = nullptr;
		TAutoConsoleVariable<float>* CVar_Bottom = &CVarSafeZone_Landscape_Bottom;

		//making an assumption that the "normal" landscape mode is Landscape right
		if (CachedOrientation == UIDeviceOrientationLandscapeLeft)
		{
			CVar_Left = &CVarSafeZone_Landscape_Left;
			CVar_Right = &CVarSafeZone_Landscape_Right;
		}
		else if (CachedOrientation == UIDeviceOrientationLandscapeRight)
		{
			CVar_Left = &CVarSafeZone_Landscape_Right;
			CVar_Right = &CVarSafeZone_Landscape_Left;
		}

		// of the CVars are set, use their values. If not, use what comes from iOS
		const float Inset_Left = (!CVar_Left || CVar_Left->AsVariable()->GetFloat() < 0.0f) ? CachedInsets.left : CVar_Left->AsVariable()->GetFloat();
		const float Inset_Top = (!CVar_Top || CVar_Top->AsVariable()->GetFloat() < 0.0f) ? CachedInsets.top : CVar_Top->AsVariable()->GetFloat();
		const float Inset_Right = (!CVar_Right || CVar_Right->AsVariable()->GetFloat() < 0.0f) ? CachedInsets.right : CVar_Right->AsVariable()->GetFloat();
		const float Inset_Bottom = (!CVar_Bottom || CVar_Bottom->AsVariable()->GetFloat() < 0.0f) ? CachedInsets.bottom : CVar_Bottom->AsVariable()->GetFloat();

		//setup the asymmetrical padding
		OutDisplayMetrics.TitleSafePaddingSize.X = Inset_Left;
		OutDisplayMetrics.TitleSafePaddingSize.Y = Inset_Top;
		OutDisplayMetrics.TitleSafePaddingSize.Z = Inset_Right;
		OutDisplayMetrics.TitleSafePaddingSize.W = Inset_Bottom;

		//scale the thing
		OutDisplayMetrics.TitleSafePaddingSize *= RequestedContentScaleFactor;

		OutDisplayMetrics.ActionSafePaddingSize = OutDisplayMetrics.TitleSafePaddingSize;
	}
	else
#endif
	{
		OutDisplayMetrics.ApplyDefaultSafeZones();
	}
}

void FIOSApplication::CacheDisplayMetrics()
	{

#if !PLATFORM_TVOS
	if (@available(iOS 11, *))
	{
		CachedInsets = [[[[UIApplication sharedApplication] delegate] window] safeAreaInsets];
		CachedOrientation = [[UIDevice currentDevice] orientation];
	}
#endif
}

TSharedRef< FGenericWindow > FIOSApplication::MakeWindow()
{
	return FIOSWindow::Make();
}

#if !PLATFORM_TVOS
void FIOSApplication::OrientationChanged(UIDeviceOrientation orientation)
{
	FScopeLock Lock(&CriticalSection);
	bOrientationChanged = true;
}
#endif

