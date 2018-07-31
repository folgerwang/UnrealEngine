// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

class UManagedArrayCollection;
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
	friend UManagedArrayCollection;
protected:
	/**
	* Protected access to array resizing. Only the managers of the Array
	* are allowed to perform a resize. (see friend list above).
	*/
	virtual void Resize(const int32 Num) {};
public:

	virtual ~FManagedArrayBase() {}

	/** Return unmanaged copy of array. */
	virtual FManagedArrayBase * NewCopy() 
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

};


/***
*  Managed Array
*
*  Restricts clients ability to resize the array external to the containing manager. 
*/
template<class InElementType>
class TManagedArray : public FManagedArrayBase
{

public:

	using ElementType = InElementType;

	/**
	* Constructor (default) Build an empty shared array
	*
	*/	
	FORCEINLINE TManagedArray()
	{}

	/**
	* Constructor (TArray)
	*
	*/
	FORCEINLINE TManagedArray(const TArray<ElementType>& Other)
		: Array(Other)
	{}

	/**
	* Copy Constructor (default)
	*/
	FORCEINLINE TManagedArray(const TManagedArray<ElementType>& Other) = delete;

	/**
	* Copy Constructor
	*/
	FORCEINLINE TManagedArray(const TManagedArray<ElementType>&& Other) = delete;

	/**
	* Assignment operator
	*/
	FORCEINLINE TManagedArray& operator=(TManagedArray<ElementType>&& Other) = delete;


	/**
	* Virtual Destructor 
	*
	*/
	virtual ~TManagedArray()
	{}

	/**
	* Return a copy of the ManagedArray.
	*/
	virtual FManagedArrayBase * NewCopy() override
	{
		return new TManagedArray<ElementType>(Array);
	}

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
	FORCEINLINE size_t GetTypeSize() const
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
