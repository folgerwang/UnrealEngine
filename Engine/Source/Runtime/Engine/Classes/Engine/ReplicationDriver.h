// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/**
 *	
 *	===================== Replication Driver Interface =====================
 *
 *	Defines an interface for driving actor replication. That is, the system that determines what actors should replicate to what connections.
 *	This is server only (in the traditional server->clients model).
 *	
 *	How to setup a Replication Driver (two ways):
 *	1. Set ReplicationDriverClassName in DefaultEngine.ini
 *	
 *		[/Script/OnlineSubsystemUtils.IpNetDriver]
 *		ReplicationDriverClassName="/Script/MyGame.MyReplicationGraph"
 *
 *	2. Bind to UReplicationDriver::CreateReplicationDriverDelegate(). Do this if you have custom logic for instantiating the driver (e.g, conditional based on map/game mode or hot fix options, etc)
 *	
 *		UReplicationDriver::CreateReplicationDriverDelegate().BindLambda([](UNetDriver* ForNetDriver, const FURL& URL, UWorld* World) -> UReplicationDriver*
 *		{
 *			return NewObject<UMyReplicationDriverClass>(GetTransientPackage());
 *		});	
 *
 */

#pragma once

#include "Delegates/Delegate.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPtr.h"

#include "ReplicationDriver.generated.h"

class AActor;
class UActorChannel;
class UNetConnection;
class UNetDriver;
class UReplicationDriver;
class UWorld;

struct FActorDestructionInfo;
struct FURL;

DECLARE_DELEGATE_RetVal_ThreeParams(UReplicationDriver*, FCreateReplicationDriver, UNetDriver*, const FURL&, UWorld*);

UCLASS(transient, config=Engine)
class ENGINE_API UReplicationDriver :public UObject
{
	GENERATED_BODY()

public:

	UReplicationDriver();

	/** This is the function UNetDriver calls to create its replication driver. It will invoke OnCreateReplicationDriver if set, otherwise will instantiate ReplicationDriverClassName from the NetDriver.  */
	static UReplicationDriver* CreateReplicationDriver(UNetDriver* NetDriver, const FURL& URL, UWorld* World);

	/** Static delegate you can bind to override replication driver creation */
	static FCreateReplicationDriver& CreateReplicationDriverDelegate();

	// -----------------------------------------------------------------------

	virtual void InitForNetDriver(UNetDriver* InNetDriver) PURE_VIRTUAL(UReplicationDriver::InitForNetDriver, );

	virtual void SetWorld(UWorld* InWorld) PURE_VIRTUAL(UReplicationDriver::SetWorld, );

	virtual void ResetGameWorldState() PURE_VIRTUAL(UReplicationDriver::ResetGameWorldState, );

	virtual void AddClientConnection(UNetConnection* NetConnection) PURE_VIRTUAL(UReplicationDriver::AddClientConnection, );

	virtual void RemoveClientConnection(UNetConnection* NetConnection) PURE_VIRTUAL(UReplicationDriver::RemoveClientConnection, );

	virtual void AddNetworkActor(AActor* Actor) PURE_VIRTUAL(UReplicationDriver::AddNetworkActor, );

	virtual void RemoveNetworkActor(AActor* Actor) PURE_VIRTUAL(UReplicationDriver::RemoveNetworkActor, );

	virtual void ForceNetUpdate(AActor* Actor) PURE_VIRTUAL(UReplicationDriver::ForceNetUpdate, );

	virtual void FlushNetDormancy(AActor* Actor, bool WasDormInitial) PURE_VIRTUAL(UReplicationDriver::FlushNetDormancy, );

	virtual void NotifyActorTearOff(AActor* Actor) PURE_VIRTUAL(UReplicationDriver::NotifyActorTearOff, );

	virtual void NotifyActorFullyDormantForConnection(AActor* Actor, UNetConnection* Connection) PURE_VIRTUAL(UReplicationDriver::NotifyActorFullyDormantForConnection, );

	virtual void NotifyActorDormancyChange(AActor* Actor, ENetDormancy OldDormancyState) PURE_VIRTUAL(UReplicationDriver::NotifyActorDormancyChange, );

	/** Handles an RPC. Returns true if it actually handled it. Returning false will cause the rep driver function to handle it instead */
	virtual bool ProcessRemoteFunction(class AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject ) { return false; }

	/** The main function that will actually replicate actors. Called every server tick. */
	virtual int32 ServerReplicateActors(float DeltaSeconds) PURE_VIRTUAL(UReplicationDriver::ServerReplicateActors, return 0; );
};

/** Class/interface for replication extension that is per connection. It is up to the replication driver to create and associate these with a UNetConnection */
UCLASS(transient)
class ENGINE_API UReplicationConnectionDriver :public UObject
{
	GENERATED_BODY()

public:

	virtual void NotifyActorChannelAdded(AActor* Actor, UActorChannel* Channel) PURE_VIRTUAL(UReplicationConnectionDriver::NotifyActorChannelAdded, );

	virtual void NotifyActorChannelRemoved(AActor* Actor) PURE_VIRTUAL(UReplicationConnectionDriver::NotifyActorChannelRemoved, );

	virtual void NotifyActorChannelCleanedUp(UActorChannel* Channel) PURE_VIRTUAL(UReplicationConnectionDriver::NotifyActorChannelCleanedUp, );

	virtual void NotifyAddDestructionInfo(FActorDestructionInfo* DestructInfo) PURE_VIRTUAL(UReplicationConnectionDriver::NotifyAddDestructionInfo, );

	virtual void NotifyRemoveDestructionInfo(FActorDestructionInfo* DestructInfo) PURE_VIRTUAL(UReplicationConnectionDriver::NotifyRemoveDestructionInfo, );

	virtual void NotifyResetDestructionInfo() PURE_VIRTUAL(UReplicationConnectionDriver::NotifyResetDestructionInfo, );

	virtual void NotifyClientVisibleLevelNamesAdd(FName LevelName, UWorld* StreamingWorld) PURE_VIRTUAL(UReplicationConnectionDriver::NotifyClientVisibleLevelNamesAdd, );

	virtual void NotifyClientVisibleLevelNamesRemove(FName LevelName) PURE_VIRTUAL(UReplicationConnectionDriver::NotifyClientVisibleLevelNamesRemove, );
};