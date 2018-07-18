// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidProcess.cpp: Android implementations of Process functions
=============================================================================*/

#include "Android/AndroidProcess.h"
#include "Android/AndroidPlatformRunnableThread.h"
#if !PLATFORM_LUMIN
#include "Android/AndroidAffinity.h"
#endif
#include "Async/TaskGraphInterfaces.h"

#include <sys/syscall.h>
#include <pthread.h>

#include "Android/AndroidJavaEnv.h"

#if !PLATFORM_LUMIN
int64 FAndroidAffinity::GameThreadMask = FPlatformAffinity::GetNoAffinityMask();
int64 FAndroidAffinity::RenderingThreadMask = FPlatformAffinity::GetNoAffinityMask();
#endif

const TCHAR* FAndroidPlatformProcess::ComputerName()
{
	static FString ComputerName;
	if (ComputerName.Len() == 0)
	{
		ComputerName = FAndroidMisc::GetDeviceModel();
	}

	return *ComputerName; 
}

void FAndroidPlatformProcess::SetThreadAffinityMask( uint64 InAffinityMask )
{
	/* 
	 * On Android we prefer not to touch the thread affinity at all unless the user has specifically requested to change it.
	 * The only way to override the default mask is to use the android.DefaultThreadAffinity console command set by ini file or device profile.
	 */
	if (FPlatformAffinity::GetNoAffinityMask() != InAffinityMask)
	{
		pid_t ThreadId = gettid();
		int AffinityMask = (int)InAffinityMask;
		syscall(__NR_sched_setaffinity, ThreadId, sizeof(AffinityMask), &AffinityMask);
	}
}

uint32 FAndroidPlatformProcess::GetCurrentProcessId()
{
	return getpid();
}

const TCHAR* FAndroidPlatformProcess::BaseDir()
{
	return TEXT("");
}

const TCHAR* FAndroidPlatformProcess::ExecutableName(bool bRemoveExtension)
{
#if USE_ANDROID_FILE
	extern FString GAndroidProjectName;
	return *GAndroidProjectName;
#else
	UE_LOG(LogAndroid, Fatal, TEXT("A sub-platform that doesn't use USE_ANDROID_FILE must implement PlatformProcess::ExecutableName"));
	return TEXT("");
#endif
}

FRunnableThread* FAndroidPlatformProcess::CreateRunnableThread()
{
	return new FRunnableThreadAndroid();
}

bool FAndroidPlatformProcess::CanLaunchURL(const TCHAR* URL)
{
	return URL != nullptr;
}

DECLARE_DELEGATE_OneParam(FAndroidLaunchURLDelegate, const FString&);

CORE_API FAndroidLaunchURLDelegate OnAndroidLaunchURL;

void FAndroidPlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	check(URL);
	const FString URLWithParams = FString::Printf(TEXT("%s %s"), URL, Parms ? Parms : TEXT("")).TrimEnd();

	OnAndroidLaunchURL.ExecuteIfBound(URLWithParams);

	if (Error != nullptr)
	{
		*Error = TEXT("");
	}
}

FString FAndroidPlatformProcess::GetGameBundleId()
{
#if USE_ANDROID_JNI
	JNIEnv* JEnv = AndroidJavaEnv::GetJavaEnv();
	if (nullptr != JEnv)
	{
		jclass Class = AndroidJavaEnv::FindJavaClass("com/epicgames/ue4/GameActivity");
		if (nullptr != Class)
		{
			jmethodID getAppPackageNameMethodId = JEnv->GetStaticMethodID(Class, "getAppPackageName", "()Ljava/lang/String;");
			jstring JPackageName = (jstring)JEnv->CallStaticObjectMethod(Class, getAppPackageNameMethodId, nullptr);
			const char * NativePackageNameString = JEnv->GetStringUTFChars(JPackageName, 0);
			FString PackageName = FString(NativePackageNameString);
			JEnv->ReleaseStringUTFChars(JPackageName, NativePackageNameString);
			JEnv->DeleteLocalRef(JPackageName);
			JEnv->DeleteLocalRef(Class);
			return PackageName;
		}
	}
#endif
	return TEXT("");
}

// Can be specified per device profile
// android.DefaultThreadAffinity GT 0x01 RT 0x02
TAutoConsoleVariable<FString> CVarAndroidDefaultThreadAffinity(
	TEXT("android.DefaultThreadAffinity"), 
	TEXT(""), 
	TEXT("Sets the thread affinity for Android platform. Pairs of args [GT|RT] [Hex affinity], ex: android.DefaultThreadAffinity GT 0x01 RT 0x02"));

static void AndroidSetAffinityOnThread()
{
	if (IsInActualRenderingThread()) // If RenderingThread is not started yet, affinity will applied at RT creation time 
	{
		FPlatformProcess::SetThreadAffinityMask(FPlatformAffinity::GetRenderingThreadMask());
	}
	else if (IsInGameThread())
	{
		FPlatformProcess::SetThreadAffinityMask(FPlatformAffinity::GetMainGameMask());
	}
}

static void ApplyDefaultThreadAffinity(IConsoleVariable* Var)
{
	FString AffinityCmd = CVarAndroidDefaultThreadAffinity.GetValueOnAnyThread();

	TArray<FString> Args;
	if (AffinityCmd.ParseIntoArrayWS(Args) > 0)
	{
		for (int32 Index = 0; Index + 1 < Args.Num(); Index += 2)
		{
			uint64 Aff = FParse::HexNumber(*Args[Index + 1]);
			if (!Aff)
			{
				UE_LOG(LogAndroid, Display, TEXT("Parsed 0 for affinity, using 0xFFFFFFFFFFFFFFFF instead"));
				Aff = 0xFFFFFFFFFFFFFFFF;
			}

#if !PLATFORM_LUMIN
			if (Args[Index] == TEXT("GT"))
			{
				FAndroidAffinity::GameThreadMask = Aff;
			}
			else if (Args[Index] == TEXT("RT"))
			{
				FAndroidAffinity::RenderingThreadMask = Aff;
			}
#endif
		}

		if (FTaskGraphInterface::IsRunning())
		{
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&AndroidSetAffinityOnThread),
				TStatId(), NULL, ENamedThreads::GetRenderThread());

			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&AndroidSetAffinityOnThread),
				TStatId(), NULL, ENamedThreads::GameThread);
		}
		else
		{
			AndroidSetAffinityOnThread();
		}
	}
}

void AndroidSetupDefaultThreadAffinity()
{
	ApplyDefaultThreadAffinity(nullptr);

	// Watch for CVar update
	CVarAndroidDefaultThreadAffinity->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&ApplyDefaultThreadAffinity));
}
