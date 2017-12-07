// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshTypes.h"
#include "MeshElementRemappings.h"

/**
 * List of attribute types which are supported.
 * We do this so we can automatically generate the attribute containers and their associated accessors with
 * some template magic. Adding a new attribute type requires no extra code elsewhere in the class.
 */
using AttributeTypes = TTuple
<
	FVector4,
	FVector,
	FVector2D,
	float,
	int,
	bool,
	FName,
	UObject*
>;


/**
 * This defines the container used to hold mesh element attributes of a particular name and index.
 * It is a simple TArray, so that all attributes are packed contiguously for each element ID.
 */
template <typename ElementType>
class TMeshAttributeArrayBase
{
public:

	/**
	 * Custom serialization for TMeshAttributeArrayBase.
	 */
	friend FArchive& operator<<( FArchive& Ar, TMeshAttributeArrayBase& Array )
	{
		Ar << Array.Container;
		return Ar;
	}

protected:

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

	/** The actual container, represented by a regular array */
	TArray<ElementType> Container;
};


template <typename AttributeType, typename ElementIDType>
class TAttributeIndicesArray;

/**
 * We prefer to access elements of the container via strongly-typed IDs.
 * This derived class imposes this type safety.
 */
template <typename ElementType, typename ElementIDType>
class TMeshAttributeArray : public TMeshAttributeArrayBase<ElementType>
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

	for( TSparseArray<ElementIDType>::TConstIterator It( IndexRemap ); It; ++It )
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
template <typename AttributeType, typename ElementIDType>
class TAttributeIndicesArray
{
public:

	using Type = AttributeType;

	/** Default constructor - required so that it builds correctly */
	TAttributeIndicesArray() = default;

	/** Constructor */
	TAttributeIndicesArray( const int32 NumberOfIndices, const AttributeType& InDefaultValue, const EMeshAttributeFlags InFlags )
		: DefaultValue( InDefaultValue ),
		  Flags( InFlags )
	{
		ArrayForIndices.SetNum( NumberOfIndices );
	}

	/** Insert the element at the given index */
	FORCEINLINE void Insert( const ElementIDType ElementID )
	{
		for( TMeshAttributeArray<AttributeType, ElementIDType>& ArrayForIndex : ArrayForIndices )
		{
			ArrayForIndex.Insert( ElementID, DefaultValue );
		}
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

	/** Return number of indices this attribute has */
	FORCEINLINE int32 GetNumIndices() const { return ArrayForIndices.Num(); }

	/** Sets number of indices this attribute has */
	void SetNumIndices( const int32 NumIndices )
	{
		const int32 OriginalNumIndices = ArrayForIndices.Num();
		ArrayForIndices.SetNum( NumIndices );

		// If there is already at least one attribute index, and it is non-empty, ensure that newly added indices
		// are initialized to the same size with the default value (they must all have equal size).
		if( OriginalNumIndices > 0 && ArrayForIndices[ 0 ].Num() > 0 )
		{
			for( int32 Index = OriginalNumIndices; Index < NumIndices; ++Index )
			{
				ArrayForIndices[ Index ].Insert( ElementIDType( ArrayForIndices[ 0 ].Num() - 1 ), DefaultValue );
			}
		}
	}

	/** Remaps all attribute indices according to the passed mapping */
	void Remap( const TSparseArray<ElementIDType>& IndexRemap )
	{
		for( TMeshAttributeArray<AttributeType, ElementIDType>& ArrayForIndex : ArrayForIndices )
		{
			ArrayForIndex.Remap( IndexRemap, DefaultValue );
		}
	}

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, TAttributeIndicesArray& AttributesArray )
	{
		Ar << AttributesArray.ArrayForIndices;
		Ar << AttributesArray.DefaultValue;
		Ar << AttributesArray.Flags;
		return Ar;
	}

private:
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
 * This alias maps an attribute name to a TAttributeIndicesArray, i.e. an array of MeshAttributeArrays, one per attribute index.
 */
template <typename AttributeType, typename ElementIDType>
using TAttributesMap = TMap<FName, TAttributeIndicesArray<AttributeType, ElementIDType>>;


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
		Container.Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().Emplace(
			AttributeName,
			TAttributeIndicesArray<AttributeType, ElementIDType>( NumberOfIndices, Default, Flags )
		);
	}

	/** Unregister an attribute name with the given type */
	template <typename AttributeType>
	void UnregisterAttribute( const FName AttributeName )
	{
		Container.Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().Remove( AttributeName );
	}

	/** Determines whether an attribute of the given type exists with the given name */
	template <typename AttributeType>
	bool HasAttribute( const FName AttributeName ) const
	{
		return Container.Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().Contains( AttributeName );
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
		return Container.Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName ).GetArrayForIndex( AttributeIndex );
	}

	template <typename AttributeType>
	const TMeshAttributeArray<AttributeType, ElementIDType>& GetAttributes( const FName AttributeName, const int32 AttributeIndex = 0 ) const
	{
		// @todo mesh description: should this handle non-existent attribute names and indices gracefully?
		return Container.Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName ).GetArrayForIndex( AttributeIndex );
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
		return Container.Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName );
	}

	template <typename AttributeType>
	const TAttributeIndicesArray<AttributeType, ElementIDType>& GetAttributesSet( const FName AttributeName ) const
	{
		// @todo mesh description: should this handle non-existent attribute names gracefully?
		return Container.Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName );
	}

	/** Returns the number of indices for the attribute with the given name */
	template <typename AttributeType>
	int32 GetAttributeIndexCount( const FName AttributeName ) const
	{
		// @todo mesh description: should this handle non-existent attribute names and indices gracefully?
		return Container.Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName ).GetNumIndices();
	}

	/** Sets the number of indices for the attribute with the given name */
	template <typename AttributeType>
	void SetAttributeIndexCount( const FName AttributeName, const int32 NumIndices )
	{
		Container.Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName ).SetNumIndices( NumIndices );
	}

	/** Returns an array of all the attribute names registered for this attribute type */
	template <typename AttributeType, typename Allocator>
	void GetAttributeNames( TArray<FName, Allocator>& OutAttributeNames ) const
	{
		Container.Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().GetKeys( OutAttributeNames );
	}

	template <typename AttributeType>
	AttributeType GetAttribute( const ElementIDType ElementID, const FName AttributeName, const int32 AttributeIndex = 0 ) const
	{
		return Container.Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName ).GetArrayForIndex( AttributeIndex )[ ElementID ];
	}

	template <typename AttributeType>
	void SetAttribute( const ElementIDType ElementID, const FName AttributeName, const int32 AttributeIndex, const AttributeType& AttributeValue )
	{
		Container.Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName ).GetArrayForIndex( AttributeIndex )[ ElementID ] = AttributeValue;
	}

	/** Inserts a default-initialized value for all attributes of the given ID */
	void Insert( const ElementIDType ElementID )
	{
		auto AddElements = [ ElementID ]( auto& AttributesMap )
		{
			for( auto& AttributeNameAndIndicesArray : AttributesMap )
			{
				AttributeNameAndIndicesArray.Value.Insert( ElementID );
			}
		};

		VisitTupleElements( Container, AddElements );
	}

	/** Removes all attributes with the given ID */
	void Remove( const ElementIDType ElementID )
	{
		auto RemoveElements = [ ElementID ]( auto& AttributesMap )
		{
			for( auto& AttributeNameAndIndicesArray : AttributesMap )
			{
				AttributeNameAndIndicesArray.Value.Remove( ElementID );
			}
		};

		VisitTupleElements( Container, RemoveElements );
	}

	/**
	 * Call the supplied function on each attribute.
	 * The prototype should be Func( const FName AttributeName, auto& AttributeIndicesArray );
	 */
	template <typename FuncType>
	void ForEachAttributeIndicesArray( const FuncType& Func )
	{
		auto ForEach = [ &Func ]( auto& AttributesMap )
		{
			for( auto& AttributeNameAndIndicesArray : AttributesMap )
			{
				Func( AttributeNameAndIndicesArray.Key, AttributeNameAndIndicesArray.Value );
			}
		};

		VisitTupleElements( Container, ForEach );
	}

	template <typename FuncType>
	void ForEachAttributeIndicesArray( const FuncType& Func ) const
	{
		auto ForEach = [ &Func ]( const auto& AttributesMap )
		{
			for( const auto& AttributeNameAndIndicesArray : AttributesMap )
			{
				Func( AttributeNameAndIndicesArray.Key, AttributeNameAndIndicesArray.Value );
			}
		};

		VisitTupleElements( Container, ForEach );
	}

	/** Applies the given remapping to the attributes set */
	void Remap( const TSparseArray<ElementIDType>& IndexRemap )
	{
		auto RemapElements = [ &IndexRemap ]( auto& AttributesMap )
		{
			for( auto& AttributeNameAndIndicesArray : AttributesMap )
			{
				AttributeNameAndIndicesArray.Value.Remap( IndexRemap );
			}
		};

		VisitTupleElements( Container, RemapElements );
	}

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, TAttributesSet& AttributesSet )
	{
		Ar << AttributesSet.Container;
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
	typename TMakeAttributesSet<ElementIDType, AttributeTypes>::Type Container;
};
