// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditableMesh.h"
#include "EditableMeshChanges.h"
#include "EditableMeshCustomVersion.h"
#include "EditableMeshAdapter.h"
#include "EditableMeshOctree.h"
#include "EngineDefines.h"	// For HALF_WORLD_MAX
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/ScopedTimers.h"	// For FAutoScopedDurationTimer
#include "GeomTools.h"
#include "mikktspace.h"	// For tangent computations
#include "MeshAttributes.h"

// =========================================================
// OpenSubdiv support
// =========================================================

#if EDITABLE_MESH_USE_OPENSUBDIV

#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable:4191)		// Disable warning C4191: 'type cast' : unsafe conversion
#endif

#ifndef M_PI
#define M_PI_NOT_DEFINED
#define M_PI PI		// OpenSubdiv is expecting M_PI to be defined already
#endif

#include "far/topologyRefinerFactory.h"
#include "far/topologyDescriptor.h"
#include "far/topologyRefiner.h"
#include "far/primvarRefiner.h"

#ifdef M_PI_NOT_DEFINED
#undef M_PI
#endif

#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

#endif

// =========================================================

DEFINE_LOG_CATEGORY( LogEditableMesh );

#ifdef EDITABLE_MESH_DEBUG_LOG
static int32 GIndent = 0;
static FString IncreaseIndent() { GIndent += 2; return FString::ChrN( GIndent - 2, ' ' ); }
static FString DecreaseIndent() { GIndent -= 2; return FString::ChrN( GIndent, ' ' ); }

#define EM_ENTER(Text, ...) UE_LOG( LogEditableMesh, Verbose, TEXT( "%s" ) Text, *IncreaseIndent(), ##__VA_ARGS__ )
#define EM_EXIT(Text, ...) UE_LOG( LogEditableMesh, Verbose, TEXT( "%s" ) Text, *DecreaseIndent(), ##__VA_ARGS__ )
#else
#define EM_ENTER(Text, ...)
#define EM_EXIT(Text, ...)
#endif

// =========================================================


namespace EditableMesh
{
	static FAutoConsoleVariable InterpolatePositionsToLimit( TEXT( "EditableMesh.InterpolatePositionsToLimit" ), 1, TEXT( "Whether to interpolate vertex positions for subdivision meshes all the way to their limit surface position.  Otherwise, we stop at the most refined mesh position." ) );
	static FAutoConsoleVariable InterpolateFVarsToLimit( TEXT( "EditableMesh.InterpolateFVarsToLimit" ), 1, TEXT( "Whether to interpolate face-varying vertex data for subdivision meshes all the way to their limit surface position.  Otherwise, we stop at the most refined mesh." ) );

	static FAutoConsoleVariable OctreeIncrementalUpdateLimit( TEXT( "EditableMesh.OctreeIncrementalUpdateLimit" ), 0.4f, TEXT( "If more than this scalar percentage of polygons have changed, we'll rebuild the octree from scratch instead of incrementally updating it." ) );
	static FAutoConsoleVariable UseBoundlessOctree( TEXT( "EditableMesh.UseBoundlessOctree" ), 1, TEXT( "If enabled, the octree for editable meshes will have a huge bounding box.  Otherwise, we'll compute a tightly wrapped bounds.  However, the bounds will not be able to grow beyond it's original size." ) );
}

//
// =========================================================
//

namespace MeshAttribute
{
	const FName PolygonGroup::MaterialAssetName( "MaterialAssetName" );
}


/** Traits class which specifies whether Lerp is defined for a given type */
template <typename T> struct TIsAttributeTypeLerpable { static const bool Value = false; };
template <> struct TIsAttributeTypeLerpable<FVector4> { static const bool Value = true; };
template <> struct TIsAttributeTypeLerpable<FVector> { static const bool Value = true; };
template <> struct TIsAttributeTypeLerpable<FVector2D> { static const bool Value = true; };
template <> struct TIsAttributeTypeLerpable<float> { static const bool Value = true; };
template <> struct TIsAttributeTypeLerpable<int> { static const bool Value = true; };


template <typename T, typename ElementIDType>
static typename TEnableIf<TIsAttributeTypeLerpable<T>::Value>::Type InterpAttributeOfType( FMeshElementAttributeList& AttributeList, const FName AttributeName, TMeshAttributesConstRef<ElementIDType, T> AttributeArrayRef, const ElementIDType ID0, const ElementIDType ID1, const float Alpha )
{
	for( int32 Index = 0; Index < AttributeArrayRef.GetNumIndices(); ++Index )
	{
		const T& Value0 = AttributeArrayRef.Get( ID0, Index );
		const T& Value1 = AttributeArrayRef.Get( ID1, Index );

		AttributeList.Attributes.Emplace( AttributeName, Index, 
			FMeshElementAttributeValue( EnumHasAllFlags( AttributeArrayRef.GetFlags(), EMeshAttributeFlags::Lerpable ) ?
				FMath::Lerp( Value0, Value1, Alpha ) :
				( Alpha <= 0.5f ) ? Value0 : Value1 )
			);
	}
}

template <typename T, typename ElementIDType>
static typename TEnableIf<!TIsAttributeTypeLerpable<T>::Value>::Type InterpAttributeOfType( FMeshElementAttributeList& AttributeList, const FName AttributeName, TMeshAttributesConstRef<ElementIDType, T> AttributeArrayRef, const ElementIDType ID0, const ElementIDType ID1, const float Alpha )
{
	// Non-Lerpable attribute type: just choose whichever value alpha is closest to
	for( int32 Index = 0; Index < AttributeArrayRef.GetNumIndices(); ++Index )
	{
		const T& Value0 = AttributeArrayRef.Get( ID0, Index );
		const T& Value1 = AttributeArrayRef.Get( ID1, Index );

		AttributeList.Attributes.Emplace( AttributeName, Index, FMeshElementAttributeValue( ( Alpha <= 0.5f ) ? Value0 : Value1 ) );
	}
}


/**
 * Automatically set all attributes as the Lerp between the two attribute indices specified (if the attribute is marked as Lerpable),
 * otherwise to the closest value.
 */
template <typename ElementIDType>
static void InterpAllAttributes( FMeshElementAttributeList& AttributeList, const TAttributesSet<ElementIDType>& AttributesSet, const ElementIDType ID0, const ElementIDType ID1, const float Alpha )
{
	AttributesSet.ForEach(
		[ &AttributeList, ID0, ID1, Alpha ]( const FName AttributeName, auto AttributeArrayRef )
		{
			InterpAttributeOfType( AttributeList, AttributeName, AttributeArrayRef, ID0, ID1, Alpha );
		}
	);
}


/**
 * Automatically set all attributes which match the predicate as the Lerp between the two attribute indices specified (if the attribute is marked as Lerpable),
 * otherwise to the closest value.
 */
template <typename ElementIDType, typename Predicate>
static void InterpAttributesByPredicate( FMeshElementAttributeList& AttributeList, const TAttributesSet<ElementIDType>& AttributesSet, const ElementIDType ID0, const ElementIDType ID1, const float Alpha, const Predicate& Pred )
{
	AttributesSet.ForEach(
		[ &AttributeList, ID0, ID1, Alpha, &Pred ]( const FName AttributeName, auto AttributeArrayRef )
		{
			if( Pred( AttributeName, AttributeArrayRef ) )
			{
				InterpAttributeOfType( AttributeList, AttributeName, AttributeArrayRef, ID0, ID1, Alpha );
			}
		}
	);
}


template <typename T, typename ElementIDType>
static typename TEnableIf<TIsAttributeTypeLerpable<T>::Value>::Type InterpAttributeOfType( FMeshElementAttributeList& AttributeList, const FName AttributeName, TMeshAttributesConstRef<ElementIDType, T> AttributeArrayRef, const ElementIDType ID0, const ElementIDType ID1, const ElementIDType ID2, const FVector Weights )
{
	if( EnumHasAllFlags( AttributeArrayRef.GetFlags(), EMeshAttributeFlags::Lerpable ) )
	{
		// Get result by interpolating between the three values according to the barycentric weight
		for( int32 Index = 0; Index < AttributeArrayRef.GetNumIndices(); ++Index )
		{
			const T& Value0 = AttributeArrayRef.Get( ID0, Index );
			const T& Value1 = AttributeArrayRef.Get( ID1, Index );
			const T& Value2 = AttributeArrayRef.Get( ID2, Index );

			AttributeList.Attributes.Emplace( AttributeName, Index, FMeshElementAttributeValue( Value0 * Weights.X + Value1 * Weights.Y + Value2 * Weights.Z ) );
		}
	}
	else
	{
		// Non-Lerpable attribute type: just choose whichever value the barycentric weight is closest to
		const ElementIDType ClosestID = ( Weights.X > Weights.Y ) ? ( ( Weights.X > Weights.Z ) ? ID0 : ID2 ) : ( ( Weights.Y > Weights.Z ) ? ID1 : ID2 );

		for( int32 Index = 0; Index < AttributeArrayRef.GetNumIndices(); ++Index )
		{
			AttributeList.Attributes.Emplace( AttributeName, Index, FMeshElementAttributeValue( AttributeArrayRef.Get( ClosestID, Index ) ) );
		}
	}
}

template <typename T, typename ElementIDType>
static typename TEnableIf<!TIsAttributeTypeLerpable<T>::Value>::Type InterpAttributeOfType( FMeshElementAttributeList& AttributeList, const FName AttributeName, TMeshAttributesConstRef<ElementIDType, T> AttributeArrayRef, const ElementIDType ID0, const ElementIDType ID1, const ElementIDType ID2, const FVector Weights )
{
	// Non-Lerpable attribute type: just choose whichever value the barycentric weight is closest to
	const ElementIDType ClosestID = ( Weights.X > Weights.Y ) ? ( ( Weights.X > Weights.Z ) ? ID0 : ID2 ) : ( ( Weights.Y > Weights.Z ) ? ID1 : ID2 );

	for( int32 Index = 0; Index < AttributeArrayRef.GetNumIndices(); ++Index )
	{
		AttributeList.Attributes.Emplace( AttributeName, Index, FMeshElementAttributeValue( AttributeArrayRef.Get( ClosestID, Index ) ) );
	}
}


/**
 * Automatically set all attributes according to the barycentric weights of the three indices specified (if the attribute is marked as Lerpable),
 * otherwise to the closest value.
 */
template <typename ElementIDType>
static void InterpAllAttributes( FMeshElementAttributeList& AttributeList, const TAttributesSet<ElementIDType>& AttributesSet, const ElementIDType ID0, const ElementIDType ID1, const ElementIDType ID2, const FVector Weights )
{
	AttributesSet.ForEach(
		[ &AttributeList, ID0, ID1, ID2, Weights ]( const FName AttributeName, auto AttributeArrayRef )
		{
			InterpAttributeOfType( AttributeList, AttributeName, AttributeArrayRef, ID0, ID1, ID2, Weights );
		}
	);
}


/**
 * Automatically set all attributes which match the predicate according to the barycentric weights of the three indices specified (if the attribute is marked as Lerpable),
 * otherwise to the closest value.
 */
template <typename ElementIDType, typename Predicate>
static void InterpAttributesByPredicate( FMeshElementAttributeList& AttributeList, const TAttributesSet<ElementIDType>& AttributesSet, const ElementIDType ID0, const ElementIDType ID1, const ElementIDType ID2, const FVector Weights, const Predicate& Pred )
{
	AttributesSet.ForEach(
		[ &AttributeList, ID0, ID1, ID2, Weights, &Pred ]( const FName AttributeName, auto AttributeArrayRef )
		{
			if( Pred( AttributeName, AttributeArrayRef ) )
			{
				InterpAttributeOfType( AttributeList, AttributeName, AttributeArrayRef, ID0, ID1, ID2, Weights );
			}
		}
	);
}


template <typename T>
static bool AreAttributeValuesNearlyEqual( const T& A, const T& B ) { return A == B; }

static bool AreAttributeValuesNearlyEqual( const FVector& A, const FVector& B ) { return A.Equals( B ); }
static bool AreAttributeValuesNearlyEqual( const FVector4& A, const FVector4& B ) { return A.Equals( B ); }
static bool AreAttributeValuesNearlyEqual( const FVector2D& A, const FVector2D& B ) { return A.Equals( B ); }
static bool AreAttributeValuesNearlyEqual( const float& A, const float& B ) { return FMath::IsNearlyEqual( A, B ); }

/** Compares two elements of all attribute arrays which match the given predicate, and returns whether they are all equal or not */
template <typename ElementIDType, typename Predicate>
static bool CompareAttributesByPredicate( const TAttributesSet<ElementIDType>& AttributesSet, const ElementIDType ID0, const ElementIDType ID1, const Predicate& Pred )
{
	bool bResult = true;
	AttributesSet.ForEach(
		[ ID0, ID1, &Pred, &bResult ]( const FName AttributeName, auto AttributeArrayRef )
		{
			if( Pred( AttributeName, AttributeArrayRef ) )
			{
				for( int32 Index = 0; Index < AttributeArrayRef.GetNumIndices(); ++Index )
				{
					if( !AreAttributeValuesNearlyEqual( AttributeArrayRef.Get( ID0, Index ), AttributeArrayRef.Get( ID1, Index ) ) )
					{
						bResult = false;
						return;
					}
				}
			}
			else
			{
				bResult = false;
			}
		}
	);

	return bResult;
}


/** Copies all element attributes with the given ID into a different AttributesSet */
template <typename ElementIDType>
static void CopyAllAttributes( TAttributesSet<ElementIDType>& DestAttributesSet, const TAttributesSet<ElementIDType>& SrcAttributesSet, const ElementIDType ElementID )
{
	SrcAttributesSet.ForEachAttributeIndicesArray(
		[ &DestAttributesSet, ElementID ]( const FName AttributeName, auto AttributeArrayRef )
		{
			for( int32 Index = 0; Index < AttributeArrayRef.GetNumIndices(); ++Index )
			{
				DestAttributesSet.SetAttribute( ElementID, AttributeName, Index, AttributeArrayRef.Get( ElementID, Index ) );
			}
		}
	);
}


/** Copies all element attributes with the given ID into an FMeshElementAttributeList */
template <typename ElementIDType>
static void BackupAllAttributes( FMeshElementAttributeList& AttributeList, const TAttributesSet<ElementIDType>& AttributesSet, const ElementIDType ElementID )
{
	AttributesSet.ForEach(
		[ &AttributeList, ElementID ]( const FName AttributeName, auto AttributeArrayRef )
		{
			for( int32 Index = 0; Index < AttributeArrayRef.GetNumIndices(); ++Index )
			{
				const auto& Value = AttributeArrayRef.Get( ElementID, Index );
				AttributeList.Attributes.Emplace( AttributeName, Index, FMeshElementAttributeValue( Value ) );
			}
		}
	);
}


/** Copies all element attributes which match the given predicate with the given ID into an FMeshElementAttributeList */
template <typename ElementIDType, typename Predicate>
static void BackupAttributesByPredicate( FMeshElementAttributeList& AttributeList, const TAttributesSet<ElementIDType>& AttributesSet, const ElementIDType ElementID, const Predicate& Pred )
{
	AttributesSet.ForEach(
		[ &AttributeList, ElementID, &Pred ]( const FName AttributeName, auto AttributeArrayRef )
		{
			if( Pred( AttributeName, AttributeArrayRef ) )
			{
				for( int32 Index = 0; Index < AttributeArrayRef.GetNumIndices(); ++Index )
				{
					const auto& Value = AttributeArrayRef.Get( ElementID, Index );
					AttributeList.Attributes.Emplace( AttributeName, Index, FMeshElementAttributeValue( Value ) );
				}
			}
		}
	);
}


/** Applies an FMeshElementAttributeData to an element attribute with the given ID */
template <typename ElementIDType>
static void ApplyAttribute( TAttributesSet<ElementIDType>& AttributesSet, const FMeshElementAttributeData& AttributeData, const ElementIDType ElementID )
{
	AttributeData.AttributeValue.Visit(
		[ &AttributesSet, &AttributeData, ElementID ]( const auto& Value )
		{
			AttributesSet.SetAttribute( ElementID, AttributeData.AttributeName, AttributeData.AttributeIndex, Value );
		}
	);
}


/** Applies all attributes in an FMeshElementAttributeList to an element attribute with the given ID */
template <typename ElementIDType>
static void ApplyAttributes( TAttributesSet<ElementIDType>& AttributesSet, const FMeshElementAttributeList& AttributeList, const ElementIDType ElementID )
{
	for( const FMeshElementAttributeData& AttributeData : AttributeList.Attributes )
	{
		ApplyAttribute( AttributesSet, AttributeData, ElementID );
	}
}


/** Given a FMeshElementAttributeList of attributes to be set, first backup the current values of those attributes into a new list, and then apply the new values */
template <typename ElementIDType>
static void BackupAttributesInList( FMeshElementAttributeList& BackupList, const FMeshElementAttributeList& SourceList, const TAttributesSet<ElementIDType>& AttributesSet, const ElementIDType ElementID )
{
	BackupList.Attributes.Reserve( SourceList.Attributes.Num() );

	for( const FMeshElementAttributeData& AttributeData : SourceList.Attributes )
	{
		AttributeData.AttributeValue.Visit(
			[ &BackupList, &AttributesSet, &AttributeData, ElementID ]( const auto& Value )
			{
				const FName& AttributeName = AttributeData.AttributeName;
				const int32 AttributeIndex = AttributeData.AttributeIndex;

				// Put current value into BackupList
				using AttributeType = typename TDecay<decltype( Value )>::Type;
				const AttributeType OldValue = AttributesSet.template GetAttribute<AttributeType>( ElementID, AttributeName, AttributeIndex );
				BackupList.Attributes.Emplace( AttributeName, AttributeIndex, FMeshElementAttributeValue( OldValue ) );
			}
		);
	}
}


UEditableMesh::UEditableMesh()
	: MeshDescription( &OwnedMeshDescription ),
	  bAllowUndo( false ),
	  bAllowCompact( false ),
	  PrimaryAdapter(nullptr),
	  PendingCompactCounter( 0 ),
	  bAllowSpatialDatabase( false )
{
}


void UEditableMesh::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar.UsingCustomVersion( FEditableMeshCustomVersion::GUID );

	Ar << OwnedMeshDescription;

	//Ar << MeshDescription;

	// If the array was serialized containing any editor-only transient adapters, they will appear here as null, so remove them.
	if( Ar.IsLoading() )
	{
		Adapters.RemoveAll( []( UEditableMeshAdapter* Adapter ) { return Adapter == nullptr; } );
	}
}


void UEditableMesh::PostLoad()
{
	Super::PostLoad();

#if EDITABLE_MESH_USE_OPENSUBDIV
	RefreshOpenSubdiv();
#endif
	RebuildOctree();
	RebuildRenderMesh();
}


void UEditableMesh::SetMeshDescription( FMeshDescription* InMeshDescription )
{
	MeshDescription = InMeshDescription;
	OwnedMeshDescription.Empty();
}


static void InvertRemapTable( TSparseArray<int32>& InvertedRemapTable, const TSparseArray<int32>& RemapTable )
{
	InvertedRemapTable.Empty( RemapTable.Num() );

	for( typename TSparseArray<int32>::TConstIterator It( RemapTable ); It; ++It )
	{
		InvertedRemapTable.Insert( *It, It.GetIndex() );
	}
}


class FCompactChange : public FChange
{
public:

	/** Constructor */
	FCompactChange()
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override
	{
		UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
		verify( !EditableMesh->AnyChangesToUndo() );
		EditableMesh->Compact();
		return EditableMesh->MakeUndo();
	}

	virtual FString ToString() const override
	{
		return FString( TEXT( "Compact" ) );
	}
};


struct FUncompactChangeInput
{
	/** A set of remap tables, specifying how the elements should have their indices remapped */
	FElementIDRemappings ElementIDRemappings;
};


class FUncompactChange : public FChange
{
public:

	/** Constructor */
	FUncompactChange( const FUncompactChangeInput& InitInput )
		: Input( InitInput )
	{
	}

	FUncompactChange( FUncompactChangeInput&& InitInput )
		: Input( MoveTemp( InitInput ) )
	{
	}

	// Parent class overrides
	virtual TUniquePtr<FChange> Execute( UObject* Object ) override
	{
		UEditableMesh* EditableMesh = CastChecked<UEditableMesh>( Object );
		verify( !EditableMesh->AnyChangesToUndo() );
		EditableMesh->Uncompact( Input.ElementIDRemappings );
		return EditableMesh->MakeUndo();
	}

	virtual FString ToString() const override
	{
		return FString( TEXT( "Uncompact" ) );
	}

private:

	/** The data we need to make this change */
	FUncompactChangeInput Input;
};


void UEditableMesh::RemapOctreeIDs( const FElementIDRemappings& Remappings )
{
	if( Octree.IsValid() )
	{
		for( FPolygonID& PolygonID : DeletedOctreePolygonIDs )
		{
			PolygonID = Remappings.GetRemappedPolygonID( PolygonID );
		}

		for( FPolygonID& PolygonID : NewOctreePolygonIDs )
		{
			PolygonID = Remappings.GetRemappedPolygonID( PolygonID );
		}
	}

	{
		// Make a temporary copy of the original map, and clear the stored version
		static TMap<FPolygonID, FOctreeElementId> OldPolygonIDToOctreeElementIDMap;
		OldPolygonIDToOctreeElementIDMap = PolygonIDToOctreeElementIDMap;
		PolygonIDToOctreeElementIDMap.Reset();

		// Rebuild the map with the remapped polygon IDs
		for( const auto& OldPolygonIDAndOctreeElementID : OldPolygonIDToOctreeElementIDMap )
		{
			const FPolygonID OldPolygonID = OldPolygonIDAndOctreeElementID.Key;
			const FOctreeElementId OctreeElementID = OldPolygonIDAndOctreeElementID.Value;

			const FPolygonID NewPolygonID = Remappings.GetRemappedPolygonID( OldPolygonID );
			PolygonIDToOctreeElementIDMap.Add( NewPolygonID, OctreeElementID );
		}
	}
}


void UEditableMesh::Compact()
{
	static FElementIDRemappings Remappings;
	GetMeshDescription()->Compact( Remappings );
	RemapOctreeIDs( Remappings );

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnReindexElements( this, Remappings );
	}

	// Broadcast event with remappings so that any cached element IDs can be fixed up.
	ElementIDsRemappedEvent.Broadcast( this, Remappings );

	RebuildRenderMesh();
	RebuildOctree();

	// Prepare the inverse transaction to reverse the compaction
	FUncompactChangeInput UncompactChangeInput;
	InvertRemapTable( UncompactChangeInput.ElementIDRemappings.NewVertexIndexLookup, Remappings.NewVertexIndexLookup );
	InvertRemapTable( UncompactChangeInput.ElementIDRemappings.NewVertexInstanceIndexLookup, Remappings.NewVertexInstanceIndexLookup );
	InvertRemapTable( UncompactChangeInput.ElementIDRemappings.NewEdgeIndexLookup, Remappings.NewEdgeIndexLookup );
	InvertRemapTable( UncompactChangeInput.ElementIDRemappings.NewPolygonIndexLookup, Remappings.NewPolygonIndexLookup );
	InvertRemapTable( UncompactChangeInput.ElementIDRemappings.NewPolygonGroupIndexLookup, Remappings.NewPolygonGroupIndexLookup );

	AddUndo( MakeUnique<FUncompactChange>( MoveTemp( UncompactChangeInput ) ) );
}


void UEditableMesh::Uncompact( const FElementIDRemappings& Remappings )
{
	GetMeshDescription()->Remap( Remappings );
	RemapOctreeIDs( Remappings );

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnReindexElements( this, Remappings );
	}

	// Broadcast event with remappings so that any cached element IDs can be fixed up.
	ElementIDsRemappedEvent.Broadcast( this, Remappings );

	RebuildRenderMesh();
	RebuildOctree();

	AddUndo( MakeUnique<FCompactChange>() );
}


void UEditableMesh::InitializeAdapters()
{
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->InitializeFromEditableMesh( this );
	}

	RebuildRenderMesh();
}


void UEditableMesh::RebuildRenderMesh()
{
	if( !IsBeingModified() )
	{
		const bool bInvalidateLighting = true;
		for( UEditableMeshAdapter* Adapter : Adapters )
		{
			Adapter->OnRebuildRenderMeshStart( this, bInvalidateLighting );
		}
	}

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnRebuildRenderMesh( this );
	}

	if( !IsBeingModified() )
	{
		const bool bRebuildBoundsAndCollision = true;
		for( UEditableMeshAdapter* Adapter : Adapters )
		{
			const bool bIsPreviewRollback = false;
			Adapter->OnRebuildRenderMeshFinish( this, bRebuildBoundsAndCollision, bIsPreviewRollback );
		}
	}
}


void UEditableMesh::StartModification( const EMeshModificationType MeshModificationType, const EMeshTopologyChange MeshTopologyChange )
{
	if( ensure( !IsBeingModified() ) )
	{
		bIsBeingModified = true;

		// Should be nothing in the undo stack if we're just starting to modify the mesh now
		ensure( !this->AnyChangesToUndo() );

		FStartOrEndModificationChangeInput RevertInput;
		RevertInput.bStartModification = false;
		RevertInput.MeshModificationType = MeshModificationType;
		RevertInput.MeshTopologyChange = MeshTopologyChange;
		AddUndo( MakeUnique<FStartOrEndModificationChange>( MoveTemp( RevertInput ) ) );

		this->CurrentModificationType = MeshModificationType;
		this->CurrentToplogyChange = MeshTopologyChange;

		// @todo mesheditor debug: Disable noisy mesh editor spew by default (here and elsewhere)
		// UE_LOG( LogEditableMesh, Log, TEXT( "UEditableStaticMesh::StartModification START: %s" ), *SubMeshAddress.ToString() );
		FAutoScopedDurationTimer FunctionTimer;

		const bool bRefreshBounds = MeshModificationType == EMeshModificationType::Final;	 // @todo mesheditor perf: Only do this if we may have changed the bounds
		const bool bInvalidateLighting = ( MeshModificationType == EMeshModificationType::FirstInterim || MeshModificationType == EMeshModificationType::Final );	// @todo mesheditor perf: We can avoid invalidating lighting on 'Final' if we know that a 'FirstInterim' happened since the last 'Final'
		for( UEditableMeshAdapter* Adapter : Adapters )
		{
			Adapter->OnRebuildRenderMeshStart( this, bInvalidateLighting );
		}

		for( UEditableMeshAdapter* Adapter : Adapters )
		{
			Adapter->OnStartModification( this, MeshModificationType, MeshTopologyChange );
		}

		PolygonsPendingNewTangentBasis.Reset();
		PolygonsPendingFlipTangentBasis.Reset();
		PolygonsPendingTriangulation.Reset();
		VerticesPendingMerging.Reset();

		// @todo mesheditor debug
		// UE_LOG( LogEditableMesh, Log, TEXT( "UEditableStaticMesh::StartModification COMPLETE in %0.4fs" ), FunctionTimer.GetTime() );
	}
}


void UEditableMesh::EndModification( const bool bFromUndo )
{
	if( ensure( IsBeingModified() ) )
	{
		// @todo mesheditor debug
		// UE_LOG( LogEditableMesh, Log, TEXT( "UEditableStaticMesh::EndModification START (ModType=%i): %s" ), (int32)MeshModificationType, *SubMeshAddress.ToString() );
		// FAutoScopedDurationTimer FunctionTimer;

		// If there are any vertices which have been marked as candidates for having any of their instances merged, do this now
		if( VerticesPendingMerging.Num() > 0 )
		{
			MergeVertexInstances();
		}

		// Now we need to retriangulate polygons and recalculate tangents/normals for any polygons and vertices which have been affected
		// by any operation in this transaction.
		// Turn off undo because the reciprocal operation will do this at the end of its transaction.
		{
			const bool bIsUndoAllowed = bAllowUndo;
			bAllowUndo = false;

			// Retriangulate first, as the triangulation of n-gons determines how the tangent basis is calculated
			if( PolygonsPendingTriangulation.Num() > 0 )
			{
				RetriangulatePolygons();
			}

			if( PolygonsPendingNewTangentBasis.Num() > 0 )
			{
				GenerateTangentsAndNormals();
			}

			// Exclude the polygons that have already regenerated their normal/tangent
			PolygonsPendingFlipTangentBasis = PolygonsPendingFlipTangentBasis.Difference( PolygonsPendingNewTangentBasis );
			if( PolygonsPendingFlipTangentBasis.Num() > 0 )
			{
				FlipTangentsAndNormals();
			}

			bAllowUndo = bIsUndoAllowed;
		}

		if( CurrentModificationType == EMeshModificationType::Final || !bFromUndo )
		{
#if EDITABLE_MESH_USE_OPENSUBDIV
			// Update subdivision limit surface
			if( CurrentToplogyChange == EMeshTopologyChange::TopologyChange )
			{
				// Mesh topology (or subdivision level or smoothing) may have changed, so go ahead and refresh our OpenSubdiv representation entirely
				RefreshOpenSubdiv();
			}
			else
			{
				// No topology change, so we can ask OpenSubdiv to quickly generate new limit surface geometry
				GenerateOpenSubdivLimitSurfaceData();
			}
#endif
		}

		// Every so often, compact the data.
		// Note we only want to do this when actions are performed, not when they are being undone/redone
		bool bDidCompact = false;

		if( CurrentModificationType == EMeshModificationType::Final &&
			CurrentToplogyChange == EMeshTopologyChange::TopologyChange &&
			!bFromUndo &&
			bAllowCompact )
		{
			if( false )//++PendingCompactCounter == CompactFrequency )
			{
				PendingCompactCounter = 0;
				Compact();
				bDidCompact = true;
			}
		}

		// If subdivision preview mode is active, we'll need to refresh the entire static mesh with data from OpenSubdiv
		// @todo mesheditor subdiv perf: Ideally we can avoid refreshing the entire thing if only positions have changed, as per above
		if( IsPreviewingSubdivisions() && ( CurrentModificationType == EMeshModificationType::Final || !bFromUndo ) )
		{
			if( !bDidCompact )	// If we did a Compact() in this function, the mesh will have already been rebuilt
			{
				for( UEditableMeshAdapter* Adapter : Adapters )
				{
					Adapter->OnRebuildRenderMesh( this );
				}
			}
		}

		for( UEditableMeshAdapter* Adapter : Adapters )
		{
			const bool bRebuildBoundsAndCollision = ( CurrentModificationType == EMeshModificationType::Final );
			const bool bIsPreviewRollback = !( CurrentModificationType == EMeshModificationType::Final || !bFromUndo );
			Adapter->OnRebuildRenderMeshFinish( this, bRebuildBoundsAndCollision, bIsPreviewRollback );
		}

		for( UEditableMeshAdapter* Adapter : Adapters )
		{
			Adapter->OnEndModification( this );
		}

		// @todo mesheditor: Not currently sure if we need to do this or not.  Also, we are trying to support runtime editing (no PostEditChange stuff)
		// 		if( GIsEditor && MeshModificationType == EMeshModificationType::Final )
		// 		{
		// 			StaticMesh->PostEditChange();
		// 		}

		// @todo mesheditor debug
		// UE_LOG( LogEditableMesh, Log, TEXT( "UEditableStaticMesh::EndModification COMPLETE in %0.4fs" ), FunctionTimer.GetTime() );	  // @todo mesheditor: Shows bogus time values

		// Rebuild our octree
		if( CurrentModificationType == EMeshModificationType::Final )
		{
			UpdateOrRebuildOctree();
		}

		FStartOrEndModificationChangeInput RevertInput;
		RevertInput.bStartModification = true;
		RevertInput.MeshModificationType = CurrentModificationType;
		RevertInput.MeshTopologyChange = CurrentToplogyChange;
		AddUndo( MakeUnique<FStartOrEndModificationChange>( MoveTemp( RevertInput ) ) );

		bIsBeingModified = false;
	}
}


bool UEditableMesh::IsCommitted() const
{
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		if( !Adapter->IsCommitted( this ) )
		{
			return false;
		}
	}

	return true;
}


bool UEditableMesh::IsCommittedAsInstance() const
{
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		if( !Adapter->IsCommittedAsInstance( this ) )
		{
			return false;
		}
	}

	return true;
}


void UEditableMesh::Commit()
{
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnCommit( this );
	}
}


UEditableMesh* UEditableMesh::CommitInstance( UPrimitiveComponent* ComponentToInstanceTo )
{
	UEditableMesh* Result = nullptr;
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		UEditableMesh* Instance = Adapter->OnCommitInstance( this, ComponentToInstanceTo );
		if( !Result )
		{
			Result = Instance;
		}
	}

	return Result;
}


// @todo mesheditor: implement these, or - better - reconsider the editing paradigm
void UEditableMesh::Revert() {}
UEditableMesh* UEditableMesh::RevertInstance() { return nullptr; }
void UEditableMesh::PropagateInstanceChanges() {}


const FEditableMeshSubMeshAddress& UEditableMesh::GetSubMeshAddress() const
{
	return SubMeshAddress;
}


void UEditableMesh::SetSubMeshAddress( const FEditableMeshSubMeshAddress& NewSubMeshAddress )
{
	SubMeshAddress = NewSubMeshAddress;
}


int32 UEditableMesh::GetVertexCount() const
{
	return GetMeshDescription()->Vertices().Num();
}


bool UEditableMesh::IsValidVertex( const FVertexID VertexID ) const
{
	return GetMeshDescription()->IsVertexValid( VertexID );
}


bool UEditableMesh::IsOrphanedVertex( const FVertexID VertexID ) const
{
	return GetMeshDescription()->IsVertexOrphaned( VertexID );
}


int32 UEditableMesh::GetVertexConnectedEdgeCount( const FVertexID VertexID ) const
{
	return GetMeshDescription()->GetVertexConnectedEdges( VertexID ).Num();
}


FEdgeID UEditableMesh::GetVertexConnectedEdge( const FVertexID VertexID, const int32 ConnectedEdgeNumber ) const
{
	const TArray<FEdgeID>& ConnectedEdgeIDs = GetMeshDescription()->GetVertexConnectedEdges( VertexID );
	return ( ConnectedEdgeNumber >= 0 && ConnectedEdgeNumber < ConnectedEdgeIDs.Num() ) ? ConnectedEdgeIDs[ ConnectedEdgeNumber ] : FEdgeID::Invalid;
}


int32 UEditableMesh::GetVertexInstanceCount() const
{
	return GetMeshDescription()->VertexInstances().Num();
}


FVertexID UEditableMesh::GetVertexInstanceVertex( const FVertexInstanceID VertexInstanceID ) const
{
	return GetMeshDescription()->GetVertexInstanceVertex( VertexInstanceID );
}


int32 UEditableMesh::GetVertexInstanceConnectedPolygonCount( const FVertexInstanceID VertexInstanceID ) const
{
	return GetMeshDescription()->GetVertexInstanceConnectedPolygons( VertexInstanceID ).Num();
}


FPolygonID UEditableMesh::GetVertexInstanceConnectedPolygon( const FVertexInstanceID VertexInstanceID, const int32 ConnectedPolygonNumber ) const
{
	const TArray<FPolygonID>& ConnectedPolygonIDs = GetMeshDescription()->GetVertexInstanceConnectedPolygons( VertexInstanceID );
	return ( ConnectedPolygonNumber >= 0 && ConnectedPolygonNumber < ConnectedPolygonIDs.Num() ) ? ConnectedPolygonIDs[ ConnectedPolygonNumber ] : FPolygonID::Invalid;
}


int32 UEditableMesh::GetEdgeCount() const
{
	return GetMeshDescription()->Edges().Num();
}


bool UEditableMesh::IsValidEdge( const FEdgeID EdgeID ) const
{
	return GetMeshDescription()->IsEdgeValid( EdgeID );
}


FVertexID UEditableMesh::GetEdgeVertex( const FEdgeID EdgeID, const int32 EdgeVertexNumber ) const
{
	checkSlow( EdgeVertexNumber >= 0 && EdgeVertexNumber < 2 );
	return GetMeshDescription()->GetEdgeVertex( EdgeID, EdgeVertexNumber );
}


int32 UEditableMesh::GetEdgeConnectedPolygonCount( const FEdgeID EdgeID ) const
{
	return GetMeshDescription()->GetEdgeConnectedPolygons( EdgeID ).Num();
}


FPolygonID UEditableMesh::GetEdgeConnectedPolygon( const FEdgeID EdgeID, const int32 ConnectedPolygonNumber ) const
{
	const TArray<FPolygonID>& ConnectedPolygonIDs = GetMeshDescription()->GetEdgeConnectedPolygons( EdgeID );
	return ( ConnectedPolygonNumber >= 0 && ConnectedPolygonNumber < ConnectedPolygonIDs.Num() ) ? ConnectedPolygonIDs[ ConnectedPolygonNumber ] : FPolygonID::Invalid;
}


int32 UEditableMesh::GetPolygonGroupCount() const
{
	return GetMeshDescription()->PolygonGroups().Num();
}


bool UEditableMesh::IsValidPolygonGroup( const FPolygonGroupID PolygonGroupID ) const
{
	return GetMeshDescription()->IsPolygonGroupValid( PolygonGroupID );
}


int32 UEditableMesh::GetPolygonCountInGroup( const FPolygonGroupID PolygonGroupID ) const
{
	return GetMeshDescription()->GetPolygonGroupPolygons( PolygonGroupID ).Num();
}


FPolygonID UEditableMesh::GetPolygonInGroup( const FPolygonGroupID PolygonGroupID, const int32 PolygonNumber ) const
{
	const TArray<FPolygonID>& PolygonIDs = GetMeshDescription()->GetPolygonGroupPolygons( PolygonGroupID );
	return ( PolygonNumber >= 0 && PolygonNumber < PolygonIDs.Num() ) ? PolygonIDs[ PolygonNumber ] : FPolygonID::Invalid;
}


int32 UEditableMesh::GetPolygonCount() const
{
	return GetMeshDescription()->Polygons().Num();
}


bool UEditableMesh::IsValidPolygon( const FPolygonID PolygonID ) const
{
	return GetMeshDescription()->IsPolygonValid( PolygonID );
}


FPolygonGroupID UEditableMesh::GetGroupForPolygon( const FPolygonID PolygonID ) const
{
	return GetMeshDescription()->GetPolygonPolygonGroup( PolygonID );
}


int32 UEditableMesh::GetPolygonPerimeterVertexCount( const FPolygonID PolygonID ) const
{
	return GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID ).Num();
}


FVertexInstanceID UEditableMesh::GetPolygonPerimeterVertexInstance( const FPolygonID PolygonID, const int32 PolygonVertexNumber ) const
{
	const TArray<FVertexInstanceID>& VertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID );
	return ( PolygonVertexNumber >= 0 && PolygonVertexNumber < VertexInstanceIDs.Num() ) ? VertexInstanceIDs[ PolygonVertexNumber ] : FVertexInstanceID::Invalid;
}


FVertexID UEditableMesh::GetPolygonPerimeterVertex( const FPolygonID PolygonID, const int32 PolygonVertexNumber ) const
{
	const FVertexInstanceID VertexInstanceID = GetPolygonPerimeterVertexInstance( PolygonID, PolygonVertexNumber );
	return ( VertexInstanceID != FVertexInstanceID::Invalid ) ? GetMeshDescription()->GetVertexInstanceVertex( VertexInstanceID ) : FVertexID::Invalid;
}


int32 UEditableMesh::GetPolygonTriangulatedTriangleCount( const FPolygonID PolygonID ) const
{
	return GetMeshDescription()->GetPolygonTriangles( PolygonID ).Num();
}


FMeshTriangle UEditableMesh::GetPolygonTriangulatedTriangle( const FPolygonID PolygonID, int32 PolygonTriangleNumber ) const
{
	const TArray<FMeshTriangle>& Triangles = GetMeshDescription()->GetPolygonTriangles( PolygonID );
	return ( PolygonTriangleNumber >= 0 && PolygonTriangleNumber < Triangles.Num() ) ? Triangles[ PolygonTriangleNumber ] : FMeshTriangle();
}


void UEditableMesh::SetVertexAttribute( const FVertexID VertexID, const FMeshElementAttributeData& Attribute )
{
	ApplyAttribute( GetMeshDescription()->VertexAttributes(), Attribute, VertexID );

	// @todo mesh editor: create attribute flag which signifies that the octree needs rebuilding
	if( Attribute.AttributeName == MeshAttribute::Vertex::Position )
	{
		// Update spatial database
		if( Octree.IsValid() )
		{
			static TArray<FPolygonID> ConnectedPolygons;
			GetVertexConnectedPolygons( VertexID, /* Out */ ConnectedPolygons );
			for( const FPolygonID PolygonID : ConnectedPolygons )
			{
				// Only mark the polygon to be removed from the octree if it was already in it to begin with
				bool bAlreadyInSet = false;
				NewOctreePolygonIDs.Add( PolygonID, /* Out */ &bAlreadyInSet );
				const bool bIsAlreadyInOctree = !bAlreadyInSet;
				if( bIsAlreadyInOctree )
				{
					DeletedOctreePolygonIDs.Add( PolygonID );
				}
			}
		}
	}

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnSetVertexAttribute( this, VertexID, Attribute );
	}
}


void UEditableMesh::SetVertexInstanceAttribute( const FVertexInstanceID VertexInstanceID, const FMeshElementAttributeData& Attribute )
{
	ApplyAttribute( GetMeshDescription()->VertexInstanceAttributes(), Attribute, VertexInstanceID );

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnSetVertexInstanceAttribute( this, VertexInstanceID, Attribute );
	}
}


void UEditableMesh::SetEdgeAttribute( const FEdgeID EdgeID, const FMeshElementAttributeData& Attribute )
{
	ApplyAttribute( GetMeshDescription()->EdgeAttributes(), Attribute, EdgeID );

	if( Attribute.AttributeName == MeshAttribute::Edge::IsHard )
	{
		if( Attribute.AttributeValue.GetValue<bool>() )
		{
			// If edge is being made hard, we may need to split vertex instances.
			static TArray<FVertexID> EdgeVertices;
			EdgeVertices.Reset( 2 );
			EdgeVertices.Add( GetMeshDescription()->GetEdgeVertex( EdgeID, 0 ) );
			EdgeVertices.Add( GetMeshDescription()->GetEdgeVertex( EdgeID, 1 ) );
			SplitVerticesIfNecessary( EdgeVertices );
		}
		else
		{
			// If edge is being made soft, mark its vertices as candidates for vertex merging at the end of the transaction.
			VerticesPendingMerging.Add( GetMeshDescription()->GetEdgeVertex( EdgeID, 0 ) );
			VerticesPendingMerging.Add( GetMeshDescription()->GetEdgeVertex( EdgeID, 1 ) );
		}

		PolygonsPendingNewTangentBasis.Append( GetMeshDescription()->GetEdgeConnectedPolygons( EdgeID ) );
	}

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnSetEdgeAttribute( this, EdgeID, Attribute );
	}
}


void UEditableMesh::SetPolygonAttribute( const FPolygonID PolygonID, const FMeshElementAttributeData& Attribute )
{
	ApplyAttribute( GetMeshDescription()->PolygonAttributes(), Attribute, PolygonID );

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		// @todo: implement me!
		Adapter->OnSetPolygonAttribute( this, PolygonID, Attribute );
	}
}


void UEditableMesh::SetPolygonGroupAttribute( const FPolygonGroupID PolygonGroupID, const FMeshElementAttributeData& Attribute )
{
	ApplyAttribute( GetMeshDescription()->PolygonGroupAttributes(), Attribute, PolygonGroupID );

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnSetPolygonGroupAttribute( this, PolygonGroupID, Attribute );
	}
}


void UEditableMesh::GetPolygonsInSameSoftEdgedGroupAsPolygon( const FPolygonID PolygonID, const TArray<FPolygonID>& CandidatePolygonIDs, const TArray<FEdgeID>& SoftEdgeIDs, TArray<FPolygonID>& OutPolygonIDs ) const
{
	// The aim of this method is:
	// - given a polygon ID,
	// - given a set of candidate polygons connected to the same vertex (which should include the polygon ID),
	// - given a set of soft edges connected to the same vertex,
	// return the polygon IDs which form an adjacent run without crossing a hard edge.

	OutPolygonIDs.Reset();

	// Maintain a list of polygon IDs to be examined. Adjacents are added to the list if suitable.
	// Add the start poly here.
	static TArray<FPolygonID> PolygonsToCheck;
	PolygonsToCheck.Reset( CandidatePolygonIDs.Num() );
	PolygonsToCheck.Add( PolygonID );

	int32 Index = 0;
	while( Index < PolygonsToCheck.Num() )
	{
		const FPolygonID PolygonToCheck = PolygonsToCheck[ Index ];
		Index++;

		if( CandidatePolygonIDs.Contains( PolygonToCheck ) )
		{
			OutPolygonIDs.Add( PolygonToCheck );

			// Now look at its adjacent polygons. If they are joined by a soft edge which includes the vertex we're interested in, we want to consider them.
			// We take a shortcut by doing this process in reverse: we already know all the soft edges we are interested in, so check if any of them
			// have the current polygon as an adjacent.
			for( const FEdgeID SoftEdgeID : SoftEdgeIDs )
			{
				const TArray<FPolygonID>& EdgeConnectedPolygons = GetMeshDescription()->GetEdgeConnectedPolygons( SoftEdgeID );
				if( EdgeConnectedPolygons.Contains( PolygonToCheck ) )
				{
					for( const FPolygonID AdjacentPolygon : EdgeConnectedPolygons )
					{
						// Only add new polygons which haven't yet been added to the list. This prevents circular runs of polygons triggering infinite loops.
						PolygonsToCheck.AddUnique( AdjacentPolygon );
					}
				}
			}
		}
	}
}


void UEditableMesh::GetVertexConnectedPolygonsInSameSoftEdgedGroup( const FVertexID VertexID, const FPolygonID PolygonID, TArray<FPolygonID>& OutPolygonIDs ) const
{
	// The aim here is to determine which polygons form part of the same soft edged group as the polygons attached to this vertex.
	// They should all contribute to the final vertex instance normal.

	// Get all polygons connected to this vertex.
	static TArray<FPolygonID> ConnectedPolygons;
	GetVertexConnectedPolygons( VertexID, ConnectedPolygons );

	// Cache a list of all soft edges which share this vertex.
	// We're only interested in finding adjacent polygons which are not the other side of a hard edge.
	static TArray<FEdgeID> ConnectedSoftEdges;
	GetConnectedSoftEdges( VertexID, ConnectedSoftEdges );

	GetPolygonsInSameSoftEdgedGroupAsPolygon( PolygonID, ConnectedPolygons, ConnectedSoftEdges, OutPolygonIDs );
}


void UEditableMesh::SplitVertexInstanceInPolygons( const FVertexInstanceID VertexInstanceID, const TArray<FPolygonID>& PolygonIDs )
{
	EM_ENTER( TEXT( "SplitVertexInstanceInPolygons: %s %s" ), *VertexInstanceID.ToString(), *LogHelpers::ArrayToString( PolygonIDs ) );

	// Create a new vertex instance copied from the one passed in

	static TArray<FVertexInstanceToCreate> VertexInstancesToCreate;
	VertexInstancesToCreate.Reset();
	VertexInstancesToCreate.SetNum( 1 );
	VertexInstancesToCreate[ 0 ].VertexID = GetMeshDescription()->GetVertexInstanceVertex( VertexInstanceID );

	BackupAllAttributes( VertexInstancesToCreate[ 0 ].VertexInstanceAttributes, GetMeshDescription()->VertexInstanceAttributes(), VertexInstanceID );

	static TArray<FVertexInstanceID> NewVertexInstanceIDs;
	CreateVertexInstances( VertexInstancesToCreate, NewVertexInstanceIDs );

	ReplaceVertexInstanceInPolygons( VertexInstanceID, NewVertexInstanceIDs[ 0 ], PolygonIDs );

	EM_EXIT( TEXT( "SplitVertexInstanceInPolygons returned" ) );
}


void UEditableMesh::ReplaceVertexInstanceInPolygons( const FVertexInstanceID OldVertexInstanceID, const FVertexInstanceID NewVertexInstanceID, const TArray<FPolygonID>& PolygonIDs )
{
	EM_ENTER( TEXT( "ReplaceVertexInstanceInPolygons: %s %s %s" ), *OldVertexInstanceID.ToString(), *NewVertexInstanceID.ToString(), *LogHelpers::ArrayToString( PolygonIDs ) );

	// Substitute the new vertex instance in the passed in polygons

	static TArray<FChangeVertexInstancesForPolygon>  VertexInstancesToChange;
	VertexInstancesToChange.Reset( PolygonIDs.Num() );

	const FPolygonArray& Polygons = GetMeshDescription()->Polygons();
	for( const FPolygonID PolygonID : PolygonIDs )
	{
		VertexInstancesToChange.Emplace();
		FChangeVertexInstancesForPolygon& ChangeVertexInstances = VertexInstancesToChange.Last();

		ChangeVertexInstances.PolygonID = PolygonID;

		const FMeshPolygon& Polygon = Polygons[ PolygonID ];
		int32 VertexInstanceIndex = Polygon.PerimeterContour.VertexInstanceIDs.Find( OldVertexInstanceID );

		if( VertexInstanceIndex != INDEX_NONE )
		{
			// Found the vertex instance in the perimeter
			ChangeVertexInstances.PerimeterVertexIndicesAndInstanceIDs.Emplace();
			ChangeVertexInstances.PerimeterVertexIndicesAndInstanceIDs[ 0 ].ContourIndex = VertexInstanceIndex;
			ChangeVertexInstances.PerimeterVertexIndicesAndInstanceIDs[ 0 ].VertexInstanceID = NewVertexInstanceID;
		}

		// We expect to have found the vertex instance somewhere in one of the polygon contours
		check( VertexInstanceIndex != INDEX_NONE );
	}

	ChangePolygonsVertexInstances( VertexInstancesToChange );

	EM_EXIT( TEXT( "ReplaceVertexInstanceInPolygons returned" ) );
}


void UEditableMesh::SplitVerticesIfNecessary( const TArray<FVertexID>& VerticesToSplit )
{
	static TArray<FVertexInstanceToCreate> VertexInstancesToCreate;
	VertexInstancesToCreate.Reset();

	// Loop for each vertex to split
	for( const FVertexID VertexToSplit : VerticesToSplit )
	{
		// Cache a list of all soft edges which share this vertex.
		static TArray<FEdgeID> ConnectedSoftEdges;
		GetConnectedSoftEdges( VertexToSplit, ConnectedSoftEdges );

		// Look at each vertex instance in turn.
		// Take a copy because splitting them will mutate the list we are iterating.
		static TArray<FVertexInstanceID> VertexInstanceIDs;
		VertexInstanceIDs = GetMeshDescription()->GetVertexVertexInstances( VertexToSplit );

		for( const FVertexInstanceID VertexInstanceID : VertexInstanceIDs )
		{
			// Get the list of polygons connected to this vertex instance.
			// We only need to potentially do something if there is more than one polygon connected.
			const TArray<FPolygonID>& PolygonIDs = GetMeshDescription()->GetVertexInstanceConnectedPolygons( VertexInstanceID );
			if( PolygonIDs.Num() > 1 )
			{
				// Take a copy of all the connected polygons.
				// This is a list we will consume as we identify groups of polygons not separated by a hard edge.
				static TArray<FPolygonID> PolygonIDsToCheck;
				PolygonIDsToCheck = PolygonIDs;

				bool bFirstTime = true;
				while( PolygonIDsToCheck.Num() > 0 )
				{
					// For the next polygon in the array, determine all other polygons in the same soft edged group
					static TArray<FPolygonID> PolygonsInSameSoftEdgedGroup;
					GetPolygonsInSameSoftEdgedGroupAsPolygon( PolygonIDsToCheck[ 0 ], PolygonIDsToCheck, ConnectedSoftEdges, PolygonsInSameSoftEdgedGroup );

					// Check that all polygons in the smoothing group are attached to this vertex instance, and remove them from the master list of polygons
					// connected to this instance. If a polygon in the smoothing group is not attached to this vertex instance, it's because it's the other
					// side of a UV seam and hence has a distinct vertex instance.
					for( const FPolygonID PolygonInSameSoftEdgedGroup : PolygonsInSameSoftEdgedGroup )
					{
						verify( PolygonIDsToCheck.Remove( PolygonInSameSoftEdgedGroup ) == 1 );
					}

					// First group which we extract: do nothing - they can keep their existing instance ID.
					// Subsequent times round the loop, we create a new vertex instance copied from the original one, and replace connected polygon vertices with it.
					if( !bFirstTime )
					{
						SplitVertexInstanceInPolygons( VertexInstanceID, PolygonsInSameSoftEdgedGroup );
					}

					bFirstTime = false;
				}
			}
		}
	}
}


void UEditableMesh::MergeVertexInstances()
{
	if( VerticesPendingMerging.Num() == 0 )
	{
		return;
	}

	EM_ENTER( TEXT( "MergeVertexInstances" ) );

	static TArray<FVertexInstanceID> VertexInstancesToDelete;
	VertexInstancesToDelete.Reset();

	for( const FVertexID VertexID : VerticesPendingMerging )
	{
		// Get all polygons connected to this vertex.
		static TArray<FPolygonID> VertexConnectedPolygons;
		GetVertexConnectedPolygons( VertexID, VertexConnectedPolygons );

		// Cache a list of all soft edges which share this vertex.
		// We're only interested in merging vertex instances which are in the same soft edged group.
		static TArray<FEdgeID> VertexConnectedSoftEdges;
		GetConnectedSoftEdges( VertexID, VertexConnectedSoftEdges );

		// Get all vertex instances of this vertex...
		const TArray<FVertexInstanceID>& VertexInstanceIDs = GetMeshDescription()->GetVertexVertexInstances( VertexID );

		// ...and iterate through pairs of vertex instances, looking for potential to merge them
		for( int32 IndexA = 0; IndexA < VertexInstanceIDs.Num() - 1; ++IndexA )
		{
			const FVertexInstanceID VertexInstanceIDA = VertexInstanceIDs[ IndexA ];

			// If vertex instance isn't connected to any polygon, we can't deduce anything about its smoothing group, so skip to the next one.
			// This will be the case if it is already orphaned, *or* if it has been merged into another vertex instance in an earlier iteration.
			const TArray<FPolygonID>& ConnectedPolygonIDsA = GetMeshDescription()->GetVertexInstanceConnectedPolygons( VertexInstanceIDA );
			if( ConnectedPolygonIDsA.Num() == 0 )
			{
				continue;
			}

			// Determine the polygons which share this vertex which form the soft edged group which contain this vertex instance
			static TArray<FPolygonID> PolygonIDsInSoftEdgedGroup;
			GetPolygonsInSameSoftEdgedGroupAsPolygon( ConnectedPolygonIDsA[ 0 ], VertexConnectedPolygons, VertexConnectedSoftEdges, PolygonIDsInSoftEdgedGroup );

			for( int32 IndexB = IndexA + 1; IndexB < VertexInstanceIDs.Num(); ++IndexB )
			{
				const FVertexInstanceID VertexInstanceIDB = VertexInstanceIDs[ IndexB ];

				// If this vertex instance has been marked for deletion in a previous iteration, skip it
				if( VertexInstancesToDelete.Contains( VertexInstanceIDB ) )
				{
					continue;
				}

				// If the vertex instances are not in the same soft edged group, skip it
				const TArray<FPolygonID>& ConnectedPolygonIDsB = GetMeshDescription()->GetVertexInstanceConnectedPolygons( VertexInstanceIDB );
				if( !PolygonIDsInSoftEdgedGroup.ContainsByPredicate( [ &ConnectedPolygonIDsB ]( const FPolygonID PolygonID ) { return ConnectedPolygonIDsB.Contains( PolygonID ); } ) )
				{
					continue;
				}

				if( CompareAttributesByPredicate(
					GetMeshDescription()->VertexInstanceAttributes(),
					VertexInstanceIDA,
					VertexInstanceIDB,
					[]( const FName AttributeName, const auto AttributeArrayRef ) { return EnumHasAllFlags( AttributeArrayRef.GetFlags(), EMeshAttributeFlags::Mergeable ); }
				  ) )
				{
					// Change occurrences of VertexInstanceB for VertexInstanceA in VertexInstanceB's connected polygons.
					// Note, this will cause VertexInstanceA's connected polygons list to be added to (at the end).
					// This works because we are evaluating the number of connected polygons each time round the loop.
					ReplaceVertexInstanceInPolygons( VertexInstanceIDB, VertexInstanceIDA, ConnectedPolygonIDsB );

					// This will also cause VertexInstanceB to be disconnected from all polygons.
					// We mark the vertex instance for deletion here, but do not delete it until the end, as to do so would interrupt iterating through vertex instances.
					check( ConnectedPolygonIDsB.Num() == 0 );
					VertexInstancesToDelete.Add( VertexInstanceIDB );
				}
			}
		}
	}

	// Delete orphaned vertex instances
	const bool bDeleteOrphanedVertices = false;
	DeleteVertexInstances( VertexInstancesToDelete, bDeleteOrphanedVertices );

	EM_EXIT( TEXT( "MergeVertexInstances returned" ) );
}


FPolygonGroupID UEditableMesh::GetFirstValidPolygonGroup() const
{
	return GetMeshDescription()->PolygonGroups().GetFirstValidID();
}


int32 UEditableMesh::GetTextureCoordinateCount() const
{
	return this->TextureCoordinateCount;
}


int32 UEditableMesh::GetSubdivisionCount() const
{
	return this->SubdivisionCount;
}


bool UEditableMesh::IsPreviewingSubdivisions() const
{
	return GetSubdivisionCount() > 0;		// @todo mesheditor subdiv: Make optional even when subdivisions are active
}


void UEditableMesh::GetVertexConnectedEdges( const FVertexID VertexID, TArray<FEdgeID>& OutConnectedEdgeIDs ) const
{
	OutConnectedEdgeIDs = GetMeshDescription()->GetVertexConnectedEdges( VertexID );
}


void UEditableMesh::GetVertexConnectedPolygons( const FVertexID VertexID, TArray<FPolygonID>& OutConnectedPolygonIDs ) const
{
	GetMeshDescription()->GetVertexConnectedPolygons( VertexID, OutConnectedPolygonIDs );
}


void UEditableMesh::GetVertexInstanceConnectedPolygons( const FVertexInstanceID VertexInstanceID, TArray<FPolygonID>& OutConnectedPolygonIDs ) const
{
	OutConnectedPolygonIDs = GetMeshDescription()->GetVertexInstanceConnectedPolygons( VertexInstanceID );
}


void UEditableMesh::GetVertexAdjacentVertices( const FVertexID VertexID, TArray<FVertexID>& OutAdjacentVertexIDs ) const
{
	GetMeshDescription()->GetVertexAdjacentVertices( VertexID, OutAdjacentVertexIDs );
}


void UEditableMesh::GetEdgeVertices( const FEdgeID EdgeID, FVertexID& OutEdgeVertexID0, FVertexID& OutEdgeVertexID1 ) const
{
	OutEdgeVertexID0 = GetMeshDescription()->GetEdgeVertex( EdgeID, 0 );
	OutEdgeVertexID1 = GetMeshDescription()->GetEdgeVertex( EdgeID, 1 );
}


void UEditableMesh::GetEdgeConnectedPolygons( const FEdgeID EdgeID, TArray<FPolygonID>& OutConnectedPolygonIDs ) const
{
	OutConnectedPolygonIDs = GetMeshDescription()->GetEdgeConnectedPolygons( EdgeID );
}


FEdgeID UEditableMesh::GetEdgeThatConnectsVertices( const FVertexID VertexID0, const FVertexID VertexID1 ) const
{
	return GetMeshDescription()->GetVertexPairEdge( VertexID0, VertexID1 );
}


void UEditableMesh::GetEdgeLoopElements( const FEdgeID EdgeID, TArray<FEdgeID>& EdgeLoopIDs ) const
{
	EdgeLoopIDs.Reset();

	// Maintain a list of unique edge IDs which form the loop
	static TSet<FEdgeID> EdgeIDs;
	EdgeIDs.Reset();
	
	// Maintain a stack of edges to be processed, in lieu of recursion.
	// We also store which vertex of the edge has already been processed (so we don't retrace our steps when processing stack items).
	static TArray<TTuple<FEdgeID, FVertexID>> EdgeStack;
	EdgeStack.Reset();
	EdgeStack.Push( MakeTuple( EdgeID, FVertexID::Invalid ) );

	// Process edge IDs on the stack
	while( EdgeStack.Num() > 0 )
	{
		const TTuple<FEdgeID, FVertexID> CurrentItem = EdgeStack.Pop();
		const FEdgeID CurrentEdgeID = CurrentItem.Get<0>();
		const FVertexID FromVertexID = CurrentItem.Get<1>();
		EdgeIDs.Add( CurrentEdgeID );

		// See which polygons are connected to the current edge.
		// When continuing the loop, the criterion is that new edges must share no polygons with this edge,
		// i.e. they are the other side of a perpendicular edge.
		const TArray<FPolygonID>& CurrentEdgeConnectedPolygons = GetMeshDescription()->GetEdgeConnectedPolygons( CurrentEdgeID );

		// Now look for edges connected to each end of this edge
		for( int32 EdgeVertexIndex = 0; EdgeVertexIndex < 2; ++EdgeVertexIndex )
		{
			const FVertexID ConnectedVertexID = GetMeshDescription()->GetEdgeVertex( CurrentEdgeID, EdgeVertexIndex );

			// If we have already processed this vertex, skip it
			if( ConnectedVertexID == FromVertexID )
			{
				continue;
			}

			// This is the candidate edge ID which continues the loop beyond the vertex being processed
			FEdgeID AdjacentEdgeID = FEdgeID::Invalid;

			// Iterate through all edges connected to this vertex
			for( const FEdgeID ConnectedEdgeID : GetMeshDescription()->GetVertexConnectedEdges( ConnectedVertexID ) )
			{
				// If this edge hasn't been added to the loop...
				if( !EdgeIDs.Contains( ConnectedEdgeID ) )
				{
					// ...see if it shares any polygons with the original edge (intersection operation)
					bool bIsCandidateEdge = true;
					for( const FPolygonID ConnectedPolygonID : GetMeshDescription()->GetEdgeConnectedPolygons( ConnectedEdgeID ) )
					{
						if( CurrentEdgeConnectedPolygons.Contains( ConnectedPolygonID ) )
						{
							bIsCandidateEdge = false;
							break;
						}
					}

					// We have found an edge connected to this vertex which doesn't share any polys with the original edge
					if( bIsCandidateEdge )
					{
						if( AdjacentEdgeID == FEdgeID::Invalid )
						{
							// If it's the first such edge which meets the criteria, remember it
							AdjacentEdgeID = ConnectedEdgeID;
						}
						else
						{
							// If we already have a possible edge, stop the loop here; we don't allow splits in the loop if there is more than one candidate
							AdjacentEdgeID = FEdgeID::Invalid;
							break;
						}
					}
				}
			}

			if( AdjacentEdgeID != FEdgeID::Invalid )
			{
				EdgeStack.Push( MakeTuple( AdjacentEdgeID, ConnectedVertexID ) );
			}
		}
	}

	for( const FEdgeID EdgeLoopID : EdgeIDs )
	{
		EdgeLoopIDs.Add( EdgeLoopID );
	}
}


int32 UEditableMesh::GetPolygonPerimeterEdgeCount( const FPolygonID PolygonID ) const
{
	// All polygons have the same number of edges as they do vertices
	return GetPolygonPerimeterVertexCount( PolygonID );
}


void UEditableMesh::GetPolygonPerimeterVertices( const FPolygonID PolygonID, TArray<FVertexID>& OutPolygonPerimeterVertexIDs ) const
{
	GetMeshDescription()->GetPolygonPerimeterVertices( PolygonID, OutPolygonPerimeterVertexIDs );
}


void UEditableMesh::GetPolygonPerimeterVertexInstances( const FPolygonID PolygonID, TArray<FVertexInstanceID>& OutPolygonPerimeterVertexInstanceIDs ) const
{
	OutPolygonPerimeterVertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID );
}


FEdgeID UEditableMesh::GetPolygonPerimeterEdge( const FPolygonID PolygonID, const int32 PerimeterEdgeNumber, bool& bOutEdgeWindingIsReversedForPolygon ) const
{
	const FMeshDescription* Description = GetMeshDescription();
	const TArray<FVertexInstanceID>& VertexInstanceIDs = Description->GetPolygonPerimeterVertexInstances( PolygonID );
	if( PerimeterEdgeNumber >= VertexInstanceIDs.Num() )
	{
		return FEdgeID::Invalid;
	}

	const FVertexID VertexID0 = Description->GetVertexInstanceVertex( VertexInstanceIDs[ PerimeterEdgeNumber ] );
	const FVertexID VertexID1 = Description->GetVertexInstanceVertex( VertexInstanceIDs[ ( PerimeterEdgeNumber + 1 ) % VertexInstanceIDs.Num() ] );
	return GetVertexPairEdge( VertexID0, VertexID1, bOutEdgeWindingIsReversedForPolygon );
}


FEdgeID UEditableMesh::GetVertexPairEdge( const FVertexID StartVertexID, const FVertexID EndVertexID, bool& bOutEdgeWindingIsReversed ) const
{
	const FEdgeID EdgeID = GetMeshDescription()->GetVertexPairEdge( StartVertexID, EndVertexID );
	if( EdgeID != FEdgeID::Invalid )
	{
		// @note: seems like a reversal of logic here, but everything else expects this behavior now
		bOutEdgeWindingIsReversed = ( GetMeshDescription()->GetEdgeVertex( EdgeID, 0 ) == StartVertexID );
	}

	return EdgeID;
}


void UEditableMesh::GetPolygonPerimeterEdges( const FPolygonID PolygonID, TArray<FEdgeID>& OutPolygonPerimeterEdgeIDs ) const
{
	const FMeshDescription* Description = GetMeshDescription();
	const TArray<FVertexInstanceID>& VertexInstanceIDs = Description->GetPolygonPerimeterVertexInstances( PolygonID );
	const int32 NumContourEdges = VertexInstanceIDs.Num();

	OutPolygonPerimeterEdgeIDs.SetNumUninitialized( NumContourEdges, false );
	for( int32 Index = 0; Index < NumContourEdges; ++Index )
	{
		const int32 NextIndex = ( Index + 1 == NumContourEdges ) ? 0 : Index + 1;
		const FVertexID VertexID0 = Description->GetVertexInstanceVertex( VertexInstanceIDs[ Index ] );
		const FVertexID VertexID1 = Description->GetVertexInstanceVertex( VertexInstanceIDs[ NextIndex ] );
		OutPolygonPerimeterEdgeIDs[ Index ] = Description->GetVertexPairEdge( VertexID0, VertexID1 );
	}
}


void UEditableMesh::GetPolygonAdjacentPolygons( const FPolygonID PolygonID, TArray<FPolygonID>& OutAdjacentPolygons ) const
{
	OutAdjacentPolygons.Reset();

	static TArray<FEdgeID> PolygonPerimeterEdges;
	GetPolygonPerimeterEdges( PolygonID, PolygonPerimeterEdges );

	for( const FEdgeID EdgeID : PolygonPerimeterEdges )
	{
		static TArray<FPolygonID> EdgeConnectedPolygons;
		GetEdgeConnectedPolygons( EdgeID, EdgeConnectedPolygons );

		for( const FPolygonID EdgeConnectedPolygon : EdgeConnectedPolygons )
		{
			if( EdgeConnectedPolygon != PolygonID )
			{
				OutAdjacentPolygons.AddUnique( EdgeConnectedPolygon );
			}
		}
	}
}


FBox UEditableMesh::ComputeBoundingBox() const
{
	FBox BoundingBox;
	BoundingBox.Init();

	TVertexAttributesConstRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	for( const FVertexID VertexID : GetMeshDescription()->Vertices().GetElementIDs() )
	{
		BoundingBox += VertexPositions[ VertexID ];
	}

	return BoundingBox;
}


FBoxSphereBounds UEditableMesh::ComputeBoundingBoxAndSphere() const
{
	const FBox BoundingBox = ComputeBoundingBox();

	FBoxSphereBounds BoundingBoxAndSphere;
	BoundingBox.GetCenterAndExtents( /* Out */ BoundingBoxAndSphere.Origin, /* Out */ BoundingBoxAndSphere.BoxExtent );

	// Calculate the bounding sphere, using the center of the bounding box as the origin.
	BoundingBoxAndSphere.SphereRadius = 0.0f;

	TVertexAttributesConstRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	for( const FVertexID VertexID : GetMeshDescription()->Vertices().GetElementIDs() )
	{
		BoundingBoxAndSphere.SphereRadius = FMath::Max( ( VertexPositions[ VertexID ] - BoundingBoxAndSphere.Origin ).Size(), BoundingBoxAndSphere.SphereRadius );
	}

	return BoundingBoxAndSphere;
}


FVector UEditableMesh::ComputePolygonCenter( const FPolygonID PolygonID ) const
{
	FVector Centroid = FVector::ZeroVector;

	static TArray<FVertexID> PerimeterVertexIDs;
	GetPolygonPerimeterVertices( PolygonID, /* Out */ PerimeterVertexIDs );

	TVertexAttributesConstRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	for( const FVertexID VertexID : PerimeterVertexIDs )
	{
		Centroid += VertexPositions[ VertexID ];
	}

	return Centroid / (float)PerimeterVertexIDs.Num();
}


FPlane UEditableMesh::ComputePolygonPlane( const FPolygonID PolygonID ) const
{
	// NOTE: This polygon plane computation code is partially based on the implementation of "Newell's method" from Real-Time 
	//       Collision Detection by Christer Ericson, published by Morgan Kaufmann Publishers, (c) 2005 Elsevier Inc

	// @todo mesheditor perf: For polygons that are just triangles, use a cross product to get the normal fast!
	// @todo mesheditor perf: We could skip computing the plane distance when we only need the normal
	// @todo mesheditor perf: We could cache these computed polygon normals; or just use the normal of the first three vertices' triangle if it is satisfactory in all cases
	// @todo mesheditor: For non-planar polygons, the result can vary. Ideally this should use the actual polygon triangulation as opposed to the arbitrary triangulation used here.

	FVector Centroid = FVector::ZeroVector;
	FVector Normal = FVector::ZeroVector;

	static TArray<FVertexID> PerimeterVertexIDs;
	GetPolygonPerimeterVertices( PolygonID, /* Out */ PerimeterVertexIDs );

	TVertexAttributesConstRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	// Use 'Newell's Method' to compute a robust 'best fit' plane from the vertices of this polygon
	for( int32 VertexNumberI = PerimeterVertexIDs.Num() - 1, VertexNumberJ = 0; VertexNumberJ < PerimeterVertexIDs.Num(); VertexNumberI = VertexNumberJ, VertexNumberJ++ )
	{
		const FVertexID VertexIDI = PerimeterVertexIDs[ VertexNumberI ];
		const FVector PositionI = VertexPositions[ VertexIDI ];

		const FVertexID VertexIDJ = PerimeterVertexIDs[ VertexNumberJ ];
		const FVector PositionJ = VertexPositions[ VertexIDJ ];

		Centroid += PositionJ;

		Normal.X += ( PositionJ.Y - PositionI.Y ) * ( PositionI.Z + PositionJ.Z );
		Normal.Y += ( PositionJ.Z - PositionI.Z ) * ( PositionI.X + PositionJ.X );
		Normal.Z += ( PositionJ.X - PositionI.X ) * ( PositionI.Y + PositionJ.Y );
	}

	Normal.Normalize();

	// Construct a plane from the normal and centroid
	return FPlane( Normal, FVector::DotProduct( Centroid, Normal ) / (float)PerimeterVertexIDs.Num() );
}


FVector UEditableMesh::ComputePolygonNormal( const FPolygonID PolygonID ) const
{
	// @todo mesheditor: Polygon normals are now computed and cached when changes are made to a polygon.
	// In theory, we can just return that cached value, but we need to check that there is nothing which relies on the value being correct before
	// the cache is updated at the end of a modification.
	const FPlane PolygonPlane = ComputePolygonPlane( PolygonID );
	const FVector PolygonNormal( PolygonPlane.X, PolygonPlane.Y, PolygonPlane.Z );
	return PolygonNormal;
}


#if EDITABLE_MESH_USE_OPENSUBDIV
void UEditableMesh::RefreshOpenSubdiv()
{
	OsdTopologyRefiner.Reset();

	if( SubdivisionCount > 0 )
	{
		// @todo mesheditor subdiv perf: Ideally we give our topology data straight to OSD rather than have it build it from this simple 
		// set of parameters.  This will be much more efficient!  In order to do this, we need to specialize TopologyRefinerFactory 
		// for our mesh (http://graphics.pixar.com/opensubdiv/docs/far_overview.html)
		OpenSubdiv::Far::TopologyDescriptor OsdTopologyDescriptor;
		{
			const int32 VertexArraySize = GetMeshDescription()->Vertices().GetArraySize();
			OsdTopologyDescriptor.numVertices = VertexArraySize;
			OsdTopologyDescriptor.numFaces = GetPolygonCount();

			// NOTE: OpenSubdiv likes weights to be between 0.0 and 10.0, so we'll account for that here
			const float OpenSubdivCreaseWeightMultiplier = 10.0f;

			{
				// Subdivision corner weights
				{
					OsdCornerVertexIndices.Reset();
					OsdCornerWeights.Reset();

					const TVertexAttributesRef<float> VertexCornerSharpnesses = GetMeshDescription()->VertexAttributes().GetAttributesRef<float>( MeshAttribute::Vertex::CornerSharpness );

					for( const FVertexID VertexID : GetMeshDescription()->Vertices().GetElementIDs() )
					{
						const float VertexCornerSharpness = VertexCornerSharpnesses[ VertexID ];
						if( VertexCornerSharpness > SMALL_NUMBER )
						{
							// This vertex is (at least partially) a subdivision corner
							OsdCornerVertexIndices.Add( VertexID.GetValue() );
							OsdCornerWeights.Add( OpenSubdivCreaseWeightMultiplier * VertexCornerSharpness );
						}
					}
				}

				// Edge creases
				{
					OsdCreaseVertexIndexPairs.Reset();
					OsdCreaseWeights.Reset();

					const TEdgeAttributesRef<float> EdgeCreaseSharpnesses = GetMeshDescription()->EdgeAttributes().GetAttributesRef<float>( MeshAttribute::Edge::CreaseSharpness );

					for( const FEdgeID EdgeID : GetMeshDescription()->Edges().GetElementIDs() )
					{
						const float EdgeCreaseSharpness = EdgeCreaseSharpnesses[ EdgeID ];
						if( EdgeCreaseSharpness > SMALL_NUMBER )
						{
							// This edge is (at least partially) creased
							FVertexID EdgeVertexID0, EdgeVertexID1;
							GetEdgeVertices( EdgeID, /* Out */ EdgeVertexID0, /* Out */ EdgeVertexID1 );

							OsdCreaseVertexIndexPairs.Add( EdgeVertexID0.GetValue() );
							OsdCreaseVertexIndexPairs.Add( EdgeVertexID1.GetValue() );
							OsdCreaseWeights.Add( OpenSubdivCreaseWeightMultiplier * EdgeCreaseSharpness );
						}
					}
				}

				OsdNumVerticesPerFace.SetNum( OsdTopologyDescriptor.numFaces, false );
				OsdVertexIndicesPerFace.Reset();
				OsdFVarIndicesPerFace.Reset();

				int32 NextOsdFaceIndex = 0;

				for( const FPolygonID PolygonID : GetMeshDescription()->Polygons().GetElementIDs() )
				{
					static TArray<FVertexID> PerimeterVertexIDs;
					GetPolygonPerimeterVertices( PolygonID, /* Out */ PerimeterVertexIDs );

					const int32 PerimeterVertexCount = PerimeterVertexIDs.Num();
					OsdNumVerticesPerFace[ NextOsdFaceIndex++ ] = PerimeterVertexCount;

					for( int32 PerimeterVertexNumber = 0; PerimeterVertexNumber < PerimeterVertexCount; ++PerimeterVertexNumber )
					{
						const FVertexID PerimeterVertexID = PerimeterVertexIDs[ PerimeterVertexNumber ];

						OsdVertexIndicesPerFace.Add( PerimeterVertexID.GetValue() );
						OsdFVarIndicesPerFace.Add( OsdFVarIndicesPerFace.Num() );
					}
				}

				check( NextOsdFaceIndex == OsdNumVerticesPerFace.Num()  );
				check( OsdVertexIndicesPerFace.Num() == OsdFVarIndicesPerFace.Num() );
			}

			{
				const int32 TotalFVarChannels = 1;
				OsdFVarChannels.SetNum( TotalFVarChannels, false );
				for( int32 FVarChannelNumber = 0; FVarChannelNumber < OsdFVarChannels.Num(); ++FVarChannelNumber )
				{
					FOsdFVarChannel& OsdFVarChannel = OsdFVarChannels[ FVarChannelNumber ];

					// @todo mesheditor subdiv: For now, we'll assuming unique face-varying data for every polygon vertex.  Ideally
					// if we were able to share rendering vertices, we'd want to share face-varying data too.  OpenSubdiv allows
					// you to pass a separate index array for every channel of face-varying data, but that's probably overkill
					// for us.  Instead we just need to reflect what we do with rendering vertices.
					OsdFVarChannel.ValueCount = OsdFVarIndicesPerFace.Num();
					OsdFVarChannel.ValueIndices = OsdFVarIndicesPerFace.GetData();
				}
			}

			OsdTopologyDescriptor.numVertsPerFace = OsdNumVerticesPerFace.GetData();
			OsdTopologyDescriptor.vertIndicesPerFace = OsdVertexIndicesPerFace.GetData();

			OsdTopologyDescriptor.numCreases = OsdCreaseWeights.Num();
			OsdTopologyDescriptor.creaseVertexIndexPairs = OsdCreaseVertexIndexPairs.GetData();
			OsdTopologyDescriptor.creaseWeights = OsdCreaseWeights.GetData();

			OsdTopologyDescriptor.numCorners = OsdCornerWeights.Num();
			OsdTopologyDescriptor.cornerVertexIndices = OsdCornerVertexIndices.GetData();
			OsdTopologyDescriptor.cornerWeights = OsdCornerWeights.GetData();

			OsdTopologyDescriptor.numHoles = 0;
			OsdTopologyDescriptor.holeIndices = nullptr;

			OsdTopologyDescriptor.isLeftHanded = true;

			// Face-varying vertex data.  This maps to our GetPolygonVertexAttribute() calls.
			OsdTopologyDescriptor.numFVarChannels = OsdFVarChannels.Num();
			OsdTopologyDescriptor.fvarChannels = reinterpret_cast<OpenSubdiv::v3_2_0::Far::TopologyDescriptor::FVarChannel*>( OsdFVarChannels.GetData() );
		}

		// We always want Catmull-Clark subdivisions
		const OpenSubdiv::Sdc::SchemeType OsdSchemeType = OpenSubdiv::Sdc::SCHEME_CATMARK;

		OpenSubdiv::Sdc::Options OsdSdcOptions;
		{
			// @todo mesheditor subdiv: Tweak these settings
			OsdSdcOptions.SetVtxBoundaryInterpolation( OpenSubdiv::Sdc::Options::VTX_BOUNDARY_EDGE_ONLY );
			OsdSdcOptions.SetFVarLinearInterpolation( OpenSubdiv::Sdc::Options::FVAR_LINEAR_ALL );
			OsdSdcOptions.SetCreasingMethod( OpenSubdiv::Sdc::Options::CREASE_UNIFORM );
			OsdSdcOptions.SetTriangleSubdivision( OpenSubdiv::Sdc::Options::TRI_SUB_CATMARK );
		}

		OpenSubdiv::Far::TopologyRefinerFactory<OpenSubdiv::Far::TopologyDescriptor>::Options OsdTopologyRefinerOptions( OsdSchemeType, OsdSdcOptions );

		this->OsdTopologyRefiner = MakeShareable(
			OpenSubdiv::Far::TopologyRefinerFactory<OpenSubdiv::Far::TopologyDescriptor>::Create(
				OsdTopologyDescriptor,
				OsdTopologyRefinerOptions ) );

		OpenSubdiv::Far::TopologyRefiner::UniformOptions OsdUniformOptions( SubdivisionCount );
		{
			// @todo mesheditor subdiv: Should we order child vertices from faces instead of child vertices from vertices first?
			OsdUniformOptions.orderVerticesFromFacesFirst = false;

			// NOTE: In order for face-varying data to work, OpenSubdiv requires 'fullTopologyInLastLevel' to be enabled.
			OsdUniformOptions.fullTopologyInLastLevel = true;
		}


		this->OsdTopologyRefiner->RefineUniform( OsdUniformOptions );
	}

	GenerateOpenSubdivLimitSurfaceData();
}
#endif


const FSubdivisionLimitData& UEditableMesh::GetSubdivisionLimitData() const
{
	return SubdivisionLimitData;
}


#if EDITABLE_MESH_USE_OPENSUBDIV
void UEditableMesh::GenerateOpenSubdivLimitSurfaceData()
{
	SubdivisionLimitData = FSubdivisionLimitData();

	if( SubdivisionCount > 0 && ensure( this->OsdTopologyRefiner.IsValid() ) )
	{
		// Create an OpenSubdiv 'primvar refiner'.  This guy allows us to interpolate data between vertices on a subdivision level.
		const OpenSubdiv::Far::PrimvarRefiner OsdPrimvarRefiner( *OsdTopologyRefiner );

		struct FOsdVector
		{
			void Clear( void* UnusedPtr = nullptr )
			{
				Position = FVector( 0, 0, 0 );
			}

			void AddWithWeight( FOsdVector const& SourceVertexPosition, float Weight )
			{
				Position += SourceVertexPosition.Position * Weight;
			}

			FVector Position;
		};

		struct FOsdFVarVertexData
		{
			void Clear()
			{
				TextureCoordinates[0] = TextureCoordinates[1] = FVector2D( 0.0f, 0.0f );
				VertexColor = FLinearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			}

			void AddWithWeight( FOsdFVarVertexData const& SourceVertex, float Weight )
			{
				TextureCoordinates[ 0 ] += SourceVertex.TextureCoordinates[ 0 ] * Weight;
				TextureCoordinates[ 1 ] += SourceVertex.TextureCoordinates[ 1 ] * Weight;
				VertexColor += SourceVertex.VertexColor * Weight;
			}

			FVector2D TextureCoordinates[2];	// @todo mesheditor subdiv perf: Only two UV sets supported for now (just to avoid heap allocs for dynamic array)
			FLinearColor VertexColor;
		};

		const int32 PolygonGroupCount = GetPolygonGroupCount();

		// Get the limit surface subdivision level from OpenSubdiv
		const OpenSubdiv::Far::TopologyLevel& OsdLimitLevel = OsdTopologyRefiner->GetLevel( SubdivisionCount );

		const int32 LimitVertexCount = OsdLimitLevel.GetNumVertices();
		const int32 LimitFaceCount = OsdLimitLevel.GetNumFaces();

		static TArray<FVector> LimitXGradients;
		LimitXGradients.Reset();
		static TArray<FVector> LimitYGradients;
		LimitYGradients.Reset();

		// Grab all of the vertex data and put them in separate contiguous arrays for OpenSubdiv
		static TArray<FVector> VertexPositions;
		static TArray<FOsdFVarVertexData> FVarVertexDatas;
		static TArray<int32> FirstPolygonNumberForPolygonGroups;
		{
			// Vertex positions
			{
				// NOTE: We're including an entry for all vertices, even vertices that aren't referenced by any triangles (due to our
				//       sparse array optimization.)
				const int32 VertexArraySize = GetMeshDescription()->Vertices().GetArraySize();
				VertexPositions.SetNum( VertexArraySize, false );

				// @todo mesheditor perf: note that now we expose vertex positions as an array of FVector, so this is just copying from one array to another.
				// This needs to be refactored to not require this.
				const TVertexAttributesRef<FVector> SrcVertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

				for( int32 VertexNumber = 0; VertexNumber < VertexArraySize; ++VertexNumber )
				{
					const FVertexID VertexID( VertexNumber );
					if( IsValidVertex( VertexID ) )
					{
						VertexPositions[ VertexNumber ] = SrcVertexPositions[ VertexID ];
					}
					else
					{
						// Vertex isn't used, but we'll include a zero'd entry so that our indices still match up.
						VertexPositions[ VertexNumber ] = FVector::ZeroVector;
					}
				}
			}

			// Texture coordinates (per polygon vertex)
			FirstPolygonNumberForPolygonGroups.Reset();

			{
				FVarVertexDatas.Reset();
				FVarVertexDatas.Reserve( OsdFVarIndicesPerFace.Num() );

				const TVertexInstanceAttributesRef<FVector2D> VertexUVs = GetMeshDescription()->VertexInstanceAttributes().GetAttributesRef<FVector2D>( MeshAttribute::VertexInstance::TextureCoordinate );
				const TVertexInstanceAttributesRef<FVector4> VertexColors = GetMeshDescription()->VertexInstanceAttributes().GetAttributesRef<FVector4>( MeshAttribute::VertexInstance::Color );

				int32 NumPolygonsSoFar = 0;
				for( const FPolygonGroupID PolygonGroupID : GetMeshDescription()->PolygonGroups().GetElementIDs() )
				{
					FirstPolygonNumberForPolygonGroups.Add( NumPolygonsSoFar );

					const TArray<FPolygonID>& PolygonIDs = GetMeshDescription()->GetPolygonGroupPolygons( PolygonGroupID );
					NumPolygonsSoFar += PolygonIDs.Num();

					for( const FPolygonID PolygonID : PolygonIDs )
					{
						for( const FVertexInstanceID VertexInstanceID : GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID ) )
						{
							FOsdFVarVertexData& FVarVertexData = *new( FVarVertexDatas ) FOsdFVarVertexData();
							FVarVertexData.TextureCoordinates[ 0 ] = TextureCoordinateCount > 0 ? VertexUVs.Get( VertexInstanceID, 0 ) : FVector2D::ZeroVector;
							FVarVertexData.TextureCoordinates[ 1 ] = TextureCoordinateCount > 1 ? VertexUVs.Get( VertexInstanceID, 1 ) : FVector2D::ZeroVector;
							FVarVertexData.VertexColor = FLinearColor( VertexColors[ VertexInstanceID ] );
						}
					}
				}

				check( FVarVertexDatas.Num() == OsdVertexIndicesPerFace.Num() );
			}
		}

		// @todo mesheditor subdiv debug
// 		for( int32 VertexIndex = 0; VertexIndex < FVarVertexDatas.Num(); ++VertexIndex )
// 		{
// 			const FOsdFVarVertexData& FVarVertexData = FVarVertexDatas[ VertexIndex ];
// 			GWarn->Logf( TEXT( "FVar%i, U:%0.2f, V:%0.2f" ), VertexIndex, FVarVertexData.TextureCoordinates[0].X, FVarVertexData.TextureCoordinates[0].Y );
// 		}

		static TArray<FOsdFVarVertexData> LimitFVarVertexDatas;
		LimitFVarVertexDatas.Reset();


		// Start with the base cage geometry, and refine the geometry until we get to the limit surface
		{
			// NOTE: The OsdVertexPositions list might contains vertices that aren't actually referenced by any polygons (due to our
			//       sparse array optimization.)  That's OK though.
			{
				int32 NextScratchBufferIndex = 0;
				static TArray<FVector> ScratchVertexPositions[ 2 ];
				ScratchVertexPositions[ 0 ].Reset();
				ScratchVertexPositions[ 1 ].Reset();

				for( int32 RefinementLevel = 1; RefinementLevel <= SubdivisionCount; ++RefinementLevel )
				{
					const OpenSubdiv::Far::TopologyLevel& OsdLevel = OsdTopologyRefiner->GetLevel( RefinementLevel );

					// For the last refinement level, we'll copy positions straight to our output buffer (to avoid having to copy the data later.)
					// For earlier levels, we'll ping-pong between scratch buffers.
					const TArray<FVector>& SourceVertexPositions = ( RefinementLevel == 1 ) ? VertexPositions : ScratchVertexPositions[ !NextScratchBufferIndex ];
					check( SourceVertexPositions.Num() == OsdTopologyRefiner->GetLevel( RefinementLevel - 1 ).GetNumVertices() );

					TArray<FVector>& DestVertexPositions = ScratchVertexPositions[ NextScratchBufferIndex ];
					DestVertexPositions.SetNumUninitialized( OsdLevel.GetNumVertices(), false );

					FOsdVector* OsdDestVertexPositions = reinterpret_cast<FOsdVector*>( DestVertexPositions.GetData() );
					OsdPrimvarRefiner.Interpolate( 
						RefinementLevel, 
						reinterpret_cast<const FOsdVector*>( SourceVertexPositions.GetData() ),
						OsdDestVertexPositions );

					NextScratchBufferIndex = !NextScratchBufferIndex;
				}

				// @todo mesheditor subdiv perf: We should probably be using stencils for faster updates (unless topology is changing every frame too)

				// We've generated interpolated positions for the most fine subdivision level, but now we need to compute the positions
				// on the limit surface.  While doing this, we also compute gradients at every vertex for either surface axis
				{
					const FOsdVector* OsdSourceLimitPositions = reinterpret_cast<FOsdVector*>( ScratchVertexPositions[ !NextScratchBufferIndex ].GetData() );
					SubdivisionLimitData.VertexPositions.SetNumUninitialized( LimitVertexCount );
					FOsdVector* OsdDestLimitPositions = reinterpret_cast<FOsdVector*>( SubdivisionLimitData.VertexPositions.GetData() );

					LimitXGradients.SetNumUninitialized( LimitVertexCount );
					FOsdVector* OsdDestLimitXGradients = reinterpret_cast<FOsdVector*>( LimitXGradients.GetData() );

					LimitYGradients.SetNumUninitialized( LimitVertexCount );
					FOsdVector* OsdDestLimitYGradients = reinterpret_cast<FOsdVector*>( LimitYGradients.GetData() );

					OsdPrimvarRefiner.Limit( OsdSourceLimitPositions, /* Out */ OsdDestLimitPositions, /* Out */ OsdDestLimitXGradients, /* Out */ OsdDestLimitYGradients );

					if( EditableMesh::InterpolatePositionsToLimit->GetInt() == 0 )
					{
						SubdivisionLimitData.VertexPositions = ScratchVertexPositions[ !NextScratchBufferIndex ];
					}
				}

				check( LimitVertexCount == SubdivisionLimitData.VertexPositions.Num() );
			}

			{
				const int32 FVarChannelNumber = 0;

				static TArray<FOsdFVarVertexData> ScratchFVarVertexDatas[ 2 ];
				ScratchFVarVertexDatas[ 0 ].Reset();
				ScratchFVarVertexDatas[ 1 ].Reset();

				int32 NextScratchBufferIndex = 0;
				for( int32 RefinementLevel = 1; RefinementLevel <= SubdivisionCount; ++RefinementLevel )
				{
					const OpenSubdiv::Far::TopologyLevel& OsdLevel = OsdTopologyRefiner->GetLevel( RefinementLevel );

					const TArray<FOsdFVarVertexData>& SourceFVarVertexDatas = ( RefinementLevel == 1 ) ? FVarVertexDatas : ScratchFVarVertexDatas[ !NextScratchBufferIndex ];
					check( SourceFVarVertexDatas.Num() == OsdTopologyRefiner->GetLevel( RefinementLevel - 1 ).GetNumFVarValues( FVarChannelNumber ) );

					TArray<FOsdFVarVertexData>& DestFVarVertexDatas = ScratchFVarVertexDatas[ NextScratchBufferIndex ];
					DestFVarVertexDatas.SetNumUninitialized( OsdLevel.GetNumFVarValues( FVarChannelNumber ), false );

					FOsdFVarVertexData* DestFVarVertexDatasPtr = DestFVarVertexDatas.GetData();
					OsdPrimvarRefiner.InterpolateFaceVarying(
						RefinementLevel,
						SourceFVarVertexDatas.GetData(),
						DestFVarVertexDatasPtr,
						FVarChannelNumber );

					NextScratchBufferIndex = !NextScratchBufferIndex;
				}
	
				if( EditableMesh::InterpolateFVarsToLimit->GetInt() != 0 )
				{
					LimitFVarVertexDatas.SetNumUninitialized( OsdLimitLevel.GetNumFVarValues( FVarChannelNumber ), false );
					FOsdFVarVertexData* DestFVarVertexDatasPtr = LimitFVarVertexDatas.GetData();
					OsdPrimvarRefiner.LimitFaceVarying(	// @todo mesheditor subdiv fvars: The OSD tutorials don't bother doing a Limit pass for UVs/Colors...
						ScratchFVarVertexDatas[ !NextScratchBufferIndex ].GetData(),
						DestFVarVertexDatasPtr,
						FVarChannelNumber );
				}
				else
				{
					LimitFVarVertexDatas = ScratchFVarVertexDatas[ !NextScratchBufferIndex ];
				}
			}
		}

		SubdivisionLimitData.Sections.SetNum( PolygonGroupCount, false );

		for( int32 LimitFaceNumber = 0; LimitFaceNumber < LimitFaceCount; ++LimitFaceNumber )
		{
			const OpenSubdiv::Far::ConstIndexArray& OsdLimitFaceVertices = OsdLimitLevel.GetFaceVertices( LimitFaceNumber );
			const int32 FaceVertexCount = OsdLimitFaceVertices.size();
			check( FaceVertexCount == 4 );	// We're always expecting quads as the result of a Catmull-Clark subdivision

			// Find the parent face in our original control mesh for this subdivided quad.  We'll use this to
			// determine which section the face belongs to
			// @todo mesheditor subdiv perf: We can use InterpolateFaceUniform() to push section indices down to the subdivided faces. More memory, might be slower (copies), but avoids iterating here.
			int32 QuadSectionNumber = 0;
			{
				int32 CurrentFaceNumber = LimitFaceNumber;
				for( int32 SubdivisionLevel = SubdivisionCount; SubdivisionLevel > 0; --SubdivisionLevel )
				{
					const OpenSubdiv::Far::TopologyLevel& OsdLevel = OsdTopologyRefiner->GetLevel( SubdivisionLevel );
					const int32 ParentLevelFace = OsdLevel.GetFaceParentFace( CurrentFaceNumber );
					CurrentFaceNumber = ParentLevelFace;
				}
				int32 BaseCageFaceNumber = CurrentFaceNumber;

				for( int32 PolygonGroupNumber = PolygonGroupCount - 1; PolygonGroupNumber >= 0; --PolygonGroupNumber)
				{
					if( BaseCageFaceNumber >= FirstPolygonNumberForPolygonGroups[ PolygonGroupNumber ] )
					{
						QuadSectionNumber = PolygonGroupNumber;
						break;
					}
				}
			}

			const int32 FVarChannelNumber = 0;
			const OpenSubdiv::Far::ConstIndexArray& OsdLimitFaceFVarValues = OsdLimitLevel.GetFaceFVarValues( LimitFaceNumber, FVarChannelNumber );
			check( OsdLimitFaceFVarValues.size() == 4 );	// Expecting quads

			FSubdivisionLimitSection& SubdivisionSection = SubdivisionLimitData.Sections[ QuadSectionNumber ];
			FSubdividedQuad& SubdividedQuad = *new( SubdivisionSection.SubdividedQuads ) FSubdividedQuad();
			for( int32 FaceVertexNumber = 0; FaceVertexNumber < FaceVertexCount; ++FaceVertexNumber )
			{
				FSubdividedQuadVertex& QuadVertex = SubdividedQuad.AccessQuadVertex( FaceVertexNumber );

				QuadVertex.VertexPositionIndex = OsdLimitFaceVertices[ FaceVertexNumber ];

				const int OsdLimitFaceFVarIndex = OsdLimitFaceFVarValues[ FaceVertexNumber ];
				const FOsdFVarVertexData& FVarVertexData = LimitFVarVertexDatas[ OsdLimitFaceFVarIndex ];

				QuadVertex.TextureCoordinate0 = FVarVertexData.TextureCoordinates[ 0 ];
				QuadVertex.TextureCoordinate1 = FVarVertexData.TextureCoordinates[ 1 ];

				QuadVertex.VertexColor = FVarVertexData.VertexColor.ToFColor( true );

				QuadVertex.VertexNormal = FVector::CrossProduct( LimitXGradients[ QuadVertex.VertexPositionIndex ].GetSafeNormal(), LimitYGradients[ QuadVertex.VertexPositionIndex ].GetSafeNormal() );
				
				// NOTE: Tangents will be computed separately, below
			}
		}

		// Compute normal and tangent vectors for each quad vertex, taking into account the texture coordinates
		for( int32 SectionNumber = 0; SectionNumber < SubdivisionLimitData.Sections.Num(); ++SectionNumber )
		{
			struct FMikkUserData
			{
				FSubdivisionLimitData* LimitData;
				int32 SectionNumber;

				FMikkUserData( FSubdivisionLimitData* InitLimitData, const int32 InitSectionNumber )
					: LimitData( InitLimitData ),
						SectionNumber( InitSectionNumber )
				{
				}
			} MikkUserData( &SubdivisionLimitData, SectionNumber );

			struct Local
			{
				static int MikkGetNumFaces( const SMikkTSpaceContext* Context )
				{
					const FMikkUserData& UserData = *static_cast<FMikkUserData*>( Context->m_pUserData );
					return UserData.LimitData->Sections[ UserData.SectionNumber ].SubdividedQuads.Num();
				}

				static int MikkGetNumVertsOfFace( const SMikkTSpaceContext* Context, const int MikkFaceIndex )
				{
					// Always quads
					return 4;
				}

				static void MikkGetPosition( const SMikkTSpaceContext* Context, float OutPosition[ 3 ], const int MikkFaceIndex, const int MikkVertexIndex )
				{
					const FMikkUserData& UserData = *static_cast<FMikkUserData*>( Context->m_pUserData );
					const FSubdividedQuadVertex& QuadVertex = UserData.LimitData->Sections[ UserData.SectionNumber ].SubdividedQuads[ MikkFaceIndex ].GetQuadVertex( MikkVertexIndex );
					const FVector VertexPosition = UserData.LimitData->VertexPositions[ QuadVertex.VertexPositionIndex ];

					OutPosition[ 0 ] = VertexPosition.X;
					OutPosition[ 1 ] = VertexPosition.Y;
					OutPosition[ 2 ] = VertexPosition.Z;
				}

				static void MikkGetNormal( const SMikkTSpaceContext* Context, float OutNormal[ 3 ], const int MikkFaceIndex, const int MikkVertexIndex )
				{
					const FMikkUserData& UserData = *static_cast<FMikkUserData*>( Context->m_pUserData );
					const FSubdividedQuadVertex& QuadVertex = UserData.LimitData->Sections[ UserData.SectionNumber ].SubdividedQuads[ MikkFaceIndex ].GetQuadVertex( MikkVertexIndex );

					OutNormal[ 0 ] = QuadVertex.VertexNormal.X;
					OutNormal[ 1 ] = QuadVertex.VertexNormal.Y;
					OutNormal[ 2 ] = QuadVertex.VertexNormal.Z;
				}

				static void MikkGetTexCoord( const SMikkTSpaceContext* Context, float OutUV[ 2 ], const int MikkFaceIndex, const int MikkVertexIndex )
				{
					const FMikkUserData& UserData = *static_cast<FMikkUserData*>( Context->m_pUserData );
					const FSubdividedQuadVertex& QuadVertex = UserData.LimitData->Sections[ UserData.SectionNumber ].SubdividedQuads[ MikkFaceIndex ].GetQuadVertex( MikkVertexIndex );

					// @todo mesheditor: Support using a custom texture coordinate index for tangent space generation?
					OutUV[ 0 ] = QuadVertex.TextureCoordinate0.X;
					OutUV[ 1 ] = QuadVertex.TextureCoordinate0.Y;
				}

				static void MikkSetTSpaceBasic( const SMikkTSpaceContext* Context, const float Tangent[ 3 ], const float BitangentSign, const int MikkFaceIndex, const int MikkVertexIndex )
				{
					FMikkUserData& UserData = *static_cast<FMikkUserData*>( Context->m_pUserData );
					FSubdividedQuadVertex& QuadVertex = UserData.LimitData->Sections[ UserData.SectionNumber ].SubdividedQuads[ MikkFaceIndex ].AccessQuadVertex( MikkVertexIndex );

					QuadVertex.VertexTangent = FVector( Tangent[ 0 ], Tangent[ 1 ], Tangent[ 2 ] );
					QuadVertex.VertexBinormalSign = BitangentSign;
				}
			};

			SMikkTSpaceInterface MikkTInterface;
			{
				MikkTInterface.m_getNumFaces = Local::MikkGetNumFaces;
				MikkTInterface.m_getNumVerticesOfFace = Local::MikkGetNumVertsOfFace;
				MikkTInterface.m_getPosition = Local::MikkGetPosition;
				MikkTInterface.m_getNormal = Local::MikkGetNormal;
				MikkTInterface.m_getTexCoord = Local::MikkGetTexCoord;

				MikkTInterface.m_setTSpaceBasic = Local::MikkSetTSpaceBasic;
				MikkTInterface.m_setTSpace = nullptr;
			}

			SMikkTSpaceContext MikkTContext;
			{
				MikkTContext.m_pInterface = &MikkTInterface;
				MikkTContext.m_pUserData = (void*)( &MikkUserData );
				MikkTContext.m_bIgnoreDegenerates = true;	// No degenerates in our list
			}

			// Now we'll ask MikkTSpace to actually generate the tangents
			genTangSpaceDefault( &MikkTContext );
		}


		// Generate our edge information for the subdivided mesh.  We'll also figure out which subdivided edges have a 
		// counterpart on the base cage mesh, so tools can display this information to the user.
		{
			const int32 LimitEdgeCount = OsdLimitLevel.GetNumEdges();
			for( int32 LimitEdgeNumber = 0; LimitEdgeNumber < LimitEdgeCount; ++LimitEdgeNumber )
			{
				const OpenSubdiv::Far::ConstIndexArray& OsdLimitEdgeVertices = OsdLimitLevel.GetEdgeVertices( LimitEdgeNumber );
				const int32 EdgeVertexCount = OsdLimitEdgeVertices.size();
				check( EdgeVertexCount == 2 );	// Edges always connect two vertices

				FSubdividedWireEdge& SubdividedWireEdge = *new( SubdivisionLimitData.SubdividedWireEdges ) FSubdividedWireEdge();

				SubdividedWireEdge.EdgeVertex0PositionIndex = OsdLimitEdgeVertices[ 0 ];
				SubdividedWireEdge.EdgeVertex1PositionIndex = OsdLimitEdgeVertices[ 1 ];

				// Default to not highlighting this edge as a base cage counterpart.  We'll actually figure this out below.
				SubdividedWireEdge.CounterpartEdgeID = FEdgeID::Invalid;
			}

			{
				static TSet<int32> BaseCageEdgeSet;
				BaseCageEdgeSet.Reset();

				const OpenSubdiv::Far::TopologyLevel& OsdBaseCageLevel = OsdTopologyRefiner->GetLevel( 0 );
				const int32 BaseCageFaceCount = OsdBaseCageLevel.GetNumFaces();
				for( int32 BaseCageFaceNumber = 0; BaseCageFaceNumber < BaseCageFaceCount; ++BaseCageFaceNumber )
				{
					const OpenSubdiv::Far::ConstIndexArray& OsdBaseCageFaceEdges = OsdBaseCageLevel.GetFaceEdges( BaseCageFaceNumber );
					for( int32 FaceEdgeNumber = 0; FaceEdgeNumber < OsdBaseCageFaceEdges.size(); ++FaceEdgeNumber )
					{
						const int32 BaseCageEdgeIndex = OsdBaseCageFaceEdges[ FaceEdgeNumber ];
						bool bIsAlreadyInSet = false;
						BaseCageEdgeSet.Add( BaseCageEdgeIndex, /* Out */ &bIsAlreadyInSet );
						if( !bIsAlreadyInSet )
						{
							// Find our original edge ID for each of the OpenSubdiv base cage edges
							const OpenSubdiv::Far::ConstIndexArray& OsdEdgeVertices = OsdBaseCageLevel.GetEdgeVertices( BaseCageEdgeIndex );
							check( OsdEdgeVertices.size() == 2 ); // Edges always connect two vertices
																	// Figure out which edge goes with these vertices
							const FEdgeID BaseCageEdgeID = GetEdgeThatConnectsVertices( FVertexID( OsdEdgeVertices[ 0 ] ), FVertexID( OsdEdgeVertices[ 1 ] ) );

							// Go through and determine the limit child edges of all of the original base cage edges by drilling down through
							// the subdivision hierarchy
							int32 NextScratchBufferIndex = 0;
							static TArray<int32> ScratchChildEdges[ 2 ];
							ScratchChildEdges[ 0 ].Reset();
							ScratchChildEdges[ 1 ].Reset();

							// Fill in our source buffer with the starting edge
							ScratchChildEdges[ NextScratchBufferIndex ].Add( BaseCageEdgeIndex );
							NextScratchBufferIndex = !NextScratchBufferIndex;

							for( int32 RefinementLevel = 0; RefinementLevel < SubdivisionCount; ++RefinementLevel )
							{
								const OpenSubdiv::Far::TopologyLevel& OsdLevel = OsdTopologyRefiner->GetLevel( RefinementLevel );

								const TArray<int32>& SourceChildEdges = ScratchChildEdges[ !NextScratchBufferIndex ];

								TArray<int32>& DestChildEdges = ScratchChildEdges[ NextScratchBufferIndex ];
								DestChildEdges.Reset();

								for( int32 SourceEdgeNumber = 0; SourceEdgeNumber < SourceChildEdges.Num(); ++SourceEdgeNumber )
								{
									const OpenSubdiv::Far::ConstIndexArray& OsdChildEdges = OsdLevel.GetEdgeChildEdges( SourceChildEdges[ SourceEdgeNumber ] );
									for( int32 ChildEdgeNumber = 0; ChildEdgeNumber < OsdChildEdges.size(); ++ChildEdgeNumber )
									{
										DestChildEdges.Add( OsdChildEdges[ ChildEdgeNumber ] );
									}
								}

								NextScratchBufferIndex = !NextScratchBufferIndex;
							}

							// No go back and update our subdivided wire edges, marking the edges that we determined were descendants of the base cage edges
							const TArray<int32>& BaseCageCounterpartEdgesAtLimit = ScratchChildEdges[ !NextScratchBufferIndex ];
							for( const int32 CounterpartEdgeAtLimit : BaseCageCounterpartEdgesAtLimit )
							{
								check( CounterpartEdgeAtLimit < SubdivisionLimitData.SubdividedWireEdges.Num() );
								SubdivisionLimitData.SubdividedWireEdges[ CounterpartEdgeAtLimit ].CounterpartEdgeID = BaseCageEdgeID;
							}
						}
					}
				}
			}
		}
	}
}
#endif


void UEditableMesh::RetriangulatePolygons()
{
	FPolygonArray& Polygons = GetMeshDescription()->Polygons();

	// Perform triangulation directly into mesh polygons
	for( const FPolygonID PolygonID : PolygonsPendingTriangulation )
	{
		FMeshPolygon& Polygon = Polygons[ PolygonID ];
		ComputePolygonTriangulation( PolygonID, Polygon.Triangles );
	}

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnRetriangulatePolygons( this, PolygonsPendingTriangulation.Array() );
	}
}


void UEditableMesh::ComputePolygonTriangulation( const FPolygonID PolygonID, TArray<FMeshTriangle>& OutTriangles ) const
{
	// NOTE: This polygon triangulation code is partially based on the ear cutting algorithm described on
	//       page 497 of the book "Real-time Collision Detection", published in 2005.

	struct Local
	{
		// Returns true if the triangle formed by the specified three positions has a normal that is facing the opposite direction of the reference normal
		static inline bool IsTriangleFlipped( const FVector ReferenceNormal, const FVector VertexPositionA, const FVector VertexPositionB, const FVector VertexPositionC )
		{
			const FVector TriangleNormal = FVector::CrossProduct(
				VertexPositionC - VertexPositionA,
				VertexPositionB - VertexPositionA ).GetSafeNormal();
			return ( FVector::DotProduct( ReferenceNormal, TriangleNormal ) <= 0.0f );
		}

	};


	OutTriangles.Reset();

	// @todo mesheditor: Perhaps should always attempt to triangulate by splitting polygons along the shortest edge, for better determinism.

//	const FMeshPolygon& Polygon = GetMeshDescription()->GetPolygon( PolygonID );
	const TArray<FVertexInstanceID>& PolygonVertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID );

	// Polygon must have at least three vertices/edges
	const int32 PolygonVertexCount = PolygonVertexInstanceIDs.Num();
	check( PolygonVertexCount >= 3 );

	// First figure out the polygon normal.  We need this to determine which triangles are convex, so that
	// we can figure out which ears to clip
	const FVector PolygonNormal = ComputePolygonNormal( PolygonID );

	// Make a simple linked list array of the previous and next vertex numbers, for each vertex number
	// in the polygon.  This will just save us having to iterate later on.
	static TArray<int32> PrevVertexNumbers, NextVertexNumbers;
	static TArray<FVector> VertexPositions;

	{
		TVertexAttributesConstRef<FVector> MeshVertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );
		PrevVertexNumbers.SetNumUninitialized( PolygonVertexCount, false );
		NextVertexNumbers.SetNumUninitialized( PolygonVertexCount, false );
		VertexPositions.SetNumUninitialized( PolygonVertexCount, false );

		for( int32 VertexNumber = 0; VertexNumber < PolygonVertexCount; ++VertexNumber )
		{
			PrevVertexNumbers[ VertexNumber ] = VertexNumber - 1;
			NextVertexNumbers[ VertexNumber ] = VertexNumber + 1;

			VertexPositions[ VertexNumber ] = MeshVertexPositions[ GetMeshDescription()->GetVertexInstanceVertex( PolygonVertexInstanceIDs[ VertexNumber ] ) ];
		}
		PrevVertexNumbers[ 0 ] = PolygonVertexCount - 1;
		NextVertexNumbers[ PolygonVertexCount - 1 ] = 0;
	}

	int32 EarVertexNumber = 0;
	int32 EarTestCount = 0;
	for( int32 RemainingVertexCount = PolygonVertexCount; RemainingVertexCount >= 3; )
	{
		bool bIsEar = true;

		// If we're down to only a triangle, just treat it as an ear.  Also, if we've tried every possible candidate
		// vertex looking for an ear, go ahead and just treat the current vertex as an ear.  This can happen when 
		// vertices are colinear or other degenerate cases.
		if( RemainingVertexCount > 3 && EarTestCount < RemainingVertexCount )
		{
			const FVector PrevVertexPosition = VertexPositions[ PrevVertexNumbers[ EarVertexNumber ] ];
			const FVector EarVertexPosition = VertexPositions[ EarVertexNumber ];
			const FVector NextVertexPosition = VertexPositions[ NextVertexNumbers[ EarVertexNumber ] ];

			// Figure out whether the potential ear triangle is facing the same direction as the polygon
			// itself.  If it's facing the opposite direction, then we're dealing with a concave triangle
			// and we'll skip it for now.
			if( !Local::IsTriangleFlipped(
					PolygonNormal,
					PrevVertexPosition,
					EarVertexPosition,
					NextVertexPosition ) )
			{
				int32 TestVertexNumber = NextVertexNumbers[ NextVertexNumbers[ EarVertexNumber ] ];

				do 
				{
					// Test every other remaining vertex to make sure that it doesn't lie inside our potential ear
					// triangle.  If we find a vertex that's inside the triangle, then it cannot actually be an ear.
					const FVector TestVertexPosition = VertexPositions[ TestVertexNumber ];
					if( FGeomTools::PointInTriangle(
							PrevVertexPosition,
							EarVertexPosition,
							NextVertexPosition,
							TestVertexPosition,
							SMALL_NUMBER ) )
					{
						bIsEar = false;
						break;
					}

					TestVertexNumber = NextVertexNumbers[ TestVertexNumber ];
				} 
				while( TestVertexNumber != PrevVertexNumbers[ EarVertexNumber ] );
			}
			else
			{
				bIsEar = false;
			}
		}

		if( bIsEar )
		{
			// OK, we found an ear!  Let's save this triangle in our output buffer.
			{
				OutTriangles.Emplace();
				FMeshTriangle& Triangle = OutTriangles.Last();

				Triangle.SetVertexInstanceID( 0, PolygonVertexInstanceIDs[ PrevVertexNumbers[ EarVertexNumber ] ] );
				Triangle.SetVertexInstanceID( 1, PolygonVertexInstanceIDs[ EarVertexNumber ] );
				Triangle.SetVertexInstanceID( 2, PolygonVertexInstanceIDs[ NextVertexNumbers[ EarVertexNumber ] ] );
			}

			// Update our linked list.  We're effectively cutting off the ear by pointing the ear vertex's neighbors to
			// point at their next sequential neighbor, and reducing the remaining vertex count by one.
			{
				NextVertexNumbers[ PrevVertexNumbers[ EarVertexNumber ] ] = NextVertexNumbers[ EarVertexNumber ];
				PrevVertexNumbers[ NextVertexNumbers[ EarVertexNumber ] ] = PrevVertexNumbers[ EarVertexNumber ];
				--RemainingVertexCount;
			}

			// Move on to the previous vertex in the list, now that this vertex was cut
			EarVertexNumber = PrevVertexNumbers[ EarVertexNumber ];

			EarTestCount = 0;
		}
		else
		{
			// The vertex is not the ear vertex, because it formed a triangle that either had a normal which pointed in the opposite direction
			// of the polygon, or at least one of the other polygon vertices was found to be inside the triangle.  Move on to the next vertex.
			EarVertexNumber = NextVertexNumbers[ EarVertexNumber ];

			// Keep track of how many ear vertices we've tested, so that if we exhaust all remaining vertices, we can
			// fall back to clipping the triangle and adding it to our mesh anyway.  This is important for degenerate cases.
			++EarTestCount;
		}
	}

	check( OutTriangles.Num() > 0 );
}


bool UEditableMesh::ComputeBarycentricWeightForPointOnPolygon( const FPolygonID PolygonID, const FVector PointOnPolygon, FMeshTriangle& OutTriangle, FVector& OutTriangleVertexWeights ) const
{
	// @todo mesheditor: Modify this method so it can return a meaningful barycentric weight for an off-polygon position, referencing the closest triangle, with out-of-range weights.

	const FMeshPolygon& Polygon = GetMeshDescription()->GetPolygon( PolygonID );
	const int32 TriangleCount = Polygon.Triangles.Num();

	TVertexAttributesConstRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	// Figure out which triangle the incoming point is within
	for( const FMeshTriangle& Triangle : Polygon.Triangles )
	{
		const FVector TriangleVertex0Position = VertexPositions[ GetMeshDescription()->GetVertexInstanceVertex( Triangle.VertexInstanceID0 ) ];
		const FVector TriangleVertex1Position = VertexPositions[ GetMeshDescription()->GetVertexInstanceVertex( Triangle.VertexInstanceID1 ) ];
		const FVector TriangleVertex2Position = VertexPositions[ GetMeshDescription()->GetVertexInstanceVertex( Triangle.VertexInstanceID2 ) ];

		// Calculate the barycentric weights for the triangle's verts and determine if the point lies within its bounds.
		OutTriangleVertexWeights = FMath::ComputeBaryCentric2D( PointOnPolygon, TriangleVertex0Position, TriangleVertex1Position, TriangleVertex2Position );

		if( OutTriangleVertexWeights.X >= 0.0f && OutTriangleVertexWeights.Y >= 0.0f && OutTriangleVertexWeights.Z >= 0.0f )
		{
			// Okay, we found a triangle that the point is inside!  Return the corresponding vertex instances.
			OutTriangle = Triangle;
			return true;
		}
	}

	return false;
}


void UEditableMesh::SetSubdivisionCount( const int32 NewSubdivisionCount )
{
#if EDITABLE_MESH_USE_OPENSUBDIV
	// @todo mesheditor subdiv: Really, instead of a custom FChange type for this type of change, we could create a change that
	// represents a property (or set of properties) being changed, and use Unreal reflection to update it.  This would be
	// reusable for future properties.

	const bool bEnablingSubdivisionPreview = ( GetSubdivisionCount() == 0 && NewSubdivisionCount > 0 );
	const bool bDisablingSubdivisionPreview = ( GetSubdivisionCount() > 0 && NewSubdivisionCount == 0 );

	FSetSubdivisionCountChangeInput RevertInput;
	RevertInput.NewSubdivisionCount = GetSubdivisionCount();

	this->SubdivisionCount = NewSubdivisionCount;

	if( bDisablingSubdivisionPreview )
	{
		// We've turned off subdivision preview, so we'll need to re-create the static mesh data from our stored mesh representation
		RebuildRenderMesh();
	}
	else
	{
		// NOTE: We don't bother regenerating geometry here because it's expected that EndModification() will be called after this, which will do the trick
	}

	AddUndo( MakeUnique<FSetSubdivisionCountChange>( MoveTemp( RevertInput ) ) );
#endif
}


void UEditableMesh::MoveVertices( const TArray<FVertexToMove>& VerticesToMove )
{
	EM_ENTER( TEXT( "MoveVertices: %s" ), *LogHelpers::ArrayToString( VerticesToMove ) );

	static TSet< FPolygonID > VertexConnectedPolygons;
	VertexConnectedPolygons.Reset();

	static TArray<FAttributesForVertex> VertexAttributesToSet;
	VertexAttributesToSet.Reset();

	const TVertexAttributesRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	for( const FVertexToMove& VertexToMove : VerticesToMove )
	{
		const FVector CurrentPosition = VertexPositions[ VertexToMove.VertexID ];

		if( VertexToMove.NewVertexPosition != CurrentPosition )
		{
			VertexAttributesToSet.Emplace();
			FAttributesForVertex& AttributesForVertex = VertexAttributesToSet.Last();
			AttributesForVertex.VertexID = VertexToMove.VertexID;
			AttributesForVertex.VertexAttributes.Attributes.Emplace( MeshAttribute::Vertex::Position, 0, FMeshElementAttributeValue( VertexToMove.NewVertexPosition ) );

			// All of the polygons that share this vertex will need new normals
			static TArray<FPolygonID> ConnectedPolygonRefs;
			GetVertexConnectedPolygons( VertexToMove.VertexID, /* Out */ ConnectedPolygonRefs );
			VertexConnectedPolygons.Append( ConnectedPolygonRefs );
		}
	}

	SetVerticesAttributes( VertexAttributesToSet );

	// Mark all polygons connected to the vertex as requiring a new tangent basis and retriangulation
	// Everything needs to be retriangulated because convexity may have changed
	PolygonsPendingNewTangentBasis.Append( VertexConnectedPolygons );
	PolygonsPendingTriangulation.Append( VertexConnectedPolygons );

	EM_EXIT( TEXT( "MoveVertices returned" ) );
}


void UEditableMesh::CreateMissingPolygonPerimeterEdges( const FPolygonID PolygonID, TArray<FEdgeID>& OutNewEdgeIDs )
{
	// @todo mesheditor: Orphaned method - should we keep it or is it entirely unnecessary now?

	OutNewEdgeIDs.Reset();

	const int32 NumPolygonPerimeterEdges = GetPolygonPerimeterEdgeCount( PolygonID );
	const int32 NumPolygonPerimeterVertices = NumPolygonPerimeterEdges;		// Edge and vertex count are always the same

	for( int32 PerimeterEdgeNumber = 0; PerimeterEdgeNumber < NumPolygonPerimeterEdges; ++PerimeterEdgeNumber )
	{
		const int32 PerimeterVertexNumber = PerimeterEdgeNumber;	// Edge and vertex counts are always the same

		const FVertexID VertexID = GetPolygonPerimeterVertex( PolygonID, PerimeterVertexNumber );
		const FVertexID NextVertexID = GetPolygonPerimeterVertex( PolygonID, ( PerimeterVertexNumber + 1 ) % NumPolygonPerimeterVertices );

		// Find the edge that connects these vertices
		FEdgeID FoundEdgeID = FEdgeID::Invalid;
		bool bFoundEdge = false;

		const int32 NumVertexConnectedEdges = GetVertexConnectedEdgeCount( VertexID );	// @todo mesheditor perf: Could be made faster by always iterating over the vertex with the fewest number of connected edges, instead of the first edge vertex
		for( int32 VertexEdgeNumber = 0; VertexEdgeNumber < NumVertexConnectedEdges; ++VertexEdgeNumber )
		{
			const FEdgeID VertexConnectedEdgeID = GetVertexConnectedEdge( VertexID, VertexEdgeNumber );

			// Try the edge's first vertex.  Does it point to our next edge?
			FVertexID OtherEdgeVertexID = GetEdgeVertex( VertexConnectedEdgeID, 0 );
			if( OtherEdgeVertexID == VertexID )
			{
				// Must be the the other one
				OtherEdgeVertexID = GetEdgeVertex( VertexConnectedEdgeID, 1 );
			}
			else
			{
				check( GetEdgeVertex( VertexConnectedEdgeID, 1 ) == VertexID );
			}

			if( OtherEdgeVertexID == NextVertexID )
			{
				// We found the edge!
				FoundEdgeID = VertexConnectedEdgeID;
				bFoundEdge = true;
				break;
			}
		}

		if( !bFoundEdge )
		{
			// Create the new edge!  Note that this does not connect the edge to the polygon.  We expect the caller to do that afterwards.
			FEdgeToCreate EdgeToCreate;
			EdgeToCreate.VertexID0 = VertexID;
			EdgeToCreate.VertexID1 = NextVertexID;

			static TArray< FEdgeToCreate > EdgesToCreate;
			EdgesToCreate.Reset();
			EdgesToCreate.Add( EdgeToCreate );

			static TArray< FEdgeID > NewEdgeIDs;
			NewEdgeIDs.Reset();
			CreateEdges( EdgesToCreate, /* Out */ NewEdgeIDs );

			OutNewEdgeIDs.Append( NewEdgeIDs );
		}
	}
}


void UEditableMesh::SplitEdge( const FEdgeID EdgeID, const TArray<float>& Splits, TArray<FVertexID>& OutNewVertexIDs )
{
	EM_ENTER( TEXT( "SplitEdge: %s %s" ), *EdgeID.ToString(), *LogHelpers::ArrayToString( Splits ) );

	// NOTE: The incoming splits should always be between 0.0 and 1.0, representing progress along 
	//       the edge from the edge's first vertex toward it's other vertex.  The order doesn't matter (we'll sort them.)

	const int32 NumNewVertices = Splits.Num();

	check( NumNewVertices > 0 );

	// Sort the split values smallest to largest.  We'll be adding a strip of vertices for each split, and the
	// indices for those new vertices need to be in order.
	static TArray<float> SortedSplits;
	SortedSplits.Reset();
	SortedSplits = Splits;
	if( NumNewVertices > 1 )
	{
		SortedSplits.Sort();
	}

	FVertexID OriginalEdgeVertexIDs[ 2 ];
	GetEdgeVertices( EdgeID, /* Out */ OriginalEdgeVertexIDs[0], /* Out */ OriginalEdgeVertexIDs[1] );

	// Split the edge, and connect the vertex to the polygons that share the two new edges
	const FVertexID OriginalEdgeFarVertexID = OriginalEdgeVertexIDs[ 1 ];

	// Get attributes for the edge being split: they will be copied into the new edges created
	static FMeshElementAttributeList EdgeAttributeList;
	EdgeAttributeList.Attributes.Reset();
	BackupAllAttributes( EdgeAttributeList, GetMeshDescription()->EdgeAttributes(), EdgeID );


	// Create new vertices, and get new attributes by interpolating values between the edge delimiters.
	static TArray<FVertexID> NewVertexIDs;
	NewVertexIDs.Reset();
	{
		OutNewVertexIDs.Reset();
		OutNewVertexIDs.Reserve( NumNewVertices );

		static TArray<FVertexToCreate> VerticesToCreate;
		VerticesToCreate.Reset();
		VerticesToCreate.Reserve( NumNewVertices );

		for( int32 NewVertexNumber = 0; NewVertexNumber < NumNewVertices; ++NewVertexNumber )
		{
			FVertexToCreate& VertexToCreate = *new( VerticesToCreate )FVertexToCreate();
			InterpAllAttributes( VertexToCreate.VertexAttributes, GetMeshDescription()->VertexAttributes(), OriginalEdgeVertexIDs[ 0 ], OriginalEdgeVertexIDs[ 1 ], SortedSplits[ NewVertexNumber ] );
		}

		CreateVertices( VerticesToCreate, /* Out */ NewVertexIDs );

		OutNewVertexIDs.Append( NewVertexIDs );
	}


	// Create new vertex instances.
	struct FAffectedPolygonEdge
	{
		int32 PerimeterVertexIndex;
		int32 NewVertexInstanceArrayIndex;
		bool bReversed;
	};

	static TMap<TTuple<FVertexInstanceID, FVertexInstanceID>, int32> VertexInstancePairsToIndex;
	static TMap<FPolygonID, FAffectedPolygonEdge> PolygonToAffectedEdge;
	static TArray<FVertexInstanceToCreate> VertexInstancesToCreate;
	static TArray<FVertexInstanceID> NewVertexInstanceIDs;
	VertexInstancePairsToIndex.Reset();
	PolygonToAffectedEdge.Reset();
	VertexInstancesToCreate.Reset();
	{
		int32 VertexInstanceIndex = 0;

		// Iterate through each polygon connected to the edge being split
		for( const FPolygonID ConnectedPolygonID : GetMeshDescription()->GetEdgeConnectedPolygons( EdgeID ) )
		{
			const TArray<FVertexInstanceID>& PerimeterVertexInstances = GetMeshDescription()->GetPolygonPerimeterVertexInstances( ConnectedPolygonID );
			const int32 NumVertexInstances = PerimeterVertexInstances.Num();

			// Find vertex indices around the perimeter corresponding to the vertices defining the edge
			int32 VertexIndex = 0;
			int32 VertexIndices[ 2 ] = { INDEX_NONE, INDEX_NONE };
			for( const FVertexInstanceID PerimeterVertexInstance : PerimeterVertexInstances )
			{
				if( GetMeshDescription()->GetVertexInstanceVertex( PerimeterVertexInstance ) == OriginalEdgeVertexIDs[ 0 ] )
				{
					VertexIndices[ 0 ] = VertexIndex;
				}
				else if( GetMeshDescription()->GetVertexInstanceVertex( PerimeterVertexInstance ) == OriginalEdgeVertexIDs[ 1 ] )
				{
					VertexIndices[ 1 ] = VertexIndex;
				}

				VertexIndex++;
			}

			// Check the vertex indices are valid, and are adjacent (taking into account wrap-around)
			check( VertexIndices[ 0 ] != INDEX_NONE && VertexIndices[ 1 ] != INDEX_NONE );
			check( FMath::Abs( VertexIndices[ 0 ] - VertexIndices[ 1 ] ) == 1 || FMath::Abs( VertexIndices[ 0 ] - VertexIndices[ 1 ] ) == NumVertexInstances - 1 );

			// Get actual vertex instance IDs of the edge ends (so we can reuse new vertex instances where appropriate)
			const FVertexInstanceID VertexInstanceID0 = PerimeterVertexInstances[ VertexIndices[ 0 ] ];
			const FVertexInstanceID VertexInstanceID1 = PerimeterVertexInstances[ VertexIndices[ 1 ] ];

			// See if this vertex instance pair has already had its intermediate vertices created
			const int32* IndexForVertexInstancePair = VertexInstancePairsToIndex.Find( MakeTuple( VertexInstanceID0, VertexInstanceID1 ) );
			const int32 NewVertexInstanceArrayIndex = IndexForVertexInstancePair ? *IndexForVertexInstancePair : VertexInstanceIndex;

			if( !IndexForVertexInstancePair )
			{
				// If not, register it in the map
				VertexInstancePairsToIndex.Add( MakeTuple( VertexInstanceID0, VertexInstanceID1 ), VertexInstanceIndex );
				VertexInstancePairsToIndex.Add( MakeTuple( VertexInstanceID1, VertexInstanceID0 ), VertexInstanceIndex );

				// And create the vertex instances, interpolating attributes
				VertexInstancesToCreate.Reserve( VertexInstancesToCreate.Num() + NumNewVertices );
				for( int32 NewVertexNumber = 0; NewVertexNumber < NumNewVertices; ++NewVertexNumber )
				{
					VertexInstancesToCreate.Emplace();
					FVertexInstanceToCreate& VertexInstanceToCreate = VertexInstancesToCreate.Last();
					VertexInstanceToCreate.VertexID = NewVertexIDs[ NewVertexNumber ];

					InterpAllAttributes( VertexInstanceToCreate.VertexInstanceAttributes,
										 GetMeshDescription()->VertexInstanceAttributes(),
										 VertexInstanceID0,
										 VertexInstanceID1,
										 SortedSplits[ NewVertexNumber ] );										 
				}

				VertexInstanceIndex += NumNewVertices;
			}

			// Add details for this polygon edge, used to determine how new vertex instances should be inserted.
			// A reversed edge is one where either:
			// - The 1st perimeter index is greater than the 2nd; OR
			// - The two indices are not consecutive (i.e. at the wrap around point);
			// but not both.
			const bool bReversed = ( ( VertexIndices[ 0 ] > VertexIndices[ 1 ] ) ^ ( FMath::Abs( VertexIndices[ 0 ] - VertexIndices[ 1 ] ) > 1 ) );

			FAffectedPolygonEdge AffectedPolygonEdge;
			AffectedPolygonEdge.PerimeterVertexIndex = ( bReversed ? VertexIndices[ 1 ] : VertexIndices[ 0 ] ) + 1;
			AffectedPolygonEdge.NewVertexInstanceArrayIndex = NewVertexInstanceArrayIndex;
			AffectedPolygonEdge.bReversed = bReversed;

			PolygonToAffectedEdge.Add( ConnectedPolygonID, AffectedPolygonEdge );
		}

		CreateVertexInstances( VertexInstancesToCreate, NewVertexInstanceIDs );
	}

	{
		static TArray<FVerticesForEdge> VerticesForEdges;
		VerticesForEdges.Reset();

		// We'll keep the existing edge, but update it to connect to the first new vertex.  The second vertex of the edge will 
		// now connect to the first (new) vertex ID, and so on.  The incoming vertices are expected to be ordered correctly.
		{
			FVerticesForEdge& VerticesForEdge = *new( VerticesForEdges ) FVerticesForEdge();

			VerticesForEdge.EdgeID = EdgeID;
			VerticesForEdge.NewVertexID0 = OriginalEdgeVertexIDs[ 0 ];
			VerticesForEdge.NewVertexID1 = NewVertexIDs[ 0 ];
		}

		SetEdgesVertices( VerticesForEdges );
	}

	// Create new edges.  One for each of the new vertex positions passed in.
	{
		const int32 NewEdgeCount = NumNewVertices;

		static TArray<FEdgeToCreate> EdgesToCreate;
		EdgesToCreate.Reset();
		EdgesToCreate.Reserve( NewEdgeCount );
		for( int32 NewEdgeNumber = 0; NewEdgeNumber < NewEdgeCount; ++NewEdgeNumber )
		{
			FEdgeToCreate& EdgeToCreate = *new( EdgesToCreate ) FEdgeToCreate();

			EdgeToCreate.VertexID0 = NewVertexIDs[ NewEdgeNumber ];
			EdgeToCreate.VertexID1 = ( NewEdgeNumber == ( NewEdgeCount - 1 ) ) ? OriginalEdgeFarVertexID : NewVertexIDs[ NewEdgeNumber + 1 ];

			EdgeToCreate.ConnectedPolygons = GetMeshDescription()->GetEdgeConnectedPolygons( EdgeID );

			// Copy edge attributes over from original edge
			EdgeToCreate.EdgeAttributes = EdgeAttributeList;
		}

		static TArray< FEdgeID > NewEdgeIDs;
		NewEdgeIDs.Reset();
		CreateEdges( EdgesToCreate, /* Out */ NewEdgeIDs );
	}

	// Update all affected polygons with their new vertices.  Also, we'll fill in polygon-specific vertex attributes (texture coordinates)
	{
		for( const FPolygonID ConnectedPolygonID : GetMeshDescription()->GetEdgeConnectedPolygons( EdgeID ) )
		{
			static TArray<FVertexAndAttributes> VerticesToInsert;
			VerticesToInsert.Reset( NumNewVertices );

			const FAffectedPolygonEdge& AffectedPolygonEdge = PolygonToAffectedEdge.FindRef( ConnectedPolygonID );

			for( int32 Count = 0; Count < NumNewVertices; ++Count )
			{
				const int32 VertexIndex = AffectedPolygonEdge.bReversed ? NumNewVertices - 1 - Count : Count;

				VerticesToInsert.Emplace();
				FVertexAndAttributes& VertexAndAttributes = VerticesToInsert.Last();
				VertexAndAttributes.VertexInstanceID = NewVertexInstanceIDs[ AffectedPolygonEdge.NewVertexInstanceArrayIndex + VertexIndex ];
			}

			// Add the new vertices to the polygon
			InsertPolygonPerimeterVertices( ConnectedPolygonID, AffectedPolygonEdge.PerimeterVertexIndex, VerticesToInsert );
		}
	}

	// Generate normals and tangents
	PolygonsPendingNewTangentBasis.Append( GetMeshDescription()->GetEdgeConnectedPolygons( EdgeID ) );

	// Retriangulate all of the affected polygons
	PolygonsPendingTriangulation.Append( GetMeshDescription()->GetEdgeConnectedPolygons( EdgeID ) );

	EM_EXIT( TEXT( "SplitEdge returned %s" ), *LogHelpers::ArrayToString( OutNewVertexIDs ) );
}


void UEditableMesh::FindPolygonLoop( const FEdgeID EdgeID, TArray<FEdgeID>& OutEdgeLoopEdgeIDs, TArray<FEdgeID>& OutFlippedEdgeIDs, TArray<FEdgeID>& OutReversedEdgeIDPathToTake, TArray<FPolygonID>& OutPolygonIDsToSplit ) const
{
	OutEdgeLoopEdgeIDs.Reset();
	OutFlippedEdgeIDs.Reset();
	OutReversedEdgeIDPathToTake.Reset();
	OutPolygonIDsToSplit.Reset();

	// Is the edge we're starting on a border edge?
	bool bStartedOnBorderEdge = ( GetEdgeConnectedPolygonCount( EdgeID ) <= 1 );

	// We'll actually do two passes searching for edges.  The first time, we'll flow along the polygons looking for a border edge.
	// If we find one, it means we won't really have a loop, but instead we're just splitting a series of connected polygons.  In that
	// case, the polygon with the border edge will become the start of our search for an opposing border edge.  Otherwise, we'll start
	// at the input polygon and flow around until we come back to that polygon again.  If we can't make it back, then no polygons
	// will be split by this operation.
	bool bIsSearchingForBorderEdge = !bStartedOnBorderEdge;

	// Keep track of whether we actually looped back around to the starting edge (rather than simply splitting across a series
	// of polygons that both end at border edges.)
	bool bIsCompleteLoop = false;

	FEdgeID CurrentEdgeID = EdgeID;
	bool bCurrentEdgeIsBorderEdge = bStartedOnBorderEdge;
	bool bCurrentEdgeIsInOppositeDirection = false;
	bool bCurrentEdgeIsInOppositeDirectionFromStartEdge = false;

	TVertexAttributesConstRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	for( ; ; )
	{
		// Add the current edge!
		checkSlow( !OutEdgeLoopEdgeIDs.Contains( CurrentEdgeID ) );
		OutEdgeLoopEdgeIDs.Add( CurrentEdgeID );
		if( bCurrentEdgeIsInOppositeDirection )
		{
			OutFlippedEdgeIDs.Add( CurrentEdgeID );
		}

		FVertexID CurrentEdgeVertexIDs[ 2 ];
		GetEdgeVertices( CurrentEdgeID, /* Out */ CurrentEdgeVertexIDs[ 0 ], /* Out */ CurrentEdgeVertexIDs[ 1 ] );

		const FVector CurrentEdgeVertex0 = VertexPositions[ CurrentEdgeVertexIDs[ 0 ] ];
		const FVector CurrentEdgeVertex1 = VertexPositions[ CurrentEdgeVertexIDs[ 1 ] ];
		const FVector CurrentEdgeDirection = ( CurrentEdgeVertex1 - CurrentEdgeVertex0 ).GetSafeNormal();

		const FEdgeID NextEdgeIDInPath = OutReversedEdgeIDPathToTake.Num() > 0 ? OutReversedEdgeIDPathToTake.Pop( false ) : FEdgeID::Invalid;

		FEdgeID BestEdgeID = FEdgeID::Invalid;
		FPolygonID BestEdgeSplitsPolygon = FPolygonID::Invalid;
		bool bBestEdgeIsInOppositeDirection = false;
		bool bBestEdgeIsBorderEdge = false;
		float LargestAbsDotProduct = -1.0f;

		// Let's take a look at all of the polygons connected to this edge.  These will start our loop.
		const int32 ConnectedPolygonCount = GetEdgeConnectedPolygonCount( CurrentEdgeID );
		for( int32 ConnectedPolygonNumber = 0; ConnectedPolygonNumber < ConnectedPolygonCount; ++ConnectedPolygonNumber )
		{
			const FPolygonID ConnectedPolygonID = GetEdgeConnectedPolygon( CurrentEdgeID, ConnectedPolygonNumber );

			// Don't bother looking at the last polygon that was added to our split list.  We never want to backtrack!
			if( OutPolygonIDsToSplit.Num() == 0 || ConnectedPolygonID != OutPolygonIDsToSplit.Last() )
			{
				static TArray<FEdgeID> CandidateEdgeIDs;
				CandidateEdgeIDs.Reset();
				GetPolygonPerimeterEdges( ConnectedPolygonID, /* Out */ CandidateEdgeIDs );

				// Which edge of the connected polygon will be at the other end of our split?
				for( const FEdgeID CandidateEdgeID : CandidateEdgeIDs )
				{
					// Don't bother with the edge we just came from
					if( CandidateEdgeID != CurrentEdgeID )
					{
						// If we need to follow a specific path, then do that
						if( NextEdgeIDInPath == FEdgeID::Invalid || CandidateEdgeID == NextEdgeIDInPath )
						{
							FVertexID CandidateEdgeVertexIDs[ 2 ];
							GetEdgeVertices( CandidateEdgeID, /* Out */ CandidateEdgeVertexIDs[ 0 ], /* Out */ CandidateEdgeVertexIDs[ 1 ] );

							const bool bIsBorderEdge = ( GetEdgeConnectedPolygonCount( CandidateEdgeID ) == 1 );

							const FVector CandidateEdgeVertex0 = VertexPositions[ CandidateEdgeVertexIDs[ 0 ] ];
							const FVector CandidateEdgeVertex1 = VertexPositions[ CandidateEdgeVertexIDs[ 1 ] ];
							const FVector CandidateEdgeDirection = ( CandidateEdgeVertex1 - CandidateEdgeVertex0 ).GetSafeNormal();

							const float DotProduct = FVector::DotProduct( CurrentEdgeDirection, CandidateEdgeDirection );
							const float AbsDotProduct = FMath::Abs( DotProduct );

							const float SameEdgeDirectionDotEpsilon = 0.05f;	// @todo mesheditor tweak
							if( FMath::IsNearlyEqual( AbsDotProduct, LargestAbsDotProduct, SameEdgeDirectionDotEpsilon ) )
							{
								// If the candidate edge directions are pretty much the same, we'll choose the edge that flows closest to the
								// direction that we split the last polygon in
								if( OutEdgeLoopEdgeIDs.Num() > 1 )
								{
									FVertexID LastSplitEdgeVertexIDs[ 2 ];
									GetEdgeVertices( OutEdgeLoopEdgeIDs[ OutEdgeLoopEdgeIDs.Num() - 2 ], /* Out */ LastSplitEdgeVertexIDs[ 0 ], /* Out */ LastSplitEdgeVertexIDs[ 1 ] );

									const FVector LastSplitEdgeVertex0 = VertexPositions[ LastSplitEdgeVertexIDs[ 0 ] ];
									const FVector LastSplitEdgeVertex1 = VertexPositions[ LastSplitEdgeVertexIDs[ 1 ] ];

									const FVector DirectionTowardCenterOfCurrentEdge =
										( FMath::Lerp( CurrentEdgeVertex0, CurrentEdgeVertex1, 0.5f ) -
											FMath::Lerp( LastSplitEdgeVertex0, LastSplitEdgeVertex1, 0.5f ) ).GetSafeNormal();

									const FVector DirectionTowardCenterOfCandidateEdge =
										( FMath::Lerp( CandidateEdgeVertex0, CandidateEdgeVertex1, 0.5f ) -
											FMath::Lerp( CurrentEdgeVertex0, CurrentEdgeVertex1, 0.5f ) ).GetSafeNormal();
									const float CandidateEdgeDot = FVector::DotProduct( DirectionTowardCenterOfCurrentEdge, DirectionTowardCenterOfCandidateEdge );

									check( BestEdgeID != FEdgeID::Invalid );

									FVertexID BestEdgeVertexIDs[ 2 ];
									GetEdgeVertices( BestEdgeID, /* Out */ BestEdgeVertexIDs[ 0 ], /* Out */ BestEdgeVertexIDs[ 1 ] );

									const FVector BestEdgeVertex0 = VertexPositions[ BestEdgeVertexIDs[ 0 ] ];
									const FVector BestEdgeVertex1 = VertexPositions[ BestEdgeVertexIDs[ 1 ] ];

									const FVector DirectionTowardCenterOfBestEdge =
										( FMath::Lerp( BestEdgeVertex0, BestEdgeVertex1, 0.5f ) -
											FMath::Lerp( CurrentEdgeVertex0, CurrentEdgeVertex1, 0.5f ) ).GetSafeNormal();

									const float BestEdgeDot = FVector::DotProduct( DirectionTowardCenterOfCurrentEdge, DirectionTowardCenterOfBestEdge );

									if( CandidateEdgeDot > BestEdgeDot )
									{
										BestEdgeID = CandidateEdgeID;
										BestEdgeSplitsPolygon = ConnectedPolygonID;
										bBestEdgeIsInOppositeDirection = ( DotProduct < 0.0f );
										bBestEdgeIsBorderEdge = bIsBorderEdge;
										LargestAbsDotProduct = AbsDotProduct;
									}
								}
								else
								{
									// Edge directions are the same, but this is the very first split so we don't have a "flow" direction yet.
									// Go ahead and prefer the edge that is closer to the initial edge.  This helps in the (uncommon) case of 
									// multiple colinear edges on the same polygon (such as after SplitEdge() is called to insert a vertex on a polygon.)
									const float BestEdgeDistance = [this, BestEdgeID, CurrentEdgeVertex0, CurrentEdgeVertex1, &VertexPositions]() -> float
									{
										check( BestEdgeID != FEdgeID::Invalid );

										FVertexID BestEdgeVertexIDs[ 2 ];
										GetEdgeVertices( BestEdgeID, /* Out */ BestEdgeVertexIDs[ 0 ], /* Out */ BestEdgeVertexIDs[ 1 ] );

										const FVector BestEdgeVertex0 = VertexPositions[ BestEdgeVertexIDs[ 0 ] ];
										const FVector BestEdgeVertex1 = VertexPositions[ BestEdgeVertexIDs[ 1 ] ];

										FVector ClosestPoint0, ClosestPoint1;
										FMath::SegmentDistToSegmentSafe( CurrentEdgeVertex0, CurrentEdgeVertex1, BestEdgeVertex0, BestEdgeVertex1, /* Out */ ClosestPoint0, /* Out */ ClosestPoint1 );
										return ( ClosestPoint1 - ClosestPoint0 ).Size();
									}( );

									const float CandidateEdgeDistance = [CurrentEdgeVertex0, CurrentEdgeVertex1, CandidateEdgeVertex0, CandidateEdgeVertex1]() -> float
									{
										FVector ClosestPoint0, ClosestPoint1;
										FMath::SegmentDistToSegmentSafe( CurrentEdgeVertex0, CurrentEdgeVertex1, CandidateEdgeVertex0, CandidateEdgeVertex1, /* Out */ ClosestPoint0, /* Out */ ClosestPoint1 );
										return ( ClosestPoint1 - ClosestPoint0 ).Size();
									}( );

									if( CandidateEdgeDistance < BestEdgeDistance )
									{
										BestEdgeID = CandidateEdgeID;
										BestEdgeSplitsPolygon = ConnectedPolygonID;
										bBestEdgeIsInOppositeDirection = ( DotProduct < 0.0f );
										bBestEdgeIsBorderEdge = bIsBorderEdge;
										LargestAbsDotProduct = AbsDotProduct;
									}
								}
							}
							else if( AbsDotProduct > LargestAbsDotProduct )
							{
								// This edge angle is the closest to our current edge so far!
								BestEdgeID = CandidateEdgeID;
								BestEdgeSplitsPolygon = ConnectedPolygonID;
								bBestEdgeIsInOppositeDirection = ( DotProduct < 0.0f );
								bBestEdgeIsBorderEdge = bIsBorderEdge;
								LargestAbsDotProduct = AbsDotProduct;
							}
						}
					}
				}
			}
		}

		if( BestEdgeID != FEdgeID::Invalid &&
			!OutPolygonIDsToSplit.Contains( BestEdgeSplitsPolygon ) )	// If we try to re-split the same polygon twice, then this loop is not valid
		{
			// OK, this polygon will definitely be split
			OutPolygonIDsToSplit.Add( BestEdgeSplitsPolygon );

			CurrentEdgeID = BestEdgeID;
			bCurrentEdgeIsBorderEdge = bBestEdgeIsBorderEdge;
			bCurrentEdgeIsInOppositeDirection = bBestEdgeIsInOppositeDirection;
			if( bBestEdgeIsInOppositeDirection )
			{
				bCurrentEdgeIsInOppositeDirectionFromStartEdge = !bCurrentEdgeIsInOppositeDirectionFromStartEdge;
			}

			// Is the best edge already part of our loop?  If so, then we're done!
			if( OutEdgeLoopEdgeIDs[ 0 ] == BestEdgeID )
			{
				bIsCompleteLoop = true;
				break;
			}
			else if( OutEdgeLoopEdgeIDs.Contains( BestEdgeID ) )	// @todo mesheditor perf: Might want to use a TSet here, but we'll still need to keep a list too (order is important.)
			{
				// We ended up back at an edge that we already split, but it wasn't the edge that we started on.  This is
				// not a valid loop, so clear our path and bail out.

				// @todo mesheditor edgeloop: We need to revisit how we handle border edges and non-manifold edges.  Basically instead of searching
				// for border edges, we should try looping around the "back" of polygons we've already visited, trying to get back to the starting
				// edge.  This will allow the case of a cube mesh with a single non-manifold face to be split properly.  We just need to make sure
				// we're not trying to split the same edge multiple times in different locations.  Probably, we don't want to split the same polygon
				// more than once either.
				OutEdgeLoopEdgeIDs.Reset();
				OutFlippedEdgeIDs.Reset();
				OutPolygonIDsToSplit.Reset();

				break;
			}
			else if( bBestEdgeIsBorderEdge && bIsSearchingForBorderEdge )
			{
				// We found a border edge, so stop the search.  We'll now start over at this edge to form our loop.
				bStartedOnBorderEdge = true;

				bIsSearchingForBorderEdge = false;
				bCurrentEdgeIsInOppositeDirection = bCurrentEdgeIsInOppositeDirectionFromStartEdge;

				// Follow the path we took to get here, in reverse order, to make sure we get back to the edge we 
				// were asked to create a loop on
				OutReversedEdgeIDPathToTake = OutEdgeLoopEdgeIDs;

				bIsCompleteLoop = false;

				OutEdgeLoopEdgeIDs.Reset();
				OutFlippedEdgeIDs.Reset();
				OutPolygonIDsToSplit.Reset();
			}
			else
			{
				// Proceed to the next edge and try to continue the loop.  If we're at a border edge, the loop will definitely end here.
			}
		}
		else
		{
			if( bStartedOnBorderEdge && bCurrentEdgeIsBorderEdge )
			{
				// We started on a border edge, and we've found the border edge on the other side of the polygons we'll be splitting.
				// This isn't actually a loop, but we'll still split the polygons.
			}
			else
			{
				// We couldn't even find another edge, so we're done.
				OutEdgeLoopEdgeIDs.Reset();
				OutFlippedEdgeIDs.Reset();
				OutPolygonIDsToSplit.Reset();
			}

			break;
		}
	}

	// We're always splitting the same number of polygons as we have edges in the loop (these can be zero), except
	// in the border edge case, where we're always splitting one less polygon.
	if( bStartedOnBorderEdge &&
		!bIsCompleteLoop )	// It's possible to start and finish on the same border edge, which is a complete loop
	{
		// We're splitting a series of polygons between two border edges
		check( ( OutEdgeLoopEdgeIDs.Num() == 0 && OutPolygonIDsToSplit.Num() == 0 ) || ( OutEdgeLoopEdgeIDs.Num() == ( OutPolygonIDsToSplit.Num() + 1 ) ) );
	}
	else
	{
		// We're splitting polygons spanning a full loop of edges.  The starting edge is the same as the beginning edge.
		check( OutEdgeLoopEdgeIDs.Num() == OutPolygonIDsToSplit.Num() );
	}
}


void UEditableMesh::InsertEdgeLoop( const FEdgeID EdgeID, const TArray<float>& Splits, TArray<FEdgeID>& OutNewEdgeIDs )
{
	EM_ENTER( TEXT( "InsertEdgeLoop: %s %s" ), *EdgeID.ToString(), *LogHelpers::ArrayToString( Splits ) );

	OutNewEdgeIDs.Reset();

	// NOTE: The incoming splits should always be between 0.0 and 1.0, representing progress along 
	//       the edge from the edge's first vertex toward it's other vertex.  The order doesn't matter (we'll sort them.)

	// @todo mesheditor: Test with concave polygons -- probably we need to disallow/avoid splits that cross a concave part


	static TArray<FEdgeID> EdgeLoopEdgeIDs;
	EdgeLoopEdgeIDs.Reset();

	static TArray<FEdgeID> FlippedEdgeIDs;
	FlippedEdgeIDs.Reset();

	static TArray<FEdgeID> ReversedEdgeIDPathToTake;
	ReversedEdgeIDPathToTake.Reset();

	static TArray<FPolygonID> PolygonIDsToSplit;
	PolygonIDsToSplit.Reset();


	FindPolygonLoop( 
		EdgeID, 
		/* Out */ EdgeLoopEdgeIDs,
		/* Out */ FlippedEdgeIDs,
		/* Out */ ReversedEdgeIDPathToTake,
		/* Out */ PolygonIDsToSplit );

	static TSet<FEdgeID> FlippedEdgeIDSet;
	FlippedEdgeIDSet.Reset();
	FlippedEdgeIDSet.Append( FlippedEdgeIDs );

	check( Splits.Num() > 0 );

	// Sort the split values smallest to largest.  We'll be adding a strip of vertices for each split, and the
	// IDs for those new vertices need to be in order.
	static TArray<float> SortedSplits;
	SortedSplits.Reset();
	SortedSplits = Splits;
	if( SortedSplits.Num() > 1 )
	{
		SortedSplits.Sort();
	}


	if( PolygonIDsToSplit.Num() > 0 )
	{
		// Keep track of the new vertices create by splitting all of the edges.  For each edge we split, an array of
		// vertex IDs for each split along that edge
		static TArray<TArray<FVertexID>> NewVertexIDsForEachEdge;
		NewVertexIDsForEachEdge.Reset();


		// Now let's go through and create new vertices for the loops by splitting edges
		{
			for( int32 EdgeLoopEdgeNumber = 0; EdgeLoopEdgeNumber < EdgeLoopEdgeIDs.Num(); ++EdgeLoopEdgeNumber )
			{
				const FEdgeID EdgeLoopEdgeID = EdgeLoopEdgeIDs[ EdgeLoopEdgeNumber ];

				// If the edge winds in the opposite direction from the last edge, we'll need to flip the split positions around
				const bool bIsFlipped = FlippedEdgeIDSet.Contains( EdgeLoopEdgeID );
				if( bIsFlipped )
				{
					static TArray<float> TempSplits;
					TempSplits.SetNumUninitialized( SortedSplits.Num(), false );
					for( int32 SplitIndex = 0; SplitIndex < SortedSplits.Num(); ++SplitIndex )
					{
						TempSplits[ SplitIndex ] = 1.0f - SortedSplits[ ( SortedSplits.Num() - 1 ) - SplitIndex ];
					}
					SortedSplits = TempSplits;
				}

				// Split this edge
				static TArray<FVertexID> CurrentVertexIDs;
				CurrentVertexIDs.Reset();
				SplitEdge( EdgeLoopEdgeID, SortedSplits, /* Out */ CurrentVertexIDs );

				// If the edge winding is backwards, we'll reverse the order of the vertex IDs in our list
				// @todo mesheditor urgent edgeloop: INCORRECT (bIsFlipped is supposed to swap from last time -- TempSplits.)  Also not sure if needed or not... might be double compensating here.
				static TArray<FVertexID> NewVertexIDsForEdge;
				NewVertexIDsForEdge.SetNum( CurrentVertexIDs.Num(), false );
				for( int32 VertexNumber = 0; VertexNumber < CurrentVertexIDs.Num(); ++VertexNumber )
				{
					NewVertexIDsForEdge[ /*bIsFlipped ? */( ( CurrentVertexIDs.Num() - VertexNumber ) - 1 ) /*: VertexNumber */] = CurrentVertexIDs[ VertexNumber ];
				}
				NewVertexIDsForEachEdge.Add( NewVertexIDsForEdge );
			}
		}


		// Time to create new polygons for the split faces (and delete the old ones)
		{
			static TArray<FPolygonToSplit> PolygonsToSplit;
			PolygonsToSplit.Reset();

			for( int32 PolygonToSplitIter = 0; PolygonToSplitIter < PolygonIDsToSplit.Num(); ++PolygonToSplitIter )
			{
				const FPolygonID PolygonID = PolygonIDsToSplit[ PolygonToSplitIter ];

				FPolygonToSplit& PolygonToSplit = *new( PolygonsToSplit ) FPolygonToSplit();
				PolygonToSplit.PolygonID = PolygonID;

				// The first and second edges connected to this polygon that are being split up
				const int32 FirstEdgeNumber = PolygonToSplitIter;
				const int32 SecondEdgeNumber = ( PolygonToSplitIter + 1 ) % EdgeLoopEdgeIDs.Num();

				const FEdgeID FirstSplitEdgeID = EdgeLoopEdgeIDs[ FirstEdgeNumber ];
				const FEdgeID SecondSplitEdgeID = EdgeLoopEdgeIDs[ SecondEdgeNumber ];
				check( FirstSplitEdgeID != SecondSplitEdgeID );

				// The (ordered) list of new vertices that was created by splitting the first and second edge.  One for each split.
				const TArray<FVertexID>& FirstSplitEdgeNewVertexIDs = NewVertexIDsForEachEdge[ FirstEdgeNumber ];
				const TArray<FVertexID>& SecondSplitEdgeNewVertexIDs = NewVertexIDsForEachEdge[ SecondEdgeNumber ];

				for( int32 SplitIter = 0; SplitIter < SortedSplits.Num(); ++SplitIter )
				{
					FVertexPair& NewVertexPair = *new( PolygonToSplit.VertexPairsToSplitAt ) FVertexPair();
					NewVertexPair.VertexID0 = FirstSplitEdgeNewVertexIDs[ SplitIter ];
					NewVertexPair.VertexID1 = SecondSplitEdgeNewVertexIDs[ SplitIter ];
				}
			}


			// Actually split up the polygons
			static TArray<FEdgeID> NewEdgeIDs;
			NewEdgeIDs.Reset();
			SplitPolygons( PolygonsToSplit, /* Out */ NewEdgeIDs );

			OutNewEdgeIDs.Append( NewEdgeIDs );
		}
	}

	EM_EXIT( TEXT( "InsertEdgeLoop returned %s" ), *LogHelpers::ArrayToString( OutNewEdgeIDs ) );
}


void UEditableMesh::SplitPolygons( const TArray<FPolygonToSplit>& PolygonsToSplit, TArray<FEdgeID>& OutNewEdgeIDs )
{
	EM_ENTER( TEXT( "SplitPolygons: %s" ), *LogHelpers::ArrayToString( PolygonsToSplit ) );

	OutNewEdgeIDs.Reset();

	static TArray<FPolygonToCreate> PolygonsToCreate;
	PolygonsToCreate.Reset();

	for( const FPolygonToSplit& PolygonToSplit : PolygonsToSplit )
	{
		const FPolygonID PolygonID = PolygonToSplit.PolygonID;

		// Get all of the polygon's vertices
		static TArray<FVertexID> PerimeterVertexIDs;
		PerimeterVertexIDs.Reset();
		GetPolygonPerimeterVertices( PolygonID, /* Out */ PerimeterVertexIDs );
		

		// Figure out where exactly we're splitting the polygon for these splits.  Remember, we support splitting the
		// polygon multiple times at once.  The first and last split are the most interesting because we need to continue
		// the original flow of the polygon after inserting our new edge.  For all of the new polygons in the middle, we'll 
		// just create simple quads.
		const int32 SplitCount = PolygonToSplit.VertexPairsToSplitAt.Num();

		int32 LastPolygonVertexNumbers[ 2 ] = { INDEX_NONE, INDEX_NONE };
		bool bLastPolygonWindsForward = false;

		const int32 NumPolygonsToCreate = SplitCount + 1;
		for( int32 PolygonIter = 0; PolygonIter < NumPolygonsToCreate; ++PolygonIter )
		{
			const FVertexPair& VertexPair = PolygonToSplit.VertexPairsToSplitAt[ FMath::Min( PolygonIter, NumPolygonsToCreate - 2 ) ];

			const FVertexID FirstVertexID = VertexPair.VertexID0;
			const FVertexID SecondVertexID = VertexPair.VertexID1;

			const int32 FirstVertexNumber = PerimeterVertexIDs.IndexOfByKey( FirstVertexID );
			check( FirstVertexNumber != INDEX_NONE );	// Incoming vertex ID must already be a part of this polygon!
			const int32 SecondVertexNumber = PerimeterVertexIDs.IndexOfByKey( SecondVertexID );
			check( SecondVertexNumber != INDEX_NONE );	// Incoming vertex ID must already be a part of this polygon!


			FPolygonToCreate& NewPolygon = *new( PolygonsToCreate ) FPolygonToCreate();
			NewPolygon.PolygonGroupID = GetGroupForPolygon( PolygonID );
			NewPolygon.PolygonEdgeHardness = EPolygonEdgeHardness::NewEdgesSoft;

			static TArray<int32> PerimeterVertexNumbers;
			PerimeterVertexNumbers.Reset();

			const bool bWindsForward = FirstVertexNumber < SecondVertexNumber;

			int32 SmallerVertexNumber = bWindsForward ? FirstVertexNumber : SecondVertexNumber;
			int32 LargerVertexNumber = bWindsForward ? SecondVertexNumber : FirstVertexNumber;

			if( PolygonIter == 0 || PolygonIter == ( NumPolygonsToCreate - 1 ) )
			{
				// This is either the first or last new polygon 
				const bool bIsFirstPolygon = ( PolygonIter == 0 );

				// Add the vertices we created for the new edge that will split the polygon
				if( bIsFirstPolygon )
				{
					PerimeterVertexNumbers.Add( SmallerVertexNumber );
					PerimeterVertexNumbers.Add( LargerVertexNumber );
				}
				else
				{
					PerimeterVertexNumbers.Add( LargerVertexNumber );
					PerimeterVertexNumbers.Add( SmallerVertexNumber );
				}

				// Now add all of the other vertices of the original polygon that are on this side of the split
				if( bIsFirstPolygon )
				{
					for( int32 VertexNumber = ( LargerVertexNumber + 1 ) % PerimeterVertexIDs.Num();
						 VertexNumber != SmallerVertexNumber;
						 VertexNumber = ( VertexNumber + 1 ) % PerimeterVertexIDs.Num() )
					{
						PerimeterVertexNumbers.Add( VertexNumber );
					}
				}
				else
				{
					for( int32 VertexNumber = ( SmallerVertexNumber + 1 ) % PerimeterVertexIDs.Num();
						 VertexNumber != LargerVertexNumber;
						 VertexNumber = ( VertexNumber + 1 ) % PerimeterVertexIDs.Num() )
					{
						PerimeterVertexNumbers.Add( VertexNumber );
					}
				}
			}
			else
			{
				// @todo mesheditor urgent edgeloop: Polygon winding is incorrect with multiple splits

				// This is a new polygon in the middle of other polygons created by the splits
				PerimeterVertexNumbers.Add( bWindsForward ? SmallerVertexNumber : LargerVertexNumber );
				PerimeterVertexNumbers.Add( bWindsForward ? LargerVertexNumber : SmallerVertexNumber );
				PerimeterVertexNumbers.Add( LastPolygonVertexNumbers[ 1 ] );
				PerimeterVertexNumbers.Add( LastPolygonVertexNumbers[ 0 ] );
			}

            check(PerimeterVertexNumbers.Num() >= 3);
			NewPolygon.PerimeterVertices.Reserve( PerimeterVertexNumbers.Num() );
			for( const int32 VertexNumber : PerimeterVertexNumbers )
			{
				FVertexAndAttributes& NewVertexAndAttributes = *new( NewPolygon.PerimeterVertices ) FVertexAndAttributes();
				NewVertexAndAttributes.VertexID = FVertexID::Invalid;
				NewVertexAndAttributes.VertexInstanceID = GetPolygonPerimeterVertexInstance( PolygonID, VertexNumber );
			}

			LastPolygonVertexNumbers[ 0 ] = PerimeterVertexNumbers[ 0 ];
			LastPolygonVertexNumbers[ 1 ] = PerimeterVertexNumbers[ 1 ];
			bLastPolygonWindsForward = bWindsForward;
		}
	}

	// Delete the old polygons
	{
		static TArray<FPolygonID> PolygonIDsToDelete;
		PolygonIDsToDelete.Reset();
		PolygonIDsToDelete.Reserve( PolygonsToSplit.Num() );
		for( const FPolygonToSplit& PolygonToSplit : PolygonsToSplit )
		{
			PolygonIDsToDelete.Add( PolygonToSplit.PolygonID );
		}

		const bool bDeleteOrphanEdges = false;
		const bool bDeleteOrphanVertices = false;
		const bool bDeleteOrphanVertexInstances = false;
		const bool bDeleteEmptyPolygonGroups = false;
		DeletePolygons( PolygonIDsToDelete, bDeleteOrphanEdges, bDeleteOrphanVertices, bDeleteOrphanVertexInstances, bDeleteEmptyPolygonGroups );
	}

	// Create new polygons that are split appropriately and connect to the new vertices we've added
	static TArray< FPolygonID > NewPolygonIDs;
	NewPolygonIDs.Reset();
	static TArray< FEdgeID > NewEdgeIDs;
	NewEdgeIDs.Reset();
	CreatePolygons( PolygonsToCreate, /* Out */ NewPolygonIDs, /* Out */ NewEdgeIDs );

	OutNewEdgeIDs.Append( NewEdgeIDs );

	EM_EXIT( TEXT( "SplitPolygons returned %s" ), *LogHelpers::ArrayToString( OutNewEdgeIDs ) );
}


void UEditableMesh::DeleteEdgeAndConnectedPolygons( const FEdgeID EdgeID, const bool bDeleteOrphanedEdges, const bool bDeleteOrphanedVertices, const bool bDeleteOrphanedVertexInstances, const bool bDeleteEmptyPolygonGroups )
{
	EM_ENTER( TEXT( "DeleteEdgeAndConnectedPolygons: %s %s %s %s %s" ), *EdgeID.ToString(), *LogHelpers::BoolToString( bDeleteOrphanedEdges ), *LogHelpers::BoolToString( bDeleteOrphanedVertices ), *LogHelpers::BoolToString( bDeleteOrphanedVertexInstances ), *LogHelpers::BoolToString( bDeleteEmptyPolygonGroups ) );

	static TArray<FPolygonID> PolygonIDsToDelete;
	PolygonIDsToDelete.Reset();

	const int32 ConnectedPolygonCount = GetEdgeConnectedPolygonCount( EdgeID );
	for( int32 ConnectedPolygonNumber = 0; ConnectedPolygonNumber < ConnectedPolygonCount; ++ConnectedPolygonNumber )
	{
		const FPolygonID PolygonID = GetEdgeConnectedPolygon( EdgeID, ConnectedPolygonNumber );

		// Although it can be uncommon, it's possible the edge is connecting the same polygon to itself.  We need to add uniquely.
		PolygonIDsToDelete.AddUnique( PolygonID );
	}

	// Delete the polygons
	DeletePolygons( PolygonIDsToDelete, bDeleteOrphanedEdges, bDeleteOrphanedVertices, bDeleteOrphanedVertexInstances, bDeleteEmptyPolygonGroups );

	// If the caller asked us not deleted orphaned edges, our edge-to-delete will still be hanging around.  Let's go
	// and delete it now.
	if( !bDeleteOrphanedEdges )
	{
		// NOTE: Because we didn't delete any orphaned edges, the incoming edge ID should still be valid
		static TArray<FEdgeID> EdgeIDsToDelete;
		EdgeIDsToDelete.Reset();
		EdgeIDsToDelete.Add( EdgeID );

		// This edge MUST be an orphan!
		check( GetEdgeConnectedPolygonCount( EdgeID ) == 0 );

		DeleteEdges( EdgeIDsToDelete, bDeleteOrphanedVertices );
	}

	EM_EXIT( TEXT( "DeleteEdgeAndConnectedPolygons returned" ) );
}


void UEditableMesh::DeleteVertexAndConnectedEdgesAndPolygons( const FVertexID VertexID, const bool bDeleteOrphanedEdges, const bool bDeleteOrphanedVertices, const bool bDeleteOrphanedVertexInstances, const bool bDeleteEmptyPolygonGroups )
{
	EM_ENTER( TEXT( "DeleteVertexAndConnectedEdgesAndPolygons: %s %s %s %s %s" ), *VertexID.ToString(), *LogHelpers::BoolToString( bDeleteOrphanedEdges ), *LogHelpers::BoolToString( bDeleteOrphanedVertices ), *LogHelpers::BoolToString( bDeleteOrphanedVertexInstances ), *LogHelpers::BoolToString( bDeleteEmptyPolygonGroups ) );

	static TArray<FEdgeID> EdgeIDsToDelete;
	EdgeIDsToDelete.Reset();

	const int32 ConnectedEdgeCount = GetVertexConnectedEdgeCount( VertexID );
	for( int32 ConnectedEdgeNumber = 0; ConnectedEdgeNumber < ConnectedEdgeCount; ++ConnectedEdgeNumber )
	{
		const FEdgeID ConnectedEdgeID = GetVertexConnectedEdge( VertexID, ConnectedEdgeNumber );
		EdgeIDsToDelete.Add( ConnectedEdgeID );
	}

	for( const FEdgeID EdgeIDToDelete : EdgeIDsToDelete )
	{
		// Make sure the edge still exists.  It may have been deleted as a polygon's edges were deleted during
		// a previous iteration through this loop.
		if( IsValidEdge( EdgeIDToDelete ) )
		{
			DeleteEdgeAndConnectedPolygons( EdgeIDToDelete, bDeleteOrphanedEdges, bDeleteOrphanedVertices, bDeleteOrphanedVertexInstances, bDeleteEmptyPolygonGroups );
		}
	}

	EM_EXIT( TEXT( "DeleteVertexAndConnectedEdgesAndPolygons returned" ) );
}


void UEditableMesh::DeleteOrphanVertices( const TArray<FVertexID>& VertexIDsToDelete )
{
	EM_ENTER( TEXT( "DeleteOrphanVertices: %s" ), *LogHelpers::ArrayToString( VertexIDsToDelete ) );

	FVertexArray& Vertices = GetMeshDescription()->Vertices();

	// Back everything up
	{
		FCreateVerticesChangeInput RevertInput;
		RevertInput.VerticesToCreate.Reserve( VertexIDsToDelete.Num() );

		// NOTE: We iterate backwards, to restore vertices in the opposite order that we deleted them
		for( int32 VertexNumber = VertexIDsToDelete.Num() - 1; VertexNumber >= 0; --VertexNumber )
		{
			const FVertexID VertexID = VertexIDsToDelete[ VertexNumber ];

			// Make sure the vertex is truly an orphan.  We're not going to be able to restore its polygon vertex attributes,
			// because the polygons won't exist when we're restoring the change
			const FMeshVertex& Vertex = Vertices[ VertexID ];
			check( Vertex.ConnectedEdgeIDs.Num() == 0 );
			check( Vertex.VertexInstanceIDs.Num() == 0 );

			RevertInput.VerticesToCreate.Emplace();
			FVertexToCreate& VertexToCreate = RevertInput.VerticesToCreate.Last();

			VertexToCreate.OriginalVertexID = VertexID;

			BackupAllAttributes( VertexToCreate.VertexAttributes, GetMeshDescription()->VertexAttributes(), VertexID );
		}

		AddUndo( MakeUnique<FCreateVerticesChange>( MoveTemp( RevertInput ) ) );
	}

	// Give the adapter a chance to handle this before they are deleted
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnDeleteOrphanVertices( this, VertexIDsToDelete );
	}
	
	// Actually delete the vertices
	{
		for( const FVertexID VertexIDToDelete : VertexIDsToDelete )
		{
			VerticesPendingMerging.Remove( VertexIDToDelete );
			GetMeshDescription()->DeleteVertex( VertexIDToDelete );
            VerticesPendingMerging.Remove(VertexIDToDelete);
		}
	}

	EM_EXIT( TEXT( "DeleteOrphanVertices returned" ) );
}


void UEditableMesh::DeleteVertexInstances( const TArray<FVertexInstanceID>& VertexInstanceIDsToDelete, const bool bDeleteOrphanedVertices )
{
	EM_ENTER( TEXT( "DeleteVertexInstances: %s" ), *LogHelpers::ArrayToString( VertexInstanceIDsToDelete ) );

	// Back everything up
	{
		FCreateVertexInstancesChangeInput RevertInput;
		RevertInput.VertexInstancesToCreate.Reserve( VertexInstanceIDsToDelete.Num() );

		// NOTE: We iterate backwards, to restore vertices in the opposite order that we deleted them
		for( int32 VertexNumber = VertexInstanceIDsToDelete.Num() - 1; VertexNumber >= 0; --VertexNumber )
		{
			const FVertexInstanceID VertexInstanceID = VertexInstanceIDsToDelete[ VertexNumber ];

			// Back up properties
			RevertInput.VertexInstancesToCreate.Emplace();
			FVertexInstanceToCreate& VertexInstanceToCreate = RevertInput.VertexInstancesToCreate.Last();

			VertexInstanceToCreate.VertexID = GetMeshDescription()->GetVertexInstanceVertex( VertexInstanceID );
			VertexInstanceToCreate.OriginalVertexInstanceID = VertexInstanceID;

			BackupAllAttributes( VertexInstanceToCreate.VertexInstanceAttributes, GetMeshDescription()->VertexInstanceAttributes(), VertexInstanceID );
		}

		AddUndo( MakeUnique<FCreateVertexInstancesChange>( MoveTemp( RevertInput ) ) );
	}

	// Give the adapter a chance to do something with this event before it happens
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnDeleteVertexInstances( this, VertexInstanceIDsToDelete );
	}

	static TArray<FVertexID> OrphanedVertexIDs;
	OrphanedVertexIDs.Reset();

	// Actually delete the vertex instances
	{
		for( const FVertexInstanceID VertexInstanceIDToDelete : VertexInstanceIDsToDelete )
		{
			VerticesPendingMerging.Add( GetMeshDescription()->GetVertexInstanceVertex( VertexInstanceIDToDelete ) );
			GetMeshDescription()->DeleteVertexInstance( VertexInstanceIDToDelete, bDeleteOrphanedVertices ? &OrphanedVertexIDs : nullptr );
		}
	}

	// Delete orphaned vertices, if there are any.
	if( OrphanedVertexIDs.Num() > 0 )
	{
		DeleteOrphanVertices( OrphanedVertexIDs );
	}

	EM_EXIT( TEXT( "DeleteVertexInstances returned" ) );
}


void UEditableMesh::DeleteEdges( const TArray<FEdgeID>& EdgeIDsToDelete, const bool bDeleteOrphanedVertices )
{
	EM_ENTER( TEXT( "DeleteEdges: %s %s" ), *LogHelpers::ArrayToString( EdgeIDsToDelete ), *LogHelpers::BoolToString( bDeleteOrphanedVertices ) );

	FEdgeArray& Edges = GetMeshDescription()->Edges();
	FVertexArray& Vertices = GetMeshDescription()->Vertices();

	// Back everything up
	{
		FCreateEdgesChangeInput RevertInput;

		// NOTE: We iterate backwards, to restore edges in the opposite order that we deleted them
		for( int32 EdgeNumber = EdgeIDsToDelete.Num() - 1; EdgeNumber >= 0; --EdgeNumber )
		{
			const FEdgeID EdgeID = EdgeIDsToDelete[ EdgeNumber ];
			const FMeshEdge& Edge = Edges[ EdgeID ];

			RevertInput.EdgesToCreate.Emplace();
			FEdgeToCreate& EdgeToCreate = RevertInput.EdgesToCreate.Last();

			EdgeToCreate.OriginalEdgeID = EdgeID;
			EdgeToCreate.VertexID0 = Edge.VertexIDs[ 0 ];
			EdgeToCreate.VertexID1 = Edge.VertexIDs[ 1 ];
			EdgeToCreate.ConnectedPolygons = Edge.ConnectedPolygons;
			
			BackupAllAttributes( EdgeToCreate.EdgeAttributes, GetMeshDescription()->EdgeAttributes(), EdgeID );
		}

		AddUndo( MakeUnique<FCreateEdgesChange>( MoveTemp( RevertInput ) ) );
	}

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnDeleteEdges( this, EdgeIDsToDelete );
	}

	// Delete the edges
	{
		static TArray<FVertexID> OrphanedVertexIDs;
		OrphanedVertexIDs.Reset();

		for( const FEdgeID EdgeID : EdgeIDsToDelete )
		{
			GetMeshDescription()->DeleteEdge( EdgeID, bDeleteOrphanedVertices ? &OrphanedVertexIDs : nullptr );
		}

		// If we orphaned any vertices and we were asked to delete those, then we'll go ahead and do that now.
		if( OrphanedVertexIDs.Num() > 0 )
		{
			DeleteOrphanVertices( OrphanedVertexIDs );
		}
	}

	EM_EXIT( TEXT( "DeleteEdges returned" ) );
}


void UEditableMesh::CreateEmptyVertexRange( const int32 NumVerticesToCreate, TArray<FVertexID>& OutNewVertexIDs )
{
	EM_ENTER( TEXT( "CreateEmptyVertexRange: %d" ), NumVerticesToCreate );

	OutNewVertexIDs.Reset( NumVerticesToCreate );

	// Create vertices
	{
		GetMeshDescription()->ReserveNewVertices( NumVerticesToCreate );
		for( int32 Count = 0; Count < NumVerticesToCreate; ++Count )
		{
			OutNewVertexIDs.Add( GetMeshDescription()->CreateVertex() );
		}
	}

	// NOTE: We iterate backwards, to delete vertices in the opposite order that we added them
	{
		FDeleteOrphanVerticesChangeInput RevertInput;
		RevertInput.VertexIDsToDelete.Reserve( NumVerticesToCreate );
		for( int32 VertexNumber = OutNewVertexIDs.Num() - 1; VertexNumber >= 0; --VertexNumber )
		{
			RevertInput.VertexIDsToDelete.Add( OutNewVertexIDs[ VertexNumber ] );
		}

		AddUndo( MakeUnique<FDeleteOrphanVerticesChange>( MoveTemp( RevertInput ) ) );
	}

	// Advise the adapter of new vertices
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnCreateEmptyVertexRange( this, OutNewVertexIDs );
	}

	EM_EXIT( TEXT( "CreateEmptyVertexRange returned %s" ), *LogHelpers::ArrayToString( OutNewVertexIDs ) );
}


void UEditableMesh::CreateVertices( const TArray<FVertexToCreate>& VerticesToCreate, TArray<FVertexID>& OutNewVertexIDs )
{
	EM_ENTER( TEXT( "CreateVertices: %s" ), *LogHelpers::ArrayToString( VerticesToCreate ) );

	// Create vertices
	{
		OutNewVertexIDs.Reset( VerticesToCreate.Num() );

		GetMeshDescription()->ReserveNewVertices( VerticesToCreate.Num() );
		for( const FVertexToCreate& VertexToCreate : VerticesToCreate )
		{
			FVertexID VertexID = VertexToCreate.OriginalVertexID;
			if( VertexID != FVertexID::Invalid )
			{
				GetMeshDescription()->CreateVertexWithID( VertexID );
			}
			else
			{
				VertexID = GetMeshDescription()->CreateVertex();
			}

			OutNewVertexIDs.Add( VertexID );
		}
	}

	// NOTE: We iterate backwards, to delete vertices in the opposite order that we added them
	{
		FDeleteOrphanVerticesChangeInput RevertInput;
		RevertInput.VertexIDsToDelete.Reserve( VerticesToCreate.Num() );
		for( int32 VertexNumber = OutNewVertexIDs.Num() - 1; VertexNumber >= 0; --VertexNumber )
		{
			RevertInput.VertexIDsToDelete.Add( OutNewVertexIDs[ VertexNumber ] );
		}

		AddUndo( MakeUnique<FDeleteOrphanVerticesChange>( MoveTemp( RevertInput ) ) );
	}

	// Advise the adapter of new vertices
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnCreateVertices( this, OutNewVertexIDs );
	}

	// Set new vertex attributes
	for( int32 Index = 0; Index < OutNewVertexIDs.Num(); ++Index )
	{
		for( const FMeshElementAttributeData& VertexAttribute : VerticesToCreate[ Index ].VertexAttributes.Attributes )
		{
			SetVertexAttribute( OutNewVertexIDs[ Index ], VertexAttribute );
		}
	}

	EM_EXIT( TEXT( "CreateVertices returned %s" ), *LogHelpers::ArrayToString( OutNewVertexIDs ) );
}


void UEditableMesh::CreateVertexInstances( const TArray<FVertexInstanceToCreate>& VertexInstancesToCreate, TArray<FVertexInstanceID>& OutNewVertexInstanceIDs )
{
	EM_ENTER( TEXT( "CreateVertexInstances: %s" ), *LogHelpers::ArrayToString( VertexInstancesToCreate ) );

	// Create new vertex instances
	{
		OutNewVertexInstanceIDs.Reset( VertexInstancesToCreate.Num() );

		GetMeshDescription()->ReserveNewVertexInstances( VertexInstancesToCreate.Num() );
		for( const FVertexInstanceToCreate& VertexInstanceToCreate : VertexInstancesToCreate )
		{
			FVertexInstanceID VertexInstanceID = VertexInstanceToCreate.OriginalVertexInstanceID;
			if( VertexInstanceID != FVertexInstanceID::Invalid )
			{
				GetMeshDescription()->CreateVertexInstanceWithID( VertexInstanceID, VertexInstanceToCreate.VertexID );
			}
			else
			{
				VertexInstanceID = GetMeshDescription()->CreateVertexInstance( VertexInstanceToCreate.VertexID );
			}

			OutNewVertexInstanceIDs.Add( VertexInstanceID );
			VerticesPendingMerging.Add( VertexInstanceToCreate.VertexID );
		}
	}

	// NOTE: We iterate backwards, to delete vertex instances in the opposite order that we added them
	{
		FDeleteVertexInstancesChangeInput RevertInput;
		RevertInput.bDeleteOrphanedVertices = false;
		RevertInput.VertexInstanceIDsToDelete.Reserve( OutNewVertexInstanceIDs.Num() );
		for( int32 VertexNumber = OutNewVertexInstanceIDs.Num() - 1; VertexNumber >= 0; --VertexNumber )
		{
			RevertInput.VertexInstanceIDsToDelete.Add( OutNewVertexInstanceIDs[ VertexNumber ] );
		}

		AddUndo( MakeUnique<FDeleteVertexInstancesChange>( MoveTemp( RevertInput ) ) );
	}

	// Advise the adapter of new vertex instances
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnCreateVertexInstances( this, OutNewVertexInstanceIDs );
	}

	// Now set vertex instance attributes
	for( int32 Index = 0; Index < OutNewVertexInstanceIDs.Num(); ++Index )
	{
		for( const FMeshElementAttributeData& VertexInstanceAttribute : VertexInstancesToCreate[ Index ].VertexInstanceAttributes.Attributes )
		{
			SetVertexInstanceAttribute( OutNewVertexInstanceIDs[ Index ], VertexInstanceAttribute );
		}
	}

	EM_EXIT( TEXT( "CreateVertexInstances returned %s" ), *LogHelpers::ArrayToString( OutNewVertexInstanceIDs ) );
}


void UEditableMesh::CreateEdges( const TArray<FEdgeToCreate>& EdgesToCreate, TArray<FEdgeID>& OutNewEdgeIDs )
{
	EM_ENTER( TEXT( "CreateEdges: %s" ), *LogHelpers::ArrayToString( EdgesToCreate ) );

	// Create new edges in the mesh description
	{
		OutNewEdgeIDs.Reset( EdgesToCreate.Num() );

		GetMeshDescription()->ReserveNewEdges( EdgesToCreate.Num() );
		for( const FEdgeToCreate& EdgeToCreate : EdgesToCreate )
		{
			FEdgeID EdgeID = EdgeToCreate.OriginalEdgeID;
			if( EdgeID != FEdgeID::Invalid )
			{
				GetMeshDescription()->CreateEdgeWithID( EdgeID, EdgeToCreate.VertexID0, EdgeToCreate.VertexID1, EdgeToCreate.ConnectedPolygons );
			}
			else
			{
				EdgeID = GetMeshDescription()->CreateEdge( EdgeToCreate.VertexID0, EdgeToCreate.VertexID1, EdgeToCreate.ConnectedPolygons );
			}

			OutNewEdgeIDs.Add( EdgeID );
		}
	}

	// NOTE: We iterate backwards, to delete edges in the opposite order that we added them
	{
		FDeleteEdgesChangeInput RevertInput;
		RevertInput.bDeleteOrphanedVertices = false;	// Don't delete any vertices on revert.  We're only creating edges here, not vertices!
		RevertInput.EdgeIDsToDelete.Reserve( EdgesToCreate.Num() );
		for( int32 EdgeNumber = OutNewEdgeIDs.Num() - 1; EdgeNumber >= 0; --EdgeNumber )
		{
			RevertInput.EdgeIDsToDelete.Add( OutNewEdgeIDs[ EdgeNumber ] );
		}

		AddUndo( MakeUnique<FDeleteEdgesChange>( MoveTemp( RevertInput ) ) );
	}

	// Advise the adapter that edges have been created
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnCreateEdges( this, OutNewEdgeIDs );
	}

	for( int32 Index = 0; Index < OutNewEdgeIDs.Num(); ++Index )
	{
		for( const FMeshElementAttributeData& EdgeAttribute : EdgesToCreate[ Index ].EdgeAttributes.Attributes )
		{
			SetEdgeAttribute( OutNewEdgeIDs[ Index ], EdgeAttribute );
		}
	}

	EM_EXIT( TEXT( "CreateEdges returned %s" ), *LogHelpers::ArrayToString( OutNewEdgeIDs ) );
}


FVertexInstanceID UEditableMesh::CreateVertexInstanceForContourVertex( const FVertexAndAttributes& ContourVertex, const FPolygonID PolygonID )
{
	FVertexInstanceID NewVertexInstanceID = ContourVertex.VertexInstanceID;

	if( NewVertexInstanceID == FVertexInstanceID::Invalid )
	{
		const FVertexID VertexID = ContourVertex.VertexID;
		check( VertexID != FVertexID::Invalid );

		// For now, always create a new vertex instance.
		// @todo mesheditor: revisit splitting/welding identical vertex instances.
		// This should be done as a post-step at the end of the transaction, according to adjacent edge hardness.

		FVertexInstanceToCreate VertexInstanceToCreate;
		VertexInstanceToCreate.VertexID = VertexID;
		VertexInstanceToCreate.VertexInstanceAttributes = ContourVertex.PolygonVertexAttributes;

		static TArray<FVertexInstanceToCreate> VertexInstancesToCreate;
		VertexInstancesToCreate.Reset();
		VertexInstancesToCreate.Add( VertexInstanceToCreate );

		static TArray<FVertexInstanceID> CreatedVertexInstances;
		CreateVertexInstances( VertexInstancesToCreate, CreatedVertexInstances );
		check( CreatedVertexInstances.Num() == 1 );
		NewVertexInstanceID = CreatedVertexInstances[ 0 ];
	}
	else
	{
		// Cannot specify both a vertex instance ID and a vertex ID
		check( ContourVertex.VertexID == FVertexID::Invalid );
	}

	return NewVertexInstanceID;
}


void UEditableMesh::CreatePolygonContour( const TArray<FVertexAndAttributes>& Contour, TArray<FVertexInstanceID>& OutVertexInstanceIDs )
{
	// All polygons must have at least three vertices
	const int32 NumContourVertices = Contour.Num();
	check( NumContourVertices >= 3 );

	OutVertexInstanceIDs.SetNumUninitialized( NumContourVertices );

	static TArray<FVertexInstanceToCreate> VertexInstancesToCreate;
	VertexInstancesToCreate.Reset();

	// Assign vertex instances to the polygon
	for( int32 VertexNumber = 0; VertexNumber < NumContourVertices; ++VertexNumber )
	{
		// Copy the supplied vertex instance ID into the output.
		// If it is valid, there is nothing more to be done.
		// If it is not valid, copy the invalid ID. They will be replaced by the IDs of the new vertex instances in a later pass.
		OutVertexInstanceIDs[ VertexNumber ] = Contour[ VertexNumber ].VertexInstanceID;

		if( Contour[ VertexNumber ].VertexInstanceID != FVertexInstanceID::Invalid )
		{
			// Cannot specify both a vertex instance ID and a vertex ID
			check( Contour[ VertexNumber ].VertexID == FVertexID::Invalid );

			// Vertex instance ID was specified; nothing more to do, it has already been output directly
			// @todo editable mesh: THIS WILL BREAK if you specify a vertex instance which is being used by a polygon the other side of a hard edge.
			// Decide what to do about this.
		}
		else
		{
			// We need to create a vertex instance; check that the vertex ID is valid
			const FVertexID VertexID = Contour[ VertexNumber ].VertexID;
			check( VertexID != FVertexID::Invalid );

			// For now, always create a new vertex instance. It will be merged with an existing one later if appropriate.
			VertexInstancesToCreate.Emplace();
			FVertexInstanceToCreate& VertexInstanceToCreate = VertexInstancesToCreate.Last();
			VertexInstanceToCreate.VertexID = VertexID;
			VertexInstanceToCreate.VertexInstanceAttributes = Contour[ VertexNumber ].PolygonVertexAttributes;

			// Add vertex to list of potential vertices to be merged at the end of the operation
			VerticesPendingMerging.Add( VertexID );
		}
	}

	// Create any vertex instances necessary
	if( VertexInstancesToCreate.Num() > 0 )
	{
		static TArray<FVertexInstanceID> NewVertexInstanceIDs;
		NewVertexInstanceIDs.Reset( NumContourVertices );

		CreateVertexInstances( VertexInstancesToCreate, NewVertexInstanceIDs );

		// Fill in missing vertex instance indices
		int32 NewVertexInstanceIndex = 0;

		for( FVertexInstanceID& VertexInstanceID : OutVertexInstanceIDs )
		{
			if( VertexInstanceID == FVertexInstanceID::Invalid )
			{
				VertexInstanceID = NewVertexInstanceIDs[ NewVertexInstanceIndex ];
				NewVertexInstanceIndex++;
			}
		}
	}
}


void UEditableMesh::CreatePolygons( const TArray<FPolygonToCreate>& PolygonsToCreate, TArray<FPolygonID>& OutNewPolygonIDs, TArray<FEdgeID>& OutNewEdgeIDs )
{
	EM_ENTER( TEXT( "CreatePolygons: %s" ), *LogHelpers::ArrayToString( PolygonsToCreate ) );

	OutNewPolygonIDs.Reset( PolygonsToCreate.Num() );
	OutNewEdgeIDs.Reset();

	// Actually create the polygons
	{
		static TArray<FAttributesForEdge> AttributesForEdges;
		AttributesForEdges.Reset();

		GetMeshDescription()->ReserveNewPolygons( PolygonsToCreate.Num() );
		for( const FPolygonToCreate& PolygonToCreate : PolygonsToCreate )
		{
			static TArray<FVertexInstanceID> PerimeterVertexInstances;
			static TArray<FEdgeID> NewEdgeIDsForContour;

			CreatePolygonContour( PolygonToCreate.PerimeterVertices, PerimeterVertexInstances );

			FPolygonID PolygonID = PolygonToCreate.OriginalPolygonID;
			if( PolygonID != FPolygonID::Invalid )
			{
				GetMeshDescription()->CreatePolygonWithID( PolygonID, PolygonToCreate.PolygonGroupID, PerimeterVertexInstances, &NewEdgeIDsForContour );
			}
			else
			{
				PolygonID = GetMeshDescription()->CreatePolygon( PolygonToCreate.PolygonGroupID, PerimeterVertexInstances, &NewEdgeIDsForContour );
			}

			OutNewPolygonIDs.Add( PolygonID );
			OutNewEdgeIDs.Append( NewEdgeIDsForContour );

			// Set edge hardness for polygon edges according to PolygonToCreate mode
			const int32 ExistingEdgeCount = PerimeterVertexInstances.Num() - NewEdgeIDsForContour.Num();
			AttributesForEdges.Reserve( AttributesForEdges.Num() + ExistingEdgeCount );

			// Set directly hardness of any edges which were automatically created by the polygon.
			// This needn't be done transactionally as we explicitly add the undo action later on.
			const bool bHardEdge = ( PolygonToCreate.PolygonEdgeHardness == EPolygonEdgeHardness::AllEdgesHard || PolygonToCreate.PolygonEdgeHardness == EPolygonEdgeHardness::NewEdgesHard );
			for( const FEdgeID EdgeID : OutNewEdgeIDs )
			{
				GetMeshDescription()->EdgeAttributes().SetAttribute( EdgeID, MeshAttribute::Edge::IsHard, 0, bHardEdge );
			}

			// If we are setting all edges' hardnesses (not just new ones), set existing edges' attributes here.
			// This will also split any vertex instances which have just been included in a polygon if necessary.
			if( ExistingEdgeCount > 0 &&
				( PolygonToCreate.PolygonEdgeHardness == EPolygonEdgeHardness::AllEdgesHard || PolygonToCreate.PolygonEdgeHardness == EPolygonEdgeHardness::AllEdgesSoft ) )
			{
				FVertexInstanceID LastVertexInstanceID = PerimeterVertexInstances.Last();
				for( const FVertexInstanceID VertexInstanceID : PerimeterVertexInstances )
				{
					const FVertexID VertexID0 = GetMeshDescription()->GetVertexInstanceVertex( VertexInstanceID );
					const FVertexID VertexID1 = GetMeshDescription()->GetVertexInstanceVertex( LastVertexInstanceID );
					const FEdgeID EdgeID = GetMeshDescription()->GetVertexPairEdge( VertexID0, VertexID1 );
					check( EdgeID != FEdgeID::Invalid );
					if( !NewEdgeIDsForContour.Contains( EdgeID ) )
					{
						AttributesForEdges.Emplace();
						FAttributesForEdge& AttributesForEdge = AttributesForEdges.Last();
						AttributesForEdge.EdgeID = EdgeID;
						AttributesForEdge.EdgeAttributes.Attributes.Emplace( MeshAttribute::Edge::IsHard, 0, FMeshElementAttributeValue( bHardEdge ) );
					}
					LastVertexInstanceID = VertexInstanceID;
				}
			}
		}

		SetEdgesAttributes( AttributesForEdges );
	}

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnCreatePolygons( this, OutNewPolygonIDs );
	}

	// Update spatial database
	if( Octree.IsValid() )
	{
		NewOctreePolygonIDs.Append( OutNewPolygonIDs );
	}

	// Generate tangent basis for the polygon
	PolygonsPendingNewTangentBasis.Append( OutNewPolygonIDs );

	// Generate triangles for the new polygon
	PolygonsPendingTriangulation.Append( OutNewPolygonIDs );

	// If any new edges were automatically created when creating the polygons, add an action to the undo stack to delete them here
	if( OutNewEdgeIDs.Num() > 0 )
	{
		FDeleteEdgesChangeInput RevertInput;
		RevertInput.bDeleteOrphanedVertices = false;
		RevertInput.EdgeIDsToDelete.Reserve( OutNewEdgeIDs.Num() );
		for( int32 EdgeNumber = OutNewEdgeIDs.Num() - 1; EdgeNumber >= 0; --EdgeNumber )
		{
			RevertInput.EdgeIDsToDelete.Add( OutNewEdgeIDs[ EdgeNumber ] );
		}

		AddUndo( MakeUnique<FDeleteEdgesChange>( MoveTemp( RevertInput ) ) );
	}

	// NOTE: We iterate backwards, to delete polygons in the opposite order that we added them
	{
		FDeletePolygonsChangeInput RevertInput;
		RevertInput.PolygonIDsToDelete.Reserve( PolygonsToCreate.Num() );
		RevertInput.bDeleteOrphanedEdges = false;	// Don't delete any edges on revert.  We're only creating polygons here, not vertices!
		RevertInput.bDeleteOrphanedVertices = false;	// Don't delete any vertices on revert.  We're only creating polygons here, not vertices!
		RevertInput.bDeleteOrphanedVertexInstances = false;
		RevertInput.bDeleteEmptySections = false;
		for( int32 PolygonNumber = OutNewPolygonIDs.Num() - 1; PolygonNumber >= 0; --PolygonNumber )
		{
			RevertInput.PolygonIDsToDelete.Add( OutNewPolygonIDs[ PolygonNumber ] );
		}

		AddUndo( MakeUnique<FDeletePolygonsChange>( MoveTemp( RevertInput ) ) );
	}

	EM_EXIT( TEXT( "CreatePolygons returned %s, %s" ), *LogHelpers::ArrayToString( OutNewPolygonIDs ), *LogHelpers::ArrayToString( OutNewEdgeIDs ) );
}


void UEditableMesh::BackupPolygonContour( const FMeshPolygonContour& Contour, TArray<FVertexAndAttributes>& OutVerticesAndAttributes )
{
	const FVertexInstanceArray& VertexInstances = GetMeshDescription()->VertexInstances();

	OutVerticesAndAttributes.Reserve( Contour.VertexInstanceIDs.Num() );
	for( const FVertexInstanceID VertexInstanceID : Contour.VertexInstanceIDs )
	{
		const FMeshVertexInstance& VertexInstance = VertexInstances[ VertexInstanceID ];

		OutVerticesAndAttributes.Emplace();
		FVertexAndAttributes& VertexAndAttributes = OutVerticesAndAttributes.Last();

		// We rely on undoing recreating vertex instances, therefore we only need pass their IDs.
		VertexAndAttributes.VertexInstanceID = VertexInstanceID;
		VertexAndAttributes.VertexID = FVertexID::Invalid;
	}
}


void UEditableMesh::DeletePolygons( const TArray<FPolygonID>& PolygonIDsToDelete, const bool bDeleteOrphanedEdges, const bool bDeleteOrphanedVertices, const bool bDeleteOrphanedVertexInstances, const bool bDeleteEmptyPolygonGroups )
{
	EM_ENTER( TEXT( "DeletePolygons: %s" ), *LogHelpers::ArrayToString( PolygonIDsToDelete ) );

	FPolygonArray& Polygons = GetMeshDescription()->Polygons();
	FPolygonGroupArray& PolygonGroups = GetMeshDescription()->PolygonGroups();

	// Back everything up
	{
		FCreatePolygonsChangeInput RevertInput;
		RevertInput.PolygonsToCreate.Reserve( PolygonIDsToDelete.Num() );

		// NOTE: We iterate backwards, to restore edges in the opposite order that we deleted them
		for( int32 PolygonNumber = PolygonIDsToDelete.Num() - 1; PolygonNumber >= 0; --PolygonNumber )
		{
			const FPolygonID PolygonID = PolygonIDsToDelete[ PolygonNumber ];
			const FMeshPolygon& Polygon = Polygons[ PolygonID ];

			RevertInput.PolygonsToCreate.Emplace();
			FPolygonToCreate& PolygonToCreate = RevertInput.PolygonsToCreate.Last();

			PolygonToCreate.PolygonGroupID = Polygon.PolygonGroupID;
			PolygonToCreate.OriginalPolygonID = PolygonID;

			BackupPolygonContour( Polygon.PerimeterContour, PolygonToCreate.PerimeterVertices );
		}

		AddUndo( MakeUnique<FCreatePolygonsChange>( MoveTemp( RevertInput ) ) );
	}

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnDeletePolygons( this, PolygonIDsToDelete );
	}

	// Delete the polygons
	{
		static TArray<FEdgeID> OrphanedEdgeIDs;
		OrphanedEdgeIDs.Reset();

		static TArray<FVertexInstanceID> OrphanedVertexInstanceIDs;
		OrphanedVertexInstanceIDs.Reset();

		static TArray<FPolygonGroupID> EmptyPolygonGroupIDs;
		EmptyPolygonGroupIDs.Reset();

		for( const FPolygonID PolygonID : PolygonIDsToDelete )
		{
			GetMeshDescription()->DeletePolygon( PolygonID, 
				bDeleteOrphanedEdges ? &OrphanedEdgeIDs : nullptr,
				bDeleteOrphanedVertexInstances ? &OrphanedVertexInstanceIDs : nullptr,
				bDeleteEmptyPolygonGroups ? &EmptyPolygonGroupIDs : nullptr );

			// Update spatial database
			if( Octree.IsValid() )
			{
				// If the polygon we're deleting is still pending being added to the octree, the only thing we need to do is remove it from 
				// the set of newly added polygons
				if( NewOctreePolygonIDs.Remove( PolygonID ) == 0 )
				{
					// The polygon wasn't in our set of newly added polygons, so it must have been committed to the octree already.  We'll
					// enqueue it for deletion here.
					DeletedOctreePolygonIDs.Add( PolygonID );
				}
			}
		}

		// Remove vertex instances which are exclusively used by this polygon.
		// We do not want this to remove orphaned vertices; this will optionally happen below when removing edges.
		if( OrphanedVertexInstanceIDs.Num() > 0 )
		{
			const bool bDeleteVertices = false;
			DeleteVertexInstances( OrphanedVertexInstanceIDs, bDeleteVertices );
		}

		// Remove any edges which may have been orphaned. This may also optionally remove any orphaned vertices.
		// We can do this here because we know any edges which were orphaned will have had only a single vertex instance at each vertex.
		// Therefore the vertex will now have no instances further to deleting them above.
		// Note: there is never a situation where there could be orphaned vertices but not orphaned edges.
		if( OrphanedEdgeIDs.Num() > 0 )
		{
			DeleteEdges( OrphanedEdgeIDs, bDeleteOrphanedVertices );
		}

		// Remove any empty polygon groups which may have resulted
		if( EmptyPolygonGroupIDs.Num() > 0 )
		{
			DeletePolygonGroups( EmptyPolygonGroupIDs );
		}
	}

	// If any of these polygons are in the pending list for triangulation or computing a new tangent basis, remove them
	for( const FPolygonID PolygonID : PolygonIDsToDelete )
	{
		PolygonsPendingNewTangentBasis.Remove( PolygonID );
		PolygonsPendingTriangulation.Remove( PolygonID );
	}

	EM_EXIT( TEXT( "DeletePolygons returned" ) );
}


void UEditableMesh::CreatePolygonGroups( const TArray<FPolygonGroupToCreate>& PolygonGroupsToCreate, TArray<FPolygonGroupID>& OutNewPolygonGroupIDs )
{
	EM_ENTER( TEXT( "CreatePolygonGroups: %s" ), *LogHelpers::ArrayToString( PolygonGroupsToCreate ) );

	// Create polygon groups and initialize them
	{
		FPolygonGroupArray& PolygonGroups = GetMeshDescription()->PolygonGroups();

		OutNewPolygonGroupIDs.Reset( PolygonGroupsToCreate.Num() );

		// Reserve elements
		GetMeshDescription()->ReserveNewPolygonGroups( PolygonGroupsToCreate.Num() );

		for( const FPolygonGroupToCreate& PolygonGroupToCreate : PolygonGroupsToCreate )
		{
			// Allocate polygon group
			FPolygonGroupID PolygonGroupID = PolygonGroupToCreate.OriginalPolygonGroupID;
			if( PolygonGroupID != FPolygonGroupID::Invalid )
			{
				GetMeshDescription()->CreatePolygonGroupWithID( PolygonGroupID );
			}
			else
			{
				PolygonGroupID = GetMeshDescription()->CreatePolygonGroup();
			}
			//Apply the PolygonGroup attribute here
			for (const FMeshElementAttributeData& EdgeAttribute : PolygonGroupToCreate.PolygonGroupAttributes.Attributes)
			{
				ApplyAttribute(GetMeshDescription()->PolygonGroupAttributes(), EdgeAttribute, PolygonGroupID);
			}
			OutNewPolygonGroupIDs.Add( PolygonGroupID );
		}
	}

	//Notify adapters that we just create some PolygonGroup
	//This must be call before sending the modified attributes to the adapter
	for (UEditableMeshAdapter* Adapter : Adapters)
	{
		Adapter->OnCreatePolygonGroups(this, OutNewPolygonGroupIDs);
	}

	// Apply attributes and notify the adapter for each modified attributes
	for (int32 Index = 0; Index < OutNewPolygonGroupIDs.Num(); ++Index)
	{
		for (const FMeshElementAttributeData& EdgeAttribute : PolygonGroupsToCreate[Index].PolygonGroupAttributes.Attributes)
		{
			SetPolygonGroupAttribute(OutNewPolygonGroupIDs[Index], EdgeAttribute);
		}
	}

	// Back up
	{
		FDeletePolygonGroupsChangeInput RevertInput;
		RevertInput.PolygonGroupIDs.Reserve( OutNewPolygonGroupIDs.Num() );
		for( int32 Index = OutNewPolygonGroupIDs.Num() - 1; Index >= 0; --Index )
		{
			RevertInput.PolygonGroupIDs.Add( OutNewPolygonGroupIDs[ Index ] );
		}

		AddUndo( MakeUnique<FDeletePolygonGroupsChange>( MoveTemp( RevertInput ) ) );
	}

	EM_EXIT( TEXT( "CreatePolygonGroups returned %s" ), *LogHelpers::ArrayToString( OutNewPolygonGroupIDs ) );
}


void UEditableMesh::DeletePolygonGroups( const TArray<FPolygonGroupID>& PolygonGroupIDs )
{
	EM_ENTER( TEXT( "DeletePolygonGroups: %s" ), *LogHelpers::ArrayToString( PolygonGroupIDs ) );

	FPolygonGroupArray& PolygonGroups = GetMeshDescription()->PolygonGroups();

	// Back everything up
	{
		FCreatePolygonGroupsChangeInput RevertInput;

		for( int32 Index = PolygonGroupIDs.Num() - 1; Index >= 0; --Index )
		{
			const FPolygonGroupID PolygonGroupID = PolygonGroupIDs[ Index ];
			const FMeshPolygonGroup& PolygonGroup = PolygonGroups[ PolygonGroupID ];

			RevertInput.PolygonGroupsToCreate.Emplace();
			FPolygonGroupToCreate& PolygonGroupToCreate = RevertInput.PolygonGroupsToCreate.Last();

			PolygonGroupToCreate.OriginalPolygonGroupID = PolygonGroupID;

			BackupAllAttributes( PolygonGroupToCreate.PolygonGroupAttributes, GetMeshDescription()->PolygonGroupAttributes(), PolygonGroupID );
		}

		AddUndo( MakeUnique<FCreatePolygonGroupsChange>( MoveTemp( RevertInput ) ) );
	}

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnDeletePolygonGroups( this, PolygonGroupIDs );
	}

	// Delete the polygon groups
	{
		for( const FPolygonGroupID PolygonGroupID : PolygonGroupIDs )
		{
			GetMeshDescription()->DeletePolygonGroup( PolygonGroupID );
		}
	}

	EM_EXIT( TEXT( "DeletePolygonGroups returned" ) );
}


void UEditableMesh::SetVerticesAttributes( const TArray<FAttributesForVertex>& AttributesForVertices )
{
	EM_ENTER( TEXT( "SetVerticesAttributes: %s" ), *LogHelpers::ArrayToString( AttributesForVertices ) );

	FSetVerticesAttributesChangeInput RevertInput;

	RevertInput.AttributesForVertices.Reserve( AttributesForVertices.Num() );
	for( const FAttributesForVertex& AttributesForVertex : AttributesForVertices )
	{
		const FVertexID VertexID = AttributesForVertex.VertexID;

		RevertInput.AttributesForVertices.Emplace();
		FAttributesForVertex& RevertVertex = RevertInput.AttributesForVertices.Last();
		RevertVertex.VertexID = VertexID;

		// Back up the attributes
		BackupAttributesInList( RevertVertex.VertexAttributes, AttributesForVertex.VertexAttributes, GetMeshDescription()->VertexAttributes(), VertexID );

		for( const FMeshElementAttributeData& VertexAttribute : AttributesForVertex.VertexAttributes.Attributes )
		{
			// Set the new attribute
			SetVertexAttribute( VertexID, VertexAttribute );
		}
	}

	AddUndo( MakeUnique<FSetVerticesAttributesChange>( MoveTemp( RevertInput ) ) );

	EM_EXIT( TEXT( "SetVerticesAttributes returned" ) );
}


void UEditableMesh::SetVertexInstancesAttributes( const TArray<FAttributesForVertexInstance>& AttributesForVertexInstances )
{
	EM_ENTER( TEXT( "SetVertexInstancesAttributes: %s" ), *LogHelpers::ArrayToString( AttributesForVertexInstances ) );

	FSetVertexInstancesAttributesChangeInput RevertInput;

	RevertInput.AttributesForVertexInstances.Reserve( AttributesForVertexInstances.Num() );
	for( const FAttributesForVertexInstance& AttributesForVertexInstance : AttributesForVertexInstances )
	{
		const FVertexInstanceID VertexInstanceID = AttributesForVertexInstance.VertexInstanceID;

		RevertInput.AttributesForVertexInstances.Emplace();
		FAttributesForVertexInstance& RevertVertexInstance = RevertInput.AttributesForVertexInstances.Last();
		RevertVertexInstance.VertexInstanceID = VertexInstanceID;

		// Back up the attributes
		BackupAttributesInList( RevertVertexInstance.VertexInstanceAttributes, AttributesForVertexInstance.VertexInstanceAttributes, GetMeshDescription()->VertexInstanceAttributes(), VertexInstanceID );

		for( const FMeshElementAttributeData& VertexInstanceAttribute : AttributesForVertexInstance.VertexInstanceAttributes.Attributes )
		{
			// Set the new attribute
			SetVertexInstanceAttribute( VertexInstanceID, VertexInstanceAttribute );
		}

		VerticesPendingMerging.Add( GetMeshDescription()->GetVertexInstanceVertex( VertexInstanceID ) );
	}

	AddUndo( MakeUnique<FSetVertexInstancesAttributesChange>( MoveTemp( RevertInput ) ) );

	EM_EXIT( TEXT( "SetVertexInstancesAttributes returned" ) );
}


void UEditableMesh::SetEdgesAttributes( const TArray<FAttributesForEdge>& AttributesForEdges )
{
	EM_ENTER( TEXT( "SetEdgesAttributes: %s" ), *LogHelpers::ArrayToString( AttributesForEdges ) );

	FSetEdgesAttributesChangeInput RevertInput;

	RevertInput.AttributesForEdges.Reserve( AttributesForEdges.Num() );
	for( const FAttributesForEdge& AttributesForEdge : AttributesForEdges )
	{
		const FEdgeID EdgeID = AttributesForEdge.EdgeID;

		RevertInput.AttributesForEdges.Emplace();
		FAttributesForEdge& RevertEdge = RevertInput.AttributesForEdges.Last();
		RevertEdge.EdgeID = EdgeID;

		// Back up the attributes
		BackupAttributesInList( RevertEdge.EdgeAttributes, AttributesForEdge.EdgeAttributes, GetMeshDescription()->EdgeAttributes(), EdgeID );

		for( const FMeshElementAttributeData& EdgeAttribute : AttributesForEdge.EdgeAttributes.Attributes )
		{
			// Set the new attribute
			SetEdgeAttribute( AttributesForEdge.EdgeID, EdgeAttribute );
		}
	}

	AddUndo( MakeUnique<FSetEdgesAttributesChange>( MoveTemp( RevertInput ) ) );

	EM_EXIT( TEXT( "SetEdgesAttributes returned" ) );
}


void UEditableMesh::ChangePolygonsVertexInstances( const TArray<FChangeVertexInstancesForPolygon>& VertexInstancesForPolygons )
{
	EM_ENTER( TEXT( "ChangePolygonsVertexInstances: %s" ), *LogHelpers::ArrayToString( VertexInstancesForPolygons ) );

	FPolygonArray& Polygons = GetMeshDescription()->Polygons();
	FVertexInstanceArray& VertexInstances = GetMeshDescription()->VertexInstances();

	// Back everything up
	{
		FChangePolygonsVertexInstancesChangeInput RevertInput;
		RevertInput.VertexInstancesForPolygons.Reserve( VertexInstancesForPolygons.Num() );

		// NOTE: We iterate backwards, to restore edges in the opposite order that we changed them
		for( int32 Index = VertexInstancesForPolygons.Num() - 1; Index >= 0; --Index )
		{
			const FChangeVertexInstancesForPolygon& VertexInstancesForPolygon = VertexInstancesForPolygons[ Index ];
			const FPolygonID PolygonID = VertexInstancesForPolygon.PolygonID;
			const FMeshPolygon& Polygon = Polygons[ PolygonID ];

			RevertInput.VertexInstancesForPolygons.Emplace();
			FChangeVertexInstancesForPolygon& RevertVertexInstancesForPolygon = RevertInput.VertexInstancesForPolygons.Last();
			RevertVertexInstancesForPolygon.PolygonID = PolygonID;

			for( const FVertexIndexAndInstanceID& IndexAndInstance : VertexInstancesForPolygon.PerimeterVertexIndicesAndInstanceIDs )
			{
				RevertVertexInstancesForPolygon.PerimeterVertexIndicesAndInstanceIDs.Emplace();
				FVertexIndexAndInstanceID& RevertIndexAndInstance = RevertVertexInstancesForPolygon.PerimeterVertexIndicesAndInstanceIDs.Last();
				RevertIndexAndInstance.ContourIndex = IndexAndInstance.ContourIndex;
				RevertIndexAndInstance.VertexInstanceID = Polygon.PerimeterContour.VertexInstanceIDs[ IndexAndInstance.ContourIndex ];
			}
		}

		AddUndo( MakeUnique<FChangePolygonsVertexInstancesChange>( MoveTemp( RevertInput ) ) );
	}

	static TArray<FPolygonID> PolygonIDs;
	PolygonIDs.Reset( VertexInstancesForPolygons.Num() );

	// Perform action
	{
		for( const FChangeVertexInstancesForPolygon& VertexInstancesForPolygon : VertexInstancesForPolygons )
		{
			const FPolygonID PolygonID = VertexInstancesForPolygon.PolygonID;
			PolygonIDs.Add( PolygonID );
			FMeshPolygon& Polygon = Polygons[ PolygonID ];

			for( const FVertexIndexAndInstanceID& IndexAndInstance : VertexInstancesForPolygon.PerimeterVertexIndicesAndInstanceIDs )
			{
				// Disconnect old vertex instance from polygon, and connect new one
				const FVertexInstanceID OldVertexInstanceID = Polygon.PerimeterContour.VertexInstanceIDs[ IndexAndInstance.ContourIndex ];
				FMeshVertexInstance& OldVertexInstance = VertexInstances[ OldVertexInstanceID ];
				verify( OldVertexInstance.ConnectedPolygons.Remove( PolygonID ) == 1 );

				FMeshVertexInstance& NewVertexInstance = VertexInstances[ IndexAndInstance.VertexInstanceID ];
				check( !NewVertexInstance.ConnectedPolygons.Contains( PolygonID ) );
				NewVertexInstance.ConnectedPolygons.Add( PolygonID );

				Polygon.PerimeterContour.VertexInstanceIDs[ IndexAndInstance.ContourIndex ] = IndexAndInstance.VertexInstanceID;

				// Fix up triangle list
				for( FMeshTriangle& Triangle : Polygon.Triangles )
				{
					for( int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex )
					{
						if( Triangle.GetVertexInstanceID( VertexIndex ) == OldVertexInstanceID )
						{
							Triangle.SetVertexInstanceID( VertexIndex, IndexAndInstance.VertexInstanceID );
						}
					}
				}
			}
		}
	}

	// Let the adapter deal with it
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnChangePolygonVertexInstances( this, PolygonIDs );
	}

	EM_EXIT( TEXT( "ChangePolygonsVertexInstances returned" ) );
}


FVertexInstanceID UEditableMesh::GetVertexInstanceInPolygonForVertex( const FPolygonID PolygonID, const FVertexID VertexID ) const
{
	const FVertexInstanceArray& VertexInstances = GetMeshDescription()->VertexInstances();

	for( const FVertexInstanceID VertexInstanceID : GetMeshDescription()->GetVertex( VertexID ).VertexInstanceIDs )
	{
		const FMeshVertexInstance& VertexInstance = VertexInstances[ VertexInstanceID ];
		if( VertexInstance.ConnectedPolygons.Contains( PolygonID ) )
		{
			return VertexInstanceID;
		}
	}

	return FVertexInstanceID::Invalid;
}


void UEditableMesh::GetConnectedSoftEdges( const FVertexID VertexID, TArray<FEdgeID>& OutConnectedSoftEdges ) const
{
	OutConnectedSoftEdges.Reset();

	TEdgeAttributesConstRef<bool> EdgeHardnesses = GetMeshDescription()->EdgeAttributes().GetAttributesRef<bool>( MeshAttribute::Edge::IsHard );
	for( const FEdgeID ConnectedEdgeID : GetMeshDescription()->GetVertex( VertexID ).ConnectedEdgeIDs )
	{
		if( !EdgeHardnesses[ ConnectedEdgeID ] )
		{
			OutConnectedSoftEdges.Add( ConnectedEdgeID );
		}
	}
}


void UEditableMesh::SetPolygonsVertexAttributes( const TArray<FVertexAttributesForPolygon>& VertexAttributesForPolygons )
{
	EM_ENTER( TEXT( "SetPolygonsVertexAttributes: %s" ), *LogHelpers::ArrayToString( VertexAttributesForPolygons ) );

	FPolygonArray& Polygons = GetMeshDescription()->Polygons();

	for( const FVertexAttributesForPolygon& VertexAttributesForPolygon : VertexAttributesForPolygons )
	{
		const FPolygonID PolygonID = VertexAttributesForPolygon.PolygonID;
		FMeshPolygon& Polygon = Polygons[ PolygonID ];

		SetPolygonContourVertexAttributes( Polygon.PerimeterContour, PolygonID, VertexAttributesForPolygon.PerimeterVertexAttributeLists );
	}

	EM_EXIT( TEXT( "SetPolygonsVertexAttributes returned" ) );
}


void UEditableMesh::SetPolygonContourVertexAttributes( FMeshPolygonContour& Contour, const FPolygonID PolygonID, const TArray<FMeshElementAttributeList>& AttributeLists )
{
	const int32 NumContourVertices = Contour.VertexInstanceIDs.Num();
	check( AttributeLists.Num() == NumContourVertices );

	// Iterate round all polygons in the contour
	for( int32 Index = 0; Index < NumContourVertices; ++Index )
	{
		const FMeshElementAttributeList& AttributeList = AttributeLists[ Index ];

		// If there are no attributes to change, skip this index
		if( AttributeList.Attributes.Num() == 0 )
		{
			continue;
		}

		// Get vertex instance and vertex.
		const FVertexInstanceID VertexInstanceID = Contour.VertexInstanceIDs[ Index ];
		const FVertexID VertexID = GetVertexInstanceVertex( VertexInstanceID );

		const TArray<FPolygonID>& ConnectedPolygons = GetMeshDescription()->GetVertexInstanceConnectedPolygons( VertexInstanceID );
		check( ConnectedPolygons.Contains( PolygonID ) );

		if( ConnectedPolygons.Num() == 1 )
		{
			// This is the only polygon using this vertex instance, so change it in place
			static TArray<FAttributesForVertexInstance> AttributesForVertexInstance;
			AttributesForVertexInstance.Reset();
			AttributesForVertexInstance.SetNum( 1 );
			AttributesForVertexInstance[ 0 ].VertexInstanceID = VertexInstanceID;
			AttributesForVertexInstance[ 0 ].VertexInstanceAttributes = AttributeList;

			SetVertexInstancesAttributes( AttributesForVertexInstance );
		}
		else
		{
			// Split vertex instance: create a new one
			static TArray<FVertexInstanceToCreate> VertexInstancesToCreate;
			VertexInstancesToCreate.Reset();
			VertexInstancesToCreate.SetNum( 1 );
			VertexInstancesToCreate[ 0 ].VertexID = VertexID;
			BackupAllAttributes( VertexInstancesToCreate[ 0 ].VertexInstanceAttributes, GetMeshDescription()->VertexInstanceAttributes(), VertexInstanceID );

			// Override original attributes with new ones by putting them at the end of the list
			VertexInstancesToCreate[ 0 ].VertexInstanceAttributes.Attributes.Append( AttributeList.Attributes );

			static TArray<FVertexInstanceID> NewVertexInstanceIDs;
			CreateVertexInstances( VertexInstancesToCreate, NewVertexInstanceIDs );

			// and set it on the contour
			static TArray<FChangeVertexInstancesForPolygon> VertexInstancesToChange;
			VertexInstancesToChange.Reset();
			VertexInstancesToChange.SetNum( 1 );

			VertexInstancesToChange[ 0 ].PolygonID = PolygonID;

			VertexInstancesToChange[ 0 ].PerimeterVertexIndicesAndInstanceIDs.Emplace();
			VertexInstancesToChange[ 0 ].PerimeterVertexIndicesAndInstanceIDs[ 0 ].ContourIndex = Index;
			VertexInstancesToChange[ 0 ].PerimeterVertexIndicesAndInstanceIDs[ 0 ].VertexInstanceID = NewVertexInstanceIDs[ 0 ];

			ChangePolygonsVertexInstances( VertexInstancesToChange );

			// Weld identical vertex instances in the same smoothing group.
			// We may need to do this after splitting a vertex instance if the result of the split created a vertex instance equal to another one.
			VerticesPendingMerging.Add( VertexID );
		}
	}
}


void UEditableMesh::TryToRemovePolygonEdge( const FEdgeID EdgeID, bool& bOutWasEdgeRemoved, FPolygonID& OutNewPolygonID )
{
	EM_ENTER( TEXT( "TryToRemovePolygonEdge: %s" ), *EdgeID.ToString() );

	bOutWasEdgeRemoved = false;
	OutNewPolygonID = FPolygonID::Invalid;

	// If the edge is not shared by at least two polygons, we can't remove it.  (We would have to delete the polygon that owns
	// this edge, which is not the intent of this feature.).  We also can't cleanly remove edges that are joining more than two 
	// polygons.  We need to create a new polygon the two polygons, and if there were more than two then the remaining polygons 
	// would be left disconnected after our edge is gone
	const int32 ConnectedPolygonCount = GetEdgeConnectedPolygonCount( EdgeID );
	if( ConnectedPolygonCount == 2 )
	{
		// Verify that both vertices on either end of this edge are connected to polygon (non-internal) edges.
		// We currently do not expect to support internal triangles that don't touch the polygonal boundaries at all.
		bool bBothVerticesConnectToPolygonEdges = true;
		for( int32 EdgeVertexNumber = 0; EdgeVertexNumber < 2; ++EdgeVertexNumber )
		{
			const FVertexID VertexID = GetEdgeVertex( EdgeID, EdgeVertexNumber );

			bool bVertexConnectsToPolygonEdge = false;

			const int32 ConnectedEdgeCount = GetVertexConnectedEdgeCount( VertexID );
			for( int32 ConnectedEdgeNumber = 0; ConnectedEdgeNumber < ConnectedEdgeCount; ++ConnectedEdgeNumber )
			{
				const FEdgeID OtherEdgeID = GetVertexConnectedEdge( VertexID, ConnectedEdgeNumber );
				if( OtherEdgeID != EdgeID )
				{
					bVertexConnectsToPolygonEdge = true;
					break;
				}
			}

			if( !bVertexConnectsToPolygonEdge )
			{
				bBothVerticesConnectToPolygonEdges = false;
			}
		}

		if( bBothVerticesConnectToPolygonEdges )
		{
			const FPolygonID PolygonAID = GetEdgeConnectedPolygon( EdgeID, 0 );
			const FPolygonID PolygonBID = GetEdgeConnectedPolygon( EdgeID, 1 );

			const TArray<FVertexInstanceID>& PolygonAVertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonAID );
			const TArray<FVertexInstanceID>& PolygonBVertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonBID );

			// If the polygons are in different polygon groups, we can't remove the edge because we can't determine
			// which polygon group the replacing polygon should belong to
			// @todo mesheditor: We could just allow this, and use the first polygon group ID.  It's just a bit weird in the UI.
			const FPolygonGroupID PolygonGroupID = GetGroupForPolygon( PolygonAID );
			if( PolygonGroupID == GetGroupForPolygon( PolygonBID ) )
			{
				// Create a polygon by combining the edges from either polygon we're connected to, omitting the edge we're removing
				TArray<FVertexAndAttributes> NewPolygonVertices;
				{
					const FVertexID EdgeVertexIDA = GetEdgeVertex( EdgeID, 0 );
					const FVertexID EdgeVertexIDB = GetEdgeVertex( EdgeID, 1 );

					// @todo mesheditor urgent: This code was written with the assumption that the total vertex count would
					// end up being two less than the total vertex count of the two polygons we're replacing.  However, this
					// might not be true for cases where either polygon shares more than one edge with the other, right?

					// @todo mesheditor: Case study: Two tris joined by an edge with an extra vertex inserted, then try to remove
					// one of those diagonal edges.

					// Find the edge vertices in the first polygon
					int32 EdgeStartsAtVertexInPolygonA = INDEX_NONE;
					for( int32 PolygonAVertexNumber = 0; PolygonAVertexNumber < PolygonAVertexInstanceIDs.Num(); ++PolygonAVertexNumber )
					{
						const FVertexInstanceID PolygonAVertexInstanceID = PolygonAVertexInstanceIDs[ PolygonAVertexNumber ];
						const FVertexInstanceID PolygonANextVertexInstanceID = PolygonAVertexInstanceIDs[ ( PolygonAVertexNumber + 1 ) % PolygonAVertexInstanceIDs.Num() ];
						const FVertexID PolygonAVertexID = GetMeshDescription()->GetVertexInstanceVertex( PolygonAVertexInstanceID );
						const FVertexID PolygonANextVertexID = GetMeshDescription()->GetVertexInstanceVertex( PolygonANextVertexInstanceID );

						if( ( PolygonAVertexID == EdgeVertexIDA || PolygonAVertexID == EdgeVertexIDB ) &&
							( PolygonANextVertexID == EdgeVertexIDA || PolygonANextVertexID == EdgeVertexIDB ) )
						{
							EdgeStartsAtVertexInPolygonA = PolygonAVertexNumber;
							break;
						}
					}
					check( EdgeStartsAtVertexInPolygonA != INDEX_NONE );
					const int32 EdgeEndsAtVertexInPolygonA = ( EdgeStartsAtVertexInPolygonA + 1 ) % PolygonAVertexInstanceIDs.Num();


					// Find the edge vertices in the second polygon
					int32 EdgeStartsAtVertexInPolygonB = INDEX_NONE;
					for( int32 PolygonBVertexNumber = 0; PolygonBVertexNumber < PolygonBVertexInstanceIDs.Num(); ++PolygonBVertexNumber )
					{
						const FVertexInstanceID PolygonBVertexInstanceID = PolygonBVertexInstanceIDs[ PolygonBVertexNumber ];
						const FVertexInstanceID PolygonBNextVertexInstanceID = PolygonBVertexInstanceIDs[ ( PolygonBVertexNumber + 1 ) % PolygonBVertexInstanceIDs.Num() ];
						const FVertexID PolygonBVertexID = GetMeshDescription()->GetVertexInstanceVertex( PolygonBVertexInstanceID );
						const FVertexID PolygonBNextVertexID = GetMeshDescription()->GetVertexInstanceVertex( PolygonBNextVertexInstanceID );

						if( ( PolygonBVertexID == EdgeVertexIDA || PolygonBVertexID == EdgeVertexIDB ) &&
							( PolygonBNextVertexID == EdgeVertexIDA || PolygonBNextVertexID == EdgeVertexIDB ) )
						{
							EdgeStartsAtVertexInPolygonB = PolygonBVertexNumber;
							break;
						}
					}
					check( EdgeStartsAtVertexInPolygonB != INDEX_NONE );
					const int32 EdgeEndsAtVertexInPolygonB = ( EdgeStartsAtVertexInPolygonB + 1 ) % PolygonBVertexInstanceIDs.Num();

					
					// Do the polygons wind in the same direction?  If they do, the edge order will be reversed.
					const FVertexID PolygonAStartVertex = GetMeshDescription()->GetVertexInstanceVertex( PolygonAVertexInstanceIDs[ EdgeStartsAtVertexInPolygonA ] );
					const FVertexID PolygonBStartVertex = GetMeshDescription()->GetVertexInstanceVertex( PolygonBVertexInstanceIDs[ EdgeStartsAtVertexInPolygonB ] );
					const bool bPolygonsWindInSameDirection = PolygonAStartVertex != PolygonBStartVertex;


					// Start adding vertices from the first polygon, starting with the vertex right after the edge we're removing.
					// We'll continue to add vertices from this polygon until we reach back around to that edge.
					const int32 PolygonAStartVertexNumber = EdgeEndsAtVertexInPolygonA;
					const int32 PolygonAEndVertexNumber = EdgeStartsAtVertexInPolygonA;
					for( int32 PolygonAVertexNumber = PolygonAStartVertexNumber; PolygonAVertexNumber != PolygonAEndVertexNumber; )
					{
						NewPolygonVertices.Emplace();
						FVertexAndAttributes& NewPolygonVertex = NewPolygonVertices.Last();
						NewPolygonVertex.VertexInstanceID = PolygonAVertexInstanceIDs[ PolygonAVertexNumber ];

						PolygonAVertexNumber = ( PolygonAVertexNumber + 1 ) % PolygonAVertexInstanceIDs.Num();
					}

					// Now add vertices from the second polygon
					const int32 PolygonBStartVertexNumber = bPolygonsWindInSameDirection ? EdgeEndsAtVertexInPolygonB : EdgeStartsAtVertexInPolygonB;
					const int32 PolygonBEndVertexNumber = bPolygonsWindInSameDirection ? EdgeStartsAtVertexInPolygonB : EdgeEndsAtVertexInPolygonB;
					const int32 PolygonBNumberIncrement = bPolygonsWindInSameDirection ? 1 : PolygonBVertexInstanceIDs.Num() - 1;
					for( int32 PolygonBVertexNumber = PolygonBStartVertexNumber; PolygonBVertexNumber != PolygonBEndVertexNumber; )
					{
						NewPolygonVertices.Emplace();
						FVertexAndAttributes& NewPolygonVertex = NewPolygonVertices.Last();
						NewPolygonVertex.VertexInstanceID = PolygonBVertexInstanceIDs[ PolygonBVertexNumber ];

						PolygonBVertexNumber = ( PolygonBVertexNumber + PolygonBNumberIncrement ) % PolygonBVertexInstanceIDs.Num();
					}
				}

				// OK, we can go ahead and delete the edge and its connected polygons.  We do NOT want to delete any orphaned
				// edges or vertices though.  We're going to create a new polygon that connects to those right afterwards.
				const bool bDeleteOrphanedEdges = false;
				const bool bDeleteOrphanedVertices = false;
				const bool bDeleteOrphanedVertexInstances = false;
				const bool bDeleteEmptyPolygonGroups = false;
				DeleteEdgeAndConnectedPolygons( EdgeID, bDeleteOrphanedEdges, bDeleteOrphanedVertices, bDeleteOrphanedVertexInstances, bDeleteEmptyPolygonGroups );

				// Now create a new polygon to replace the two polygons we deleted
				{
					// Create the polygon
					static TArray<FPolygonToCreate> PolygonsToCreate;
					PolygonsToCreate.Reset();

					// @todo mesheditor perf: Ideally we support creating multiple polygons at once and batching up the work
					FPolygonToCreate& PolygonToCreate = *new( PolygonsToCreate ) FPolygonToCreate();
					PolygonToCreate.PolygonGroupID = PolygonGroupID;
					PolygonToCreate.PerimeterVertices = NewPolygonVertices;	// @todo mesheditor perf: Extra allocatons/copies: Ideally MoveTemp() here but we can't move a STATIC local!

					static TArray<FPolygonID> NewPolygonIDs;
					NewPolygonIDs.Reset();
					static TArray<FEdgeID> NewEdgeIDs;
					NewEdgeIDs.Reset();
					CreatePolygons( PolygonsToCreate, /* Out */ NewPolygonIDs, /* Out */ NewEdgeIDs );	// @todo mesheditor perf: Extra allocatons/copies: Ideally MoveTemp() here but we can't move a STATIC local!

					OutNewPolygonID = NewPolygonIDs[ 0 ];
				}

				bOutWasEdgeRemoved = true;
			}
		}
	}

	EM_EXIT( TEXT( "TryToRemovePolygonEdge returned %s %s" ), *LogHelpers::BoolToString( bOutWasEdgeRemoved ), *OutNewPolygonID.ToString() );
}


void UEditableMesh::TryToRemoveVertex( const FVertexID VertexID, bool& bOutWasVertexRemoved, FEdgeID& OutNewEdgeID )
{
	EM_ENTER( TEXT( "TryToRemoveVertex: %s" ), *VertexID.ToString() );

	bOutWasVertexRemoved = false;
	OutNewEdgeID = FEdgeID::Invalid;

	// We only support removing vertices that are shared by just two edges
	const int32 ConnectedEdgeCount = GetVertexConnectedEdgeCount( VertexID );
	if( ConnectedEdgeCount == 2 )
	{
		// Get the two vertices on the other edge of either edge
		FVertexID NewEdgeVertexIDs[ 2 ];
		for( int32 EdgeNumber = 0; EdgeNumber < ConnectedEdgeCount; ++EdgeNumber )
		{
			const FEdgeID OtherEdgeID = GetVertexConnectedEdge( VertexID, EdgeNumber );
			FVertexID OtherEdgeVertexIDs[ 2 ];
			GetEdgeVertices( OtherEdgeID, /* Out */ OtherEdgeVertexIDs[0], /* Out */ OtherEdgeVertexIDs[1] );

			NewEdgeVertexIDs[ EdgeNumber ] = OtherEdgeVertexIDs[ 0 ] == VertexID ? OtherEdgeVertexIDs[ 1 ] : OtherEdgeVertexIDs[ 0 ];
		}

		// Try to preserve attributes of the edges we're deleting.  We'll take the attributes from the first
		// edge and apply them to the newly created edge
		static FMeshElementAttributeList EdgeAttributeList;
		{
			const FEdgeID OtherEdgeID = GetVertexConnectedEdge( VertexID, 0 );		// @todo mesheditor: This is sort of subjective how we handle this (taking attributes from the first edge)

			EdgeAttributeList.Attributes.Reset();
			BackupAllAttributes( EdgeAttributeList, GetMeshDescription()->EdgeAttributes(), OtherEdgeID );
		}

		// The new edge will be connected to the same polygons as both of the edges we're replacing.  Because
		// we only support deleting a vertex shared by two edges, the two edges are guaranteed to be connected
		// to the same exact polygons
		static TArray<FPolygonID> NewEdgeConnectedPolygons;
		GetVertexConnectedPolygons( VertexID, /* Out */ NewEdgeConnectedPolygons );

		// Remove the vertex from its connected polygons
		{
			for( const FPolygonID PolygonID : NewEdgeConnectedPolygons )
			{
				const int32 PolygonVertexNumber = FindPolygonPerimeterVertexNumberForVertex( PolygonID, VertexID );
				check( PolygonVertexNumber != INDEX_NONE );
				const bool bDeleteOrphanedVertexInstances = false;
				RemovePolygonPerimeterVertices( PolygonID, PolygonVertexNumber, 1, bDeleteOrphanedVertexInstances );
			}
		}

		// Delete the two edges
		{
			static TArray<FEdgeID> EdgeIDsToDelete;
			EdgeIDsToDelete.Reset();
			for( int32 EdgeNumber = 0; EdgeNumber < ConnectedEdgeCount; ++EdgeNumber )
			{
				EdgeIDsToDelete.Add( GetVertexConnectedEdge( VertexID, EdgeNumber ) );
			}

			// NOTE: We can't delete the orphan vertex yet because the polygon triangles are still referencing
			// its rendering vertices.  We'll delete the edges, retriangulate, then delete the vertex afterwards.
			const bool bDeleteOrphanedVertices = false;
			DeleteEdges( EdgeIDsToDelete, bDeleteOrphanedVertices );
		}

		FEdgeID NewEdgeID = FEdgeID::Invalid;

		// Create a new edge to replace the vertex and two edges we deleted
		{
			static TArray<FEdgeToCreate> EdgesToCreate;
			EdgesToCreate.Reset();

			FEdgeToCreate& EdgeToCreate = *new( EdgesToCreate ) FEdgeToCreate;
			EdgeToCreate.VertexID0 = NewEdgeVertexIDs[ 0 ];
			EdgeToCreate.VertexID1 = NewEdgeVertexIDs[ 1 ];
			EdgeToCreate.ConnectedPolygons = NewEdgeConnectedPolygons;
			EdgeToCreate.EdgeAttributes = EdgeAttributeList;

			static TArray<FEdgeID> NewEdgeIDs;
			NewEdgeIDs.Reset();
			CreateEdges( EdgesToCreate, /* Out */ NewEdgeIDs );

			NewEdgeID = NewEdgeIDs[ 0 ];
		}

		// Update the normals of the affected polygons
		PolygonsPendingNewTangentBasis.Append( NewEdgeConnectedPolygons );

		// Retriangulate all of the affected polygons
		PolygonsPendingTriangulation.Append( NewEdgeConnectedPolygons );

		// Delete the vertex instances and subsequently orphaned vertex
		{
			FMeshVertex& Vertex = GetMeshDescription()->GetVertex( VertexID );
			const bool bDeleteOrphanedVertices = true;

			// Take a copy of the array, because it will be modified by the DeleteVertexInstances call
			static TArray<FVertexInstanceID> VertexInstanceIDs;
			VertexInstanceIDs = Vertex.VertexInstanceIDs;

			DeleteVertexInstances( VertexInstanceIDs, bDeleteOrphanedVertices );
		}

		bOutWasVertexRemoved = true;
		OutNewEdgeID = NewEdgeID;
	}

	EM_EXIT( TEXT( "TryToRemoveVertex returned %s %s" ), *LogHelpers::BoolToString( bOutWasVertexRemoved ), *OutNewEdgeID.ToString() );
}


void UEditableMesh::ExtrudePolygons( const TArray<FPolygonID>& PolygonIDs, const float ExtrudeDistance, const bool bKeepNeighborsTogether, TArray<FPolygonID>& OutNewExtrudedFrontPolygons )
{
	EM_ENTER( TEXT( "ExtrudePolygons: %s" ), *LogHelpers::ArrayToString( PolygonIDs ) );

	// @todo mesheditor perf: We can make this much faster by batching up polygons together.  Just be careful about how neighbors are handled.
	OutNewExtrudedFrontPolygons.Reset();

	// Convert our incoming polygon array to a TSet so we can lookup quickly to see which polygons in the mesh sare members of the set
	static TSet<FPolygonID> PolygonsSet;
	PolygonsSet.Reset();
	PolygonsSet.Append( PolygonIDs );
	
	static TArray<FPolygonID> AllNewPolygons;
	AllNewPolygons.Reset();

	static TArray<FAttributesForEdge> AttributesForEdges;
	AttributesForEdges.Reset();

	static TArray<FAttributesForVertex> AttributesForVertices;
	AttributesForVertices.Reset();

	// Get vertex position attribute array
	const TVertexAttributesRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	// First, let's figure out which of the polygons we were asked to extrude share edges or vertices.  We'll keep those
	// edges intact!
	static TMap<FEdgeID, uint32> EdgeUsageCounts;	// Maps an edge ID to the number of times it is referenced by the incoming polygons
	EdgeUsageCounts.Reset();
	static TSet<FVertexID> UniqueVertexIDs;
	UniqueVertexIDs.Reset();

	for( int32 PolygonIter = 0; PolygonIter < PolygonIDs.Num(); ++PolygonIter )
	{
		const FPolygonID PolygonID = PolygonIDs[ PolygonIter ];

		static TArray<FEdgeID> PolygonPerimeterEdgeIDs;
		GetPolygonPerimeterEdges( PolygonID, /* Out */ PolygonPerimeterEdgeIDs );

		for( const FEdgeID EdgeID : PolygonPerimeterEdgeIDs )
		{
			FVertexID EdgeVertexIDs[ 2 ];
			GetEdgeVertices( EdgeID, /* Out */ EdgeVertexIDs[ 0 ], /* Out */ EdgeVertexIDs[ 1 ] );

			uint32* EdgeUsageCountPtr = EdgeUsageCounts.Find( EdgeID );
			if( EdgeUsageCountPtr == nullptr )
			{
				EdgeUsageCounts.Add( EdgeID, 1 );
			}
			else
			{
				++( *EdgeUsageCountPtr );
			}
		}


		static TArray<FVertexID> PolygonPerimeterVertexIDs;
		this->GetPolygonPerimeterVertices( PolygonID, /* Out */ PolygonPerimeterVertexIDs );

		for( const FVertexID VertexID : PolygonPerimeterVertexIDs )
		{
			UniqueVertexIDs.Add( VertexID );
		}
	}

	const int32 NumVerticesToCreate = UniqueVertexIDs.Num();

	// Create new vertices for all of the extruded polygons
	static TArray<FVertexID> ExtrudedVertexIDs;
	ExtrudedVertexIDs.Reset();
	ExtrudedVertexIDs.Reserve( NumVerticesToCreate );
	CreateEmptyVertexRange( NumVerticesToCreate, /* Out */ ExtrudedVertexIDs );
	int32 NextAvailableExtrudedVertexIDNumber = 0;


	static TMap<FVertexID, FVertexID> VertexIDToExtrudedCopy;
	VertexIDToExtrudedCopy.Reset();

	for( int32 PassIndex = 0; PassIndex < 2; ++PassIndex )
	{
		// Extrude all of the shared edges first, then do the non-shared edges.  This is to make sure that a vertex doesn't get offset
		// without taking into account all of the connected polygons in our set
		const bool bIsExtrudingSharedEdges = ( PassIndex == 0 );

		for( int32 PolygonIter = 0; PolygonIter < PolygonIDs.Num(); ++PolygonIter )
		{
			const FPolygonID PolygonID = PolygonIDs[ PolygonIter ];
			const FPolygonGroupID PolygonGroupID = GetGroupForPolygon( PolygonID );

			if( !bKeepNeighborsTogether )
			{
				VertexIDToExtrudedCopy.Reset();
			}

			// Map all of the edge vertices to their new extruded counterpart
			const int32 PerimeterEdgeCount = GetPolygonPerimeterEdgeCount( PolygonID );
			for( int32 PerimeterEdgeNumber = 0; PerimeterEdgeNumber < PerimeterEdgeCount; ++PerimeterEdgeNumber )
			{
				// @todo mesheditor perf: We can change GetPolygonPerimeterEdges() to have a version that returns whether winding is reversed or not, and avoid this call entirely.
				// @todo mesheditor perf: O(log N^2) iteration here. For every edge, for every edge up to this index.  Need to clean this up. 
				//		--> Also, there are quite a few places where we are stepping through edges in perimeter-order.  We need to have a nice way to walk that.
				bool bEdgeWindingIsReversedForPolygon;
				const FEdgeID EdgeID = this->GetPolygonPerimeterEdge( PolygonID, PerimeterEdgeNumber, /* Out */ bEdgeWindingIsReversedForPolygon );

				const bool bIsSharedEdge = bKeepNeighborsTogether && EdgeUsageCounts[ EdgeID ] > 1;
				if( bIsSharedEdge == bIsExtrudingSharedEdges )
				{
					FVertexID EdgeVertexIDs[ 2 ];
					GetEdgeVertices( EdgeID, /* Out */ EdgeVertexIDs[ 0 ], /* Out */ EdgeVertexIDs[ 1 ] );
					if( bEdgeWindingIsReversedForPolygon )
					{
						::Swap( EdgeVertexIDs[ 0 ], EdgeVertexIDs[ 1 ] );
					}

					if( !bIsSharedEdge )
					{
						// After extruding, all of the edges of the original polygon become hard edges
						AttributesForEdges.Emplace();
						FAttributesForEdge& AttributesForEdge = AttributesForEdges.Last();
						AttributesForEdge.EdgeID = EdgeID;
						AttributesForEdge.EdgeAttributes.Attributes.Emplace( MeshAttribute::Edge::IsHard, 0, FMeshElementAttributeValue( true ) );
					}

					FVertexID ExtrudedEdgeVertexIDs[ 2 ];
					for( int32 EdgeVertexNumber = 0; EdgeVertexNumber < 2; ++EdgeVertexNumber )
					{
						const FVertexID EdgeVertexID = EdgeVertexIDs[ EdgeVertexNumber ];
						FVertexID* ExtrudedEdgeVertexIDPtr = VertexIDToExtrudedCopy.Find( EdgeVertexID );

						// @todo mesheditor extrude: Ideally we would detect whether the vertex that was already extruded came from a edge
						// from a polygon that does not actually share an edge with any polygons this polygon shares an edge with.  This
						// would avoid the problem where extruding two polygons that are connected only by a vertex are not extruded
						// separately.
						const bool bVertexIsSharedByAnEdgeOfAnotherSelectedPolygon = false;
						if( ExtrudedEdgeVertexIDPtr != nullptr)// && !bVertexIsSharedByAnEdgeOfAnotherSelectedPolygon )
						{
							ExtrudedEdgeVertexIDs[ EdgeVertexNumber ] = *ExtrudedEdgeVertexIDPtr;
						}
						else
						{
							// Create a copy of this vertex for the extruded face
							const FVertexID ExtrudedVertexID = ExtrudedVertexIDs[ NextAvailableExtrudedVertexIDNumber++ ];

							ExtrudedEdgeVertexIDPtr = &VertexIDToExtrudedCopy.Add( EdgeVertexID, ExtrudedVertexID );
							// Push the vertex out along the polygon's normal
							const FVector OriginalVertexPosition = VertexPositions[ EdgeVertexID ];

							FVector ExtrudedVertexPosition;
							if( bIsSharedEdge )
							{
								// Get all of the polygons that share this edge that were part of the set of polygons passed in.  We'll
								// generate an extrude direction that's the average of those polygon normals.
								FVector ExtrudeDirection = FVector::ZeroVector;

								static TArray<FPolygonID> ConnectedPolygonIDs;
								GetVertexConnectedPolygons( EdgeVertexID, /* Out */ ConnectedPolygonIDs );

								static TArray<FPolygonID> NeighborPolygonIDs;
								NeighborPolygonIDs.Reset();
								for( const FPolygonID ConnectedPolygonID : ConnectedPolygonIDs )
								{
									// We only care about polygons that are members of the set of polygons we were asked to extrude
									if( PolygonsSet.Contains( ConnectedPolygonID ) )
									{
										NeighborPolygonIDs.Add( ConnectedPolygonID );

										// We'll need this polygon's normal to figure out where to put the extruded copy of the polygon
										const FVector NeighborPolygonNormal = ComputePolygonNormal( ConnectedPolygonID );
										ExtrudeDirection += NeighborPolygonNormal;
									}
								}
								ExtrudeDirection.Normalize();


								// OK, we have the direction to extrude for this vertex.  Now we need to know how far to extrude.  We'll
								// loop over all of the neighbor polygons to this vertex, and choose the closest intersection point with our
								// vertex's extrude direction and the neighbor polygon's extruded plane
								FVector ClosestIntersectionPointWithExtrudedPlanes;
								float ClosestIntersectionDistanceSquared = TNumericLimits<float>::Max();

								for( const FPolygonID NeighborPolygonID : NeighborPolygonIDs )
								{
									const FPlane NeighborPolygonPlane = ComputePolygonPlane( NeighborPolygonID );

									// Push the plane out
									const FPlane ExtrudedPlane = [NeighborPolygonPlane, ExtrudeDistance]
										{ 
											FPlane NewPlane = NeighborPolygonPlane;
											NewPlane.W += ExtrudeDistance; 
											return NewPlane; 
										}();

									// Is this the closest intersection point so far?
									const FVector IntersectionPointWithExtrudedPlane = FMath::RayPlaneIntersection( OriginalVertexPosition, ExtrudeDirection, ExtrudedPlane );
									const float IntersectionDistanceSquared = FVector::DistSquared( OriginalVertexPosition, IntersectionPointWithExtrudedPlane );
									if( IntersectionDistanceSquared < ClosestIntersectionDistanceSquared )
									{
										ClosestIntersectionPointWithExtrudedPlanes = IntersectionPointWithExtrudedPlane;
										ClosestIntersectionDistanceSquared = IntersectionDistanceSquared;
									}
								}

								ExtrudedVertexPosition = ClosestIntersectionPointWithExtrudedPlanes;
							}
							else
							{
								// We'll need this polygon's normal to figure out where to put the extruded copy of the polygon
								const FVector PolygonNormal = ComputePolygonNormal( PolygonID );
								ExtrudedVertexPosition = OriginalVertexPosition + ExtrudeDistance * PolygonNormal;
							}

							// Fill in the vertex
							AttributesForVertices.Emplace();
							FAttributesForVertex& AttributesForVertex = AttributesForVertices.Last();
							AttributesForVertex.VertexID = ExtrudedVertexID;
							AttributesForVertex.VertexAttributes.Attributes.Emplace( MeshAttribute::Vertex::Position, 0, FMeshElementAttributeValue( ExtrudedVertexPosition ) );
						}
						ExtrudedEdgeVertexIDs[ EdgeVertexNumber ] = *ExtrudedEdgeVertexIDPtr;
					}

					if( !bIsSharedEdge )
					{
						static TArray<FVertexAndAttributes> NewSidePolygonVertices;
						NewSidePolygonVertices.Reset( 4 );
						NewSidePolygonVertices.SetNum( 4, false );	// Always four edges in an extruded face

						NewSidePolygonVertices[ 0 ].VertexID = EdgeVertexIDs[ 1 ];
						NewSidePolygonVertices[ 1 ].VertexID = EdgeVertexIDs[ 0 ];
						NewSidePolygonVertices[ 2 ].VertexID = ExtrudedEdgeVertexIDs[ 0 ];
						NewSidePolygonVertices[ 3 ].VertexID = ExtrudedEdgeVertexIDs[ 1 ];

						// Get vertex instance IDs on this polygon corresponding to the edge start/end vertices
						const FVertexInstanceID EdgeVertexInstanceID0 = GetMeshDescription()->GetVertexInstanceForPolygonVertex( PolygonID, EdgeVertexIDs[ 0 ] );
						const FVertexInstanceID EdgeVertexInstanceID1 = GetMeshDescription()->GetVertexInstanceForPolygonVertex( PolygonID, EdgeVertexIDs[ 1 ] );

						BackupAllAttributes( NewSidePolygonVertices[ 0 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), EdgeVertexInstanceID1 );
						BackupAllAttributes( NewSidePolygonVertices[ 1 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), EdgeVertexInstanceID0 );
						BackupAllAttributes( NewSidePolygonVertices[ 2 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), EdgeVertexInstanceID0 );
						BackupAllAttributes( NewSidePolygonVertices[ 3 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), EdgeVertexInstanceID1 );

						FPolygonID NewSidePolygonID;	// Filled in below
						{
							static TArray<FPolygonToCreate> PolygonsToCreate;
							PolygonsToCreate.Reset();

							// Create the polygon
							// @todo mesheditor perf: Ideally we support creating multiple polygons at once and batching up the work
							FPolygonToCreate& PolygonToCreate = *new( PolygonsToCreate ) FPolygonToCreate();
							PolygonToCreate.PolygonGroupID = PolygonGroupID;
							PolygonToCreate.PerimeterVertices = NewSidePolygonVertices;	// @todo mesheditor perf: Copying static array here, ideally allocations could be avoided
							PolygonToCreate.PolygonEdgeHardness = EPolygonEdgeHardness::AllEdgesHard;

							static TArray<FPolygonID> NewPolygonIDs;
							NewPolygonIDs.Reset();
							static TArray<FEdgeID> NewEdgeIDs;
							NewEdgeIDs.Reset();
							CreatePolygons( PolygonsToCreate, /* Out */ NewPolygonIDs, /* Out */ NewEdgeIDs );	// @todo mesheditor perf: Extra allocatons/copies: Ideally MoveTemp() here but we can't move a STATIC local!

							NewSidePolygonID = NewPolygonIDs[ 0 ];
						}
						AllNewPolygons.Add( NewSidePolygonID );
					}
				}
			}
		}
	}

	for( int32 PolygonIter = 0; PolygonIter < PolygonIDs.Num(); ++PolygonIter )
	{
		const FPolygonID PolygonID = PolygonIDs[ PolygonIter ];
		const FPolygonGroupID PolygonGroupID = GetGroupForPolygon( PolygonID );

		static TArray<FVertexID> PolygonVertexIDs;
		this->GetPolygonPerimeterVertices( PolygonID, /* Out */ PolygonVertexIDs );

		// Create a new extruded polygon for the face
		FPolygonID ExtrudedFrontPolygonID;	// Filled in below
		{
			static TArray<FVertexAndAttributes> NewFrontPolygonVertices;
			NewFrontPolygonVertices.Reset( PolygonVertexIDs.Num() );
			NewFrontPolygonVertices.SetNum( PolygonVertexIDs.Num(), false );

			const TArray<FVertexInstanceID>& VertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID );

			// Map all of the polygon's vertex IDs to their extruded counterparts to create the new polygon perimeter
			for( int32 PolygonVertexNumber = 0; PolygonVertexNumber < PolygonVertexIDs.Num(); ++PolygonVertexNumber )
			{
				const FVertexID VertexID = PolygonVertexIDs[ PolygonVertexNumber ];
				const FVertexID* ExtrudedCopyVertexIDPtr = VertexIDToExtrudedCopy.Find( VertexID );
				if( ExtrudedCopyVertexIDPtr != nullptr )
				{
					NewFrontPolygonVertices[ PolygonVertexNumber ].VertexID = VertexIDToExtrudedCopy[ VertexID ];
				}
				else
				{
					// We didn't need to extrude a new copy of this vertex (because it was part of a shared edge), so just connect the polygon to the original vertex
					NewFrontPolygonVertices[ PolygonVertexNumber ].VertexID = VertexID;
				}

				// Copy vertex instance attributes from original polygon vertex to extruded polygon vertex
				BackupAllAttributes( NewFrontPolygonVertices[ PolygonVertexNumber ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), VertexInstanceIDs[ PolygonVertexNumber ] );
			}

			{
				static TArray<FPolygonToCreate> PolygonsToCreate;
				PolygonsToCreate.Reset();

				// Create the polygon
				// @todo mesheditor perf: Ideally we support creating multiple polygons at once and batching up the work
				FPolygonToCreate& PolygonToCreate = *new( PolygonsToCreate ) FPolygonToCreate();
				PolygonToCreate.PolygonGroupID = PolygonGroupID;
				PolygonToCreate.PolygonEdgeHardness = EPolygonEdgeHardness::AllEdgesHard;
				PolygonToCreate.PerimeterVertices = NewFrontPolygonVertices;	// @todo mesheditor perf: Copying static array here, ideally allocations could be avoided
				static TArray<FPolygonID> NewPolygonIDs;
				NewPolygonIDs.Reset();
				static TArray<FEdgeID> NewEdgeIDs;
				NewEdgeIDs.Reset();
				CreatePolygons( PolygonsToCreate, /* Out */ NewPolygonIDs, /* Out */ NewEdgeIDs );	// @todo mesheditor perf: Extra allocatons/copies: Ideally MoveTemp() here but we can't move a STATIC local!

				ExtrudedFrontPolygonID = NewPolygonIDs[ 0 ];
			}
			AllNewPolygons.Add( ExtrudedFrontPolygonID );

			// All of the border edges of the new polygon will be hard.  If it was a shared edge, then we'll just preserve whatever was
			// originally going on with the internal edge.
			{
				const TEdgeAttributesRef<bool> EdgeHardnesses = GetMeshDescription()->EdgeAttributes().GetAttributesRef<bool>( MeshAttribute::Edge::IsHard );

				const int32 NewPerimeterEdgeCount = this->GetPolygonPerimeterEdgeCount( ExtrudedFrontPolygonID );
				check( NewPerimeterEdgeCount == GetPolygonPerimeterEdgeCount( PolygonID ) );	// New polygon should always have the same number of edges (in the same order) as the original!
				for( int32 PerimeterEdgeNumber = 0; PerimeterEdgeNumber < NewPerimeterEdgeCount; ++PerimeterEdgeNumber )
				{
					bool bOriginalEdgeWindingIsReversedForPolygon;
					const FEdgeID OriginalEdgeID = this->GetPolygonPerimeterEdge( PolygonID, PerimeterEdgeNumber, /* Out */ bOriginalEdgeWindingIsReversedForPolygon );
					const bool bIsSharedEdge = bKeepNeighborsTogether && EdgeUsageCounts[ OriginalEdgeID ] > 1;

					bool bEdgeWindingIsReversedForPolygon;
					const FEdgeID EdgeID = this->GetPolygonPerimeterEdge( ExtrudedFrontPolygonID, PerimeterEdgeNumber, /* Out */ bEdgeWindingIsReversedForPolygon );

					const bool bNewEdgeHardnessAttribute = bIsSharedEdge ? EdgeHardnesses[ OriginalEdgeID ] : true;

					FAttributesForEdge& AttributesForEdge = *new( AttributesForEdges ) FAttributesForEdge();
					AttributesForEdge.EdgeID = EdgeID;
					AttributesForEdge.EdgeAttributes.Attributes.Emplace( MeshAttribute::Edge::IsHard, 0, FMeshElementAttributeValue( bNewEdgeHardnessAttribute ) );
				}
			}
		}

		OutNewExtrudedFrontPolygons.Add( ExtrudedFrontPolygonID );
	}
	check( NextAvailableExtrudedVertexIDNumber == ExtrudedVertexIDs.Num() );	// Make sure all of the vertices we created were actually used by new polygons

	// Update edge attributes in bulk
	SetEdgesAttributes( AttributesForEdges );

	// Update vertex attributes in bulk
	SetVerticesAttributes( AttributesForVertices );

	// Delete the original polygons
	{
		const bool bDeleteOrphanedEdges = true;
		const bool bDeleteOrphanedVertices = true;
		const bool bDeleteOrphanedVertexInstances = true;
		const bool bDeleteEmptyPolygonGroups = false;
		DeletePolygons( PolygonIDs, bDeleteOrphanedEdges, bDeleteOrphanedVertices, bDeleteOrphanedVertexInstances, bDeleteEmptyPolygonGroups );
	}

	EM_EXIT( TEXT( "ExtrudePolygons returned %s" ), *LogHelpers::ArrayToString( OutNewExtrudedFrontPolygons ) );
}


void UEditableMesh::ExtendEdges( const TArray<FEdgeID>& EdgeIDs, const bool bWeldNeighbors, TArray<FEdgeID>& OutNewExtendedEdgeIDs )
{
	EM_ENTER( TEXT( "ExtendEdges: %s" ), *LogHelpers::ArrayToString( EdgeIDs ) );

	OutNewExtendedEdgeIDs.Reset();

	static TArray<FVertexID> NewVertexIDs;
	NewVertexIDs.Reset();

	// For each original edge vertex ID that we'll be creating a counterpart for on the extended edge, a mapping
	// to the vertex number of our NewVertexIDs (and VerticesToCreate) list.
	static TMap<FVertexID, int32> OriginalVertexIDToCreatedVertexNumber;
	OriginalVertexIDToCreatedVertexNumber.Reset();

	// Create new vertices for all of the new edges.  If bWeldNeighbors is true, we'll share vertices between edges that share the
	// same vertex instead of creating new edges.
	{
		static TArray<FVertexToCreate> VerticesToCreate;
		VerticesToCreate.Reset();
		VerticesToCreate.Reserve( EdgeIDs.Num() * 2 );	// Might actually end up needing less, but that's OK.

		for( const FEdgeID EdgeID : EdgeIDs )
		{
			FVertexID EdgeVertexIDs[ 2 ];
			GetEdgeVertices( EdgeID, /* Out */ EdgeVertexIDs[0], /* Out */ EdgeVertexIDs[1] );

			for( const FVertexID EdgeVertexID : EdgeVertexIDs )
			{
				// Have we already created a counterpart for this vertex?  If we were asked to weld extended neighbor edges,
				// we'll want to make sure that we share the extended vertex too!
				const int32* FoundCreatedVertexNumberPtr = OriginalVertexIDToCreatedVertexNumber.Find( EdgeVertexID );
				if( !( bWeldNeighbors && FoundCreatedVertexNumberPtr != nullptr ) )
				{
					const int32 CreatedVertexNumber = VerticesToCreate.Num();
					FVertexToCreate& VertexToCreate = *new( VerticesToCreate ) FVertexToCreate();

					// Copy attributes from the original vertex
					BackupAllAttributes( VertexToCreate.VertexAttributes, GetMeshDescription()->VertexAttributes(), EdgeVertexID );

					// Keep track of which vertex we're creating a counterpart for
					// @todo mesheditor perf: In the case where bWeldNeighbors=false, we can skip updating this map and instead calculate
					// the index of the new vertex (it will be 2*EdgeNumber+EdgeVertexNumber, because all vertices will be unique.)
					OriginalVertexIDToCreatedVertexNumber.Add( EdgeVertexID, CreatedVertexNumber );
				}
			}
		}

		CreateVertices( VerticesToCreate, /* Out */ NewVertexIDs );
	}


	// Create the extended edges
	{
		static TArray<FEdgeToCreate> EdgesToCreate;
		EdgesToCreate.Reset();
		EdgesToCreate.Reserve( EdgeIDs.Num() );

		for( const FEdgeID EdgeID : EdgeIDs )
		{
			FVertexID EdgeVertexIDs[ 2 ];
			GetEdgeVertices( EdgeID, /* Out */ EdgeVertexIDs[0], /* Out */ EdgeVertexIDs[1] );

			FEdgeToCreate& EdgeToCreate = *new( EdgesToCreate ) FEdgeToCreate();

			EdgeToCreate.VertexID0 = NewVertexIDs[ OriginalVertexIDToCreatedVertexNumber.FindChecked( EdgeVertexIDs[ 0 ] ) ];
			EdgeToCreate.VertexID1 = NewVertexIDs[ OriginalVertexIDToCreatedVertexNumber.FindChecked( EdgeVertexIDs[ 1 ] ) ];

			// Copy attributes from our original edge
			BackupAllAttributes( EdgeToCreate.EdgeAttributes, GetMeshDescription()->EdgeAttributes(), EdgeID );

			// We're not connected to any polygons yet.  That will come later.
			EdgeToCreate.ConnectedPolygons.Reset();
		}

		CreateEdges( EdgesToCreate, /* Out */ OutNewExtendedEdgeIDs );
	}


	// For every edge, make a quad to connect the original edge with it's extended counterpart.
	{
		static TArray<FPolygonToCreate> PolygonsToCreate;
		PolygonsToCreate.Reset();
		PolygonsToCreate.Reserve( EdgeIDs.Num() );

		for( int32 ExtendedEdgeNumber = 0; ExtendedEdgeNumber < OutNewExtendedEdgeIDs.Num(); ++ExtendedEdgeNumber )
		{
			const FEdgeID OriginalEdgeID = EdgeIDs[ ExtendedEdgeNumber ];
			const FEdgeID ExtendedEdgeID = OutNewExtendedEdgeIDs[ ExtendedEdgeNumber ];

			FVertexID OriginalEdgeVertexIDs[ 2 ];
			GetEdgeVertices( OriginalEdgeID, /* Out */ OriginalEdgeVertexIDs[0], /* Out */ OriginalEdgeVertexIDs[1] );

			FVertexID ExtendedEdgeVertexIDs[ 2 ];
			GetEdgeVertices( ExtendedEdgeID, /* Out */ ExtendedEdgeVertexIDs[0], /* Out */ ExtendedEdgeVertexIDs[1] );

			FPolygonToCreate& PolygonToCreate = *new( PolygonsToCreate ) FPolygonToCreate();

			// We need to figure out which mesh PolygonGroup to put the new polygons in.  To do this, we'll look at
			// which polygons are already connected to the current edge, and use PolygonGroup number from the first
			// polygon we can find.  If no polygons are connected, then we'll just use first PolygonGroup in the mesh.
			// We'll also capture texture coordinates from this polygon, so we can apply them to the new polygon vertices.
			FPolygonID ConnectedPolygonID = FPolygonID::Invalid;
			{
				const int32 ConnectedPolygonCount = GetEdgeConnectedPolygonCount( OriginalEdgeID );
				if( ConnectedPolygonCount > 0 )
				{
					ConnectedPolygonID = GetEdgeConnectedPolygon( OriginalEdgeID, 0 );
				}
			}

			PolygonToCreate.PolygonGroupID = ( ConnectedPolygonID != FPolygonID::Invalid ) ? GetGroupForPolygon( ConnectedPolygonID ) : GetFirstValidPolygonGroup();
			check( PolygonToCreate.PolygonGroupID != FPolygonGroupID::Invalid );

			PolygonToCreate.PerimeterVertices.SetNum( 4, false );

			// @todo mesheditor urgent subdiv: This causes degenerate UV triangles, which OpenSubdiv sort of freaks out about (causes flickering geometry)
			PolygonToCreate.PerimeterVertices[ 0 ].VertexID = OriginalEdgeVertexIDs[ 1 ];
			PolygonToCreate.PerimeterVertices[ 1 ].VertexID = OriginalEdgeVertexIDs[ 0 ];
			PolygonToCreate.PerimeterVertices[ 2 ].VertexID = ExtendedEdgeVertexIDs[ 0 ];
			PolygonToCreate.PerimeterVertices[ 3 ].VertexID = ExtendedEdgeVertexIDs[ 1 ];

			// Preserve polygon vertex attributes
			if( ConnectedPolygonID != FPolygonID::Invalid )
			{
				// Get vertex instance IDs on this polygon corresponding to the edge start/end vertices
				const FVertexInstanceID EdgeVertexInstanceID0 = GetMeshDescription()->GetVertexInstanceForPolygonVertex( ConnectedPolygonID, OriginalEdgeVertexIDs[ 0 ] );
				const FVertexInstanceID EdgeVertexInstanceID1 = GetMeshDescription()->GetVertexInstanceForPolygonVertex( ConnectedPolygonID, OriginalEdgeVertexIDs[ 1 ] );

				BackupAllAttributes( PolygonToCreate.PerimeterVertices[ 0 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), EdgeVertexInstanceID1 );
				BackupAllAttributes( PolygonToCreate.PerimeterVertices[ 1 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), EdgeVertexInstanceID0 );
				BackupAllAttributes( PolygonToCreate.PerimeterVertices[ 2 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), EdgeVertexInstanceID0 );
				BackupAllAttributes( PolygonToCreate.PerimeterVertices[ 3 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), EdgeVertexInstanceID1 );
			}
		}

		// Create the polygons.  Note that this will also automatically create the missing side edges that connect
		// the original edge to it's extended counterpart.
		static TArray<FPolygonID> NewPolygonIDs;
		NewPolygonIDs.Reset();
		static TArray<FEdgeID> NewEdgeIDs;
		NewEdgeIDs.Reset();
		CreatePolygons( PolygonsToCreate, /* Out */ NewPolygonIDs, /* Out */ NewEdgeIDs );

		// Expecting no more than two new edges to be created while creating the polygon.  It's possible for zero or one edge
		// to be created, depending on how many edges we share with neighbors that were extended.
		check( bWeldNeighbors ? ( NewEdgeIDs.Num() <= 2 * EdgeIDs.Num() ) : ( NewEdgeIDs.Num() == 2 * EdgeIDs.Num() ) );
	}

	EM_EXIT( TEXT( "ExtendEdges returned %s" ), *LogHelpers::ArrayToString( OutNewExtendedEdgeIDs ) );
}


void UEditableMesh::ExtendVertices( const TArray<FVertexID>& VertexIDs, const bool bOnlyExtendClosestEdge, const FVector ReferencePosition, TArray<FVertexID>& OutNewExtendedVertexIDs )
{
	EM_ENTER( TEXT( "ExtendVertices: %s" ), *LogHelpers::ArrayToString( VertexIDs ) );

	OutNewExtendedVertexIDs.Reset();

	// Create new vertices for all of the new edges.  If bWeldNeighbors is true, we'll share vertices between edges that share the
	// same vertex instead of creating new edges.
	{
		static TArray<FVertexToCreate> VerticesToCreate;
		VerticesToCreate.Reset();
		VerticesToCreate.Reserve( VertexIDs.Num() );

		for( const FVertexID VertexID : VertexIDs )
		{
			const int32 CreatedVertexNumber = VerticesToCreate.Num();
			FVertexToCreate& VertexToCreate = *new( VerticesToCreate ) FVertexToCreate();

			BackupAllAttributes( VertexToCreate.VertexAttributes, GetMeshDescription()->VertexAttributes(), VertexID );
		}

		CreateVertices( VerticesToCreate, /* Out */ OutNewExtendedVertexIDs );
	}


	// For each vertex, we'll now create new triangles to connect the new vertex to each of the original vertex's adjacent vertices.
	// If the option bOnlyExtendClosestEdge was enabled, we'll only bother doing this for next closest vertex (so, only a single
	// triangle per vertex will be created.)
	{
		static TArray<FPolygonToCreate> PolygonsToCreate;
		PolygonsToCreate.Reset();

		const TVertexAttributesRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

		for( int32 VertexNumber = 0; VertexNumber < VertexIDs.Num(); ++VertexNumber )
		{
			const FVertexID OriginalVertexID = VertexIDs[ VertexNumber ];
			const FVertexID NewVertexID = OutNewExtendedVertexIDs[ VertexNumber ];

			FVertexID ClosestVertexID = FVertexID::Invalid;
			if( bOnlyExtendClosestEdge )
			{
				// Iterate over the edges connected to this vertex, and figure out which edge is closest to the 
				// specified reference position
				float ClosestSquaredEdgeDistance = TNumericLimits<float>::Max();

				const int32 ConnectedEdgeCount = GetVertexConnectedEdgeCount( OriginalVertexID );
				for( int32 EdgeNumber = 0; EdgeNumber < ConnectedEdgeCount; ++EdgeNumber )
				{
					const FEdgeID ConnectedEdgeID = GetVertexConnectedEdge( OriginalVertexID, EdgeNumber );

					FVertexID EdgeVertexIDs[ 2 ];
					GetEdgeVertices( ConnectedEdgeID, /* Out */ EdgeVertexIDs[0], /* Out */ EdgeVertexIDs[1] );

					const float SquaredEdgeDistance = FMath::PointDistToSegmentSquared( ReferencePosition, VertexPositions[ EdgeVertexIDs[ 0 ] ], VertexPositions[ EdgeVertexIDs[ 1 ] ] );
					if( SquaredEdgeDistance < ClosestSquaredEdgeDistance )
					{
						ClosestVertexID = EdgeVertexIDs[ 0 ] == OriginalVertexID ? EdgeVertexIDs[ 1 ] : EdgeVertexIDs[ 0 ];
						ClosestSquaredEdgeDistance = SquaredEdgeDistance;
					}
				}
			}

			static TArray<FVertexID> AdjacentVertexIDs;
			GetVertexAdjacentVertices( OriginalVertexID, /* Out */ AdjacentVertexIDs );

			// For every adjacent vertex, go ahead and create a new triangle
			for( const FVertexID AdjacentVertexID : AdjacentVertexIDs )
			{
				// If we were asked to only extend an edge that's closest to a reference position, check for that here
				if( !bOnlyExtendClosestEdge || ( AdjacentVertexID == ClosestVertexID ) )
				{
					FPolygonToCreate& PolygonToCreate = *new( PolygonsToCreate ) FPolygonToCreate();

					// Figure out which of the connected polygons shares the edge we're going to be using
					FPolygonID ConnectedPolygonID = FPolygonID::Invalid;
					{
						static TArray<FPolygonID> ConnectedPolygonIDs;
						GetVertexConnectedPolygons( OriginalVertexID, /* Out */ ConnectedPolygonIDs );

						for( const FPolygonID PolygonID : ConnectedPolygonIDs )
						{
							const int32 AdjacentVertexNumber = FindPolygonPerimeterVertexNumberForVertex( PolygonID, AdjacentVertexID );
							if( AdjacentVertexNumber != INDEX_NONE )
							{
								// NOTE: There can be more than one polygon that shares this edge.  We'll just take the first one we find.
								ConnectedPolygonID = PolygonID;
								break;
							}
						}
					}

					bool bConnectedPolygonWindsForward = true;
					if( ConnectedPolygonID != FPolygonID::Invalid )
					{
						const int32 OriginalVertexNumberOnConnectedPolygon = FindPolygonPerimeterVertexNumberForVertex( ConnectedPolygonID, OriginalVertexID );
						check( OriginalVertexNumberOnConnectedPolygon != INDEX_NONE );

						const int32 AdjacentVertexNumberOnConnectedPolygon = FindPolygonPerimeterVertexNumberForVertex( ConnectedPolygonID, AdjacentVertexID );
						check( AdjacentVertexNumberOnConnectedPolygon != INDEX_NONE );

						const int32 PerimeterVertexCount = GetPolygonPerimeterVertexCount( ConnectedPolygonID );
						if( !( OriginalVertexNumberOnConnectedPolygon == ( PerimeterVertexCount - 1 ) && AdjacentVertexNumberOnConnectedPolygon == 0 ) &&  // Doesn't wrap forward?
							( OriginalVertexNumberOnConnectedPolygon > AdjacentVertexNumberOnConnectedPolygon ||			// Winds backwards?
							  ( AdjacentVertexNumberOnConnectedPolygon == ( PerimeterVertexCount - 1 ) && OriginalVertexNumberOnConnectedPolygon == 0 ) ) )	// Wraps backwards?
						{
							bConnectedPolygonWindsForward = false;
						}
					}

					PolygonToCreate.PolygonGroupID = ( ConnectedPolygonID != FPolygonID::Invalid ) ? GetGroupForPolygon( ConnectedPolygonID ) : GetFirstValidPolygonGroup();
					check( PolygonToCreate.PolygonGroupID != FPolygonGroupID::Invalid );

					FVertexID ConnectedPolygonVertexIDsForTextureCoordinates[ 3 ];
					PolygonToCreate.PerimeterVertices.SetNum( 3, false );
					{
						int32 NextVertexNumber = 0;

						// Original selected vertex
						ConnectedPolygonVertexIDsForTextureCoordinates[ NextVertexNumber ] = OriginalVertexID;
						PolygonToCreate.PerimeterVertices[ NextVertexNumber++ ].VertexID = OriginalVertexID;

						if( bConnectedPolygonWindsForward )
						{
							// The new vertex we created
							ConnectedPolygonVertexIDsForTextureCoordinates[ NextVertexNumber ] = OriginalVertexID;
							PolygonToCreate.PerimeterVertices[ NextVertexNumber++ ].VertexID = NewVertexID;

							// The adjacent vertex
							ConnectedPolygonVertexIDsForTextureCoordinates[ NextVertexNumber ] = AdjacentVertexID;
							PolygonToCreate.PerimeterVertices[ NextVertexNumber++ ].VertexID = AdjacentVertexID;
						}
						else
						{
							// The adjacent vertex
							ConnectedPolygonVertexIDsForTextureCoordinates[ NextVertexNumber ] = AdjacentVertexID;
							PolygonToCreate.PerimeterVertices[ NextVertexNumber++ ].VertexID = AdjacentVertexID;

							// The new vertex we created
							ConnectedPolygonVertexIDsForTextureCoordinates[ NextVertexNumber ] = OriginalVertexID;
							PolygonToCreate.PerimeterVertices[ NextVertexNumber++ ].VertexID = NewVertexID;
						}
					}

					// Preserve polygon vertex attributes
					if( ConnectedPolygonID != FPolygonID::Invalid )
					{
						for( int32 PerimeterVertexNumber = 0; PerimeterVertexNumber < PolygonToCreate.PerimeterVertices.Num(); ++PerimeterVertexNumber )
						{
							const FVertexID ConnectedPolygonVertexIDForTextureCoordinates = ConnectedPolygonVertexIDsForTextureCoordinates[ PerimeterVertexNumber ];
							const FVertexInstanceID VertexInstanceID = GetMeshDescription()->GetVertexInstanceForPolygonVertex( ConnectedPolygonID, ConnectedPolygonVertexIDForTextureCoordinates );
							check( VertexInstanceID != FVertexInstanceID::Invalid );

							BackupAllAttributes( PolygonToCreate.PerimeterVertices[ PerimeterVertexNumber ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), VertexInstanceID );
						}
					}
				}
			}		
		}

		static TArray<FPolygonID> NewPolygonIDs;
		NewPolygonIDs.Reset();

		// Create the polygons.  Note that this will also automatically create the missing side edges that connect
		// the original edge to it's extended counterpart.
		static TArray<FEdgeID> NewEdgeIDs;
		NewEdgeIDs.Reset();
		CreatePolygons( PolygonsToCreate, /* Out */ NewPolygonIDs, /* Out */ NewEdgeIDs );
	}

	EM_EXIT( TEXT( "ExtendVertices returned %s" ), *LogHelpers::ArrayToString( OutNewExtendedVertexIDs ) );
}


void UEditableMesh::ComputePolygonsSharedEdges( const TArray<FPolygonID>& PolygonIDs, TArray<FEdgeID>& OutSharedEdgeIDs ) const
{
	OutSharedEdgeIDs.Reset();

	static TSet<FEdgeID> EdgesSeenSoFar;
	EdgesSeenSoFar.Reset();
	for( const FPolygonID PolygonID : PolygonIDs )
	{
		static TArray<FEdgeID> PolygonPerimeterEdgeIDs;
		GetPolygonPerimeterEdges( PolygonID, /* Out */ PolygonPerimeterEdgeIDs );

		for( const FEdgeID EdgeID : PolygonPerimeterEdgeIDs )
		{
			bool bWasAlreadyInSet = false;
			EdgesSeenSoFar.Add( EdgeID, /* Out */ &bWasAlreadyInSet );

			if( bWasAlreadyInSet )
			{
				// OK, this edge was referenced by more than one polygon!
				OutSharedEdgeIDs.Add( EdgeID );
			}
		}
	}
}


void UEditableMesh::BevelOrInsetPolygons( const TArray<FPolygonID>& PolygonIDs, const float InsetFixedDistance, const float InsetProgressTowardCenter, const EInsetPolygonsMode Mode, const bool bShouldBevel, TArray<FPolygonID>& OutNewCenterPolygonIDs, TArray<FPolygonID>& OutNewSidePolygonIDs )
{
	// @todo mesheditor inset/bevel: Weird feedback loop issues at glancing angles when moving mouse while beveling

	static TArray<FPolygonToCreate> SidePolygonsToCreate;
	SidePolygonsToCreate.Reset();

	static TArray<FPolygonToCreate> CenterPolygonsToCreate;
	CenterPolygonsToCreate.Reset();

	static TArray<FAttributesForVertex> AttributesForVertices;
	AttributesForVertices.Reset();

	const TVertexAttributesRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	for( const FPolygonID PolygonID : PolygonIDs )
	{
		const FPolygonGroupID PolygonGroupID = GetGroupForPolygon( PolygonID );

		// Find the center of this polygon
		const FVector PolygonCenter = ComputePolygonCenter( PolygonID );

		static TArray<FVertexID> PerimeterVertexIDs;
		GetPolygonPerimeterVertices( PolygonID, /* Out */ PerimeterVertexIDs );

		const TArray<FVertexInstanceID>& PerimeterVertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID );

		static TArray<FVertexToCreate> VerticesToCreate;
		VerticesToCreate.Reset( PerimeterVertexIDs.Num() );

		static TArray<TTuple<FMeshTriangle, FVector>> BarycentricWeightsForNewVertices;
		BarycentricWeightsForNewVertices.Reset();
		BarycentricWeightsForNewVertices.Reserve( PerimeterVertexIDs.Num() );

		for( int32 PerimeterVertexNumber = 0; PerimeterVertexNumber < PerimeterVertexIDs.Num(); ++PerimeterVertexNumber )
		{
			const FVertexID PerimeterVertexID = PerimeterVertexIDs[ PerimeterVertexNumber ];

			FVector OffsetDirection = FVector::ZeroVector;

			// If we're beveling, go ahead and move the original polygon perimeter vertices
			if( bShouldBevel )
			{
				// Figure out if this vertex is shared with other polygons that we were asked to bevel.  If it is,
				// then we'll want to offset the vertex in the average direction of all of those shared polygons.
				// However, if the vertex is ONLY shared with polygons we were asked to bevel (no other polygons),
				// then we don't need to move it at all -- it's an internal edge vertex.
				// @todo mesheditor bevel: With multiple polygons, we don't want to move vertices that are part of
				// a shared internal border edge.  But we're not handling that yet.
				int32 ConnectedBevelPolygonCount = 0;
				bool bIsOnlyConnectedToBevelPolygons = true;

				static TArray<FPolygonID> ConnectedPolygonIDs;
				GetVertexConnectedPolygons( PerimeterVertexID, /* Out */ ConnectedPolygonIDs );
				for( const FPolygonID ConnectedPolygonID : ConnectedPolygonIDs )
				{
					if( PolygonIDs.Contains( ConnectedPolygonID ) )
					{
						++ConnectedBevelPolygonCount;
						const FVector ConnectedPolygonNormal = ComputePolygonNormal( ConnectedPolygonID );
						OffsetDirection += -ConnectedPolygonNormal;
					}
					else
					{
						bIsOnlyConnectedToBevelPolygons = false;
					}
				}

				OffsetDirection.Normalize();
			}


			const FVector VertexPosition = VertexPositions[ PerimeterVertexID ];

			FVector DirectionTowardCenter;
			float DistanceToCenter;
			( PolygonCenter - VertexPosition ).ToDirectionAndLength( /* Out */ DirectionTowardCenter, /* Out */ DistanceToCenter );

			const float InsetOffset = ( DistanceToCenter * InsetProgressTowardCenter + InsetFixedDistance );
			const FVector InsetVertexPosition = VertexPosition + DirectionTowardCenter * InsetOffset;

			VerticesToCreate.Emplace();
			FVertexToCreate& VertexToCreate = VerticesToCreate.Last();
			VertexToCreate.VertexAttributes.Attributes.Emplace( MeshAttribute::Vertex::Position, 0, FMeshElementAttributeValue( InsetVertexPosition ) );

			// @todo mesheditor: interpolate all other vertex attributes (maybe handle this with InterpAllAttributes)

			// Determine the barycentric weights of
			FMeshTriangle Triangle;
			FVector TriangleVertexWeights;

			bool bPointInPolygon = ComputeBarycentricWeightForPointOnPolygon( PolygonID, InsetVertexPosition, Triangle, TriangleVertexWeights );
			BarycentricWeightsForNewVertices.Emplace( Triangle, TriangleVertexWeights );

			// If we're beveling, go ahead and move the original polygon perimeter vertices
			if( bShouldBevel )
			{
				// Offset the vertex by the opposite direction of the polygon's normal.  We'll move it the same distance
				// that we're insetting the new polygon
				const FVector NewVertexPosition = VertexPosition + OffsetDirection * InsetOffset;

				// @todo mesheditor urgent: Undo/Redo breaks if we have the same vertex more than once for some reason!!  Get rid of this ideally
				const bool bAlreadyHaveVertex = AttributesForVertices.ContainsByPredicate( [PerimeterVertexID]( FAttributesForVertex& AttributesForVertex ) { return AttributesForVertex.VertexID == PerimeterVertexID; } );
				if( !bAlreadyHaveVertex )
				{
					AttributesForVertices.Emplace();
					FAttributesForVertex& AttributesForVertex = AttributesForVertices.Last();
					AttributesForVertex.VertexID = PerimeterVertexID;
					AttributesForVertex.VertexAttributes.Attributes.Emplace( MeshAttribute::Vertex::Position, 0, FMeshElementAttributeValue( NewVertexPosition ) );
				}
			}
		}

		static TArray<FVertexID> NewVertexIDs;
		CreateVertices( VerticesToCreate, /* Out */ NewVertexIDs );

		// The new (inset) polygon will be surrounded by new "side" quad polygons, one for each vertex of the perimeter
		// that's being inset.
		if( Mode == EInsetPolygonsMode::All || Mode == EInsetPolygonsMode::SidePolygonsOnly )
		{
			const int32 NewSidePolygonCount = NewVertexIDs.Num();
			for( int32 SidePolygonNumber = 0; SidePolygonNumber < NewSidePolygonCount; ++SidePolygonNumber )
			{
				const int32 LeftVertexNumber = SidePolygonNumber;
				const int32 RightVertexNumber = ( LeftVertexNumber + 1 ) % NewSidePolygonCount;

				const FVertexID LeftOriginalVertexID = PerimeterVertexIDs[ LeftVertexNumber ];
				const FVertexID LeftInsetVertexID = NewVertexIDs[ LeftVertexNumber ];
				const FVertexID RightOriginalVertexID = PerimeterVertexIDs[ RightVertexNumber ];
				const FVertexID RightInsetVertexID = NewVertexIDs[ RightVertexNumber ];

				const FVertexInstanceID LeftOriginalVertexInstanceID = PerimeterVertexInstanceIDs[ LeftVertexNumber ];
				const FVertexInstanceID RightOriginalVertexInstanceID = PerimeterVertexInstanceIDs[ RightVertexNumber ];

				SidePolygonsToCreate.Emplace();
				FPolygonToCreate& PolygonToCreate = SidePolygonsToCreate.Last();
				PolygonToCreate.PolygonGroupID = PolygonGroupID;
				if( bShouldBevel )
				{
					PolygonToCreate.PolygonEdgeHardness = EPolygonEdgeHardness::AllEdgesHard;
				}

				PolygonToCreate.PerimeterVertices.SetNum( 4 );

				PolygonToCreate.PerimeterVertices[ 0 ].VertexID = LeftOriginalVertexID;
				PolygonToCreate.PerimeterVertices[ 1 ].VertexID = RightOriginalVertexID;
				PolygonToCreate.PerimeterVertices[ 2 ].VertexID = RightInsetVertexID;
				PolygonToCreate.PerimeterVertices[ 3 ].VertexID = LeftInsetVertexID;

				// Original left
				BackupAllAttributes( PolygonToCreate.PerimeterVertices[ 0 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), LeftOriginalVertexInstanceID );

				// Original right
				BackupAllAttributes( PolygonToCreate.PerimeterVertices[ 1 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), RightOriginalVertexInstanceID );

				// Inset right
				if( BarycentricWeightsForNewVertices[ RightVertexNumber ].Get<0>().VertexInstanceID0 != FVertexInstanceID::Invalid )
				{
					InterpAllAttributes( PolygonToCreate.PerimeterVertices[ 2 ].PolygonVertexAttributes,
										 GetMeshDescription()->VertexInstanceAttributes(),
										 BarycentricWeightsForNewVertices[ RightVertexNumber ].Get<0>().VertexInstanceID0,
										 BarycentricWeightsForNewVertices[ RightVertexNumber ].Get<0>().VertexInstanceID1,
										 BarycentricWeightsForNewVertices[ RightVertexNumber ].Get<0>().VertexInstanceID2,
										 BarycentricWeightsForNewVertices[ RightVertexNumber ].Get<1>() );
				}
				else
				{
					BackupAllAttributes( PolygonToCreate.PerimeterVertices[ 2 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), RightOriginalVertexInstanceID );
				}

				// Inset left
				if( BarycentricWeightsForNewVertices[ LeftVertexNumber ].Get<0>().VertexInstanceID0 != FVertexInstanceID::Invalid )
				{
					InterpAllAttributes( PolygonToCreate.PerimeterVertices[ 3 ].PolygonVertexAttributes,
										 GetMeshDescription()->VertexInstanceAttributes(),
										 BarycentricWeightsForNewVertices[ LeftVertexNumber ].Get<0>().VertexInstanceID0,
										 BarycentricWeightsForNewVertices[ LeftVertexNumber ].Get<0>().VertexInstanceID1,
										 BarycentricWeightsForNewVertices[ LeftVertexNumber ].Get<0>().VertexInstanceID2,
										 BarycentricWeightsForNewVertices[ LeftVertexNumber ].Get<1>() );
				}
				else
				{
					BackupAllAttributes( PolygonToCreate.PerimeterVertices[ 3 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), LeftOriginalVertexInstanceID );
				}
			}
		}

		// Now create the new center polygon that will connect all of the new inset vertices
		if( Mode == EInsetPolygonsMode::All || Mode == EInsetPolygonsMode::CenterPolygonOnly )
		{
			FPolygonToCreate& PolygonToCreate = *new( CenterPolygonsToCreate ) FPolygonToCreate();
			PolygonToCreate.PolygonGroupID = PolygonGroupID;
			if( bShouldBevel )
			{
				PolygonToCreate.PolygonEdgeHardness = EPolygonEdgeHardness::AllEdgesHard;
			}

			PolygonToCreate.PerimeterVertices.SetNum( NewVertexIDs.Num() );

			for( int32 NewVertexNumber = 0; NewVertexNumber < NewVertexIDs.Num(); ++NewVertexNumber )
			{
				const FVertexID NewVertexID = NewVertexIDs[ NewVertexNumber ];

				FVertexAndAttributes& NewPerimeterVertex = PolygonToCreate.PerimeterVertices[ NewVertexNumber ];
				NewPerimeterVertex.VertexID = NewVertexID;

				if( BarycentricWeightsForNewVertices[ NewVertexNumber ].Get<0>().VertexInstanceID0 != FVertexInstanceID::Invalid )
				{
					InterpAllAttributes( NewPerimeterVertex.PolygonVertexAttributes,
										 GetMeshDescription()->VertexInstanceAttributes(),
										 BarycentricWeightsForNewVertices[ NewVertexNumber ].Get<0>().VertexInstanceID0,
										 BarycentricWeightsForNewVertices[ NewVertexNumber ].Get<0>().VertexInstanceID1,
										 BarycentricWeightsForNewVertices[ NewVertexNumber ].Get<0>().VertexInstanceID2,
										 BarycentricWeightsForNewVertices[ NewVertexNumber ].Get<1>() );
				}
				else
				{
					BackupAllAttributes( NewPerimeterVertex.PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), PerimeterVertexInstanceIDs[ NewVertexNumber ] );
				}
			}
		}
	}

	// Delete the original polygons
	{
		const bool bDeleteOrphanedEdges = false;	// No need to delete orphans, because this function won't orphan anything
		const bool bDeleteOrphanedVertices = false;
		const bool bDeleteOrphanedVertexInstances = true;
		const bool bDeleteEmptyPolygonGroups = false;
		DeletePolygons( PolygonIDs, bDeleteOrphanedEdges, bDeleteOrphanedVertices, bDeleteOrphanedVertexInstances, bDeleteEmptyPolygonGroups );
	}

	// @todo mesheditor inset: Should we create the new inset polygon with hard edges or soft?  Currently using the default.

	// Updated any vertices that need to be moved
	if( AttributesForVertices.Num() > 0 )
	{
		SetVerticesAttributes( AttributesForVertices );
	}

	if( SidePolygonsToCreate.Num() > 0 )
	{
		static TArray<FEdgeID> NewEdgeIDs;
		CreatePolygons( SidePolygonsToCreate, /* Out */ OutNewSidePolygonIDs, /* Out */ NewEdgeIDs );
	}

	// Create the new center polygons.  Note that we pass back the IDs of the newly-created center polygons
	if( CenterPolygonsToCreate.Num() > 0 )
	{
		static TArray<FEdgeID> NewEdgeIDs;
		CreatePolygons( CenterPolygonsToCreate, /* Out */ OutNewCenterPolygonIDs, /* Out */ NewEdgeIDs );
	}
}


void UEditableMesh::InsetPolygons( const TArray<FPolygonID>& PolygonIDs, const float InsetFixedDistance, const float InsetProgressTowardCenter, const EInsetPolygonsMode Mode, TArray<FPolygonID>& OutNewCenterPolygonIDs, TArray<FPolygonID>& OutNewSidePolygonIDs )
{
	EM_ENTER( TEXT( "InsetPolygons: %s %f %f" ), *LogHelpers::ArrayToString( PolygonIDs ), InsetFixedDistance, InsetProgressTowardCenter );

	const bool bShouldBevel = false;
	BevelOrInsetPolygons( PolygonIDs, InsetFixedDistance, InsetProgressTowardCenter, Mode, bShouldBevel, /* Out */ OutNewCenterPolygonIDs, /* Out */ OutNewSidePolygonIDs );

	EM_EXIT( TEXT( "InsetPolygons returned %s %s" ), *LogHelpers::ArrayToString( OutNewCenterPolygonIDs ), *LogHelpers::ArrayToString( OutNewSidePolygonIDs ) );
}


void UEditableMesh::BevelPolygons( const TArray<FPolygonID>& PolygonIDs, const float BevelFixedDistance, const float BevelProgressTowardCenter, TArray<FPolygonID>& OutNewCenterPolygonIDs, TArray<FPolygonID>& OutNewSidePolygonIDs )
{
	EM_ENTER( TEXT( "BevelPolygons: %s %f %f" ), *LogHelpers::ArrayToString( PolygonIDs ), BevelFixedDistance, BevelProgressTowardCenter );

	const bool bShouldBevel = true;
	BevelOrInsetPolygons( PolygonIDs, BevelFixedDistance, BevelProgressTowardCenter, EInsetPolygonsMode::All, bShouldBevel, /* Out */ OutNewCenterPolygonIDs, /* Out */ OutNewSidePolygonIDs );

	EM_EXIT( TEXT( "BevelPolygons returned %s %s" ), *LogHelpers::ArrayToString( OutNewCenterPolygonIDs ), *LogHelpers::ArrayToString( OutNewSidePolygonIDs ) );
}


float UEditableMesh::GetPolygonCornerAngleForVertex( const FPolygonID PolygonID, const FVertexID VertexID ) const
{
	const FMeshPolygon& Polygon = GetMeshDescription()->GetPolygon( PolygonID );

	// Lambda function which returns the inner angle at a given index on a polygon contour
	auto GetContourAngle = [ this ]( const FMeshPolygonContour& Contour, const int32 ContourIndex )
	{
		const int32 NumVertices = Contour.VertexInstanceIDs.Num();

		const int32 PrevIndex = ( ContourIndex + NumVertices - 1 ) % NumVertices;
		const int32 NextIndex = ( ContourIndex + 1 ) % NumVertices;

		const FVertexID PrevVertexID = GetMeshDescription()->GetVertexInstanceVertex( Contour.VertexInstanceIDs[ PrevIndex ] );
		const FVertexID ThisVertexID = GetMeshDescription()->GetVertexInstanceVertex( Contour.VertexInstanceIDs[ ContourIndex ] );
		const FVertexID NextVertexID = GetMeshDescription()->GetVertexInstanceVertex( Contour.VertexInstanceIDs[ NextIndex ] );

		TVertexAttributesConstRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

		const FVector PrevVertexPosition = VertexPositions[ PrevVertexID ];
		const FVector ThisVertexPosition = VertexPositions[ ThisVertexID ];
		const FVector NextVertexPosition = VertexPositions[ NextVertexID ];

		const FVector Direction1 = ( PrevVertexPosition - ThisVertexPosition ).GetSafeNormal();
		const FVector Direction2 = ( NextVertexPosition - ThisVertexPosition ).GetSafeNormal();

		return FMath::Acos( FVector::DotProduct( Direction1, Direction2 ) );
	};

	const FVertexInstanceArray& VertexInstances = GetMeshDescription()->VertexInstances();
	auto IsVertexInstancedFromThisVertex = [ &VertexInstances, VertexID ]( const FVertexInstanceID VertexInstanceID )
	{
		return VertexInstances[ VertexInstanceID ].VertexID == VertexID;
	};

	// First look for the vertex instance in the perimeter
	int32 ContourIndex = Polygon.PerimeterContour.VertexInstanceIDs.IndexOfByPredicate( IsVertexInstancedFromThisVertex );
	if( ContourIndex != INDEX_NONE )
	{
		// Return the internal angle if found
		return GetContourAngle( Polygon.PerimeterContour, ContourIndex );
	}

	// Found nothing; return 0
	return 0.0f;
}


void UEditableMesh::GeneratePolygonTangentsAndNormals( const TArray<FPolygonID>& PolygonIDs )
{
	const TVertexAttributesRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	// @todo mesheditor: currently hardcoded to calculate the tangent basis from UV0.
	const TVertexInstanceAttributesRef<FVector2D> VertexUVs = GetMeshDescription()->VertexInstanceAttributes().GetAttributesRef<FVector2D>( MeshAttribute::VertexInstance::TextureCoordinate );

	TPolygonAttributesRef<FVector> PolygonNormals = GetMeshDescription()->PolygonAttributes().GetAttributesRef<FVector>( MeshAttribute::Polygon::Normal );
	TPolygonAttributesRef<FVector> PolygonTangents = GetMeshDescription()->PolygonAttributes().GetAttributesRef<FVector>( MeshAttribute::Polygon::Tangent );
	TPolygonAttributesRef<FVector> PolygonBinormals = GetMeshDescription()->PolygonAttributes().GetAttributesRef<FVector>( MeshAttribute::Polygon::Binormal );
	TPolygonAttributesRef<FVector> PolygonCenters = GetMeshDescription()->PolygonAttributes().GetAttributesRef<FVector>( MeshAttribute::Polygon::Center );

	for( const FPolygonID PolygonID : PolygonIDs )
	{
		// Calculate the center of this polygon
		FVector Center = FVector::ZeroVector;
		const TArray<FVertexInstanceID>& VertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID );
		for( const FVertexInstanceID VertexInstanceID : VertexInstanceIDs )
		{
			Center += VertexPositions[ GetMeshDescription()->GetVertexInstanceVertex( VertexInstanceID ) ];
		}
		Center /= float( VertexInstanceIDs.Num() );

		// Calculate the tangent basis for the polygon, based on the average of all constituent triangles
		FVector Normal = FVector::ZeroVector;
		FVector Tangent = FVector::ZeroVector;
		FVector Binormal = FVector::ZeroVector;

		for( const FMeshTriangle& Triangle : GetMeshDescription()->GetPolygonTriangles( PolygonID ) )
		{
			const FVertexID VertexID0 = GetMeshDescription()->GetVertexInstanceVertex( Triangle.VertexInstanceID0 );
			const FVertexID VertexID1 = GetMeshDescription()->GetVertexInstanceVertex( Triangle.VertexInstanceID1 );
			const FVertexID VertexID2 = GetMeshDescription()->GetVertexInstanceVertex( Triangle.VertexInstanceID2 );

			const FVector DPosition1 = VertexPositions[ VertexID1 ] - VertexPositions[ VertexID0 ];
			const FVector DPosition2 = VertexPositions[ VertexID2 ] - VertexPositions[ VertexID0 ];

			const FVector2D DUV1 = VertexUVs.Get( Triangle.VertexInstanceID1, 0 ) - VertexUVs.Get( Triangle.VertexInstanceID0, 0 );
			const FVector2D DUV2 = VertexUVs.Get( Triangle.VertexInstanceID2, 0 ) - VertexUVs.Get( Triangle.VertexInstanceID0, 0 );

			// We have a left-handed coordinate system, but a counter-clockwise winding order
			// Hence normal calculation has to take the triangle vectors cross product in reverse.
			Normal += FVector::CrossProduct( DPosition2, DPosition1 );

			// ...and tangent space seems to be right-handed.
			const float DetUV = FVector2D::CrossProduct( DUV1, DUV2 );
			const float InvDetUV = ( DetUV == 0.0f ) ? 0.0f : 1.0f / DetUV;

			Tangent += ( DPosition1 * DUV2.Y - DPosition2 * DUV1.Y ) * InvDetUV;
			Binormal += ( DPosition2 * DUV1.X - DPosition1 * DUV2.X ) * InvDetUV;
		}

		PolygonNormals[ PolygonID ] = Normal.GetSafeNormal();
		PolygonTangents[ PolygonID ] = Tangent.GetSafeNormal();
		PolygonBinormals[ PolygonID ] = Binormal.GetSafeNormal();
		PolygonCenters[ PolygonID ] = Center;
	}
}


void UEditableMesh::GenerateTangentsAndNormals()
{
	const TPolygonAttributesRef<FVector> PolygonNormals = GetMeshDescription()->PolygonAttributes().GetAttributesRef<FVector>( MeshAttribute::Polygon::Normal );
	const TPolygonAttributesRef<FVector> PolygonTangents = GetMeshDescription()->PolygonAttributes().GetAttributesRef<FVector>( MeshAttribute::Polygon::Tangent );
	const TPolygonAttributesRef<FVector> PolygonBinormals = GetMeshDescription()->PolygonAttributes().GetAttributesRef<FVector>( MeshAttribute::Polygon::Binormal );

	GeneratePolygonTangentsAndNormals( PolygonsPendingNewTangentBasis.Array() );

	static TSet<FVertexInstanceID> VertexInstanceIDs;
	VertexInstanceIDs.Reset();

	for( const FPolygonID PolygonID : PolygonsPendingNewTangentBasis )
	{
		VertexInstanceIDs.Append( GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID ) );
	}

	static TArray<FAttributesForVertexInstance> AttributesForVertexInstances;
	AttributesForVertexInstances.Reset( VertexInstanceIDs.Num() );

	for( const FVertexInstanceID VertexInstanceID : VertexInstanceIDs )
	{
		FVector Normal = FVector::ZeroVector;
		FVector Tangent = FVector::ZeroVector;
		FVector Binormal = FVector::ZeroVector;

		const FVertexID VertexID = GetMeshDescription()->GetVertexInstanceVertex( VertexInstanceID );

		// Get all polygons connected to this vertex instance, and also any in the same smoothing group connected to a different vertex instance
		// (as they still have influence over the normal).
		static TArray<FPolygonID> AllConnectedPolygons;
		const TArray<FPolygonID>& VertexInstanceConnectedPolygons = GetMeshDescription()->GetVertexInstanceConnectedPolygons( VertexInstanceID );
		check( VertexInstanceConnectedPolygons.Num() > 0 );
		GetVertexConnectedPolygonsInSameSoftEdgedGroup( VertexID, VertexInstanceConnectedPolygons[ 0 ], AllConnectedPolygons );

		// The vertex instance normal is computed as a sum of all connected polygons' normals, weighted by the angle they make with the vertex
		for( const FPolygonID ConnectedPolygonID : AllConnectedPolygons )
		{
			const float Angle = GetPolygonCornerAngleForVertex( ConnectedPolygonID, VertexID );

			Normal += PolygonNormals[ ConnectedPolygonID ] * Angle;

			// If this polygon is actually connected to the vertex instance we're processing, also include its contributions towards the tangent
			if( VertexInstanceConnectedPolygons.Contains( ConnectedPolygonID ) )
			{
				Tangent += PolygonTangents[ ConnectedPolygonID ] * Angle;
				Binormal += PolygonBinormals[ ConnectedPolygonID ] * Angle;
			}
		}

		// Normalize Normal
		Normal = Normal.GetSafeNormal();

		// Make Tangent orthonormal to Normal.
		// This is a quicker method than normalizing Tangent, taking the cross product Normal X Tangent, and then a further cross product with that result
		Tangent = ( Tangent - Normal * FVector::DotProduct( Normal, Tangent ) ).GetSafeNormal();

		// Calculate binormal sign
		const float BinormalSign = ( FVector::DotProduct( FVector::CrossProduct( Normal, Tangent ), Binormal ) < 0.0f ) ? -1.0f : 1.0f;

		//UE_LOG( LogEditableMesh, Verbose, TEXT( "%s normal: old %s" ), *VertexInstanceID.ToString(), *VertexInstance.Normal.ToString() );
		//UE_LOG( LogEditableMesh, Verbose, TEXT( "%s normal: new %s" ), *VertexInstanceID.ToString(), *Normal.ToString() );

		//UE_LOG( LogEditableMesh, Verbose, TEXT( "%s tangent: old %s" ), *VertexInstanceID.ToString(), *VertexInstance.Tangent.ToString() );
		//UE_LOG( LogEditableMesh, Verbose, TEXT( "%s tangent: new %s" ), *VertexInstanceID.ToString(), *Tangent.ToString() );

		AttributesForVertexInstances.Emplace();
		FAttributesForVertexInstance& AttributesForVertexInstance = AttributesForVertexInstances.Last();

		AttributesForVertexInstance.VertexInstanceID = VertexInstanceID;
		AttributesForVertexInstance.VertexInstanceAttributes.Attributes.Reset( 3 );
		AttributesForVertexInstance.VertexInstanceAttributes.Attributes.Emplace( MeshAttribute::VertexInstance::Normal, 0, FMeshElementAttributeValue( Normal ) );
		AttributesForVertexInstance.VertexInstanceAttributes.Attributes.Emplace( MeshAttribute::VertexInstance::Tangent, 0, FMeshElementAttributeValue( Tangent ) );
		AttributesForVertexInstance.VertexInstanceAttributes.Attributes.Emplace( MeshAttribute::VertexInstance::BinormalSign, 0, FMeshElementAttributeValue( BinormalSign ) );
	}

	SetVertexInstancesAttributes( AttributesForVertexInstances );
}

void UEditableMesh::FlipTangentsAndNormals()
{
	const TVertexInstanceAttributesRef<FVector> VertexNormals = GetMeshDescription()->VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Normal );
	const TVertexInstanceAttributesRef<FVector> VertexTangents = GetMeshDescription()->VertexInstanceAttributes().GetAttributesRef<FVector>( MeshAttribute::VertexInstance::Tangent );
	const TVertexInstanceAttributesRef<float> VertexBinormals = GetMeshDescription()->VertexInstanceAttributes().GetAttributesRef<float>( MeshAttribute::VertexInstance::BinormalSign );

	static TSet<FVertexInstanceID> VertexInstanceIDs;
	VertexInstanceIDs.Reset();

	for( const FPolygonID PolygonID : PolygonsPendingFlipTangentBasis )
	{
		VertexInstanceIDs.Append( GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID ) );
	}

	static TArray<FAttributesForVertexInstance> AttributesForVertexInstances;
	AttributesForVertexInstances.Reset( VertexInstanceIDs.Num() );

	for( const FVertexInstanceID VertexInstanceID : VertexInstanceIDs )
	{
		const FVertexID VertexID = GetMeshDescription()->GetVertexInstanceVertex( VertexInstanceID );

		// Just reverse the sign of the normals/tangents; note that since binormals are the cross product of normal with tangent, they are left untouched
		FVector Normal = VertexNormals[ VertexInstanceID ] * -1.0f;
		FVector Tangent = VertexTangents[ VertexInstanceID ] * -1.0f;
		float BinormalSign = VertexBinormals[ VertexInstanceID ];

		FAttributesForVertexInstance& AttributesForVertexInstance = AttributesForVertexInstances.AddDefaulted_GetRef();

		AttributesForVertexInstance.VertexInstanceID = VertexInstanceID;
		AttributesForVertexInstance.VertexInstanceAttributes.Attributes.Reset( 3 );
		AttributesForVertexInstance.VertexInstanceAttributes.Attributes.Emplace( MeshAttribute::VertexInstance::Normal, 0, FMeshElementAttributeValue( Normal ) );
		AttributesForVertexInstance.VertexInstanceAttributes.Attributes.Emplace( MeshAttribute::VertexInstance::Tangent, 0, FMeshElementAttributeValue( Tangent ) );
		AttributesForVertexInstance.VertexInstanceAttributes.Attributes.Emplace( MeshAttribute::VertexInstance::BinormalSign, 0, FMeshElementAttributeValue( BinormalSign ) );
	}

	SetVertexInstancesAttributes( AttributesForVertexInstances );
}

void UEditableMesh::SplitPolygonalMesh(const FPlane& InPlane, TArray<FPolygonID>& PolygonIDs1, TArray<FPolygonID>& PolygonIDs2, TArray<FEdgeID>& BoundaryEdges)
{
	TPolygonAttributesConstRef<FVector> PolygonCenters = GetMeshDescription()->PolygonAttributes().GetAttributesRef<FVector>(MeshAttribute::Polygon::Center);
	TVertexAttributesConstRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);

    // Find potential polygons to split
    TArray<FPolygonID> PotentialPolygonsToSplit;
    SearchSpatialDatabaseForPolygonsPotentiallyIntersectingPlane(InPlane, PotentialPolygonsToSplit);
    TSet<FPolygonID> PotentialPolygonsToSplitSet;
    for (const auto& PolygonID : PotentialPolygonsToSplit)
    {
        PotentialPolygonsToSplitSet.Add(PolygonID);
    }

    TMap<FPolygonID, TArray<FEdgeID>> PolygonToEdgesMap;
    for (const auto& PolygonID : GetMeshDescription()->Polygons().GetElementIDs())
    {
        PolygonToEdgesMap.Add(PolygonID, {});
        GetMeshDescription()->GetPolygonEdges(PolygonID, PolygonToEdgesMap[PolygonID]);
    }
    
    // Find polygons that need to be split and if they don't need to add them to the appropriate list
    TArray<FPolygonToSplit> PolygonsToSplit;
    TMap<FEdgeID, FVertexID> EdgeToSplitVertMap;
    for (const auto& PolygonID : GetMeshDescription()->Polygons().GetElementIDs())
    {
        bool bIsPolygonIntersecting = false;
        if (PotentialPolygonsToSplitSet.Contains(PolygonID))
        {
            TSet<FVertexInstanceID> VertexInstanceIDs;
            VertexInstanceIDs.Append(GetMeshDescription()->GetPolygonPerimeterVertexInstances(PolygonID));
            int32 PosNeg = 0;
            TArray<bool> PosNegResults;
            for (const auto& VertexInstanceID : VertexInstanceIDs)
            {
                const FVertexID VertexID = GetMeshDescription()->GetVertexInstanceVertex(VertexInstanceID);
                const float PlaneDot = InPlane.PlaneDot(VertexPositions[VertexID]);
                if (PlaneDot > 0)
                {
                    if (PosNeg < 0)
                    {
                        bIsPolygonIntersecting = true;
                        break;
                    }
                    else if (PosNeg == 0)
                    {
                        PosNeg = 1;
                    }
                }
                else if (PlaneDot < 0)
                {
                    if (PosNeg > 0)
                    {
                        bIsPolygonIntersecting = true;
                        break;
                    }
                    else if (PosNeg == 0)
                    {
                        PosNeg = -1;
                    }
                }
                PosNegResults.Add(PlaneDot > 0);
            }
        }
        if (bIsPolygonIntersecting)
        {
            const TArray<FEdgeID>& EdgeIDs = PolygonToEdgesMap[PolygonID];
            TArray<FPolygonID> TriangulatedPolygons;
            if (EdgeIDs.Num() > 3)
            {
                TArray<FPolygonID> PolygonIDs, OutPolygonIDs;
                TriangulatePolygons(PolygonIDs, OutPolygonIDs);
                for (const auto& NewPolygonID : OutPolygonIDs)
                {
                    TriangulatedPolygons.Add(NewPolygonID);
                    PolygonToEdgesMap.Add(NewPolygonID, {});
                    GetMeshDescription()->GetPolygonEdges(PolygonID, PolygonToEdgesMap[NewPolygonID]);
                }
            }
            else
            {
                TriangulatedPolygons.Add(PolygonID);
            }
            for (int32 i = 0; i < TriangulatedPolygons.Num(); ++i)
            {
                const auto& TriPolygonID = TriangulatedPolygons[i];
                const auto& TriEdgeIDs = PolygonToEdgesMap[TriPolygonID];
                check(TriEdgeIDs.Num() == 3);
                TArray<FVertexID> SplitVertexIDs;
                for (const auto& EdgeID : TriEdgeIDs)
                {
                    if (EdgeToSplitVertMap.Contains(EdgeID))
                    {
                        SplitVertexIDs.Add(EdgeToSplitVertMap[EdgeID]);
                    }
                    else
                    {
                        const auto& VertexID0 = GetMeshDescription()->GetEdgeVertex(EdgeID, 0);
                        const auto& VertexID1 = GetMeshDescription()->GetEdgeVertex(EdgeID, 1);
                        const bool Sign0 = InPlane.PlaneDot(VertexPositions[VertexID0]) > 0;
                        const bool Sign1 = InPlane.PlaneDot(VertexPositions[VertexID1]) > 0;
                        if (Sign0 != Sign1)
                        {
                            const FVector EdgeVertex0Location = VertexPositions[VertexID0];
                            const FVector EdgeVertex1Location = VertexPositions[VertexID1];

                            const FVector Direction = (EdgeVertex1Location - EdgeVertex0Location).GetSafeNormal();
                            const FVector Intersection = FMath::RayPlaneIntersection(EdgeVertex0Location, Direction, InPlane);

                            const float EdgeLength = (EdgeVertex1Location - EdgeVertex0Location).Size();
                            const float ImpactProgressAlongEdge = (Intersection - EdgeVertex0Location).Size() / EdgeLength;

                            TArray<FVertexID> NewVertexIDs;
                            TArray<float> SplitEdgeSplitList;
                            SplitEdgeSplitList.SetNumUninitialized(1);
                            SplitEdgeSplitList[0] = ImpactProgressAlongEdge;

                            SplitEdge(EdgeID, SplitEdgeSplitList, NewVertexIDs);
                            check(NewVertexIDs.Num() == 1);
                            SplitVertexIDs.Add(NewVertexIDs[0]);
                            EdgeToSplitVertMap.Add(EdgeID, NewVertexIDs[0]);
                        }
                    }
                }
                // If we split then we may not be intersection
                if (SplitVertexIDs.Num())
                {
                    check(SplitVertexIDs.Num() == 2);
                    FPolygonToSplit PolygonToSplit;
                    PolygonToSplit.PolygonID = TriPolygonID;
                    PolygonToSplit.VertexPairsToSplitAt.SetNum(1);
                    PolygonToSplit.VertexPairsToSplitAt[0].VertexID0 = SplitVertexIDs[0];
                    PolygonToSplit.VertexPairsToSplitAt[0].VertexID1 = SplitVertexIDs[1];
                    PolygonsToSplit.Add(PolygonToSplit);
                }
                else
                {
                    //if (InPlane.PlaneDot(PolygonCenters[PolygonID]) >= 0)
                    if (InPlane.PlaneDot(UEditableMesh::ComputePolygonCenter(PolygonID)) >= 0)
                    {
                        PolygonIDs1.Add(PolygonID);
                    }
                    else
                    {
                        PolygonIDs2.Add(PolygonID);
                    }
                }
            }
        }
        else
        {
            //if (InPlane.PlaneDot(PolygonCenters[PolygonID]) >= 0)
            if (InPlane.PlaneDot(UEditableMesh::ComputePolygonCenter(PolygonID)) >= 0)
            {
                PolygonIDs1.Add(PolygonID);
            }
            else
            {
                PolygonIDs2.Add(PolygonID);
            }
        }
    }

    // Split polygons and add new ones to the correct list
    SplitPolygons(PolygonsToSplit, BoundaryEdges);
    for (const auto& EdgeID : BoundaryEdges)
    {
        const TArray<FPolygonID>& EdgeIDConnectedPolygons = GetMeshDescription()->GetEdgeConnectedPolygons(EdgeID);
        for (const auto& NewPolygonID : EdgeIDConnectedPolygons)
        {
            //if (InPlane.PlaneDot(PolygonCenters[NewPolygonID]) >= 0)
            if (InPlane.PlaneDot(UEditableMesh::ComputePolygonCenter(NewPolygonID)) >= 0)
            {
                PolygonIDs1.Add(NewPolygonID);
            }
            {
                PolygonIDs2.Add(NewPolygonID);
            }
        }
    }
}

void UEditableMesh::GeometryHitTest(const FHitParamsIn& InParams, FHitParamsOut& OutParams)
{
#if WITH_EDITOR
	if (PrimaryAdapter)
	{
		PrimaryAdapter->GeometryHitTest(InParams, OutParams);
	}
#endif // WITH_EDITOR
}

#if 0
void UEditableMesh::GenerateTangentsForPolygons( const TArray< FPolygonID >& PolygonIDs )
{
	// @todo mesheditor: This method is currently not used. Revisit MikkTSpace in the future.
	static TArray<FVertexAttributesForPolygon> VertexAttributesForPolygons;
	VertexAttributesForPolygons.Reset();
	VertexAttributesForPolygons.Reserve( PolygonIDs.Num() );
	for( const FPolygonID PolygonID : PolygonIDs )
	{
		FVertexAttributesForPolygon& PolygonNewAttributes = *new( VertexAttributesForPolygons ) FVertexAttributesForPolygon();
		PolygonNewAttributes.PolygonID = PolygonID;

		const int32 PolygonPerimeterVertexCount = GetPolygonPerimeterVertexCount( PolygonID );
		PolygonNewAttributes.PerimeterVertexAttributeLists.SetNum( PolygonPerimeterVertexCount, false );
	}

	struct FMikkUserData
	{
		UEditableMesh* Self;
		const TArray< FPolygonID >& Polygons;
		TArray<FVertexAttributesForPolygon>& VertexAttributesForPolygons;

		FMikkUserData( UEditableMesh* InitSelf, const TArray< FPolygonID >& InitPolygons, TArray<FVertexAttributesForPolygon>& InitVertexAttributesForPolygons )
			: Self( InitSelf ),
			  Polygons( InitPolygons ),
			  VertexAttributesForPolygons( InitVertexAttributesForPolygons )
		{
		}
	} MikkUserData( this, PolygonIDs, VertexAttributesForPolygons );

	struct Local
	{
		static int MikkGetNumFaces( const SMikkTSpaceContext* Context )
		{
			const FMikkUserData& UserData = *static_cast<FMikkUserData*>( Context->m_pUserData );
			return UserData.Polygons.Num();
		}

		static int MikkGetNumVertsOfFace( const SMikkTSpaceContext* Context, const int MikkFaceIndex )
		{
			const FMikkUserData& UserData = *static_cast<FMikkUserData*>( Context->m_pUserData );

			const FPolygonID PolygonID = UserData.Polygons[ MikkFaceIndex ];

			const int32 PolygonVertexCount = UserData.Self->GetPolygonPerimeterVertexCount( PolygonID );
			return PolygonVertexCount;
		}

		static void MikkGetPosition( const SMikkTSpaceContext* Context, float OutPosition[3], const int MikkFaceIndex, const int MikkVertexIndex )
		{
			const FMikkUserData& UserData = *static_cast<FMikkUserData*>( Context->m_pUserData );

			const FPolygonID PolygonID = UserData.Polygons[ MikkFaceIndex ];
			const FVector VertexPosition = UserData.Self->GetPolygonPerimeterVertexAttribute( PolygonID, MikkVertexIndex, MeshAttribute::Vertex::Position, 0 );

			OutPosition[0] = VertexPosition.X;
			OutPosition[1] = VertexPosition.Y;
			OutPosition[2] = VertexPosition.Z;
		}

		static void MikkGetNormal( const SMikkTSpaceContext* Context, float OutNormal[3], const int MikkFaceIndex, const int MikkVertexIndex )
		{
			const FMikkUserData& UserData = *static_cast<FMikkUserData*>( Context->m_pUserData );

			const FPolygonID PolygonID = UserData.Polygons[ MikkFaceIndex ];
			const FVector PolygonVertexNormal = UserData.Self->GetPolygonPerimeterVertexAttribute( PolygonID, MikkVertexIndex, MeshAttribute::VertexInstance::Normal, 0 );

			OutNormal[ 0 ] = PolygonVertexNormal.X;
			OutNormal[ 1 ] = PolygonVertexNormal.Y;
			OutNormal[ 2 ] = PolygonVertexNormal.Z;
		}

		static void MikkGetTexCoord( const SMikkTSpaceContext* Context, float OutUV[2], const int MikkFaceIndex, const int MikkVertexIndex )
		{
			const FMikkUserData& UserData = *static_cast<FMikkUserData*>( Context->m_pUserData );

			// @todo mesheditor: Support using a custom texture coordinate index for tangent space generation?
			const int32 TextureCoordinateIndex = 0;

			const FPolygonID PolygonID = UserData.Polygons[ MikkFaceIndex ];
			const FVector2D PolygonVertexTextureCoordinate( UserData.Self->GetPolygonPerimeterVertexAttribute( PolygonID, MikkVertexIndex, MeshAttribute::VertexInstance::TextureCoordinate, TextureCoordinateIndex ) );

			OutUV[0] = PolygonVertexTextureCoordinate.X;
			OutUV[1] = PolygonVertexTextureCoordinate.Y;
		}

		static void MikkSetTSpaceBasic( const SMikkTSpaceContext* Context, const float Tangent[3], const float BitangentSign, const int MikkFaceIndex, const int MikkVertexIndex )
		{
			FMikkUserData& UserData = *static_cast<FMikkUserData*>( Context->m_pUserData );

			const FVector NewTangent( Tangent[ 0 ], Tangent[ 1 ], Tangent[ 2 ] );

			// Save it!
			TArray<FMeshElementAttributeData>& PerimeterVertexNewAttributes = UserData.VertexAttributesForPolygons[ MikkFaceIndex ].PerimeterVertexAttributeLists[ MikkVertexIndex ].Attributes;
			check( PerimeterVertexNewAttributes.Num() == 0 );
			PerimeterVertexNewAttributes.Emplace( MeshAttribute::VertexInstance::Tangent, 0, FVector4( NewTangent, 0.0f ) );
			PerimeterVertexNewAttributes.Emplace( MeshAttribute::VertexInstance::BinormalSign, 0, FVector4( BitangentSign ) );
		}
	};

	SMikkTSpaceInterface MikkTInterface;
	{
		MikkTInterface.m_getNumFaces = Local::MikkGetNumFaces;
		MikkTInterface.m_getNumVerticesOfFace = Local::MikkGetNumVertsOfFace;
		MikkTInterface.m_getPosition = Local::MikkGetPosition;
		MikkTInterface.m_getNormal = Local::MikkGetNormal;
		MikkTInterface.m_getTexCoord = Local::MikkGetTexCoord;

		MikkTInterface.m_setTSpaceBasic = Local::MikkSetTSpaceBasic;
		MikkTInterface.m_setTSpace = nullptr;
	}

	SMikkTSpaceContext MikkTContext;
	{
		MikkTContext.m_pInterface = &MikkTInterface;
		MikkTContext.m_pUserData = (void*)( &MikkUserData );

		// @todo mesheditor perf: Turning this on can apparently improve performance.  Needs investigation.
		MikkTContext.m_bIgnoreDegenerates = false;
	}

	// Now we'll ask MikkTSpace to actually generate the tangents
	genTangSpaceDefault( &MikkTContext );

	SetPolygonsVertexAttributes( VertexAttributesForPolygons );
}
#endif


void UEditableMesh::SetVerticesCornerSharpness( const TArray<FVertexID>& VertexIDs, const TArray<float>& VerticesNewSharpness )
{
	check( VertexIDs.Num() == VerticesNewSharpness.Num() );

	static TArray<FAttributesForVertex> AttributesForVertices;
	AttributesForVertices.Reset();

	for( int32 VertexNumber = 0; VertexNumber < VertexIDs.Num(); ++VertexNumber )
	{
		const FVertexID VertexID = VertexIDs[ VertexNumber ];

		AttributesForVertices.Emplace();
		FAttributesForVertex& AttributesForVertex = AttributesForVertices.Last();
		AttributesForVertex.VertexID = VertexID;
		AttributesForVertex.VertexAttributes.Attributes.Emplace( MeshAttribute::Vertex::CornerSharpness, 0, FMeshElementAttributeValue( VerticesNewSharpness[ VertexNumber ] ) );
	}

	SetVerticesAttributes( AttributesForVertices );
}


void UEditableMesh::SetEdgesCreaseSharpness( const TArray<FEdgeID>& EdgeIDs, const TArray<float>& EdgesNewCreaseSharpness )
{
	check( EdgeIDs.Num() == EdgesNewCreaseSharpness.Num() );

	static TArray<FAttributesForEdge> AttributesForEdges;
	AttributesForEdges.Reset();

	for( int32 EdgeNumber = 0; EdgeNumber < EdgeIDs.Num(); ++EdgeNumber )
	{
		const FEdgeID EdgeID = EdgeIDs[ EdgeNumber ];

		AttributesForEdges.Emplace();
		FAttributesForEdge& AttributesForEdge = AttributesForEdges.Last();
		AttributesForEdge.EdgeID = EdgeID;
		AttributesForEdge.EdgeAttributes.Attributes.Emplace( MeshAttribute::Edge::CreaseSharpness, 0, FMeshElementAttributeValue( EdgesNewCreaseSharpness[ EdgeNumber ] ) );
	}

	SetEdgesAttributes( AttributesForEdges );
}


void UEditableMesh::SetEdgesHardness( const TArray<FEdgeID>& EdgeIDs, const TArray<bool>& EdgesNewIsHard )
{
	check( EdgeIDs.Num() == EdgesNewIsHard.Num() );

	static TArray<FAttributesForEdge> AttributesForEdges;
	AttributesForEdges.Reset();

	static TSet<FPolygonID> UniqueConnectedPolygonIDs;
	UniqueConnectedPolygonIDs.Reset();

	for( int32 EdgeNumber = 0; EdgeNumber < EdgeIDs.Num(); ++EdgeNumber )
	{
		const FEdgeID EdgeID = EdgeIDs[ EdgeNumber ];

		AttributesForEdges.Emplace();
		FAttributesForEdge& AttributesForEdge = AttributesForEdges.Last();
		AttributesForEdge.EdgeID = EdgeID;
		AttributesForEdge.EdgeAttributes.Attributes.Emplace( MeshAttribute::Edge::IsHard, 0, FMeshElementAttributeValue( EdgesNewIsHard[ EdgeNumber ] ) );

		// Get the polygons this edge is connected to.  They'll need new normals.
		static TArray<FPolygonID> ConnectedPolygonIDs;
		GetEdgeConnectedPolygons( EdgeID, /* Out */ ConnectedPolygonIDs );
		UniqueConnectedPolygonIDs.Append( ConnectedPolygonIDs );
	}

	SetEdgesAttributes( AttributesForEdges );
}


void UEditableMesh::SetEdgesHardnessAutomatically( const TArray<FEdgeID>& EdgeIDs, const float MaxDotProductForSoftEdge )
{
	static TArray<bool> EdgesNewIsHard;
	EdgesNewIsHard.SetNumUninitialized( EdgeIDs.Num(), false );

	for( int32 EdgeNumber = 0; EdgeNumber < EdgeIDs.Num(); ++EdgeNumber )
	{
		const FEdgeID EdgeID = EdgeIDs[ EdgeNumber ];

		// Default to soft if we have no polygons attached
		bool bIsSoftEdge = true;

		const int32 ConnectedPolygonCount = GetEdgeConnectedPolygonCount( EdgeID );
		if( ConnectedPolygonCount > 0 )
		{
			float MinDot = 1.0f;

			const FPolygonID FirstPolygonID = GetEdgeConnectedPolygon( EdgeID, 0 );

			FVector LastPolygonNormal = ComputePolygonNormal( FirstPolygonID );

			for( int32 ConnectedPolygonNumber = 1; ConnectedPolygonNumber < ConnectedPolygonCount; ++ConnectedPolygonNumber )
			{
				const FPolygonID PolygonID = GetEdgeConnectedPolygon( EdgeID, ConnectedPolygonNumber );

				const FVector PolygonNormal = ComputePolygonNormal( PolygonID );

				const float Dot = FVector::DotProduct( PolygonNormal, LastPolygonNormal );
				MinDot = FMath::Min( Dot, MinDot );
			}

			bIsSoftEdge = ( MinDot >= MaxDotProductForSoftEdge );
		}

		EdgesNewIsHard[ EdgeNumber ] = !bIsSoftEdge;
	}


	// Set the edges hardness (and generate new normals)
	SetEdgesHardness( EdgeIDs, EdgesNewIsHard );
}


void UEditableMesh::SetEdgesVertices( const TArray<FVerticesForEdge>& VerticesForEdges )
{
	EM_ENTER( TEXT( "SetEdgesVertices: %s" ), *LogHelpers::ArrayToString( VerticesForEdges ) );

	FEdgeArray& Edges = GetMeshDescription()->Edges();
	FVertexArray& Vertices = GetMeshDescription()->Vertices();

	FSetEdgesVerticesChangeInput RevertInput;
	RevertInput.VerticesForEdges.AddUninitialized( VerticesForEdges.Num() );

	static TArray<FEdgeID> EdgeIDs;
	EdgeIDs.SetNumUninitialized( VerticesForEdges.Num() );

	for( int32 EdgeNumber = 0; EdgeNumber < VerticesForEdges.Num(); ++EdgeNumber )
	{
		const FVerticesForEdge& VerticesForEdge = VerticesForEdges[ EdgeNumber ];

		// Save the backup
		FVerticesForEdge& RevertVerticesForEdge = RevertInput.VerticesForEdges[ EdgeNumber ];
		RevertVerticesForEdge.EdgeID = VerticesForEdge.EdgeID;
		GetEdgeVertices( VerticesForEdge.EdgeID, /* Out */ RevertVerticesForEdge.NewVertexID0, /* Out */ RevertVerticesForEdge.NewVertexID1 );

		// Edit the edge
		FMeshEdge& Edge = Edges[ VerticesForEdge.EdgeID ];
		EdgeIDs[ EdgeNumber ] = VerticesForEdge.EdgeID;

		for( uint32 EdgeVertexNumber = 0; EdgeVertexNumber < 2; ++EdgeVertexNumber )
		{
			// Disconnect the edge from its existing vertices
			const FVertexID VertexID = Edge.VertexIDs[ EdgeVertexNumber ];
			FMeshVertex& Vertex = Vertices[ VertexID ];
			verify( Vertex.ConnectedEdgeIDs.RemoveSingleSwap( VerticesForEdge.EdgeID ) == 1 );	// Must have been already connected!
		}

		Edge.VertexIDs[0] = VerticesForEdge.NewVertexID0;
		Edge.VertexIDs[1] = VerticesForEdge.NewVertexID1;

		// Connect the new vertices to the edge
		for( uint32 EdgeVertexNumber = 0; EdgeVertexNumber < 2; ++EdgeVertexNumber )
		{
			const FVertexID VertexID = Edge.VertexIDs[ EdgeVertexNumber ];
			FMeshVertex& Vertex = Vertices[ VertexID ];

			check( !Vertex.ConnectedEdgeIDs.Contains( VerticesForEdge.EdgeID ) );	// Should not have already been connected
			Vertex.ConnectedEdgeIDs.Add( VerticesForEdge.EdgeID );
		}
	}

	// Give the adapter a chance to handle this
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnSetEdgesVertices( this, EdgeIDs );
	}

	AddUndo( MakeUnique<FSetEdgesVerticesChange>( MoveTemp( RevertInput ) ) );

	EM_EXIT( TEXT( "SetEdgesVertices finished") );
}


void UEditableMesh::InsertPolygonPerimeterVertices( const FPolygonID PolygonID, const int32 InsertBeforeVertexNumber, const TArray<FVertexAndAttributes>& VerticesToInsert )
{
	EM_ENTER( TEXT( "InsertPolygonPerimeterVertices: PolygonID:%s InsertBeforeVertexNumber:%d %s" ), *PolygonID.ToString(), InsertBeforeVertexNumber, *LogHelpers::ArrayToString( VerticesToInsert ) );
	// @todo mesheditor: see if we want to keep this action. It is only used by SplitEdge, and doesn't generate missing edges.
	// We can achieve the same thing by deleting a polygon without deleting vertex instances, and creating a new polygon with an extra vertex instance.

	FMeshPolygon& Polygon = GetMeshDescription()->GetPolygon( PolygonID );
	FVertexInstanceArray& VertexInstances = GetMeshDescription()->VertexInstances();

	// @todo mesheditor: should create missing edges here too?

	// Insert new vertex instances
	for( int32 InsertVertexIter = 0; InsertVertexIter < VerticesToInsert.Num(); ++InsertVertexIter )
	{
		const FVertexAndAttributes& VertexToInsert = VerticesToInsert[ InsertVertexIter ];

		const FVertexInstanceID VertexInstanceID = CreateVertexInstanceForContourVertex( VertexToInsert, PolygonID );
		Polygon.PerimeterContour.VertexInstanceIDs.Insert( VertexInstanceID, InsertBeforeVertexNumber + InsertVertexIter );

		// Connect polygon to vertex instance
		FMeshVertexInstance& VertexInstance = VertexInstances[ VertexInstanceID ];
		check( !VertexInstance.ConnectedPolygons.Contains( PolygonID ) );
		VertexInstance.ConnectedPolygons.Add( PolygonID );
	}

	// Back up insert
	{
		FRemovePolygonPerimeterVerticesChangeInput RevertInput;

		RevertInput.PolygonID = PolygonID;
		RevertInput.FirstVertexNumberToRemove = InsertBeforeVertexNumber;
		RevertInput.NumVerticesToRemove = VerticesToInsert.Num();
		RevertInput.bDeleteOrphanedVertexInstances = false;

		AddUndo( MakeUnique<FRemovePolygonPerimeterVerticesChange>( MoveTemp( RevertInput ) ) );
	}

	PolygonsPendingTriangulation.Add( PolygonID );
	PolygonsPendingNewTangentBasis.Add( PolygonID );	// @todo mesheditor: and other vertex connected polygons too?

	// Update spatial database
	if( Octree.IsValid() )
	{
		// Only mark the polygon to be removed from the octree if it was already in it to begin with
		bool bAlreadyInSet = false;
		NewOctreePolygonIDs.Add( PolygonID, /* Out */ &bAlreadyInSet );
		const bool bIsAlreadyInOctree = !bAlreadyInSet;
		if( bIsAlreadyInOctree )
		{
			DeletedOctreePolygonIDs.Add( PolygonID );
		}
	}

	EM_EXIT( TEXT( "InsertPolygonPerimeterVertices finished" ) );
}


void UEditableMesh::RemovePolygonPerimeterVertices( const FPolygonID PolygonID, const int32 FirstVertexNumberToRemove, const int32 NumVerticesToRemove, const bool bDeleteOrphanedVertexInstances )
{
	EM_ENTER( TEXT( "RemovePolygonPerimeterVertices: PolygonID:%s FirstVertexNumberToRemove:%d NumVerticesToRemove:%d" ), *PolygonID.ToString(), FirstVertexNumberToRemove, NumVerticesToRemove );
	FMeshPolygon& Polygon = GetMeshDescription()->GetPolygon( PolygonID );

	// Back up 
	{
		FInsertPolygonPerimeterVerticesChangeInput RevertInput;

		RevertInput.PolygonID = PolygonID;
		RevertInput.InsertBeforeVertexNumber = FirstVertexNumberToRemove;

		RevertInput.VerticesToInsert.SetNum( NumVerticesToRemove, false );
		for( int32 VertexToRemoveIter = 0; VertexToRemoveIter < NumVerticesToRemove; ++VertexToRemoveIter )
		{
			FVertexAndAttributes& RevertVertexToInsert = RevertInput.VerticesToInsert[ VertexToRemoveIter ];
			RevertVertexToInsert.VertexInstanceID = Polygon.PerimeterContour.VertexInstanceIDs[ FirstVertexNumberToRemove + VertexToRemoveIter ];
			RevertVertexToInsert.VertexID = FVertexID::Invalid;
		}

		AddUndo( MakeUnique<FInsertPolygonPerimeterVerticesChange>( MoveTemp( RevertInput ) ) );
	}

	{
		static TArray<FVertexInstanceID> OrphanedVertexInstanceIDs;
		OrphanedVertexInstanceIDs.Reset();

		FVertexInstanceArray& VertexInstances = GetMeshDescription()->VertexInstances();

		// Delete them backwards so it is done in the opposite order to Insert
		for( int32 Index = NumVerticesToRemove - 1; Index >= 0; --Index )
		{
			const int32 VertexNumber = FirstVertexNumberToRemove + Index;
			const FVertexInstanceID ContourVertexInstanceID = Polygon.PerimeterContour.VertexInstanceIDs[ VertexNumber ];

			// Disconnect the polygon from the vertex instance
			FMeshVertexInstance& VertexInstance = VertexInstances[ ContourVertexInstanceID ];
			verify( VertexInstance.ConnectedPolygons.Remove( PolygonID ) == 1 );

			// If the vertex instance is now orphaned, add it to the list
			if( bDeleteOrphanedVertexInstances && VertexInstance.ConnectedPolygons.Num() == 0 )
			{
				OrphanedVertexInstanceIDs.Add( ContourVertexInstanceID );
			}
		}

		Polygon.PerimeterContour.VertexInstanceIDs.RemoveAt( FirstVertexNumberToRemove, NumVerticesToRemove );

		if( OrphanedVertexInstanceIDs.Num() > 0 )
		{
			DeleteVertexInstances( OrphanedVertexInstanceIDs, false );
		}
	}

	PolygonsPendingTriangulation.Add( PolygonID );
	PolygonsPendingNewTangentBasis.Add( PolygonID );	// @todo mesheditor: and other vertex connected polygons too?

	// Update spatial database
	if( Octree.IsValid() )
	{
		// Only mark the polygon to be removed from the octree if it was already in it to begin with
		bool bAlreadyInSet = false;
		NewOctreePolygonIDs.Add( PolygonID, /* Out */ &bAlreadyInSet );
		const bool bIsAlreadyInOctree = !bAlreadyInSet;
		if( bIsAlreadyInOctree )
		{
			DeletedOctreePolygonIDs.Add( PolygonID );
		}
	}

	EM_EXIT( TEXT( "RemovePolygonPerimeterVertices finished" ) );
}


int32 UEditableMesh::FindPolygonPerimeterVertexNumberForVertex( const FPolygonID PolygonID, const FVertexID VertexID ) const
{
	int32 FoundPolygonVertexNumber = INDEX_NONE;

	const int32 PolygonPerimeterVertexCount = this->GetPolygonPerimeterVertexCount( PolygonID );
	for( int32 PolygonVertexNumber = 0; PolygonVertexNumber < PolygonPerimeterVertexCount; ++PolygonVertexNumber )
	{
		if( VertexID == this->GetPolygonPerimeterVertex( PolygonID, PolygonVertexNumber ) )
		{
			FoundPolygonVertexNumber = PolygonVertexNumber;
			break;
		}
	}

	return FoundPolygonVertexNumber;
}


int32 UEditableMesh::FindPolygonPerimeterEdgeNumberForVertices( const FPolygonID PolygonID, const FVertexID EdgeVertexID0, const FVertexID EdgeVertexID1 ) const
{
	// @todo mesheditor: surely quicker just to iterate round the perimeter, checking pairs of adjacent vertices against the passed IDs (in both orders)?

	int32 FoundPolygonEdgeNumber = INDEX_NONE;

	static TArray<FEdgeID> EdgeIDs;
	GetPolygonPerimeterEdges( PolygonID, /* Out */ EdgeIDs );

	for( int32 PolygonEdgeNumber = 0; PolygonEdgeNumber < EdgeIDs.Num(); ++PolygonEdgeNumber )
	{
		const FEdgeID EdgeID = EdgeIDs[ PolygonEdgeNumber ];

		FVertexID TestEdgeVertexIDs[ 2 ];
		GetEdgeVertices( EdgeID, /* Out */ TestEdgeVertexIDs[0], /* Out */ TestEdgeVertexIDs[1] );

		if( ( TestEdgeVertexIDs[ 0 ] == EdgeVertexID0 && TestEdgeVertexIDs[ 1 ] == EdgeVertexID1 ) ||
			( TestEdgeVertexIDs[ 1 ] == EdgeVertexID0 && TestEdgeVertexIDs[ 0 ] == EdgeVertexID1 ) )
		{
			FoundPolygonEdgeNumber = PolygonEdgeNumber;
			break;
		}
	}

	return FoundPolygonEdgeNumber;
}


void UEditableMesh::FlipPolygons( const TArray<FPolygonID>& PolygonIDs )
{
	EM_ENTER( TEXT( "FlipPolygons %s" ), *LogHelpers::ArrayToString( PolygonIDs ) );

	FFlipPolygonsChangeInput RevertInput;
	RevertInput.PolygonIDsToFlip = PolygonIDs;
	AddUndo( MakeUnique<FFlipPolygonsChange>( MoveTemp( RevertInput ) ) );

	for (const FPolygonID& PolygonID : PolygonIDs)
	{
		GetMeshDescription()->ReversePolygonFacing(PolygonID);
	}

	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnRetriangulatePolygons( this, PolygonIDs );
	}

	PolygonsPendingFlipTangentBasis.Append(PolygonIDs);

	EM_EXIT( TEXT( "FlipPolygons returned" ) );
}


void UEditableMesh::TriangulatePolygons( const TArray<FPolygonID>& PolygonIDs, TArray<FPolygonID>& OutNewTrianglePolygons )
{
	EM_ENTER( TEXT( "TriangulatePolygons: %s" ), *LogHelpers::ArrayToString( PolygonIDs ) );

	OutNewTrianglePolygons.Reset();

	static TArray<FPolygonToCreate> PolygonsToCreate;
	PolygonsToCreate.Reset();

	static TArray<FPolygonID> PolygonsToDelete;
	PolygonsToDelete.Reset();

	for( const FPolygonID PolygonID : PolygonIDs )
	{
		// Skip right over polygons with fewer than four vertices
		const int32 PerimeterVertexCount = GetPolygonPerimeterVertexCount( PolygonID );
		if( PerimeterVertexCount > 3 )
		{
			// We'll be replacing this polygon with it's triangulated counterpart polygons
			PolygonsToDelete.Add( PolygonID );

			// Figure out the triangulation for this polygon
			static TArray<FMeshTriangle> Triangles;
			ComputePolygonTriangulation( PolygonID, /* Out */ Triangles );

			// Build polygons for each of the triangles that made up the original
			{
				for( const FMeshTriangle& Triangle : Triangles )
				{
					PolygonsToCreate.Emplace();
					FPolygonToCreate& PolygonToCreate = PolygonsToCreate.Last();
					
					PolygonToCreate.PolygonGroupID = GetGroupForPolygon( PolygonID );
					PolygonToCreate.PolygonEdgeHardness = EPolygonEdgeHardness::NewEdgesSoft;

					for( int32 TriangleVertexNumber = 0; TriangleVertexNumber < 3; ++TriangleVertexNumber )
					{
						PolygonToCreate.PerimeterVertices.Emplace();
						FVertexAndAttributes& PerimeterVertex = PolygonToCreate.PerimeterVertices.Last();

						PerimeterVertex.VertexInstanceID = Triangle.GetVertexInstanceID( TriangleVertexNumber );
						PerimeterVertex.VertexID = FVertexID::Invalid;
					}
				}
			}
		}
	}

	// Delete the original polygons, but don't erase any orphaned edges or vertices, because we're about to put in
	// triangles to replace those polygons.  Also, we won't touch polygons that we didn't have to triangulate!
	{
		const bool bDeleteOrphanEdges = false;
		const bool bDeleteOrphanVertices = false;
		const bool bDeleteOrphanVertexInstances = false;
		const bool bDeleteEmptyPolygonGroups = false;
		DeletePolygons( PolygonsToDelete, bDeleteOrphanEdges, bDeleteOrphanVertices, bDeleteOrphanVertexInstances, bDeleteEmptyPolygonGroups );
	}

	// Create the new polygons.  One for each triangle.  Note that new edges will be created here too on the inside of
	// the original polygon to border the triangles.
	{
		static TArray<FEdgeID> NewEdgeIDs;
		CreatePolygons( PolygonsToCreate, /* Out */ OutNewTrianglePolygons, /* Out */ NewEdgeIDs );
	}

	EM_EXIT( TEXT( "TriangulatePolygons returned %s" ), *LogHelpers::ArrayToString( OutNewTrianglePolygons ) );
}


void UEditableMesh::AssignPolygonsToPolygonGroups( const TArray<FPolygonGroupForPolygon>& PolygonGroupForPolygons, const bool bDeleteOrphanedPolygonGroups )
{
	EM_ENTER( TEXT( "AssignPolygonsToPolygonGroups: %s" ), *LogHelpers::ArrayToString( PolygonGroupForPolygons ) );
	// Back up
	{
		FAssignPolygonsToPolygonGroupChangeInput RevertInput;
		RevertInput.PolygonGroupForPolygons.Reserve( PolygonGroupForPolygons.Num() );
		for( int32 Index = PolygonGroupForPolygons.Num() - 1; Index >= 0; --Index )
		{
			RevertInput.PolygonGroupForPolygons.Emplace();
			FPolygonGroupForPolygon& RevertPolygonGroupForPolygon = RevertInput.PolygonGroupForPolygons.Last();
			RevertPolygonGroupForPolygon.PolygonID = PolygonGroupForPolygons[ Index ].PolygonID;
			RevertPolygonGroupForPolygon.PolygonGroupID = GetGroupForPolygon( PolygonGroupForPolygons[ Index ].PolygonID );
		}

		AddUndo( MakeUnique<FAssignPolygonsToPolygonGroupChange>( MoveTemp( RevertInput ) ) );
	}

	// Let the adapters handle this
	for( UEditableMeshAdapter* Adapter : Adapters )
	{
		Adapter->OnAssignPolygonsToPolygonGroups( this, PolygonGroupForPolygons );
	}

	// Maintain an array of orphaned polygon groups
	static TArray<FPolygonGroupID> PolygonGroupsToDelete;
	PolygonGroupsToDelete.Reset();

	FPolygonArray& Polygons = GetMeshDescription()->Polygons();
	FPolygonGroupArray& PolygonGroups = GetMeshDescription()->PolygonGroups();

	for( const FPolygonGroupForPolygon& PolygonGroupForPolygon : PolygonGroupForPolygons )
	{
		const FPolygonID PolygonID = PolygonGroupForPolygon.PolygonID;
		const FPolygonGroupID NewPolygonGroupID = PolygonGroupForPolygon.PolygonGroupID;

		FMeshPolygon& Polygon = Polygons[ PolygonID ];
		const FPolygonGroupID OldPolygonGroupID = Polygon.PolygonGroupID;

		// Remove polygon reference from old group
		FMeshPolygonGroup& OldPolygonGroup = PolygonGroups[ OldPolygonGroupID ];
		verify( OldPolygonGroup.Polygons.RemoveSwap( PolygonID ) == 1 );

		// If old group is now empty (and we're deleting orphans), add it to the list to delete
		if( bDeleteOrphanedPolygonGroups && OldPolygonGroup.Polygons.Num() == 0 )
		{
			PolygonGroupsToDelete.Add( OldPolygonGroupID );
		}

		// Add polygon reference to new group
		FMeshPolygonGroup& NewPolygonGroup = PolygonGroups[ NewPolygonGroupID ];
		check( !NewPolygonGroup.Polygons.Contains( PolygonID ) );
		NewPolygonGroup.Polygons.Add( PolygonID );

		// Change polygon group
		Polygon.PolygonGroupID = NewPolygonGroupID;
	}

	// If there's any groups to delete, do it now
	if( PolygonGroupsToDelete.Num() > 0 )
	{
		DeletePolygonGroups( PolygonGroupsToDelete );
	}

	EM_EXIT( TEXT( "AssignPolygonsToPolygonGroups returned" ) );
}


void UEditableMesh::WeldVertices( const TArray<FVertexID>& VertexIDsToWeld, FVertexID& OutNewVertexID )
{
	EM_ENTER( TEXT( "WeldVertices: %s" ), *LogHelpers::ArrayToString( VertexIDsToWeld ) );

	OutNewVertexID = FVertexID::Invalid;

	// This function takes a list of perimeter vertices and a list of vertices to be welded as input.
	// It returns a tuple stating whether the result is valid, and the [first, last) range of vertices to be welded.
	// (It will be invalid if there is more than one contiguous run of vertices to weld.)
	auto GetPerimeterVertexRangeToWeld = []( const TArray<FVertexID>& PolygonVertexIDs, const TArray<FVertexID>& VerticesToWeld )
	{
		bool bValid = true;
		int32 StartIndex = INDEX_NONE;
		int32 EndIndex = INDEX_NONE;

		const int32 NumPolygonVertices = PolygonVertexIDs.Num();
		bool bPrevVertexNeedsWelding = VerticesToWeld.Contains( PolygonVertexIDs[ NumPolygonVertices - 1 ] );
		for( int32 Index = 0; Index < NumPolygonVertices; ++Index )
		{
			const bool bThisVertexNeedsWelding = VerticesToWeld.Contains( PolygonVertexIDs[ Index ] );
			if( !bPrevVertexNeedsWelding && bThisVertexNeedsWelding )
			{
				// Transition from 'doesn't need welding' to 'needs welding'
				if( StartIndex == INDEX_NONE )
				{
					StartIndex = Index;
				}
				else
				{
					// If this is not the first time we've seen this transition, there is more than one contiguous run of vertices
					// which need welding, which is not allowed.
					bValid = false;
				}
			}

			if( bPrevVertexNeedsWelding && !bThisVertexNeedsWelding )
			{
				// Transition from 'needs welding' to 'doesn't need welding'
				if( EndIndex == INDEX_NONE )
				{
					EndIndex = Index;
				}
				else
				{
					bValid = false;
				}
			}

			bPrevVertexNeedsWelding = bThisVertexNeedsWelding;
		}

		// If the indices are not set, either there were no vertices to weld, or they were all to be welded.
		// In the latter case, initialize the full vertex range.
		if( StartIndex == INDEX_NONE && EndIndex == INDEX_NONE && bPrevVertexNeedsWelding )
		{
			StartIndex = 0;
			EndIndex = NumPolygonVertices;
		}

		// Get the size of the range.
		// The array is circular, so it's possible for the EndIndex to be smaller than the start index (and compensate for that accordingly)
		const int32 RangeSize = ( EndIndex - StartIndex ) + ( ( EndIndex < StartIndex ) ? NumPolygonVertices : 0 );

		// If, after welding perimeter vertices, we have fewer than three vertices left, this poly just disappears.
		// (+ 1 below for the new vertex which replaces the welded range)
		bool bWouldBeDegenerate = ( NumPolygonVertices - RangeSize + 1 < 3 );

		return MakeTuple( bValid, bWouldBeDegenerate, StartIndex, EndIndex );
	};

	// Build a list of all polygons which contain at least one of the vertices to be welded
	static TArray<FPolygonID> AllConnectedPolygonIDs;
	{
		AllConnectedPolygonIDs.Reset();

		for( FVertexID VertexID : VertexIDsToWeld )
		{
			static TArray<FPolygonID> ConnectedPolygonIDs;
			GetVertexConnectedPolygons( VertexID, ConnectedPolygonIDs );

			for( FPolygonID PolygonID : ConnectedPolygonIDs )
			{
				AllConnectedPolygonIDs.AddUnique( PolygonID );
			}
		}
	}

	// Check whether the operation is valid. We can't weld vertices if there are any polygons which have non-contiguous
	// vertices on the perimeter which are marked to be welded
	bool bNeedToCreateWeldedVertex = false;
	for( FPolygonID ConnectedPolygonID : AllConnectedPolygonIDs )
	{
		static TArray<FVertexID> PolygonVertexIDs;
		GetPolygonPerimeterVertices( ConnectedPolygonID, PolygonVertexIDs );

		auto VertexRangeToWeld = GetPerimeterVertexRangeToWeld( PolygonVertexIDs, VertexIDsToWeld );
		const bool bIsValid = VertexRangeToWeld.Get<0>();
		const bool bWouldBeDegenerate = VertexRangeToWeld.Get<1>();

		// If the resulting poly is valid (has 3 or more verts), we know we need to create the welded vertex
		if( !bWouldBeDegenerate )
		{
			bNeedToCreateWeldedVertex = true;
		}

		// If the result is invalid (because it would cause a poly to be welded in more than one place on its perimeter), abort now
		if( !bIsValid )
		{
			// Return with the NewVertexID set to Invalid
			EM_EXIT( TEXT( "WeldVertices failed" ) );
			return;
		}
	}

	if( !bNeedToCreateWeldedVertex )
	{
		// For now, abort if we don't need to create a welded vertex.
		// This generally implies that all (or a disconnected subset) of the mesh is about to disappear,
		// which arguably is not something we would want to do like this anyway.
		EM_EXIT( TEXT( "WeldVertices failed" ) );
		return;
	}

	const TVertexAttributesRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	// Create new welded vertex
	static TArray<FVertexID> NewVertices;
	{
		static TArray<FVertexToCreate> VerticesToCreate;
		VerticesToCreate.SetNum( 1 );
		FVertexToCreate& VertexToCreate = VerticesToCreate.Last();

		const FVector NewVertexPosition = VertexPositions[ VertexIDsToWeld.Last() ];

		// The vertex which is created will be at the position of the last vertex in the array of vertices to weld.
		// @todo mesheditor: maybe specify a position to the method instead of using one of the existing vertices?
		VertexToCreate.VertexAttributes.Attributes.Emplace( MeshAttribute::Vertex::Position, 0, FMeshElementAttributeValue( NewVertexPosition ) );

		// @todo mesheditor: vertex corner sharpness too?

		CreateVertices( VerticesToCreate, NewVertices );
	}


	static TArray<FPolygonToCreate> PolygonsToCreate;
	PolygonsToCreate.Reset( AllConnectedPolygonIDs.Num() );

	static TArray<FAttributesForEdge> AttributesForEdges;
	AttributesForEdges.Reset();

	// Now for each polygon, merge runs of consecutive vertices
	for( FPolygonID ConnectedPolygonID : AllConnectedPolygonIDs )
	{
		const int32 NumPerimeterVertices = GetPolygonPerimeterVertexCount( ConnectedPolygonID );

		// Get perimeter vertices and edges for this polygon
		static TArray<FVertexID> PolygonVertexIDs;
		static TArray<FEdgeID> PolygonEdgeIDs;
		GetPolygonPerimeterVertices( ConnectedPolygonID, PolygonVertexIDs );
		GetPolygonPerimeterEdges( ConnectedPolygonID, PolygonEdgeIDs );
		const TArray<FVertexInstanceID>& PolygonVertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( ConnectedPolygonID );

		// Get the index range of perimeter vertices to be welded.
		// This should definitely be valid, as any invalid welded poly will have caused early exit, above.
		auto VertexRangeToWeld = GetPerimeterVertexRangeToWeld( PolygonVertexIDs, VertexIDsToWeld );
		const bool bIsValid = VertexRangeToWeld.Get<0>();
		const bool bWouldBeDegenerate = VertexRangeToWeld.Get<1>();
		const int32 StartIndex = VertexRangeToWeld.Get<2>();
		const int32 EndIndex = VertexRangeToWeld.Get<3>();
		check( bIsValid );

		if( bWouldBeDegenerate )
		{
			continue;
		}

		// Prepare to create a new polygon
		PolygonsToCreate.Emplace();
		FPolygonToCreate& PolygonToCreate = PolygonsToCreate.Last();

		PolygonToCreate.PolygonGroupID = GetGroupForPolygon( ConnectedPolygonID );

		// Iterate through perimeter vertices starting at index 0.
		// We skip through the run of welded vertices, replacing them with a single welded vertex.
		// We need to check whether we are starting in the middle of a run (if EndIndex < StartIndex).
		bool bInsideWeldedRange = ( EndIndex < StartIndex );
		for( int32 Index = 0; Index < NumPerimeterVertices; ++Index )
		{
			if( bInsideWeldedRange )
			{
				if( Index == EndIndex )
				{
					// EndIndex is range exclusive, so we now need to process this vertex.
					bInsideWeldedRange = false;
				}
				else
				{
					// Otherwise still inside the welded range; skip the remaining vertices in the range.
					continue;
				}
			}

			// Add new perimeter vertex in the polygon to create
			PolygonToCreate.PerimeterVertices.Emplace();
			FVertexAndAttributes& VertexAndAttributes = PolygonToCreate.PerimeterVertices.Last();

			if( Index == StartIndex )
			{
				// If this is the first vertex in the run of vertices to weld, replace the ID with the newly created welded vertex
				VertexAndAttributes.VertexID = NewVertices[ 0 ];
				bInsideWeldedRange = true;
			}
			else
			{
				// Otherwise use the original Vertex ID
				VertexAndAttributes.VertexID = PolygonVertexIDs[ Index ];
			}

			// Copy the polygon vertex attributes over
			BackupAllAttributes( VertexAndAttributes.PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(), PolygonVertexInstanceIDs[ Index ] );

			// Prepare to assign the old edge's attributes to the new edge.
			// We build up an array of edge attributes to set, in perimeter vertex order for each polygon.
			AttributesForEdges.Emplace();
			FAttributesForEdge& AttributesForEdge = AttributesForEdges.Last();
			BackupAllAttributes( AttributesForEdge.EdgeAttributes, GetMeshDescription()->EdgeAttributes(), PolygonEdgeIDs[ Index ] );
		}
	}

	// Create polygons
	static TArray<FPolygonID> NewPolygonIDs;
	{
		static TArray<FEdgeID> NewEdgeIDs;
		CreatePolygons( PolygonsToCreate, NewPolygonIDs, NewEdgeIDs );
	}

	// Set new edge attributes.
	// Now that we have a list of newly created polygon IDs, we need to go through the attributes for edge list, filling in the new Edge ID.
	// This relies on the fact that the NewPolygonIDs array lists the polygons in the same order as they were defined in PolygonsToCreate,
	// and that the edges are strictly ordered from perimeter vertex 0.
	{
		int32 AttributesForEdgeIndex = 0;
		for( FPolygonID NewPolygonID : NewPolygonIDs )
		{
			static TArray<FEdgeID> NewPolygonEdgeIDs;
			GetPolygonPerimeterEdges( NewPolygonID, NewPolygonEdgeIDs );

			for( FEdgeID NewPolygonEdgeID : NewPolygonEdgeIDs )
			{
				AttributesForEdges[ AttributesForEdgeIndex ].EdgeID = NewPolygonEdgeID;
				AttributesForEdgeIndex++;
			}
		}
		check( AttributesForEdgeIndex == AttributesForEdges.Num() );
		SetEdgesAttributes( AttributesForEdges );
	}

	// Delete old polygons, removing any orphaned edges and vertices at the same time
	{
		const bool bDeleteOrphanedEdges = true;
		const bool bDeleteOrphanedVertices = true;
		const bool bDeleteOrphanedVertexInstances = true;
		const bool bDeleteEmptyPolygonGroups = false;
		DeletePolygons( AllConnectedPolygonIDs, bDeleteOrphanedEdges, bDeleteOrphanedVertices, bDeleteOrphanedVertexInstances, bDeleteEmptyPolygonGroups );
	}

	EM_EXIT( TEXT( "WeldVertices returned %s" ), *OutNewVertexID.ToString() );
}


void UEditableMesh::TessellatePolygons( const TArray<FPolygonID>& PolygonIDs, const ETriangleTessellationMode TriangleTessellationMode, TArray<FPolygonID>& OutNewPolygonIDs )
{
	EM_ENTER( TEXT( "TessellatePolygons: %s" ), *LogHelpers::ArrayToString( PolygonIDs ) );

	OutNewPolygonIDs.Reset();

	const TVertexAttributesRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

	//
	// Simple tessellation algorithm:
	//
	//   - Triangles will be split into either three or four triangles depending on the 'Mode' argument.
	//			-> ThreeTriangles: Connect each vertex to a new center vertex, forming three triangles
	//			-> FourTriangles: Split each edge and create a center polygon that connects those new vertices, then three additional polygons for each original corner
	//
	//   - Everything else will be split into quads by creating a new vertex in the center, then adding a new vertex to 
	//       each original perimeter edge and connecting each original vertex to it's new neighbors and the center
	//
	// NOTE: Concave polygons will yield bad results
	//

	// Create a new vertex in the center of each incoming polygon
	static TArray<FVertexID> PolygonCenterVertices;
	PolygonCenterVertices.Reset();
	{
		static TArray<FVertexToCreate> VerticesToCreate;
		VerticesToCreate.Reset();
		for( const FPolygonID PolygonID : PolygonIDs )
		{
			const int32 PerimeterEdgeCount = GetPolygonPerimeterEdgeCount( PolygonID );
			if( TriangleTessellationMode == ETriangleTessellationMode::ThreeTriangles || PerimeterEdgeCount > 3 )
			{
				// Find the center of this polygon
				const FVector PolygonCenter = ComputePolygonCenter( PolygonID );

				FVertexToCreate& VertexToCreate = *new( VerticesToCreate ) FVertexToCreate();
				VertexToCreate.VertexAttributes.Attributes.Emplace( MeshAttribute::Vertex::Position, 0, FMeshElementAttributeValue( PolygonCenter ) );
			}
		}

		this->CreateVertices( VerticesToCreate, /* Out */ PolygonCenterVertices );
	}


	// Split all of the edges of the original polygons (except triangles).  Remember, some edges may be shared between
	// the incoming polygons so we'll keep track of that and make sure not to split them again.  
	{
		static TSet<FEdgeID> EdgesToSplit;
		EdgesToSplit.Reset();

		for( int32 PolygonNumber = 0; PolygonNumber < PolygonIDs.Num(); ++PolygonNumber )
		{
			const FPolygonID PolygonID = PolygonIDs[ PolygonNumber ];

			static TArray<FEdgeID> PerimeterEdgeIDs;
			GetPolygonPerimeterEdges( PolygonID, /* Out */ PerimeterEdgeIDs );
			
			if( TriangleTessellationMode == ETriangleTessellationMode::FourTriangles || PerimeterEdgeIDs.Num() > 3 )
			{
				for( const FEdgeID PerimeterEdgeID : PerimeterEdgeIDs )
				{
					EdgesToSplit.Add( PerimeterEdgeID );
				}
			}
		}

		for( const FEdgeID EdgeID : EdgesToSplit )
		{
			// Split the edge
			static TArray<float> Splits;
			Splits.SetNumUninitialized( 1 );
			Splits[ 0 ] = 0.5f;

			static TArray<FVertexID> NewVertexIDsFromSplit;
			this->SplitEdge( EdgeID, Splits, /* Out */ NewVertexIDsFromSplit );
			check( NewVertexIDsFromSplit.Num() == 1 );
		}
	}


	// We'll now define the new polygons to be created.
	static TArray<FPolygonToCreate> PolygonsToCreate;
	PolygonsToCreate.Reset();

	int32 PolygonWithNewCenterVertexNumber = 0;
	for( int32 PolygonNumber = 0; PolygonNumber < PolygonIDs.Num(); ++PolygonNumber )
	{
		const FPolygonID PolygonID = PolygonIDs[ PolygonNumber ];
		const FPolygonGroupID PolygonGroupID = GetGroupForPolygon( PolygonID );

		const int32 PerimeterEdgeCount = GetPolygonPerimeterEdgeCount( PolygonID );

		FVertexID PolygonCenterVertexID = FVertexID::Invalid;
		if( TriangleTessellationMode == ETriangleTessellationMode::ThreeTriangles || PerimeterEdgeCount > 6 )
		{
			PolygonCenterVertexID = PolygonCenterVertices[ PolygonWithNewCenterVertexNumber++ ];
		}

		// Don't bother with triangles, because we'll simply connect the original three vertices to a new
		// center position to tessellate those.
		if( PerimeterEdgeCount > 6 )
		{
			static TArray<FVertexID> PerimeterVertexIDs;
			GetPolygonPerimeterVertices( PolygonID, /* Out */ PerimeterVertexIDs );

			const TArray<FVertexInstanceID>& PerimeterVertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID );

			const int32 PerimeterVertexCount = PerimeterEdgeCount;
			const int32 OriginalPerimeterEdgeCount = PerimeterEdgeCount / 2;
			for( int32 OriginalPerimeterEdgeNumber = 0; OriginalPerimeterEdgeNumber < OriginalPerimeterEdgeCount; ++OriginalPerimeterEdgeNumber )
			{
				const int32 CurrentVertexNumber = OriginalPerimeterEdgeNumber * 2;
				const int32 PreviousVertexNumber = ( ( CurrentVertexNumber - 1 ) + PerimeterVertexCount ) % PerimeterVertexCount;
				const int32 NextVertexNumber = ( CurrentVertexNumber + 1 ) % PerimeterVertexCount;

				FPolygonToCreate& PolygonToCreate = *new( PolygonsToCreate ) FPolygonToCreate();
				PolygonToCreate.PolygonGroupID = PolygonGroupID;

				for( int32 QuadVertexNumber = 0; QuadVertexNumber < 4; ++QuadVertexNumber )
				{
					PolygonToCreate.PerimeterVertices.Emplace();
					FVertexAndAttributes& VertexAndAttributes = PolygonToCreate.PerimeterVertices.Last();

					int32 PerimeterVertexNumber = INDEX_NONE;
					switch( QuadVertexNumber )
					{
						case 0:
							PerimeterVertexNumber = PreviousVertexNumber;
							break;

						case 1:
							PerimeterVertexNumber = CurrentVertexNumber;
							break;

						case 2:
							PerimeterVertexNumber = NextVertexNumber;
							break;

						case 3:
							PerimeterVertexNumber = INDEX_NONE;	// The center vertex!
							break;

						default:
							check( 0 );
					}


					if( PerimeterVertexNumber == INDEX_NONE )
					{
						VertexAndAttributes.VertexID = PolygonCenterVertexID;

						// Generate interpolated UVs and vertex colors for the new vertex in the center
						{
							const FVector CenterVertexPosition = VertexPositions[ PolygonCenterVertexID ];

							FMeshTriangle Triangle;
							FVector TriangleVertexWeights;
							if( ComputeBarycentricWeightForPointOnPolygon( PolygonID, CenterVertexPosition, /* Out */ Triangle, /* Out */ TriangleVertexWeights ) )
							{
								InterpAllAttributes( VertexAndAttributes.PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(),
									Triangle.VertexInstanceID0, Triangle.VertexInstanceID1, Triangle.VertexInstanceID2, TriangleVertexWeights );
							}
						}
					}
					else
					{
						// Reuse the original vertex instance
						VertexAndAttributes.VertexInstanceID = PerimeterVertexInstanceIDs[ PerimeterVertexNumber ];
					}
				}
			}
		}
		else
		{
			if( TriangleTessellationMode == ETriangleTessellationMode::ThreeTriangles )
			{
				const TArray<FVertexInstanceID>& PerimeterVertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID );
				check( PerimeterVertexInstanceIDs.Num() == 3 );

				// Define the three new triangles for the original tessellated triangle
				for( int32 PerimeterVertexNumber = 0; PerimeterVertexNumber < 3; ++PerimeterVertexNumber )
				{
					const FVertexInstanceID VertexInstanceID0 = PerimeterVertexInstanceIDs[ PerimeterVertexNumber ];
					const FVertexInstanceID VertexInstanceID1 = PerimeterVertexInstanceIDs[ ( PerimeterVertexNumber + 1 ) % 3 ];

					PolygonsToCreate.Emplace();
					FPolygonToCreate& PolygonToCreate = PolygonsToCreate.Last();

					PolygonToCreate.PolygonGroupID = PolygonGroupID;
					PolygonToCreate.PerimeterVertices.SetNum( 3 );

					// Set first two vertices of the new polygon directly to the original vertex instances
					PolygonToCreate.PerimeterVertices[ 0 ].VertexInstanceID = VertexInstanceID0;
					PolygonToCreate.PerimeterVertices[ 1 ].VertexInstanceID = VertexInstanceID1;

					// Last vertex is the center vertex, so assign the new vertex
					PolygonToCreate.PerimeterVertices[ 2 ].VertexID = PolygonCenterVertexID;

					// Generate interpolated vertex instance attributes for the new vertex in the center
					{
						const FVector CenterVertexPosition = VertexPositions[ PolygonCenterVertexID ];

						FMeshTriangle Triangle;
						FVector TriangleVertexWeights;
						if( ComputeBarycentricWeightForPointOnPolygon( PolygonID, CenterVertexPosition, /* Out */ Triangle, /* Out */ TriangleVertexWeights ) )
						{
							InterpAllAttributes( PolygonToCreate.PerimeterVertices[ 2 ].PolygonVertexAttributes, GetMeshDescription()->VertexInstanceAttributes(),
								Triangle.VertexInstanceID0, Triangle.VertexInstanceID1, Triangle.VertexInstanceID2, TriangleVertexWeights );
						}
					}
				}
			}
			else if( ensure( TriangleTessellationMode == ETriangleTessellationMode::FourTriangles ) )
			{
				// Define the four new triangles for the original tessellated triangle.  One triangle will go in
				// the center, connecting the three new vertices that we created between each original edge.  The
				// other three triangles will go in the corners of the original triangle.

				const TArray<FVertexInstanceID>& PerimeterVertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID );
				check( PerimeterVertexInstanceIDs.Num() == 6 );	// We split the triangle's 3 edges earlier, so we must have six edges now

				// Define the new center triangle
				{
					FPolygonToCreate& PolygonToCreate = *new( PolygonsToCreate ) FPolygonToCreate();
					PolygonToCreate.PolygonGroupID = PolygonGroupID;

					for( int32 OriginalVertexNumber = 0; OriginalVertexNumber < 3; ++OriginalVertexNumber )
					{
						const int32 VertexNumber = ( OriginalVertexNumber * 2 + 1 ) % PerimeterVertexInstanceIDs.Num();

						FVertexAndAttributes& VertexAndAttributes = *new( PolygonToCreate.PerimeterVertices ) FVertexAndAttributes();
						VertexAndAttributes.VertexInstanceID = PerimeterVertexInstanceIDs[ VertexNumber ];
					}
				}

				// Define the three corner triangles
				for( int32 OriginalEdgeNumber = 0; OriginalEdgeNumber < 3; ++OriginalEdgeNumber )
				{
					const int32 CurrentVertexNumber = OriginalEdgeNumber * 2;
					const int32 PreviousVertexNumber = ( ( CurrentVertexNumber - 1 ) + PerimeterVertexInstanceIDs.Num() ) % PerimeterVertexInstanceIDs.Num();
					const int32 NextVertexNumber = ( CurrentVertexNumber + 1 ) % PerimeterVertexInstanceIDs.Num();

					FPolygonToCreate& PolygonToCreate = *new( PolygonsToCreate ) FPolygonToCreate();
					PolygonToCreate.PolygonGroupID = PolygonGroupID;
					PolygonToCreate.PerimeterVertices.SetNum( 3 );

					PolygonToCreate.PerimeterVertices[ 0 ].VertexInstanceID = PerimeterVertexInstanceIDs[ PreviousVertexNumber ];
					PolygonToCreate.PerimeterVertices[ 1 ].VertexInstanceID = PerimeterVertexInstanceIDs[ CurrentVertexNumber ];
					PolygonToCreate.PerimeterVertices[ 2 ].VertexInstanceID = PerimeterVertexInstanceIDs[ NextVertexNumber ];
				}
			}
		}
	}


	// Delete the original polygons
	{
		const bool bDeleteOrphanedEdges = false;	// No need to delete orphans, because this function won't orphan anything
		const bool bDeleteOrphanedVertices = false;
		const bool bDeleteOrphanedVertexInstances = false;
		const bool bDeleteEmptyPolygonGroups = false;
		DeletePolygons( PolygonIDs, bDeleteOrphanedEdges, bDeleteOrphanedVertices, bDeleteOrphanedVertexInstances, bDeleteEmptyPolygonGroups );
	}


	// Create all of the new polygons for the tessellated representation of the original polygons
	{
		static TArray<FEdgeID> NewEdgeIDs;
		CreatePolygons( PolygonsToCreate, /* Out */ OutNewPolygonIDs, /* Out */ NewEdgeIDs );
	}

	EM_EXIT( TEXT( "TessellatePolygons returned %s" ), *LogHelpers::ArrayToString( OutNewPolygonIDs ) );
}


void UEditableMesh::SetTextureCoordinateCount( const int32 NumTexCoords )
{
	TextureCoordinateCount = FMath::Max( NumTexCoords, 0 );
}


void UEditableMesh::QuadrangulateMesh( TArray<FPolygonID>& OutNewPolygonIDs )
{
	// Iterate through all polygon groups in the mesh, quadrangulating each in turn
	for( const FPolygonGroupID PolygonGroupID : GetMeshDescription()->PolygonGroups().GetElementIDs() )
	{
		static TArray<FPolygonID> NewPolygonIDs;
		QuadrangulatePolygonGroup( PolygonGroupID, NewPolygonIDs );
		OutNewPolygonIDs.Append( NewPolygonIDs );
	}
}


void UEditableMesh::QuadrangulatePolygonGroup( const FPolygonGroupID PolygonGroupID, TArray<FPolygonID>& OutNewPolygonIDs )
{
	// Uses the first two steps of the algorithm described by
	// http://www.lirmm.fr/~beniere/ArticlesPersos/GRAPP10_Beniere_Final.pdf

	// Tweakable parameters affecting how quadrangulate works
	const float CosAngleThreshold = 0.984f;		// about 10 degrees
	const bool bKeepHardEdges = true;

	OutNewPolygonIDs.Reset();

	static TArray<FPolygonID> PolygonIDs;
	PolygonIDs.Reset();

	// Get a list of all polygon refs in the mesh
	// @todo mesheditor: provide direct access to the polygons array in the group
	{
		const int32 PolygonCountInGroup = GetPolygonCountInGroup( PolygonGroupID );
		PolygonIDs.Reserve( PolygonCountInGroup );
		for( int32 PolygonIndex = 0; PolygonIndex < PolygonCountInGroup; ++PolygonIndex )
		{
			PolygonIDs.Add( GetPolygonInGroup( PolygonGroupID, PolygonIndex ) );
		}
	}

	// This represents an adjacent triangle which can be merged to a quadrilateral, and an assigned score based on the 'quality' of the resulting quadrilateral.
	struct FAdjacentPolygon
	{
		// Polygon ID of the adjacent triangle. This object is keyed on 'our' Polygon ID.
		FPolygonID PolygonID;

		// List of the vertices which form the merged quadrilateral - (Polygon ID, PolygonPerimeterIndex)
		TTuple<FPolygonID, int32> Vertices[ 4 ];

		// 'Quality' of the quadrilateral (internal angles closer to 90 degrees are better)
		float Score;

		FAdjacentPolygon() {}

		FAdjacentPolygon( FPolygonID InPolygonID, FPolygonID InAdjacentPolygonID, int32 InVertex0, int32 InVertex1, int32 InVertex2, int32 InVertex3, float InScore )
		{
			PolygonID = InAdjacentPolygonID;
			Vertices[ 0 ] = MakeTuple( InPolygonID, InVertex0 );
			Vertices[ 1 ] = MakeTuple( InPolygonID, InVertex1 );
			Vertices[ 2 ] = MakeTuple( InAdjacentPolygonID, InVertex2 );
			Vertices[ 3 ] = MakeTuple( InAdjacentPolygonID, InVertex3 );
			Score = InScore;
		}
	};


	// This represents a list of adjacent polygons, ordered by score.
	// Since we are only connecting triangles, there are a maximum of three adjacent polygons.
	struct FAdjacentPolygons
	{
		enum { MaxAdjacentPolygons = 3 };

		FAdjacentPolygon AdjacentPolygons[ MaxAdjacentPolygons ];
		int32 NumAdjacentPolygons;

		FAdjacentPolygons()
		{
			NumAdjacentPolygons = 0;
		}

		void Add( const FAdjacentPolygon& AdjacentPolygon )
		{
			check( NumAdjacentPolygons < MaxAdjacentPolygons );
			int32 InsertIndex = 0;
			for( int32 Index = 0; Index < NumAdjacentPolygons; ++Index )
			{
				if( AdjacentPolygon.Score > AdjacentPolygons[ Index ].Score )
				{
					InsertIndex++;
				}
				else
				{
					break;
				}
			}

			for( int32 Index = NumAdjacentPolygons; Index > InsertIndex; --Index )
			{
				AdjacentPolygons[ Index ] = AdjacentPolygons[ Index - 1 ];
			}
			AdjacentPolygons[ InsertIndex ] = AdjacentPolygon;
			NumAdjacentPolygons++;
		}

		const FAdjacentPolygon& GetBestAdjacentPolygon() const
		{
			check( NumAdjacentPolygons > 0 );
			return AdjacentPolygons[ 0 ];
		}
		
		bool Remove( const FPolygonID PolygonID )
		{
			for( int32 Index = 0; Index < NumAdjacentPolygons; ++Index )
			{
				if( AdjacentPolygons[ Index ].PolygonID == PolygonID )
				{
					for( int32 CopyIndex = Index + 1; CopyIndex < NumAdjacentPolygons; ++CopyIndex )
					{
						AdjacentPolygons[ CopyIndex - 1 ] = AdjacentPolygons[ CopyIndex ];
					}

					NumAdjacentPolygons--;
					return true;
				}
			}

			return false;
		}

		bool Contains( const FPolygonID PolygonID ) const
		{
			for( int32 Index = 0; Index < NumAdjacentPolygons; ++Index )
			{
				if( AdjacentPolygons[ Index ].PolygonID == PolygonID )
				{
					return true;
				}
			}

			return false;
		}

		FPolygonID GetPolygonID( int32 Index ) const
		{
			check( Index < NumAdjacentPolygons );
			return AdjacentPolygons[ Index ].PolygonID;
		}

		int32 Num() const { return NumAdjacentPolygons; }
		bool IsValid() const { return NumAdjacentPolygons > 0; }
	};


	// Build list of valid adjacent triangle pairs, and assign a score based on the quality of the quadrilateral they form

	static TMap<FPolygonID, FAdjacentPolygons> AdjacentPolygonsMap;
	AdjacentPolygonsMap.Reset();

	FPolygonID PolygonIDToMerge1 = FPolygonID::Invalid;
	{
		float BestScore = TNumericLimits<float>::Max();

		const TEdgeAttributesRef<bool> EdgeHardnesses = GetMeshDescription()->EdgeAttributes().GetAttributesRef<bool>( MeshAttribute::Edge::IsHard );
		const TVertexAttributesRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );

		for( const FPolygonID PolygonID : PolygonIDs )
		{
			// If it's not a triangle, don't consider this polygon at all
			if( GetPolygonPerimeterEdgeCount( PolygonID ) != 3 )
			{
				continue;
			}

			// We're only interested in adjacent triangles which are nearly coplanar; get the normal so we can compare it with the adjacent polygons' normals
			const FVector PolygonNormal = ComputePolygonNormal( PolygonID );

			// Go round the edge considering all adjacent polygons, looking for valid pairs and assigning a quality score (lower is better)
			for( int32 PerimeterEdgeIndex = 0; PerimeterEdgeIndex < 3; ++PerimeterEdgeIndex )
			{
				bool bOutEdgeWindingIsReversedForPolygon;
				const FEdgeID PerimeterEdgeID = GetPolygonPerimeterEdge( PolygonID, PerimeterEdgeIndex, bOutEdgeWindingIsReversedForPolygon );

				const bool bIsSoftEdge = !EdgeHardnesses[ PerimeterEdgeID ];
				if( !bKeepHardEdges || bIsSoftEdge )
				{
					const FPolygonID AdjacentPolygonID = [ this, PerimeterEdgeID, PolygonID ]() -> FPolygonID
					{
						const int32 EdgeConnectedPolygonCount = GetEdgeConnectedPolygonCount( PerimeterEdgeID );

						// Only interested in edges with exactly two connected polygons
						if( EdgeConnectedPolygonCount == 2 )
						{
							for( int32 EdgeConnectedPolygonIndex = 0; EdgeConnectedPolygonIndex < 2; ++EdgeConnectedPolygonIndex )
							{
								const FPolygonID EdgeConnectedPolygonID = GetEdgeConnectedPolygon( PerimeterEdgeID, EdgeConnectedPolygonIndex );
								if( EdgeConnectedPolygonID != PolygonID )
								{
									return GetPolygonPerimeterEdgeCount( EdgeConnectedPolygonID ) == 3 ? EdgeConnectedPolygonID : FPolygonID::Invalid;
								}
							}
						}

						return FPolygonID::Invalid;
					}();

					if( AdjacentPolygonID != FPolygonID::Invalid )
					{
						FAdjacentPolygons* AdjacentPolygons = AdjacentPolygonsMap.Find( PolygonID );

						if( !AdjacentPolygons || !AdjacentPolygons->Contains( AdjacentPolygonID ) )
						{
							const FVector AdjacentPolygonNormal = ComputePolygonNormal( AdjacentPolygonID );
							const float AdjacentPolygonDot = FVector::DotProduct( PolygonNormal, AdjacentPolygonNormal );

							if( AdjacentPolygonDot >= CosAngleThreshold )
							{
								// Found a valid triangle pair whose interplanar angle is sufficiently shallow;
								// now calculate a score according to the internal angles of the resulting quad

								// We consider points on the two triangles' perimeters which form the quadrilateral.
								// If the shared edge of the adjacent triangles falls on perimeter vertex N1 of triangle 1,
								// and perimeter vertex N2 of triangle 2, then the points we consider are:
								//
								// (triangle 1, point N1 - 1)
								// (triangle 1, point N1)
								// (triangle 2, point N2 + 1)
								// (triangle 2, point N2 - 1)
								//
								// or, from the perspective of the other triangle:
								//
								// (triangle 2, point N2 + 1)
								// (triangle 2, point N2 - 1)
								// (triangle 1, point N1 - 1)
								// (triangle 1, point N1)

								const int32 PrevPerimeterEdgeIndex = ( PerimeterEdgeIndex + 2 ) % 3;
								const int32 NextPerimeterEdgeIndex = ( PerimeterEdgeIndex + 1 ) % 3;

								const FVertexID SharedVertexID = GetPolygonPerimeterVertex( PolygonID, PerimeterEdgeIndex );
								const int32 AdjacentPerimeterEdgeIndex = FindPolygonPerimeterVertexNumberForVertex( AdjacentPolygonID, SharedVertexID );

								const int32 PrevAdjacentPerimeterEdgeIndex = ( AdjacentPerimeterEdgeIndex + 2 ) % 3;
								const int32 NextAdjacentPerimeterEdgeIndex = ( AdjacentPerimeterEdgeIndex + 1 ) % 3;

								const TArray<FVertexInstanceID>& PerimeterVertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( PolygonID );
								const TArray<FVertexInstanceID>& AdjacentPerimeterVertexInstanceIDs = GetMeshDescription()->GetPolygonPerimeterVertexInstances( AdjacentPolygonID );

								const FVertexInstanceID VertexInstanceStart1 = PerimeterVertexInstanceIDs[ PerimeterEdgeIndex ];
								const FVertexInstanceID VertexInstanceStart2 = AdjacentPerimeterVertexInstanceIDs[ AdjacentPerimeterEdgeIndex ];
								const FVertexInstanceID VertexInstanceEnd1 = PerimeterVertexInstanceIDs[ NextPerimeterEdgeIndex ];
								const FVertexInstanceID VertexInstanceEnd2 = AdjacentPerimeterVertexInstanceIDs[ PrevAdjacentPerimeterEdgeIndex ];

								auto AttributePred = []( const auto AttributeName, const auto& AttributeIndicesArray ) { return EnumHasAllFlags( AttributeIndicesArray.GetFlags(), EMeshAttributeFlags::Mergeable ); };

								const bool bAttributesEqual = ( VertexInstanceStart1 == VertexInstanceStart2 && VertexInstanceEnd1 == VertexInstanceEnd2 ) ||
									( CompareAttributesByPredicate( GetMeshDescription()->VertexInstanceAttributes(), VertexInstanceStart1, VertexInstanceStart2, AttributePred ) &&
									  CompareAttributesByPredicate( GetMeshDescription()->VertexInstanceAttributes(), VertexInstanceEnd1, VertexInstanceEnd2, AttributePred ) );

								if( bAttributesEqual )
								{
									const FVertexID V0 = GetPolygonPerimeterVertex( PolygonID, PrevPerimeterEdgeIndex );
									const FVertexID V1 = GetPolygonPerimeterVertex( PolygonID, PerimeterEdgeIndex );
									const FVertexID V2 = GetPolygonPerimeterVertex( AdjacentPolygonID, NextAdjacentPerimeterEdgeIndex );
									const FVertexID V3 = GetPolygonPerimeterVertex( AdjacentPolygonID, PrevAdjacentPerimeterEdgeIndex );
									check( V3 == GetPolygonPerimeterVertex( PolygonID, NextPerimeterEdgeIndex ) );

									const FVector P0 = VertexPositions[ V0 ];
									const FVector P1 = VertexPositions[ V1 ];
									const FVector P2 = VertexPositions[ V2 ];
									const FVector P3 = VertexPositions[ V3 ];

									const FVector D01 = ( P1 - P0 ).GetSafeNormal();
									const FVector D12 = ( P2 - P1 ).GetSafeNormal();
									const FVector D23 = ( P3 - P2 ).GetSafeNormal();
									const FVector D30 = ( P0 - P3 ).GetSafeNormal();

									// Calculate a score based on the internal angles of the quadrilateral and the interplanar angle.
									// Internal angles close to 90 degrees, and an interplanar angle close to 180 degrees are ideal.
									const float Score =
										FMath::Abs( HALF_PI - FMath::Acos( FVector::DotProduct( -D30, D01 ) ) ) +
										FMath::Abs( HALF_PI - FMath::Acos( FVector::DotProduct( -D01, D12 ) ) ) +
										FMath::Abs( HALF_PI - FMath::Acos( FVector::DotProduct( -D12, D23 ) ) ) +
										FMath::Abs( HALF_PI - FMath::Acos( FVector::DotProduct( -D23, D30 ) ) ) +
										FMath::Acos( AdjacentPolygonDot );

									if( Score < BestScore )
									{
										BestScore = Score;
										PolygonIDToMerge1 = PolygonID;
									}

									// Add to a list of adjacent polygons, sorted by score
									FAdjacentPolygons& AdjacentPolygons1 = AdjacentPolygonsMap.FindOrAdd( PolygonID );
									AdjacentPolygons1.Add( FAdjacentPolygon(
										PolygonID,
										AdjacentPolygonID,
										PrevPerimeterEdgeIndex,
										PerimeterEdgeIndex,
										NextAdjacentPerimeterEdgeIndex,
										PrevAdjacentPerimeterEdgeIndex,
										Score ) );

									// And perform the corresponding operation the other way round
									FAdjacentPolygons& AdjacentPolygons2 = AdjacentPolygonsMap.FindOrAdd( AdjacentPolygonID );
									check( !AdjacentPolygons2.Contains( PolygonID ) );
									AdjacentPolygons2.Add( FAdjacentPolygon(
										AdjacentPolygonID,
										PolygonID,
										NextAdjacentPerimeterEdgeIndex,
										PrevAdjacentPerimeterEdgeIndex,
										PrevPerimeterEdgeIndex,
										PerimeterEdgeIndex,
										Score ) );
								}
							}
						}
					}
				}
			}
		}
	}

	// If there were no valid pairs of polys to merge, finish already
	if( PolygonIDToMerge1 == FPolygonID::Invalid )
	{
		return;
	}

	static TArray<FPolygonToCreate> PolygonsToCreate;
	PolygonsToCreate.Reset();

	static TArray<FPolygonID> PolygonIDsToDelete;
	PolygonIDsToDelete.Reset();

	static TSet<FPolygonID> BoundaryPolygons;
	BoundaryPolygons.Reset();

	// Propagate quadrangulated area outwards from starting polygon
	for( ; ; )
	{
		FAdjacentPolygons& AdjacentPolygons1 = AdjacentPolygonsMap.FindChecked( PolygonIDToMerge1 );
		check( AdjacentPolygons1.IsValid() );
		const FAdjacentPolygon& AdjacentPolygon1 = AdjacentPolygons1.GetBestAdjacentPolygon();
		const FPolygonID PolygonIDToMerge2 = AdjacentPolygon1.PolygonID;

		FAdjacentPolygons& AdjacentPolygons2 = AdjacentPolygonsMap.FindChecked( PolygonIDToMerge2 );
		check( AdjacentPolygons2.IsValid() );

		// Create new quadrilateral

		PolygonsToCreate.Emplace();

		FPolygonToCreate& PolygonToCreate = PolygonsToCreate.Last();

		PolygonToCreate.PolygonGroupID = PolygonGroupID;
		PolygonToCreate.PerimeterVertices.Reset( 4 );

		for( int32 VertexIndex = 0; VertexIndex < 4; ++VertexIndex )
		{
			PolygonToCreate.PerimeterVertices.Emplace();
			FVertexAndAttributes& VertexAndAttributes = PolygonToCreate.PerimeterVertices.Last();

			// Build polygon from original vertex instances, to save creating new ones and copying the original attributes over
			VertexAndAttributes.VertexInstanceID = GetPolygonPerimeterVertexInstance(
				AdjacentPolygon1.Vertices[ VertexIndex ].Get<0>(),
				AdjacentPolygon1.Vertices[ VertexIndex ].Get<1>() );
		}

		// Specify old polygons to be deleted

		check( !PolygonIDsToDelete.Contains( PolygonIDToMerge1 ) );
		check( !PolygonIDsToDelete.Contains( PolygonIDToMerge2 ) );
		PolygonIDsToDelete.Emplace( PolygonIDToMerge1 );
		PolygonIDsToDelete.Emplace( PolygonIDToMerge2 );

		// And remove them from the boundary set

		BoundaryPolygons.Remove( PolygonIDToMerge1 );
		BoundaryPolygons.Remove( PolygonIDToMerge2 );

		// Now break connections between newly added polygons and their neighbors.
		// If a polygon ends up with no connections, delete it entirely from the map so it is no longer considered.
		// This happens if a polygon has been added to the quadrangulated set, or if it is an orphaned triangle which cannot be paired to anything.
		// We defer deleting the entry from the map until we have broken all connections.

		verify( AdjacentPolygons1.Remove( PolygonIDToMerge2 ) );
		verify( AdjacentPolygons2.Remove( PolygonIDToMerge1 ) );

		static TArray<FPolygonID> AdjacentPolygonsEntryToDelete;
		AdjacentPolygonsEntryToDelete.Reset();

		for( int32 Index = 0; Index < AdjacentPolygons1.Num(); ++Index )
		{
			const FPolygonID AdjacentPolygonID = AdjacentPolygons1.GetPolygonID( Index );

			FAdjacentPolygons* OtherAdjacentPolygons = AdjacentPolygonsMap.Find( AdjacentPolygonID );
			if( OtherAdjacentPolygons )
			{
				verify( OtherAdjacentPolygons->Remove( PolygonIDToMerge1 ) );
				if( !OtherAdjacentPolygons->IsValid() )
				{
					AdjacentPolygonsEntryToDelete.Add( AdjacentPolygonID );
				}
				else
				{
					BoundaryPolygons.Add( AdjacentPolygonID );
				}
			}
		}

		AdjacentPolygonsEntryToDelete.Add( PolygonIDToMerge1 );

		for( int32 Index = 0; Index < AdjacentPolygons2.Num(); ++Index )
		{
			const FPolygonID AdjacentPolygonID = AdjacentPolygons2.GetPolygonID( Index );

			FAdjacentPolygons* OtherAdjacentPolygons = AdjacentPolygonsMap.Find( AdjacentPolygonID );
			if( OtherAdjacentPolygons )
			{
				verify( OtherAdjacentPolygons->Remove( PolygonIDToMerge2 ) );
				if( !OtherAdjacentPolygons->IsValid() )
				{
					AdjacentPolygonsEntryToDelete.Add( AdjacentPolygonID );
				}
				else
				{
					BoundaryPolygons.Add( AdjacentPolygonID );
				}
			}
		}

		AdjacentPolygonsEntryToDelete.Add( PolygonIDToMerge2 );

		// Clean up: any polygons' map entries which now have no adjacent polygons get deleted completely.
		// This implies they have no connected neighbors which can be merged (either because they are near an edge with only unmergeable polygons nearby, or because
		// they are in the middle of the quadrangulated area)

		for( const FPolygonID AdjacentPolygonsEntry : AdjacentPolygonsEntryToDelete )
		{
			AdjacentPolygonsMap.Remove( AdjacentPolygonsEntry );
			BoundaryPolygons.Remove( AdjacentPolygonsEntry );
		}

		// Now look for the next polygon to use: it is the one with the best score from the BoundaryPolygons set.

		float BestScore = TNumericLimits<float>::Max();
		PolygonIDToMerge1 = FPolygonID::Invalid;
		for( const FPolygonID BoundaryPolygon : BoundaryPolygons )
		{
			const FAdjacentPolygons& AdjacentPolygons = AdjacentPolygonsMap.FindChecked( BoundaryPolygon );
			const FAdjacentPolygon& AdjacentPolygon = AdjacentPolygons.GetBestAdjacentPolygon();
			if( AdjacentPolygon.Score < BestScore )
			{
				PolygonIDToMerge1 = AdjacentPolygon.PolygonID;
			}
		}

		// If there are still no candidates adjacent to the already quadrangulated area, choose the best candidate elsewhere.
		// This will start a new quadrangulated area, which is grown in the same way as the last.

		if( PolygonIDToMerge1 == FPolygonID::Invalid )
		{
			BoundaryPolygons.Reset();

			for( const auto& AdjacentPolygonsMapEntry : AdjacentPolygonsMap )
			{
				const FPolygonID PolygonID = AdjacentPolygonsMapEntry.Key;
				const FAdjacentPolygons& AdjacentPolygons = AdjacentPolygonsMapEntry.Value;
				check( AdjacentPolygons.IsValid() );

				const FAdjacentPolygon& AdjacentPolygon = AdjacentPolygons.GetBestAdjacentPolygon();

				if( AdjacentPolygon.Score < BestScore )
				{
					BestScore = AdjacentPolygon.Score;
					PolygonIDToMerge1 = PolygonID;
				}
			}
		}

		// If there are still no candidates, we've done as much as we can do

		if( PolygonIDToMerge1 == FPolygonID::Invalid )
		{
			break;
		}
	}

	// Finally, actually change the geometry and rebuild normals/tangents

	static TArray<FEdgeID> CreatedEdgeIDs;

	CreatePolygons( PolygonsToCreate, OutNewPolygonIDs, CreatedEdgeIDs );

	const bool bDeleteOrphanedEdges = true;
	const bool bDeleteOrphanedVertices = false;
	const bool bDeleteOrphanedVertexInstances = true;
	const bool bDeleteEmptyPolygonGroups = false;
	DeletePolygons( PolygonIDsToDelete, bDeleteOrphanedEdges, bDeleteOrphanedVertices, bDeleteOrphanedVertexInstances, bDeleteEmptyPolygonGroups );
}


void UEditableMesh::UpdateOrRebuildOctree()
{
	bool bAnythingChanged = true;
	bool bDoIncrementalUpdate = false;

	if( Octree.IsValid() )
	{
		if( IsSpatialDatabaseAllowed() )
		{
			if( NewOctreePolygonIDs.Num() == 0 &&
				DeletedOctreePolygonIDs.Num() == 0 )
			{
				// Nothing has changed!
				bAnythingChanged = false;
			}
			else
			{
				// Make sure we haven't changed or deleted so many polygons that it's not even worth doing an incremental
				// update to the mesh.  It's generally more expensive to remove things from the octree than to add to it,
				// because it will need to consider how to collapse nodes.  If we're only adding new things, then we'll
				// never rebuild it from scratch.
				const float ScalarPercentOfPolygonsChangedOrDeleted = (float)DeletedOctreePolygonIDs.Num() / (float)GetPolygonCount();
				if( ScalarPercentOfPolygonsChangedOrDeleted < EditableMesh::OctreeIncrementalUpdateLimit->GetFloat() )
				{
					// We have a reasonable number of incremental changes, so let's go ahead and make those!
					bDoIncrementalUpdate = true;
				}
			}
		}
	}

	if( bAnythingChanged )
	{
		if( bDoIncrementalUpdate )
		{
			check( Octree.IsValid() );

			// Clear out deleted polygons from our octree
			{
				for( const FPolygonID PolygonID : DeletedOctreePolygonIDs )
				{
					const FOctreeElementId OctreeElementID = PolygonIDToOctreeElementIDMap.FindAndRemoveChecked( PolygonID );
					Octree->RemoveElement( OctreeElementID );
				}
				DeletedOctreePolygonIDs.Reset();
			}

			// Now, add new polygons to the octree
			{
				const TVertexAttributesRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );
				for( const FPolygonID PolygonID : NewOctreePolygonIDs )
				{
					checkSlow( IsValidPolygon( PolygonID ) );

					FBox BoundingBox;
					{
						BoundingBox.Init();

						static TArray<FVertexID> PerimeterVertices;
						GetPolygonPerimeterVertices( PolygonID, /* Out */ PerimeterVertices );
						for( const FVertexID VertexID : PerimeterVertices )
						{
							BoundingBox += VertexPositions[ VertexID ];
						}
					}

					const FEditableMeshOctreePolygon OctreePolygon( *this, PolygonID, FBoxCenterAndExtent( BoundingBox ) );
					Octree->AddElement( OctreePolygon );
				}
				NewOctreePolygonIDs.Reset();
			}
		}
		else
		{
			RebuildOctree();
		}
	}
}


void UEditableMesh::RebuildOctree()
{
	Octree.Reset();
	PolygonIDToOctreeElementIDMap.Reset();
	NewOctreePolygonIDs.Reset();
	DeletedOctreePolygonIDs.Reset();

	if( IsSpatialDatabaseAllowed() )
	{
		FVector OctreeOrigin;
		float OctreeExtent;
		{
			if( EditableMesh::UseBoundlessOctree->GetInt() != 0 )
			{
				// We use a 'boundless' octree for our mesh.  This is because we don't want to have to update the root node bounds
				// of the tree as we change the mesh, and also to avoid having to compute a bounding box here.
				OctreeOrigin = FVector::ZeroVector;
				OctreeExtent = HALF_WORLD_MAX;
			}
			else
			{
				// Compute a bounding box to use that tightly wraps the mesh.

				// NOTE: This will have problems.  The root bounding box cannot change without rebuilding the octree, so if the
				// mesh is modified and grown, elements outside the original bounds will not be selectable!  Additionally, we
				// incur the additional performance cost of computing a fresh bounding box here.

				const FBox MeshBoundingBox = this->ComputeBoundingBox();
				OctreeOrigin = MeshBoundingBox.GetCenter();
				OctreeExtent = MeshBoundingBox.GetExtent().GetAbsMax();
			}
		}

		Octree = MakeShareable( new FEditableMeshOctree( OctreeOrigin, OctreeExtent ) );

		// @todo mesheditor spatial: We'll currently only ever find vertices and edges that are connected to polygons.  Our spatial
		// database only tracks polygons.  Do we need to support hover/selection of "loose" vertices and edges also?

		const TVertexAttributesRef<FVector> VertexPositions = GetMeshDescription()->VertexAttributes().GetAttributesRef<FVector>( MeshAttribute::Vertex::Position );
		for( const FPolygonID PolygonID : GetMeshDescription()->Polygons().GetElementIDs() )
		{
			FBox BoundingBox;
			{
				BoundingBox.Init();

				static TArray<FVertexID> PerimeterVertices;
				GetPolygonPerimeterVertices( PolygonID, /* Out */ PerimeterVertices );
				for( const FVertexID VertexID : PerimeterVertices )
				{
					BoundingBox += VertexPositions[ VertexID ];
				}
			}

			const FEditableMeshOctreePolygon OctreePolygon( *this, PolygonID, FBoxCenterAndExtent( BoundingBox ) );
			Octree->AddElement( OctreePolygon );
		}

		// @todo mesheditor debug
		// Octree->DumpStats();
	}
}

void UEditableMesh::SearchSpatialDatabaseWithPredicate( TFunctionRef< bool( const FBox& Bounds ) > Predicate, TArray< FPolygonID >& OutPolygons ) const
{
	OutPolygons.Reset();

	// @todo mesheditor scripting: Should spit a warning for Blueprint users if Octree is not allowed when calling this function

	if( IsSpatialDatabaseAllowed() && ensure( Octree.IsValid() ) )
	{
		// @todo mesheditor perf: Do we need to use a custom stack allocator for iterating?  The default should probably be okay.
		for( FEditableMeshOctree::TConstIterator<> OctreeIt( *Octree );
			 OctreeIt.HasPendingNodes();
			 OctreeIt.Advance() )
		{
			const FEditableMeshOctree::FNode& OctreeNode = OctreeIt.GetCurrentNode();
			const FOctreeNodeContext& OctreeNodeContext = OctreeIt.GetCurrentContext();

			// Leaf nodes have no children, so don't bother iterating
			if( !OctreeNode.IsLeaf() )
			{
				FOREACH_OCTREE_CHILD_NODE( ChildRef )
				{
					if( OctreeNode.HasChild( ChildRef ) )
					{
						const FOctreeNodeContext ChildContext = OctreeNodeContext.GetChildContext( ChildRef );

						if( Predicate( ChildContext.Bounds.GetBox() ) )
						{
							// Push it on the iterator's pending node stack.
							OctreeIt.PushChild( ChildRef );
						}
					}
				}
			}

			// All of the elements in this octree node are candidates.  Note this node may not be a leaf node, and that's OK.
			for( FEditableMeshOctree::ElementConstIt OctreeElementIt( OctreeNode.GetElementIt() ); OctreeElementIt; ++OctreeElementIt )
			{
				const FEditableMeshOctreePolygon& OctreePolygon = *OctreeElementIt;
				OutPolygons.Add( OctreePolygon.PolygonID );
			}
		}
	}
}

void UEditableMesh::SearchSpatialDatabaseForPolygonsInVolume( const TArray<FPlane>& Planes, TArray<FPolygonID>& OutPolygons ) const
{
	auto SearchInVolume = [ &Planes ]( const FBox& Bounds )
	{
		bool bIsInside = true;

		// Inside volume if node intersects or above all planes that form the volume
		for( int32 Index = 0; bIsInside && Index < Planes.Num(); ++Index )
		{
			bIsInside = bIsInside && FMath::PlaneAABBRelativePosition( Planes[ Index ], Bounds ) >= 0;
		}
		return bIsInside;
	};

	SearchSpatialDatabaseWithPredicate( SearchInVolume, OutPolygons );
}

void UEditableMesh::SearchSpatialDatabaseForPolygonsPotentiallyIntersectingLineSegment( const FVector LineSegmentStart, const FVector LineSegmentEnd, TArray<FPolygonID>& OutPolygons ) const
{
	const FVector LineSegmentVector = LineSegmentEnd - LineSegmentStart;
	const FVector LineSegmentVectorReciprocal = LineSegmentVector.Reciprocal();

	auto SearchByLineSegmentIntersection = [ & ]( const FBox& Bounds )
	{
		// @todo mesheditor: LineBoxIntersection() has a magic number in its implementation we might want to look at (search for BOX_SIDE_THRESHOLD)
		return FMath::LineBoxIntersection( Bounds, LineSegmentStart, LineSegmentEnd, LineSegmentVector, LineSegmentVectorReciprocal );

	};

	SearchSpatialDatabaseWithPredicate( SearchByLineSegmentIntersection, OutPolygons );
}

// @todo mesheditor: Combine this function with the one above
void UEditableMesh::SearchSpatialDatabaseForPolygonsPotentiallyIntersectingPlane( const FPlane& InPlane, TArray<FPolygonID>& OutPolygons ) const
{
	OutPolygons.Reset();

	// @todo mesheditor scripting: Should spit a warning for Blueprint users if Octree is not allowed when calling this function

	if( IsSpatialDatabaseAllowed() && ensure( Octree.IsValid() ) )
	{
		// @todo mesheditor perf: Do we need to use a custom stack allocator for iterating?  The default should probably be okay.
		for( FEditableMeshOctree::TConstIterator<> OctreeIt( *Octree );
			 OctreeIt.HasPendingNodes();
			 OctreeIt.Advance() )
		{
			const FEditableMeshOctree::FNode& OctreeNode = OctreeIt.GetCurrentNode();
			const FOctreeNodeContext& OctreeNodeContext = OctreeIt.GetCurrentContext();

			// Leaf nodes have no children, so don't bother iterating
			if( !OctreeNode.IsLeaf() )
			{
				// Find children of this octree node that overlap our line segment
				FOREACH_OCTREE_CHILD_NODE( ChildRef )
				{
					if( OctreeNode.HasChild( ChildRef ) )
					{
						const FOctreeNodeContext ChildContext = OctreeNodeContext.GetChildContext( ChildRef );

                        const bool bIsOverlappingLineSegment =
                            FMath::PlaneAABBIntersection(
                                InPlane,
                                ChildContext.Bounds.GetBox());

						if( bIsOverlappingLineSegment )
						{
							// DrawDebugBox( GWorld, ChildContext.Bounds.Center, ChildContext.Bounds.Extent * 0.8f, FQuat::Identity, FColor::Green, false, 0.0f );		// @todo mesheditor debug: (also, wrong coordinate system!)

							// Push it on the iterator's pending node stack.
							OctreeIt.PushChild( ChildRef );
						}
						else
						{
							// DrawDebugBox( GWorld, ChildContext.Bounds.Center, ChildContext.Bounds.Extent, FQuat::Identity, FColor( 128, 128, 128 ), false, 0.0f );	// @todo mesheditor debug: (also, wrong coordinate system!)
						}
					}
				}
			}

			// All of the elements in this octree node are candidates.  Note this node may not be a leaf node, and that's OK.
			for( FEditableMeshOctree::ElementConstIt OctreeElementIt( OctreeNode.GetElementIt() ); OctreeElementIt; ++OctreeElementIt )
			{
				const FEditableMeshOctreePolygon& OctreePolygon = *OctreeElementIt;
				OutPolygons.Add( OctreePolygon.PolygonID );
			}
		}
	}
}

void UEditableMesh::SetAllowSpatialDatabase( const bool bInAllowSpatialDatabase )
{
	if( bAllowSpatialDatabase != bInAllowSpatialDatabase )
	{
		bAllowSpatialDatabase = bInAllowSpatialDatabase;

		if( !IsBeingModified() )
		{
			RebuildOctree();
		}
	}
}


bool UEditableMesh::AnyChangesToUndo() const
{
	return bAllowUndo && Undo.IsValid() && Undo->Subchanges.Num() > 0;
}


void UEditableMesh::AddUndo( TUniquePtr<FChange> NewUndo )
{
	if( NewUndo.IsValid() )
	{
//		UE_LOG(LogEditableMesh, Verbose, TEXT("%s"), *NewUndo->ToString());
		if( bAllowUndo )
		{
			if( !Undo.IsValid() )
			{
				Undo = MakeUnique<FCompoundChangeInput>();
			}

			Undo->Subchanges.Add( MoveTemp( NewUndo ) );
		}
	}
}


TUniquePtr<FChange> UEditableMesh::MakeUndo()
{
	TUniquePtr<FChange> UndoChange = nullptr;
	if( AnyChangesToUndo() )
	{
		UndoChange = MakeUnique<FCompoundChange>( MoveTemp( *Undo ) );
	}
	Undo.Reset();

	return UndoChange;
}

