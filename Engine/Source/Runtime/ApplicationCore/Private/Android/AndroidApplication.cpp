// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidApplication.h"
#include "Android/AndroidInputInterface.h"
#include "Android/AndroidWindow.h"
#include "Android/AndroidCursor.h"
#include "IInputDeviceModule.h"
#include "HAL/OutputDevices.h"
#include "Misc/AssertionMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogAndroidApplication, Log, All);

bool FAndroidApplication::bWindowSizeChanged = false;

FAndroidApplication* FAndroidApplication::_application = nullptr;

FAndroidApplication* FAndroidApplication::CreateAndroidApplication()
{
	return new FAndroidApplication();
}

FAndroidApplication::FAndroidApplication()
	: GenericApplication(MakeShareable(new FAndroidCursor()))
	, InputInterface( FAndroidInputInterface::Create( MessageHandler, Cursor ) )
	, bHasLoadedInputPlugins(false)
{
	_application = this;
}

FAndroidApplication::FAndroidApplication(TSharedPtr<class FAndroidInputInterface> InInputInterface)
	: GenericApplication((InInputInterface.IsValid() && InInputInterface->GetCursor()) ? InInputInterface->GetCursor() : MakeShareable(new FAndroidCursor()))
	, InputInterface(InInputInterface)
	, bHasLoadedInputPlugins(false)
{
	_application = this;
}

void FAndroidApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler( MessageHandler );
}

void FAndroidApplication::AddExternalInputDevice(TSharedPtr<IInputDevice> InputDevice)
{
	if (InputDevice.IsValid())
	{
		InputInterface->AddExternalInputDevice(InputDevice);
	}
}

void FAndroidApplication::PollGameDeviceState( const float TimeDelta )
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
	InputInterface->Tick( TimeDelta );
	InputInterface->SendControllerEvents();
	
	if (bWindowSizeChanged && 
		Windows.Num() > 0 && 
		FAndroidWindow::GetHardwareWindow() != nullptr)
	{
		int32 WindowX,WindowY, WindowWidth,WindowHeight;
		Windows[0]->GetFullScreenInfo(WindowX, WindowY, WindowWidth, WindowHeight);

		GenericApplication::GetMessageHandler()->OnSizeChanged(Windows[0],WindowWidth,WindowHeight, false);
		GenericApplication::GetMessageHandler()->OnResizingWindow(Windows[0]);
		
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
		BroadcastDisplayMetricsChanged(DisplayMetrics);

		// the cursor needs to compute the proper slate scaling factor each time the display metrics change
		TSharedPtr<FAndroidCursor> AndroidCursor = StaticCastSharedPtr<FAndroidCursor>(Cursor);
		if (AndroidCursor.IsValid())
		{
			AndroidCursor->ComputeUIScaleFactor();
		}

		bWindowSizeChanged = false;
	}
}

FPlatformRect FAndroidApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	return FAndroidWindow::GetScreenRect();
}

IInputInterface* FAndroidApplication::GetInputInterface()
{
	// NOTE: This does not increase the reference count, so don't cache the result
	return InputInterface.Get();
}

void FAndroidApplication::Tick(const float TimeDelta)
{
	//generate event that will end up calling 'QueryCursor' in slate to support proper reporting of the cursor's type.
	MessageHandler->OnCursorSet();
}

bool FAndroidApplication::IsGamepadAttached() const
{
	FAndroidInputInterface* AndroidInputInterface = (FAndroidInputInterface*)InputInterface.Get();

	if (AndroidInputInterface)
	{
		return AndroidInputInterface->IsGamepadAttached();
	}

	return false;
}

void FDisplayMetrics::RebuildDisplayMetrics( FDisplayMetrics& OutDisplayMetrics )
{
	// Get screen rect
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FAndroidWindow::GetScreenRect();
	OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;

	// Total screen size of the primary monitor
	OutDisplayMetrics.PrimaryDisplayWidth = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right - OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left;
	OutDisplayMetrics.PrimaryDisplayHeight = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top;

	// Apply the debug safe zones
	OutDisplayMetrics.ApplyDefaultSafeZones();

	float Inset_Left = -1.0f;
	float Inset_Top = -1.0f;
	float Inset_Right = -1.0f;
	float Inset_Bottom = -1.0f;

	if (FString* SafeZoneLandscape = FAndroidMisc::GetConfigRulesVariable(TEXT("SafeZone_Landscape")))
	{
		TArray<FString> ZoneVector;
		if (SafeZoneLandscape->ParseIntoArray(ZoneVector, TEXT(","), true) == 4)
		{
			Inset_Left = FCString::Atof(*ZoneVector[0]);
			Inset_Top = FCString::Atof(*ZoneVector[1]);
			Inset_Right = FCString::Atof(*ZoneVector[2]);
			Inset_Bottom = FCString::Atof(*ZoneVector[3]);
		}
	}

	OutDisplayMetrics.TitleSafePaddingSize.X = (Inset_Left >= 0.0f) ? Inset_Left : OutDisplayMetrics.TitleSafePaddingSize.X;
	OutDisplayMetrics.TitleSafePaddingSize.Y = (Inset_Top >= 0.0f) ? Inset_Top : OutDisplayMetrics.TitleSafePaddingSize.Y;
	OutDisplayMetrics.TitleSafePaddingSize.Z = (Inset_Right >= 0.0f) ? Inset_Right : OutDisplayMetrics.TitleSafePaddingSize.Z;
	OutDisplayMetrics.TitleSafePaddingSize.W = (Inset_Bottom >= 0.0f) ? Inset_Bottom : OutDisplayMetrics.TitleSafePaddingSize.W;
	OutDisplayMetrics.ActionSafePaddingSize = OutDisplayMetrics.TitleSafePaddingSize;
}

TSharedRef< FGenericWindow > FAndroidApplication::MakeWindow()
{
	return FAndroidWindow::Make();
}

void FAndroidApplication::InitializeWindow( const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately )
{
	const TSharedRef< FAndroidWindow > Window = StaticCastSharedRef< FAndroidWindow >( InWindow );
	const TSharedPtr< FAndroidWindow > ParentWindow = StaticCastSharedPtr< FAndroidWindow >( InParent );

	Windows.Add( Window );
	Window->Initialize( this, InDefinition, ParentWindow, bShowImmediately );
}

void FAndroidApplication::OnWindowSizeChanged()
{
	bWindowSizeChanged = true;
}

