// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "HttpRetrySystem.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Math/RandomStream.h"
#include "HttpModule.h"
#include "Http.h"
#include "HttpManager.h"
#include "Stats/Stats.h"

FHttpRetrySystem::FRequest::FRequest(
	FManager& InManager,
	const TSharedRef<IHttpRequest>& HttpRequest, 
	const FHttpRetrySystem::FRetryLimitCountSetting& InRetryLimitCountOverride,
	const FHttpRetrySystem::FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride,
	const FHttpRetrySystem::FRetryResponseCodes& InRetryResponseCodes,
	const FHttpRetrySystem::FRetryVerbs& InRetryVerbs,
	const FHttpRetrySystem::FRetryDomainsPtr& InRetryDomains
	)
    : FHttpRequestAdapterBase(HttpRequest)
    , Status(FHttpRetrySystem::FRequest::EStatus::NotStarted)
    , RetryLimitCountOverride(InRetryLimitCountOverride)
    , RetryTimeoutRelativeSecondsOverride(InRetryTimeoutRelativeSecondsOverride)
	, RetryResponseCodes(InRetryResponseCodes)
	, RetryVerbs(InRetryVerbs)
	, RetryDomains(InRetryDomains)
	, RetryManager(InManager)
{
    // if the InRetryTimeoutRelativeSecondsOverride override is being used the value cannot be negative
    check(!(InRetryTimeoutRelativeSecondsOverride.IsSet()) || (InRetryTimeoutRelativeSecondsOverride.GetValue() >= 0.0));

	if (RetryDomains.IsValid())
	{
		if (RetryDomains->Domains.Num() == 0)
		{
			// If there are no domains to cycle through, go through the simpler path
			RetryDomains.Reset();
		}
		else
		{
			// Start with the active index
			RetryDomainsIndex = RetryDomains->ActiveIndex;
			check(RetryDomains->Domains.IsValidIndex(RetryDomainsIndex));
		}
	}
}

bool FHttpRetrySystem::FRequest::ProcessRequest()
{ 
	TSharedRef<FRequest> RetryRequest = StaticCastSharedRef<FRequest>(AsShared());

	OriginalUrl = HttpRequest->GetURL();
	if (RetryDomains.IsValid())
	{
		SetUrlFromRetryDomains();
	}

	HttpRequest->OnRequestProgress().BindSP(RetryRequest, &FHttpRetrySystem::FRequest::HttpOnRequestProgress);

	return RetryManager.ProcessRequest(RetryRequest);
}

void FHttpRetrySystem::FRequest::SetUrlFromRetryDomains()
{
	check(RetryDomains.IsValid());
	FString OriginalUrlDomain = FPlatformHttp::GetUrlDomain(OriginalUrl);
	if (!OriginalUrlDomain.IsEmpty())
	{
		const FString Url(OriginalUrl.Replace(*OriginalUrlDomain, *RetryDomains->Domains[RetryDomainsIndex]));
		HttpRequest->SetURL(Url);
	}
}

void FHttpRetrySystem::FRequest::MoveToNextRetryDomain()
{
	check(RetryDomains.IsValid());
	const int32 NextDomainIndex = (RetryDomainsIndex + 1) % RetryDomains->Domains.Num();
	if (RetryDomains->ActiveIndex.CompareExchange(RetryDomainsIndex, NextDomainIndex))
	{
		RetryDomainsIndex = NextDomainIndex;
	}
	SetUrlFromRetryDomains();
}

void FHttpRetrySystem::FRequest::CancelRequest() 
{ 
	TSharedRef<FRequest> RetryRequest = StaticCastSharedRef<FRequest>(AsShared());

	RetryManager.CancelRequest(RetryRequest);
}

void FHttpRetrySystem::FRequest::HttpOnRequestProgress(FHttpRequestPtr InHttpRequest, int32 BytesSent, int32 BytesRcv)
{
	OnRequestProgress().ExecuteIfBound(AsShared(), BytesSent, BytesRcv);
}

FHttpRetrySystem::FManager::FManager(const FRetryLimitCountSetting& InRetryLimitCountDefault, const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsDefault)
    : RandomFailureRate(FRandomFailureRateSetting())
    , RetryLimitCountDefault(InRetryLimitCountDefault)
	, RetryTimeoutRelativeSecondsDefault(InRetryTimeoutRelativeSecondsDefault)
{}

TSharedRef<FHttpRetrySystem::FRequest> FHttpRetrySystem::FManager::CreateRequest(
	const FRetryLimitCountSetting& InRetryLimitCountOverride,
	const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride,
	const FRetryResponseCodes& InRetryResponseCodes,
	const FRetryVerbs& InRetryVerbs,
	const FRetryDomainsPtr& InRetryDomains)
{
	return MakeShareable(new FRequest(
		*this,
		FHttpModule::Get().CreateRequest(),
		InRetryLimitCountOverride,
		InRetryTimeoutRelativeSecondsOverride,
		InRetryResponseCodes,
		InRetryVerbs,
		InRetryDomains
		));
}

bool FHttpRetrySystem::FManager::ShouldRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
    bool bResult = false;

	FHttpResponsePtr Response = HttpRetryRequestEntry.Request->GetResponse();
	// invalid response means connection or network error but we need to know which one
	if (!Response.IsValid())
	{
		// ONLY retry bad responses if they are connection errors (NOT protocol errors or unknown) otherwise request may be sent (and processed!) twice
		EHttpRequestStatus::Type Status = HttpRetryRequestEntry.Request->GetStatus();
		if (Status == EHttpRequestStatus::Failed_ConnectionError)
		{
			bResult = true;
		}
		else if (Status == EHttpRequestStatus::Failed)
		{
			const FName Verb = FName(*HttpRetryRequestEntry.Request->GetVerb());

			// Be default, we will also allow retry for GET and HEAD requests even if they may duplicate on the server
			static const TSet<FName> DefaultRetryVerbs(TArray<FName>({ FName(TEXT("GET")), FName(TEXT("HEAD")) }));

			const bool bIsRetryVerbsEmpty = HttpRetryRequestEntry.Request->RetryVerbs.Num() == 0;
			if (bIsRetryVerbsEmpty && DefaultRetryVerbs.Contains(Verb))
			{
				bResult = true;
			}
			// If retry verbs are specified, only allow retrying the specified list of verbs
			else if (HttpRetryRequestEntry.Request->RetryVerbs.Contains(Verb))
			{
				bResult = true;
			}
		}
	}
	else
	{
		// this may be a successful response with one of the explicitly listed response codes we want to retry on
		if (HttpRetryRequestEntry.Request->RetryResponseCodes.Contains(Response->GetResponseCode()))
		{
			bResult = true;
		}
	}

    return bResult;
}

bool FHttpRetrySystem::FManager::CanRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
    bool bResult = false;

    bool bShouldTestCurrentRetryCount = false;
    double RetryLimitCount = 0;
    if (HttpRetryRequestEntry.Request->RetryLimitCountOverride.IsSet())
    {
        bShouldTestCurrentRetryCount = true;
        RetryLimitCount = HttpRetryRequestEntry.Request->RetryLimitCountOverride.GetValue();
    }
    else if (RetryLimitCountDefault.IsSet())
    {
        bShouldTestCurrentRetryCount = true;
        RetryLimitCount = RetryLimitCountDefault.GetValue();
    }

    if (bShouldTestCurrentRetryCount)
    {
        if (HttpRetryRequestEntry.CurrentRetryCount < RetryLimitCount)
        {
            bResult = true;
        }
    }

    return bResult;
}

bool FHttpRetrySystem::FManager::HasTimedOut(const FHttpRetryRequestEntry& HttpRetryRequestEntry, const double NowAbsoluteSeconds)
{
    bool bResult = false;

    bool bShouldTestRetryTimeout = false;
    double RetryTimeoutAbsoluteSeconds = HttpRetryRequestEntry.RequestStartTimeAbsoluteSeconds;
    if (HttpRetryRequestEntry.Request->RetryTimeoutRelativeSecondsOverride.IsSet())
    {
        bShouldTestRetryTimeout = true;
        RetryTimeoutAbsoluteSeconds += HttpRetryRequestEntry.Request->RetryTimeoutRelativeSecondsOverride.GetValue();
    }
    else if (RetryTimeoutRelativeSecondsDefault.IsSet())
    {
        bShouldTestRetryTimeout = true;
        RetryTimeoutAbsoluteSeconds += RetryTimeoutRelativeSecondsDefault.GetValue();
    }

    if (bShouldTestRetryTimeout)
    {
        if (NowAbsoluteSeconds >= RetryTimeoutAbsoluteSeconds)
        {
            bResult = true;
        }
    }

    return bResult;
}

float FHttpRetrySystem::FManager::GetLockoutPeriodSeconds(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
	float LockoutPeriod = 0.0f;

	// Check if there was a Retry-After header
	FHttpResponsePtr Response = HttpRetryRequestEntry.Request->GetResponse();
	if (Response.IsValid())
	{
		int32 ResponseCode = Response->GetResponseCode();
		if (ResponseCode == EHttpResponseCodes::TooManyRequests || ResponseCode == EHttpResponseCodes::ServiceUnavail)
		{
			FString RetryAfter = Response->GetHeader(TEXT("Retry-After"));
			if (!RetryAfter.IsEmpty())
			{
				if (RetryAfter.IsNumeric())
				{
					// seconds
					LockoutPeriod = FCString::Atof(*RetryAfter);
				}
				else
				{
					// http date
					FDateTime UTCServerTime;
					if (FDateTime::ParseHttpDate(RetryAfter, UTCServerTime))
					{
						const FDateTime UTCNow = FDateTime::UtcNow();
						LockoutPeriod = (UTCServerTime - UTCNow).GetTotalSeconds();
					}
				}
			}
			else
			{
				FString RateLimitReset = Response->GetHeader(TEXT("X-Rate-Limit-Reset"));
				if (!RateLimitReset.IsEmpty())
				{
					// UTC seconds
					const FDateTime UTCServerTime = FDateTime::FromUnixTimestamp(FCString::Atoi64(*RateLimitReset));
					const FDateTime UTCNow = FDateTime::UtcNow();
					LockoutPeriod = (UTCServerTime - UTCNow).GetTotalSeconds();
				}
			}
		}
	}

	if (HttpRetryRequestEntry.CurrentRetryCount >= 1)
	{
		if (LockoutPeriod <= 0.0f)
		{
			const bool bFailedToConnect = HttpRetryRequestEntry.Request->GetStatus() == EHttpRequestStatus::Failed_ConnectionError;
			const bool bHasRetryDomains = HttpRetryRequestEntry.Request->RetryDomains.IsValid();
			// Skip the lockout period if we failed to connect to a domain and we have other domains to try
			const bool bSkipLockoutPeriod = (bFailedToConnect && bHasRetryDomains);
			if (!bSkipLockoutPeriod)
			{
				constexpr const float LockoutPeriodMinimumSeconds = 5.0f;
				constexpr const float LockoutPeriodEscalationSeconds = 2.5f;
				constexpr const float LockoutPeriodMaxSeconds = 30.0f;
				LockoutPeriod = LockoutPeriodMinimumSeconds + LockoutPeriodEscalationSeconds * (HttpRetryRequestEntry.CurrentRetryCount - 1);
				LockoutPeriod = FMath::Min(LockoutPeriod, LockoutPeriodMaxSeconds);
			}
		}
	}

	return LockoutPeriod;
}

static FRandomStream temp(4435261);

bool FHttpRetrySystem::FManager::Update(uint32* FileCount, uint32* FailingCount, uint32* FailedCount, uint32* CompletedCount)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRetrySystem_FManager_Update);

	bool bIsGreen = true;

	if (FileCount != nullptr)
	{
		*FileCount = RequestList.Num();
	}

	const double NowAbsoluteSeconds = FPlatformTime::Seconds();

	// Basic algorithm
	// for each managed item
	//    if the item hasn't timed out
	//       if the item's retry state is NotStarted
	//          if the item's request's state is not NotStarted
	//             move the item's retry state to Processing
	//          endif
	//       endif
	//       if the item's retry state is Processing
	//          if the item's request's state is Failed
	//             flag return code to false
	//             if the item can be retried
	//                increment FailingCount if applicable
	//                retry the item's request
	//                increment the item's retry count
	//             else
	//                increment FailedCount if applicable
	//                set the item's retry state to FailedRetry
	//             endif
	//          else if the item's request's state is Succeeded
	//          endif
	//       endif
	//    else
	//       flag return code to false
	//       set the item's retry state to FailedTimeout
	//       increment FailedCount if applicable
	//    endif
	//    if the item's retry state is FailedRetry
	//       do stuff
	//    endif
	//    if the item's retry state is FailedTimeout
	//       do stuff
	//    endif
	//    if the item's retry state is Succeeded
	//       do stuff
	//    endif
	// endfor

	int32 index = 0;
	while (index < RequestList.Num())
	{
		FHttpRetryRequestEntry& HttpRetryRequestEntry = RequestList[index];
		TSharedRef<FHttpRetrySystem::FRequest>& HttpRetryRequest = HttpRetryRequestEntry.Request;

		const EHttpRequestStatus::Type RequestStatus = HttpRetryRequest->GetStatus();

		if (HttpRetryRequestEntry.bShouldCancel)
		{
			UE_LOG(LogHttp, Warning, TEXT("Request cancelled on %s"), *(HttpRetryRequest->GetURL()));
			HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::Cancelled;
		}
		else
		{
			if (!HasTimedOut(HttpRetryRequestEntry, NowAbsoluteSeconds))
			{
				if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::NotStarted)
				{
					if (RequestStatus != EHttpRequestStatus::NotStarted)
					{
						HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::Processing;
					}
				}

				if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Processing)
				{
					bool forceFail = false;

					// Code to simulate request failure
					if (RequestStatus == EHttpRequestStatus::Succeeded && RandomFailureRate.IsSet())
					{
						float random = temp.GetFraction();
						if (random < RandomFailureRate.GetValue())
						{
							forceFail = true;
						}
					}

					// If we failed to connect, try the next domain in the list
					if (RequestStatus == EHttpRequestStatus::Failed_ConnectionError)
					{
						if (HttpRetryRequest->RetryDomains.IsValid())
						{
							HttpRetryRequest->MoveToNextRetryDomain();
						}
					}
					// Save these for failure case retry checks if we hit a completion state
					bool bShouldRetry = false;
					bool bCanRetry = false;
					if (RequestStatus == EHttpRequestStatus::Failed || RequestStatus == EHttpRequestStatus::Failed_ConnectionError || RequestStatus == EHttpRequestStatus::Succeeded)
					{
						bShouldRetry = ShouldRetry(HttpRetryRequestEntry);
						bCanRetry = CanRetry(HttpRetryRequestEntry);
					}

					if (RequestStatus == EHttpRequestStatus::Failed || RequestStatus == EHttpRequestStatus::Failed_ConnectionError || forceFail || (bShouldRetry && bCanRetry))
					{
						bIsGreen = false;

						if (forceFail || (bShouldRetry && bCanRetry))
						{
							float LockoutPeriod = GetLockoutPeriodSeconds(HttpRetryRequestEntry);

							if (LockoutPeriod > 0.0f)
							{
								UE_LOG(LogHttp, Warning, TEXT("Lockout of %fs on %s"), LockoutPeriod, *(HttpRetryRequest->GetURL()));
							}

							HttpRetryRequestEntry.LockoutEndTimeAbsoluteSeconds = NowAbsoluteSeconds + LockoutPeriod;
							HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::ProcessingLockout;
							
							HttpRetryRequest->OnRequestWillRetry().ExecuteIfBound(HttpRetryRequest, HttpRetryRequest->GetResponse(), LockoutPeriod);
						}
						else
						{
							UE_LOG(LogHttp, Warning, TEXT("Retry exhausted on %s"), *(HttpRetryRequest->GetURL()));
							if (FailedCount != nullptr)
							{
								++(*FailedCount);
							}
							HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::FailedRetry;
						}
					}
					else if (RequestStatus == EHttpRequestStatus::Succeeded)
					{
						if (HttpRetryRequestEntry.CurrentRetryCount > 0)
						{
							UE_LOG(LogHttp, Warning, TEXT("Success on %s"), *(HttpRetryRequest->GetURL()));
						}

						if (CompletedCount != nullptr)
						{
							++(*CompletedCount);
						}

						HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::Succeeded;
					}
				}

				if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::ProcessingLockout)
				{
					if (NowAbsoluteSeconds >= HttpRetryRequestEntry.LockoutEndTimeAbsoluteSeconds)
					{
						// if this fails the HttpRequest's state will be failed which will cause the retry logic to kick(as expected)
						bool success = HttpRetryRequest->HttpRequest->ProcessRequest();
						if (success)
						{
							UE_LOG(LogHttp, Warning, TEXT("Retry %d on %s"), HttpRetryRequestEntry.CurrentRetryCount + 1, *(HttpRetryRequest->GetURL()));

							++HttpRetryRequestEntry.CurrentRetryCount;
							HttpRetryRequest->Status = FRequest::EStatus::Processing;
						}
					}

					if (FailingCount != nullptr)
					{
						++(*FailingCount);
					}
				}
			}
			else
			{
				UE_LOG(LogHttp, Warning, TEXT("Timeout on retry %d: %s"), HttpRetryRequestEntry.CurrentRetryCount + 1, *(HttpRetryRequest->GetURL()));
				bIsGreen = false;
				HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::FailedTimeout;
				if (FailedCount != nullptr)
				{
					++(*FailedCount);
				}
			}
		}

		bool bWasCompleted = false;
		bool bWasSuccessful = false;

        if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Cancelled ||
            HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::FailedRetry ||
            HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::FailedTimeout ||
            HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Succeeded)
		{
			bWasCompleted = true;
            bWasSuccessful = HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Succeeded;
		}

		if (bWasCompleted)
		{
			if (bWasSuccessful)
			{
				HttpRetryRequest->BroadcastResponseHeadersReceived();
			}
			HttpRetryRequest->OnProcessRequestComplete().ExecuteIfBound(HttpRetryRequest, HttpRetryRequest->GetResponse(), bWasSuccessful);
		}

        if(bWasSuccessful)
        {
            if(CompletedCount != nullptr)
            {
                ++(*CompletedCount);
            }
        }

		if (bWasCompleted)
		{
			RequestList.RemoveAtSwap(index);
		}
		else
		{
			++index;
		}
	}

	return bIsGreen;
}

FHttpRetrySystem::FManager::FHttpRetryRequestEntry::FHttpRetryRequestEntry(TSharedRef<FHttpRetrySystem::FRequest>& InRequest)
    : bShouldCancel(false)
    , CurrentRetryCount(0)
	, RequestStartTimeAbsoluteSeconds(FPlatformTime::Seconds())
	, Request(InRequest)
{}

bool FHttpRetrySystem::FManager::ProcessRequest(TSharedRef<FHttpRetrySystem::FRequest>& HttpRetryRequest)
{
	bool bResult = HttpRetryRequest->HttpRequest->ProcessRequest();

	if (bResult)
	{
		RequestList.Add(FHttpRetryRequestEntry(HttpRetryRequest));
	}

	return bResult;
}

void FHttpRetrySystem::FManager::CancelRequest(TSharedRef<FHttpRetrySystem::FRequest>& HttpRetryRequest)
{
	// Find the existing request entry if is was previously processed.
	bool bFound = false;
	for (int32 i = 0; i < RequestList.Num(); ++i)
	{
		FHttpRetryRequestEntry& EntryRef = RequestList[i];

		if (EntryRef.Request == HttpRetryRequest)
		{
			EntryRef.bShouldCancel = true;
			bFound = true;
		}
	}
	// If we did not find the entry, likely auth failed for the request, in which case ProcessRequest does not get called.
	// Adding it to the list and flagging as cancel will process it on next tick.
	if (!bFound)
	{
		FHttpRetryRequestEntry RetryRequestEntry(HttpRetryRequest);
		RetryRequestEntry.bShouldCancel = true;
		RequestList.Add(RetryRequestEntry);
	}
	HttpRetryRequest->HttpRequest->CancelRequest();
}

/* This should only be used when shutting down or suspending, to make sure 
	all pending HTTP requests are flushed to the network */
void FHttpRetrySystem::FManager::BlockUntilFlushed(float InTimeoutSec)
{
	const float SleepInterval = 0.016;
	float TimeElapsed = 0.0f;
	uint32 FileCount, FailingCount, FailedCount, CompleteCount;
	while (RequestList.Num() > 0 && TimeElapsed < InTimeoutSec)
	{
		FHttpModule::Get().GetHttpManager().Tick(SleepInterval);
		Update(&FileCount, &FailingCount, &FailedCount, &CompleteCount);
		FPlatformProcess::Sleep(SleepInterval);
		TimeElapsed += SleepInterval;
	}
}