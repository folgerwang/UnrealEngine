// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Party/PartyTypes.h"
#include "UObject/GCObject.h"
#include "Containers/Ticker.h"
#include "Interfaces/OnlinePartyInterface.h"

/** Util exclusively for use by TPartyDataReplicator to circumvent circular include header issues */
class FPartyDataReplicatorHelper
{
	template <typename> friend class TPartyDataReplicator;
	PARTY_API static void ReplicateDataToMembers(const FOnlinePartyRepDataBase& RepDataInstance, const UScriptStruct& RepDataType, const FOnlinePartyData& ReplicationPayload);
};

/** Base util class for dealing with data that is replicated to party members */
template <typename RepDataT>
class TPartyDataReplicator : public FGCObject
{
	static_assert(TIsDerivedFrom<RepDataT, FOnlinePartyRepDataBase>::IsDerived, "TPartyDataReplicator is only intended to function with FOnlinePartyRepDataBase types.");

public:
	~TPartyDataReplicator()
	{
		Reset();
	}

	const RepDataT* operator->() const { check(RepDataPtr); return RepDataPtr; }
	RepDataT* operator->() { check(RepDataPtr); return RepDataPtr; }
	const RepDataT& operator*() const { return *RepDataPtr; }
	RepDataT& operator*() { return *RepDataPtr; }

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(RepDataType);
	}

	bool IsValid() const { return RepDataType && RepDataPtr && RepDataCopy; }
	
	template <typename ChildRepDataT>
	void EstablishRepDataInstance(ChildRepDataT& RepDataInstance)
	{
		static_assert(TIsDerivedFrom<ChildRepDataT, RepDataT>::IsDerived, "Incompatible RepData child struct type");

		static_cast<FOnlinePartyRepDataBase*>(&RepDataInstance)->OnDataChanged.BindRaw(this, &TPartyDataReplicator::HandleRepDataChanged);

		RepDataPtr = &RepDataInstance;
		RepDataType = ChildRepDataT::StaticStruct();

		RepDataCopy = (ChildRepDataT*)FMemory::Malloc(RepDataType->GetCppStructOps()->GetSize());
		RepDataType->GetCppStructOps()->Construct(RepDataCopy);
	}

PACKAGE_SCOPE:
	void ProcessReceivedData(const FOnlinePartyData& IncomingPartyData, bool bCompareToPrevious = true)
	{
		if (FVariantDataConverter::VariantMapToUStruct(IncomingPartyData.GetKeyValAttrs(), RepDataType, RepDataPtr, 0, CPF_Transient | CPF_RepSkip))
		{
			static_cast<FOnlinePartyRepDataBase*>(RepDataPtr)->PostReplication();

			if (bCompareToPrevious)
			{
				static_cast<FOnlinePartyRepDataBase*>(RepDataPtr)->CompareAgainst(*RepDataCopy);
			}
			ensure(RepDataType->GetCppStructOps()->Copy(RepDataCopy, RepDataPtr, 1));
		}
		else
		{
			UE_LOG(LogParty, Error, TEXT("Failed to serialize received party data!"));
		}
	}

	void Reset()
	{
		if (RepDataPtr)
		{
			static_cast<FOnlinePartyRepDataBase*>(RepDataPtr)->OnDataChanged.Unbind();
			RepDataPtr = nullptr;
		}
		if (RepDataType && RepDataCopy)
		{
			RepDataType->GetCppStructOps()->Destruct(RepDataCopy);
			FMemory::Free(RepDataCopy);
			RepDataCopy = nullptr;
			RepDataType = nullptr;
		}
		if (UpdateTickerHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(UpdateTickerHandle);
			UpdateTickerHandle.Reset();
		}
	}

private:
	void HandleRepDataChanged()
	{
		if (!UpdateTickerHandle.IsValid())
		{
			UpdateTickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &TPartyDataReplicator::DeferredHandleReplicateChanges));
		}
	}

	bool DeferredHandleReplicateChanges(float)
	{
		UpdateTickerHandle.Reset();

		FOnlinePartyData OnlinePartyData;
		if (FVariantDataConverter::UStructToVariantMap(RepDataType, RepDataPtr, OnlinePartyData.GetKeyValAttrs(), 0, CPF_Transient | CPF_RepSkip))
		{
			FPartyDataReplicatorHelper::ReplicateDataToMembers(*RepDataPtr, *RepDataType, OnlinePartyData);
		}
		return false;
	}

	/** Reflection data for child USTRUCT */
	const UScriptStruct* RepDataType = nullptr;

	/**
	 * Pointer to child UStruct that holds the current state of the party. Only modifiable by party leader.
	 * To establish a custom state struct, call EstablishPartyState<T> with the desired type within the child class's constructor
	 */
	RepDataT* RepDataPtr = nullptr;

	/** Scratch copy of child UStruct for handling replication comparisons */
	RepDataT* RepDataCopy = nullptr;

	FDelegateHandle UpdateTickerHandle;
};