// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterInputModule.h"

#include "DisplayClusterInput.h"
#include "Misc/DisplayClusterInputLog.h"

#include "IDisplayCluster.h"


FDisplayClusterInputModule::FDisplayClusterInputModule()
	: bIsSessionStarted(false)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputModule);
}
FDisplayClusterInputModule::~FDisplayClusterInputModule()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputModule);
}


void FDisplayClusterInputModule::StartupModule()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputModule)

	IInputDeviceModule::StartupModule();

	ButtonController.Initialize();
	AnalogController.Initialize();
	TrackerController.Initialize();
	KeyboardController.Initialize();
	
	IDisplayCluster& DisplayCluster = IDisplayCluster::Get();
	DisplayCluster.OnDisplayClusterStartSession().AddRaw(this, &FDisplayClusterInputModule::OnDisplayClusterStartSession);
	DisplayCluster.OnDisplayClusterEndSession().AddRaw(this, &FDisplayClusterInputModule::OnDisplayClusterEndSession);
	DisplayCluster.OnDisplayClusterPreTick().AddRaw(this, &FDisplayClusterInputModule::OnDisplayClusterPreTick);
}

void FDisplayClusterInputModule::ShutdownModule()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputModule);
}


TSharedPtr< class IInputDevice > FDisplayClusterInputModule::CreateInputDevice( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputModule)

	TSharedPtr< FDisplayClusterInput > InputDevice(new FDisplayClusterInput(InMessageHandler, *this));
	DisplayClusterInputDevice = InputDevice;

	return InputDevice;
}

void FDisplayClusterInputModule::SendControllerEvents(const TSharedPtr<FGenericApplicationMessageHandler>& MessageHandler, int UnrealControllerIndex)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputModule);

	const double CurrentTime = FPlatformTime::Seconds();
	
	// Send all changes for button and analog to UE4 core
	ButtonController.UpdateEvents(CurrentTime, MessageHandler.Get(), UnrealControllerIndex);
	AnalogController.UpdateEvents(CurrentTime, MessageHandler.Get(), UnrealControllerIndex);
	KeyboardController.UpdateEvents(CurrentTime, MessageHandler.Get(), UnrealControllerIndex);
	TrackerController.ApplyTrackersChanges();
}

void FDisplayClusterInputModule::UpdateVrpnBindings()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputModule);

	{
		// Set up delayed keyboard reflections
		for (const auto& It : DelayedKeyboardReflects)
		{
			KeyboardController.ReflectKeyboard(It.VrpnDeviceId, It.ReflectMode);
		}

		DelayedKeyboardReflects.Empty();
	}

	{
		// Set up delayed data bindings
		for (auto& It : DelayedBinds)
		{
			if (KeyboardController.HasDevice(It.VrpnDeviceId))
			{
				KeyboardController.BindChannel(It.VrpnDeviceId, It.VrpnChannel, It.BindTargetName);
			}
			else if (ButtonController.HasDevice(It.VrpnDeviceId))
			{
				ButtonController.BindChannel(It.VrpnDeviceId, It.VrpnChannel, It.BindTargetName);
			}
			else if (AnalogController.HasDevice(It.VrpnDeviceId))
			{
				AnalogController.BindChannel(It.VrpnDeviceId, It.VrpnChannel, It.BindTargetName);
			}
			else if (TrackerController.HasDevice(It.VrpnDeviceId))
			{
				TrackerController.BindTracker(It.VrpnDeviceId, It.VrpnChannel, It.BindTargetName);
			}
		}

		DelayedBinds.Empty();
	}
}

bool FDisplayClusterInputModule::BindVrpnChannel(const FString& VrpnDeviceId, uint32 VrpnChannel, const FString& BindTargetName)
{
	if (IsSessionStarted())
	{
		DelayedBinds.Add(VrpnChannelBind(VrpnDeviceId, VrpnChannel, BindTargetName));
		return true;
	}

	return false;
}

// Bind all keyboard keys to ue4 (default keyboard and|or nDisplay second keyboard namespaces)
bool FDisplayClusterInputModule::SetVrpnKeyboardReflectionMode(const FString& VrpnDeviceId, EDisplayClusterInputKeyboardReflectMode ReflectMode)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputModule);

	if (IsSessionStarted())
	{
		if (KeyboardController.HasDevice(VrpnDeviceId))
		{
			DelayedKeyboardReflects.Add(VrpnKeyboardReflect(VrpnDeviceId, ReflectMode));
			return true;
		}
	}

	return false;
}

void FDisplayClusterInputModule::OnDisplayClusterStartSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputModule);

	bIsSessionStarted = true;

	for (auto& Controller : Controllers)
	{
		Controller->ProcessStartSession();
	}
}

void FDisplayClusterInputModule::OnDisplayClusterEndSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputModule);

	bIsSessionStarted = false;

	for (auto& Controller : Controllers)
	{
		Controller->ProcessEndSession();
	}
}

void FDisplayClusterInputModule::OnDisplayClusterPreTick()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputModule);
	
	if (IsSessionStarted())
	{
		for (auto& Controller : Controllers)
		{
			Controller->ProcessPreTick();
		}
	}
}

IMPLEMENT_MODULE( FDisplayClusterInputModule, DisplayClusterInput)
