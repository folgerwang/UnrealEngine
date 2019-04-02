// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MeshAttributeArray.h"
#include "UObject/EditorObjectVersion.h"


FArchive& operator<<( FArchive& Ar, FAttributesSetEntry& Entry )
{
	if( Ar.IsLoading() )
	{
		uint32 AttributeType;
		Ar << AttributeType;
		Entry.CreateArrayOfType( AttributeType );
		Entry.Ptr->Serialize( Ar );
	}
	else
	{
		check( Entry.Ptr.IsValid() );
		uint32 AttributeType = Entry.Ptr->GetType();
		Ar << AttributeType;
		Entry.Ptr->Serialize( Ar );
	}

	return Ar;
}


template <typename T>
void SerializeLegacy( FArchive& Ar, FAttributesSetBase& AttributesSet )
{
	Ar << AttributesSet.NumElements;

	TMap<FName, TMeshAttributeArraySet<T>> OldContainer;
	Ar << OldContainer;

	for( const auto& MapEntry : OldContainer )
	{
		AttributesSet.RegisterAttribute<T>( MapEntry.Key, 0 );
		static_cast<TMeshAttributeArraySet<T>&>( *AttributesSet.Map.FindChecked( MapEntry.Key ).Get() ) = MapEntry.Value;
	}
}


FArchive& operator<<( FArchive& Ar, FAttributesSetBase& AttributesSet )
{
	Ar.UsingCustomVersion( FEditorObjectVersion::GUID );

	if( Ar.IsLoading() && Ar.CustomVer( FEditorObjectVersion::GUID ) < FEditorObjectVersion::MeshDescriptionNewAttributeFormat )
	{
		// Legacy serialization format
		int32 NumAttributeTypes;
		Ar << NumAttributeTypes;
		check( NumAttributeTypes == 7 );

		AttributesSet.Map.Empty();
		SerializeLegacy<FVector4>( Ar, AttributesSet );
		SerializeLegacy<FVector>( Ar, AttributesSet );
		SerializeLegacy<FVector2D>( Ar, AttributesSet );
		SerializeLegacy<float>( Ar, AttributesSet );
		SerializeLegacy<int>( Ar, AttributesSet );
		SerializeLegacy<bool>( Ar, AttributesSet );
		SerializeLegacy<FName>( Ar, AttributesSet );

		return Ar;
	}

	Ar << AttributesSet.NumElements;

	// If saving, store transient attribute arrays and remove them temporarily from the map
	TArray<TTuple<FName, FAttributesSetEntry>> TransientArrays;
	if( Ar.IsSaving() && !Ar.IsTransacting() )
	{
		for( TMap<FName, FAttributesSetEntry>::TIterator It( AttributesSet.Map ); It; ++It )
		{
			if( EnumHasAnyFlags( It.Value()->GetFlags(), EMeshAttributeFlags::Transient ) )
			{
				TransientArrays.Emplace( MakeTuple( It.Key(), MoveTemp( It.Value() ) ) );
				It.RemoveCurrent();
			}
		}
	}

	// Serialize map
	Ar << AttributesSet.Map;

	// Restore transient attribute arrays if saving
	if( Ar.IsSaving() && !Ar.IsTransacting() )
	{
		for( auto& TransientArray : TransientArrays )
		{
			AttributesSet.Map.Emplace( TransientArray.Get<0>(), MoveTemp( TransientArray.Get<1>() ) );
		}
	}

	return Ar;
}


void FAttributesSetBase::Remap( const TSparseArray<int32>& IndexRemap )
{
	// Determine the number of elements by finding the maximum remapped element index in the IndexRemap array.
	NumElements = 0;
	for( const int32 ElementIndex : IndexRemap )
	{
		NumElements = FMath::Max( NumElements, ElementIndex + 1 );
	}

	for( auto& MapEntry : Map )
	{
		MapEntry.Value->Remap( IndexRemap );
		check( MapEntry.Value->GetNumElements() == NumElements );
	}
}
