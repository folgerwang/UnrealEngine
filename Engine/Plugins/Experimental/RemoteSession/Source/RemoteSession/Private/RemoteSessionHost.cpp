// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RemoteSessionHost.h"
#include "BackChannel/Transport/IBackChannelTransport.h"
#include "FrameGrabber.h"
#include "Widgets/SViewport.h"
#include "BackChannel/Utils/BackChannelThreadedConnection.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCMessage.h"
#include "Channels/RemoteSessionInputChannel.h"
#include "Channels/RemoteSessionFrameBufferChannel.h"
#include "Engine/GameEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "RemoteSession.h"

#if WITH_EDITOR
	#include "Editor.h"
	#include "Editor/EditorEngine.h"
	#include "ILevelViewport.h"
#endif

namespace RemoteSessionEd
{
	static FAutoConsoleVariable SlateDragDistanceOverride(TEXT("RemoteSessionEd.SlateDragDistanceOverride"), 10.0f, TEXT("How many pixels you need to drag before a drag and drop operation starts in remote app"));
};


FRemoteSessionHost::FRemoteSessionHost(int32 InQuality, int32 InFramerate, const TMap<FString, ERemoteSessionChannelMode>& InSupportedChannels)
{
	HostTCPPort = 0;
	Quality = InQuality;
	Framerate = InFramerate;
    SupportedChannels = InSupportedChannels;
	SavedEditorDragTriggerDistance = FSlateApplication::Get().GetDragTriggerDistance();
	IsListenerConnected = false;
}

FRemoteSessionHost::~FRemoteSessionHost()
{
	// close this manually to force the thread to stop before things start to be 
	// destroyed
	if (Listener.IsValid())
	{
		Listener->Close();
	}

	Close();
}


void FRemoteSessionHost::Close()
{
	FRemoteSessionRole::Close();

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetDragTriggerDistance(SavedEditorDragTriggerDistance);
	}
}

void FRemoteSessionHost::SetScreenSharing(const bool bEnabled)
{
}

void FRemoteSessionHost::SetConsumeInput(const bool bConsume)
{
	/*if (PlaybackMessageHandler.IsValid())
	{
		PlaybackMessageHandler->SetConsumeInput(bConsume);
	}*/
}

bool FRemoteSessionHost::StartListening(const uint16 InPort)
{
	if (Listener.IsValid())
	{
		return false;
	}

	if (IBackChannelTransport* Transport = IBackChannelTransport::Get())
	{
		Listener = Transport->CreateConnection(IBackChannelTransport::TCP);

		if (Listener->Listen(InPort) == false)
		{
			Listener = nullptr;
		}
		HostTCPPort = InPort;
	}

	return Listener.IsValid();
}

void FRemoteSessionHost::OnBindEndpoints()
{
	FRemoteSessionRole::OnBindEndpoints();
}

void FRemoteSessionHost::OnCreateChannels()
{
	FRemoteSessionRole::OnCreateChannels();
	
	ClearChannels();
	
	CreateChannels(SupportedChannels);
    
	IsListenerConnected = true;
	
	TWeakPtr<SWindow> InputWindow;
	TSharedPtr<FSceneViewport> SceneViewport;
	
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				FSlatePlayInEditorInfo* SlatePlayInEditorSession = GEditor->SlatePlayInEditorMap.Find(Context.ContextHandle);
				if (SlatePlayInEditorSession)
				{
					if (SlatePlayInEditorSession->DestinationSlateViewport.IsValid())
					{
						TSharedPtr<ILevelViewport> DestinationLevelViewport = SlatePlayInEditorSession->DestinationSlateViewport.Pin();
						SceneViewport = DestinationLevelViewport->GetSharedActiveViewport();
						InputWindow = FSlateApplication::Get().FindWidgetWindow(DestinationLevelViewport->AsWidget());
					}
					else if (SlatePlayInEditorSession->SlatePlayInEditorWindowViewport.IsValid())
					{
						SceneViewport = SlatePlayInEditorSession->SlatePlayInEditorWindowViewport;
						InputWindow = SlatePlayInEditorSession->SlatePlayInEditorWindow;
					}
				}
			}
		}
		
		SavedEditorDragTriggerDistance = FSlateApplication::Get().GetDragTriggerDistance();
		FSlateApplication::Get().SetDragTriggerDistance(RemoteSessionEd::SlateDragDistanceOverride->GetFloat());
	}
	else
#endif
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		SceneViewport = GameEngine->SceneViewport;
		InputWindow = GameEngine->GameViewportWindow;
	}
	
	// setup framebuffer capture
	TSharedPtr<FRemoteSessionFrameBufferChannel> FBChannel = IRemoteSessionRole::GetChannel<FRemoteSessionFrameBufferChannel>();
	if (FBChannel.IsValid())
	{
		FBChannel->SetCaptureViewport(SceneViewport.ToSharedRef());
		FBChannel->SetCaptureQuality(Quality, Framerate);
	}
	
	// setup input playback
	TSharedPtr<FRemoteSessionInputChannel> InputChannel = IRemoteSessionRole::GetChannel<FRemoteSessionInputChannel>();
	if (InputChannel.IsValid())
	{
		InputChannel->SetPlaybackWindow(InputWindow, SceneViewport);
	}
	
	// now ask the client to start these channels
	FBackChannelOSCMessage Msg(*GetChannelSelectionEndPoint());
	
	// send these across as a name/mode pair
	for (const auto& KP : SupportedChannels)
	{
		ERemoteSessionChannelMode ClientMode = (KP.Value == ERemoteSessionChannelMode::Write) ? ERemoteSessionChannelMode::Read : ERemoteSessionChannelMode::Write;
		Msg.Write(KP.Key);
		Msg.Write((int32)ClientMode);
	}
	
	OSCConnection->SendPacket(Msg);
}


void FRemoteSessionHost::Tick(float DeltaTime)
{
	// non-threaded listener
	if (IsConnected() == false)
	{
		if (Listener.IsValid() && IsListenerConnected)
		{
			Listener->Close();
			Listener = nullptr;

			//reset the host TCP socket
			StartListening(HostTCPPort);
			IsListenerConnected = false;
		}
        
        if (Listener.IsValid())
        {
            Listener->WaitForConnection(0, [this](TSharedRef<IBackChannelConnection> InConnection) {
                Close();
                CreateOSCConnection(InConnection);
                return true;
            });
        }
	}
	
	FRemoteSessionRole::Tick(DeltaTime);
}
