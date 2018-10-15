// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "HttpManager.h"
#include "HttpModule.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"
#include "Http.h"
#include "Misc/Guid.h"

#include "HttpThread.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"

#include "Stats/Stats.h"

// FHttpManager

FCriticalSection FHttpManager::RequestLock;

FHttpManager::FHttpManager()
	: FTickerObjectBase(0.0f)
	, Thread(nullptr)
	, CorrelationIdMethod(FHttpManager::GetDefaultCorrelationIdMethod())
	, DeferredDestroyDelay(10.0f)
{
}

FHttpManager::~FHttpManager()
{
	if (Thread)
	{
		Thread->StopThread();
		delete Thread;
	}
}

void FHttpManager::Initialize()
{
	if (FPlatformHttp::UsesThreadedHttp())
	{
		Thread = CreateHttpThread();
		Thread->StartThread();
	}
}

void FHttpManager::SetCorrelationIdMethod(TFunction<FString()> InCorrelationIdMethod)
{
	check(InCorrelationIdMethod);
	CorrelationIdMethod = MoveTemp(InCorrelationIdMethod);
}

FString FHttpManager::CreateCorrelationId() const
{
	return CorrelationIdMethod();
}

bool FHttpManager::IsDomainAllowed(const FString& Url) const
{
#if !UE_BUILD_SHIPPING
#if !(UE_GAME || UE_SERVER)
	// Whitelist is opt-in in non-shipping non-game/server builds
	static const bool bEnableWhitelist = FParse::Param(FCommandLine::Get(), TEXT("EnableHttpWhitelist"));
	if (!bEnableWhitelist)
	{
		return true;
	}
#else
	// Allow non-shipping game/server builds to disable the whitelist check
	static const bool bDisableWhitelist = FParse::Param(FCommandLine::Get(), TEXT("DisableHttpWhitelist"));
	if (bDisableWhitelist)
	{
		return true;
	}
#endif
#endif // !UE_BUILD_SHIPPING

	// check to see if the Domain is white-listed (or no white-list specified)
	const TArray<FString>& AllowedDomains = FHttpModule::Get().GetAllowedDomains();
	if (AllowedDomains.Num() > 0)
	{
		const FString Domain = FPlatformHttp::GetUrlDomain(Url);
		for (const FString& AllowedDomain : AllowedDomains)
		{
			if (Domain.EndsWith(AllowedDomain))
			{
				return true;
			}
		}
		return false;
	}
	return true;
}

/*static*/
TFunction<FString()> FHttpManager::GetDefaultCorrelationIdMethod()
{
	return []{ return FGuid::NewGuid().ToString(); };
}

FHttpThread* FHttpManager::CreateHttpThread()
{
	return new FHttpThread();
}

void FHttpManager::Flush(bool bShutdown)
{
	FScopeLock ScopeLock(&RequestLock);
	double MaxFlushTimeSeconds = -1.0; // default to no limit
	GConfig->GetDouble(TEXT("HTTP"), TEXT("MaxFlushTimeSeconds"), MaxFlushTimeSeconds, GEngineIni);

	if (bShutdown)
	{
		if (Requests.Num())
		{
			UE_LOG(LogHttp, Display, TEXT("Http module shutting down, but needs to wait on %d outstanding Http requests:"), Requests.Num());
		}
		// Clear delegates since they may point to deleted instances
		for (TArray<TSharedRef<IHttpRequest>>::TIterator It(Requests); It; ++It)
		{
			TSharedRef<IHttpRequest> Request = *It;
			Request->OnProcessRequestComplete().Unbind();
			Request->OnRequestProgress().Unbind();
			UE_LOG(LogHttp, Display, TEXT("	verb=[%s] url=[%s] status=%s"), *Request->GetVerb(), *Request->GetURL(), EHttpRequestStatus::ToString(Request->GetStatus()));
		}
	}

	// block until all active requests have completed
	double BeginWaitTime = FPlatformTime::Seconds();
	double LastTime = BeginWaitTime;
	while (Requests.Num() > 0)
	{
		const double AppTime = FPlatformTime::Seconds();
		//UE_LOG(LogHttp, Display, TEXT("Waiting for %0.2f seconds. Limit:%0.2f seconds"), (AppTime - BeginWaitTime), MaxFlushTimeSeconds);
		if (bShutdown && MaxFlushTimeSeconds > 0 && (GIsRequestingExit || (AppTime - BeginWaitTime > MaxFlushTimeSeconds)))
		{
			UE_LOG(LogHttp, Display, TEXT("Canceling remaining HTTP requests after waiting %0.2f seconds"), (AppTime - BeginWaitTime));
			for (TArray<TSharedRef<IHttpRequest>>::TIterator It(Requests); It; ++It)
			{
				TSharedRef<IHttpRequest> Request = *It;
				Request->CancelRequest();
			}
		}
		Tick(AppTime - LastTime);
		LastTime = AppTime;
		if (Requests.Num() > 0)
		{
			if (FPlatformProcess::SupportsMultithreading())
			{
				UE_LOG(LogHttp, Display, TEXT("Sleeping 0.5s to wait for %d outstanding Http requests."), Requests.Num());
				FPlatformProcess::Sleep(0.5f);
			}
			else if (Thread)
			{
				Thread->Tick();
			}
			else
			{
				check(!FPlatformHttp::UsesThreadedHttp());
			}
		}
	}
}

bool FHttpManager::Tick(float DeltaSeconds)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpManager_Tick);

	FScopeLock ScopeLock(&RequestLock);

	// Tick each active request
	for (TArray<TSharedRef<IHttpRequest>>::TIterator It(Requests); It; ++It)
	{
		TSharedRef<IHttpRequest> Request = *It;
		Request->Tick(DeltaSeconds);
	}
	// Tick any pending destroy objects
	for (int Idx=0; Idx < PendingDestroyRequests.Num(); Idx++)
	{
		FRequestPendingDestroy& Request = PendingDestroyRequests[Idx];
		Request.TimeLeft -= DeltaSeconds;
		if (Request.TimeLeft <= 0)
		{	
			PendingDestroyRequests.RemoveAt(Idx--);
		}		
	}

	if (Thread)
	{
		TArray<IHttpThreadedRequest*> CompletedThreadedRequests;
		Thread->GetCompletedRequests(CompletedThreadedRequests);

		// Finish and remove any completed requests
		for (IHttpThreadedRequest* CompletedRequest : CompletedThreadedRequests)
		{
			// Keep track of requests that have been removed to be destroyed later
			PendingDestroyRequests.AddUnique(FRequestPendingDestroy(DeferredDestroyDelay, CompletedRequest->AsShared()));

			CompletedRequest->FinishRequest();
			Requests.Remove(CompletedRequest->AsShared());
		}
	}
	// keep ticking
	return true;
}

void FHttpManager::AddRequest(const TSharedRef<IHttpRequest>& Request)
{
	FScopeLock ScopeLock(&RequestLock);

	Requests.Add(Request);
}

void FHttpManager::RemoveRequest(const TSharedRef<IHttpRequest>& Request)
{
	FScopeLock ScopeLock(&RequestLock);

	// Keep track of requests that have been removed to be destroyed later
	PendingDestroyRequests.AddUnique(FRequestPendingDestroy(DeferredDestroyDelay,Request));

	Requests.Remove(Request);
}

void FHttpManager::AddThreadedRequest(const TSharedRef<IHttpThreadedRequest>& Request)
{
	check(Thread);
	{
		FScopeLock ScopeLock(&RequestLock);
		Requests.Add(Request);
	}
	Thread->AddRequest(&Request.Get());
}

void FHttpManager::CancelThreadedRequest(const TSharedRef<IHttpThreadedRequest>& Request)
{
	check(Thread);
	Thread->CancelRequest(&Request.Get());
}

bool FHttpManager::IsValidRequest(const IHttpRequest* RequestPtr) const
{
	FScopeLock ScopeLock(&RequestLock);

	bool bResult = false;
	for (const TSharedRef<IHttpRequest>& Request : Requests)
	{
		if (&Request.Get() == RequestPtr)
		{
			bResult = true;
			break;
		}
	}

	return bResult;
}

void FHttpManager::DumpRequests(FOutputDevice& Ar) const
{
	FScopeLock ScopeLock(&RequestLock);
	
	Ar.Logf(TEXT("------- (%d) Http Requests"), Requests.Num());
	for (const TSharedRef<IHttpRequest>& Request : Requests)
	{
		Ar.Logf(TEXT("	verb=[%s] url=[%s] status=%s"),
			*Request->GetVerb(), *Request->GetURL(), EHttpRequestStatus::ToString(Request->GetStatus()));
	}
}

bool FHttpManager::SupportsDynamicProxy() const
{
	return false;
}
