// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"

#if WITH_CEF3

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"
#endif
#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
THIRD_PARTY_INCLUDES_START
#if PLATFORM_APPLE
PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif
	#include "include/cef_app.h"
#if PLATFORM_APPLE
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
THIRD_PARTY_INCLUDES_END
#pragma pop_macro("OVERRIDE")
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

/**
 * Implements CEF App and other Process level interfaces
 */
class FCEFBrowserApp : public CefApp,
	public CefBrowserProcessHandler
{
public:

	/**
	 * Default Constructor
	 */
	FCEFBrowserApp();

	/** A delegate this is invoked when an existing browser requests creation of a new browser window. */
	DECLARE_DELEGATE_OneParam(FOnRenderProcessThreadCreated, CefRefPtr<CefListValue> /*ExtraInfo*/);
	virtual FOnRenderProcessThreadCreated& OnRenderProcessThreadCreated()
	{
		return RenderProcessThreadCreatedDelegate;
	}

	/** Used to pump the CEF message loop whenever OnScheduleMessagePumpWork is triggered */
	void TickMessagePump(float DeltaTime, bool bForce);

private:
	// CefApp methods.
	virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
	virtual void OnBeforeCommandLineProcessing(const CefString& ProcessType, CefRefPtr< CefCommandLine > CommandLine) override;
	// CefBrowserProcessHandler methods:
	virtual void OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> CommandLine) override;
	virtual void OnRenderProcessThreadCreated(CefRefPtr<CefListValue> ExtraInfo) override;
#if !PLATFORM_LINUX
	virtual void OnScheduleMessagePumpWork(int64 delay_ms) override;
#endif

	FOnRenderProcessThreadCreated RenderProcessThreadCreatedDelegate;

	// Include the default reference counting implementation.
	IMPLEMENT_REFCOUNTING(FCEFBrowserApp);

	// Lock for access MessagePumpCountdown
	FCriticalSection MessagePumpCountdownCS;
	// Countdown in milliseconds until CefDoMessageLoopWork is called.  Updated by OnScheduleMessagePumpWork
	int64 MessagePumpCountdown;
};
#endif
