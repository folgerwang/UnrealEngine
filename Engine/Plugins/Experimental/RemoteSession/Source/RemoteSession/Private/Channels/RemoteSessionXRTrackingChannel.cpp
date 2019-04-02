// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Channels/RemoteSessionXRTrackingChannel.h"
#include "RemoteSession.h"
#include "Framework/Application/SlateApplication.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCConnection.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCMessage.h"
#include "ARSessionConfig.h"
#include "ARBlueprintLibrary.h"
#include "MessageHandler/Messages.h"
#include "Engine/Engine.h"
#include "Async/Async.h"

bool FXRTrackingProxy::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		static const int32 DeviceId = IXRTrackingSystem::HMDDeviceId;
		OutDevices.Add(DeviceId);
		return true;
	}
	return false;
}

bool FXRTrackingProxy::GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition)
{
	OutOrientation = CachedTrackingToWorld.GetRotation();
	OutPosition = CachedTrackingToWorld.GetLocation();
	return true;
}

FName FXRTrackingProxy::GetSystemName() const
{
	static const FName RemoteSessionXRTrackingProxyName(TEXT("RemoteSessionXRTrackingProxy"));
	return RemoteSessionXRTrackingProxyName;
}

#define MESSAGE_ADDRESS TEXT("/XRTracking")

FRemoteSessionXRTrackingChannel::FRemoteSessionXRTrackingChannel(ERemoteSessionChannelMode InRole, TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> InConnection)
	: IRemoteSessionChannel(InRole, InConnection)
	, Connection(InConnection)
	, Role(InRole)
{
	// If we are sending, we grab the data from GEngine->XRSystem, otherwise we back the current one up for restore later
	XRSystem = GEngine->XRSystem;
	
	if (Role == ERemoteSessionChannelMode::Read)
	{
		// Make the proxy and set GEngine->XRSystem to it
		ProxyXRSystem = MakeShared<FXRTrackingProxy, ESPMode::ThreadSafe>();
		GEngine->XRSystem = ProxyXRSystem;
		
		auto Delegate = FBackChannelDispatchDelegate::FDelegate::CreateRaw(this, &FRemoteSessionXRTrackingChannel::ReceiveXRTracking);
		MessageCallbackHandle = Connection->AddMessageHandler(MESSAGE_ADDRESS, Delegate);
		Connection->SetMessageOptions(MESSAGE_ADDRESS, 1);
	}
    else
    {
#if PLATFORM_IOS
		if (UARBlueprintLibrary::GetARSessionStatus().Status != EARSessionStatus::Running)
		{
			UARSessionConfig* Config = NewObject<UARSessionConfig>();
			UARBlueprintLibrary::StartARSession(Config);
		}
#endif
    }
}

FRemoteSessionXRTrackingChannel::~FRemoteSessionXRTrackingChannel()
{
	if (Role == ERemoteSessionChannelMode::Read)
	{
		// Remove the callback so it doesn't call back on an invalid this
		Connection->RemoveMessageHandler(MESSAGE_ADDRESS, MessageCallbackHandle);

        if (GEngine != nullptr)
        {
            // Reset the engine back to what it was before
            GEngine->XRSystem = XRSystem;
        }
	}
	// Release our xr trackers
	XRSystem = nullptr;
	ProxyXRSystem = nullptr;
}

void FRemoteSessionXRTrackingChannel::Tick(const float InDeltaTime)
{
	// Inbound data gets handled as callbacks
	if (Role == ERemoteSessionChannelMode::Write)
	{
		SendXRTracking();
	}
}

void FRemoteSessionXRTrackingChannel::SendXRTracking()
{
	if (Connection.IsValid())
    {
        if (XRSystem.IsValid() && XRSystem->IsTracking(IXRTrackingSystem::HMDDeviceId))
        {
            FVector Location;
            FQuat Orientation;
            if (XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, Orientation, Location))
            {
                FRotator Rotation(Orientation);

                TwoParamMsg<FVector, FRotator> MsgParam(Location, Rotation);
                FBackChannelOSCMessage Msg(MESSAGE_ADDRESS);
                Msg.Write(MsgParam.AsData());

                Connection->SendPacket(Msg);
                
                UE_LOG(LogRemoteSession, Verbose, TEXT("Sent Rotation (%.02f,%.02f,%.02f)"), Rotation.Pitch,Rotation.Yaw,Rotation.Roll);
            }
            else
            {
                 UE_LOG(LogRemoteSession, Warning, TEXT("Failed to get XRPose"));
            }
        }
        else
        {
            UE_LOG(LogRemoteSession, Warning, TEXT("XR Tracking not available to send"));
        }
    }
}

void FRemoteSessionXRTrackingChannel::ReceiveXRTracking(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch)
{
	if (!ProxyXRSystem.IsValid())
	{
        UE_LOG(LogRemoteSession, Warning, TEXT("XRProxy is invalid. Cannot receive pose"));
		return;
	}

	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> DataCopy = MakeShareable(new TArray<uint8>());
    TSharedPtr<IXRTrackingSystem, ESPMode::ThreadSafe> TaskXRSystem = ProxyXRSystem;

	Message << *DataCopy;

    AsyncTask(ENamedThreads::GameThread, [TaskXRSystem, DataCopy]
	{
		FMemoryReader Ar(*DataCopy);
		TwoParamMsg<FVector, FRotator> MsgParam(Ar);
        
        UE_LOG(LogRemoteSession, Verbose, TEXT("Received Rotation (%.02f,%.02f,%.02f)"), MsgParam.Param2.Pitch,MsgParam.Param2.Yaw,MsgParam.Param2.Roll);

		FTransform NewTransform(MsgParam.Param2, MsgParam.Param1);
        TaskXRSystem->UpdateTrackingToWorldTransform(NewTransform);
	});
}
