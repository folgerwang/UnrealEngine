// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RemoteSessionHost.h"
#include "BackChannel/Transport/IBackChannelTransport.h"
#include "FrameGrabber.h"
#include "Widgets/SViewport.h"
#include "BackChannel/Utils/BackChannelThreadedConnection.h"
#include "Channels/RemoteSessionInputChannel.h"
#include "Channels/RemoteSessionXRTrackingChannel.h"
#include "Channels/RemoteSessionFrameBufferChannel.h"
#include "Engine/GameEngine.h"

#if WITH_EDITOR
	#include "Editor.h"
	#include "Editor/EditorEngine.h"
#endif

PRAGMA_DISABLE_OPTIMIZATION



FRemoteSessionHost::FRemoteSessionHost(int32 InQuality, int32 InFramerate)
{
	Quality = InQuality;
	Framerate = InFramerate;
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

		Listener->Listen(InPort);
	}

	return Listener.IsValid();
}


bool FRemoteSessionHost::ProcessIncomingConnection(TSharedRef<IBackChannelConnection> NewConnection)
{
	Close();

	OSCConnection = MakeShareable(new FBackChannelOSCConnection(NewConnection));

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
					if (SlatePlayInEditorSession->SlatePlayInEditorWindowViewport.IsValid())
					{
						SceneViewport = SlatePlayInEditorSession->SlatePlayInEditorWindowViewport;
					}

					InputWindow = SlatePlayInEditorSession->SlatePlayInEditorWindow;
				}
			}
		}
		
	}
	else
#endif
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		SceneViewport = GameEngine->SceneViewport;
		InputWindow = GameEngine->GameViewportWindow;
	}

	TSharedPtr<FRemoteSessionInputChannel> InputChannel = MakeShareable(new FRemoteSessionInputChannel(ERemoteSessionChannelMode::Receive, OSCConnection));
	InputChannel->SetPlaybackWindow(InputWindow, SceneViewport);
	Channels.Add(InputChannel);
	Channels.Add(MakeShareable(new FRemoteSessionXRTrackingChannel(ERemoteSessionChannelMode::Receive, OSCConnection)));

	if (SceneViewport.IsValid())
	{
		TSharedPtr<FRemoteSessionFrameBufferChannel> FramebufferChannel = MakeShareable(new FRemoteSessionFrameBufferChannel(ERemoteSessionChannelMode::Send, OSCConnection));
		FramebufferChannel->SetCaptureViewport(SceneViewport.ToSharedRef());
		FramebufferChannel->SetCaptureQuality(Quality, Framerate);
		Channels.Add(FramebufferChannel);
	}

	OSCConnection->StartReceiveThread();

	return true;
}


void FRemoteSessionHost::Tick(float DeltaTime)
{
	// non-threaded listener
	if (IsConnected() == false)
	{
		Listener->WaitForConnection(0, [this](TSharedRef<IBackChannelConnection> NewConnection) {
			return ProcessIncomingConnection(NewConnection);
		});
	}
	
	FRemoteSessionRole::Tick(DeltaTime);
}



PRAGMA_ENABLE_OPTIMIZATION