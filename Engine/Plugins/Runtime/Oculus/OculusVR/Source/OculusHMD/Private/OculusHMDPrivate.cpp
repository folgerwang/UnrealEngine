// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OculusHMDPrivate.h"
#include "RHICommandList.h"
#include "RenderingThread.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// Utility functions
//-------------------------------------------------------------------------------------------------

bool InGameThread()
{
	if (GIsGameThreadIdInitialized)
	{
		return FPlatformTLS::GetCurrentThreadId() == GGameThreadId;
	}
	else
	{
		return true;
	}
}


bool InRenderThread()
{
	if (GRenderingThread && !GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed))
	{
		return FPlatformTLS::GetCurrentThreadId() == GRenderingThread->GetThreadID();
	}
	else
	{
		return InGameThread();
	}
}


bool InRHIThread()
{
	if (GRenderingThread && !GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed))
	{
		if (GRHIThreadId)
		{
			if (FPlatformTLS::GetCurrentThreadId() == GRHIThreadId)
			{
				return true;
			}
			
			if (FPlatformTLS::GetCurrentThreadId() == GRenderingThread->GetThreadID())
			{
				return GetImmediateCommandList_ForRenderCommand().Bypass();
			}

			return false;
		}
		else
		{
			return FPlatformTLS::GetCurrentThreadId() == GRenderingThread->GetThreadID();
		}
	}
	else
	{
		return InGameThread();
	}
}

#if OCULUS_HMD_SUPPORTED_PLATFORMS
bool IsOculusServiceRunning()
{
#if PLATFORM_WINDOWS
	HANDLE hEvent = ::OpenEventW(SYNCHRONIZE, 0 /*FALSE*/, L"OculusHMDConnected");

	if (!hEvent)
	{
		return false;
	}

	::CloseHandle(hEvent);
#endif

	return true;
}


bool IsOculusHMDConnected()
{
#if PLATFORM_WINDOWS
	HANDLE hEvent = ::OpenEventW(SYNCHRONIZE, 0 /*FALSE*/, L"OculusHMDConnected");

	if (!hEvent)
	{
		return false;
	}

	uint32 dwWait = ::WaitForSingleObject(hEvent, 0);

	::CloseHandle(hEvent);

	if (WAIT_OBJECT_0 != dwWait)
	{
		return false;
	}
#endif

	return true;
}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS

} // namespace OculusHMD
