// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UObject/SoftObjectPath.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectRedirector.h"
#include "Misc/PackageName.h"
#include "UObject/LinkerLoad.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/CoreRedirects.h"
#include "Misc/RedirectCollector.h"

FSoftObjectPath::FSoftObjectPath(const UObject* InObject)
{
	if (InObject)
	{
		SetPath(InObject->GetPathName());
	}
}

FString FSoftObjectPath::ToString() const
{
	// Most of the time there is no sub path so we can do a single string allocation
	FString AssetPathString = GetAssetPathString();
	if (SubPathString.IsEmpty())
	{
		return AssetPathString;
	}
	FString FullPathString;

	// Preallocate to correct size and then append strings
	FullPathString.Empty(AssetPathString.Len() + SubPathString.Len() + 1);
	FullPathString.Append(AssetPathString);
	FullPathString.AppendChar(':');
	FullPathString.Append(SubPathString);
	return FullPathString;
}

void FSoftObjectPath::SetPath(FString Path)
{
	if (Path.IsEmpty() || Path.Equals(TEXT("None"), ESearchCase::CaseSensitive))
	{
		// Empty path, just empty the pathname.
		Reset();
	}
	else if (ensureMsgf(!FPackageName::IsShortPackageName(Path), TEXT("Cannot create SoftObjectPath with short package name '%s'! You must pass in fully qualified package names"), *Path))
	{
		if (Path[0] != '/')
		{
			// Possibly an ExportText path. Trim the ClassName.
			Path = FPackageName::ExportTextPathToObjectPath(Path);
		}

		int32 ColonIndex = INDEX_NONE;

		if (Path.FindChar(':', ColonIndex))
		{
			// Has a subobject, split on that then create a name from the temporary path
			SubPathString = Path.Mid(ColonIndex + 1);
			Path.RemoveAt(ColonIndex, Path.Len() - ColonIndex);
			AssetPathName = *Path;
		}
		else
		{
			// No Subobject
			AssetPathName = *Path;
			SubPathString.Empty();
		}
	}
}

#if WITH_EDITOR
	extern bool* GReportSoftObjectPathRedirects;
#endif

bool FSoftObjectPath::PreSavePath(bool* bReportSoftObjectPathRedirects)
{
#if WITH_EDITOR
	if (IsNull())
	{
		return false;
	}

	FName FoundRedirection = GRedirectCollector.GetAssetPathRedirection(AssetPathName);

	if (FoundRedirection != NAME_None)
	{
		if (AssetPathName != FoundRedirection && bReportSoftObjectPathRedirects)
		{
			*bReportSoftObjectPathRedirects = true;
		}
		AssetPathName = FoundRedirection;
		return true;
	}

	if (FixupCoreRedirects())
	{
		return true;
	}
#endif // WITH_EDITOR
	return false;
}

void FSoftObjectPath::PostLoadPath(FArchive* InArchive) const
{
#if WITH_EDITOR
	GRedirectCollector.OnSoftObjectPathLoaded(*this, InArchive);
#endif // WITH_EDITOR
}

bool FSoftObjectPath::Serialize(FArchive& Ar)
{
	// Archivers will call back into SerializePath for the various fixups
	Ar << *this;

	return true;
}

bool FSoftObjectPath::Serialize(FStructuredArchive::FSlot Slot)
{
	// Archivers will call back into SerializePath for the various fixups
	Slot << *this;

	return true;
}

void FSoftObjectPath::SerializePath(FArchive& Ar)
{
	bool bSerializeInternals = true;
#if WITH_EDITOR
	if (Ar.IsSaving())
	{
		PreSavePath(false ? GReportSoftObjectPathRedirects : nullptr);
	}

	// Only read serialization options in editor as it is a bit slow
	FName PackageName, PropertyName;
	ESoftObjectPathCollectType CollectType = ESoftObjectPathCollectType::AlwaysCollect;
	ESoftObjectPathSerializeType SerializeType = ESoftObjectPathSerializeType::AlwaysSerialize;

	FSoftObjectPathThreadContext& ThreadContext = FSoftObjectPathThreadContext::Get();
	ThreadContext.GetSerializationOptions(PackageName, PropertyName, CollectType, SerializeType, &Ar);

	if (SerializeType == ESoftObjectPathSerializeType::NeverSerialize)
	{
		bSerializeInternals = false;
	}
	else if (SerializeType == ESoftObjectPathSerializeType::SkipSerializeIfArchiveHasSize)
	{
		bSerializeInternals = Ar.IsObjectReferenceCollector() || Ar.Tell() < 0;
	}
#endif // WITH_EDITOR

	if (bSerializeInternals)
	{
		if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_ADDED_SOFT_OBJECT_PATH)
		{
			FString Path;
			Ar << Path;

			if (Ar.UE4Ver() < VER_UE4_KEEP_ONLY_PACKAGE_NAMES_IN_STRING_ASSET_REFERENCES_MAP)
			{
				Path = FPackageName::GetNormalizedObjectPath(Path);
			}

			SetPath(MoveTemp(Path));
		}
		else
		{
			Ar << AssetPathName;
			Ar << SubPathString;
		}
	}

#if WITH_EDITOR
	if (Ar.IsLoading())
	{
		if (Ar.IsPersistent())
		{
			PostLoadPath(&Ar);

			// If we think it's going to work, we try to do the pre-save fixup now. This is important because it helps with blueprint CDO save determinism with redirectors
			// It's important that the entire CDO hierarchy gets fixed up before an instance in a map gets saved otherwise the delta serialization will save too much
			// If the asset registry hasn't fully loaded this won't necessarily work, but it won't do any harm
			// This will never work in -game builds or on initial load so don't try
			if (GIsEditor && !GIsInitialLoad)
			{
				PreSavePath(nullptr);
			}
		}
		if (Ar.GetPortFlags()&PPF_DuplicateForPIE)
		{
			// Remap unique ID if necessary
			// only for fixing up cross-level references, inter-level references handled in FDuplicateDataReader
			FixupForPIE();
		}
	}
#endif // WITH_EDITOR
}

bool FSoftObjectPath::operator==(FSoftObjectPath const& Other) const
{
	return AssetPathName == Other.AssetPathName && SubPathString == Other.SubPathString;
}

FSoftObjectPath& FSoftObjectPath::operator=(FSoftObjectPath Other)
{
	AssetPathName = Other.AssetPathName;
	SubPathString = MoveTemp(Other.SubPathString);
	return *this;
}

bool FSoftObjectPath::ExportTextItem(FString& ValueStr, FSoftObjectPath const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (0 != (PortFlags & EPropertyPortFlags::PPF_ExportCpp))
	{
		return false;
	}

	if (!IsNull())
	{
		// Fixup any redirectors
		FSoftObjectPath Temp = *this;
		Temp.PreSavePath();

		ValueStr += Temp.ToString();
	}
	else
	{
		ValueStr += TEXT("None");
	}
	return true;
}

bool FSoftObjectPath::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText, FArchive* InSerializingArchive)
{
	FString ImportedPath;
	const TCHAR* NewBuffer = UPropertyHelpers::ReadToken(Buffer, ImportedPath, 1);
	if (!NewBuffer)
	{
		return false;
	}
	Buffer = NewBuffer;
	if (ImportedPath == TEXT("None"))
	{
		ImportedPath = TEXT("");
	}
	else
	{
		if (*Buffer == TCHAR('\''))
		{
			// A ' token likely means we're looking at a path string in the form "Texture2d'/Game/UI/HUD/Actions/Barrel'" and we need to read and append the path part
			// We have to skip over the first ' as UPropertyHelpers::ReadToken doesn't read single-quoted strings correctly, but does read a path correctly
			Buffer++; // Skip the leading '
			ImportedPath.Reset();
			NewBuffer = UPropertyHelpers::ReadToken(Buffer, ImportedPath, 1);
			if (!NewBuffer)
			{
				return false;
			}
			Buffer = NewBuffer;
			if (*Buffer++ != TCHAR('\''))
			{
				return false;
			}
		}
	}

	SetPath(MoveTemp(ImportedPath));

#if WITH_EDITOR
	if (Parent && IsEditorOnlyObject(Parent))
	{
		// We're probably reading config for an editor only object, we need to mark this reference as editor only
		FSoftObjectPathSerializationScope SerializationScope(NAME_None, NAME_None, ESoftObjectPathCollectType::EditorOnlyCollect, ESoftObjectPathSerializeType::AlwaysSerialize);

		PostLoadPath(InSerializingArchive);
	}
	else
#endif
	{
		// Consider this a load, so Config string references get cooked
		PostLoadPath(InSerializingArchive);
	}

	return true;
}

/**
 * Serializes from mismatched tag.
 *
 * @template_param TypePolicy The policy should provide two things:
 *	- GetTypeName() method that returns registered name for this property type,
 *	- typedef Type, which is a C++ type to serialize if property matched type name.
 * @param Tag Property tag to match type.
 * @param Ar Archive to serialize from.
 */
template <class TypePolicy>
bool SerializeFromMismatchedTagTemplate(FString& Output, const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	if (Tag.Type == TypePolicy::GetTypeName())
	{
		typename TypePolicy::Type* ObjPtr = nullptr;
		Slot << ObjPtr;
		if (ObjPtr)
		{
			Output = ObjPtr->GetPathName();
		}
		else
		{
			Output = FString();
		}
		return true;
	}
	else if (Tag.Type == NAME_StrProperty)
	{
		FString String;
		Slot << String;

		Output = String;
		return true;
	}
	return false;
}

bool FSoftObjectPath::SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FStructuredArchive::FSlot Slot)
{
	struct UObjectTypePolicy
	{
		typedef UObject Type;
		static const FName FORCEINLINE GetTypeName() { return NAME_ObjectProperty; }
	};

	FString Path = ToString();

	bool bReturn = SerializeFromMismatchedTagTemplate<UObjectTypePolicy>(Path, Tag, Slot);

	if (Slot.GetUnderlyingArchive().IsLoading())
	{
		SetPath(MoveTemp(Path));
		PostLoadPath(&Slot.GetUnderlyingArchive());
	}

	return bReturn;
}

UObject* FSoftObjectPath::TryLoad(FUObjectSerializeContext* InLoadContext) const
{
	UObject* LoadedObject = nullptr;

	if (!IsNull())
	{
		LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *ToString(), nullptr, LOAD_None, nullptr, true, InLoadContext);

#if WITH_EDITOR
		// Look at core redirects if we didn't find the object
		if (!LoadedObject)
		{
			FSoftObjectPath FixupObjectPath = *this;
			if (FixupObjectPath.FixupCoreRedirects())
			{
				LoadedObject = LoadObject<UObject>(nullptr, *FixupObjectPath.ToString());
			}
		}
#endif

		while (UObjectRedirector* Redirector = Cast<UObjectRedirector>(LoadedObject))
		{
			LoadedObject = Redirector->DestinationObject;
		}
	}

	return LoadedObject;
}

UObject* FSoftObjectPath::ResolveObject() const
{
	// Don't try to resolve if we're saving a package because StaticFindObject can't be used here
	// and we usually don't want to force references to weak pointers while saving.
	if (IsNull() || GIsSavingPackage)
	{
		return nullptr;
	}

	FString PathString = ToString();
#if WITH_EDITOR
	if (GPlayInEditorID != INDEX_NONE)
	{
		// If we are in PIE and this hasn't already been fixed up, we need to fixup at resolution time. We cannot modify the path as it may be somewhere like a blueprint CDO
		FSoftObjectPath FixupObjectPath = *this;
		if (FixupObjectPath.FixupForPIE())
		{
			PathString = FixupObjectPath.ToString();
		}
	}
#endif

	UObject* FoundObject = FindObject<UObject>(nullptr, *PathString);

#if WITH_EDITOR
	// Look at core redirects if we didn't find the object
	if (!FoundObject)
	{
		FSoftObjectPath FixupObjectPath = *this;
		if (FixupObjectPath.FixupCoreRedirects())
		{
			PathString = FixupObjectPath.ToString();
			FoundObject = FindObject<UObject>(nullptr, *PathString);
		}
	}
#endif

	while (UObjectRedirector* Redirector = Cast<UObjectRedirector>(FoundObject))
	{
		FoundObject = Redirector->DestinationObject;
	}

	return FoundObject;
}

FSoftObjectPath FSoftObjectPath::GetOrCreateIDForObject(const class UObject *Object)
{
	check(Object);
	return FSoftObjectPath(Object);
}

void FSoftObjectPath::AddPIEPackageName(FName NewPIEPackageName)
{
	PIEPackageNames.Add(NewPIEPackageName);
}

void FSoftObjectPath::ClearPIEPackageNames()
{
	PIEPackageNames.Empty();
}

bool FSoftObjectPath::FixupForPIE(int32 PIEInstance)
{
#if WITH_EDITOR
	if (PIEInstance != INDEX_NONE && !IsNull())
	{
		const FString Path = ToString();

		// Determine if this reference has already been fixed up for PIE
		const FString ShortPackageOuterAndName = FPackageName::GetLongPackageAssetName(Path);
		if (!ShortPackageOuterAndName.StartsWith(PLAYWORLD_PACKAGE_PREFIX))
		{
			// Name of the ULevel subobject of UWorld, set in InitializeNewWorld
			const bool bIsChildOfLevel = SubPathString.StartsWith(TEXT("PersistentLevel."));

			FString PIEPath = FString::Printf(TEXT("%s/%s_%d_%s"), *FPackageName::GetLongPackagePath(Path), PLAYWORLD_PACKAGE_PREFIX, PIEInstance, *ShortPackageOuterAndName);
			const FName PIEPackage = (!bIsChildOfLevel ? FName(*FPackageName::ObjectPathToPackageName(PIEPath)) : NAME_None);

			// Duplicate if this an already registered PIE package or this looks like a level subobject reference
			if (bIsChildOfLevel || PIEPackageNames.Contains(PIEPackage))
			{
				// Need to prepend PIE prefix, as we're in PIE and this refers to an object in a PIE package
				SetPath(MoveTemp(PIEPath));

				return true;
			}
		}
	}
#endif
	return false;
}

bool FSoftObjectPath::FixupForPIE()
{
	return FixupForPIE(GPlayInEditorID);
}

bool FSoftObjectPath::FixupCoreRedirects()
{
	FString OldString = ToString();
	FCoreRedirectObjectName OldName = FCoreRedirectObjectName(OldString);

	// Always try the object redirect, this will pick up any package redirects as well
	// For things that look like native objects, try all types as we don't know which it would be
	const bool bIsNative = OldString.StartsWith(TEXT("/Script/"));
	FCoreRedirectObjectName NewName = FCoreRedirects::GetRedirectedName(bIsNative ? ECoreRedirectFlags::Type_AllMask : ECoreRedirectFlags::Type_Object, OldName);

	if (OldName != NewName)
	{
		// Only do the fixup if the old object isn't in memory, this avoids false positives
		UObject* FoundOldObject = FindObjectSafe<UObject>(nullptr, *OldString);

		if (!FoundOldObject)
		{
			SetPath(NewName.ToString());
			return true;
		}
	}

	return false;
}

bool FSoftClassPath::SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FStructuredArchive::FSlot Slot)
{
	struct UClassTypePolicy
	{
		typedef UClass Type;
		// Class property shares the same tag id as Object property
		static const FName FORCEINLINE GetTypeName() { return NAME_ObjectProperty; }
	};

	FString Path = ToString();

	bool bReturn = SerializeFromMismatchedTagTemplate<UClassTypePolicy>(Path, Tag, Slot);

	if (Slot.GetUnderlyingArchive().IsLoading())
	{
		SetPath(MoveTemp(Path));
		PostLoadPath(&Slot.GetUnderlyingArchive());
	}

	return bReturn;
}

UClass* FSoftClassPath::ResolveClass() const
{
	return Cast<UClass>(ResolveObject());
}

FSoftClassPath FSoftClassPath::GetOrCreateIDForClass(const UClass *InClass)
{
	check(InClass);
	return FSoftClassPath(InClass);
}

bool FSoftObjectPathThreadContext::GetSerializationOptions(FName& OutPackageName, FName& OutPropertyName, ESoftObjectPathCollectType& OutCollectType, ESoftObjectPathSerializeType& OutSerializeType, FArchive* Archive) const
{
	FName CurrentPackageName, CurrentPropertyName;
	ESoftObjectPathCollectType CurrentCollectType = ESoftObjectPathCollectType::AlwaysCollect;
	ESoftObjectPathSerializeType CurrentSerializeType = ESoftObjectPathSerializeType::AlwaysSerialize;
	bool bFoundAnything = false;
	if (OptionStack.Num() > 0)
	{
		// Go from the top of the stack down
		for (int32 i = OptionStack.Num() - 1; i >= 0; i--)
		{
			const FSerializationOptions& Options = OptionStack[i];
			// Find first valid package/property names. They may not necessarily match
			if (Options.PackageName != NAME_None && CurrentPackageName == NAME_None)
			{
				CurrentPackageName = Options.PackageName;
			}
			if (Options.PropertyName != NAME_None && CurrentPropertyName == NAME_None)
			{
				CurrentPropertyName = Options.PropertyName;
			}

			// Restrict based on lowest/most restrictive collect type
			if (Options.CollectType < CurrentCollectType)
			{
				CurrentCollectType = Options.CollectType;
			}
			if (Options.SerializeType < CurrentSerializeType)
			{
				CurrentSerializeType = Options.SerializeType;
			}
		}

		bFoundAnything = true;
	}
	
	// Check UObject serialize context as a backup
	FUObjectSerializeContext* LoadContext = Archive ? Archive->GetSerializeContext() : nullptr;
	if (LoadContext && LoadContext->SerializedObject)
	{
		FLinkerLoad* Linker = LoadContext->SerializedObject->GetLinker();
		if (Linker)
		{
			if (CurrentPackageName == NAME_None)
			{
				CurrentPackageName = FName(*FPackageName::FilenameToLongPackageName(Linker->Filename));
			}
			if (Archive == nullptr)
			{
				// Use archive from linker if it wasn't passed in
				Archive = Linker;
			}
			bFoundAnything = true;
		}
	}

	// Check archive for property/editor only info, this works for any serialize if passed in
	if (Archive)
	{
		UProperty* CurrentProperty = Archive->GetSerializedProperty();
			
		if (CurrentProperty && CurrentPropertyName == NAME_None)
		{
			CurrentPropertyName = CurrentProperty->GetFName();
		}
		bool bEditorOnly = false;
#if WITH_EDITOR
		bEditorOnly = Archive->IsEditorOnlyPropertyOnTheStack();

		static FName UntrackedName = TEXT("Untracked");
		if (CurrentProperty && CurrentProperty->HasMetaData(UntrackedName))
		{
			// Property has the Untracked metadata, so set to never collect references
			CurrentCollectType = ESoftObjectPathCollectType::NeverCollect;
		}
#endif
		// If we were always collect before and not overridden by stack options, set to editor only
		if (bEditorOnly && CurrentCollectType == ESoftObjectPathCollectType::AlwaysCollect)
		{
			CurrentCollectType = ESoftObjectPathCollectType::EditorOnlyCollect;
		}

		bFoundAnything = true;
	}

	if (bFoundAnything)
	{
		OutPackageName = CurrentPackageName;
		OutPropertyName = CurrentPropertyName;
		OutCollectType = CurrentCollectType;
		OutSerializeType = CurrentSerializeType;
		return true;
	}

	return bFoundAnything;
}

FThreadSafeCounter FSoftObjectPath::CurrentTag(1);
TSet<FName> FSoftObjectPath::PIEPackageNames;
