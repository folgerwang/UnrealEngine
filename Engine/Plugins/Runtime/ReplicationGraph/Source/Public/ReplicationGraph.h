// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================
Replication Graph

Implementation of Replication Driver. This is customizable via subclassing UReplicationGraph. Default implementation (UReplicationGraph) does not fully function and is intended to be overridden.
	Check out BasicReplicationGraph.h for a minimal implementation that works "out of the box" with a minimal feature set.
	Check out ShooterGame / UShooterReplicationGraph for a more advanced implementation.



High level overview of ReplicationGraph:
	* The graph is a collection of nodes which produce replication lists for each network connection. The graph essentially maintains persistent lists of actors to replicate and feeds them to connections. 
	
	* This allows for more work to be shared and greatly improves the scalability of the system with respect to number of actors * number of connections.
	
	* For example, one node on the graph is the spatialization node. All actors that essentially use distance based relevancy will go here. There are also always relevant nodes. Nodes can be global, per connection, or shared (E.g, "Always relevant for team" nodes).

	* The main impact here is that virtual functions like IsNetRelevantFor and GetNetPriority are not used by the replication graph. Several properties are also not used or are slightly different in use. 

	* Instead there are essentially three ways for game code to affect this part of replication:
		* The graph itself. Adding new UReplicationNodes or changing the way an actor is placed in the graph.
		* FGlobalActorReplicationInfo: The associative data the replication graph keeps, globally, about each actor. 
		* FConnectionReplicationActorInfo: The associative data that the replication keeps, per connection, about each actor. 

	* After returned from the graph, the actor lists are further culled for distance and frequency, then merged and prioritized. The end result is a sorted list of actors to replicate that we then do logic for creating or updating actor channels.



Subclasses should implement these functions:

UReplicationGraph::InitGlobalActorClassSettings
	Initialize UReplicationGraph::GlobalActorReplicationInfoMap.

UReplicationGraph::InitGlobalGraphNodes
	Instantiate new UGraphNodes via ::CreateNewNode. Use ::AddGlobalGraphNode if they are global (for all connections).

UReplicationGraph::RouteAddNetworkActorToNodes/::RouteRemoveNetworkActorToNodes
	Route actor spawning/despawning to the right node. (Or your nodes can gather the actors themselves)

UReplicationGraph::InitConnectionGraphNodes
	Initialize per-connection nodes (or associate shared nodes with them via ::AddConnectionGraphNode)


=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Serialization/BitReader.h"
#include "Misc/NetworkGuid.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Actor.h"
#include "Misc/EngineVersion.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Misc/NetworkVersion.h"
#include "Engine/ReplicationDriver.h"
#include "ReplicationGraphTypes.h"

#include "ReplicationGraph.generated.h"

UCLASS(transient, config=Engine)
class REPLICATIONGRAPH_API UReplicationGraphNode : public UObject
{
	GENERATED_BODY()

public:

	UReplicationGraphNode();

	/** Called when a network actor is spawned or an actor changes replication status */
	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor ) PURE_VIRTUAL(UReplicationGraphNode::NotifyAddNetworkActor, );
	
	/** Called when a networked actor is being destroyed or no longer wants to replicate */
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& Actor, bool bWarnIfNotFound=true) PURE_VIRTUAL(UReplicationGraphNode::NotifyRemoveNetworkActor, return false; );

	/** Called when world changes or when all subclasses should dump any persistent data/lists about replicated actors here. (The new/next world will be set before this is called) */
	virtual void NotifyResetAllNetworkActors();
	
	/** Mark the node and all its children PendingKill */
	virtual void TearDown();
	
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) PURE_VIRTUAL(UReplicationGraphNode::GatherActorListsForConnection, );

	/** Called once per frame prior to replication ONLY on root nodes (nodes created via UReplicationGraph::CreateNode) has RequiresPrepareForReplicationCall=true */
	virtual void PrepareForReplication() { };

	/** Debugging only function to return a normal TArray of actor rep list (for logging, debug UIs, etc) */
	virtual void GetAllActorsInNode_Debugging(TArray<FActorRepListType>& OutArray) const { }

	// -----------------------------------------------------

	bool GetRequiresPrepareForReplication() const { return bRequiresPrepareForReplicationCall; }

	void Initialize(const TSharedPtr<FReplicationGraphGlobalData>& InGraphGlobals) { GraphGlobals = InGraphGlobals; }

	virtual UWorld* GetWorld() const override final { return  GraphGlobals.IsValid() ? GraphGlobals->World : nullptr; }

	virtual void LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const;

	virtual FString GetDebugString() const { return GetName(); }

	/** Allocates and initializes ChildNode of a specific type T. This is what you will want to call in your FCreateChildNodeFuncs.  */
	template< class T >
	T* CreateChildNode()
	{
		T* NewNode = NewObject<T>(this);
		NewNode->Initialize(GraphGlobals);
		AllChildNodes.Add(NewNode);
		return NewNode;
	}

	void ToggleHighFrequencyPawns();

protected:

	UPROPERTY()
	TArray< UReplicationGraphNode* > AllChildNodes;

	TSharedPtr<FReplicationGraphGlobalData>	GraphGlobals;

	/** Determines if PrepareForReplication() is called. This currently must be set in the constructor, not dynamically. */
	bool bRequiresPrepareForReplicationCall = false;
};

// -----------------------------------


struct FStreamingLevelActorListCollection
{
	void AddActor(const FNewReplicatedActorInfo& ActorInfo);
	bool RemoveActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound, UReplicationGraphNode* Outer);
	void Reset();
	void Gather(const FConnectionGatherActorListParameters& Params);
	void DeepCopyFrom(const FStreamingLevelActorListCollection& Source);
	void GetAll_Debug(TArray<FActorRepListType>& OutArray) const;
	void Log(FReplicationGraphDebugInfo& DebugInfo) const;
	int32 NumLevels() const { return StreamingLevelLists.Num(); }
	

	struct FStreamingLevelActors
	{
		FStreamingLevelActors(FName InName) : StreamingLevelName(InName) { repCheck(InName != NAME_None); ReplicationActorList.Reset(4); /** FIXME[19]: see above comment */ }
		FName StreamingLevelName;
		FActorRepListRefView ReplicationActorList;
		bool operator==(const FName& InName) const { return InName == StreamingLevelName; };
	};

	/** Lists for streaming levels. Actors that "came from" streaming levels go here. These lists are only returned if the connection has their streaming level loaded. */
	static const int32 NumInlineAllocations = 4;
	TArray<FStreamingLevelActors, TInlineAllocator<NumInlineAllocations>> StreamingLevelLists;

	void CountBytes(FArchive& Ar)
	{
		StreamingLevelLists.CountBytes(Ar);
	}
};

// -----------------------------------

/** A Node that contains ReplicateActorLists. This contains 1 "base" list and a TArray of lists that are conditioned on a streaming level being loaded. */
UCLASS()
class REPLICATIONGRAPH_API UReplicationGraphNode_ActorList : public UReplicationGraphNode
{
	GENERATED_BODY()

public:
	
	UReplicationGraphNode_ActorList() { if (!HasAnyFlags(RF_ClassDefaultObject)) { ReplicationActorList.Reset(4); } }

	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo) override;
	
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound=true) override;
	
	virtual void NotifyResetAllNetworkActors() override;
	
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	virtual void LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const override;

	virtual void GetAllActorsInNode_Debugging(TArray<FActorRepListType>& OutArray) const;

	/** Copies the contents of Source into this node. Note this does not copy child nodes, just the ReplicationActorList/StreamingLevelCollection lists on this node. */
	void DeepCopyActorListsFrom(const UReplicationGraphNode_ActorList* Source);

protected:

	/** Just logs our ReplicationActorList and StreamingLevelCollection (not our child nodes). Useful when subclasses override LogNode */
	void LogActorList(FReplicationGraphDebugInfo& DebugInfo) const;

	/** The base list that most actors will go in */
	FActorRepListRefView ReplicationActorList;

	/** A collection of lists that streaming actors go in */
	FStreamingLevelActorListCollection StreamingLevelCollection;

	friend class AReplicationGraphDebugActor;
};

// -----------------------------------

/** A Node that contains ReplicateActorLists. This contains multiple buckets for non streaming level actors and will pull from each bucket on alternating frames. It is a way to broadly load balance. */
UCLASS()
class REPLICATIONGRAPH_API UReplicationGraphNode_ActorListFrequencyBuckets : public UReplicationGraphNode
{
	GENERATED_BODY()

public:

	struct FSettings
	{
		int32 NumBuckets = 3;
		int32 ListSize = 12;
		bool EnableFastPath = false; // Whether to return lists as FastPath in "off frames". Defaults to false.
		int32 FastPathFrameModulo = 1; // Only do fast path if frame num % this = 0

		// Threshold for dynamically balancing buckets bsaed on number of actors in this node. E.g, more buckets when there are more actors.
		struct FBucketThresholds
		{
			FBucketThresholds(int32 InMaxActors, int32 InNumBuckets) : MaxActors(InMaxActors), NumBuckets(InNumBuckets) { }
			int32 MaxActors;	// When num actors <= to MaxActors
			int32 NumBuckets;	// use this NumBuckets
		};

		TArray<FBucketThresholds, TInlineAllocator<4>> BucketThresholds;
	};

	/** Default settings for all nodes. By being static, this allows games to easily override the settings are all nodes without having to subclass every graph node class */
	static FSettings DefaultSettings;

	/** Settings for this specific node. If not set we will fallback to the static/global DefaultSettings */
	TSharedPtr<FSettings> Settings;

	const FSettings& GetSettings() const { return Settings.IsValid() ? *Settings.Get() : DefaultSettings; }
	
	UReplicationGraphNode_ActorListFrequencyBuckets() { if (!HasAnyFlags(RF_ClassDefaultObject)) { SetNonStreamingCollectionSize(GetSettings().NumBuckets); } }

	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo) override;
	
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound=true) override;
	
	virtual void NotifyResetAllNetworkActors() override;
	
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	virtual void LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const override;

	virtual void GetAllActorsInNode_Debugging(TArray<FActorRepListType>& OutArray) const;

	void SetNonStreamingCollectionSize(const int32 NewSize);

protected:

	void CheckRebalance();

	int32 TotalNumNonStreamingActors = 0;

	/** Non streaming actors go in one of these lists */
	TArray<FActorRepListRefView, TInlineAllocator<2>> NonStreamingCollection;

	/** A collection of lists that streaming actors go in */
	FStreamingLevelActorListCollection StreamingLevelCollection;

	friend class AReplicationGraphDebugActor;
};

// -----------------------------------


/** A node intended for dynamic (moving) actors where replication frequency is based on distance to the connection's view location */
UCLASS()
class REPLICATIONGRAPH_API UReplicationGraphNode_DynamicSpatialFrequency : public UReplicationGraphNode_ActorList
{
	GENERATED_BODY()

public:

	UReplicationGraphNode_DynamicSpatialFrequency();
	
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	// --------------------------------------------------------

	struct FSpatializationZone
	{
		// Init directly
		FSpatializationZone(float InMinDotProduct, float InMinDistPct, float InMaxDistPct, uint32 InMinRepPeriod, uint32 InMaxRepPeriod, uint32 InMinRepPeriod_FastPath, uint32 InMaxRepPeriod_FastPath) :
			MinDotProduct(InMinDotProduct), MinDistPct(InMinDistPct), MaxDistPct(InMaxDistPct), MinRepPeriod(InMinRepPeriod), MaxRepPeriod(InMaxRepPeriod), FastPath_MinRepPeriod(InMinRepPeriod_FastPath), FastPath_MaxRepPeriod(InMaxRepPeriod_FastPath) { }

		// Init based on target frame rate
		FSpatializationZone(float InMinDotProduct, float InMinDistPct, float InMaxDistPct, float InMinRepHz, float InMaxRepHz, float InMinRepHz_FastPath, float InMaxRepHz_FastPath, float TickRate) :
			MinDotProduct(InMinDotProduct), MinDistPct(InMinDistPct), MaxDistPct(InMaxDistPct), 
				MinRepPeriod(HzToFrm(InMinRepHz,TickRate)), MaxRepPeriod(HzToFrm(InMaxRepHz,TickRate)), FastPath_MinRepPeriod(HzToFrm(InMinRepHz_FastPath,TickRate)), FastPath_MaxRepPeriod(HzToFrm(InMaxRepHz_FastPath,TickRate)) { }

		float MinDotProduct = 1.f;	// Must have dot product >= to this to be in this zone
		float MinDistPct = 0.f;		// Min distance as pct of per-connection culldistance, to map to MinRepPeriod
		float MaxDistPct = 1.f;		// Max distance as pct of per-connection culldistance, to map to MaxRepPeriod

		uint32 MinRepPeriod = 5;
		uint32 MaxRepPeriod = 10;

		uint32 FastPath_MinRepPeriod = 1;
		uint32 FastPath_MaxRepPeriod = 5;

		static uint32 HzToFrm(float Hz, float TargetFrameRate)
		{
			return  Hz > 0.f ? (uint32)FMath::CeilToInt(TargetFrameRate / Hz) : 0;
		}
	};

	struct FSettings
	{
		FSettings() :MaxBitsPerFrame(0), MaxNearestActors(-1) { }

		FSettings(TArrayView<FSpatializationZone> InZoneSettings, TArrayView<FSpatializationZone> InZoneSettings_NonFastSharedActors, int64 InMaxBitsPerFrame, int32 InMaxNearestActors = -1) 
			: ZoneSettings(InZoneSettings), ZoneSettings_NonFastSharedActors(InZoneSettings_NonFastSharedActors), MaxBitsPerFrame(InMaxBitsPerFrame), MaxNearestActors(InMaxNearestActors) { }

		TArrayView<FSpatializationZone> ZoneSettings;
		TArrayView<FSpatializationZone> ZoneSettings_NonFastSharedActors;	// Zone Settings for actors that do not support FastShared replication.
		int64 MaxBitsPerFrame;
		int32 MaxNearestActors; // Only replicate the X nearest actors to a connection in this node. -1 = no limit.
	};

	static FSettings DefaultSettings;	// Default settings used by all instance
	FSettings* Settings;				// per instance override settings (optional)

	FSettings& GetSettings() { return Settings ? *Settings : DefaultSettings; }

	const char* CSVStatName; // Statname to use for tracking the gather/prioritizing phase of this node

protected:

	struct FDynamicSpatialFrequency_SortedItem
	{
		FDynamicSpatialFrequency_SortedItem() { }
		FDynamicSpatialFrequency_SortedItem(AActor* InActor, int32 InFramesTillReplicate, bool InEnableFastPath, FGlobalActorReplicationInfo* InGlobal, FConnectionReplicationActorInfo* InConnection ) 
			: Actor(InActor), FramesTillReplicate(InFramesTillReplicate), EnableFastPath(InEnableFastPath), GlobalInfo(InGlobal), ConnectionInfo(InConnection) { }

		FDynamicSpatialFrequency_SortedItem(AActor* InActor, int32 InDistance, FGlobalActorReplicationInfo* InGlobal)
			: Actor(InActor), FramesTillReplicate(InDistance), GlobalInfo(InGlobal) {}

		bool operator<(const FDynamicSpatialFrequency_SortedItem& Other) const { return FramesTillReplicate < Other.FramesTillReplicate; }

		UPROPERTY()
		AActor* Actor = nullptr;

		int32 FramesTillReplicate = 0; // Note this also serves as "Distance Sq" when doing the FSettings::MaxNearestActors pass.
		bool EnableFastPath = false;

		FGlobalActorReplicationInfo* GlobalInfo = nullptr;
		FConnectionReplicationActorInfo* ConnectionInfo = nullptr;
	};

	// Working area for our sorted replication list. Reset each frame for each connection as we build the list
	TArray<FDynamicSpatialFrequency_SortedItem>	SortedReplicationList;
	
	// Working ints for adaptive load balancing. Does not count actors that rep every frame
	int32 NumExpectedReplicationsThisFrame = 0;
	int32 NumExpectedReplicationsNextFrame = 0;

	bool IgnoreCullDistance = false;

	virtual void GatherActors(const FActorRepListRefView& RepList, FGlobalActorReplicationInfoMap& GlobalMap, FPerConnectionActorInfoMap& ConnectionMap, const FConnectionGatherActorListParameters& Params, UNetConnection* NetConnection);
	virtual void GatherActors_DistanceOnly(const FActorRepListRefView& RepList, FGlobalActorReplicationInfoMap& GlobalMap, FPerConnectionActorInfoMap& ConnectionMap, const FConnectionGatherActorListParameters& Params);

	void CalcFrequencyForActor(AActor* Actor, UReplicationGraph* RepGraph, UNetConnection* NetConnection, FGlobalActorReplicationInfo& GlobalInfo, FConnectionReplicationActorInfo& ConnectionInfo, FSettings& MySettings, const FVector& ConnectionViewLocation, const FVector& ConnectionViewDir, const uint32 FrameNum, int32 ExistingItemIndex);
};


// -----------------------------------

/** Removes dormant (on connection) actors from its rep lists */
UCLASS()
class REPLICATIONGRAPH_API UReplicationGraphNode_ConnectionDormanyNode : public UReplicationGraphNode_ActorList
{
	GENERATED_BODY()
public:
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool WarnIfNotFound) override;
	virtual void NotifyResetAllNetworkActors() override;

	void NotifyActorDormancyFlush(FActorRepListType Actor);

	void OnClientVisibleLevelNameAdd(FName LevelName, UWorld* World);

private:
	void ConditionalGatherDormantActorsForConnection(FActorRepListRefView& ConnectionRepList, const FConnectionGatherActorListParameters& Params, FActorRepListRefView* RemovedList);
	
	int32 TrickleStartCounter = 10;

	// Tracks actors we've removed in this per-connection node, so that we can restore them if the streaming level is unloaded and reloaded.
	FStreamingLevelActorListCollection RemovedStreamingLevelActorListCollection;
};


/** Stores per-connection copies of a master actor list. Skips and removes elements from per connection list that are fully dormant */
UCLASS()
class REPLICATIONGRAPH_API UReplicationGraphNode_DormancyNode : public UReplicationGraphNode_ActorList
{
	GENERATED_BODY()

public:

	static float MaxZForConnection; // Connection Z location has to be < this for ConnectionsNodes to be made.

	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo) override { ensureMsgf(false, TEXT("UReplicationGraphNode_DormancyNode::NotifyAddNetworkActor not functional.")); }
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound=true) override { ensureMsgf(false, TEXT("UReplicationGraphNode_DormancyNode::NotifyRemoveNetworkActor not functional.")); return false; }
	virtual void NotifyResetAllNetworkActors() override;
	
	void AddDormantActor(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo);
	void RemoveDormantActor(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& ActorRepInfo);

	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	void OnActorDormancyFlush(FActorRepListType Actor, FGlobalActorReplicationInfo& GlobalInfo);

	void ConditionalGatherDormantDynamicActors(FActorRepListRefView& RepList, const FConnectionGatherActorListParameters& Params, FActorRepListRefView* RemovedList);

	UReplicationGraphNode_ConnectionDormanyNode* GetConnectionNode(const FConnectionGatherActorListParameters& Params);

private:

	TMap<UNetReplicationGraphConnection*, UReplicationGraphNode_ConnectionDormanyNode*> ConnectionNodes;
};

UCLASS()
class REPLICATIONGRAPH_API UReplicationGraphNode_GridCell : public UReplicationGraphNode_ActorList
{
	GENERATED_BODY()

public:

	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo) override { ensureMsgf(false, TEXT("UReplicationGraphNode_Simple2DSpatializationLeaf::NotifyAddNetworkActor not functional.")); }
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound=true) override { ensureMsgf(false, TEXT("UReplicationGraphNode_Simple2DSpatializationLeaf::NotifyRemoveNetworkActor not functional.")); return false; }
	virtual void LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const override;
	virtual void GetAllActorsInNode_Debugging(TArray<FActorRepListType>& OutArray) const override;

	void AddStaticActor(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalRepInfo, bool bParentNodeHandlesDormancyChange);
	void AddDynamicActor(const FNewReplicatedActorInfo& ActorInfo);

	void RemoveStaticActor(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& ActorRepInfo, bool bWasAddedAsDormantActor);
	void RemoveDynamicActor(const FNewReplicatedActorInfo& ActorInfo);

	// Allow graph to override function for creating the dynamic node in the cell
	TFunction<UReplicationGraphNode*(UReplicationGraphNode_GridCell* Parent)> CreateDynamicNodeOverride;

	UReplicationGraphNode_DormancyNode* GetDormancyNode();

private:

	UPROPERTY()
	UReplicationGraphNode* DynamicNode = nullptr;

	UPROPERTY()
	UReplicationGraphNode_DormancyNode* DormancyNode = nullptr;

	UReplicationGraphNode* GetDynamicNode();

	void OnActorDormancyFlush(FActorRepListType Actor, FGlobalActorReplicationInfo& GlobalInfo, UReplicationGraphNode_DormancyNode* DormancyNode );

	void ConditionalCopyDormantActors(FActorRepListRefView& FromList, UReplicationGraphNode_DormancyNode* ToNode);	
	void OnStaticActorNetDormancyChange(FActorRepListType Actor, FGlobalActorReplicationInfo& GlobalInfo, ENetDormancy NewVlue, ENetDormancy OldValue);
};

// -----------------------------------

UCLASS()
class REPLICATIONGRAPH_API UReplicationGraphNode_GridSpatialization2D : public UReplicationGraphNode
{
	GENERATED_BODY()

public:

	UReplicationGraphNode_GridSpatialization2D();

	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor) override;	
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound=true) override;
	virtual void NotifyResetAllNetworkActors() override;
	virtual void PrepareForReplication() override;
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	void AddActor_Static(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& ActorRepInfo) { AddActorInternal_Static(ActorInfo, ActorRepInfo, false); }
	void AddActor_Dynamic(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& ActorRepInfo) { AddActorInternal_Dynamic(ActorInfo); }
	void AddActor_Dormancy(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& ActorRepInfo);

	void RemoveActor_Static(const FNewReplicatedActorInfo& ActorInfo);
	void RemoveActor_Dynamic(const FNewReplicatedActorInfo& ActorInfo) { RemoveActorInternal_Dynamic(ActorInfo); }
	void RemoveActor_Dormancy(const FNewReplicatedActorInfo& ActorInfo);
	

	// Called if cull distance changes. Note the caller must update Global/Connection actor rep infos. This just changes cached state within this node
	void NotifyActorCullDistChange(AActor* Actor, FGlobalActorReplicationInfo& GlobalInfo, float OldDistSq);
	
	float		CellSize;
	FVector2D	SpatialBias;
	float		ConnectionMaxZ = WORLD_MAX; // Connection locations have to be <= to this to pull from the grid
	
	// Allow graph to override function for creating cell nodes in this grid.
	TFunction<UReplicationGraphNode_GridCell*(UReplicationGraphNode_GridSpatialization2D* Parent)>	CreateCellNodeOverride;

	void ForceRebuild() { bNeedsRebuild = true; }

	void AddSpatialRebuildBlacklistClass(UClass* Class) { RebuildSpatialBlacklistMap.Set(Class, true); }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	TArray<FString> DebugActorNames;
#endif

protected:

	/** For adding new actor to the graph */
	virtual void AddActorInternal_Dynamic(const FNewReplicatedActorInfo& ActorInfo);
	virtual void AddActorInternal_Static(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& ActorRepInfo, bool IsDormancyDriven); // Can differ the actual call to implementation if actor is not ready
	virtual void AddActorInternal_Static_Implementation(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& ActorRepInfo, bool IsDormancyDriven);

	virtual void RemoveActorInternal_Dynamic(const FNewReplicatedActorInfo& Actor);
	virtual void RemoveActorInternal_Static(const FNewReplicatedActorInfo& Actor, FGlobalActorReplicationInfo& ActorRepInfo, bool WasAddedAsDormantActor);

private:

	/** Called when an actor is out of spatial bounds */
	void HandleActorOutOfSpatialBounds(AActor* Actor, const FVector& Location3D, const bool bStaticActor);

	// Classmap of actor classes which CANNOT force a rebuild of the spatialization tree. They will be clamped instead. E.g, projectiles.
	TClassMap<bool> RebuildSpatialBlacklistMap;
	
	struct FActorCellInfo
	{
		bool IsValid() const { return StartX != -1; }
		void Reset() { StartX = -1; }
		int32 StartX=-1;
		int32 StartY;
		int32 EndX;
		int32 EndY;
	};
	
	struct FCachedDynamicActorInfo
	{
		FCachedDynamicActorInfo(const FNewReplicatedActorInfo& InInfo) : ActorInfo(InInfo) { }
		FNewReplicatedActorInfo ActorInfo;	
		FActorCellInfo CellInfo;
	};
	
	TMap<FActorRepListType, FCachedDynamicActorInfo> DynamicSpatializedActors;


	struct FCachedStaticActorInfo
	{
		FCachedStaticActorInfo(const FNewReplicatedActorInfo& InInfo, bool bInDormDriven) : ActorInfo(InInfo), bDormancyDriven(bInDormDriven) { }
		FNewReplicatedActorInfo ActorInfo;
		bool bDormancyDriven = false; // This actor will be removed from static actor list if it becomes non dormant.
	};

	TMap<FActorRepListType, FCachedStaticActorInfo> StaticSpatializedActors;

	// Static spatialized actors that were not fully initialized when registering with this node. We differ their placement in the grid until the next frame
	struct FPendingStaticActors
	{
		FPendingStaticActors(const FActorRepListType& InActor, const bool InDormancyDriven) : Actor(InActor), DormancyDriven(InDormancyDriven) { }
		bool operator==(const FActorRepListType& InActor) const { return InActor == Actor; };
		FActorRepListType Actor;
		bool DormancyDriven;
	};
	TArray<FPendingStaticActors> PendingStaticSpatializedActors;

	void OnNetDormancyChange(FActorRepListType Actor, FGlobalActorReplicationInfo& GlobalInfo, ENetDormancy NewVlue, ENetDormancy OldValue);
	
	void PutStaticActorIntoCell(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& ActorRepInfo, bool bDormancyDriven);

	UReplicationGraphNode_GridCell* GetCellNode(UReplicationGraphNode_GridCell*& NodePtr)
	{
		if (NodePtr == nullptr)
		{
			if (CreateCellNodeOverride)
			{
				NodePtr = CreateCellNodeOverride(this);
			}
			else
			{
				NodePtr = CreateChildNode<UReplicationGraphNode_GridCell>();
			}
		}

		return NodePtr;
	}

	TArray< TArray<UReplicationGraphNode_GridCell*> > Grid;

	TArray<UReplicationGraphNode_GridCell*>& GetGridX(int32 X)
	{
		if (Grid.Num() <= X)
		{
			Grid.SetNum(X+1);
		}
		return Grid[X];
	}

	UReplicationGraphNode_GridCell*& GetCell(TArray<UReplicationGraphNode_GridCell*>& GridX, int32 Y)
	{
		if (GridX.Num() <= Y)
		{
			GridX.SetNum(Y+1);
		}
		return GridX[Y];
	}			

	bool bNeedsRebuild = false;

	void GetGridNodesForActor(FActorRepListType Actor, const FGlobalActorReplicationInfo& ActorRepInfo, TArray<UReplicationGraphNode_GridCell*>& OutNodes);
	void GetGridNodesForActor(FActorRepListType Actor, const UReplicationGraphNode_GridSpatialization2D::FActorCellInfo& CellInfo, TArray<UReplicationGraphNode_GridCell*>& OutNodes);

	FActorCellInfo GetCellInfoForActor(FActorRepListType Actor, const FVector& Location3D, float CullDistanceSquared);

	// This is a reused TArray for gathering actor nodes. Just to prevent using a stack based TArray everywhere or static/reset patten.
	TArray<UReplicationGraphNode_GridCell*> GatheredNodes;

	friend class AReplicationGraphDebugActor;
};

// -----------------------------------


UCLASS()
class REPLICATIONGRAPH_API UReplicationGraphNode_AlwaysRelevant : public UReplicationGraphNode
{
	GENERATED_BODY()

public:

	UReplicationGraphNode_AlwaysRelevant();

	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor) override { }
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound=true) override { return false; }
	virtual void NotifyResetAllNetworkActors() override { }
	virtual void PrepareForReplication() override;
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	void AddAlwaysRelevantClass(UClass* Class);

protected:

	UPROPERTY()
	UReplicationGraphNode* ChildNode = nullptr;

	// Explicit list of classes that are always relevant. This 
	TArray<UClass*>	AlwaysRelevantClasses;
};

// -----------------------------------

/** Adds actors that are always relevant for a connection. This engine version just adds the PlayerController and ViewTarget (usually the pawn) */
UCLASS()
class REPLICATIONGRAPH_API UReplicationGraphNode_AlwaysRelevant_ForConnection : public UReplicationGraphNode_ActorList
{
	GENERATED_BODY()

public:
	
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	/** Rebuilt-every-frame list based on UNetConnection state */
	FActorRepListRefView ReplicationActorList;

	UPROPERTY()
	AActor* LastViewer = nullptr;
	
	UPROPERTY()
	AActor* LastViewTarget = nullptr;
};

// -----------------------------------


USTRUCT()
struct FTearOffActorInfo
{
	GENERATED_BODY()
	FTearOffActorInfo() : TearOffFrameNum(0), Actor(nullptr), bHasReppedOnce(false) { }
	FTearOffActorInfo(AActor* InActor, uint32 InTearOffFrameNum) : TearOffFrameNum(InTearOffFrameNum), Actor(InActor), bHasReppedOnce(false) { }

	uint32 TearOffFrameNum;

	UPROPERTY()
	AActor* Actor;

	bool bHasReppedOnce;
};

/** Adds actors that are always relevant for a connection. This engine version just adds the PlayerController and ViewTarget (usually the pawn) */
UCLASS()
class REPLICATIONGRAPH_API UReplicationGraphNode_TearOff_ForConnection : public UReplicationGraphNode
{
	GENERATED_BODY()

public:

	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo) override { }
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound=true) override { return false; }
	virtual void NotifyResetAllNetworkActors() override { TearOffActors.Reset(); }
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;
	virtual void LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const override;

	void NotifyTearOffActor(AActor* Actor, uint32 FrameNum);

	// Fixme: not safe to have persistent FActorRepListrefViews yet, so need a uproperty based list to hold the persistent items.
	UPROPERTY()
	TArray<FTearOffActorInfo> TearOffActors;

	FActorRepListRefView ReplicationActorList;
};

// -----------------------------------

/** Manages actor replication for an entire world / net driver */
UCLASS(transient, config=Engine)
class REPLICATIONGRAPH_API UReplicationGraph : public UReplicationDriver
{
	GENERATED_BODY()

public:

	UReplicationGraph();

	/** The per-connection manager class to instantiate. This will be read off the instantiated UNetReplicationManager. */
	UPROPERTY(Config)
	TSubclassOf<UNetReplicationGraphConnection> ReplicationConnectionManagerClass;

	UPROPERTY()
	UNetDriver*	NetDriver;

	/** List of connection managers. This list is not sorted and not stable. */
	UPROPERTY()
	TArray<UNetReplicationGraphConnection*> Connections;

	/** ConnectionManagers that we have created but haven't officially been added to the net driver's ClientConnection list. This is a  hack for silly order of initialization issues. */
	UPROPERTY()
	TArray<UNetReplicationGraphConnection*> PendingConnections;

	/** The max distance between an FActorDestructionInfo and a connection that we will replicate. */
	float DestructInfoMaxDistanceSquared = 15000.f * 15000.f;

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface
	
	// --------------------------------------------------------------

	//~ Begin UReplicationDriver Interface
	virtual void SetRepDriverWorld(UWorld* InWorld) override;
	virtual void InitForNetDriver(UNetDriver* InNetDriver) override;
	virtual void InitializeActorsInWorld(UWorld* InWorld) override;
	virtual void ResetGameWorldState() override { }

	/** Called by the NetDriver when the client connection is ready/added to the NetDriver's client connection list */
	virtual void AddClientConnection(UNetConnection* NetConnection) override;
	virtual void RemoveClientConnection(UNetConnection* NetConnection) override;
	virtual void AddNetworkActor(AActor* Actor) override;
	virtual void RemoveNetworkActor(AActor* Actor) override;
	virtual void ForceNetUpdate(AActor* Actor) override;
	virtual void FlushNetDormancy(AActor* Actor, bool bWasDormInitial) override;
	virtual void NotifyActorTearOff(AActor* Actor) override;
	virtual void NotifyActorFullyDormantForConnection(AActor* Actor, UNetConnection* Connection) override;
	virtual void NotifyActorDormancyChange(AActor* Actor, ENetDormancy OldDormancyState) override;
	virtual bool ProcessRemoteFunction(class AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, class UObject* SubObject) override;
	virtual int32 ServerReplicateActors(float DeltaSeconds) override;
	virtual void PostTickDispatch() override;
	//~ End UReplicationDriver Interface

	virtual void RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo);

	virtual void RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo);

	bool IsConnectionReady(UNetConnection* Connection);

	// --------------------------------------------------------------

	/** Creates a new node for the graph. This and UReplicationNode::CreateChildNode should be the only things that create the graph node UObjects */
	template< class T >
	T* CreateNewNode()
	{
		T* NewNode = NewObject<T>(this);
		InitNode(NewNode);
		return NewNode;
	}

	/** Add a global node to the root that will have a chance to emit actor rep lists for all connections */
	void AddGlobalGraphNode(UReplicationGraphNode* GraphNode);
	
	/** Associate a node to a specific connection. When this connection replicates, this GraphNode will get a chance to add. */
	void AddConnectionGraphNode(UReplicationGraphNode* GraphNode, UNetReplicationGraphConnection* ConnectionManager);
	void AddConnectionGraphNode(UReplicationGraphNode* GraphNode, UNetConnection* NetConnection) { AddConnectionGraphNode(GraphNode, FindOrAddConnectionManager(NetConnection)); }

	void RemoveConnectionGraphNode(UReplicationGraphNode* GraphNode, UNetReplicationGraphConnection* ConnectionManager);
	void RemoveConnectionGraphNode(UReplicationGraphNode* GraphNode, UNetConnection* NetConnection) { RemoveConnectionGraphNode(GraphNode, FindOrAddConnectionManager(NetConnection)); }

	// --------------------------------------------------------------

	virtual UWorld* GetWorld() const override final { return GraphGlobals.IsValid() ? GraphGlobals->World : nullptr; }

	virtual void LogGraph(FReplicationGraphDebugInfo& DebugInfo) const;
	virtual void LogGlobalGraphNodes(FReplicationGraphDebugInfo& DebugInfo) const;
	virtual void LogConnectionGraphNodes(FReplicationGraphDebugInfo& DebugInfo) const;

	const TSharedPtr<FReplicationGraphGlobalData>& GetGraphGlobals() const { return GraphGlobals; }

	uint32 GetReplicationGraphFrame() const { return ReplicationGraphFrame; }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	virtual class AReplicationGraphDebugActor* CreateDebugActor() const;
#endif


	/** Prioritization Constants: these affect how the final priority of an actor is calculated in the prioritize phase */
	struct FPrioritizationConstants
	{
		float MaxDistanceScaling = 3000.f * 3000.f;		// Distance scaling for prioritization scales up to this distance, everything passed this distance is the same or "capped"
		uint32 MaxFramesSinceLastRep = 20;				// Time since last rep scales up to this
		
	};
	FPrioritizationConstants PrioritizationConstants;

	struct FFastSharedPathConstants
	{
		float DistanceRequirementPct = 0.1f;	// Must be this close, as a factor of cull distance *squared*, to use fast shared replication path
		int32 MaxBitsPerFrame = 2048;			// 5kBytes/sec @ 20hz
		int32 ListSkipPerFrame = 3;
	};
	FFastSharedPathConstants FastSharedPathConstants;

	/** Invoked when a rep list is requested that exceeds the size of the preallocated lists */
	static TFunction<void(int32)> OnListRequestExceedsPooledSize;

	// --------------------------------------------------------------

	int64 ReplicateSingleActor(AActor* Actor, FConnectionReplicationActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalActorInfo, FPerConnectionActorInfoMap& ConnectionActorInfoMap, UNetConnection* NetConnection, const uint32 FrameNum);
	int64 ReplicateSingleActor_FastShared(AActor* Actor, FConnectionReplicationActorInfo& ConnectionData, FGlobalActorReplicationInfo& GlobalActorInfo, UNetConnection* NetConnection, const uint32 FrameNum);

	void UpdateActorChannelCloseFrameNum(AActor* Actor, FConnectionReplicationActorInfo& ConnectionData, const FGlobalActorReplicationInfo& GlobalData, const uint32 FrameNum, UNetConnection* NetConnection) const;

protected:

	virtual void InitializeForWorld(UWorld* World);

	virtual void InitNode(UReplicationGraphNode* Node);

	/** Override this function to initialize the per-class data for replication */
	virtual void InitGlobalActorClassSettings();

	/** Override this function to init/configure your project's Global Graph */
	virtual void InitGlobalGraphNodes();

	/** Override this function to init/configure graph for a specific connection. Note they do not all have to be unique: connections can share nodes (e.g, 2 nodes for 2 teams) */
	virtual void InitConnectionGraphNodes(UNetReplicationGraphConnection* ConnectionManager);

	UNetReplicationGraphConnection* FindOrAddConnectionManager(UNetConnection* NetConnection);

	void HandleStarvedActorList(const FPrioritizedRepList& List, int32 StartIdx, FPerConnectionActorInfoMap& ConnectionActorInfoMap, uint32 FrameNum);

	/** How long, in frames, without replicating before an actor channel is closed on a connection. This is a global value added to the individual actor's ActorChannelFrameTimeout */
	uint32 GlobalActorChannelFrameNumTimeout;

	TSharedPtr<FReplicationGraphGlobalData> GraphGlobals;

	/** Temporary List we use while prioritizing actors */
	FPrioritizedRepList PrioritizedReplicationList;
	
	/** A list of nodes that can add actors to all connections. They don't necessarily *have to* add actors to each connection, but they will get a chance to. These are added via ::AddGlobalGraphNode  */
	UPROPERTY()
	TArray<UReplicationGraphNode*> GlobalGraphNodes;

	/** A list of nodes that want PrepareForReplication() to be called on them at the top of the replication frame. */
	UPROPERTY()
	TArray<UReplicationGraphNode*> PrepareForReplicationNodes;
	
	FGlobalActorReplicationInfoMap GlobalActorReplicationInfoMap;

	/** The authoritative set of "what actors are in the graph" */
	TSet<AActor*> ActiveNetworkActors;

	/** Special case handling of specific RPCs. Currently supports immediate send/flush for multicasts */
	TMap<FObjectKey /** UFunction* */, FRPCSendPolicyInfo> RPCSendPolicyMap;
	TClassMap<bool> RPC_Multicast_OpenChannelForClass; // Set classes to true here to open channel for them when receiving multicast RPC (and within cull distance range)

	FReplicationGraphCSVTracker CSVTracker;

	FOutBunch* FastSharedReplicationBunch = nullptr;
	class UActorChannel* FastSharedReplicationChannel = nullptr;

#if REPGRAPH_DETAILS
	bool bEnableFullActorPrioritizationDetailsAllConnections = false;
#endif

	/** Default Replication Path */
	void ReplicateActorListsForConnection_Default(UNetReplicationGraphConnection* ConnectionManager, FGatheredReplicationActorLists& GatheredReplicationListsForConnection, FNetViewer& Viewer);

	/** "FastShared" Replication Path */
	void ReplicateActorListsForConnection_FastShared(UNetReplicationGraphConnection* ConnectionManager, FGatheredReplicationActorLists& GatheredReplicationListsForConnection, FNetViewer& Viewer);

	/** Connections needing a FlushNet in PostTickDispatch */
	TArray<UNetConnection*> ConnectionsNeedingsPostTickDispatchFlush;

private:

	/** Internal frame counter for replication. This is only updated by us. The one of UNetDriver can be updated by RPC calls and is only used to invalidate shared property CLs/serialiation data. */
	uint32 ReplicationGraphFrame = 0;

	UNetReplicationGraphConnection* CreateClientConnectionManagerInternal(UNetConnection* Connection);

	friend class AReplicationGraphDebugActor;
};

// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------


/** Manages actor replication for a specific connection */
UCLASS(transient)
class REPLICATIONGRAPH_API UNetReplicationGraphConnection : public UReplicationConnectionDriver
{
	GENERATED_BODY()

public:

	UNetReplicationGraphConnection();

	UPROPERTY()
	UNetConnection* NetConnection;

	/** A map of all of our per-actor data */
	FPerConnectionActorInfoMap ActorInfoMap;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPostReplicatePrioritizedLists, UNetReplicationGraphConnection*, FPrioritizedRepList*);
	FOnPostReplicatePrioritizedLists OnPostReplicatePrioritizeLists;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnClientVisibleLevelNamesAdd, FName, UWorld*);
	FOnClientVisibleLevelNamesAdd OnClientVisibleLevelNameAdd;	// Global Delegate, will be called for every level
	TMap<FName, FOnClientVisibleLevelNamesAdd> OnClientVisibleLevelNameAddMap; // LevelName lookup map delegates

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnClientVisibleLevelNamesRemove, FName);
	FOnClientVisibleLevelNamesRemove OnClientVisibleLevelNameRemove;


#if REPGRAPH_DETAILS
	bool bEnableFullActorPrioritizationDetails = false;
#endif

	UPROPERTY()
	class AReplicationGraphDebugActor* DebugActor = nullptr;

	bool bEnableDebugging;

	// ID that is assigned by the replication graph. Will be reassigned/compacted as clients disconnect. Useful for spacing out connection operations. E.g., not stable but always compact.
	int32 ConnectionId; 

	FVector LastGatherLocation;

	/** Returns connection graph nodes. This is const so that you do not mutate the array itself. You should use AddConnectionGraphNode/RemoveConnectionGraphNode.  */
	const TArray<UReplicationGraphNode*>& GetConnectionGraphNodes() { return ConnectionGraphNodes; }

	virtual void NotifyAddDormantDestructionInfo(AActor* Actor) override;

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	//~ Begin UReplicationConnectionDriver Interface
	virtual void TearDown() override;

private:

	friend UReplicationGraph;

	virtual void NotifyActorChannelAdded(AActor* Actor, class UActorChannel* Channel) override;

	virtual void NotifyActorChannelRemoved(AActor* Actor) override;

	virtual void NotifyActorChannelCleanedUp(UActorChannel* Channel) override;

	virtual void NotifyAddDestructionInfo(FActorDestructionInfo* DestructInfo) override;

	virtual void NotifyRemoveDestructionInfo(FActorDestructionInfo* DestructInfo) override;

	virtual void NotifyResetDestructionInfo() override;

	virtual void NotifyClientVisibleLevelNamesAdd(FName LevelName, UWorld* StreamingWorld) override;

	virtual void NotifyClientVisibleLevelNamesRemove(FName LevelName) override { OnClientVisibleLevelNameRemove.Broadcast(LevelName); }
	//~ End UReplicationConnectionDriver Interface

	// ----------------------------------------

	/** Called right after this is created to associate with the owning Graph */
	void InitForGraph(UReplicationGraph* Graph);

	/** Called after InitForGraph is called to associate this connection manager with a net connection */
	void InitForConnection(UNetConnection* Connection);

	/** Adds a node to this connection manager */
	void AddConnectionGraphNode(UReplicationGraphNode* Node);

	/** Remove a node to this connection manager */
	void RemoveConnectionGraphNode(UReplicationGraphNode* Node);

	bool PrepareForReplication();

	int64 ReplicateDestructionInfos(const FVector& ConnectionViewLocation, const float DestructInfoMaxDistanceSquared);
	
	int64 ReplicateDormantDestructionInfos();

	UPROPERTY()
	TArray<UReplicationGraphNode*> ConnectionGraphNodes;

	UPROPERTY()
	UReplicationGraphNode_TearOff_ForConnection* TearOffNode;
	
	/** DestructionInfo handling. This is how we send "delete this actor" to clients when the actor is deleted on the server (placed in map actors) */
	struct FCachedDestructInfo
	{
		FCachedDestructInfo(FActorDestructionInfo* InDestructInfo) : DestructionInfo(InDestructInfo), CachedPosition(InDestructInfo->DestroyedPosition) {}
		bool operator==(const FActorDestructionInfo* InDestructInfo) const { return InDestructInfo == DestructionInfo; };
		
		FActorDestructionInfo* DestructionInfo;
		FVector CachedPosition;

		void CountBytes(FArchive& Ar) const
		{
			if (DestructionInfo)
			{
				Ar.CountBytes(sizeof(FActorDestructionInfo), sizeof(FActorDestructionInfo));
				DestructionInfo->CountBytes(Ar);
			}
		}
	};

	TArray<FCachedDestructInfo> PendingDestructInfoList;
	TSet<FActorDestructionInfo*> TrackedDestructionInfoPtrs; // Set used to guard against double adds into PendingDestructInfoList

	struct FCachedDormantDestructInfo
	{
		TWeakObjectPtr<ULevel> Level;
		TWeakObjectPtr<UObject> ObjOuter;
		FNetworkGUID NetGUID;
		FString PathName;
	};

	TArray<FCachedDormantDestructInfo> PendingDormantDestructList;
};

// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------


/** Specialized actor for replicating debug information about replication to specific connections. This actor is never spawned in shipping builds and never counts towards bandwidth limits */
UCLASS(NotPlaceable, Transient)
class REPLICATIONGRAPH_API AReplicationGraphDebugActor : public AActor
{
	GENERATED_BODY()

public:

	AReplicationGraphDebugActor()
	{
		bReplicates = true; // must be set for RPCs to be sent
	}

	// To prevent demo netdriver from replicating.
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override { return false; }
	virtual bool IsReplayRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation, const float CullDistanceSquared) const override { return false; }

	UPROPERTY()
	UReplicationGraph* ReplicationGraph;

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStartDebugging();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStopDebugging();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerCellInfo();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerPrintAllActorInfo(const FString& Str);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetCullDistanceForClass(UClass* Class, float CullDistance);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetPeriodFrameForClass(UClass* Class, int32 PeriodFrame);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetConditionalActorBreakpoint(AActor* Actor);

	UFUNCTION(Client, Reliable)
	void ClientCellInfo(FVector CellLocation, FVector CellExtent, const TArray<AActor*>& Actors);

	void PrintCullDistances();

	void PrintAllActorInfo(FString MatchString);


	UPROPERTY()
	UNetReplicationGraphConnection* ConnectionManager;

	virtual UNetConnection* GetNetConnection() const override;
};
