// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshTypes.h"
#include "MeshElementRemappings.h"
#include "UObject/ReleaseObjectVersion.h"


/**
 * List of attribute types which are supported.
 * We do this so we can automatically generate the attribute containers and their associated accessors with
 * some template magic.
 *
 * IMPORTANT NOTE: Do not remove any type from this tuple, or serialization will fail.
 * Types may be added at the end of this list if necessary, although please do so sparingly as each extra type will
 * impact on performance and object size.
 */
using AttributeTypes = TTuple
<
	FVector4,
	FVector,
	FVector2D,
	float,
	int,
	bool,
	FName
>;


/**
 * Traits class to specify which attribute types can be bulk serialized.
 */
template <typename T> struct TIsBulkSerializable { static const bool Value = true; };
template <> struct TIsBulkSerializable<FName> { static const bool Value = false; };


/**
 * This defines the container used to hold mesh element attributes of a particular name and index.
 * It is a simple TArray, so that all attributes are packed contiguously for each element ID.
 *
 * Note that the container may grow arbitrarily as new elements are inserted, but it will never be
 * shrunk as elements are removed. The only operations that will shrink the container are Initialize() and Remap().
 */
template <typename ElementType>
class TMeshAttributeArrayBase
{
public:

	/**
	 * Custom serialization for TMeshAttributeArrayBase.
	 */
	template <typename T>
	friend typename TEnableIf<!TIsBulkSerializable<T>::Value, FArchive>::Type& operator<<( FArchive& Ar, TMeshAttributeArrayBase<T>& Array );

	template <typename T>
	friend typename TEnableIf<TIsBulkSerializable<T>::Value, FArchive>::Type& operator<<( FArchive& Ar, TMeshAttributeArrayBase<T>& Array );

protected:

	friend class UMeshDescription;

	/** Should not instance this base class directly */
	TMeshAttributeArrayBase() = default;

	/** Disallow use of the copy constructor outside of FMeshDescription, to prevent arrays being mistakenly accessed in the UMeshDescription by value */
	TMeshAttributeArrayBase( const TMeshAttributeArrayBase& ) = default;
	TMeshAttributeArrayBase( TMeshAttributeArrayBase&& ) = default;
	TMeshAttributeArrayBase& operator=( const TMeshAttributeArrayBase& ) = default;
	TMeshAttributeArrayBase& operator=( TMeshAttributeArrayBase&& ) = default;

	/** Expands the array if necessary so that the passed element index is valid. Newly created elements will be assigned the default value. */
	void Insert( const int32 Index, const ElementType& Default )
	{
		if( Index >= Container.Num() )
		{
			// If the index is off the end of the container, add as many elements as required to make it the last valid index.
			int32 StartIndex = Container.AddUninitialized( Index + 1 - Container.Num() );
			ElementType* Data = Container.GetData() + StartIndex;

			// Construct added elements with the default value passed in
			while( StartIndex <= Index )
			{
				new( Data ) ElementType( Default );
				StartIndex++;
				Data++;
			}
		}
	}

	/** Initializes the array to the given size with the default value */
	void Initialize( const int32 ElementCount, const ElementType& Default )
	{
		Container.Reset( ElementCount );
		Insert( ElementCount - 1, Default );
	}

	/** The actual container, represented by a regular array */
	TArray<ElementType> Container;
};


template <typename T>
inline typename TEnableIf<!TIsBulkSerializable<T>::Value, FArchive>::Type& operator<<( FArchive& Ar, TMeshAttributeArrayBase<T>& Array )
{
	// Serialize types which aren't bulk serializable, which need to be serialized element-by-element
	Ar << Array.Container;
	return Ar;
}

template <typename T>
inline typename TEnableIf<TIsBulkSerializable<T>::Value, FArchive>::Type& operator<<( FArchive& Ar, TMeshAttributeArrayBase<T>& Array )
{
	if( Ar.IsLoading() && Ar.CustomVer( FReleaseObjectVersion::GUID ) < FReleaseObjectVersion::MeshDescriptionNewSerialization )
	{
		// Legacy path for old format attribute arrays. BulkSerialize has a different format from regular serialization.
		Ar << Array.Container;
	}
	else
	{
		// Serialize types which are bulk serializable, i.e. which can be memcpy'd in bulk
		Array.Container.BulkSerialize( Ar );
	}

	return Ar;
}



template <typename AttributeType, typename ElementIDType>
class TAttributeIndicesArray;

/**
 * We prefer to access elements of the container via strongly-typed IDs.
 * This derived class imposes this type safety.
 */
template <typename ElementType, typename ElementIDType>
class TMeshAttributeArray : private TMeshAttributeArrayBase<ElementType>
{
	static_assert( TIsDerivedFrom<ElementIDType, FElementID>::IsDerived, "ElementIDType must be derived from FElementID" );

	using TMeshAttributeArrayBase<ElementType>::Container;

public:

	/** Element accessors */
	FORCEINLINE const ElementType& operator[]( const ElementIDType ElementID ) const { return Container[ ElementID.GetValue() ]; }
	FORCEINLINE ElementType& operator[]( const ElementIDType ElementID ) { return Container[ ElementID.GetValue() ]; }

	/** Return size of container */
	FORCEINLINE int32 Num() const { return Container.Num(); }

	/** Return base of data */
	FORCEINLINE const ElementType* GetData() const { return Container.GetData(); }

protected:

	friend class TAttributeIndicesArray<ElementType, ElementIDType>;

	/** Expands the array if necessary so that the passed element index is valid. Newly created elements will be assigned the default value. */
	FORCEINLINE void Insert( const ElementIDType Index, const ElementType& Default )
	{
		TMeshAttributeArrayBase<ElementType>::Insert( Index.GetValue(), Default );
	}

	/** Serializer */
	friend FORCEINLINE FArchive& operator<<( FArchive& Ar, TMeshAttributeArray& Array )
	{
		Ar << static_cast<TMeshAttributeArrayBase<ElementType>&>( Array );
		return Ar;
	}

	/** Remaps elements according to the passed remapping table */
	void Remap( const TSparseArray<ElementIDType>& IndexRemap, const ElementType& Default );
};


template <typename ElementType, typename ElementIDType>
void TMeshAttributeArray<ElementType, ElementIDType>::Remap( const TSparseArray<ElementIDType>& IndexRemap, const ElementType& Default )
{
	TMeshAttributeArray NewAttributeArray;

	for( typename TSparseArray<ElementIDType>::TConstIterator It( IndexRemap ); It; ++It )
	{
		const int32 OldElementIndex = It.GetIndex();
		const ElementIDType NewElementIndex = IndexRemap[ OldElementIndex ];

		NewAttributeArray.Insert( NewElementIndex, Default );
		NewAttributeArray[ NewElementIndex ] = MoveTemp( Container[ OldElementIndex ] );
	}

	Container = MoveTemp( NewAttributeArray.Container );
}


/** Define aliases for element attributes */
template <typename AttributeType> using TVertexAttributeArray = TMeshAttributeArray<AttributeType, FVertexID>;
template <typename AttributeType> using TVertexInstanceAttributeArray = TMeshAttributeArray<AttributeType, FVertexInstanceID>;
template <typename AttributeType> using TEdgeAttributeArray = TMeshAttributeArray<AttributeType, FEdgeID>;
template <typename AttributeType> using TPolygonAttributeArray = TMeshAttributeArray<AttributeType, FPolygonID>;
template <typename AttributeType> using TPolygonGroupAttributeArray = TMeshAttributeArray<AttributeType, FPolygonGroupID>;


/**
 * Flags specifying properties of an attribute
 * @todo mesh description: this needs to be moved to a application-specific place;
 * this code is too low-level to assume particular meanings for the flags.
 */
enum class EMeshAttributeFlags : uint32
{
	None				= 0,
	Lerpable			= ( 1 << 0 ),
	AutoGenerated		= ( 1 << 1 ),
	Mergeable			= ( 1 << 2 )
};

ENUM_CLASS_FLAGS( EMeshAttributeFlags );


/**
 * This class represents a container for a named attribute on a mesh element.
 * It contains an array of TMeshAttributeArrays, one per attribute index. 
 */
template <typename T, typename U>
class TAttributeIndicesArray
{
public:

	using AttributeType = T;
	using ElementIDType = U;

	/** Default constructor - required so that it builds correctly */
	TAttributeIndicesArray() = default;

	/** Constructor */
	TAttributeIndicesArray( const int32 NumberOfIndices, const AttributeType& InDefaultValue, const EMeshAttributeFlags InFlags, const int32 InNumberOfElements )
		: NumElements( InNumberOfElements ),
		  DefaultValue( InDefaultValue ),
		  Flags( InFlags )
	{
		SetNumIndices( NumberOfIndices );
	}

	/** Insert the element at the given index */
	FORCEINLINE void Insert( const ElementIDType ElementID )
	{
		for( TMeshAttributeArray<AttributeType, ElementIDType>& ArrayForIndex : ArrayForIndices )
		{
			ArrayForIndex.Insert( ElementID, DefaultValue );
		}

		NumElements = FMath::Max( NumElements, ElementID.GetValue() + 1 );
	}

	/** Remove the element at the given index, replacing it with a default value */
	FORCEINLINE void Remove( const ElementIDType ElementID )
	{
		for( TMeshAttributeArray<AttributeType, ElementIDType>& ArrayForIndex : ArrayForIndices )
		{
			ArrayForIndex[ ElementID ] = DefaultValue;
		}
	}

	/** Return the TMeshAttributeArray corresponding to the given attribute index */
	FORCEINLINE const TMeshAttributeArray<AttributeType, ElementIDType>& GetArrayForIndex( const int32 Index ) const { return ArrayForIndices[ Index ]; }
	FORCEINLINE TMeshAttributeArray<AttributeType, ElementIDType>& GetArrayForIndex( const int32 Index ) { return ArrayForIndices[ Index ]; }

	/** Return flags for this attribute type */
	FORCEINLINE EMeshAttributeFlags GetFlags() const { return Flags; }

	/** Return default value for this attribute type */
	FORCEINLINE AttributeType GetDefaultValue() const { return DefaultValue; }

	/** Return number of indices this attribute has */
	FORCEINLINE int32 GetNumIndices() const { return ArrayForIndices.Num(); }

	/** Return number of elements each attribute index has */
	FORCEINLINE int32 GetNumElements() const { return NumElements; }

	/** Sets number of indices this attribute has */
	void SetNumIndices( const int32 NumIndices )
	{
		check( NumIndices > 0 );
		const int32 OriginalNumIndices = ArrayForIndices.Num();
		ArrayForIndices.SetNum( NumIndices );

		// If we have added new indices, ensure they are filled out with the correct number of elements
		for( int32 Index = OriginalNumIndices; Index < NumIndices; ++Index )
		{
			ArrayForIndices[ Index ].Initialize( NumElements, DefaultValue );
		}
	}

	/** Sets the number of elements to the exact number provided, and initializes them to the default value */
	void Initialize( const int32 Count )
	{
		NumElements = Count;
		for( TMeshAttributeArray<AttributeType, ElementIDType>& ArrayForIndex : ArrayForIndices )
		{
			ArrayForIndex.Initialize( Count, DefaultValue );
		}
	}

	/** Remaps all attribute indices according to the passed mapping */
	void Remap( const TSparseArray<ElementIDType>& IndexRemap )
	{
		for( TMeshAttributeArray<AttributeType, ElementIDType>& ArrayForIndex : ArrayForIndices )
		{
			ArrayForIndex.Remap( IndexRemap, DefaultValue );
			NumElements = ArrayForIndex.Num();
		}
	}

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, TAttributeIndicesArray& AttributesArray )
	{
		Ar << AttributesArray.NumElements;
		Ar << AttributesArray.ArrayForIndices;
		Ar << AttributesArray.DefaultValue;
		Ar << AttributesArray.Flags;
		return Ar;
	}

private:
	/** Number of elements in each index */
	int32 NumElements;

	/** An array of MeshAttributeArrays, one per attribute index */
	TArray<TMeshAttributeArray<AttributeType, ElementIDType>> ArrayForIndices;

	/** The default value for an attribute of this name */
	AttributeType DefaultValue;

	/** Implementation-defined attribute name flags */
	EMeshAttributeFlags Flags;
};


/** Define aliases for element attributes */
template <typename AttributeType> using TVertexAttributeIndicesArray = TAttributeIndicesArray<AttributeType, FVertexID>;
template <typename AttributeType> using TVertexInstanceAttributeIndicesArray = TAttributeIndicesArray<AttributeType, FVertexInstanceID>;
template <typename AttributeType> using TEdgeAttributeIndicesArray = TAttributeIndicesArray<AttributeType, FEdgeID>;
template <typename AttributeType> using TPolygonAttributeIndicesArray = TAttributeIndicesArray<AttributeType, FPolygonID>;
template <typename AttributeType> using TPolygonGroupAttributeIndicesArray = TAttributeIndicesArray<AttributeType, FPolygonGroupID>;


/**
 * This maps an attribute name to a TAttributeIndicesArray, i.e. an array of MeshAttributeArrays, one per attribute index.
 */
template <typename T, typename U>
class TAttributesMap
{
public:
	using AttributeType = T;
	using ElementIDType = U;
	using AttributeIndicesArrayType = TAttributeIndicesArray<AttributeType, ElementIDType>;
	using MapType = TMap<FName, AttributeIndicesArrayType>;

	FORCEINLINE TAttributesMap()
		: NumElements( 0 )
	{}

	/** Register an attribute name */
	FORCEINLINE void RegisterAttribute( const FName AttributeName, const int32 NumberOfIndices, const AttributeType& Default, const EMeshAttributeFlags Flags )
	{
		if( !Map.Contains( AttributeName ) )
		{
			Map.Emplace( AttributeName, TAttributeIndicesArray<AttributeType, ElementIDType>( NumberOfIndices, Default, Flags, NumElements ) );
		}
	}

	/** Unregister an attribute name */
	FORCEINLINE void UnregisterAttribute( const FName AttributeName )
	{
		Map.Remove( AttributeName );
	}

	/** Determines whether an attribute exists with the given name */
	FORCEINLINE bool HasAttribute( const FName AttributeName ) const
	{
		return Map.Contains( AttributeName );
	}

	/** Get attribute array with the given name and index */
	FORCEINLINE TMeshAttributeArray<AttributeType, ElementIDType>& GetAttributes( const FName AttributeName, const int32 AttributeIndex = 0 )
	{
		// @todo mesh description: should this handle non-existent attribute names and indices gracefully?
		return Map.FindChecked( AttributeName ).GetArrayForIndex( AttributeIndex );
	}

	FORCEINLINE const TMeshAttributeArray<AttributeType, ElementIDType>& GetAttributes( const FName AttributeName, const int32 AttributeIndex = 0 ) const
	{
		// @todo mesh description: should this handle non-existent attribute names and indices gracefully?
		return Map.FindChecked( AttributeName ).GetArrayForIndex( AttributeIndex );
	}

	/** Get attribute indices array with the given name */
	FORCEINLINE TAttributeIndicesArray<AttributeType, ElementIDType>& GetAttributesSet( const FName AttributeName )
	{
		// @todo mesh description: should this handle non-existent attribute names gracefully?
		return Map.FindChecked( AttributeName );
	}

	FORCEINLINE const TAttributeIndicesArray<AttributeType, ElementIDType>& GetAttributesSet( const FName AttributeName ) const
	{
		// @todo mesh description: should this handle non-existent attribute names gracefully?
		return Map.FindChecked( AttributeName );
	}

	/** Returns the number of indices for the attribute with the given name */
	FORCEINLINE int32 GetAttributeIndexCount( const FName AttributeName ) const
	{
		// @todo mesh description: should this handle non-existent attribute names and indices gracefully?
		return Map.FindChecked( AttributeName ).GetNumIndices();
	}

	/** Sets the number of indices for the attribute with the given name */
	FORCEINLINE void SetAttributeIndexCount( const FName AttributeName, const int32 NumIndices )
	{
		Map.FindChecked( AttributeName ).SetNumIndices( NumIndices );
	}

	/** Returns an array of all the attribute names registered for this attribute type */
	template <typename Allocator>
	FORCEINLINE void GetAttributeNames( TArray<FName, Allocator>& OutAttributeNames ) const
	{
		Map.GetKeys( OutAttributeNames );
	}

	/** Gets a single attribute with the given ElementID, Name and Index */
	FORCEINLINE AttributeType GetAttribute( const ElementIDType ElementID, const FName AttributeName, const int32 AttributeIndex = 0 ) const
	{
		return Map.FindChecked( AttributeName ).GetArrayForIndex( AttributeIndex )[ ElementID ];
	}

	/** Sets a single attribute with the given ElementID, Name and Index to the given value */
	FORCEINLINE void SetAttribute( const ElementIDType ElementID, const FName AttributeName, const int32 AttributeIndex, const AttributeType& AttributeValue )
	{
		Map.FindChecked( AttributeName ).GetArrayForIndex( AttributeIndex )[ ElementID ] = AttributeValue;
	}

	/** Inserts a default-initialized value for all attributes of the given ID */
	void Insert( const ElementIDType ElementID )
	{
		NumElements = FMath::Max( NumElements, ElementID.GetValue() + 1 );
		for( auto& AttributeNameAndIndicesArray : Map )
		{
			AttributeNameAndIndicesArray.Value.Insert( ElementID );
			check( AttributeNameAndIndicesArray.Value.GetNumElements() == this->NumElements );
		}
	}

	/** Removes all attributes with the given ID */
	void Remove( const ElementIDType ElementID )
	{
		for( auto& AttributeNameAndIndicesArray : Map )
		{
			AttributeNameAndIndicesArray.Value.Remove( ElementID );
		}
	}

	/** Initializes all attributes to have the given number of elements with the default value */
	void Initialize( const int32 Count )
	{
		NumElements = Count;
		for( auto& AttributeNameAndIndicesArray : Map )
		{
			AttributeNameAndIndicesArray.Value.Initialize( Count );
		}
	}

	/** Returns the number of elements held by each attribute in this map */
	FORCEINLINE int32 GetNumElements() const
	{
		return NumElements;
	}

	/**
	 * Call the supplied function on each attribute.
	 * The prototype should be Func( const FName AttributeName, auto& AttributeIndicesArray );
	 */
	template <typename FuncType>
	void ForEachAttributeIndicesArray( const FuncType& Func )
	{
		for( auto& AttributeNameAndIndicesArray : Map )
		{
			Func( AttributeNameAndIndicesArray.Key, AttributeNameAndIndicesArray.Value );
		}
	}

	template <typename FuncType>
	void ForEachAttributeIndicesArray( const FuncType& Func ) const
	{
		for( const auto& AttributeNameAndIndicesArray : Map )
		{
			Func( AttributeNameAndIndicesArray.Key, AttributeNameAndIndicesArray.Value );
		}
	}

	/** Registers attributes copied from the specified attributes map */
	void RegisterAttributesFromAttributesSet( const TAttributesMap& SrcAttributesMap )
	{
		for( const auto& AttributeNameAndIndicesArray : SrcAttributesMap.Map )
		{
			const FName& AttributeName = AttributeNameAndIndicesArray.Key;
			const AttributeIndicesArrayType& AttributeIndicesArray = AttributeNameAndIndicesArray.Value;

			RegisterAttribute( AttributeName, AttributeIndicesArray.GetNumIndices(), AttributeIndicesArray.GetDefaultValue(), AttributeIndicesArray.GetFlags() );
		}
	}

	/** Applies the given remapping to the attributes set */
	void Remap( const TSparseArray<ElementIDType>& IndexRemap )
	{
		if( Map.Num() == 0 )
		{
			// If there are no attributes registered, determine the number of elements by finding the maximum
			// remapped element ID in the IndexRemap array
			NumElements = 0;
			for( const ElementIDType ElementID : IndexRemap )
			{
				NumElements = FMath::Max( NumElements, ElementID.GetValue() + 1 );
			}
		}
		else
		{
			// Otherwise perform the remap, and get the number of elements from the resulting attribute indices array.
			for( auto& AttributeNameAndIndicesArray : Map )
			{
				AttributeNameAndIndicesArray.Value.Remap( IndexRemap );
				NumElements = AttributeNameAndIndicesArray.Value.GetNumElements();
			}
		}
	}

private:
	friend FArchive& operator<<( FArchive& Ar, TAttributesMap& AttributesMap )
	{
		// First serialize the number of elements which each attribute should contain
		Ar << AttributesMap.NumElements;

		// Now serialize the attributes of this type.
		Ar << AttributesMap.Map;

		return Ar;
	}

	/** Number of elements for each attribute index */
	int32 NumElements;

	/** The actual container */
	MapType Map;
};


/**
 * Helper template which transforms a tuple of types into a tuple of TAttributesArrays of those types.
 *
 * We need to instance TAttributeArrays for each type in the AttributeTypes tuple.
 * Then we can access the appropriate array (as long as we know what its index is).
 *
 * This template, given ElementIDType and TTuple<A, B>, will generate:
 * TTuple<TAttributesArray<A, ElementIDType>, TAttributesArray<B, ElementIDType>>
 */
template <typename ElementIDType, typename Tuple>
struct TMakeAttributesSet;

template <typename ElementIDType, typename... TupleTypes>
struct TMakeAttributesSet<ElementIDType, TTuple<TupleTypes...>>
{
	using Type = TTuple<TAttributesMap<TupleTypes, ElementIDType>...>;
};


/**
 * Helper template which gets the tuple index of a given type from a given TTuple.
 *
 * Given Type = char, and Tuple = TTuple<int, float, char>,
 * TTupleIndex<Type, Tuple>::Value will be 2.
 */
template <typename Type, typename Tuple>
struct TTupleIndex;

template <typename Type, typename... Types>
struct TTupleIndex<Type, TTuple<Type, Types...>>
{
	static const uint32 Value = 0U;
};

template <typename Type, typename Head, typename... Tail>
struct TTupleIndex<Type, TTuple<Head, Tail...>>
{
	static const uint32 Value = 1U + TTupleIndex<Type, TTuple<Tail...>>::Value;
};


/** Helper template which splits a tuple into head and tail */
template <typename Tuple>
struct TSplitTuple;

template <typename TupleHead, typename... TupleTail>
struct TSplitTuple<TTuple<TupleHead, TupleTail...>>
{
	using Head = TupleHead;
	using Tail = TTuple<TupleTail...>;
};


/**
 * This is the container for all attributes of a particular mesh element.
 * It contains a TTuple of TAttributesMap, one per attribute type (FVector, float, bool, etc)
 */
template <typename ElementIDType>
class TAttributesSet
{
public:
	/**
	 * Register a new attribute name with the given type (must be a member of the AttributeTypes tuple).
	 *
	 * Example of use:
	 *
	 *		VertexInstanceAttributes().RegisterAttribute<FVector2D>( "UV", 8 );
	 *                        . . .
	 *		TVertexInstanceAttributeArray<FVector2D>& UV0 = VertexInstanceAttributes().GetAttributes<FVector2D>( "UV", 0 );
	 *		UV0[ VertexInstanceID ] = FVector2D( 1.0f, 1.0f );
	 */
	template <typename AttributeType>
	void RegisterAttribute( const FName AttributeName, const int32 NumberOfIndices = 1, const AttributeType& Default = AttributeType(), const EMeshAttributeFlags Flags = EMeshAttributeFlags::None )
	{
		Container.template Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().RegisterAttribute( AttributeName, NumberOfIndices, Default, Flags );
	}

	/** Unregister an attribute name with the given type */
	template <typename AttributeType>
	void UnregisterAttribute( const FName AttributeName )
	{
		Container.template Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().UnregisterAttribute( AttributeName );
	}

	/** Determines whether an attribute of the given type exists with the given name */
	template <typename AttributeType>
	bool HasAttribute( const FName AttributeName ) const
	{
		return Container.template Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().HasAttribute( AttributeName );
	}

	/**
	 * Get an attribute array with the given type, name and index.
	 *
	 * Example of use:
	 *
	 *		const TVertexAttributeArray<FVector>& VertexPositions = VertexAttributes().GetAttributes<FVector>( "Position" );
	 *		for( const FVertexID VertexID : GetVertices().GetElementIDs() )
	 *		{
	 *			const FVector Position = VertexPositions[ VertexID ];
	 *			DoSomethingWith( Position );
	 *		}
	 */
	template <typename AttributeType>
	TMeshAttributeArray<AttributeType, ElementIDType>& GetAttributes( const FName AttributeName, const int32 AttributeIndex = 0 )
	{
		// @todo mesh description: should this handle non-existent attribute names and indices gracefully?
		return Container.template Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().GetAttributes( AttributeName, AttributeIndex );
	}

	template <typename AttributeType>
	const TMeshAttributeArray<AttributeType, ElementIDType>& GetAttributes( const FName AttributeName, const int32 AttributeIndex = 0 ) const
	{
		// @todo mesh description: should this handle non-existent attribute names and indices gracefully?
		return Container.template Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().GetAttributes( AttributeName, AttributeIndex );
	}

	/**
	 * Get a set of attribute arrays with the given type and name.
	 *
	 * Example of use:
	 *
	 *		const TArray<TVertexInstanceAttributeArray<FVector2D>>& UVs = VertexInstanceAttributes().GetAttributesSet<FVector2D>( "UV" );
	 *		for( const FVertexInstanceID VertexInstanceID : GetVertexInstances().GetElementIDs() )
	 *		{
	 *			const FVector2D UV0 = UVs[ 0 ][ VertexInstanceID ];
	 *			const FVector2D UV1 = UVs[ 1 ][ VertexInstanceID ];
	 *			DoSomethingWith( UV0, UV1 );
	 *		}
	 */
	template <typename AttributeType>
	TAttributeIndicesArray<AttributeType, ElementIDType>& GetAttributesSet( const FName AttributeName )
	{
		// @todo mesh description: should this handle non-existent attribute names gracefully?
		return Container.template Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().GetAttributesSet( AttributeName );
	}

	template <typename AttributeType>
	const TAttributeIndicesArray<AttributeType, ElementIDType>& GetAttributesSet( const FName AttributeName ) const
	{
		// @todo mesh description: should this handle non-existent attribute names gracefully?
		return Container.template Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().GetAttributesSet( AttributeName );
	}

	/** Returns the number of indices for the attribute with the given name */
	template <typename AttributeType>
	int32 GetAttributeIndexCount( const FName AttributeName ) const
	{
		// @todo mesh description: should this handle non-existent attribute names and indices gracefully?
		return Container.template Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().GetAttributeIndexCount( AttributeName );
	}

	/** Sets the number of indices for the attribute with the given name */
	template <typename AttributeType>
	void SetAttributeIndexCount( const FName AttributeName, const int32 NumIndices )
	{
		Container.template Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().SetAttributeIndexCount( AttributeName, NumIndices );
	}

	/** Returns an array of all the attribute names registered for this attribute type */
	template <typename AttributeType, typename Allocator>
	void GetAttributeNames( TArray<FName, Allocator>& OutAttributeNames ) const
	{
		Container.template Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().GetAttributeNames( OutAttributeNames );
	}

	template <typename AttributeType>
	AttributeType GetAttribute( const ElementIDType ElementID, const FName AttributeName, const int32 AttributeIndex = 0 ) const
	{
		return Container.template Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().GetAttribute( ElementID, AttributeName, AttributeIndex );
	}

	template <typename AttributeType>
	void SetAttribute( const ElementIDType ElementID, const FName AttributeName, const int32 AttributeIndex, const AttributeType& AttributeValue )
	{
		Container.template Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().SetAttribute( ElementID, AttributeName, AttributeIndex, AttributeValue );
	}

	/** Inserts a default-initialized value for all attributes of the given ID */
	void Insert( const ElementIDType ElementID )
	{
		VisitTupleElements( [ ElementID ]( auto& AttributesMap ) { AttributesMap.Insert( ElementID ); }, Container );
	}

	/** Removes all attributes with the given ID */
	void Remove( const ElementIDType ElementID )
	{
		VisitTupleElements( [ ElementID ]( auto& AttributesMap ) { AttributesMap.Remove( ElementID ); }, Container );
	}

	/** Initializes the attribute set with the given number of elements, all at the default value */
	void Initialize( const int32 NumElements )
	{
		VisitTupleElements( [ NumElements ]( auto& AttributesMap ) { AttributesMap.Initialize( NumElements); }, Container );
	}

	/**
	 * Call the supplied function on each attribute.
	 * The prototype should be Func( const FName AttributeName, auto& AttributeIndicesArray );
	 */
	template <typename FuncType>
	void ForEachAttributeIndicesArray( const FuncType& Func )
	{
		VisitTupleElements( [ &Func ]( auto& AttributesMap ) { AttributesMap.ForEachAttributeIndicesArray( Func ); }, Container );
	}

	template <typename FuncType>
	void ForEachAttributeIndicesArray( const FuncType& Func ) const
	{
		VisitTupleElements( [ &Func ]( const auto& AttributesMap ) { AttributesMap.ForEachAttributeIndicesArray( Func ); }, Container );
	}

	/** Applies the given remapping to the attributes set */
	void Remap( const TSparseArray<ElementIDType>& IndexRemap )
	{
		VisitTupleElements( [ &IndexRemap ]( auto& AttributesMap ) { AttributesMap.Remap( IndexRemap ); }, Container );
	}

	/** Copies registered attributes from another TAttributesSet */
	void RegisterAttributesFromAttributesSet( const TAttributesSet& Other )
	{
		VisitTupleElements( [ this ]( auto& DestAttributesMap, const auto& SrcAttributesMap ) { DestAttributesMap.RegisterAttributesFromAttributesSet( SrcAttributesMap ); },
			Container, Other.Container );
	}

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, TAttributesSet& AttributesSet )
	{
		// Serialize the number of attribute types in the container tuple.
		// If loading, this may be different to the current number defined.
		const int32 NumAttributeTypes = TTupleArity<AttributeTypes>::Value;
		int32 SerializedAttributeTypes = NumAttributeTypes;
		Ar << SerializedAttributeTypes;
		// Cannot deserialize more attribute types than there are
		check( NumAttributeTypes >= SerializedAttributeTypes );

		// Serialize the tuple of attribute maps by hand, so we can deserialize correctly when the archive contains fewer tuple elements than the current code
		// NOTE: This relies on the assumption that VisitTupleElements will always visit elements in ascending order.
		VisitTupleElements( [ &Ar, SerializedAttributeTypes, TypeIndex = 0, NumElements = 0 ]( auto& AttributesMap ) mutable
			{
				if( TypeIndex < SerializedAttributeTypes )
				{
					// Serialize attributes map, and keep note of the number of elements present.
					// This should be the same every iteration.
					Ar << AttributesMap;
					check( TypeIndex == 0 || NumElements == AttributesMap.GetNumElements() );
					NumElements = AttributesMap.GetNumElements();
				}
				else
				{
					// If we have run out of data to deserialize, initialize this attributes map so that it has the same number of elements
					// as the other maps.
					check( Ar.IsLoading() );
					AttributesMap.Initialize( NumElements );
				}
				TypeIndex++;
			},
			AttributesSet.Container );

		return Ar;
	}

private:
	/**
	 * Define type for the entire attribute container.
	 * We can have attributes of multiple types, each with a name and an arbitrary number of indices,
	 * whose elements are indexed by an ElementIDType.
	 *
	 * This implies the below data structure:
	 * A TTuple (one per attribute type) of
	 * TMap keyed on the attribute name,
	 * yielding a TArray indexed by attribute index,
	 * yielding a TMeshAttributeArray indexed by an Element ID,
	 * yielding an item of type AttributeType.
	 *
	 * This looks complicated, but actually makes attribute lookup easy when we are interested in a particular attribute for many element IDs.
	 * By caching the TMeshAttributeArray arrived at by the attribute name and index, we have O(1) access to that attribute for all elements.
	 */
	using ContainerType = typename TMakeAttributesSet<ElementIDType, AttributeTypes>::Type;
	ContainerType Container;
};
