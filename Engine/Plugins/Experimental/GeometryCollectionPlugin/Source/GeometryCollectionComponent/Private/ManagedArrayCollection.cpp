// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ManagedArrayCollection.h"

DEFINE_LOG_CATEGORY_STATIC(UManagedArrayCollectionLogging, NoLogging, All);

UManagedArrayCollection::UManagedArrayCollection(class FObjectInitializer const & Object)
	:UObject(Object)
{
}


void UManagedArrayCollection::Initialize(UManagedArrayCollection & CollectionIn)
{
	this->Map = CollectionIn.Map;
	this->GroupInfo = CollectionIn.GroupInfo;
}

void UManagedArrayCollection::AddGroup(FName Group)
{
	check(!GroupInfo.Contains(Group))
		FGroupInfo info {
		0
	};
	GroupInfo.Add(Group, info);
}


TArray<FName> UManagedArrayCollection::GroupNames() const
{
	TArray<FName> keys;
	if (GroupInfo.Num())
	{
		GroupInfo.GetKeys(keys);
	}
	return keys;
}

bool UManagedArrayCollection::HasAttribute(FName Name, FName Group) const
{
	bool bReturnValue = false;
	for (const auto& Entry : Map)
	{
		if (Entry.Key.Get<0>() == Name && Entry.Key.Get<1>() == Group)
		{
			bReturnValue = true;
			break;
		}
	}
	return bReturnValue;
}

TArray<FName> UManagedArrayCollection::AttributeNames(FName Group) const
{
	TArray<FName> AttributeNames;
	for (const auto & Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			AttributeNames.Add(Entry.Key.Get<0>());
		}
	}
	return AttributeNames;
}

int32 UManagedArrayCollection::NumElements(FName Group) const
{
	int32 Num = 0;
	if (GroupInfo.Contains(Group))
	{
		Num = GroupInfo[Group].Size;
	}
	return Num;
}

int32 UManagedArrayCollection::AddElements(int32 NumberElements, FName Group)
{
	int32 StartSize = 0;
	if (!GroupInfo.Contains(Group))
	{
		AddGroup(Group);
	}

	StartSize = GroupInfo[Group].Size;
	for (auto & Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			Entry.Value.Value->Resize(StartSize + NumberElements);
		}
	}
	GroupInfo[Group].Size += NumberElements;
	return StartSize;
}

void UManagedArrayCollection::LocalizeAttribute(FName Name, FName Group)
{
	check(HasAttribute(Name, Group));
	FKeyType key = UManagedArrayCollection::MakeMapKey(Name, Group);
	
	TSharedPtr<FManagedArrayBase> ManagedArrayPtr = TSharedPtr<FManagedArrayBase>(Map[key].Value->NewCopy());
	EArrayType ArrayType = Map[key].ArrayType;
	
	Map.Add(key, FValueType(ArrayType, ManagedArrayPtr));
	BindSharedArrays();
}

void UManagedArrayCollection::Resize(int32 Size, FName Group)
{
	check(HasGroup(Group));
	GroupInfo[Group].Size = Size;
	for (auto & Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			Entry.Value.Value->Resize(Size);
		}
	}
}

#include <sstream> 
#include <string>
FString UManagedArrayCollection::ToString() const
{
	FString Buffer("");
	for (FName GroupName : GroupNames()) 
	{
		Buffer += GroupName.ToString() + "\n";
		for (FName AttributeName : AttributeNames(GroupName))
		{
			auto Key = UManagedArrayCollection::MakeMapKey(AttributeName, GroupName);
			auto Value = Map[Key];

			const void * PointerAddress = static_cast<const void*>(Value.Value.Get());
			std::stringstream AddressStream;
			AddressStream << PointerAddress;

			Buffer += GroupName.ToString() + ":" + AttributeName.ToString() + " ["+ FString(AddressStream.str().c_str()) +"]\n";
		}
	}
	return Buffer;
}

void UManagedArrayCollection::SetArrayScopes(EArrayScope ArrayScope)
{
	for(auto& Elem : Map)
	{
		Elem.Value.ArrayScope = ArrayScope;
	}
}



void UManagedArrayCollection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	int Version = 1;
	Ar << Version;

	Ar << GroupInfo << Map;
}

FArchive& operator<<(FArchive& Ar, UManagedArrayCollection::FGroupInfo& GroupInfo)
{
	int Version = 1;
	Ar << Version;

	Ar << GroupInfo.Size;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, UManagedArrayCollection::FValueType& ValueIn)
{
	int Version = 1;
	Ar << Version;

	// @question : Is there a better way to do this?
	int ArrayTypeAsInt = static_cast<int>(ValueIn.ArrayType);
	Ar << ArrayTypeAsInt;
	ValueIn.ArrayType = static_cast<UManagedArrayCollection::EArrayType>(ArrayTypeAsInt);

	int ArrayScopeAsInt = static_cast<int>(ValueIn.ArrayScope);
	Ar << ArrayScopeAsInt;
	ValueIn.ArrayScope = static_cast<UManagedArrayCollection::EArrayScope>(ArrayScopeAsInt);

	if (!ValueIn.Value.IsValid()) 
	{
		ValueIn.Value = NewManagedTypedArray(ValueIn.ArrayType);
	}

	// make Serialize a public base
	// add a OnSerialize as private virtual. 
	ValueIn.Value->Serialize(Ar);
	return Ar;
}
