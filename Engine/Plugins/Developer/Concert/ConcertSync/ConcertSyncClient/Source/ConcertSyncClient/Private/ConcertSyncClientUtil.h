// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"
#include "Containers/ArrayView.h"

class UObject;
class UStruct;
class ULevel;
class AActor;
class UActorComponent;

class FConcertSyncWorldRemapper;
class FConcertLocalIdentifierTable;

struct FConcertObjectId;
struct FConcertWorldNodeId;
struct FConcertSerializedPropertyData;

namespace ConcertSyncClientUtil
{
	enum class EGetObjectResultFlags : uint8
	{
		None = 0,
		NeedsGC = 1 << 0,
		NeedsPostSpawn = 1 << 1,
	};
	ENUM_CLASS_FLAGS(EGetObjectResultFlags);

	struct FGetObjectResult
	{
		FGetObjectResult()
			: Obj(nullptr)
			, Flags(EGetObjectResultFlags::None)
		{
		}

		explicit FGetObjectResult(UObject* InObj, const EGetObjectResultFlags InFlags = EGetObjectResultFlags::None)
			: Obj(InObj)
			, Flags(InFlags)
		{
		}

		bool NeedsGC() const
		{
			return EnumHasAnyFlags(Flags, EGetObjectResultFlags::NeedsGC);
		}

		bool NeedsPostSpawn() const
		{
			return EnumHasAnyFlags(Flags, EGetObjectResultFlags::NeedsPostSpawn);
		}

		UObject* Obj;
		EGetObjectResultFlags Flags;
	};

	bool CanPerformBlockingAction(const bool bBlockDuringInteraction = true);

	void UpdatePendingKillState(UObject* InObj, const bool bIsPendingKill);

	FGetObjectResult GetObject(const FConcertObjectId& InObjectId, const FName InNewName, const FName InNewOuterPath, const bool bAllowCreate);
	
	bool ImportPropertyData(const FConcertLocalIdentifierTable* InLocalIdentifierTable, const FConcertSyncWorldRemapper& InWorldRemapper, UObject* InObj, const FName InPropertyName, const TArray<uint8>& InSerializedData);

	TArray<FName> GetRootProperties(const TArray<FName>& InChangedProperties);

	UProperty* GetExportedProperty(const UStruct* InStruct, const FName InPropertyName, const bool InIncludeEditorOnlyData);

	void SerializeProperties(FConcertLocalIdentifierTable* InLocalIdentifierTable, const UObject* InObject, const TArray<FName>& InChangedProperties, const bool InIncludeEditorOnlyData, TArray<FConcertSerializedPropertyData>& OutPropertyDatas);

	void SerializeProperty(FConcertLocalIdentifierTable* InLocalIdentifierTable, const UObject* InObject, const UProperty* InProperty, const bool InIncludeEditorOnlyData, TArray<uint8>& OutSerializedData);

	void SerializeObject(FConcertLocalIdentifierTable* InLocalIdentifierTable, const UObject* InObject, const TArray<FName>* InChangedProperties, const bool InIncludeEditorOnlyData, TArray<uint8>& OutSerializedData);

	void FlushPackageLoading(const FName InPackageName);

	void FlushPackageLoading(const FString& InPackageName);

	void HotReloadPackages(TArrayView<const FName> InPackageNames);

	void PurgePackages(TArrayView<const FName> InPackageNames);
}
