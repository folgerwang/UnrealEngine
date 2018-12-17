// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "QosRegionManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "QosInterface.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "QosEvaluator.h"
#include "QosModule.h"

#define LAST_REGION_EVALUATION 3

EQosDatacenterResult FRegionQosInstance::GetRegionResult() const
{
	EQosDatacenterResult Result = EQosDatacenterResult::Success;
	for (const FDatacenterQosInstance& Datacenter : DatacenterOptions)
	{
		if (Datacenter.Result == EQosDatacenterResult::Invalid)
		{
			Result = EQosDatacenterResult::Invalid;
			break;
		}
		if (Datacenter.Result == EQosDatacenterResult::Incomplete)
		{
			Result = EQosDatacenterResult::Incomplete;
			break;
		}
	}

	return Result;
}

int32 FRegionQosInstance::GetBestAvgPing() const
{
	int32 BestPing = UNREACHABLE_PING;
	if (DatacenterOptions.Num() > 0)
	{
		// Presorted for best result first
		BestPing = DatacenterOptions[0].AvgPingMs;
	}

	return BestPing;
}

FString FRegionQosInstance::GetBestSubregion() const
{
	FString BestDatacenterId;
	if (DatacenterOptions.Num() > 0)
	{
		// Presorted for best result first
		BestDatacenterId = DatacenterOptions[0].Definition.Id;
	}

	return BestDatacenterId;
}

void FRegionQosInstance::GetSubregionPreferences(TArray<FString>& OutSubregions) const
{
	for (const FDatacenterQosInstance& Option : DatacenterOptions)
	{
		// Presorted for best result first
		OutSubregions.Add(Option.Definition.Id);
	}
}

UQosRegionManager::UQosRegionManager(const FObjectInitializer& ObjectInitializer)
	: NumTestsPerRegion(3)
	, PingTimeout(5.0f)
	, LastCheckTimestamp(0)
	, Evaluator(nullptr)
	, QosEvalResult(EQosCompletionResult::Invalid)
{
	check(GConfig);
	GConfig->GetString(TEXT("Qos"), TEXT("ForceRegionId"), ForceRegionId, GEngineIni);

	// get a forced region id from the command line as an override
	bRegionForcedViaCommandline = FParse::Value(FCommandLine::Get(), TEXT("McpRegion="), ForceRegionId);
	if (!ForceRegionId.IsEmpty())
	{
		ForceRegionId.ToUpperInline();
	}
}

void UQosRegionManager::PostReloadConfig(UProperty* PropertyThatWasLoaded)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		for (int32 RegionIdx = RegionOptions.Num() - 1; RegionIdx >= 0; RegionIdx--)
		{
			FRegionQosInstance& RegionOption = RegionOptions[RegionIdx];

			bool bFound = false;
			for (FQosRegionInfo& RegionDef : RegionDefinitions)
			{
				if (RegionDef.RegionId == RegionOption.Definition.RegionId)
				{
					bFound = true;
				}
			}

			if (!bFound)
			{
				// Old value needs to be removed, preserve order
				RegionOptions.RemoveAt(RegionIdx);
			}
		}

		for (int32 RegionIdx = 0; RegionIdx < RegionDefinitions.Num(); RegionIdx++)
		{
			FQosRegionInfo& RegionDef = RegionDefinitions[RegionIdx];

			bool bFound = false;
			for (FRegionQosInstance& RegionOption : RegionOptions)
			{
				if (RegionDef.RegionId == RegionOption.Definition.RegionId)
				{
					// Overwrite the metadata
					RegionOption.Definition = RegionDef;
					bFound = true;
				}
			}

			if (!bFound)
			{
				// Add new value not in old list
				FRegionQosInstance NewRegion(RegionDef);
				RegionOptions.Insert(NewRegion, RegionIdx);
			}
		}

		OnQoSSettingsChangedDelegate.ExecuteIfBound();

		// Validate the current region selection (skipped if a selection has never been attempted)
		if (QosEvalResult != EQosCompletionResult::Invalid)
		{
			TrySetDefaultRegion();
		}

		SanityCheckDefinitions();
	}
}

int32 UQosRegionManager::GetMaxPingMs() const
{
	int32 MaxPing = -1;
	if (GConfig->GetInt(TEXT("Qos"), TEXT("MaximumPingMs"), MaxPing, GEngineIni) && MaxPing > 0)
	{
		return MaxPing;
	}
	return -1;
}

// static
FString UQosRegionManager::GetDatacenterId()
{
	struct FDcidInfo
	{
		FDcidInfo()
		{
			FString OverrideDCID;
			if (FParse::Value(FCommandLine::Get(), TEXT("DCID="), OverrideDCID))
			{
				// DCID specified on command line
				DCIDString = OverrideDCID.ToUpper();
			}
			else
			{
				FString DefaultDCID;
				check(GConfig);
				if (GConfig->GetString(TEXT("Qos"), TEXT("DCID"), DefaultDCID, GEngineIni))
				{
					// DCID specified in ini file
					DCIDString = DefaultDCID.ToUpper();
				}
			}
		}

		FString DCIDString;
	};
	static FDcidInfo DCID;
	return DCID.DCIDString;
}

FString UQosRegionManager::GetAdvertisedSubregionId()
{
	struct FSubregion
	{
		FSubregion()
		{
			FString OverrideSubregion;
			if (FParse::Value(FCommandLine::Get(), TEXT("McpSubregion="), OverrideSubregion))
			{
				// Subregion specified on command line
				SubregionString = OverrideSubregion.ToUpper();
			}
			else
			{
				FString DefaultSubregion;
				check(GConfig);
				if (GConfig->GetString(TEXT("Qos"), TEXT("McpSubregion"), DefaultSubregion, GEngineIni))
				{
					// DCID specified in ini file
					SubregionString = DefaultSubregion.ToUpper();
				}
			}
		}

		FString SubregionString;
	};
	static FSubregion Subregion;
	return Subregion.SubregionString;
}

void UQosRegionManager::BeginQosEvaluation(UWorld* World, const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, const FSimpleDelegate& OnComplete)
{
	check(World);

	// There are valid cached results, use them
	if ((RegionOptions.Num() > 0) &&
		(QosEvalResult == EQosCompletionResult::Success) &&
		(FDateTime::UtcNow() - LastCheckTimestamp).GetTotalSeconds() <= LAST_REGION_EVALUATION)
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([OnComplete]()
		{
			OnComplete.ExecuteIfBound();
		}));
		return;
	}

	// add to the completion delegate
	OnQosEvalCompleteDelegate.Add(OnComplete);

	// if we're already evaluating, simply return
	if (Evaluator == nullptr)
	{
		// create a new evaluator and start the process of running
		Evaluator = NewObject<UQosEvaluator>();
		Evaluator->AddToRoot();
		Evaluator->SetWorld(World);
		Evaluator->SetAnalyticsProvider(AnalyticsProvider);

		FQosParams Params;
		Params.NumTestsPerRegion = NumTestsPerRegion;
		Params.Timeout = PingTimeout;
		Evaluator->FindDatacenters(Params, RegionDefinitions, DatacenterDefinitions, FOnQosSearchComplete::CreateUObject(this, &UQosRegionManager::OnQosEvaluationComplete));
	}
}

void UQosRegionManager::OnQosEvaluationComplete(EQosCompletionResult Result, const TArray<FDatacenterQosInstance>& DatacenterInstances)
{
	// toss the evaluator
	if (Evaluator != nullptr)
	{
		Evaluator->RemoveFromRoot();
		Evaluator->MarkPendingKill();
		Evaluator = nullptr;
	}
	QosEvalResult = Result;
	RegionOptions.Empty(RegionDefinitions.Num());

	TMultiMap<FString, FDatacenterQosInstance> DatacenterMap;
	for (const FDatacenterQosInstance& Datacenter : DatacenterInstances)
	{
		DatacenterMap.Add(Datacenter.Definition.RegionId, Datacenter);
	}

	for (const FQosRegionInfo& RegionInfo : RegionDefinitions)
	{
		if (RegionInfo.IsPingable())
		{
			if (DatacenterMap.Num(RegionInfo.RegionId))
			{
				// Build region options from datacenter details
				FRegionQosInstance* NewRegion = new (RegionOptions) FRegionQosInstance(RegionInfo);
				DatacenterMap.MultiFind(RegionInfo.RegionId, NewRegion->DatacenterOptions);

				NewRegion->DatacenterOptions.Sort([](const FDatacenterQosInstance& A, const FDatacenterQosInstance& B)
				{
					// Sort ping best to worst
					return A.AvgPingMs < B.AvgPingMs;
				});
			}
			else
			{
				UE_LOG(LogQos, Warning, TEXT("No datacenters for region %s"), *RegionInfo.RegionId);
			}
		}
	}

	LastCheckTimestamp = FDateTime::UtcNow();

	if (!SelectedRegionId.IsEmpty() && SelectedRegionId == NO_REGION)
	{
		// Put the dev region back into the list and select it
		ForceSelectRegion(SelectedRegionId);
	}

	// treat lack of any regions as a failure
	if (RegionOptions.Num() <= 0)
	{
		QosEvalResult = EQosCompletionResult::Failure;
	}

	if (QosEvalResult == EQosCompletionResult::Success ||
		QosEvalResult == EQosCompletionResult::Failure)
	{
		if (RegionOptions.Num() > 0)
		{
			// Try to set something regardless of Qos result
			TrySetDefaultRegion();
		}
	}
	
	// fire notifications
	TArray<FSimpleDelegate> NotifyList = OnQosEvalCompleteDelegate;
	OnQosEvalCompleteDelegate.Empty();
	for (const auto& Callback : NotifyList)
	{
		Callback.ExecuteIfBound();
	}
}

FString UQosRegionManager::GetRegionId() const
{
	if (!ForceRegionId.IsEmpty())
	{
		// we may have updated INI to bypass this process
		return ForceRegionId;
	}

	if (QosEvalResult == EQosCompletionResult::Invalid)
	{
		// if we haven't run the evaluator just use the region from settings
		// development dedicated server will come here, live services should use -mcpregion
		return NO_REGION;
	}

	if (SelectedRegionId.IsEmpty())
	{
		// Always set some kind of region, empty implies "wildcard" to the matchmaking code
		UE_LOG(LogQos, Verbose, TEXT("No region currently set."));
		return NO_REGION;
	}

	return SelectedRegionId;
}

FString UQosRegionManager::GetBestRegion() const
{
	if (!ForceRegionId.IsEmpty())
	{
		return ForceRegionId;
	}

	FString BestRegionId;

	// try to select the lowest ping
	int32 BestPing = INT_MAX;
	const TArray<FRegionQosInstance>& LocalRegionOptions = GetRegionOptions();
	for (const FRegionQosInstance& Region : LocalRegionOptions)
	{
		if (Region.IsAutoAssignable() && Region.GetBestAvgPing() < BestPing)
		{
			BestPing = Region.GetBestAvgPing();
			BestRegionId = Region.Definition.RegionId;
		}
	}

	return BestRegionId;
}

void UQosRegionManager::GetSubregionPreferences(const FString& RegionId, TArray<FString>& OutSubregions) const
{
	const TArray<FRegionQosInstance>& LocalRegionOptions = GetRegionOptions();
	for (const FRegionQosInstance& Region : LocalRegionOptions)
	{
		if (Region.Definition.RegionId == RegionId)
		{
			Region.GetSubregionPreferences(OutSubregions);
			break;
		}
	}
}

const TArray<FRegionQosInstance>& UQosRegionManager::GetRegionOptions() const
{
	if (ForceRegionId.IsEmpty())
	{
		return RegionOptions;
	}

	static TArray<FRegionQosInstance> ForcedRegionOptions;
	ForcedRegionOptions.Empty(1);
	for (const FRegionQosInstance& RegionOption : RegionOptions)
	{
		if (RegionOption.Definition.RegionId == ForceRegionId)
		{
			ForcedRegionOptions.Add(RegionOption);
		}
	}
#if !UE_BUILD_SHIPPING
	if (ForcedRegionOptions.Num() == 0)
	{
		FRegionQosInstance FakeRegionInfo;
		FakeRegionInfo.Definition.DisplayName =	NSLOCTEXT("MMRegion", "DevRegion", "Development");
		FakeRegionInfo.Definition.RegionId = ForceRegionId;
		FakeRegionInfo.Definition.bEnabled = true;
		FakeRegionInfo.Definition.bVisible = true;
		FakeRegionInfo.Definition.bAutoAssignable = false;
		FDatacenterQosInstance FakeDatacenterInfo;
		FakeDatacenterInfo.Result = EQosDatacenterResult::Success;
		FakeDatacenterInfo.AvgPingMs = 0;
		FakeRegionInfo.DatacenterOptions.Add(MoveTemp(FakeDatacenterInfo));
		ForcedRegionOptions.Add(MoveTemp(FakeRegionInfo));
	}
#endif
	return ForcedRegionOptions;
}

void UQosRegionManager::ForceSelectRegion(const FString& InRegionId)
{
	if (!bRegionForcedViaCommandline)
	{
		QosEvalResult = EQosCompletionResult::Success;
		ForceRegionId = InRegionId.ToUpper();

		// make sure we can select this region
		if (!SetSelectedRegion(ForceRegionId, true))
		{
			UE_LOG(LogQos, Log, TEXT("Failed to force set region id %s"), *ForceRegionId);
			ForceRegionId.Empty();
		}
	}
	else
	{
		UE_LOG(LogQos, Log, TEXT("Forcing region %s skipped because commandline override used %s"), *InRegionId, *ForceRegionId);
	}
}

void UQosRegionManager::TrySetDefaultRegion()
{
	if (!IsRunningDedicatedServer())
	{
		// Try to set a default region if one hasn't already been selected
		if (!SetSelectedRegion(GetRegionId()))
		{
			FString BestRegionId = GetBestRegion();
			if (!SetSelectedRegion(BestRegionId))
			{
				UE_LOG(LogQos, Warning, TEXT("Unable to set a good region!"));
				UE_LOG(LogQos, Warning, TEXT("Wanted to set %s, failed to fall back to %s"), *GetRegionId(), *BestRegionId);
				DumpRegionStats();
			}
		}
	}
}

bool UQosRegionManager::IsUsableRegion(const FString& InRegionId) const
{
	const TArray<FRegionQosInstance>& LocalRegionOptions = GetRegionOptions();
	for (const FRegionQosInstance& RegionInfo : LocalRegionOptions)
	{
		if (RegionInfo.Definition.RegionId == InRegionId)
		{
			return RegionInfo.IsUsable();
		}
	}

	UE_LOG(LogQos, Log, TEXT("IsUsableRegion: failed to find region id %s"), *InRegionId);
	return false;
}

bool UQosRegionManager::SetSelectedRegion(const FString& InRegionId, bool bForce)
{
	// make sure we've enumerated
	if (bForce || QosEvalResult == EQosCompletionResult::Success)
	{
		// make sure it's in the option list
		FString RegionId = InRegionId.ToUpper();

		const TArray<FRegionQosInstance>& LocalRegionOptions = GetRegionOptions();
		for (const FRegionQosInstance& RegionInfo : LocalRegionOptions)
		{
			if (RegionInfo.Definition.RegionId == RegionId)
			{
				if (RegionInfo.IsUsable())
				{
					SelectedRegionId = RegionId;
					return true;
				}
				else
				{
					return false;
				}
			}
		}
	}

	// can't select a region not in the options list (NONE is special, it means pick best)
	if (!InRegionId.IsEmpty() && (InRegionId != NO_REGION))
	{
		UE_LOG(LogQos, Log, TEXT("SetSelectedRegion: failed to find region id %s"), *InRegionId);
	}
	return false;
}

void UQosRegionManager::ClearSelectedRegion()
{ 
	// Do not default to NO_REGION
	SelectedRegionId.Empty();
	if (!bRegionForcedViaCommandline)
	{
		ForceRegionId.Empty();
	}
}

bool UQosRegionManager::AllRegionsFound() const
{
	int32 NumDatacenters = 0;
	for (const FQosDatacenterInfo& Datacenter : DatacenterDefinitions)
	{
		if (Datacenter.IsPingable())
		{
			++NumDatacenters;
		}
	}

	int32 NumDatacentersWithGoodResponses = 0;
	for (const FRegionQosInstance& Region : RegionOptions)
	{
		for (const FDatacenterQosInstance& Datacenter : Region.DatacenterOptions)
		{
			const bool bGoodPercentage = (((float)Datacenter.NumResponses / (float)NumTestsPerRegion) >= 0.5f);
			NumDatacentersWithGoodResponses += bGoodPercentage ? 1 : 0;
		}
	}

	return (NumDatacenters > 0) && (NumDatacentersWithGoodResponses > 0) && (NumDatacenters == NumDatacentersWithGoodResponses);
}

void UQosRegionManager::SanityCheckDefinitions() const
{
	// Check data syntax
	for (const FQosRegionInfo& Region : RegionDefinitions)
	{
		UE_CLOG(!Region.IsValid(), LogQos, Warning, TEXT("Invalid QOS region entry!"));
	}

	// Check data syntax
	for (const FQosDatacenterInfo& Datacenter : DatacenterDefinitions)
	{
		UE_CLOG(!Datacenter.IsValid(), LogQos, Warning, TEXT("Invalid QOS datacenter entry!"));
	}

	// Every datacenter maps to a parent region
	for (const FQosDatacenterInfo& Datacenter : DatacenterDefinitions)
	{
		bool bFoundParentRegion = false;
		for (const FQosRegionInfo& Region : RegionDefinitions)
		{
			if (Datacenter.RegionId == Region.RegionId)
			{
				bFoundParentRegion = true;
				break;
			}
		}

		if (!bFoundParentRegion)
		{
			UE_LOG(LogQos, Warning, TEXT("Datacenter %s has undefined parent region %s"), *Datacenter.Id, *Datacenter.RegionId);
		}
	}

	// Regions with no available datacenters
	for (const FQosRegionInfo& Region : RegionDefinitions)
	{
		int32 NumDatacenters = 0;
		int32 NumPingableDatacenters = 0;
		for (const FQosDatacenterInfo& Datacenter : DatacenterDefinitions)
		{
			if (Datacenter.RegionId == Region.RegionId)
			{
				NumDatacenters++;
				if (Datacenter.IsPingable())
				{
					NumPingableDatacenters++;
				}
			}
		}

		if (NumDatacenters == 0)
		{
			UE_LOG(LogQos, Warning, TEXT("Region %s has no datacenters"), *Region.RegionId);
		}

		if (NumDatacenters > 0 && NumPingableDatacenters == 0)
		{
			UE_LOG(LogQos, Warning, TEXT("Region %s has %d datacenters, all disabled"), *Region.RegionId, NumDatacenters);
		}
	}

	// Every auto assignable region has at least one auto assignable datacenter
	int32 NumAutoAssignableRegions = 0;
	for (const FQosRegionInfo& Region : RegionDefinitions)
	{
		if (Region.IsAutoAssignable())
		{
			int32 NumPingableDatacenters = 0;
			for (const FQosDatacenterInfo& Datacenter : DatacenterDefinitions)
			{
				if (Datacenter.RegionId == Region.RegionId)
				{
					if (Datacenter.IsPingable())
					{
						NumPingableDatacenters++;
					}
				}
			}

			if (NumPingableDatacenters)
			{
				NumAutoAssignableRegions++;
			}

			UE_LOG(LogQos, Display, TEXT("AutoRegion %s: %d datacenters available"), *Region.RegionId, NumPingableDatacenters);
		}
	}

	// At least one region is auto assignable
	if (NumAutoAssignableRegions == 0)
	{
		UE_LOG(LogQos, Warning, TEXT("No auto assignable regions available!"));
	}
}

void UQosRegionManager::DumpRegionStats() const
{
	UE_LOG(LogQos, Display, TEXT("Region Info:"));
	UE_LOG(LogQos, Display, TEXT("Current: %s "), *SelectedRegionId);
	if (!ForceRegionId.IsEmpty())
	{
		UE_LOG(LogQos, Display, TEXT("Forced: %s "), *ForceRegionId);
	}

	TMultiMap<FString, const FQosDatacenterInfo*> DatacentersByRegion;
	for (const FQosDatacenterInfo& DatacenterDef : DatacenterDefinitions)
	{
		DatacentersByRegion.Emplace(DatacenterDef.RegionId, &DatacenterDef);
	}

	TMap<FString, const FRegionQosInstance* const> RegionInstanceByRegion;
	for (const FRegionQosInstance& Region : RegionOptions)
	{
		RegionInstanceByRegion.Emplace(Region.Definition.RegionId, &Region);
	}

	// Look at real region options here
	UE_LOG(LogQos, Display, TEXT("Definitions:"));
	for (const FQosRegionInfo& RegionDef : RegionDefinitions)
	{
		const FRegionQosInstance* const* RegionInst = RegionInstanceByRegion.Find(RegionDef.RegionId);

		TArray<const FQosDatacenterInfo*> OutValues;
		DatacentersByRegion.MultiFind(RegionDef.RegionId, OutValues);

		UE_LOG(LogQos, Display, TEXT("\tRegion: %s [%s] (%d datacenters)"), *RegionDef.DisplayName.ToString(), *RegionDef.RegionId, OutValues.Num());
		UE_LOG(LogQos, Display, TEXT("\t Enabled: %d Visible: %d Beta: %d"), RegionDef.bEnabled, RegionDef.bVisible, RegionDef.bAutoAssignable);

		TSet<FString> FoundSubregions;
		if (RegionInst)
		{
			for (const FDatacenterQosInstance& Datacenter : (*RegionInst)->DatacenterOptions)
			{
				for (const FQosDatacenterInfo* DatacenterDef : OutValues)
				{
					if (DatacenterDef->Id == Datacenter.Definition.Id)
					{
						FoundSubregions.Add(DatacenterDef->Id);
						float ResponsePercent = (static_cast<float>(Datacenter.NumResponses) / static_cast<float>(NumTestsPerRegion)) * 100.0f;
						UE_LOG(LogQos, Display, TEXT("\t  Datacenter: %s%s %dms (%0.2f%%) %s"), 
							*DatacenterDef->Id, !DatacenterDef->bEnabled ? TEXT(" Disabled") : TEXT(""),
							Datacenter.AvgPingMs, ResponsePercent, ToString(Datacenter.Result)
						);
						break;
					}
				}
			}
		}

		for (const FQosDatacenterInfo* DatacenterDef : OutValues)
		{
			UE_CLOG(!FoundSubregions.Contains(DatacenterDef->Id), LogQos, Display, TEXT("\t  Datacenter: %s%s"), *DatacenterDef->Id, !DatacenterDef->bEnabled ? TEXT(" Disabled") : TEXT(""));
		}

		if (!RegionInst)
		{
			UE_LOG(LogQos, Display, TEXT("No instances for region"));
		}
	}

	UE_LOG(LogQos, Display, TEXT("Results: %s"), ToString(QosEvalResult));

	SanityCheckDefinitions();
}

void UQosRegionManager::RegisterQoSSettingsChangedDelegate(const FSimpleDelegate& OnQoSSettingsChanged)
{
	// add to the completion delegate
	OnQoSSettingsChangedDelegate = OnQoSSettingsChanged;
}
