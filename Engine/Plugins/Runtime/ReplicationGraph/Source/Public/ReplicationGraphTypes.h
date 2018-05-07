// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Package.h"
#include "EngineGlobals.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectKey.h"
#include "UObject/UObjectHash.h"

class AActor;
class UNetConnection;
class UNetReplicationGraphConnection;
class UReplicationGraph;

struct FActorDestructionInfo;

DECLARE_LOG_CATEGORY_EXTERN( LogReplicationGraph, Log, All );

// Check aliases for within the system. The intention is that these can be flipped to checkSlow once the system is stable.

// Macro enable extra logging/bookkeeping. Disabled in Test/Shipping for perf.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	#define REPGRAPH_DETAILS 1
	#define DO_REPGRAPH_DETAILS(X) (X)
	#define repCheck(x) check(x)
	#define repCheckf(expr, format, ...) checkf(expr, format, ##__VA_ARGS__ )
	#define RG_QUICK_SCOPE_CYCLE_COUNTER(x) QUICK_SCOPE_CYCLE_COUNTER(x)
	extern int32 CVar_RepGraph_Verify;
#else 
	#define REPGRAPH_DETAILS 0
	#define DO_REPGRAPH_DETAILS(X) 0
	#define repCheck(x)
	#define repCheckf(expr, format, ...)
	#define RG_QUICK_SCOPE_CYCLE_COUNTER(X)
#endif


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)	
#define REPGRAPH_STR(x) #x
#define REPGRAPH_DEVCVAR_SHIPCONST(Type,VarName,Var,Value,Help) \
	Type Var = Value; \
	static FAutoConsoleVariableRef Var##CVar(TEXT(VarName), Var, TEXT(Help), ECVF_Cheat );
#else
#define REPGRAPH_DEVCVAR_SHIPCONST(Type,VarName,Var,Value,Help) \
	const Type Var = Value;
#endif



// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// Replication List Category
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// 
// ReplicationListCategory is just a bit in a uin32. All FReplicationListCategory must be power of 2 values so that they can be unioned in an FReplicationListCategoryMask.
// This implementation was chosen to allow projects to be able to fully extend/customize categories while providing some type safety/clarity in code.
//
// The important high level concepts around categories are:
//	1) They provide a way to broadly cull replication lists. Each list stored in the Replication Graph has a category associated with it. When gathering lists,
//		the system will filter out categories that are not requested by the packet budget.
//	2) Once we have the culled lists, categories are what the FPacketBudgets are specified in terms of.  That is, as we are filling up the packet, we using
//		categories to determine how much space a list can occupy.


// Enables run time ensure that FReplicationListCategory are initialized with power of 2 values
#define REP_CAT_ENSURE_POW2 !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

struct REPLICATIONGRAPH_API FReplicationListCategory
{
	FReplicationListCategory() : Value(InvalidListCategory)	{ }
	FReplicationListCategory(uint32 In): Value(In)
	{ 
#if REP_CAT_ENSURE_POW2
		ensure(FMath::IsPowerOfTwo(Value));
#endif
	}
	enum { InvalidListCategory = 0 };
	operator uint32 () const { return Value; }
	friend uint32 GetTypeHash(const FReplicationListCategory& ListCategory) { return GetTypeHash(ListCategory.Value); };
	FORCEINLINE void Reset() { Value = InvalidListCategory; }
	
	FString GetDebugStringSlow() const;
	
	// Static methods for setting the string mapping function
	typedef TFunction<FString (FReplicationListCategory)> FGetDebugStringFunc;
	static void SetDebugStringFunc(FGetDebugStringFunc Func);
	static void SetDebugStringFunc_StandardEnum(TCHAR* EnumString);

protected:

	uint32 Value;
	static FGetDebugStringFunc CustomDebugStringFunc;

	friend struct FReplicationListCategoryMask;
};

// Represents a mask or union of FReplicationListCategories. The purpose of this is to make clear what places are dealing with unions of categories vs which are expecting a single category
struct REPLICATIONGRAPH_API FReplicationListCategoryMask : public FReplicationListCategory
{
	FReplicationListCategoryMask() : FReplicationListCategory() { }
	FReplicationListCategoryMask(uint32 In) { Value = In; }

	FReplicationListCategoryMask operator|(FReplicationListCategory const& Rhs) const { FReplicationListCategoryMask Result(*this); Result |= Rhs; return Result; }
	FReplicationListCategoryMask operator&(FReplicationListCategory const& Rhs) const { FReplicationListCategoryMask Result(*this); Result &= Rhs; return Result; }

	FReplicationListCategoryMask const& operator|=(FReplicationListCategory const& Rhs) { Value |= Rhs.Value; return *this; }
	FReplicationListCategoryMask const& operator&=(FReplicationListCategory const& Rhs) { Value &= Rhs.Value; return *this; }
};

// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// Actor Replication List Types
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------

// Currently we are using raw AActor* for our replication lists. We may want to change this one day to be an ID or something else that can index
// fast into arrays etc. (Currently we are using TMaps for associative data, static arrays would be faster but introduce constraints and headaches)
// So for now, using a typedef and some helper functions to call out the interface/usage of FActorRepListType.

typedef AActor* FActorRepListType;
FORCEINLINE FString GetActorRepListTypeDebugString(const FActorRepListType& In) { return GetNameSafe(In); }
FORCEINLINE UClass* GetActorRepListTypeClass(const FActorRepListType& In) { return In->GetClass(); }


// Tests if an actor is valid for replication: not pending kill, etc. Says nothing about wanting to replicate or should replicate, etc.
FORCEINLINE bool IsActorValidForReplication(const FActorRepListType& In) { return !In->IsPendingKill() && !In->IsPendingKillPending(); }

// Tests if an actor is valid for replication gathering. Meaning, it can be gathered from the replication graph and considered for replication.
FORCEINLINE bool IsActorValidForReplicationGather(const FActorRepListType& In)
{ 
	if (!IsActorValidForReplication(In))
		return false;

	if (In->GetIsReplicated() == false)
		return false;

	if (In->GetTearOff())
		return false;

	if (In->NetDormancy == DORM_Initial && In->IsNetStartupActor())
		return false;

/*
	These checks were done in legacy code and we would like to avoid them.

	// Actors should finish initialization outside of the replication loop. Maybe some weird multi frame delayed case?
	if (!In->IsActorInitialized())
		return false;

	// This check is slow and is not needed unless you are streaming levels on the server. If needed this should be opt in globally some how.
	ULevel* Level = In->GetLevel();
	if ( Level->HasVisibilityChangeRequestPending() || Level->bIsAssociatingLevel )
		return false;

*/

	return true;
}

/** The actual underlying list type that the system uses for a list of replicated actors. These are never manually allocated or even really used. All Replication code interacts with the "View" types below  */
struct REPLICATIONGRAPH_API FActorRepList : FNoncopyable
{
	/** List Header */
	int32 RefCount;
	int32 Max;
	int32 Num;

	/** the "used bit" from the block we came from, so that we can clear it fast */
	FBitReference UsedBitRef;

	/** Variable length Data segment */
	FActorRepListType Data[];

	/** For TRefCountPtr usage */
	void AddRef() { RefCount++; }
	void Release();
};

/** This is a base templated type for the list "Views". This provides basic read only access between the two real views. */
template<typename PointerType>
struct TActorRepListViewBase
{
	TActorRepListViewBase() { }
	TActorRepListViewBase(const PointerType& In) : RepList(In) { }

	FORCEINLINE int32 Num() const { return RepList->Num; }
	FORCEINLINE const FActorRepListType& operator[](int32 idx) const  { repCheck(RepList); repCheck(RepList->Max > idx); return RepList->Data[idx]; }

	/** Resets the view to null - meaning it is not pointing to any list (as opposed to an empty list of some preallocated size) */
	FORCEINLINE void ResetToNull() { RepList = nullptr; }
	FORCEINLINE bool IsValid() const { return RepList != nullptr; }

	int32 IndexOf(const FActorRepListType& Value) const
	{
		repCheck(RepList);
		FActorRepListType* Data = RepList->Data;
		int32 Num = RepList->Num;
		for (int32 idx=0 ; idx < Num; ++idx)
		{
			if (Data[idx] == Value)
			{
				return idx;
			}
		}
		return -1;
	}

	bool Contains(const FActorRepListType& Value) const { return IndexOf(Value) != -1; }

	/** Add contents to TArray. this is intended for debugging/ease of use */
	void AppendToTArray(TArray<FActorRepListType>& OutArray) const
	{
		if (IsValid())
		{
			for (FActorRepListType Actor : *this)
			{
				OutArray.Add(Actor);
			}
		}
	}

	void AppendToTSet(TSet<FActorRepListType>& OutSet) const
	{
		if (IsValid())
		{
			for (FActorRepListType Actor : *this)
			{
				OutSet.Add(Actor);
			}
		}
	}
	
	PointerType RepList;

private:

	FORCEINLINE friend FActorRepListType* begin(const TActorRepListViewBase<PointerType>& View) { return View.RepList->Data; }
	FORCEINLINE friend FActorRepListType* end(const TActorRepListViewBase<PointerType>& View) { return View.RepList->Data + View.RepList->Num; }
};

/** A view that maintains ownership/(ref counting) to an actor replication list.  */
struct REPLICATIONGRAPH_API FActorRepListRefView : public TActorRepListViewBase<TRefCountPtr<FActorRepList>>
{
	/** Ideally, use Reset to set the initial size of the list. But if nothing is set, the first list we request will be of this size */
	enum { InitialListSize = 4 } ;

	FActorRepListRefView() { }
	FActorRepListRefView(FActorRepList& InRepList) { RepList = &InRepList; }
	FORCEINLINE FActorRepListType& operator[](int32 idx) const  { repCheck(RepList); repCheck(RepList->Max > idx); return RepList->Data[idx]; }

	/** Initializes a new list for a given ExpectedMaxSize. Best practice is to call this once to get a good initial size to avoid reallocations/copying. Passing 0 will preserve the current size (if current size is also 0, InitialListSize is used) */
	void Reset(int32 ExpectedMaxSize=0)
	{
		if (RepList && RepList->RefCount == 1 && RepList->Max >= ExpectedMaxSize)
		{
			// We can keep using this list
			RepList->Num = 0;
			CachedNum = 0;
		}
		else
		{
			// We must request a new list for this size
			RequestNewList(ExpectedMaxSize > 0 ? ExpectedMaxSize : CachedNum, false);
		}
	}

	/** Prepares the list for modifications. If this list is shared by other RefViews, then you will get a new underlying list to reference. ResetContent determines if the content is cleared or not (regardless of refcount/new list) */
	void PrepareForWrite(bool bResetContent=false)
	{
		if (RepList == nullptr)
		{
			RequestNewList(InitialListSize, false);
		}
		else if (RepList->RefCount > 1)
		{
			// This list we are viewing is shared by others, so request a new one
			RequestNewList(CachedNum, !bResetContent);
		}
		else if(bResetContent)
		{
			// We already have our own list but need to reset back to 0
			RepList->Num = 0;
			CachedNum = 0;
		}
	}

	bool ConditionalAdd(const FActorRepListType& NewElement)
	{
		if (IsActorValidForReplicationGather(NewElement))
		{
			Add(NewElement);
			return true;
		}
		return false;
	}

	void Add(const FActorRepListType& NewElement)
	{
		repCheckf(RepList != nullptr, TEXT("Invalid RepList when calling add new element to a list. Call ::PrepareForWrite or ::Reset before writing!"));
		repCheckf(RepList->RefCount == 1, TEXT("Attempting to add new element to a list with >1 RefCount (%d). Call ::PrepareForWrite before writing!"), RepList->RefCount);
		if (CachedNum == CachedMax)
		{
			// We can't add more to the list we are referencing, we need to get a new list and copy the contents over. This is transparent to the caller.
			RequestNewList(CachedMax+1, true);
		}
		
		CachedData[CachedNum++] = NewElement;
		RepList->Num++;
	}

	bool Remove(const FActorRepListType& ElementToRemove)
	{
		int32 idx = IndexOf(ElementToRemove);
		if (idx >= 0)
		{
			RemoveAtImpl(idx);
			return true;
		}
		return false;
	}

	void RemoveAtSwap(int32 idx)
	{
		repCheck(RepList && Num() > idx);
		CachedData[idx] = CachedData[CachedNum-1];
		CachedNum--;
		RepList->Num--;
	}

	void CopyContentsFrom(const FActorRepListRefView& Source);
	bool VerifyContents_Slow() const;

	int32 Num() const { return CachedNum; }

private:

	/** Cached data from our FActorRepList to avoid looking it up each time */
	FActorRepListType* CachedData = nullptr;
	int32 CachedNum  = 0;
	int32 CachedMax = 0;

	void RequestNewList(int32 NewSize, bool bCopyExistingContent);

	void RemoveAtImpl(int32 Index)
	{
		repCheck(RepList && Num() > Index);
		int32 NumToMove = CachedNum - Index - 1;
		if (NumToMove)
		{
			FMemory::Memmove((void*)(&CachedData[Index]), (void*)(&CachedData[Index + 1]), NumToMove * sizeof(FActorRepListType));
		}

		CachedNum--;
		RepList->Num--;
	}
};

/** A read only, non owning (ref counting) view to an actor replication list: essentially a raw pointer and the category of the list. These are only created *from* FActorRepListRefView */
struct REPLICATIONGRAPH_API FActorRepListRawView : public TActorRepListViewBase<FActorRepList*>
{
	/** Standard ctor: make raw view from ref view */
	FActorRepListRawView(const FActorRepListRefView& Source, FReplicationListCategory InCategory) : Category(InCategory) { RepList = Source.RepList.GetReference(); }
	
	/** Sublist cstor: make raw view from portion of a ref view */
	FActorRepListRawView(const FActorRepListRefView& Source, FReplicationListCategory InCategory, int32 StartIdx, int32 Num) : Category(InCategory)
	{

	}
	
	FActorRepListRefView ToRefView() const { return FActorRepListRefView(*RepList); }

	FReplicationListCategory Category;
};

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Intended to be called from immediate mode window while debugging
	extern "C" DLLEXPORT void PrintRepListDetails(int32 PoolSize, int32 BlockIdx, int32 ListIdx);
	extern "C" DLLEXPORT void PrintRepListStats(int32 mode=0);
	
	void PrintRepListStatsAr(int32 mode, FOutputDevice& Ar=*GLog);	
#endif

/** To be called by projets to preallocate replication lists. This isn't strictly necessary: lists will be allocated on demand as well. */
REPLICATIONGRAPH_API void PreAllocateRepList(int32 ListSize, int32 NumLists);


// This represents "the list of gathered lists". This is what we push down the Replication Graph and nodes will either Push/Pop List Categories or will add their Replication Lists.
struct REPLICATIONGRAPH_API FGatheredReplicationActorLists
{
	void AddReplicationActorList(const FActorRepListRefView& List)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!ensureMsgf(CurrentListCategory, TEXT("FGatheredReplicationActorLists Attemping to add a ReplicationActorList with invalid ListCategory")))
			return;

		if (CVar_RepGraph_Verify)
			List.VerifyContents_Slow();
#endif
		repCheck(List.IsValid());
		if (List.Num() > 0)
		{
			OutReplicationLists.Emplace(FActorRepListRawView(List, CurrentListCategory)); 
		}
	}

	void Reset() { OutReplicationLists.Reset(); ListCategoryStack.Reset(); }
	
	void PushListCategory(FReplicationListCategory ListCategory) { ListCategoryStack.Push(CurrentListCategory); CurrentListCategory = ListCategory; }
	void PopListCategory() { CurrentListCategory = ListCategoryStack.Last(); ListCategoryStack.Pop(); }	

	FORCEINLINE int32 Num() const { return OutReplicationLists.Num(); }
	FORCEINLINE const FActorRepListRawView& operator[](int32 idx) const  { return OutReplicationLists[idx]; }
	
	FORCEINLINE friend FActorRepListRawView* begin(FGatheredReplicationActorLists& Array) { return Array.OutReplicationLists.GetData(); }
	FORCEINLINE friend FActorRepListRawView* end(FGatheredReplicationActorLists& Array) { return Array.OutReplicationLists.GetData() + Array.OutReplicationLists.Num(); }

private:	

	TArray< FActorRepListRawView > OutReplicationLists;
	FReplicationListCategory CurrentListCategory;
	TArray< FReplicationListCategory, TInlineAllocator<1> > ListCategoryStack;
};

// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// Packet Budget
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------

// Packet Budget represents the desired makeup of a replication packet. It maps ListCategories to the desired amount of data, It is the const data that we look at while building packets.
struct FPacketBudget
{
	/** What happens when a budget item ends while being under/over the target budget */
	enum EBalancePolicy
	{
		/** The balance is absorbed by this item. */
		Absorb,
		/** The balance rolls over to the next item (e.g, if > 0, the next item gets more bits, if < 0, the next item gets less bits). Note that a budget item cannot be completely pushed out - it will replicate at least once! */
		RollOver
	};

	struct FItem
	{
		FItem(FReplicationListCategoryMask InListCategory, int64 InMaxBits, EBalancePolicy InUnderBudgetPolicy, EBalancePolicy InOverBudgetPolicy) 
			: ListCategory(InListCategory), MaxBits(InMaxBits), UnderBudgetPolicy(InUnderBudgetPolicy), OverBudgetPolicy(InOverBudgetPolicy)  { }

		FReplicationListCategoryMask ListCategory;
		int64 MaxBits = 0;
		EBalancePolicy UnderBudgetPolicy;
		EBalancePolicy OverBudgetPolicy;
	};

	/** The actual list of items that make up this packet budget */
	TArray<FItem> BudgetItems;

	/** Combined mask for all categories this packet budget cares about: used for broad filtering during gather phase */
	FReplicationListCategoryMask ListCategoryMask;

	/** Identifying string for debug/logging only */
	const TCHAR* DebugName = nullptr;

	/** Adds a budget item for a given category. */
	void AddBudgetForCategory(FReplicationListCategoryMask Category, int64 MaxBits, EBalancePolicy UnderBudgetPolicy = EBalancePolicy::Absorb, EBalancePolicy OverBudgetPolicy = EBalancePolicy::Absorb)
	{
		BudgetItems.Emplace(FItem(Category, MaxBits, UnderBudgetPolicy, OverBudgetPolicy));
	}

	/** Finalizes the budget: call this (once) after finished calling AddBudgetForCategory (as many times as you need) */
	void Finalize()
	{
		ListCategoryMask = FReplicationListCategoryMask();
		for (auto& Item : BudgetItems)
		{
			ListCategoryMask |= Item.ListCategory;
		}

		BudgetItems.Sort([](const FItem& ItemA, const FItem& ItemB) { return ItemA.ListCategory < ItemB.ListCategory; } );
	}
};

// Represents a filled budget item during an actual frame. These are produced by FPacketBudgetTracker and mainly used for logging/profiling etc.
struct FPacketBudgetRecord
{
	struct FItem
	{
		int64 BitsWritten = 0;
	};

	void SetBudget(const FPacketBudget* InBudget) { Budget = InBudget; Items.Reset(); Items.SetNum(InBudget->BudgetItems.Num()); }
	void TrackSentBits(int64 NumBits, int32 BudgetIdx) {	Items[BudgetIdx].BitsWritten += NumBits; }
	void TrackActorChannelCloseBits(int64 NumBits) { ActorChannelCloseBits += NumBits; }
	void TrackDestructionInfoBits(int64 NumBits) { DestructionInfoBits += NumBits; }
	
	const FPacketBudget* Budget = nullptr;
	TArray<FItem, TInlineAllocator<8> >	Items;
	int64 ActorChannelCloseBits = 0;
	int64 DestructionInfoBits = 0;
};

// The tracker is what actually does the book keeping between a given FPacketBudget and the data that is pushed into packets. Essentially, the runtime/dynamic data as we are replicating actors.
struct FPacketBudgetTracker
{
	FPacketBudgetTracker(const FPacketBudget& InBudget) : Budget(InBudget), CurrentIdx(0), RemainingBits(0) { }
	
	/** Sets the current index of the FPacketBudget::FItem that we are filling (this is an index into this->Budget.BudgetItems) */
	void SetBudgetItemIndex(int32 Index)
	{
		if ((RemainingBits < 0 && Budget.BudgetItems[CurrentIdx].OverBudgetPolicy == FPacketBudget::RollOver) || (Budget.BudgetItems[CurrentIdx].UnderBudgetPolicy == FPacketBudget::RollOver))
		{
			// Rollover the balance (note, don't check RemainingBits > 0 because that is an extra banch and effectively the exact same)
			RemainingBits += Budget.BudgetItems[Index].MaxBits;
		}
		else
		{
			// Absorb the balance
			RemainingBits = Budget.BudgetItems[Index].MaxBits;
		}

		CurrentIdx = Index;
	}
	
	/** Called as bits are written to the send buffer. Returns true if the current budget item is not exceeded. (note: technically they are not "sent" yet, not until the net driver's send buffer is flushed) */
	bool TrackSentBits(int64 NumBits)
	{
		if (OutRecord)
		{
			OutRecord->TrackSentBits(NumBits, CurrentIdx);
		}

		RemainingBits -= NumBits;
		return (RemainingBits > 0);
	}

	void TrackActorChannelCloseBits(int64 NumBits) { if (OutRecord) OutRecord->TrackActorChannelCloseBits(NumBits); }
	void TrackDestructionInfoBits(int64 NumBits) { if (OutRecord) OutRecord->TrackDestructionInfoBits(NumBits); }

	/** Optional output: useful for logging/debugging etc */
	void SetOutputRecord(struct FPacketBudgetRecord* NewOutRecord) { OutRecord = NewOutRecord; }
	
private:

	const FPacketBudget& Budget;
	FReplicationListCategory CurrentListCategory;
	int32 CurrentIdx;
	int64 RemainingBits;

	FPacketBudgetRecord* OutRecord = nullptr;
};


struct FPacketBudgetRecordBuffer
{
	const int32 BufferSize = 64;
	TArray<FPacketBudgetRecord, TInlineAllocator<64> > Records;
	uint32 NextWriteIdx = 0;

	FPacketBudgetRecordBuffer()
	{
		Records.SetNum(BufferSize);
	}

	FPacketBudgetRecord* GetNextWriteRecord(const FPacketBudget& ForBudget)
	{
		FPacketBudgetRecord* Record = &Records[NextWriteIdx % BufferSize];
		Record->SetBudget(&ForBudget);
		NextWriteIdx++;
		return Record;
	}

	int32 Num() const { return FMath::Min<uint32>(BufferSize, NextWriteIdx); }
	FPacketBudgetRecord& GetAtIndex(int32 idx) { return Records[(NextWriteIdx - Num() + idx) % Records.Num()]; }
	void Reset() { Records.Reset(); Records.SetNum(BufferSize); NextWriteIdx = 0; }
};

// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// Connection Gather Actor List Parameters
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------

// Parameter structure for what we actually pass down during the Gather phase.
struct FConnectionGatherActorListParameters
{
	FConnectionGatherActorListParameters(FNetViewer& InViewer, UNetReplicationGraphConnection& InConnectionManager, TSet<FName>& InClientVisibleLevelNamesRef, FPacketBudget& InPacketBudget, uint32 InReplicationFrameNum, FGatheredReplicationActorLists& InOutGatheredReplicationLists)
		: Viewer(InViewer), ConnectionManager(InConnectionManager), PacketBudget(InPacketBudget), ReplicationFrameNum(InReplicationFrameNum), OutGatheredReplicationLists(InOutGatheredReplicationLists), ClientVisibleLevelNamesRef(InClientVisibleLevelNamesRef)
	{
	}

	/** In: The Data the nodes have to work with */
	FNetViewer& Viewer;
	UNetReplicationGraphConnection& ConnectionManager;
	FPacketBudget& PacketBudget;
	uint32 ReplicationFrameNum;

	/** Out: The data nodes are going to add to */
	FGatheredReplicationActorLists& OutGatheredReplicationLists;

	/** Attempts to push the ListCategory, returns true if the list category is currently allowed. If this returns true, you are expected to call PopListCategory when you are done! */
	bool AttemptPushListCategory(FReplicationListCategory ListCategory) const
	{
		if ((PacketBudget.ListCategoryMask & ListCategory))
		{
			OutGatheredReplicationLists.PushListCategory(ListCategory);
			return true;
		}
		return false;
	}

	/** Pop the list category that was pushed */
	void PopListCategory() const
	{
		OutGatheredReplicationLists.PopListCategory();
	}

	bool CheckClientVisibilityForLevel(const FName& StreamingLevelName) const
	{
		// We track the last checked StreamingLevelName that was visible. This is a simple optimization under the premise that in an ideal replication graph,
		// a connection is only pulling from nodes with 1 conditional streaming level. (E.g, even if you map has 300 streaming levels, spatially the client should
		// only be "over" one of those levels, and should only attempt to pull from nodes that have that one level in its conditional actor lists.
		// 
		// Really this is still left over from legacy implementation. I would rather completely skip nodes in the spatialization graph based on level streaming status,
		// but the way that level streaming works, there is no mechanism or guarantees about what space a streaming level takes up. This is ok for now.
		if (StreamingLevelName == LastCheckedVisibleLevelName)
		{
			return true;
		}

		const bool bVisible = ClientVisibleLevelNamesRef.Contains(StreamingLevelName);
		if (bVisible)
		{
			LastCheckedVisibleLevelName = StreamingLevelName;
		}
		return bVisible;
	}



	// Cached off reference for fast Level Visibility lookup
	TSet<FName>& ClientVisibleLevelNamesRef;

private:

	mutable FName LastCheckedVisibleLevelName;
};

// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// Per Class/Actor Global/PerConnectino Data Structs
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------

/** Per-Class actor data about how the actor replicates */
struct FClassReplicationInfo
{
	FClassReplicationInfo() { }
	float DistancePriorityScale = 1.f;
	float StarvationPriorityScale = 1.f;
	float CullDistanceSquared = 0.f;
	
	uint32 ReplicationPeriodFrame = 1;
	uint32 ActorChannelFrameTimeout = 4;

	FString BuildDebugStringDelta() const
	{
		FClassReplicationInfo DefaultValues;
		FString Str;
		if (CullDistanceSquared != DefaultValues.CullDistanceSquared)
		{
			Str += FString::Printf(TEXT("CullDistance: %.2f "), FMath::Sqrt(CullDistanceSquared));
		}
		if (StarvationPriorityScale != DefaultValues.StarvationPriorityScale)
		{
			Str += FString::Printf(TEXT("StarvationPriorityScale: %.2f "), StarvationPriorityScale);
		}
		if (DistancePriorityScale != DefaultValues.DistancePriorityScale)
		{
			Str += FString::Printf(TEXT("DistancePriorityScale: %.2f "), DistancePriorityScale);
		}
		if (ReplicationPeriodFrame != DefaultValues.ReplicationPeriodFrame)
		{
			Str += FString::Printf(TEXT("ReplicationPeriodFrame: %d "), ReplicationPeriodFrame);
		}
		if (ActorChannelFrameTimeout != DefaultValues.ActorChannelFrameTimeout)
		{
			Str += FString::Printf(TEXT("ActorChannelFrameTimeout: %d "), ActorChannelFrameTimeout);
		}
		return Str;
	}
};

struct FGlobalActorReplicationInfo;

DECLARE_MULTICAST_DELEGATE_FourParams(FNotifyActorChangeDormancy, FActorRepListType, FGlobalActorReplicationInfo&, ENetDormancy /*NewVlue*/, ENetDormancy /*OldValue*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FNotifyActorFlushDormancy, FActorRepListType, FGlobalActorReplicationInfo&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FNotifyActorForceNetUpdate, FActorRepListType, FGlobalActorReplicationInfo&);

struct FGlobalActorReplicationEvents
{
	FNotifyActorChangeDormancy	DormancyChange;
	FNotifyActorFlushDormancy	DormancyFlush; // This delegate is cleared after broadcasting
	FNotifyActorForceNetUpdate	ForceNetUpdate;
};

/** Per-Actor data that is global for the entire Replication Graph */
struct FGlobalActorReplicationInfo
{
	FGlobalActorReplicationInfo(FClassReplicationInfo& ClassInfo) 
		: LastPreReplicationFrame(0), WorldLocation(ForceInitToZero), Settings(ClassInfo) { }

	// -----------------------------------------------------------
	//	Dynamic state
	// -----------------------------------------------------------

	/** The last time AActor::PreReplication was called. Used to track when we need to call it again. */
	uint32 LastPreReplicationFrame = 0;

	/** the last time game code called ForceNetUpdate on this actor */
	uint32 ForceNetUpdateFrame = 0;

	/** Cached World Location of the actor */
	FVector WorldLocation;

	/** Mirrors AActor::NetDormany > DORM_Awake */
	bool bWantsToBeDormant = false;

	/** When this actor replicates, we replicate these actors immediately afterwards (they are not gathered/prioritized/etc) */
	FActorRepListRefView DependentActorList;

	// -----------------------------------------------------------
	//	Class default mirrors: state that is initialized directly from class defaults (and can be later changed on a per-actor basis).
	// -----------------------------------------------------------

	FClassReplicationInfo Settings;

	// -----------------------------------------------------------
	//	Events
	// -----------------------------------------------------------
	FGlobalActorReplicationEvents Events;

	void LogDebugString(FOutputDevice& Ar) const;
};

/** Templatd struct for mapping UClasses to some data type. The main things this provides is that if a UClass* was not explicitly added, it will climb the class heirachy and find the best match (and then store this for faster lookup next time) */
template<typename ValueType>
struct TClassMap
{
	/** Returns ClassInfo for a given class. */
	ValueType& GetChecked(const UClass* Class)
	{
		ValueType* Ptr = Get(Class);
		repCheckf(Ptr, TEXT("No ClassInfo found for %s"), *GetNameSafe(Class));
		return *Ptr;
	}

	ValueType* Get(const UClass* Class)
	{
		FObjectKey ObjKey(Class);
		if (ValueType* Ptr = Map.Find(ObjKey))
		{
			return Ptr;
		}

		// We haven't seen this class before, look it up (slower)
		return GetClassInfoForNewClass_r(ObjKey, Class);
	}

	/** Returns if class has data in the map.  */
	bool Contains(UClass* Class, bool bIncludeSuperClasses) const
	{
		if (bIncludeSuperClasses)
		{
			while(Class)
			{
				if (Map.Find(FObjectKey(Class)) != nullptr)
				{
					return true;
				}

				Class = Class->GetSuperClass();
			}
			return false;
		}
		
		return (Map.Find(FObjectKey(Class)) != nullptr);
	}

	/** Sets class info for a given class. Call this in your Replication Graphs setup */
	void Set(UClass* InClass, const ValueType& Value)
	{
		Map.Add(FObjectKey(InClass)) = Value;
		
		// Sets value for all derived classes. This is probably not useful since all classes may not be loaded anyways. TClassMap will 
		// climb class heir achy when it encounters a new request. This shouldn't be too expensive, so the lazy approach seems better.
		/*
		TArray<UClass*> Classes = { InClass };
		GetDerivedClasses(InClass, Classes, true);
		for (UClass* Class : Classes)
		{
			UE_LOG(LogTemp, Display, TEXT("Adding for %s [From %s]"), *GetNameSafe(Class), *GetNameSafe(InClass));
			Map.Add(FObjectKey(Class)) = Value;
		}*/
	}

	FORCEINLINE typename TMap<FObjectKey, ValueType>::TIterator CreateIterator() { return Map.CreateIterator(); }

private:

	ValueType* GetClassInfoForNewClass_r(FObjectKey OriginalObjKey, const UClass* OriginalClass)
	{
		const UClass* Class = OriginalClass->GetSuperClass();
		if (!Class)
		{
			return nullptr;
		}

		FObjectKey ObjKey(Class);

		if (ValueType* Ptr = Map.Find(ObjKey))
		{
			// Set the original class's data to the super that we found (so that we don't have to climb the chain next time)
			// Need to make a local copy in case the add reallocates the internal TMap data. This prevents Gil writing a blog post about you.
			ValueType LocalData = *Ptr;
			ValueType& NewData = Map.Add(OriginalObjKey);
			NewData = LocalData;
			return &NewData;
		}

		return GetClassInfoForNewClass_r(ObjKey, Class);
	}

	TMap<FObjectKey /** UClass */, ValueType> Map;
};

struct FGlobalActorReplicationInfoMap
{
	FGlobalActorReplicationInfoMap()
	{
		// Initialize so we always at least have AActor defined with default values. 
		// It is ok to override this by calling SetClassInfo again.
		ClassMap.Set(AActor::StaticClass(), FClassReplicationInfo());
	}

	/** Returns data associated with the actor. Will create it with default class values if necessary  */
	FGlobalActorReplicationInfo& Get(const FActorRepListType& Actor)
	{
		// Quick lookup - this is the most common case
		if (FGlobalActorReplicationInfo* Ptr = ActorMap.Find(Actor))
		{
			return *Ptr;
		}

		// We need to add data for this actor
		FClassReplicationInfo& ClassInfo = GetClassInfo( GetActorRepListTypeClass(Actor) );
		return ActorMap.Emplace(Actor, ClassInfo);
	}

	// Same as above but outputs bool if it was created. This is uncommonly called. Don't want to slow down the frequently called version
	FGlobalActorReplicationInfo& Get(const FActorRepListType& Actor, bool& bWasCreated)
	{
		// Quick lookup - this is the most common case
		if (FGlobalActorReplicationInfo* Ptr = ActorMap.Find(Actor))
		{
			return *Ptr;
		}

		bWasCreated = true;

		// We need to add data for this actor
		FClassReplicationInfo& ClassInfo = GetClassInfo( GetActorRepListTypeClass(Actor) );
		return ActorMap.Emplace(Actor, ClassInfo);
	}

	/** Finds data associated with the actor but does not create if its not there yet. */
	FORCEINLINE FGlobalActorReplicationInfo* Find(const FActorRepListType& Actor) { return ActorMap.Find(Actor); }

	/** Removes actor data from map */
	FORCEINLINE int32 Remove(const FActorRepListType& Actor) { return ActorMap.Remove(Actor); }

	/** Returns ClassInfo for a given class. */
	FORCEINLINE FClassReplicationInfo& GetClassInfo(UClass* Class) { return ClassMap.GetChecked(Class); }

	/** Sets class info for a given class and its derived classes if desired. Call this in your Replication Graphs setup */
	FORCEINLINE void SetClassInfo(UClass* InClass, const FClassReplicationInfo& Info) {	ClassMap.Set(InClass, Info); }
	
	FORCEINLINE TMap<FActorRepListType, FGlobalActorReplicationInfo>::TIterator CreateActorMapIterator() { return ActorMap.CreateIterator(); }
	FORCEINLINE TMap<FObjectKey, FClassReplicationInfo>::TIterator CreateClassMapIterator() { return ClassMap.CreateIterator(); }

private:

	TMap<FActorRepListType, FGlobalActorReplicationInfo> ActorMap;
	TClassMap<FClassReplicationInfo> ClassMap;
};

/** Per-Actor data that is stored per connection */
struct FConnectionReplicationActorInfo
{
	FConnectionReplicationActorInfo() { }

	FConnectionReplicationActorInfo(const FGlobalActorReplicationInfo& GlobalInfo)
	{
		// Pull data from the global actor info. This is done for things that we just want to duplicate in both places so that we can avoid a lookup into the global map
		// and also for things that we want to be overridden per (connection/actor)

		ReplicationPeriodFrame = GlobalInfo.Settings.ReplicationPeriodFrame;
		CullDistanceSquared = GlobalInfo.Settings.CullDistanceSquared;
	}

	UActorChannel* Channel = nullptr;

	float CullDistanceSquared = 0.f;

	/** The next frame we are allowed to replicate on */
	uint32 NextReplicationFrameNum = 0;

	/** Min frames that have to pass between subsequent calls to ReplicateActor */
	uint32 ReplicationPeriodFrame = 1;

	/** The last frame that this actor replicated on to this connection */
	uint32 LastRepFrameNum = 0;

	/** The frame that this actor was (first) starved on. Meaning it wanted to replicate but we ran out of budget to do so. This is cleared everytime we do replicate */
	uint32 StarvedFrameNum = 0;

	/** The frame num that we will close the actor channel. This will get updated/pushed anytime the actor replicates based on FGlobalActorReplicationInfo::ActorChannelFrameTimeout  */
	uint32 ActorChannelCloseFrameNum = 0;

	bool bDormantOnConnection = false;
	bool bTearOff = false;

	void LogDebugString(FOutputDevice& Ar) const;
};

/** Map for Actor -> ConnectionActorInfo. This wraps the TMap mainly so we can do custom initialization in FindOrAdd. */
struct FPerConnectionActorInfoMap
{
	FORCEINLINE_DEBUGGABLE FConnectionReplicationActorInfo& FindOrAdd(const FActorRepListType& Actor)
	{
		if (FConnectionReplicationActorInfo* Value = Map.Find(Actor))
		{
			return *Value;
		}
		FConnectionReplicationActorInfo NewInfo(GlobalMap->Get(Actor));

		return Map.Emplace(Actor, GlobalMap->Get(Actor) );
	}

	FORCEINLINE FConnectionReplicationActorInfo* Find(const FActorRepListType& Actor)
	{
		return Map.Find(Actor);
	}

	FORCEINLINE TMap<FActorRepListType, FConnectionReplicationActorInfo>::TIterator CreateIterator()
	{
		return Map.CreateIterator();
	}

	FORCEINLINE FConnectionReplicationActorInfo& FindChecked(const FActorRepListType& Actor)
	{
		return Map.FindChecked(Actor);
	}

	FORCEINLINE void SetGlobalMap(FGlobalActorReplicationInfoMap* InGlobalMap)
	{
		GlobalMap = InGlobalMap;
	}

private:
	TMap<FActorRepListType, FConnectionReplicationActorInfo> Map;
	FGlobalActorReplicationInfoMap* GlobalMap;
};

/** Data that every replication graph has access to/is initialized with */
struct FReplicationGraphGlobalData
{
	FReplicationGraphGlobalData() { }
	FReplicationGraphGlobalData(FGlobalActorReplicationInfoMap* InRepMap, UWorld* InWorld) : GlobalActorReplicationInfoMap(InRepMap), World(InWorld) { }

	FGlobalActorReplicationInfoMap* GlobalActorReplicationInfoMap = nullptr;

	UWorld* World = nullptr;
};

// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// Prioritized Actor Lists
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------

/** Stores "full debug details" about how an actor was prioritized. This is not used in the actual replication code, just saved off for logging/debugging.  */
struct FPrioritizedActorFullDebugDetails
{
	FPrioritizedActorFullDebugDetails(FActorRepListType InActor) : Actor(InActor) { }
	bool operator==(const FActorRepListType& InActor) const { return Actor == InActor; }

	FActorRepListType Actor;
	float DistanceSq = 0.f;
	float DistanceFactor = 0.f;

	uint32 FramesSinceLastRap = 0;
	float StarvationFactor = 0.f;

	uint32 ForceNetUpdateDelta = 0;
	float GameCodeScaling = 0.f;

	FString BuildString() const
	{
		// This is pretty stupid slow but only needed for debug/logging
		FString Str;
		auto Append = [&Str](FString InStr) { Str += FString::Printf(TEXT("%-30s "), *InStr); };
		
		Append( DistanceFactor > 0.f ? FString::Printf(TEXT("(Dist: %.2f | %.2f) "), FMath::Sqrt(DistanceSq), DistanceFactor) : TEXT(""));
		Append( StarvationFactor > 0.f ? FString::Printf(TEXT("(FramesLastRep: %d | %.2f) "), FramesSinceLastRap, StarvationFactor) : TEXT("") );
		Append ( GameCodeScaling > 0.f ? FString::Printf(TEXT("(ForceNetUpdate: %d | %.2f) "), ForceNetUpdateDelta, GameCodeScaling) : TEXT(""));
		
		return Str;
	}
};

/** Debug data about an actor that was skipped during the prioritization phase */
struct FSkippedActorFullDebugDetails
{
	FSkippedActorFullDebugDetails(FActorRepListType InActor) : Actor(InActor) { }
	FActorRepListType Actor;
	bool bWasDormant = false; // If set, was skipped because it is dormant on this connection
	float DistanceCulled = 0.f; // If set, was skipped due to distance culling
	uint32 FramesTillNextReplication = 0; // If set, was skipped due to not being ready for replication
};

/** Prioritized List of actors to replicate. This is what we actually use to replicate actors. */
struct FPrioritizedRepList
{
	FPrioritizedRepList(FReplicationListCategory InCategory) : ListCategory(InCategory) { }
	struct FItem
	{
		// FIXME: We cannot store pointers to backing actor data yet because of how they are stored in (unstable) TMaps

		FItem(float InPriority, FActorRepListType InActor /*, FGlobalActorReplicationInfo* InGlobal, FReplicationConnectionActorInfo* InConn*/) 
			: Priority(InPriority), Actor(InActor) /*, GlobalData(InGlobal), ConnectionData(InConn)*/ { }
		bool operator<(const FItem& Other) const { return Priority < Other.Priority; }

		float Priority;
		FActorRepListType Actor;

		/*
		FGlobalActorReplicationInfo* GlobalData;
		FReplicationConnectionActorInfo* ConnectionData;
		*/
	};

	TArray<FItem> Items;
	FReplicationListCategory ListCategory;
	
#if REPGRAPH_DETAILS
	FPrioritizedActorFullDebugDetails* GetNextFullDebugDetails(FActorRepListType Actor)
	{
		if (FullDebugDetails.IsValid() == false)
		{
			FullDebugDetails = MakeUnique<TArray<FPrioritizedActorFullDebugDetails> >();
		}
		return new (*FullDebugDetails) FPrioritizedActorFullDebugDetails(Actor);
	}
	TUniquePtr<TArray<FPrioritizedActorFullDebugDetails> > FullDebugDetails;
	
	
	FSkippedActorFullDebugDetails* GetNextSkippedDebugDetails(FActorRepListType Actor)
	{
		if (SkippedDebugDetails.IsValid() == false)
		{
			SkippedDebugDetails = MakeUnique<TArray<FSkippedActorFullDebugDetails> >();
		}
		return new (*SkippedDebugDetails) FSkippedActorFullDebugDetails(Actor);
	}
	TUniquePtr<TArray<FSkippedActorFullDebugDetails> > SkippedDebugDetails;
#endif
};


// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// Prioritized Actor Lists
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------

/** This is the struct we use to push new replication actors into the graph. "New" doesn't mean "newly spawned" it means "new to the graph". FIXME: Please suggest a better name! */
struct FNewReplicatedActorInfo
{
	explicit FNewReplicatedActorInfo(const FActorRepListType& InActor) : Actor(InActor), Class(InActor->GetClass())
	{
		ULevel* Level = Cast<ULevel>(GetActor()->GetOuter());
		if (Level && Level->IsPersistentLevel() == false)
		{
			StreamingLevelName = Level->GetOutermost()->GetFName();
		}
	}

	AActor* GetActor() const { return Actor; }

	FActorRepListType Actor;
	FName StreamingLevelName;
	UClass* Class;
};

// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// RPCs
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------

struct FRPCSendPolicyInfo
{
	FRPCSendPolicyInfo(const bool bInSendImmediately) : bSendImmediately(bInSendImmediately) { }
	bool bSendImmediately;

	// Suspect that this will grow over time. Possibly things like "min distance to send immediately" etc
};

// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// Debug Info
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------

struct FReplicationGraphDebugInfo
{
	FReplicationGraphDebugInfo( FOutputDevice& InAr ) : Ar(InAr) { }

	enum EFlags
	{
		ShowActors,
		ShowClasses,
		ShowNativeClasses,
		ShowTotalCount
	};
	
	FOutputDevice& Ar;
	EFlags Flags;
	FString CurrentIndentString;
	const FString IndentString = TEXT("  ");

	void Log(const FString& Str) { Ar.Logf(TEXT("%s%s"), *CurrentIndentString, *Str); }

	void PushIndent() { CurrentIndentString += IndentString; }
	void PopIndent() { CurrentIndentString = CurrentIndentString.LeftChop(IndentString.Len()); }
};

REPLICATIONGRAPH_API void LogActorRepList(FReplicationGraphDebugInfo& DebugInfo, FString Prefix, const FActorRepListRefView& List);

struct FPrioritizedActorDebugInfo
{
	int32 DormantCount = 0;
	int32 NotReadyCount = 0;
	int32 DistanceCulledCount = 0;
	int32 ReplicatedCount = 0;
	int32 StarvedCount = 0;
};


struct FNativeClassAccumulator
{
	void Increment(UClass* Class)
	{
		while (Class)
		{
			if (Class->IsNative())
			{
				break;
			}
			Class = Class->GetSuperClass();
		}
		
		Map.FindOrAdd(Class)++;
	}

	FString BuildString()
	{
		FString Str;
		Sort();
		for (auto& It: Map)
		{
			Str += FString::Printf(TEXT("[%s, %d] "), *It.Key->GetName(), It.Value);
			
		}
		return Str;
	}

	void Reset() { Map.Reset(); }
	void Sort() { Map.ValueSort(TGreater<int32>()); }
	TMap<UClass*, int32> Map;
};

// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// Profiling
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------

#define USE_REPCSVPROFILER !UE_BUILD_SHIPPING
#define USE_REPCSVPROFILER_EX !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if USE_REPCSVPROFILER
struct FReplicationGraphProfiler
{
	FReplicationGraphProfiler()
	{
		bEnabled = FParse::Param(FCommandLine::Get(), TEXT("RepGraphProfile"));
	}

	~FReplicationGraphProfiler()
	{
		End();
	}

	void OnClientConnect();
	void StartRepFrame();
	void EndRepFrame();

private:
	void End();
	
	double StartTime = 0.f;

	bool bEnabled = false;
	bool bStarted = false;
	uint32 KillFrame = -1;
	float TimeLimit = 60.f * 10;
};
#endif

