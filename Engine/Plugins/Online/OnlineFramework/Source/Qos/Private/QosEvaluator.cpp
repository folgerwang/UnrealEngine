// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "QosEvaluator.h"
#include "TimerManager.h"
#include "QosModule.h"
#include "QosStats.h"
#include "Engine/World.h"
#include "Icmp.h"

UQosEvaluator::UQosEvaluator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ParentWorld(nullptr)
	, StartTimestamp(0.0)
	, bInProgress(false)
	, bCancelOperation(false)
	, AnalyticsProvider(nullptr)
	, QosStats(nullptr)
{
}

void UQosEvaluator::SetWorld(UWorld* InWorld)
{
	ParentWorld = InWorld;
}

void UQosEvaluator::SetAnalyticsProvider(TSharedPtr<IAnalyticsProvider> InAnalyticsProvider)
{
	AnalyticsProvider = InAnalyticsProvider;
}

void UQosEvaluator::Cancel()
{
	bCancelOperation = true;
}

void UQosEvaluator::FindDatacenters(const FQosParams& InParams, const TArray<FQosRegionInfo>& InRegions, const TArray<FQosDatacenterInfo>& InDatacenters, const FOnQosSearchComplete& InCompletionDelegate)
{
	if (!IsActive())
	{
		bInProgress = true;
		StartTimestamp = FPlatformTime::Seconds();

		StartAnalytics();

		Datacenters.Empty(InDatacenters.Num());
		for (const FQosRegionInfo& Region : InRegions)
		{
			if (Region.IsPingable())
			{
				int32 NumDatacenters = 0;
				for (const FQosDatacenterInfo& Datacenter : InDatacenters)
				{
					if (Datacenter.RegionId == Region.RegionId)
					{
						if (Datacenter.IsPingable())
						{
							Datacenters.Emplace(Datacenter, Region.IsUsable());
							NumDatacenters++;
						}
						else
						{
							UE_LOG(LogQos, Verbose, TEXT("Skipping datacenter [%s]"), *Datacenter.Id);
						}
					}
				}

				if (NumDatacenters == 0)
				{
					UE_LOG(LogQos, Warning, TEXT("Region [%s] has no usable datacenters"), *Region.RegionId);
				}
			}
			else
			{
				UE_LOG(LogQos, Verbose, TEXT("Skipping region [%s]"), *Region.RegionId);
			}
		}

		// Ping list of known servers defined by config
		PingRegionServers(InParams, InCompletionDelegate);
	}
	else
	{
		UE_LOG(LogQos, Log, TEXT("Qos evaluation already in progress, ignoring"));
		// Just trigger delegate now (Finalize resets state vars)
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([InCompletionDelegate]() {
			InCompletionDelegate.ExecuteIfBound(EQosCompletionResult::Failure, TArray<FDatacenterQosInstance>());
		}));
	}
}

void UQosEvaluator::PingRegionServers(const FQosParams& InParams, const FOnQosSearchComplete& InCompletionDelegate)
{
	// Failsafe for bad configuration
	bool bDidNothing = true;

	const int32 NumTestsPerRegion = InParams.NumTestsPerRegion;

	TWeakObjectPtr<UQosEvaluator> WeakThisCap(this);
	for (FDatacenterQosInstance& Datacenter : Datacenters)
	{
		if (Datacenter.Definition.IsPingable())
		{
			const FString& RegionId = Datacenter.Definition.Id;
			const int32 NumServers = Datacenter.Definition.Servers.Num();
			int32 ServerIdx = FMath::RandHelper(NumServers);
			// Default to invalid ping tests and set it to something else later
			Datacenter.Result = EQosDatacenterResult::Invalid;
			if (NumServers > 0)
			{
				for (int32 PingIdx = 0; PingIdx < NumTestsPerRegion; PingIdx++)
				{
					const FQosPingServerInfo& Server = Datacenter.Definition.Servers[ServerIdx];
					const FString Address = FString::Printf(TEXT("%s:%d"), *Server.Address, Server.Port);

					auto CompletionDelegate = [WeakThisCap, RegionId, NumTestsPerRegion, InCompletionDelegate](FIcmpEchoResult InResult)
					{
						if (WeakThisCap.IsValid())
						{
							auto StrongThis = WeakThisCap.Get();
							if (!StrongThis->IsPendingKill())
							{
								StrongThis->OnPingResultComplete(RegionId, NumTestsPerRegion, InResult);
								if (StrongThis->AreAllRegionsComplete())
								{
									UE_LOG(LogQos, Verbose, TEXT("Qos complete in %0.2f s"), FPlatformTime::Seconds() - StrongThis->StartTimestamp);
									EQosCompletionResult TotalResult = EQosCompletionResult::Success;
									StrongThis->CalculatePingAverages();
									StrongThis->EndAnalytics(TotalResult);
									InCompletionDelegate.ExecuteIfBound(TotalResult, StrongThis->Datacenters);
								}
								else if (StrongThis->bCancelOperation)
								{
									UE_LOG(LogQos, Verbose, TEXT("Qos cancelled after %0.2f s"), FPlatformTime::Seconds() - StrongThis->StartTimestamp);
									EQosCompletionResult TotalResult = EQosCompletionResult::Canceled;
									StrongThis->EndAnalytics(TotalResult);
									InCompletionDelegate.ExecuteIfBound(TotalResult, StrongThis->Datacenters);
								}
							}
						}
					};

					UE_LOG(LogQos, VeryVerbose, TEXT("Pinging %s %s"), *Datacenter.Definition.ToDebugString(), *Address);
					FUDPPing::UDPEcho(Address, InParams.Timeout, CompletionDelegate);
					ServerIdx = (ServerIdx + 1) % NumServers;
					bDidNothing = false;
				}
			}
			else
			{
				UE_LOG(LogQos, Verbose, TEXT("Nothing to ping %s"), *Datacenter.Definition.ToDebugString());
			}
		}
		else
		{
			UE_LOG(LogQos, Verbose, TEXT("Datacenter disabled %s"), *Datacenter.Definition.ToDebugString());
		}
	}

	if (bDidNothing)
	{
		InCompletionDelegate.ExecuteIfBound(EQosCompletionResult::Failure, Datacenters);
	}
}

void UQosEvaluator::CalculatePingAverages(int32 TimeToDiscount)
{
	for (FDatacenterQosInstance& Datacenter : Datacenters)
	{
		int32 TotalPingInMs = 0;
		int32 NumResults = 0;
		for (int32 PingIdx = 0; PingIdx < Datacenter.PingResults.Num(); PingIdx++)
		{
			if (Datacenter.PingResults[PingIdx] != UNREACHABLE_PING)
			{
				TotalPingInMs += Datacenter.PingResults[PingIdx];
				NumResults++;
			}
			else
			{
				UE_LOG(LogQos, Log, TEXT("Datacenter[%s]: qos unreachable"), *Datacenter.Definition.Id);
			}
		}

		int32 RawAvgPing = UNREACHABLE_PING;
		Datacenter.AvgPingMs = RawAvgPing;
		if (NumResults > 0)
		{
			RawAvgPing = TotalPingInMs / NumResults;
			Datacenter.AvgPingMs = FMath::Max<int32>(RawAvgPing - TimeToDiscount, 1);
		}

		UE_LOG(LogQos, Verbose, TEXT("Datacenter[%s] Avg: %d Num: %d; Adjusted: %d"), *Datacenter.Definition.Id, RawAvgPing, NumResults, Datacenter.AvgPingMs);

		if (QosStats.IsValid())
		{
			QosStats->RecordRegionInfo(Datacenter, NumResults);
		}
	}
}

bool UQosEvaluator::AreAllRegionsComplete()
{
	for (FDatacenterQosInstance& Region : Datacenters)
	{
		if (Region.Definition.bEnabled && Region.Result == EQosDatacenterResult::Invalid)
		{
			return false;
		}
	}

	return true;
}

void UQosEvaluator::OnPingResultComplete(const FString& RegionId, int32 NumTests, const FIcmpEchoResult& Result)
{
	for (FDatacenterQosInstance& Region : Datacenters)
	{
		if (Region.Definition.Id == RegionId)
		{
			UE_LOG(LogQos, VeryVerbose, TEXT("Ping Complete %s %s: %d"), *Region.Definition.ToDebugString(), *Result.ResolvedAddress, (int32)(Result.Time * 1000.0f));

			const bool bSuccess = (Result.Status == EIcmpResponseStatus::Success);
			int32 PingInMs = bSuccess ? (int32)(Result.Time * 1000.0f) : UNREACHABLE_PING;
			Region.PingResults.Add(PingInMs);
			Region.NumResponses += bSuccess ? 1 : 0;

			if (QosStats.IsValid())
			{
				QosStats->RecordQosAttempt(RegionId, Result.ResolvedAddress, PingInMs, bSuccess);
			}

			if (Region.PingResults.Num() == NumTests)
			{
				Region.LastCheckTimestamp = FDateTime::UtcNow();
				Region.Result = (Region.NumResponses == NumTests) ? EQosDatacenterResult::Success : EQosDatacenterResult::Incomplete;
			}
			break;
		}
	}
}

void UQosEvaluator::StartAnalytics()
{
	if (AnalyticsProvider.IsValid())
	{
		ensure(!QosStats.IsValid());
		QosStats = MakeShareable(new FQosDatacenterStats());
		QosStats->StartQosPass();
	}
}

void UQosEvaluator::EndAnalytics(EQosCompletionResult CompletionResult)
{
	if (QosStats.IsValid())
	{
		if (CompletionResult != EQosCompletionResult::Canceled)
		{
			EDatacenterResultType ResultType = EDatacenterResultType::Failure;
			if (CompletionResult != EQosCompletionResult::Failure)
			{
				ResultType = EDatacenterResultType::Normal;
			}

			QosStats->EndQosPass(ResultType);
			QosStats->Upload(AnalyticsProvider);
		}

		QosStats = nullptr;
	}
}

UWorld* UQosEvaluator::GetWorld() const
{
	UWorld* World = ParentWorld.Get();
	check(World);
	return World;
}

FTimerManager& UQosEvaluator::GetWorldTimerManager() const
{
	return GetWorld()->GetTimerManager();
}

