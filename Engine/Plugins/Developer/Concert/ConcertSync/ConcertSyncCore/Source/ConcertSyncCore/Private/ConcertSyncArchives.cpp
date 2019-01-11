// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertSyncArchives.h"
#include "ConcertSyncSettings.h"

#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/LinkerLoad.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/PropertyPortFlags.h"
#include "Internationalization/TextPackageNamespaceUtil.h"

static const FName SkipAssetsMarker = TEXT("SKIPASSETS");

namespace ConcertSyncUtil
{
	bool ShouldSkipTransientProperty(const UProperty* Property)
	{
		if (Property->HasAnyPropertyFlags(CPF_Transient))
		{
			const UConcertSyncConfig* SyncConfig = GetDefault<UConcertSyncConfig>();
			for (const FSoftObjectPath& TransactionProperty : SyncConfig->AllowedTransientProperties)
			{
				UProperty* FilterProperty = Cast<UProperty>(TransactionProperty.TryLoad());
				if (Property == FilterProperty)
				{
					// Allowed transient property, do not skip
					return false;
				}
			}
			// Not allowed transient property, skip
			return true;
		}
		// Non transient property, might not skip
		return false;
	}
}

FString FConcertSyncWorldRemapper::RemapObjectPathName(const FString& InObjectPathName) const
{
	return HasMapping() ? InObjectPathName.Replace(*SourceWorldPathName, *DestWorldPathName) : InObjectPathName;
}

bool FConcertSyncWorldRemapper::ObjectBelongsToWorld(const FString& InObjectPathName) const
{
	return HasMapping() && (InObjectPathName.StartsWith(SourceWorldPathName) || InObjectPathName.StartsWith(DestWorldPathName));
}

bool FConcertSyncWorldRemapper::HasMapping() const
{
	return SourceWorldPathName.Len() > 0 && DestWorldPathName.Len() > 0;
}

FConcertSyncObjectWriter::FConcertSyncObjectWriter(FConcertLocalIdentifierTable* InLocalIdentifierTable, UObject* InObj, TArray<uint8>& OutBytes, const bool InIncludeEditorOnlyData, const bool InSkipAssets)
	: FConcertIdentifierWriter(InLocalIdentifierTable, OutBytes, /*bIsPersistent*/false)
	, bSkipAssets(InSkipAssets)
	, ShouldSkipPropertyFunc()
{
	ArIgnoreClassRef = false;
	ArIgnoreArchetypeRef = false;
	ArNoDelta = true;
	//SetWantBinaryPropertySerialization(true);

	SetIsTransacting(true);
	SetFilterEditorOnly(!InIncludeEditorOnlyData);

#if USE_STABLE_LOCALIZATION_KEYS
	if (GIsEditor && !(ArPortFlags & PPF_DuplicateForPIE))
	{
		SetLocalizationNamespace(TextNamespaceUtil::EnsurePackageNamespace(InObj));
	}
#endif // USE_STABLE_LOCALIZATION_KEYS
}

void FConcertSyncObjectWriter::SerializeObject(UObject* InObject, const TArray<FName>* InPropertyNamesToWrite)
{
	if (InPropertyNamesToWrite)
	{
		ShouldSkipPropertyFunc = [InObject, InPropertyNamesToWrite](const UProperty* InProperty) -> bool
		{
			return InProperty->GetOwnerStruct() == InObject->GetClass() && !InPropertyNamesToWrite->Contains(InProperty->GetFName());
		};

		InObject->Serialize(*this);

		ShouldSkipPropertyFunc = FShouldSkipPropertyFunc();
	}
	else
	{
		InObject->Serialize(*this);
	}
}

void FConcertSyncObjectWriter::SerializeProperty(UProperty* InProp, UObject* InObject)
{
	for (int32 Idx = 0; Idx < InProp->ArrayDim; ++Idx)
	{
		InProp->SerializeItem(FStructuredArchiveFromArchive(*this).GetSlot(), InProp->ContainerPtrToValuePtr<void>(InObject, Idx));
	}
}

FArchive& FConcertSyncObjectWriter::operator<<(UObject*& Obj)
{
	FName ObjPath;
	if (Obj)
	{
		ObjPath = (Obj->IsAsset() && bSkipAssets) ? SkipAssetsMarker : *Obj->GetPathName();
	}

	*this << ObjPath;
	return *this;
}

FArchive& FConcertSyncObjectWriter::operator<<(FLazyObjectPtr& LazyObjectPtr)
{
	UObject* Obj = LazyObjectPtr.Get();
	*this << Obj;
	return *this;
}

FArchive& FConcertSyncObjectWriter::operator<<(FSoftObjectPtr& AssetPtr)
{
	FSoftObjectPath Obj = AssetPtr.ToSoftObjectPath();
	*this << Obj;
	return *this;
}

FArchive& FConcertSyncObjectWriter::operator<<(FSoftObjectPath& AssetPtr)
{
	FName ObjPath = bSkipAssets ? SkipAssetsMarker : *AssetPtr.ToString();
	*this << ObjPath;
	return *this;
}

FArchive& FConcertSyncObjectWriter::operator<<(FWeakObjectPtr& Value)
{
	UObject* Obj = Value.Get();
	*this << Obj;
	return *this;
}

FString FConcertSyncObjectWriter::GetArchiveName() const
{
	return TEXT("FConcertSyncObjectWriter");
}

bool FConcertSyncObjectWriter::ShouldSkipProperty(const UProperty* InProperty) const
{
	return (ShouldSkipPropertyFunc && ShouldSkipPropertyFunc(InProperty)) || 
		ConcertSyncUtil::ShouldSkipTransientProperty(InProperty);
}

FConcertSyncObjectReader::FConcertSyncObjectReader(const FConcertLocalIdentifierTable* InLocalIdentifierTable, FConcertSyncWorldRemapper InWorldRemapper, UObject* InObj, const TArray<uint8>& InBytes)
	: FConcertIdentifierReader(InLocalIdentifierTable, InBytes, /*bIsPersistent*/false)
	, WorldRemapper(MoveTemp(InWorldRemapper))
{
	ArIgnoreClassRef = false;
	ArIgnoreArchetypeRef = false;
	ArNoDelta = true;
	//SetWantBinaryPropertySerialization(true);

	SetIsTransacting(true);
	SetFilterEditorOnly(!WITH_EDITORONLY_DATA);

#if USE_STABLE_LOCALIZATION_KEYS
	if (GIsEditor && !(ArPortFlags & PPF_DuplicateForPIE))
	{
		SetLocalizationNamespace(TextNamespaceUtil::EnsurePackageNamespace(InObj));
	}
#endif // USE_STABLE_LOCALIZATION_KEYS
}

void FConcertSyncObjectReader::SerializeObject(UObject* InObject)
{
	InObject->Serialize(*this);
}

void FConcertSyncObjectReader::SerializeProperty(UProperty* InProp, UObject* InObject)
{
	for (int32 Idx = 0; Idx < InProp->ArrayDim; ++Idx)
	{
		InProp->SerializeItem(FStructuredArchiveFromArchive(*this).GetSlot(), InProp->ContainerPtrToValuePtr<void>(InObject, Idx));
	}
}

FArchive& FConcertSyncObjectReader::operator<<(UObject*& Obj)
{
	FName ObjPath;
	*this << ObjPath;

	if (ObjPath.IsNone())
	{
		Obj = nullptr;
	}
	else if (ObjPath != SkipAssetsMarker)
	{
		const FString ResolvedObjPath = WorldRemapper.RemapObjectPathName(ObjPath.ToString());

		// Always attempt to find an in-memory object first as we may be calling this function while a load is taking place
		Obj = StaticFindObject(UObject::StaticClass(), nullptr, *ResolvedObjPath);

		// We do not attempt to load objects within the current world as they may not have been created yet, 
		// and we don't want to trigger a reload of the world package (when iterative cooking is enabled)
		const bool bAllowLoad = !WorldRemapper.ObjectBelongsToWorld(ResolvedObjPath);
		if (!Obj && bAllowLoad)
		{
			// If the outer name is a package path that isn't currently loaded, then we need to try loading it to avoid 
			// creating an in-memory version of the package (which would prevent the real package ever loading)
			if (FPackageName::IsValidLongPackageName(ResolvedObjPath))
			{
				Obj = LoadPackage(nullptr, *ResolvedObjPath, LOAD_NoWarn);
			}
			else
			{
				Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *ResolvedObjPath);
			}
		}
	}

	return *this;
}

FArchive& FConcertSyncObjectReader::operator<<(FLazyObjectPtr& LazyObjectPtr)
{
	UObject* Obj = nullptr;
	*this << Obj;
	LazyObjectPtr = Obj;
	return *this;
}

FArchive& FConcertSyncObjectReader::operator<<(FSoftObjectPtr& AssetPtr)
{
	FSoftObjectPath Obj;
	*this << Obj;
	AssetPtr = Obj;
	return *this;
}

FArchive& FConcertSyncObjectReader::operator<<(FSoftObjectPath& AssetPtr)
{
	FName ObjPath;
	*this << ObjPath;

	if (ObjPath != SkipAssetsMarker)
	{
		const FString ResolvedObjPath = WorldRemapper.RemapObjectPathName(ObjPath.ToString());
		AssetPtr.SetPath(ResolvedObjPath);
	}

	return *this;
}

FArchive& FConcertSyncObjectReader::operator<<(FWeakObjectPtr& Value)
{
	UObject* Obj = nullptr;
	*this << Obj;
	Value = Obj;
	return *this;
}

FString FConcertSyncObjectReader::GetArchiveName() const
{
	return TEXT("FConcertSyncObjectReader");
}
