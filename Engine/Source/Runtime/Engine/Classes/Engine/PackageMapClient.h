// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/**
 * PackageMap implementation that is client/connection specific. This subclass implements all NetGUID Acking and interactions with a UConnection.
 *	On the server, each client will have their own instance of UPackageMapClient.
 *
 *	UObjects are first serialized as <NetGUID, Name/Path> pairs. UPackageMapClient tracks each NetGUID's usage and knows when a NetGUID has been ACKd.
 *	Once ACK'd, objects are just serialized as <NetGUID>. The result is higher bandwidth usage upfront for new clients, but minimal bandwidth once
 *	things gets going.
 *
 *	A further optimization is enabled by default. References will actually be serialized via:
 *	<NetGUID, <(Outer *), Object Name> >. Where Outer * is a reference to the UObject's Outer.
 *
 *	The main advantages from this are:
 *		-Flexibility. No precomputed net indices or large package lists need to be exchanged for UObject serialization.
 *		-Cross version communication. The name is all that is needed to exchange references.
 *		-Efficiency in that a very small % of UObjects will ever be serialized. Only Objects that serialized are assigned NetGUIDs.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/NetworkGuid.h"
#include "Misc/NetworkVersion.h"
#include "UObject/CoreNet.h"
#include "Net/DataBunch.h"
#include "PackageMapClient.generated.h"

class UNetConnection;
class UNetDriver;

PRAGMA_DISABLE_DEPRECATION_WARNINGS	// 4.22, Name, Type
class ENGINE_API FNetFieldExport
{
public:
	FNetFieldExport() : bExported( false ), Handle( 0 ), CompatibleChecksum( 0 ), bIncompatible( false )
	{
	}

	UE_DEPRECATED(4.22, "Type is no longer required, please use other constructor.")
	FNetFieldExport( const uint32 InHandle, const uint32 InCompatibleChecksum, const FString InName, FString InType ) :
		bExported( false ),
		Handle( InHandle ),
		CompatibleChecksum( InCompatibleChecksum ),
		ExportName( *InName ),
		Name( InName ),
		Type( InType ),
		bIncompatible( false )
	{
	}

	FNetFieldExport( const uint32 InHandle, const uint32 InCompatibleChecksum, const FName& InName ) :
		bExported( false ),
		Handle( InHandle ),
		CompatibleChecksum( InCompatibleChecksum ),
		ExportName( InName ),
		bIncompatible( false )
	{
	}

	friend FArchive& operator<<( FArchive& Ar, FNetFieldExport& C )
	{
		uint8 Flags = C.bExported ? 1 : 0;

		Ar << Flags;

		if ( Ar.IsLoading() )
		{
			C.bExported = (Flags == 1);
		}

		if ( C.bExported )
		{
			Ar.SerializeIntPacked( C.Handle );
			Ar << C.CompatibleChecksum;

			if (Ar.IsLoading() && Ar.EngineNetVer() < HISTORY_NETEXPORT_SERIALIZATION)
			{
				Ar << C.Name;
				Ar << C.Type;

				C.ExportName = FName(*C.Name);
			}
			else
			{
				if (Ar.IsLoading() && Ar.EngineNetVer() < HISTORY_NETEXPORT_SERIALIZE_FIX)
				{
					Ar << C.ExportName;
				}
				else
				{
					UPackageMap::StaticSerializeName(Ar, C.ExportName);
				}

				if (Ar.IsLoading())
				{
					C.Name = C.ExportName.ToString();
				}
			}
		}

		return Ar;
	}

	void CountBytes(FArchive& Ar) const;

	bool			bExported;
	uint32			Handle;
	uint32			CompatibleChecksum;
	FName			ExportName;
	UE_DEPRECATED(4.22, "Name is deprecated.")
	FString			Name;
	UE_DEPRECATED(4.22, "Type is deprecated.")
	FString			Type;

	// Transient properties
	mutable bool	bIncompatible;		// If true, we've already determined that this property isn't compatible. We use this to curb warning spam.
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

class ENGINE_API FNetFieldExportGroup
{
public:
	FNetFieldExportGroup() : PathNameIndex( 0 ) { }

	FString						PathName;
	uint32						PathNameIndex;
	TArray< FNetFieldExport >	NetFieldExports;

	friend FArchive& operator<<( FArchive& Ar, FNetFieldExportGroup& C )
	{
		Ar << C.PathName;

		Ar.SerializeIntPacked( C.PathNameIndex );

		uint32 NumNetFieldExports = C.NetFieldExports.Num();
		Ar.SerializeIntPacked( NumNetFieldExports );

		if ( Ar.IsLoading() )
		{
			C.NetFieldExports.AddDefaulted( ( int32 )NumNetFieldExports );
		}

		for ( int32 i = 0; i < C.NetFieldExports.Num(); i++ )
		{
			Ar << C.NetFieldExports[i];
		}

		return Ar;
	}

	int32 FindNetFieldExportHandleByChecksum( const uint32 Checksum )
	{
		for ( int32 i = 0; i < NetFieldExports.Num(); i++ )
		{
			if ( NetFieldExports[i].CompatibleChecksum == Checksum )
			{
				return i;
			}
		}

		return -1;
	}

	void CountBytes(FArchive& Ar) const;
};

/** Stores an object with path associated with FNetworkGUID */
class FNetGuidCacheObject
{
public:
	FNetGuidCacheObject() : NetworkChecksum( 0 ), ReadOnlyTimestamp( 0 ), bNoLoad( 0 ), bIgnoreWhenMissing( 0 ), bIsPending( 0 ), bIsBroken( 0 )
	{
	}

	TWeakObjectPtr< UObject >	Object;

	// These fields are set when this guid is static
	FNetworkGUID				OuterGUID;
	FName						PathName;
	uint32						NetworkChecksum;			// Network checksum saved, used to determine backwards compatible

	double						ReadOnlyTimestamp;			// Time in second when we should start timing out after going read only

	uint8						bNoLoad				: 1;	// Don't load this, only do a find
	uint8						bIgnoreWhenMissing	: 1;	// Don't warn when this asset can't be found or loaded
	uint8						bIsPending			: 1;	// This object is waiting to be fully loaded
	uint8						bIsBroken			: 1;	// If this object failed to load, then we set this to signify that we should stop trying
};

class ENGINE_API FNetGUIDCache
{
public:
	FNetGUIDCache( UNetDriver * InDriver );

	enum class ENetworkChecksumMode
	{
		None			= 0,		// Don't use checksums
		SaveAndUse		= 1,		// Save checksums in stream, and use to validate while loading packages
		SaveButIgnore	= 2,		// Save checksums in stream, but ignore when loading packages
	};

	enum class EAsyncLoadMode
	{
		UseCVar			= 0,		// Use CVar (net.AllowAsyncLoading) to determine if we should async load
		ForceDisable	= 1,		// Disable async loading
		ForceEnable		= 2,		// Force enable async loading
	};

	void			CleanReferences();
	bool			SupportsObject( const UObject* Object, const TWeakObjectPtr<UObject>* WeakObjectPtr=nullptr /** Optional: pass in existing weakptr to prevent this function from constructing one internally */ ) const;
	bool			IsDynamicObject( const UObject* Object );
	bool			IsNetGUIDAuthority() const;
	FNetworkGUID	GetOrAssignNetGUID( UObject* Object, const TWeakObjectPtr<UObject>* WeakObjectPtr=nullptr /** Optional: pass in existing weakptr to prevent this function from constructing one internally */ );
	FNetworkGUID	GetNetGUID( const UObject* Object ) const;
	FNetworkGUID	GetOuterNetGUID( const FNetworkGUID& NetGUID ) const;
	FNetworkGUID	AssignNewNetGUID_Server( UObject* Object );
	FNetworkGUID	AssignNewNetGUIDFromPath_Server( const FString& PathName, UObject* ObjOuter, UClass* ObjClass );
	void			RegisterNetGUID_Internal( const FNetworkGUID& NetGUID, const FNetGuidCacheObject& CacheObject );
	void			RegisterNetGUID_Server( const FNetworkGUID& NetGUID, UObject* Object );
	void			RegisterNetGUID_Client( const FNetworkGUID& NetGUID, const UObject* Object );
	void			RegisterNetGUIDFromPath_Client( const FNetworkGUID& NetGUID, const FString& PathName, const FNetworkGUID& OuterGUID, const uint32 NetworkChecksum, const bool bNoLoad, const bool bIgnoreWhenMissing );
	void			RegisterNetGUIDFromPath_Server( const FNetworkGUID& NetGUID, const FString& PathName, const FNetworkGUID& OuterGUID, const uint32 NetworkChecksum, const bool bNoLoad, const bool bIgnoreWhenMissing );
	UObject *		GetObjectFromNetGUID( const FNetworkGUID& NetGUID, const bool bIgnoreMustBeMapped );
	bool			ShouldIgnoreWhenMissing( const FNetworkGUID& NetGUID ) const;
	bool			IsGUIDRegistered( const FNetworkGUID& NetGUID ) const;
	bool			IsGUIDLoaded( const FNetworkGUID& NetGUID ) const;
	bool			IsGUIDBroken( const FNetworkGUID& NetGUID, const bool bMustBeRegistered ) const;
	bool			IsGUIDNoLoad( const FNetworkGUID& NetGUID ) const;
	bool			IsGUIDPending( const FNetworkGUID& NetGUID ) const;
	FString			FullNetGUIDPath( const FNetworkGUID& NetGUID ) const;
	void			GenerateFullNetGUIDPath_r( const FNetworkGUID& NetGUID, FString& FullPath ) const;
	uint32			GetClassNetworkChecksum( UClass* Class );
	uint32			GetNetworkChecksum( UObject* Obj );
	void			SetNetworkChecksumMode( const ENetworkChecksumMode NewMode );
	void			SetAsyncLoadMode( const EAsyncLoadMode NewMode );
	bool			ShouldAsyncLoad() const;
	bool			CanClientLoadObject( const UObject* Object, const FNetworkGUID& NetGUID ) const;

	void			AsyncPackageCallback(const FName& PackageName, UPackage * Package, EAsyncLoadingResult::Type Result);

	void			ResetCacheForDemo();

	void			CountBytes(FArchive& Ar) const;

	TMap< FNetworkGUID, FNetGuidCacheObject >		ObjectLookup;
	TMap< TWeakObjectPtr< UObject >, FNetworkGUID >	NetGUIDLookup;
	int32											UniqueNetIDs[2];

	TSet< FNetworkGUID >							ImportedNetGuids;
	TMap< FNetworkGUID, TSet< FNetworkGUID > >		PendingOuterNetGuids;

	bool											IsExportingNetGUIDBunch;

	UNetDriver *									Driver;

	TMap< FName, FNetworkGUID >						PendingAsyncPackages;

	ENetworkChecksumMode							NetworkChecksumMode;
	EAsyncLoadMode									AsyncLoadMode;

private:

	friend class UPackageMapClient;

	/** Maps net field export group name to the respective FNetFieldExportGroup */
	TMap < FString, TSharedPtr< FNetFieldExportGroup > >	NetFieldExportGroupMap;

	/** Maps field export group path to assigned index */
	TMap < FString, uint32 >								NetFieldExportGroupPathToIndex;

	/** Maps assigned net field export group index to pointer to group, lifetime of the referenced FNetFieldExportGroups are managed by NetFieldExportGroupMap **/
	TMap < uint32, FNetFieldExportGroup* >					NetFieldExportGroupIndexToGroup;

	/** Current index used when filling in NetFieldExportGroupPathToIndex/NetFieldExportGroupIndexToPath */
	int32													UniqueNetFieldExportGroupPathIndex;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
public:
	// History for debugging entries in the guid cache
	TMap<FNetworkGUID, FString>						History;
private:
#endif
};

class ENGINE_API FPackageMapAckState
{
public:
	TMap< FNetworkGUID, int32 >	NetGUIDAckStatus;				// Map that represents the ack state of each net guid for this connection
	TSet< uint32 >				NetFieldExportGroupPathAcked;	// Map that represents whether or not a net field export group has been ack'd by the client
	TSet< uint64 >				NetFieldExportAcked;			// Map that represents whether or not a net field export has been ack'd by the client

	void Reset()
	{
		NetGUIDAckStatus.Empty();
		NetFieldExportGroupPathAcked.Empty();
		NetFieldExportAcked.Empty();
	}

	void CountBytes(FArchive& Ar) const;
};

UCLASS(transient)
class ENGINE_API UPackageMapClient : public UPackageMap
{
public:
	GENERATED_BODY()

	UPackageMapClient(const FObjectInitializer & ObjectInitializer = FObjectInitializer::Get());

	void Initialize(UNetConnection * InConnection, TSharedPtr<FNetGUIDCache> InNetGUIDCache)
	{
		Connection = InConnection;
		GuidCache = InNetGUIDCache;
		ExportNetGUIDCount = 0;
		OverrideAckState = &AckState;
	}

	virtual ~UPackageMapClient()
	{
		if (CurrentExportBunch)
		{
			delete CurrentExportBunch;
			CurrentExportBunch = NULL;
		}
	}

	
	// UPackageMap Interface
	virtual bool SerializeObject( FArchive& Ar, UClass* InClass, UObject*& Obj, FNetworkGUID *OutNetGUID = NULL ) override;
	virtual bool SerializeNewActor( FArchive& Ar, class UActorChannel *Channel, class AActor*& Actor) override;
	
	virtual bool WriteObject( FArchive& Ar, UObject* InOuter, FNetworkGUID NetGUID, FString ObjName ) override;

	// UPackageMapClient Connection specific methods

	bool NetGUIDHasBeenAckd(FNetworkGUID NetGUID);

	virtual void ReceivedNak( const int32 NakPacketId ) override;
	virtual void ReceivedAck( const int32 AckPacketId ) override;
	virtual void NotifyBunchCommit( const int32 OutPacketId, const FOutBunch* OutBunch ) override;
	virtual void GetNetGUIDStats(int32 &AckCount, int32 &UnAckCount, int32 &PendingCount) override;

	void ReceiveNetGUIDBunch( FInBunch &InBunch );
	void AppendExportBunches(TArray<FOutBunch *>& OutgoingBunches);

	void AppendExportData(FArchive& Archive);
	void ReceiveExportData(FArchive& Archive);

	TMap<FNetworkGUID, int32>	NetGUIDExportCountMap;	// How many times we've exported each NetGUID on this connection. Public for ListNetGUIDExports 

	void HandleUnAssignedObject( UObject* Obj );

	static void	AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	virtual void NotifyStreamingLevelUnload(UObject* UnloadedLevel) override;

	virtual bool PrintExportBatch() override;

	virtual void			LogDebugInfo( FOutputDevice & Ar) override;
	virtual UObject *		GetObjectFromNetGUID( const FNetworkGUID& NetGUID, const bool bIgnoreMustBeMapped ) override;
	virtual FNetworkGUID	GetNetGUIDFromObject( const UObject* InObject) const override;
	virtual bool			IsGUIDBroken( const FNetworkGUID& NetGUID, const bool bMustBeRegistered ) const override { return GuidCache->IsGUIDBroken( NetGUID, bMustBeRegistered ); }

	/** Returns true if this guid is directly pending, or depends on another guid that is pending */
	virtual bool			IsGUIDPending( const FNetworkGUID& NetGUID ) const;

	/** Set rather this actor is associated with a channel with queued bunches */
	virtual void			SetHasQueuedBunches( const FNetworkGUID& NetGUID, bool bHasQueuedBunches );

	TArray< FNetworkGUID > & GetMustBeMappedGuidsInLastBunch() { return MustBeMappedGuidsInLastBunch; }

	class UNetConnection* GetConnection() { return Connection; }

	void SyncPackageMapExportAckStatus( const UPackageMapClient* Source );

	void SavePackageMapExportAckStatus( FPackageMapAckState& OutState );
	void RestorePackageMapExportAckStatus( const FPackageMapAckState& InState );
	void OverridePackageMapExportAckStatus( FPackageMapAckState* NewState );

	/** Functions to help with exporting/importing net field info */
	TSharedPtr< FNetFieldExportGroup >	GetNetFieldExportGroup( const FString& PathName );
	void								AddNetFieldExportGroup( const FString& PathName, TSharedPtr< FNetFieldExportGroup > NewNetFieldExportGroup );
	void								TrackNetFieldExport( FNetFieldExportGroup* NetFieldExportGroup, const int32 NetFieldExportHandle );
	TSharedPtr< FNetFieldExportGroup >	GetNetFieldExportGroupChecked( const FString& PathName ) const;
	void								SerializeNetFieldExportGroupMap( FArchive& Ar, bool bClearPendingExports=true );

	TUniquePtr<TGuardValue<bool>> ScopedIgnoreReceivedExportGUIDs()
	{
		return MakeUnique<TGuardValue<bool>>(bIgnoreReceivedExportGUIDs, true);
	}

	virtual void Serialize(FArchive& Ar) override;

protected:

	/** Functions to help with exporting/importing net field export info */
	void AppendNetFieldExports( FArchive& Archive );
	void ReceiveNetFieldExports( FArchive& Archive );

	void AppendNetExportGUIDs( FArchive& Archive );
	void ReceiveNetExportGUIDs( FArchive& Archive );

	bool ExportNetGUIDForReplay( FNetworkGUID&, UObject* Object, FString& PathName, UObject* ObjOuter );
	bool ExportNetGUID( FNetworkGUID NetGUID, UObject* Object, FString PathName, UObject* ObjOuter );
	void ExportNetGUIDHeader();

	void			InternalWriteObject( FArchive& Ar, FNetworkGUID NetGUID, UObject* Object, FString ObjectPathName, UObject* ObjectOuter );	
	FNetworkGUID	InternalLoadObject( FArchive & Ar, UObject *& Object, int InternalLoadObjectRecursionCount );

	virtual UObject* ResolvePathAndAssignNetGUID( const FNetworkGUID& NetGUID, const FString& PathName ) override;

	bool	ShouldSendFullPath(const UObject* Object, const FNetworkGUID &NetGUID);
	
	bool IsNetGUIDAuthority() const;

	class UNetConnection* Connection;

	bool ObjectLevelHasFinishedLoading(UObject* Obj);

	TArray<TArray<uint8>>				ExportGUIDArchives;
	TSet< FNetworkGUID >				CurrentExportNetGUIDs;				// Current list of NetGUIDs being written to the Export Bunch.
	TSet< FNetworkGUID >				CurrentQueuedBunchNetGUIDs;			// List of NetGuids with currently queued bunches

	TArray< FNetworkGUID >				PendingAckGUIDs;					// Quick access to all GUID's that haven't been acked

	FPackageMapAckState					AckState;							// Current ack state of exported data
	FPackageMapAckState*				OverrideAckState;					// This is a pointer that allows us to override the current ack state, it's never NULL (it will point to AckState by default)

	// Bunches of NetGUID/path tables to send with the current content bunch
	TArray<FOutBunch* >					ExportBunches;
	FOutBunch *							CurrentExportBunch;

	int32								ExportNetGUIDCount;

	TSharedPtr< FNetGUIDCache >			GuidCache;

	TArray< FNetworkGUID >				MustBeMappedGuidsInLastBunch;

	/** List of net field exports that need to go out on next bunch */
	TSet< uint64 >						NetFieldExports;

private:
	void ReceiveNetFieldExportsCompat(FInBunch& InBunch);

	bool bIgnoreReceivedExportGUIDs;
};
