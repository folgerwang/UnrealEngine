// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Containers/Array.h"
#include "GeometryCollection/GeometryCollectionBoneNode.h"
#include "GeometryCollection/GeometryCollectionSection.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

class FManagedArrayCollection;
DEFINE_LOG_CATEGORY_STATIC(UManagedArrayLogging, NoLogging, All);

/***
*  Managed Array Base
*
*  The ManagedArrayBase allows a common base class for the
*  the template class ManagedArray<T>. (see ManagedArray)
*
*/
class FManagedArrayBase : public FNoncopyable
{
	friend FManagedArrayCollection;
protected:
	/**
	* Protected access to array resizing. Only the managers of the Array
	* are allowed to perform a resize. (see friend list above).
	*/
	virtual void Resize(const int32 Num) {};

	/**
	* Init from a predefined Array
	*/
	virtual void Init(const FManagedArrayBase& ) {};

public:

	virtual ~FManagedArrayBase() {}

	/** Return unmanaged copy of array. */
	virtual FManagedArrayBase * NewCopy() 
	{ 
		check(false); 
		return nullptr; 
	}

	/** Return unmanaged copy of array with input indices. */
	virtual FManagedArrayBase * NewCopy(const TArray<int32> & DeletionList)
	{
		check(false);
		return nullptr;
	}

	/** The length of the array.*/
	virtual int32 Num() const 
	{
		return 0; 
	};

	/** Serialization */
	virtual void Serialize(FArchive& Ar) 
	{
		check(false);
	}

	/** TypeSize */
	virtual size_t GetTypeSize() const
	{
		return 0;
	}

	/**
	* Reindex - Adjust index dependent elements.  
	*   Offsets is the size of the dependent group;
	*   Final is post resize of dependent group used for bounds checking on remapped indices.
	*/
	virtual void Reindex(const TArray<int32> & Offsets, const int32 & FinalSize, const TArray<int32> & SortedDeletionList) { }

};


/***
*  Managed Array
*
*  Restricts clients ability to resize the array external to the containing manager. 
*/
template<class InElementType>
class TManagedArrayBase : public FManagedArrayBase
{

public:

	using ElementType = InElementType;

	/**
	* Constructor (default) Build an empty shared array
	*
	*/	
	FORCEINLINE TManagedArrayBase()
	{}

	/**
	* Constructor (TArray)
	*
	*/
	FORCEINLINE TManagedArrayBase(const TArray<ElementType>& Other)
		: Array(Other)
	{}

	/**
	* Copy Constructor (default)
	*/
	FORCEINLINE TManagedArrayBase(const TManagedArrayBase<ElementType>& Other) = delete;

	/**
	* Move Constructor
	*/
	FORCEINLINE TManagedArrayBase(TManagedArrayBase<ElementType>&& Other) = delete;

	/**
	* Assignment operator
	*/
	FORCEINLINE TManagedArrayBase& operator=(TManagedArrayBase<ElementType>&& Other) = delete;


	/**
	* Virtual Destructor 
	*
	*/
	virtual ~TManagedArrayBase()
	{}

	/**
	* Return a copy of the ManagedArray.
	*/
	virtual FManagedArrayBase * NewCopy() override
	{
		return new TManagedArrayBase<ElementType>(Array);
	}

	virtual FManagedArrayBase * NewCopy(const TArray<int32> & SortedDeletionList) override
	{
		int32 ArraySize = Array.Num();
		int32 DeletionListSize = SortedDeletionList.Num();
		TManagedArrayBase<ElementType>* BaseArray = new TManagedArrayBase<ElementType>();
		BaseArray->Resize(ArraySize - DeletionListSize);

		// walk the old array skipping the deleted indices
		int32 IndexA = 0, IndexB = 0, DelIndex = 0;
		for (; DelIndex < DeletionListSize; DelIndex++, IndexA++)
		{
			while (IndexA < SortedDeletionList[DelIndex])
			{
				BaseArray->operator[](IndexB++) = Array[IndexA++];
			}
		}
		for (int32 Index = IndexA; Index < ArraySize && IndexB<BaseArray->Num(); Index++)
		{
			BaseArray->operator[](IndexB++) = Array[Index];
		}
		return BaseArray;
	}

	/**
	* Init from a predefined Array of matching type
	*/
	virtual void Init(const FManagedArrayBase& NewArray) override
	{
		ensureMsgf(NewArray.GetTypeSize() == GetTypeSize(),TEXT("TManagedArrayBase<T>::Init : Invalid array types."));
		const TManagedArrayBase<ElementType> & NewTypedArray = static_cast< const TManagedArrayBase<ElementType>& >(NewArray);
		int32 Size = NewTypedArray.Num();

		Resize(Size);
		for (int32 Index = 0; Index < Size; Index++)
		{
			Array[Index] = NewTypedArray[Index];
		}
	};

	/**
	* Returning a reference to the element at index.
	*
	* @returns Array element reference
	*/
	FORCEINLINE ElementType & operator[](int Index)
	{
		// @todo : optimization
		// TArray->operator(Index) will perform checks against the 
		// the array. It might be worth implementing the memory
		// management directly on the ManagedArray, to avoid the
		// overhead of the TArray.
		return Array[Index];
	}
	FORCEINLINE const ElementType & operator[](int Index) const
	{
		return Array[Index];
	}

	/**
	* Helper function for returning a typed pointer to the first array entry.
	*
	* @returns Pointer to first array entry or nullptr if ArrayMax == 0.
	*/
	FORCEINLINE const ElementType * GetData() const
	{
		return &Array.operator[](0);
	}

	/**
	* Helper function returning the size of the inner type.
	*
	* @returns Size in bytes of array type.
	*/
	FORCEINLINE size_t GetTypeSize() const override
	{
		return sizeof(ElementType);
	}

	/**
	* Returning the size of the array
	*
	* @returns Array size
	*/
	FORCEINLINE int32 Num() const override
	{
		return Array.Num();
	}

	/**
	* Find first index of the element
	*/
	int32 Find(const ElementType& Item) const
	{
		return Array.Find(Item);
	}


	/**
	* Checks if index is in array range.
	*
	* @param Index Index to check.
	*/
	FORCEINLINE void RangeCheck(int32 Index) const
	{
		checkf((Index >= 0) & (Index < Array.Num()), TEXT("Array index out of bounds: %i from an array of size %i"), Index, Array.Num());
	}

	/**
	* Serialization Support
	*
	* @param FArchive& Ar
	*/
	virtual void Serialize(FArchive& Ar)
	{		
		int Version = 1;
		Ar << Version;
		Ar << Array;
	}

	// @todo Add RangedFor support. 

private:

	/**
	* Protected Resize to prevent external resizing of the array
	*
	* @param New array size.
	*/
	void Resize(const int32 Size) 
	{ 
		Array.SetNum(Size,true);
	}

	TArray<InElementType> Array;

};

template<class InElementType>
class TManagedArray : public TManagedArrayBase<InElementType>
{
public:
	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<InElementType>& Other)
		: TManagedArrayBase<InElementType>(Other)
	{}

	FORCEINLINE TManagedArray(const TManagedArray<InElementType>& Other) = delete;
	FORCEINLINE TManagedArray(TManagedArray<InElementType>&& Other) = delete;
	FORCEINLINE TManagedArray& operator=(TManagedArray<InElementType>&& Other) = delete;

	virtual ~TManagedArray()
	{}
};

template<>
class TManagedArray<int32> : public TManagedArrayBase<int32>
{
public:
    using TManagedArrayBase<int32>::Num;

	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<int32>& Other)
		: TManagedArrayBase<int32>(Other)
	{}

	FORCEINLINE TManagedArray(const TManagedArray<int32>& Other) = delete;
	FORCEINLINE TManagedArray(TManagedArray<int32>&& Other) = delete;
	FORCEINLINE TManagedArray& operator=(TManagedArray<int32>&& Other) = delete;

	virtual ~TManagedArray()
	{}

	virtual void Reindex(const TArray<int32> & Offsets, const int32 & FinalSize, const TArray<int32> & SortedDeletionList) override
	{
		UE_LOG(UManagedArrayLogging, Log, TEXT("TManagedArray<int32>[%p]::Reindex()"),this);
	
		int32 ArraySize = Num(), MaskSize = Offsets.Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			int32 RemapVal = this->operator[](Index);
			if (0 <= RemapVal)
			{
				ensure(RemapVal < MaskSize);
				this->operator[](Index) -= Offsets[RemapVal];
				ensure(-1 <= this->operator[](Index) && this->operator[](Index) < FinalSize);
			}
		}
	}
};


template<>
class TManagedArray<TSet<int32>> : public TManagedArrayBase<TSet<int32>>
{
public:
	using TManagedArrayBase<TSet<int32>>::Num;

	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<TSet<int32>>& Other)
		: TManagedArrayBase< TSet<int32> >(Other)
	{}

	FORCEINLINE TManagedArray(const TManagedArray<TSet<int32>>& Other) = delete;
	FORCEINLINE TManagedArray(TManagedArray<TSet<int32>>&& Other) = delete;
	FORCEINLINE TManagedArray& operator=(TManagedArray<TSet<int32>>&& Other) = delete;

	virtual ~TManagedArray()
	{}
	
	virtual void Reindex(const TArray<int32> & Offsets, const int32 & FinalSize, const TArray<int32> & SortedDeletionList) override
	{
		UE_LOG(UManagedArrayLogging, Log, TEXT("TManagedArray<TArray<int32>>[%p]::Reindex()"), this);
		
		int32 ArraySize = Num(), MaskSize = Offsets.Num();
		TSet<int32> SortedDeletionSet(SortedDeletionList);

		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			TSet<int32>& RemapVal = this->operator[](Index);

			RemapVal = RemapVal.Difference(RemapVal.Intersect(SortedDeletionSet));

			for (int i = 0; i < RemapVal.Num(); i++)
			{
				FSetElementId Id = RemapVal.FindId(RemapVal.Array()[i]);
				ensure(Id.IsValidId());
				if (0 <= RemapVal[Id])
				{
					ensure(RemapVal[Id] < MaskSize);
					this->operator[](Index)[Id] -= Offsets[RemapVal[Id]];
					ensure(-1 <= this->operator[](Index)[Id] && this->operator[](Index)[Id] < FinalSize);
				}
			}
		}
		
	}
};

template<>
class TManagedArray<FIntVector> : public TManagedArrayBase<FIntVector>
{
public:
    using TManagedArrayBase<FIntVector>::Num;

	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<FIntVector>& Other)
		: TManagedArrayBase<FIntVector>(Other)
	{}

	FORCEINLINE TManagedArray(const TManagedArray<FIntVector>& Other) = delete;
	FORCEINLINE TManagedArray(TManagedArray<FIntVector>&& Other) = delete;
	FORCEINLINE TManagedArray& operator=(TManagedArray<FIntVector>&& Other) = delete;

	virtual ~TManagedArray()
	{}

	virtual void Reindex(const TArray<int32> & Offsets, const int32 & FinalSize, const TArray<int32> & SortedDeletionList) override
	{
		UE_LOG(UManagedArrayLogging, Log, TEXT("TManagedArray<FIntVector>[%p]::Reindex()"), this);
		int32 ArraySize = Num(), MaskSize = Offsets.Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			const FIntVector & RemapVal = this->operator[](Index);
			for (int i = 0; i < 3; i++)
			{
				if (0 <= RemapVal[i])
				{
					ensure(RemapVal[i] < MaskSize);
					this->operator[](Index)[i] -= Offsets[RemapVal[i]];
					ensure(-1 <= this->operator[](Index)[i] && this->operator[](Index)[i] < FinalSize);
				}
			}
		}
	}
};

template<>
class TManagedArray<FGeometryCollectionBoneNode> : public TManagedArrayBase<FGeometryCollectionBoneNode>
{
public:
    using TManagedArrayBase<FGeometryCollectionBoneNode>::Num;

	FORCEINLINE TManagedArray()
	{}

	FORCEINLINE TManagedArray(const TArray<FGeometryCollectionBoneNode>& Other)
		: TManagedArrayBase<FGeometryCollectionBoneNode>(Other)
	{}

	FORCEINLINE TManagedArray(const TManagedArray<FGeometryCollectionBoneNode>& Other) = delete;
	FORCEINLINE TManagedArray(TManagedArray<FGeometryCollectionBoneNode>&& Other) = delete;
	FORCEINLINE TManagedArray& operator=(TManagedArray<FGeometryCollectionBoneNode>&& Other) = delete;

	virtual ~TManagedArray()
	{}

	virtual void Reindex(const TArray<int32> & Offsets, const int32 & FinalSize, const TArray<int32> & SortedDeletionList) override
	{
		UE_LOG(UManagedArrayLogging, Log, TEXT("TManagedArray<FGeometryCollectionBoneNode>[%p]::Reindex()"), this);
	
		int32 ArraySize = Num(), OffsetsSize = Offsets.Num();
		for (int32 Index = 0; Index < ArraySize; Index++)
		{
			FGeometryCollectionBoneNode & Node = this->operator[](Index);
	
			// remap the parents (-1 === Invalid )
			if (Node.Parent != -1)
				Node.Parent -= Offsets[Node.Parent];
			ensure(-1 <= Node.Parent && Node.Parent < FinalSize);
	
			// remap children
			TSet<int32> Children = Node.Children;
			Node.Children.Empty();
			for (int32 ChildID : Children)
			{
				if (0 <= ChildID && ChildID < OffsetsSize)
				{
					int32 NewChildID = ChildID - Offsets[ChildID];
					if (0 <= NewChildID && NewChildID < FinalSize)
					{
						Node.Children.Add(NewChildID);
					}
				}
			}
		}
	}
};
