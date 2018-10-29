// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CEF/CEFBrowserApp.h"

#if WITH_CEF3

FCEFBrowserApp::FCEFBrowserApp()
	: MessagePumpCountdown(0)
{
}

void FCEFBrowserApp::OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> CommandLine)
{
}

void FCEFBrowserApp::OnBeforeCommandLineProcessing(const CefString& ProcessType, CefRefPtr< CefCommandLine > CommandLine)
{
	CommandLine->AppendSwitch("disable-gpu");
	CommandLine->AppendSwitch("disable-gpu-compositing");
#if !PLATFORM_MAC
	CommandLine->AppendSwitch("enable-begin-frame-scheduling");
#endif
}

void FCEFBrowserApp::OnRenderProcessThreadCreated(CefRefPtr<CefListValue> ExtraInfo)
{
	RenderProcessThreadCreatedDelegate.ExecuteIfBound(ExtraInfo);
}

#if !PLATFORM_LINUX
void FCEFBrowserApp::OnScheduleMessagePumpWork(int64 delay_ms)
{
	FScopeLock Lock(&MessagePumpCountdownCS);

	if (MessagePumpCountdown == -1)
	{
		MessagePumpCountdown = delay_ms;
	}
	else
	{
		// override if we need the pump to happen sooner
		MessagePumpCountdown = FMath::Clamp<int64>(MessagePumpCountdown, 0, delay_ms);
	}
}
#endif

void FCEFBrowserApp::TickMessagePump(float DeltaTime, bool bForce)
{
#if PLATFORM_LINUX
	CefDoMessageLoopWork();
	return;
#endif

	FScopeLock Lock(&MessagePumpCountdownCS);

	bool bPump = false;	
	// count down in order to call message pump
	if (MessagePumpCountdown >= 0)
	{
		MessagePumpCountdown -= DeltaTime * 1000;
		if (MessagePumpCountdown <= 0)
		{
			bPump = true;
		}
	}
	if (bPump || bForce)
	{
		// -1 indicates that no countdown is currently happening
		MessagePumpCountdown = -1;
		CefDoMessageLoopWork();
	}
}

#endif
