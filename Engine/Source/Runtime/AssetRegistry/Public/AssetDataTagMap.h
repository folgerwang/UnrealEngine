// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/SortedMap.h"

#if !defined(USE_COMPACT_ASSET_REGISTRY)
#define USE_COMPACT_ASSET_REGISTRY (0)
#endif

#if !USE_COMPACT_ASSET_REGISTRY

/** Type of tag map */
typedef TSortedMap<FName, FString, FDefaultAllocator, FNameSortIndexes> FAssetDataTagMap;

#else

/** Singleton class to manage the storage for the compact tag maps. */
class FAssetDataTagMapValueStorage
{
public:
	/** Wrapper for an index into one of the three compact storage sparse arrays. */
	struct FStorageID
	{
	private:
		friend class FAssetDataTagMapValueStorage;
		enum 
		{
			MaxNoNumberFNameIndex = (1 << 27) - 1 // must match, the bitfield below
		};
		struct FFields
		{
			uint32 Index : 27; // must match, the enum, above
			uint32 IsString : 1;
			uint32 IsFName : 1;
			uint32 IsFNameExportText : 1;
			uint32 IsLocText : 1;
			uint32 NoNumbers : 1;
		};

		union
		{
			FFields Fields;
			uint32 AllFields;
		};
	public:
		FStorageID()
			: AllFields(0)
		{
		}
		bool IsNull() const
		{
			return AllFields == 0;
		}

		/** Conversion operator to convert an id into a string. */
		operator FString() const;

		bool operator==(const FStorageID& Other) const
		{
			return AllFields == Other.AllFields;
		}

		int32 GetAllocatedSize() const
		{
			if (Fields.IsString)
			{
				return FString(*this).GetAllocatedSize();
			}
			return 0;
		}
	};
private:
	/** Helper class for condensing strings of these types into  1 - 3 FNames
		[class]'[package]'.[object]
		[package].[object]
		[package]
	*/
	struct FCompactExportText
	{
		FName Class;
		FName Package;
		FName Object;

		FString ToString() const
		{
			FString Result;
			if (Class != NAME_None)
			{
				Class.AppendString(Result);
				Result += TCHAR('\'');
			}
			Package.AppendString(Result);
			if (Object != NAME_None)
			{
				Result += TCHAR('.');
				Object.AppendString(Result);
			}
			if (Class != NAME_None)
			{
				Result += TCHAR('\'');
			}
			return Result;
		}
	};
	struct FCompactExportTextNoNumbers
	{
		NAME_INDEX Class;
		NAME_INDEX Package;
		NAME_INDEX Object;

		FString ToString() const
		{
			FString Result;
			if (Class != 0)
			{
				FName(Class, Class, 0).AppendString(Result);
				Result += TCHAR('\'');
			}
			FName(Package, Package, 0).AppendString(Result);
			if (Object != 0)
			{
				Result += TCHAR('.');
				FName(Object, Object, 0).AppendString(Result);
			}
			if (Class != 0)
			{
				Result += TCHAR('\'');
			}
			return Result;
		}
	};
	/** Storage for strings, plus the two compact formats. */
	TSparseArray<FString> Strings;
	TSparseArray<FName> FNames;
	TSparseArray<FCompactExportText> ExportTexts;
	TSparseArray<FCompactExportTextNoNumbers> ExportTextsNoNumbers;
	TSparseArray<FText> FTexts;

	/** Allocate a new string */
	FStorageID StoreAsString(const FString& Value)
	{
		FStorageID Result;
		Result.Fields.IsString = true;
		Result.Fields.Index = Strings.Add(Value);
		checkf(IdToString(Result).Compare(Value) == 0, TEXT("Failed to correctly store a value compactly %s != %s"), *IdToString(Result), *Value);
		return Result;
	}
	/** Allocate a new FName */
	FStorageID StoreAsFName(const FString& Value)
	{
		FStorageID Result;
		Result.Fields.IsFName = true;
		FName ValueName(*Value);
		if (ValueName.GetNumber() || ValueName.GetComparisonIndex() != ValueName.GetDisplayIndex() || ValueName.GetComparisonIndex() > FStorageID::MaxNoNumberFNameIndex)
		{
			Result.Fields.Index = FNames.Add(ValueName);
		}
		else
		{
			check(ValueName.GetComparisonIndex() >= 0);
			Result.Fields.Index = ValueName.GetComparisonIndex();
			Result.Fields.NoNumbers = true;
		}
		// there are cases where the results do not match on case
		checkf(IdToString(Result).Compare(Value, ESearchCase::IgnoreCase) == 0, TEXT("Failed to correctly store a value compactly %s != %s"), *IdToString(Result), *Value);
		return Result;
	}
	/** Allocate a new export text style FName triple */
	FStorageID StoreAsExportText(const FString& Value)
	{
		FString ClassName;
		FString ObjectPath;
		FString PackageName;
		FString ObjectName;
		FCompactExportText CompactExportText;
		if (Value.Contains(TEXT("'")))
		{
			verify(FPackageName::ParseExportTextPath(Value, &ClassName, &ObjectPath));
			CompactExportText.Class = *ClassName;
		}
		else
		{
			ObjectPath = Value;
		}

		PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
		if (PackageName != ObjectPath)
		{
			ObjectName = ObjectPath.Mid(PackageName.Len() + 1);
			CompactExportText.Object = *ObjectName;
		}
		CompactExportText.Package = *PackageName;

		FStorageID Result;
		Result.Fields.IsFNameExportText = true;
		if (
			CompactExportText.Class.GetNumber() || CompactExportText.Class.GetComparisonIndex() != CompactExportText.Class.GetDisplayIndex() ||
			CompactExportText.Package.GetNumber() || CompactExportText.Package.GetComparisonIndex() != CompactExportText.Package.GetDisplayIndex() ||
			CompactExportText.Object.GetNumber() || CompactExportText.Object.GetComparisonIndex() != CompactExportText.Object.GetDisplayIndex()
			)
		{
			Result.Fields.Index = ExportTexts.Add(CompactExportText);
		}
		else
		{
			FCompactExportTextNoNumbers CompactExportTextNoNumbers;
			CompactExportTextNoNumbers.Class = CompactExportText.Class.GetComparisonIndex();
			CompactExportTextNoNumbers.Object = CompactExportText.Object.GetComparisonIndex();
			CompactExportTextNoNumbers.Package = CompactExportText.Package.GetComparisonIndex();
			Result.Fields.Index = ExportTextsNoNumbers.Add(CompactExportTextNoNumbers);
			Result.Fields.NoNumbers = true;
		}
		// there are cases where the results do not match on case
		checkf(IdToString(Result).Compare(Value, ESearchCase::IgnoreCase) == 0, TEXT("Failed to correctly store a value compactly %s != %s"), *IdToString(Result), *Value);
		return Result;
	}
	/** Allocate a FText */
	FStorageID StoreAsLocText(const FString& Value)
	{
		FStorageID Result;
		Result.Fields.IsLocText = true;
		FText TextValue;
		if (!FTextStringHelper::ReadFromBuffer(*Value, TextValue))
		{
			TextValue = FText::FromString(*Value);
		}
		Result.Fields.Index = FTexts.Add(TextValue);
		return Result;
	}

public:
	// if any of these cause a link error, then you can't use USE_COMPACT_ASSET_REGISTRY with this build config

	/** Singleton */
	static FAssetDataTagMapValueStorage& Get();

	/** Determine if this key value should be stored as an FName */
	static bool KeyShouldHaveFNameValue(FName Key, const FString& Value);
	/** Determine if this key value should be stored as an FName triple */
	static bool KeyShouldHaveCompactExportTextValue(FName Key, const FString& Value);
	/** Determine if this key value should be stored as a FText */
	static bool KeyShouldHaveLocTextExportTextValue(FName Key, const FString& Value);

	uint32 GetAllocatedSize() const
	{
		return Strings.GetAllocatedSize() + FNames.GetAllocatedSize() + ExportTexts.GetAllocatedSize() + FTexts.GetAllocatedSize();
	}
	void Shrink()
	{
		Strings.Shrink();
		FNames.Shrink();
		ExportTexts.Shrink();
		ExportTextsNoNumbers.Shrink();
		FTexts.Shrink();
	}
	/** Return the total size of all values stored as strings */
	uint32 GetStringSize() const
	{
		uint32 Result = 0;
		for (const FString& Item : Strings)
		{
			Result += Item.GetAllocatedSize();
		}
		return Result;
	}
	/** Return the total size of all values stored as strings, after deduplication (to simplify things, I ignore case here) */
	uint32 GetUniqueStringSize() const
	{
		uint32 Result = 0;
		TSet<FString> Seen;
		for (const FString& Item : Strings)
		{
			if (!Seen.Contains(Item))
			{
				Result += Item.GetAllocatedSize();
				Seen.Add(Item);
			}
		}
		return Result;
	}

	bool IsValidIndex(FStorageID Id) const
	{
		if (Id.Fields.IsString + Id.Fields.IsFName + Id.Fields.IsFNameExportText + Id.Fields.IsLocText != 1)
		{
			return false;
		}
		if (Id.Fields.IsString)
		{
			return Strings.IsAllocated(Id.Fields.Index);
		}
		if (Id.Fields.IsFName)
		{
			if (!Id.Fields.NoNumbers)
			{
				return FNames.IsAllocated(Id.Fields.Index);
			}
			return FName(Id.Fields.Index, Id.Fields.Index, 0).IsValid();
		}
		if (Id.Fields.IsFNameExportText)
		{
			if (!Id.Fields.NoNumbers)
			{
				return ExportTexts.IsAllocated(Id.Fields.Index);
			}
			return ExportTextsNoNumbers.IsAllocated(Id.Fields.Index);
		}
		return FTexts.IsAllocated(Id.Fields.Index);
	}

	/** Return the string associated with an Id, regardless of how it was stored. */
	FString IdToString(FStorageID Id) const
	{
		check(IsValidIndex(Id));
		if (Id.Fields.IsString)
		{
			return Strings[Id.Fields.Index];
		}
		if (Id.Fields.IsFName)
		{
			if (!Id.Fields.NoNumbers)
			{
				return FNames[Id.Fields.Index].ToString();
			}
			if (Id.Fields.Index == NAME_TRUE)
			{
				return TEXT("True");
			}
			if (Id.Fields.Index == NAME_FALSE)
			{
				return TEXT("False");
			}
			return FName(Id.Fields.Index, Id.Fields.Index, 0).ToString();
		}
		if (Id.Fields.IsFNameExportText)
		{
			if (!Id.Fields.NoNumbers)
			{
				return ExportTexts[Id.Fields.Index].ToString();
			}
			return ExportTextsNoNumbers[Id.Fields.Index].ToString();
		}
		FString LocResult;
		FTextStringHelper::WriteToBuffer(LocResult, FTexts[Id.Fields.Index]);
		return LocResult;
	}

	/** Removed an Id and any associated storage. */
	void RemoveId(FStorageID Id)
	{
		check(IsValidIndex(Id));
		if (Id.Fields.IsString)
		{
			return Strings.RemoveAt(Id.Fields.Index);
		}
		if (Id.Fields.IsFName)
		{
			if (!Id.Fields.NoNumbers)
			{
				return FNames.RemoveAt(Id.Fields.Index);
			}
			// No number FNames are not deleted
			return;
		}
		if (Id.Fields.IsFNameExportText)
		{
			if (!Id.Fields.NoNumbers)
			{
				return ExportTexts.RemoveAt(Id.Fields.Index);
			}
			return ExportTextsNoNumbers.RemoveAt(Id.Fields.Index);
		}
		return FTexts.RemoveAt(Id.Fields.Index);
	}

	/** Store a new value, possibly as a FName or FNames and return the Id */
	FStorageID Store(FName Key, const FString& Value)
	{
		if (KeyShouldHaveFNameValue(Key, Value))
		{
			return StoreAsFName(Value);
		}
		if (KeyShouldHaveCompactExportTextValue(Key, Value))
		{
			return StoreAsExportText(Value);
		}
		if (KeyShouldHaveLocTextExportTextValue(Key, Value))
		{
			return StoreAsLocText(Value);
		}
		return StoreAsString(Value);
	}

};

inline FAssetDataTagMapValueStorage::FStorageID::operator FString() const
{
	return FAssetDataTagMapValueStorage::Get().IdToString(*this);
}

typedef TSortedMap<FName, FAssetDataTagMapValueStorage::FStorageID, FDefaultAllocator, FNameSortIndexes> FAssetDataTagMapBase;

/** Wrapper of the underlying map that handles making sure that when the map dies, the underlying storage for the strings is freed. */
class FAssetDataTagMap : public FAssetDataTagMapBase
{
	/** Free the storage forthe key without affecting the mapping. The mapping is typically overwritten or deleted after this. */
	void RemoveIdForKey(FName InKey)
	{
		FAssetDataTagMapValueStorage::FStorageID Id = FindRef(InKey);
		if (!Id.IsNull())
		{
			FAssetDataTagMapValueStorage::Get().RemoveId(Id);
		}
	}
	/** Free the storage for all keys without affecting the mapping. The mapping is typically overwritten or deleted after this. */
	void RemoveAll()
	{
		for (auto& Pair : *this)
		{
			FAssetDataTagMapValueStorage::Get().RemoveId(Pair.Value);
		}
	}
public:

	FAssetDataTagMap() = default;
	FAssetDataTagMap(FAssetDataTagMap&&) = default;

	FAssetDataTagMap(const FAssetDataTagMap& InMap)
	{
		for (auto& Pair : InMap)
		{
			Add(Pair.Key, FString(Pair.Value));
		}
	}


	FAssetDataTagMap& operator=(const FAssetDataTagMap& InMap)
	{
		Empty(InMap.Num());
		for (auto& Pair : InMap)
		{
			Add(Pair.Key, FString(Pair.Value));
		}
		return *this;
	}

	~FAssetDataTagMap()
	{
		RemoveAll();
	}
	void Empty(int32 Slack)
	{
		RemoveAll();
		FAssetDataTagMapBase::Empty(Slack);
	}
	FORCEINLINE void Add(FName InKey, const FString&  InValue) 
	{ 
		RemoveIdForKey(InKey);
		Emplace(InKey, FAssetDataTagMapValueStorage::Get().Store(InKey, InValue)); 
	}
	FORCEINLINE void Add(FName InKey, FString&& InValue) 
	{ 
		RemoveIdForKey(InKey);
		Emplace(InKey, FAssetDataTagMapValueStorage::Get().Store(InKey, InValue));
	}
	FORCEINLINE void Remove(FName InKey)
	{
		RemoveIdForKey(InKey);
		FAssetDataTagMapBase::Remove(InKey);
	}

	/** This is serialization compatible with the non-compact version...so we just load the non-compact version and iterate it to compress the data structure. */
	friend FArchive& operator<<(FArchive& Ar, FAssetDataTagMap& This)
	{
		TSortedMap<FName, FString, FDefaultAllocator, FNameSortIndexes> EmulatedContainer;
		if (Ar.IsLoading())
		{
			Ar << EmulatedContainer;
			This.Empty(EmulatedContainer.Num());
			for (auto& Pair : EmulatedContainer)
			{
				This.Add(Pair.Key, Pair.Value);
			}
		}
		else
		{
			for (auto& Pair : This)
			{
				EmulatedContainer.Add(Pair.Key, FString(Pair.Value));
			}
			Ar << EmulatedContainer;
		}
		return Ar;
	}
};

#endif

/** Wrapper of shared pointer to a map */
class FAssetDataTagMapSharedView
{
	
public:
	/** Default constructor - empty map */
	FAssetDataTagMapSharedView()
	{
	}

	/** Constructor from an existing map pointer */
	FAssetDataTagMapSharedView(TSharedPtr<FAssetDataTagMap> InMap)
		: Map(InMap)
	{
	}

	/** Constructor from an existing map pointer */
	FAssetDataTagMapSharedView(FAssetDataTagMap&& InMap)
	{
		if (InMap.Num())
		{
			Map = MakeShareable(new FAssetDataTagMap(MoveTemp(InMap)));
		}
	}

#if !USE_COMPACT_ASSET_REGISTRY
	struct FFindTagResult
	{
		typedef const FString* FContainedType;

		FFindTagResult(FContainedType InValue)
			: Value(InValue)
		{
		}
		bool IsSet() const
		{
			return !!Value;
		}
		const FString& GetValue() const
		{
			check(Value);
			return *Value;
		}
	private:
		FContainedType Value;
	};
	/** Find a value by key (nullptr if not found) */
	UE_DEPRECATED(4.22, "FAssetDataTagMapSharedView::Find is not compatible with USE_COMPACT_ASSET_REGISTRY, use FindTag instead.")
	const FString* Find(FAssetDataTagMap::KeyConstPointerType Key) const
	{
		return GetMap().Find(Key);
	}
#else
	struct FFindTagResult
	{
		typedef FAssetDataTagMapValueStorage::FStorageID* FContainedType;

		FFindTagResult(FContainedType InValue)
			: Value(InValue)
		{
		}
		bool IsSet() const
		{
			return Value && !Value->IsNull();
		}
		FString GetValue() const
		{
			check(IsSet());
			return FString(*Value);
		}
	private:
		const FContainedType Value;
	};
#endif
	/** Find a value by key and return an option indicating if it was found, and if so, what the value is. */
	FFindTagResult FindTag(FName Tag) const
	{
		FFindTagResult::FContainedType TagValue = nullptr;
		if (Map.IsValid())
		{
			TagValue = Map->Find(Tag);
		}
		return FFindTagResult(TagValue);
	}

	/** Return true if this map contains a specific key value pair. Value comparisons are NOT cases sensitive.*/
	bool ContainsKeyValue(FName Tag, const FString& Value) const
	{
		FFindTagResult Result = FindTag(Tag);
		return Result.IsSet() && Result.GetValue() == Value;
	}

	/** Find a value by key (abort if not found) */
	const FString FindChecked(FAssetDataTagMap::KeyConstPointerType Key) const
	{
		return GetMap().FindChecked(Key);
	}
	/** Find a value by key (default value if not found) */
	FString FindRef(FAssetDataTagMap::KeyConstPointerType Key) const
	{
		return GetMap().FindRef(Key);
	}

	/** Determine whether a key is present in the map */
	bool Contains(FAssetDataTagMap::KeyConstPointerType Key) const
	{
		return GetMap().Contains(Key);
	}

	/** Retrieve size of map */
	int32 Num() const
	{
		return GetMap().Num();
	}

	/** Populate an array with all the map's keys */
	template <class FAllocator>
	int32 GetKeys(TArray<FName, FAllocator>& OutKeys) const
	{
		return GetMap().GetKeys(OutKeys);
	}

	/** Populate an array with all the map's keys */
	template <class FAllocator>
	void GenerateKeyArray(TArray<FName, FAllocator>& OutKeys) const
	{
		GetMap().GenerateKeyArray(OutKeys);
	}

	/** Populate an array with all the map's values */
	template <class FAllocator>
	void GenerateValueArray(TArray<FName, FAllocator>& OutValues) const
	{
		GetMap().GenerateValueArray(OutValues);
	}

	/** Iterate all key-value pairs */
	FAssetDataTagMap::TConstIterator CreateConstIterator() const
	{
		return GetMap().CreateConstIterator();
	}

	/** Const access to the underlying map, mainly for taking a copy */
	const FAssetDataTagMap& GetMap() const
	{
		static FAssetDataTagMap EmptyMap;

		if (Map.IsValid())
		{
			return *Map;
		}

		return EmptyMap;
	}

	/** Returns amount of extra memory used by this structure, including shared ptr overhead */
	uint32 GetAllocatedSize() const
	{
		uint32 AllocatedSize = 0;
		if (Map.IsValid())
		{
			AllocatedSize += sizeof(FAssetDataTagMap); // Map itself
			AllocatedSize += (sizeof(int32) * 2); // SharedPtr overhead
			AllocatedSize += Map->GetAllocatedSize();
		}

		return AllocatedSize;
	}
	/** Shrinks the contained map */
	void Shrink()
	{
		if (Map.IsValid())
		{
			Map->Shrink();
		}
	}
private:
	//friend struct FAssetData;
	friend class FAssetRegistryState;

	/* Strip a key */
	void StripKey(FName Key)
	{
		if (Map.IsValid())
		{
			Map->Remove(Key);
		}
	}

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FAssetDataTagMapSharedView& SharedView)
	{
		if (Ar.IsSaving())
		{
			if (SharedView.Map.IsValid())
			{
				Ar << *SharedView.Map;
			}
			else
			{
				FAssetDataTagMap TempMap;
				Ar << TempMap;
			}
		}
		else
		{
			// Serialize into temporary map, if it isn't empty move memory into new map
			FAssetDataTagMap TempMap;
			Ar << TempMap;

			if (TempMap.Num())
			{
				SharedView.Map = MakeShareable(new FAssetDataTagMap(MoveTemp(TempMap)));
			}
		}

		return Ar;
	}

public:
	/** Range for iterator access - DO NOT USE DIRECTLY */
	FAssetDataTagMap::RangedForConstIteratorType begin() const
	{
		return GetMap().begin();
	}

	/** Range for iterator access - DO NOT USE DIRECTLY */
	FAssetDataTagMap::RangedForConstIteratorType end() const
	{
		return GetMap().end();
	}

private:

	/** Pointer to map being wrapped, it is created on demand */
	TSharedPtr<FAssetDataTagMap> Map;
};

