// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/ManagedArrayCollection.h"

#include "GeometryCollection/GeometryCollectionAlgo.h"

DEFINE_LOG_CATEGORY_STATIC(FManagedArrayCollectionLogging, NoLogging, All);

int8 FManagedArrayCollection::Invalid = -1;


FManagedArrayCollection::FManagedArrayCollection()
{
}

void FManagedArrayCollection::Initialize(FManagedArrayCollection & CollectionIn)
{
	this->Map = CollectionIn.Map;
	this->GroupInfo = CollectionIn.GroupInfo;
}

void FManagedArrayCollection::AddGroup(FName Group)
{
	ensure(!GroupInfo.Contains(Group));
	FGroupInfo info {
		0
	};
	GroupInfo.Add(Group, info);
}

void FManagedArrayCollection::RemoveElements(const FName & Group, const TArray<int32> & SortedDeletionList)
{
	if (SortedDeletionList.Num())
	{
		int32 GroupSize = NumElements(Group);
		int32 DelListNum = SortedDeletionList.Num();
		GeometryCollectionAlgo::ValidateSortedList(SortedDeletionList, GroupSize);
		ensure(GroupSize >= DelListNum);

		TArray<int32> Offsets;
		GeometryCollectionAlgo::BuildIncrementMask(SortedDeletionList, GroupSize, Offsets);

		for (const TTuple<FKeyType, FValueType>& Entry : Map)
		{
			//
			// Reindex attributes dependent on the group being resized
			//
			if (Entry.Value.GroupIndexDependency == Group)
			{
				Entry.Value.Value->Reindex(Offsets, GroupSize - DelListNum, SortedDeletionList);
			}

			//
			//  Resize the array and clobber deletion indices
			//
			if (Entry.Key.Get<1>() == Group)
			{
				FManagedArrayBase* NewArray = Entry.Value.Value->NewCopy(SortedDeletionList);
				Map[Entry.Key].Value->Init(*NewArray);
				delete NewArray;
			}

		}
		GroupInfo[Group].Size -= DelListNum;
	}
}


TArray<FName> FManagedArrayCollection::GroupNames() const
{
	TArray<FName> keys;
	if (GroupInfo.Num())
	{
		GroupInfo.GetKeys(keys);
	}
	return keys;
}

bool FManagedArrayCollection::HasAttribute(FName Name, FName Group) const
{
	bool bReturnValue = false;
	for (const TTuple<FKeyType,FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<0>() == Name && Entry.Key.Get<1>() == Group)
		{
			bReturnValue = true;
			break;
		}
	}
	return bReturnValue;
}

TArray<FName> FManagedArrayCollection::AttributeNames(FName Group) const
{
	TArray<FName> AttributeNames;
	for (const TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			AttributeNames.Add(Entry.Key.Get<0>());
		}
	}
	return AttributeNames;
}

int32 FManagedArrayCollection::NumElements(FName Group) const
{
	int32 Num = 0;
	if (GroupInfo.Contains(Group))
	{
		Num = GroupInfo[Group].Size;
	}
	return Num;
}

int32 FManagedArrayCollection::AddElements(int32 NumberElements, FName Group)
{
	int32 StartSize = 0;
	if (!GroupInfo.Contains(Group))
	{
		AddGroup(Group);
	}

	StartSize = GroupInfo[Group].Size;
	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			Entry.Value.Value->Resize(StartSize + NumberElements);
		}
	}
	GroupInfo[Group].Size += NumberElements;
	return StartSize;
}

void FManagedArrayCollection::LocalizeAttribute(FName Name, FName Group)
{
	ensure(HasAttribute(Name, Group));
	FKeyType key = FManagedArrayCollection::MakeMapKey(Name, Group);
	
	TSharedPtr<FManagedArrayBase> ManagedArrayPtr = TSharedPtr<FManagedArrayBase>(Map[key].Value->NewCopy());
	EArrayType ArrayType = Map[key].ArrayType;
	
	Map.Add(key, FValueType(ArrayType, ManagedArrayPtr));
	BindSharedArrays();
}

void FManagedArrayCollection::RemoveAttribute(FName Name, FName Group)
{
	for (const TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<0>() == Name && Entry.Key.Get<1>() == Group)
		{
			Map.Remove(Entry.Key);
			return;
		}
	}
}

void FManagedArrayCollection::Resize(int32 Size, FName Group)
{
	ensure(HasGroup(Group));
	ensureMsgf(Size > NumElements(Group),TEXT("Use RemoveElements to shrink a group."));
	GroupInfo[Group].Size = Size;
	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			Entry.Value.Value->Resize(Size);
		}
	}
}

void FManagedArrayCollection::SetDependency(FName Name, FName Group, FName DependencyGroup)
{
	ensure(HasAttribute(Name, Group));
	FKeyType Key = FManagedArrayCollection::MakeMapKey(Name, Group);
	Map[Key].GroupIndexDependency = DependencyGroup;
}

void FManagedArrayCollection::RemoveDependencyFor(FName Group)
{
	ensure(HasGroup(Group));
	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Value.GroupIndexDependency == Group)
		{
			Entry.Value.GroupIndexDependency = "";
		}
	}

}


#include <sstream> 
#include <string>
FString FManagedArrayCollection::ToString() const
{
	FString Buffer("");
	for (FName GroupName : GroupNames()) 
	{
		Buffer += GroupName.ToString() + "\n";
		for (FName AttributeName : AttributeNames(GroupName))
		{
			FKeyType Key = FManagedArrayCollection::MakeMapKey(AttributeName, GroupName);
			FValueType Value = Map[Key];

			const void * PointerAddress = static_cast<const void*>(Value.Value.Get());
			std::stringstream AddressStream;
			AddressStream << PointerAddress;

			Buffer += GroupName.ToString() + ":" + AttributeName.ToString() + " ["+ FString(AddressStream.str().c_str()) +"]\n";
		}
	}
	return Buffer;
}

void FManagedArrayCollection::SetArrayScopes(EArrayScope ArrayScope)
{
	for(TTuple<FKeyType, FValueType>& Elem : Map)
	{
		Elem.Value.ArrayScope = ArrayScope;
	}
}



void FManagedArrayCollection::Serialize(FArchive& Ar)
{
	int Version = 3;
	Ar << Version;

	Ar << GroupInfo << Map;
}

FArchive& operator<<(FArchive& Ar, FManagedArrayCollection::FGroupInfo& GroupInfo)
{
	int Version = 3;
	Ar << Version;

	Ar << GroupInfo.Size;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FManagedArrayCollection::FValueType& ValueIn)
{
	int Version = 3;
	Ar << Version;

	int ArrayTypeAsInt = static_cast<int>(ValueIn.ArrayType);
	Ar << ArrayTypeAsInt;
	ValueIn.ArrayType = static_cast<FManagedArrayCollection::EArrayType>(ArrayTypeAsInt);

	int ArrayScopeAsInt = static_cast<int>(ValueIn.ArrayScope);
	Ar << ArrayScopeAsInt;
	ValueIn.ArrayScope = static_cast<FManagedArrayCollection::EArrayScope>(ArrayScopeAsInt);

	if (Version >= 2)
	{
		Ar << ValueIn.GroupIndexDependency;
		Ar << ValueIn.Saved;
	}

	if (!ValueIn.Value.IsValid()) 
	{
		ValueIn.Value = NewManagedTypedArray(ValueIn.ArrayType);
	}

	if ( ValueIn.Saved )
	{
		ValueIn.Value->Serialize(Ar);
	}
	return Ar;
}
