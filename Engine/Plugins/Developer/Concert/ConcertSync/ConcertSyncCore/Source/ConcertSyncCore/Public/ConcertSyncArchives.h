// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IdentifierTable/ConcertTransportArchives.h"

namespace ConcertSyncUtil
{
	 bool CONCERTSYNCCORE_API ShouldSkipTransientProperty(const UProperty* Property);
}

/** Util to handle remapping objects within the source world to be the equivalent objects in the destination world */
class CONCERTSYNCCORE_API FConcertSyncWorldRemapper
{
public:
	FConcertSyncWorldRemapper() = default;

	FConcertSyncWorldRemapper(FString InSourceWorldPathName, FString InDestWorldPathName)
		: SourceWorldPathName(MoveTemp(InSourceWorldPathName))
		, DestWorldPathName(MoveTemp(InDestWorldPathName))
	{
	}

	FString RemapObjectPathName(const FString& InObjectPathName) const;

	bool ObjectBelongsToWorld(const FString& InObjectPathName) const;

	bool HasMapping() const;

private:
	FString SourceWorldPathName;
	FString DestWorldPathName;
};

/** Archive for writing objects in a way that they can be sent to another instance via Concert */
class CONCERTSYNCCORE_API FConcertSyncObjectWriter : public FConcertIdentifierWriter
{
public:
	FConcertSyncObjectWriter(FConcertLocalIdentifierTable* InLocalIdentifierTable, UObject* InObj, TArray<uint8>& OutBytes, const bool InIncludeEditorOnlyData, const bool InSkipAssets);

	void SerializeObject(UObject* InObject, const TArray<FName>* InPropertyNamesToWrite = nullptr);
	void SerializeProperty(UProperty* InProp, UObject* InObject);

	using FConcertIdentifierWriter::operator<<; // For visibility of the overloads we don't override

	//~ Begin FArchive Interface
	virtual FArchive& operator<<(UObject*& Obj) override;
	virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override;
	virtual FArchive& operator<<(FSoftObjectPtr& AssetPtr) override;
	virtual FArchive& operator<<(FSoftObjectPath& AssetPtr) override;
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override;
	virtual FString GetArchiveName() const override;
	virtual bool ShouldSkipProperty(const UProperty* InProperty) const override;
	//~ End FArchive Interface

private:
	typedef TFunction<bool(const UProperty*)> FShouldSkipPropertyFunc;
	bool bSkipAssets;
	FShouldSkipPropertyFunc ShouldSkipPropertyFunc;
};

/** Archive for reading objects that can been received from another instance via Concert */
class CONCERTSYNCCORE_API FConcertSyncObjectReader : public FConcertIdentifierReader
{
public:
	FConcertSyncObjectReader(const FConcertLocalIdentifierTable* InLocalIdentifierTable, FConcertSyncWorldRemapper InWorldRemapper, UObject* InObj, const TArray<uint8>& InBytes);

	void SerializeObject(UObject* InObject);
	void SerializeProperty(UProperty* InProp, UObject* InObject);

	using FConcertIdentifierReader::operator<<; // For visibility of the overloads we don't override

	//~ Begin FArchive Interface
	virtual FArchive& operator<<(UObject*& Obj) override;
	virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override;
	virtual FArchive& operator<<(FSoftObjectPtr& AssetPtr) override;
	virtual FArchive& operator<<(FSoftObjectPath& AssetPtr) override;
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override;
	virtual FString GetArchiveName() const override;
	//~ End FArchive Interface

private:
	FConcertSyncWorldRemapper WorldRemapper;
};
