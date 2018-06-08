// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
// Copyright 2016 Magic Leap, Inc. All Rights Reserved.

#include "Lumin/LuminPlatformProcess.h"
#include "Lumin/LuminPlatformMisc.h"
#include "Android/AndroidPlatformRunnableThread.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include <sys/syscall.h>
#include <pthread.h>
#include <libgen.h>
#include <dlfcn.h>
#include <ml_dispatch.h>

const TCHAR* FLuminPlatformProcess::ComputerName()
{
	return TEXT("Lumin Device");
}

namespace LuminProcess
{
	static TCHAR * ExecutablePath()
	{
		// @todo Lumin - If later we're generating an so and running that, this probably won't work
		// will need to pull the exe name through some other mechanism
		static TCHAR * CachedResult = nullptr;
		if (CachedResult == nullptr)
		{
			// The common Linux way of using lstat to dynamically discover the length
			// of the symlink name doesn't work on Lumin. As it returns a zero size
			// for the link. [RR]
			char * SelfPath = (char*)FMemory::Malloc(MAX_PATH + 1);
			FMemory::Memzero(SelfPath, MAX_PATH + 1);
			if (readlink("/proc/self/exe", SelfPath, MAX_PATH) == -1)
			{
				int ErrNo = errno;
				UE_LOG(LogHAL, Fatal, TEXT("readlink() failed with errno = %d (%s)"), ErrNo,
					StringCast< TCHAR >(strerror(ErrNo)).Get());
				// unreachable
				return CachedResult;
			}
			CachedResult = (TCHAR*)FMemory::Malloc((FCStringAnsi::Strlen(SelfPath) + 1)*sizeof(TCHAR));
			FCString::Strcpy(CachedResult, MAX_PATH, ANSI_TO_TCHAR(SelfPath));
			FMemory::Free(SelfPath);
		}
		return CachedResult;
	}
}

const TCHAR* FLuminPlatformProcess::UserSettingsDir()
{
	// TODO: Use corekit to obtain the writable location for this.
	const static TCHAR * CachedResult = ApplicationSettingsDir();
	return CachedResult;
}

const TCHAR* FLuminPlatformProcess::ApplicationSettingsDir()
{
	static FString CachedResultString = FLuminPlatformMisc::GetApplicationWritableDirectoryPath();
	return *CachedResultString;
}

const TCHAR* FLuminPlatformProcess::ExecutableName(bool bRemoveExtension)
{
	static TCHAR * CachedResult = nullptr;
	if (CachedResult == nullptr)
	{
		TCHAR * SelfPath = LuminProcess::ExecutablePath();
		if (SelfPath)
		{
			CachedResult = (TCHAR*)FMemory::Malloc((FCString::Strlen(SelfPath) + 1)*sizeof(TCHAR));
			FCString::Strcpy(CachedResult, FCString::Strlen(SelfPath), *FPaths::GetBaseFilename(SelfPath, bRemoveExtension));
		}
	}
	return CachedResult;
}

void FLuminPlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	check(URL);
	const FString URLWithParams = FString::Printf(TEXT("%s %s"), URL, Parms ? Parms : TEXT("")).TrimEnd();

	MLDispatchPacket* Packet = MLDispatchAllocateEmptyPacket();
	if (Packet != nullptr)
	{
		bool bSetUri = MLDispatchSetUri(Packet, TCHAR_TO_UTF8(*URLWithParams));
		if (bSetUri)
		{
			MLDispatchErrorCode ErrorCode = MLDispatchTryOpenApplication(Packet);
			switch(ErrorCode)
			{
				case MLDispatchErrorCode_CannotStartApp:
					UE_LOG(LogCore, Error, TEXT("Failed to launch URL %s - Cannot start app"), *URLWithParams);
					break;
				case MLDispatchErrorCode_InvalidPacket:
					UE_LOG(LogCore, Error, TEXT("Failed to launch URL %s - Invalid packet"), *URLWithParams);
					break;
				case MLDispatchErrorCode_NoAppFound:
					UE_LOG(LogCore, Error, TEXT("Failed to launch URL %s - No app found"), *URLWithParams);
					break;
				case MLDispatchErrorCode_Internal:
					UE_LOG(LogCore, Error, TEXT("Failed to launch URL %s - Internal error"), *URLWithParams);
					break;
				default:
					UE_LOG(LogCore, Display, TEXT("Launching URL %s"), *URLWithParams);					
			}
		}
		else
		{
			UE_LOG(LogCore, Error, TEXT("Failed to launch URL %s - could not set URI in dispatch packet"), *URLWithParams);
		}
		MLDispatchReleasePacket(&Packet, true, false);
	}
	else
	{
		UE_LOG(LogCore, Error, TEXT("Failed to launch URL %s - could not allocate dispatch packet"), *URLWithParams);
	}
}

void* FLuminPlatformProcess::GetDllHandle(const TCHAR* Filename)
{
	check(Filename);
	FString AbsolutePath = FPaths::ConvertRelativePathToFull(Filename);

	int DlOpenMode = RTLD_LAZY;
	DlOpenMode |= RTLD_LOCAL; // Local symbol resolution when loading shared objects - Needed for Hot-Reload

	void *Handle = dlopen(TCHAR_TO_UTF8(*AbsolutePath), DlOpenMode);
	if (!Handle)
	{
		UE_LOG(LogLinux, Warning, TEXT("dlopen failed: %s"), UTF8_TO_TCHAR(dlerror()));
	}
	return Handle;
}

void FLuminPlatformProcess::FreeDllHandle(void* DllHandle)
{
	check(DllHandle);
	dlclose(DllHandle);
}

void* FLuminPlatformProcess::GetDllExport(void* DllHandle, const TCHAR* ProcName)
{
	check(DllHandle);
	check(ProcName);
	return dlsym(DllHandle, TCHAR_TO_ANSI(ProcName));
}

int32 FLuminPlatformProcess::GetDllApiVersion(const TCHAR* Filename)
{
	check(Filename);
	return FEngineVersion::CompatibleWith().GetChangelist();
}

const TCHAR* FLuminPlatformProcess::GetModulePrefix()
{
	return TEXT("lib");
}

const TCHAR* FLuminPlatformProcess::GetModuleExtension()
{
	return TEXT("so");
}

const TCHAR* FLuminPlatformProcess::GetBinariesSubdirectory()
{
	return TEXT(""); //binaries are located in bin/ we no longer have a subdirectory to return.
}
