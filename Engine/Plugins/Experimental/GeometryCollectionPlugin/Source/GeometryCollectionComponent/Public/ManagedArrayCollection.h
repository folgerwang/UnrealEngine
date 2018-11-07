// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ManagedArray.h"
#include "ManagedArrayTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "GeometryCollectionBoneNode.h"

#include "ManagedArrayCollection.generated.h"

/**
* ManagedArrayCollection (UObject)
*
*   ManagedArrayCollection will store groups of TArray attributes, where each group is constrained to the same length.
*   The collection will make use of the TManagedArray class, that provides access to the TArray attributes Resizing of
*   the groups is performed by the ManagedArrayCollection.
*
*   For example:
*
	UManagedArrayCollection* Collection(NewObject<UManagedArrayCollection>());
	Collection->AddElements(10, "GroupBar"); // Create a group GroupBar and add 10 elements.
	Collection->AddAttribute<FVector>("AttributeFoo", "GroupBar"); // Add a FVector array named AttributeFoo to GroupBar.
	TSharedPtr< TManagedArray<FVector> >  FooArray = Collection->GetAttribute<FVector>("AttributeFoo", "GroupBar"); // Get AttributeFoo
	TManagedArray<FVector>& Foo = *FooArray; // for optimal usage, de-reference the shared pointer before iterative access.
	for (int32 i = 0; i < Foo.Num(); i++)
	{
		Foo[i] = FVector(i, i, i); // Update AttribureFoo's elements
	}
*
*
*
*/
UCLASS()
class GEOMETRYCOLLECTIONCOMPONENT_API UManagedArrayCollection : public UObject
{
	GENERATED_UCLASS_BODY()

	typedef EManagedArrayType EArrayType;

	/**
	* EArrayScope will indicate if the array is locally owned or
	* shared across multiple collections. Be careful with the
	* FScopeShared types, modifying these will modify all
	* connected collections. 
	*/
	enum class EArrayScope : uint8
	{
		FScopeShared,
		FScopeLocal
	};


private:

	/****
	*  Mapping Key/Value
	*
	*    The Key and Value pairs for the attribute arrays allow for grouping
	*    of array attributes based on the Name,Group pairs. Array attributes
	*    that share Group names will have the same lengths.
	*
	*    The FKeyType is a tuple of <FName,FName> where the Get<0>() parameter
	*    is the AttributeName, and the Get<1>() parameter is the GroupName.
	*    Construction of the FKeyType will follow the following pattern:
	*    FKeyType("AttributeName","GroupName");
	*
	*/
	typedef TTuple<FName, FName> FKeyType;
	struct FGroupInfo
	{
		int32 Size;
	}; 

	static FKeyType MakeMapKey(FName Name, FName Group)
	{
		return FKeyType(Name, Group);
	};



	struct FValueType
	{
		EArrayType ArrayType;
		EArrayScope ArrayScope;

		TSharedPtr<FManagedArrayBase> Value;

		FValueType() : ArrayType(EArrayType::FNoneType),ArrayScope(EArrayScope::FScopeShared), Value() {};

		FValueType(EArrayType ArrayTypeIn, TSharedPtr <FManagedArrayBase> In) :
			ArrayType(ArrayTypeIn)
			, ArrayScope(EArrayScope::FScopeLocal)
			, Value(In) 
		{};
	};

	template<class T>
	static FValueType MakeMapValue()
	{
		return FValueType( ManagedArrayType<T>(), TSharedPtr< TManagedArray<T> >(new TManagedArray<T>()) );
	};

	TMap< FKeyType, FValueType> Map;
	TMap< FName, FGroupInfo> GroupInfo;

protected:

	/***/
	virtual void BindSharedArrays() {}

	/**
	* Returns attribute access of Type(T) from the group
	* @param Name - The name of the attribute
	* @param Group - The group that manages the attribute
	* @return const ManagedArray<T> &
	*/
	template<class T>
	TSharedRef< TManagedArray<T> > ShareAttribute(FName Name, FName Group)
	{
		if (! HasAttribute(Name, Group) )
		{
			AddAttribute<T>(Name, Group);
		}
		FKeyType key = UManagedArrayCollection::MakeMapKey(Name, Group);
		return StaticCastSharedPtr< TManagedArray<T> >(Map[key].Value).ToSharedRef();
	};

public:

	/**
	* Add an attribute of Type(T) to the group, from an existing ManagedAttribute
	* @param Name - The name of the attribute
	* @param Group - The group that manages the attribute
	* @param ValueIn - The group that manages the attribute
	* @return reference to the stored ManagedArray<T>
	*/
	template<class T>
	TSharedRef< TManagedArray<T> > AddAttribute(FName Name, FName Group, TSharedPtr<FManagedArrayBase> ValueIn)
	{
		check(ValueIn.IsValid());
		check(!HasAttribute(Name, Group));

		if (!HasGroup(Group))
			AddGroup(Group);

		FValueType Value = FValueType(ManagedArrayType<T>(), ValueIn);
		Value.Value->Resize(NumElements(Group));
		Map.Add(UManagedArrayCollection::MakeMapKey(Name, Group), Value);

		return StaticCastSharedPtr< TManagedArray<T> >(Value.Value).ToSharedRef();
	};

	/**
	* Add an attribute of Type(T) to the group
	* @param Name - The name of the attribute
	* @param Group - The group that manages the attribute
	* @return reference to the created ManagedArray<T>
	*/
	template<class T>
	TSharedRef< TManagedArray<T> > AddAttribute(FName Name, FName Group)
	{
		if (!HasAttribute(Name, Group))
		{
			if (!HasGroup(Group))
				AddGroup(Group);

			FValueType Value = UManagedArrayCollection::MakeMapValue<T>();
			Value.Value->Resize(NumElements(Group));
			Map.Add(UManagedArrayCollection::MakeMapKey(Name, Group), Value);
		}
		return GetAttribute<T>(Name, Group);
	};

	/**
	* Create a group on the collection. Adding attribute will also create unknown groups.
	* @param Group - The group name
	*/
	void AddGroup(FName Group);


	/**
	* List all the attributes in a group names.
	* @return list of group names
	*/
	TArray<FName> AttributeNames(FName Group) const;

	/**
	* Add elements to a group
	* @param NumberElements - The number of array entries to add
	* @param Group - The group to append entries to.
	* @return starting index of the new ManagedArray<T> entries.
	*/
	int32 AddElements(int32 NumberElements, FName Group);

	/**
	* Returns attribute(Name) of Type(T) from the group
	* @param Name - The name of the attribute
	* @param Group - The group that manages the attribute
	* @return ManagedArray<T> &
	*/
	template<class T>
	TSharedPtr<TManagedArray<T> > FindAttribute(FName Name, FName Group)
	{
		if (HasAttribute(Name, Group))
		{
			FKeyType key = UManagedArrayCollection::MakeMapKey(Name, Group);
			return StaticCastSharedPtr< TManagedArray<T> >(Map[key].Value);
		}
		return TSharedPtr<TManagedArray<T> >(0);
	};

	/**
	* Returns const attribute access of Type(T) from the group
	* @param Name - The name of the attribute
	* @param Group - The group that manages the attribute
	* @return const ManagedArray<T> &
	*/
	template<class T>
	TSharedRef< TManagedArray<T> > GetAttribute(FName Name, FName Group)
	{
		check(HasAttribute(Name, Group))
		FKeyType key = UManagedArrayCollection::MakeMapKey(Name, Group);
		return StaticCastSharedPtr< TManagedArray<T> >(Map[key].Value).ToSharedRef();
	};

	template<class T>
	const TSharedRef< TManagedArray<T> > GetAttribute(FName Name, FName Group) const
	{
		check(HasAttribute(Name, Group))
			FKeyType key = UManagedArrayCollection::MakeMapKey(Name, Group);
		return StaticCastSharedPtr< TManagedArray<T> >(Map[key].Value).ToSharedRef();
	};

	/**
	* List all the group names.
	*/
	TArray<FName> GroupNames() const;

	/**
	* Check for the existence of a attribute
	* @param Name - The name of the attribute
	* @param Group - The group that manages the attribute
	*/
	bool HasAttribute(FName Name, FName Group) const;

	/**
	* Check for the existence of a group
	* @param Group - The group name
	*/
	FORCEINLINE bool HasGroup(FName Group) const { return GroupInfo.Contains(Group); }

	/** 
	* Setup collection based on input collection, resulting arrays are shared.
	* @param CollectionIn : Input collection to share
	*/
	virtual void Initialize(UManagedArrayCollection & CollectionIn);

	/**
	* Copy the shared reference, removing the connection to any shared attributes.
	* @param Name - The name of the attribute
	* @param Group - The group that manages the attribute
	*/
	void LocalizeAttribute(FName Name, FName Group);

	/**
	* Number of elements in a group
	* @return the group size, and Zero(0) if group does not exist.
	*/
	int32 NumElements(FName Group) const;

	/**
	* Resize a group
	* @param Size - The size of the group
	* @param Group - The group to resize
	*/
	void Resize(int32 Size, FName Group);

	/**
	* Serialize
	*/
	virtual void Serialize(FArchive& Ar) override;

	/**
	* Set the managed arrays scope. Assets that share their state with others 
	* should be marked as FAssetScope. When an attributed is localized this 
	* will change the scope to FScopeLocal. This is a record keeping tool to 
	* validate how the attributes are exposed through the application.
	* @param Scope : FAssetScope for shared, FLocalScope for not shared
	*/
	void SetArrayScopes(EArrayScope Scope);

	/**
	* Dump the contents to a FString
	*/
	FString ToString() const;

	/**
	*   operator<<(FGroupInfo)
	*/
	friend FArchive& operator<<(FArchive& Ar, FGroupInfo& GroupInfo);

	/**
	*   operator<<(FValueType)
	*/
	friend FArchive& operator<<(FArchive& Ar, FValueType& ValueIn);

};

