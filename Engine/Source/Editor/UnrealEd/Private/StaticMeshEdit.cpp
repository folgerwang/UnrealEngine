// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticMeshEdit.cpp: Static mesh edit functions.
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Misc/FeedbackContext.h"
#include "Engine/EngineTypes.h"
#include "Model.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorFramework/ThumbnailInfo.h"
#include "Engine/MeshMerging.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Polys.h"
#include "Editor.h"
#include "StaticMeshResources.h"
#include "BSPOps.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/BodySetup.h"
#include "FbxImporter.h"
#include "Misc/ScopedSlowTask.h"

#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "PerPlatformProperties.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Settings/EditorExperimentalSettings.h"
#include "MeshDescription.h"
#include "MeshAttributes.h"

#include "Modules/ModuleManager.h"
#include "IMeshReductionManagerModule.h"
#include "IMeshReductionInterfaces.h"


bool GBuildStaticMeshCollision = 1;

DEFINE_LOG_CATEGORY_STATIC(LogStaticMeshEdit, Log, All);

#define LOCTEXT_NAMESPACE "StaticMeshEdit"

static float MeshToPrimTolerance = 0.001f;

/** Floating point comparitor */
static FORCEINLINE bool AreEqual(float a, float b)
{
	return FMath::Abs(a - b) < MeshToPrimTolerance;
}

/** Returns 1 if vectors are parallel OR anti-parallel */
static FORCEINLINE bool AreParallel(const FVector& a, const FVector& b)
{
	float Dot = a | b;

	if( AreEqual(FMath::Abs(Dot), 1.f) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

/** Utility struct used in AddBoxGeomFromTris. */
struct FPlaneInfo
{
	FVector Normal;
	int32 DistCount;
	float PlaneDist[2];

	FPlaneInfo()
	{
		Normal = FVector::ZeroVector;
		DistCount = 0;
		PlaneDist[0] = 0.f;
		PlaneDist[1] = 0.f;
	}
};

struct FMeshConnectivityVertex
{
	FVector				Position;
	TArray<int32>		Triangles;

	/** Constructor */
	FMeshConnectivityVertex( const FVector &v )
		: Position( v )
	{
	}

	/** Check if this vertex is in the same place as given point */
	FORCEINLINE bool IsSame( const FVector &v )
	{
		const float eps = 0.01f;
		return v.Equals( Position, eps );
	}

	/** Add link to triangle */
	FORCEINLINE void AddTriangleLink( int32 Triangle )
	{
		Triangles.Add( Triangle );
	}
};

struct FMeshConnectivityTriangle
{
	int32				Vertices[3];
	int32				Group;

	/** Constructor */
	FMeshConnectivityTriangle( int32 a, int32 b, int32 c )
		: Group( INDEX_NONE )
	{
		Vertices[0] = a;
		Vertices[1] = b;
		Vertices[2] = c;
	}
};

struct FMeshConnectivityGroup
{
	TArray<int32>		Triangles;
};

class FMeshConnectivityBuilder
{
public:
	TArray<FMeshConnectivityVertex>		Vertices;
	TArray<FMeshConnectivityTriangle>	Triangles;
	TArray<FMeshConnectivityGroup>		Groups;

public:
	/** Add vertex to connectivity information */
	int32 AddVertex( const FVector &v )
	{
		// Try to find existing vertex
		// TODO: should use hash map
		for ( int32 i=0; i<Vertices.Num(); ++i )
		{
			if ( Vertices[i].IsSame( v ) )
			{
				return i;
			}
		}

		// Add new vertex
		new ( Vertices ) FMeshConnectivityVertex( v );
		return Vertices.Num() - 1;
	}

	/** Add triangle to connectivity information */
	int32 AddTriangle( const FVector &a, const FVector &b, const FVector &c )
	{
		// Map vertices
		int32 VertexA = AddVertex( a );
		int32 VertexB = AddVertex( b );
		int32 VertexC = AddVertex( c );

		// Make sure triangle is not degenerated
		if ( VertexA!=VertexB && VertexB!=VertexC && VertexC!=VertexA )
		{
			// Setup connectivity info
			int32 TriangleIndex = Triangles.Num();
			Vertices[ VertexA ].AddTriangleLink( TriangleIndex );
			Vertices[ VertexB ].AddTriangleLink( TriangleIndex );
			Vertices[ VertexC ].AddTriangleLink( TriangleIndex );

			// Create triangle
			new ( Triangles ) FMeshConnectivityTriangle( VertexA, VertexB, VertexC );
			return TriangleIndex;
		}
		else
		{
			// Degenerated triangle
			return INDEX_NONE;
		}
	}

	/** Create connectivity groups */
	void CreateConnectivityGroups()
	{
		// Delete group list
		Groups.Empty();

		// Reset group assignments
		for ( int32 i=0; i<Triangles.Num(); i++ )
		{
			Triangles[i].Group = INDEX_NONE;
		}

		// Flood fill using connectivity info
		for ( ;; )
		{
			// Find first triangle without group assignment
			int32 InitialTriangle = INDEX_NONE;
			for ( int32 i=0; i<Triangles.Num(); i++ )
			{
				if ( Triangles[i].Group == INDEX_NONE )
				{
					InitialTriangle = i;
					break;
				}
			}

			// No more unassigned triangles, flood fill is done
			if ( InitialTriangle == INDEX_NONE )
			{
				break;
			}

			// Create group
			int32 GroupIndex = Groups.AddZeroed( 1 );

			// Start flood fill using connectivity information
			FloodFillTriangleGroups( InitialTriangle, GroupIndex );
		}
	}

private:
	/** FloodFill core */
	void FloodFillTriangleGroups( int32 InitialTriangleIndex, int32 GroupIndex )
	{
		TArray<int32> TriangleStack;

		// Start with given triangle
		TriangleStack.Add( InitialTriangleIndex );

		// Set the group for our first triangle
		Triangles[ InitialTriangleIndex ].Group = GroupIndex;

		// Process until we have triangles in stack
		while ( TriangleStack.Num() )
		{
			// Pop triangle index from stack
			int32 TriangleIndex = TriangleStack.Pop();

			FMeshConnectivityTriangle &Triangle = Triangles[ TriangleIndex ];

			// All triangles should already have a group before we start processing neighbors
			checkSlow( Triangle.Group == GroupIndex );

			// Add to list of triangles in group
			Groups[ GroupIndex ].Triangles.Add( TriangleIndex );

			// Recurse to all other triangles connected with this one
			for ( int32 i=0; i<3; i++ )
			{
				int32 VertexIndex = Triangle.Vertices[ i ];
				const FMeshConnectivityVertex &Vertex = Vertices[ VertexIndex ];

				for ( int32 j=0; j<Vertex.Triangles.Num(); j++ )
				{
					int32 OtherTriangleIndex = Vertex.Triangles[ j ];
					FMeshConnectivityTriangle &OtherTriangle = Triangles[ OtherTriangleIndex ];

					// Only recurse if triangle was not already assigned to a group
					if ( OtherTriangle.Group == INDEX_NONE )
					{
						// OK, the other triangle now belongs to our group!
						OtherTriangle.Group = GroupIndex;

						// Add the other triangle to the stack to be processed
						TriangleStack.Add( OtherTriangleIndex );
					}
				}
			}
		}
	}
};

void DecomposeUCXMesh( const TArray<FVector>& CollisionVertices, const TArray<int32>& CollisionFaceIdx, UBodySetup* BodySetup )
{
	// We keep no ref to this Model, so it will be GC'd at some point after the import.
	auto TempModel = NewObject<UModel>();
	TempModel->Initialize(nullptr, 1);

	FMeshConnectivityBuilder ConnectivityBuilder;

	// Send triangles to connectivity builder
	for(int32 x = 0;x < CollisionFaceIdx.Num();x += 3)
	{
		const FVector &VertexA = CollisionVertices[ CollisionFaceIdx[x + 2] ];
		const FVector &VertexB = CollisionVertices[ CollisionFaceIdx[x + 1] ];
		const FVector &VertexC = CollisionVertices[ CollisionFaceIdx[x + 0] ];
		ConnectivityBuilder.AddTriangle( VertexA, VertexB, VertexC );
	}

	ConnectivityBuilder.CreateConnectivityGroups();

	// For each valid group build BSP and extract convex hulls
	for ( int32 i=0; i<ConnectivityBuilder.Groups.Num(); i++ )
	{
		const FMeshConnectivityGroup &Group = ConnectivityBuilder.Groups[ i ];

		// TODO: add some BSP friendly checks here
		// e.g. if group triangles form a closed mesh

		// Generate polygons from group triangles
		TempModel->Polys->Element.Empty();

		for ( int32 j=0; j<Group.Triangles.Num(); j++ )
		{
			const FMeshConnectivityTriangle &Triangle = ConnectivityBuilder.Triangles[ Group.Triangles[j] ];

			FPoly*	Poly = new( TempModel->Polys->Element ) FPoly();
			Poly->Init();
			Poly->iLink = j / 3;

			// Add vertices
			new( Poly->Vertices ) FVector( ConnectivityBuilder.Vertices[ Triangle.Vertices[0] ].Position );
			new( Poly->Vertices ) FVector( ConnectivityBuilder.Vertices[ Triangle.Vertices[1] ].Position );
			new( Poly->Vertices ) FVector( ConnectivityBuilder.Vertices[ Triangle.Vertices[2] ].Position );

			// Update polygon normal
			Poly->CalcNormal(1);
		}

		// Build bounding box.
		TempModel->BuildBound();

		// Build BSP for the brush.
		FBSPOps::bspBuild( TempModel,FBSPOps::BSP_Good,15,70,1,0 );
		FBSPOps::bspRefresh( TempModel, 1 );
		FBSPOps::bspBuildBounds( TempModel );

		// Convert collision model into a collection of convex hulls.
		// Generated convex hulls will be added to existing ones
		BodySetup->CreateFromModel( TempModel, false );
	}
}

/** 
 *	Function for adding a box collision primitive to the supplied collision geometry based on the mesh of the box.
 * 
 *	We keep a list of triangle normals found so far. For each normal direction,
 *	we should have 2 distances from the origin (2 parallel box faces). If the 
 *	mesh is a box, we should have 3 distinct normal directions, and 2 distances
 *	found for each. The difference between these distances should be the box
 *	dimensions. The 3 directions give us the key axes, and therefore the
 *	box transformation matrix. This shouldn't rely on any vertex-ordering on 
 *	the triangles (normals are compared +ve & -ve). It also shouldn't matter 
 *	about how many triangles make up each side (but it will take longer). 
 *	We get the centre of the box from the centre of its AABB.
 */
void AddBoxGeomFromTris( const TArray<FPoly>& Tris, FKAggregateGeom* AggGeom, const TCHAR* ObjName )
{
	TArray<FPlaneInfo> Planes;

	for(int32 i=0; i<Tris.Num(); i++)
	{
		bool bFoundPlane = false;
		for(int32 j=0; j<Planes.Num() && !bFoundPlane; j++)
		{
			// if this triangle plane is already known...
			if( AreParallel( Tris[i].Normal, Planes[j].Normal ) )
			{
				// Always use the same normal when comparing distances, to ensure consistent sign.
				float Dist = Tris[i].Vertices[0] | Planes[j].Normal;

				// we only have one distance, and its not that one, add it.
				if( Planes[j].DistCount == 1 && !AreEqual(Dist, Planes[j].PlaneDist[0]) )
				{
					Planes[j].PlaneDist[1] = Dist;
					Planes[j].DistCount = 2;
				}
				// if we have a second distance, and its not that either, something is wrong.
				else if( Planes[j].DistCount == 2 && !AreEqual(Dist, Planes[j].PlaneDist[1]) )
				{
					UE_LOG(LogStaticMeshEdit, Log, TEXT("AddBoxGeomFromTris (%s): Found more than 2 planes with different distances."), ObjName);
					return;
				}

				bFoundPlane = true;
			}
		}

		// If this triangle does not match an existing plane, add to list.
		if(!bFoundPlane)
		{
			check( Planes.Num() < Tris.Num() );

			FPlaneInfo NewPlane;
			NewPlane.Normal = Tris[i].Normal;
			NewPlane.DistCount = 1;
			NewPlane.PlaneDist[0] = Tris[i].Vertices[0] | NewPlane.Normal;
			
			Planes.Add(NewPlane);
		}
	}

	// Now we have our candidate planes, see if there are any problems

	// Wrong number of planes.
	if(Planes.Num() != 3)
	{
		UE_LOG(LogStaticMeshEdit, Log, TEXT("AddBoxGeomFromTris (%s): Not very box-like (need 3 sets of planes)."), ObjName);
		return;
	}

	// If we don't have 3 pairs, we can't carry on.
	if((Planes[0].DistCount != 2) || (Planes[1].DistCount != 2) || (Planes[2].DistCount != 2))
	{
		UE_LOG(LogStaticMeshEdit, Log, TEXT("AddBoxGeomFromTris (%s): Incomplete set of planes (need 2 per axis)."), ObjName);
		return;
	}

	FMatrix BoxTM = FMatrix::Identity;

	BoxTM.SetAxis(0, Planes[0].Normal);
	BoxTM.SetAxis(1, Planes[1].Normal);

	// ensure valid TM by cross-product
	FVector ZAxis = Planes[0].Normal ^ Planes[1].Normal;

	if( !AreParallel(ZAxis, Planes[2].Normal) )
	{
		UE_LOG(LogStaticMeshEdit, Log, TEXT("AddBoxGeomFromTris (%s): Box axes are not perpendicular."), ObjName);
		return;
	}

	BoxTM.SetAxis(2, ZAxis);

	// OBB centre == AABB centre.
	FBox Box(ForceInit);
	for(int32 i=0; i<Tris.Num(); i++)
	{
		Box += Tris[i].Vertices[0];
		Box += Tris[i].Vertices[1];
		Box += Tris[i].Vertices[2];
	}

	BoxTM.SetOrigin( Box.GetCenter() );

	// Allocate box in array
	FKBoxElem BoxElem;
	BoxElem.SetTransform( FTransform( BoxTM ) );	
	// distance between parallel planes is box edge lengths.
	BoxElem.X = FMath::Abs(Planes[0].PlaneDist[0] - Planes[0].PlaneDist[1]);
	BoxElem.Y = FMath::Abs(Planes[1].PlaneDist[0] - Planes[1].PlaneDist[1]);
	BoxElem.Z = FMath::Abs(Planes[2].PlaneDist[0] - Planes[2].PlaneDist[1]);
	AggGeom->BoxElems.Add(BoxElem);
}

/**
 *	Function for adding a sphere collision primitive to the supplied collision geometry based on a set of Verts.
 *	
 *	Simply put an AABB around mesh and use that to generate centre and radius.
 *	It checks that the AABB is square, and that all vertices are either at the
 *	centre, or within 5% of the radius distance away.
 */
void AddSphereGeomFromVerts( const TArray<FVector>& Verts, FKAggregateGeom* AggGeom, const TCHAR* ObjName )
{
	if(Verts.Num() == 0)
	{
		return;
	}

	FBox Box(ForceInit);

	for(int32 i=0; i<Verts.Num(); i++)
	{
		Box += Verts[i];
	}

	FVector Center, Extents;
	Box.GetCenterAndExtents(Center, Extents);
	float Longest = 2.f * Extents.GetMax();
	float Shortest = 2.f * Extents.GetMin();

	// check that the AABB is roughly a square (5% tolerance)
	if((Longest - Shortest)/Longest > 0.05f)
	{
		UE_LOG(LogStaticMeshEdit, Log, TEXT("AddSphereGeomFromVerts (%s): Sphere bounding box not square."), ObjName);
		return;
	}

	float Radius = 0.5f * Longest;

	// Test that all vertices are a similar radius (5%) from the sphere centre.
	float MaxR = 0;
	float MinR = BIG_NUMBER;
	for(int32 i=0; i<Verts.Num(); i++)
	{
		FVector CToV = Verts[i] - Center;
		float RSqr = CToV.SizeSquared();

		MaxR = FMath::Max(RSqr, MaxR);

		// Sometimes vertex at centre, so reject it.
		if(RSqr > KINDA_SMALL_NUMBER)
		{
			MinR = FMath::Min(RSqr, MinR);
		}
	}

	MaxR = FMath::Sqrt(MaxR);
	MinR = FMath::Sqrt(MinR);

	if((MaxR-MinR)/Radius > 0.05f)
	{
		UE_LOG(LogStaticMeshEdit, Log, TEXT("AddSphereGeomFromVerts (%s): Vertices not at constant radius."), ObjName );
		return;
	}

	// Allocate sphere in array
	FKSphereElem SphereElem;
	SphereElem.Center = Center;
	SphereElem.Radius = Radius;
	AggGeom->SphereElems.Add(SphereElem);
}

void AddCapsuleGeomFromVerts(const TArray<FVector>& Verts, FKAggregateGeom* AggGeom, const TCHAR* ObjName)
{
	if (Verts.Num() < 3)
	{
		return;
	}

	FVector AxisStart, AxisEnd;
	float MaxDistSqr = 0.f;

	for (int32 IndexA = 0; IndexA < Verts.Num() - 1; IndexA++)
	{
		for (int32 IndexB = IndexA + 1; IndexB < Verts.Num(); IndexB++)
		{
			float DistSqr = (Verts[IndexA] - Verts[IndexB]).SizeSquared();
			if (DistSqr > MaxDistSqr)
			{
				AxisStart = Verts[IndexA];
				AxisEnd = Verts[IndexB];
				MaxDistSqr = DistSqr;
			}
		}
	}

	// If we got a valid axis, find vertex furthest from it
	if (MaxDistSqr > SMALL_NUMBER)
	{
		float MaxRadius = 0.f;

		const FVector LineOrigin = AxisStart;
		const FVector LineDir = (AxisEnd - AxisStart).GetSafeNormal();

		for (int32 IndexA = 0; IndexA < Verts.Num() - 1; IndexA++)
		{
			float DistToAxis = FMath::PointDistToLine(Verts[IndexA], LineDir, LineOrigin);
			if (DistToAxis > MaxRadius)
			{
				MaxRadius = DistToAxis;
			}
		}

		if (MaxRadius > SMALL_NUMBER)
		{
			// Allocate capsule in array
			FKSphylElem SphylElem;
			SphylElem.Center = 0.5f * (AxisStart + AxisEnd);
			SphylElem.Rotation = FQuat::FindBetweenVectors(FVector(0,0,1), LineDir).Rotator(); // Get quat that takes you from z axis to desired axis
			SphylElem.Radius = MaxRadius;
			SphylElem.Length = FMath::Max(FMath::Sqrt(MaxDistSqr) - (2.f * MaxRadius), 0.f); // subtract two radii from total length to get segment length (ensure > 0)
			AggGeom->SphylElems.Add(SphylElem);
		}
	}
}


/** Utility for adding one convex hull from the given verts */
void AddConvexGeomFromVertices( const TArray<FVector>& Verts, FKAggregateGeom* AggGeom, const TCHAR* ObjName )
{
	if(Verts.Num() == 0)
	{
		return;
	}

	FKConvexElem* ConvexElem = new(AggGeom->ConvexElems) FKConvexElem();
	ConvexElem->VertexData = Verts;
	ConvexElem->UpdateElemBox();
}


/**
 * Creates a static mesh object from raw triangle data.
 */
UStaticMesh* CreateStaticMesh(FMeshDescription& RawMesh,TArray<FStaticMaterial>& Materials,UObject* InOuter,FName InName)
{
	// Create the UStaticMesh object.
	FStaticMeshComponentRecreateRenderStateContext RecreateRenderStateContext(FindObject<UStaticMesh>(InOuter,*InName.ToString()));
	auto StaticMesh = NewObject<UStaticMesh>(InOuter, InName, RF_Public | RF_Standalone);

	// Add one LOD for the base mesh
	FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
	FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(0);
	*MeshDescription = RawMesh;
	StaticMesh->CommitMeshDescription(0);
	StaticMesh->StaticMaterials = Materials;

	int32 NumSections = StaticMesh->StaticMaterials.Num();

	// Set up the SectionInfoMap to enable collision
	for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
	{
		FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(0, SectionIdx);
		Info.MaterialIndex = SectionIdx;
		Info.bEnableCollision = true;
		StaticMesh->SectionInfoMap.Set(0, SectionIdx, Info);
		StaticMesh->OriginalSectionInfoMap.Set(0, SectionIdx, Info);
	}

	//Set the Imported version before calling the build
	StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

	StaticMesh->Build();
	StaticMesh->MarkPackageDirty();
	return StaticMesh;
}


/**
 *Constructor, setting all values to usable defaults 
 */
FMergeStaticMeshParams::FMergeStaticMeshParams()
	: Offset(0.0f, 0.0f, 0.0f)
	, Rotation(0, 0, 0)
	, ScaleFactor(1.0f)
	, ScaleFactor3D(1.0f, 1.0f, 1.0f)
	, bDeferBuild(false)
	, OverrideElement(INDEX_NONE)
	, bUseUVChannelRemapping(false)
	, bUseUVScaleBias(false)
{
	// initialize some UV channel arrays
	for (int32 Channel = 0; Channel < ARRAY_COUNT(UVChannelRemap); Channel++)
	{
		// we can't just map channel to channel by default, because we need to know when a UV channel is
		// actually being redirected in to, so that we can update Triangle.NumUVs
		UVChannelRemap[Channel] = INDEX_NONE;

		// default to a noop scale/bias
		UVScaleBias[Channel] = FVector4(1.0f, 1.0f, 0.0f, 0.0f);
	}
}

/**
 * Merges SourceMesh into DestMesh, applying transforms along the way
 *
 * @param DestMesh The static mesh that will have SourceMesh merged into
 * @param SourceMesh The static mesh to merge in to DestMesh
 * @param Params Settings for the merge
 */
void MergeStaticMesh(UStaticMesh* DestMesh, UStaticMesh* SourceMesh, const FMergeStaticMeshParams& Params)
{
	// TODO_STATICMESH: Remove me.
}

//
//	FVerticesEqual
//

inline bool FVerticesEqual(FVector& V1,FVector& V2)
{
	if(FMath::Abs(V1.X - V2.X) > THRESH_POINTS_ARE_SAME * 4.0f)
	{
		return 0;
	}

	if(FMath::Abs(V1.Y - V2.Y) > THRESH_POINTS_ARE_SAME * 4.0f)
	{
		return 0;
	}

	if(FMath::Abs(V1.Z - V2.Z) > THRESH_POINTS_ARE_SAME * 4.0f)
	{
		return 0;
	}

	return 1;
}

void GetBrushMesh(ABrush* Brush, UModel* Model, FMeshDescription& MeshDescription, TArray<FStaticMaterial>& OutMaterials)
{
	TVertexAttributesRef<FVector> VertexPositions = MeshDescription.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = MeshDescription.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
	TEdgeAttributesRef<bool> EdgeHardnesses = MeshDescription.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	TEdgeAttributesRef<float> EdgeCreaseSharpnesses = MeshDescription.EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness);
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = MeshDescription.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);
	
	//Make sure we have one UVChannel
	VertexInstanceUVs.SetNumIndices(1);
	
	// Calculate the local to world transform for the source brush.
	FMatrix	ActorToWorld = Brush ? Brush->ActorToWorld().ToMatrixWithScale() : FMatrix::Identity;
	bool	ReverseVertices = 0;
	FVector4	PostSub = Brush ? FVector4(Brush->GetActorLocation()) : FVector4(0, 0, 0, 0);

	TMap<uint32, FEdgeID> RemapEdgeID;
	int32 NumPolys = Model->Polys->Element.Num();
	//Create Fill the vertex position
	for (int32 PolygonIndex = 0; PolygonIndex < NumPolys; ++PolygonIndex)
	{
		FPoly& Polygon = Model->Polys->Element[PolygonIndex];

		// Find a material index for this polygon.
		UMaterialInterface*	Material = Polygon.Material;
		if (Material == nullptr)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		int32 MaterialIndex = OutMaterials.AddUnique(FStaticMaterial(Material, Material->GetFName(), Material->GetFName()));
		FPolygonGroupID CurrentPolygonGroupID = FPolygonGroupID::Invalid;
		for (const FPolygonGroupID& PolygonGroupID : MeshDescription.PolygonGroups().GetElementIDs())
		{
			if (Material->GetFName() == PolygonGroupImportedMaterialSlotNames[PolygonGroupID])
			{
				CurrentPolygonGroupID = PolygonGroupID;
				break;
			}
		}
		if (CurrentPolygonGroupID == FPolygonGroupID::Invalid)
		{
			CurrentPolygonGroupID = MeshDescription.CreatePolygonGroup();
			PolygonGroupImportedMaterialSlotNames[CurrentPolygonGroupID] = Material->GetFName();
		}

		// Cache the texture coordinate system for this polygon.
		FVector	TextureBase = Polygon.Base - (Brush ? Brush->GetPivotOffset() : FVector::ZeroVector),
			TextureX = Polygon.TextureU / UModel::GetGlobalBSPTexelScale(),
			TextureY = Polygon.TextureV / UModel::GetGlobalBSPTexelScale();
		// For each vertex after the first two vertices...
		for (int32 VertexIndex = 2; VertexIndex < Polygon.Vertices.Num(); VertexIndex++)
		{
			FVector Positions[3];
			Positions[ReverseVertices ? 0 : 2] = ActorToWorld.TransformPosition(Polygon.Vertices[0]) - PostSub;
			Positions[1] = ActorToWorld.TransformPosition(Polygon.Vertices[VertexIndex - 1]) - PostSub;
			Positions[ReverseVertices ? 2 : 0] = ActorToWorld.TransformPosition(Polygon.Vertices[VertexIndex]) - PostSub;
			FVertexID VertexID[3] = { FVertexID::Invalid, FVertexID::Invalid, FVertexID::Invalid };
			for (FVertexID IterVertexID : MeshDescription.Vertices().GetElementIDs())
			{
				if (FVerticesEqual(Positions[0], VertexPositions[IterVertexID]))
				{
					VertexID[0] = IterVertexID;
				}
				if (FVerticesEqual(Positions[1], VertexPositions[IterVertexID]))
				{
					VertexID[1] = IterVertexID;
				}
				if (FVerticesEqual(Positions[2], VertexPositions[IterVertexID]))
				{
					VertexID[2] = IterVertexID;
				}
			}

			//Create the vertex instances
			TArray<FVertexInstanceID> VertexInstanceIDs;
			VertexInstanceIDs.SetNum(3);

			for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				if (VertexID[CornerIndex] == FVertexID::Invalid)
				{
					VertexID[CornerIndex] = MeshDescription.CreateVertex();
					VertexPositions[VertexID[CornerIndex]] = Positions[CornerIndex];
				}
				VertexInstanceIDs[CornerIndex] = MeshDescription.CreateVertexInstance(VertexID[CornerIndex]);
				VertexInstanceUVs.Set(VertexInstanceIDs[CornerIndex], 0, FVector2D(
					(Positions[CornerIndex] - TextureBase) | TextureX,
					(Positions[CornerIndex] - TextureBase) | TextureY));
			}

			// Create a polygon with the 3 vertex instances
			
			TArray<FEdgeID> NewEdgeIDs;
			const FPolygonID NewPolygonID = MeshDescription.CreatePolygon(CurrentPolygonGroupID, VertexInstanceIDs, &NewEdgeIDs);
			for (const FEdgeID NewEdgeID : NewEdgeIDs)
			{
				//All edge are hard for BSP
				EdgeHardnesses[NewEdgeID] = true;
			}
			int32 NewTriangleIndex = MeshDescription.GetPolygonTriangles(NewPolygonID).AddDefaulted();
			FMeshTriangle& NewTriangle = MeshDescription.GetPolygonTriangles(NewPolygonID)[NewTriangleIndex];
			for (int32 TriangleVertexIndex = 0; TriangleVertexIndex < 3; ++TriangleVertexIndex)
			{
				const FVertexInstanceID &VertexInstanceID = VertexInstanceIDs[TriangleVertexIndex];
				NewTriangle.SetVertexInstanceID(TriangleVertexIndex, VertexInstanceID);
			}
		}
	}
}

//
//	CreateStaticMeshFromBrush - Creates a static mesh from the triangles in a model.
//

UStaticMesh* CreateStaticMeshFromBrush(UObject* Outer, FName Name, ABrush* Brush, UModel* Model)
{
	FScopedSlowTask SlowTask(0.0f, NSLOCTEXT("UnrealEd", "CreatingStaticMeshE", "Creating static mesh..."));
	SlowTask.MakeDialog();

	// Create the UStaticMesh object.
	FStaticMeshComponentRecreateRenderStateContext RecreateRenderStateContext(FindObject<UStaticMesh>(Outer, *Name.ToString()));
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Outer, Name, RF_Public | RF_Standalone);

	// Add one LOD for the base mesh
	FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
	const int32 LodIndex = StaticMesh->SourceModels.Num() - 1;
	FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(LodIndex);
	UStaticMesh::RegisterMeshAttributes(*MeshDescription);

	// Fill out the mesh description and materials from the brush geometry
	TArray<FStaticMaterial> Materials;
	GetBrushMesh(Brush, Model, *MeshDescription, Materials);

	// Commit mesh description and materials list to static mesh
	StaticMesh->CommitMeshDescription(LodIndex);
	StaticMesh->StaticMaterials = Materials;

	// Set up the SectionInfoMap to enable collision
	const int32 NumSections = StaticMesh->StaticMaterials.Num();
	for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
	{
		FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(0, SectionIdx);
		Info.MaterialIndex = SectionIdx;
		Info.bEnableCollision = true;
		StaticMesh->SectionInfoMap.Set(0, SectionIdx, Info);
		StaticMesh->OriginalSectionInfoMap.Set(0, SectionIdx, Info);
	}

	//Set the Imported version before calling the build
	StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

	StaticMesh->Build();
	StaticMesh->MarkPackageDirty();

	return StaticMesh;

}

// Accepts a triangle (XYZ and UV values for each point) and returns a poly base and UV vectors
// NOTE : the UV coords should be scaled by the texture size
static inline void FTexCoordsToVectors(const FVector& V0, const FVector& UV0,
									   const FVector& V1, const FVector& InUV1,
									   const FVector& V2, const FVector& InUV2,
									   FVector* InBaseResult, FVector* InUResult, FVector* InVResult )
{
	// Create polygon normal.
	FVector PN = FVector((V0-V1) ^ (V2-V0));
	PN = PN.GetSafeNormal();

	FVector UV1( InUV1 );
	FVector UV2( InUV2 );

	// Fudge UV's to make sure no infinities creep into UV vector math, whenever we detect identical U or V's.
	if( ( UV0.X == UV1.X ) || ( UV2.X == UV1.X ) || ( UV2.X == UV0.X ) ||
		( UV0.Y == UV1.Y ) || ( UV2.Y == UV1.Y ) || ( UV2.Y == UV0.Y ) )
	{
		UV1 += FVector(0.004173f,0.004123f,0.0f);
		UV2 += FVector(0.003173f,0.003123f,0.0f);
	}

	//
	// Solve the equations to find our texture U/V vectors 'TU' and 'TV' by stacking them 
	// into a 3x3 matrix , one for  u(t) = TU dot (x(t)-x(o) + u(o) and one for v(t)=  TV dot (.... , 
	// then the third assumes we're perpendicular to the normal. 
	//
	FMatrix TexEqu = FMatrix::Identity;
	TexEqu.SetAxis( 0, FVector(	V1.X - V0.X, V1.Y - V0.Y, V1.Z - V0.Z ) );
	TexEqu.SetAxis( 1, FVector( V2.X - V0.X, V2.Y - V0.Y, V2.Z - V0.Z ) );
	TexEqu.SetAxis( 2, FVector( PN.X,        PN.Y,        PN.Z        ) );
	TexEqu = TexEqu.InverseFast();

	const FVector UResult( UV1.X-UV0.X, UV2.X-UV0.X, 0.0f );
	const FVector TUResult = TexEqu.TransformVector( UResult );

	const FVector VResult( UV1.Y-UV0.Y, UV2.Y-UV0.Y, 0.0f );
	const FVector TVResult = TexEqu.TransformVector( VResult );

	//
	// Adjust the BASE to account for U0 and V0 automatically, and force it into the same plane.
	//				
	FMatrix BaseEqu = FMatrix::Identity;
	BaseEqu.SetAxis( 0, TUResult );
	BaseEqu.SetAxis( 1, TVResult ); 
	BaseEqu.SetAxis( 2, FVector( PN.X, PN.Y, PN.Z ) );
	BaseEqu = BaseEqu.InverseFast();

	const FVector BResult = FVector( UV0.X - ( TUResult|V0 ), UV0.Y - ( TVResult|V0 ),  0.0f );

	*InBaseResult = - 1.0f *  BaseEqu.TransformVector( BResult );
	*InUResult = TUResult;
	*InVResult = TVResult;

}


/**
 * Creates a model from the triangles in a static mesh.
 */
void CreateModelFromStaticMesh(UModel* Model,AStaticMeshActor* StaticMeshActor)
{
#ifdef TODO_STATICMESH
	UStaticMesh*	StaticMesh = StaticMeshActor->StaticMeshComponent->StaticMesh;
	FMatrix			ActorToWorld = StaticMeshActor->ActorToWorld().ToMatrixWithScale();

	Model->Polys->Element.Empty();

	const FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) StaticMesh->LODModels[0].RawTriangles.Lock(LOCK_READ_ONLY);
	if(StaticMesh->LODModels[0].RawTriangles.GetElementCount())
	{
		for(int32 TriangleIndex = 0;TriangleIndex < StaticMesh->LODModels[0].RawTriangles.GetElementCount();TriangleIndex++)
		{
			const FStaticMeshTriangle&	Triangle	= RawTriangleData[TriangleIndex];
			FPoly*						Polygon		= new(Model->Polys->Element) FPoly;

			Polygon->Init();
			Polygon->iLink = Polygon - Model->Polys->Element.GetData();
			Polygon->Material = StaticMesh->LODModels[0].Elements[Triangle.MaterialIndex].Material;
			Polygon->PolyFlags = PF_DefaultFlags;
			Polygon->SmoothingMask = Triangle.SmoothingMask;

			new(Polygon->Vertices) FVector(ActorToWorld.TransformPosition(Triangle.Vertices[2]));
			new(Polygon->Vertices) FVector(ActorToWorld.TransformPosition(Triangle.Vertices[1]));
			new(Polygon->Vertices) FVector(ActorToWorld.TransformPosition(Triangle.Vertices[0]));

			Polygon->CalcNormal(1);
			Polygon->Finalize(NULL,0);
			FTexCoordsToVectors(Polygon->Vertices[2],FVector(Triangle.UVs[0][0].X * UModel::GetGlobalBSPTexelScale(),Triangle.UVs[0][0].Y * UModel::GetGlobalBSPTexelScale(),1),
								Polygon->Vertices[1],FVector(Triangle.UVs[1][0].X * UModel::GetGlobalBSPTexelScale(),Triangle.UVs[1][0].Y * UModel::GetGlobalBSPTexelScale(),1),
								Polygon->Vertices[0],FVector(Triangle.UVs[2][0].X * UModel::GetGlobalBSPTexelScale(),Triangle.UVs[2][0].Y * UModel::GetGlobalBSPTexelScale(),1),
								&Polygon->Base,&Polygon->TextureU,&Polygon->TextureV);
		}
	}
	StaticMesh->LODModels[0].RawTriangles.Unlock();

	Model->Linked = 1;
	FBSPOps::bspValidateBrush(Model,0,1);
	Model->BuildBound();
#endif // #if TODO_STATICMESH
}

static void TransformPolys(UPolys* Polys,const FMatrix& Matrix)
{
	for(int32 PolygonIndex = 0;PolygonIndex < Polys->Element.Num();PolygonIndex++)
	{
		FPoly&	Polygon = Polys->Element[PolygonIndex];

		for(int32 VertexIndex = 0;VertexIndex < Polygon.Vertices.Num();VertexIndex++)
			Polygon.Vertices[VertexIndex] = Matrix.TransformPosition( Polygon.Vertices[VertexIndex] );

		Polygon.Base		= Matrix.TransformPosition( Polygon.Base		);
		Polygon.TextureU	= Matrix.TransformPosition( Polygon.TextureU );
		Polygon.TextureV	= Matrix.TransformPosition( Polygon.TextureV	);
	}

}

// LOD data to copy over
struct ExistingLODMeshData
{
	FMeshBuildSettings				ExistingBuildSettings;
	FMeshReductionSettings			ExistingReductionSettings;
	TUniquePtr<FMeshDescription>	ExistingMeshDescription;
	TArray<FStaticMaterial>			ExistingMaterials;
	FPerPlatformFloat				ExistingScreenSize;
	FString							ExistingSourceImportFilename;
};

struct ExistingStaticMeshData
{
	TArray<FStaticMaterial> 	ExistingMaterials;

	FMeshSectionInfoMap			ExistingSectionInfoMap;
	TArray<ExistingLODMeshData>	ExistingLODData;

	TArray<UStaticMeshSocket*>	ExistingSockets;

	bool						ExistingCustomizedCollision;
	bool						bAutoComputeLODScreenSize;

	int32						ExistingLightMapResolution;
	int32						ExistingLightMapCoordinateIndex;

	TWeakObjectPtr<UAssetImportData> ExistingImportData;
	TWeakObjectPtr<UThumbnailInfo> ExistingThumbnailInfo;

	UModel*						ExistingCollisionModel;
	UBodySetup*					ExistingBodySetup;

	// A mapping of vertex positions to their color in the existing static mesh
	TMap<FVector, FColor>		ExistingVertexColorData;

	float						LpvBiasMultiplier;
	bool						bHasNavigationData;
	FName						LODGroup;
	FPerPlatformInt				MinLOD;

	int32						ImportVersion;

	bool UseMaterialNameSlotWorkflow;
	//The last import material data (fbx original data before user changes)
	TArray<FName> LastImportMaterialOriginalNameData;
	TArray<TArray<FName>> LastImportMeshLodSectionMaterialData;

	bool						ExistingGenerateMeshDistanceField;
	int32						ExistingLODForCollision;
	float						ExistingDistanceFieldSelfShadowBias;
	bool						ExistingSupportUniformlyDistributedSampling;
	bool						ExistingAllowCpuAccess;
	FVector						ExistingPositiveBoundsExtension;
	FVector						ExistingNegativeBoundsExtension;
};

bool IsUsingMaterialSlotNameWorkflow(UAssetImportData* AssetImportData)
{
	UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(AssetImportData);
	if (ImportData == nullptr || ImportData->ImportMaterialOriginalNameData.Num() <= 0)
	{
		return false;
	}
	bool AllNameAreNone = true;
	for (FName ImportMaterialName : ImportData->ImportMaterialOriginalNameData)
	{
		if (ImportMaterialName != NAME_None)
		{
			AllNameAreNone = false;
			break;
		}
	}
	return !AllNameAreNone;
}

ExistingStaticMeshData* SaveExistingStaticMeshData(UStaticMesh* ExistingMesh, UnFbx::FBXImportOptions* ImportOptions, int32 LodIndex)
{
	struct ExistingStaticMeshData* ExistingMeshDataPtr = NULL;

	if (ExistingMesh)
	{
		bool bSaveMaterials = !ImportOptions->bImportMaterials;
		ExistingMeshDataPtr = new ExistingStaticMeshData();

		ExistingMeshDataPtr->ImportVersion = ExistingMesh->ImportVersion;
		ExistingMeshDataPtr->UseMaterialNameSlotWorkflow = IsUsingMaterialSlotNameWorkflow(ExistingMesh->AssetImportData);

		FMeshSectionInfoMap OldSectionInfoMap = ExistingMesh->SectionInfoMap;

		bool bIsReimportCustomLODOverGeneratedLOD = ExistingMesh->SourceModels.IsValidIndex(LodIndex) &&
			(ExistingMesh->SourceModels[LodIndex].IsRawMeshEmpty() || !(ExistingMesh->IsReductionActive(LodIndex) && ExistingMesh->SourceModels[LodIndex].ReductionSettings.BaseLODModel != LodIndex));

		//We need to reset some data in case we import a custom LOD over a generated LOD
		if (bIsReimportCustomLODOverGeneratedLOD)
		{
			//Reset the section info map for this LOD
			for (int32 SectionIndex = 0; SectionIndex < ExistingMesh->GetNumSections(LodIndex); ++SectionIndex)
			{
				OldSectionInfoMap.Remove(LodIndex, SectionIndex);
			}
		}

		ExistingMeshDataPtr->ExistingMaterials.Empty();
		if (bSaveMaterials)
		{
			for (const FStaticMaterial &StaticMaterial : ExistingMesh->StaticMaterials)
			{
				ExistingMeshDataPtr->ExistingMaterials.Add(StaticMaterial);
			}
		}

		ExistingMeshDataPtr->ExistingLODData.AddZeroed(ExistingMesh->SourceModels.Num());

		// refresh material and section info map here
		// we have to make sure it only contains valid item
		// we go through section info and only add it back if used, otherwise we don't want to use
		if (LodIndex == INDEX_NONE)
		{
			ExistingMesh->SectionInfoMap.Clear();
		}
		else
		{
			//Remove only the target section InfoMap, if we destroy more we will not restore the correct material assignment for other Lods contain in the same file.
			int32 ReimportSectionNumber = ExistingMesh->SectionInfoMap.GetSectionNumber(LodIndex);
			for (int32 SectionIndex = 0; SectionIndex < ReimportSectionNumber; ++SectionIndex)
			{
				ExistingMesh->SectionInfoMap.Remove(LodIndex, SectionIndex);
			}
		}
		int32 TotalMaterialIndex = ExistingMeshDataPtr->ExistingMaterials.Num();
		for(int32 i=0; i<ExistingMesh->SourceModels.Num(); i++)
		{
			//If the last import was exceeding the maximum number of LOD the source model will contain more LOD so just break the loop
			if (i >= ExistingMesh->RenderData->LODResources.Num())
				break;
			FStaticMeshLODResources& LOD = ExistingMesh->RenderData->LODResources[i];
			int32 NumSections = LOD.Sections.Num();
			for(int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
			{
				FMeshSectionInfo Info = OldSectionInfoMap.Get(i, SectionIndex);
				if(bSaveMaterials && ExistingMesh->StaticMaterials.IsValidIndex(Info.MaterialIndex))
				{
					if (ExistingMeshDataPtr->UseMaterialNameSlotWorkflow)
					{
						int32 ExistMaterialIndex = ExistingMeshDataPtr->ExistingMaterials.Find(ExistingMesh->StaticMaterials[Info.MaterialIndex]);
						if (ExistMaterialIndex == INDEX_NONE)
						{
							ExistMaterialIndex = ExistingMeshDataPtr->ExistingMaterials.Add(ExistingMesh->StaticMaterials[Info.MaterialIndex]);
						}
						Info.MaterialIndex = ExistMaterialIndex;
					}
					else
					{
						// we only save per LOD separeate IF the material index isn't added yet. 
						// if it's already added, we don't have to add another one. 
						if (Info.MaterialIndex >= TotalMaterialIndex)
						{
							ExistingMeshDataPtr->ExistingMaterials.Add(ExistingMesh->StaticMaterials[Info.MaterialIndex]);

							// @Todo @fixme
							// have to refresh material index since it might be pointing at wrong one
							// this will break IF the base material number grows or shoterns and index will be off
							// I think we have to save material index per section, so that we don't have to worry about global index
							Info.MaterialIndex = TotalMaterialIndex++;
						}
					}
					ExistingMeshDataPtr->ExistingSectionInfoMap.Set(i, SectionIndex, Info);
				}
			}

			ExistingMeshDataPtr->ExistingLODData[i].ExistingBuildSettings = ExistingMesh->SourceModels[i].BuildSettings;
			ExistingMeshDataPtr->ExistingLODData[i].ExistingReductionSettings = ExistingMesh->SourceModels[i].ReductionSettings;
			if (bIsReimportCustomLODOverGeneratedLOD && (i == LodIndex))
			{
				//Reset the reduction
				ExistingMeshDataPtr->ExistingLODData[i].ExistingReductionSettings.PercentTriangles = 1.0f;
				ExistingMeshDataPtr->ExistingLODData[i].ExistingReductionSettings.PercentVertices = 1.0f;
				ExistingMeshDataPtr->ExistingLODData[i].ExistingReductionSettings.MaxDeviation = 0.0f;
			}
			ExistingMeshDataPtr->ExistingLODData[i].ExistingScreenSize = ExistingMesh->SourceModels[i].ScreenSize.Default;
			ExistingMeshDataPtr->ExistingLODData[i].ExistingSourceImportFilename = ExistingMesh->SourceModels[i].SourceImportFilename;

			const FMeshDescription* MeshDescription = ExistingMesh->GetMeshDescription(i);
			if (MeshDescription)
			{
				ExistingMeshDataPtr->ExistingLODData[i].ExistingMeshDescription = MakeUnique<FMeshDescription>(*MeshDescription);
			}
		}

		ExistingMeshDataPtr->ExistingSockets = ExistingMesh->Sockets;

		ExistingMeshDataPtr->ExistingCustomizedCollision = ExistingMesh->bCustomizedCollision;
		ExistingMeshDataPtr->bAutoComputeLODScreenSize = ExistingMesh->bAutoComputeLODScreenSize;

		ExistingMeshDataPtr->ExistingLightMapResolution = ExistingMesh->LightMapResolution;
		ExistingMeshDataPtr->ExistingLightMapCoordinateIndex = ExistingMesh->LightMapCoordinateIndex;

		ExistingMeshDataPtr->ExistingImportData = ExistingMesh->AssetImportData;
		ExistingMeshDataPtr->ExistingThumbnailInfo = ExistingMesh->ThumbnailInfo;

		ExistingMeshDataPtr->ExistingBodySetup = ExistingMesh->BodySetup;

		ExistingMeshDataPtr->LpvBiasMultiplier = ExistingMesh->LpvBiasMultiplier;
		ExistingMeshDataPtr->bHasNavigationData = ExistingMesh->bHasNavigationData;
		ExistingMeshDataPtr->LODGroup = ExistingMesh->LODGroup;
		ExistingMeshDataPtr->MinLOD = ExistingMesh->MinLOD;

		ExistingMeshDataPtr->ExistingGenerateMeshDistanceField = ExistingMesh->bGenerateMeshDistanceField;
		ExistingMeshDataPtr->ExistingLODForCollision = ExistingMesh->LODForCollision;
		ExistingMeshDataPtr->ExistingDistanceFieldSelfShadowBias = ExistingMesh->DistanceFieldSelfShadowBias;
		ExistingMeshDataPtr->ExistingSupportUniformlyDistributedSampling = ExistingMesh->bSupportUniformlyDistributedSampling;
		ExistingMeshDataPtr->ExistingAllowCpuAccess = ExistingMesh->bAllowCPUAccess;
		ExistingMeshDataPtr->ExistingPositiveBoundsExtension = ExistingMesh->PositiveBoundsExtension;
		ExistingMeshDataPtr->ExistingNegativeBoundsExtension = ExistingMesh->NegativeBoundsExtension;

		UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(ExistingMesh->AssetImportData);
		if (ImportData && ExistingMeshDataPtr->UseMaterialNameSlotWorkflow)
		{
			for (int32 ImportMaterialOriginalNameDataIndex = 0; ImportMaterialOriginalNameDataIndex < ImportData->ImportMaterialOriginalNameData.Num(); ++ImportMaterialOriginalNameDataIndex)
			{
				FName MaterialName = ImportData->ImportMaterialOriginalNameData[ImportMaterialOriginalNameDataIndex];
				ExistingMeshDataPtr->LastImportMaterialOriginalNameData.Add(MaterialName);
			}
			for (int32 InternalLodIndex = 0; InternalLodIndex < ImportData->ImportMeshLodData.Num(); ++InternalLodIndex)
			{
				ExistingMeshDataPtr->LastImportMeshLodSectionMaterialData.AddZeroed();
				const FImportMeshLodSectionsData &ImportMeshLodSectionsData = ImportData->ImportMeshLodData[InternalLodIndex];
				for (int32 SectionIndex = 0; SectionIndex < ImportMeshLodSectionsData.SectionOriginalMaterialName.Num(); ++SectionIndex)
				{
					FName MaterialName = ImportMeshLodSectionsData.SectionOriginalMaterialName[SectionIndex];
					ExistingMeshDataPtr->LastImportMeshLodSectionMaterialData[InternalLodIndex].Add(MaterialName);
				}
			}
		}
	}

	return ExistingMeshDataPtr;
}

// Helper to find if some reduction settings are active
bool IsReductionActive(const FMeshReductionSettings& ReductionSettings)
{
	bool bUseQuadricSimplier = true;
	{
		// Are we using our tool, or simplygon?  The tool is only changed during editor restarts
		IMeshReduction* ReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface").GetStaticMeshReductionInterface();
		FString VersionString = ReductionModule->GetVersionString();
		TArray<FString> SplitVersionString;
		VersionString.ParseIntoArray(SplitVersionString, TEXT("_"), true);
		bUseQuadricSimplier = SplitVersionString[0].Equals("QuadricMeshReduction");
	}
	const bool bVertTermination = (bUseQuadricSimplier) && (ReductionSettings.TerminationCriterion != EStaticMeshReductionTerimationCriterion::Triangles) && (ReductionSettings.PercentVertices < 1.0f);
	const bool bTriTermination = ReductionSettings.TerminationCriterion != EStaticMeshReductionTerimationCriterion::Vertices && (ReductionSettings.PercentTriangles < 1.0f);
	return bTriTermination || bVertTermination || (ReductionSettings.MaxDeviation > 0.0f);
}

/* This function is call before building the mesh when we do a re-import*/
void RestoreExistingMeshSettings(ExistingStaticMeshData* ExistingMesh, UStaticMesh* NewMesh, int32 LODIndex)
{
	if (!ExistingMesh)
	{
		return;
	}
	NewMesh->LODGroup = ExistingMesh->LODGroup;
	NewMesh->MinLOD = ExistingMesh->MinLOD;
	int32 ExistingNumLods = ExistingMesh->ExistingLODData.Num();
	int32 CurrentNumLods = NewMesh->SourceModels.Num();
	if (LODIndex == INDEX_NONE)
	{
		if (CurrentNumLods > ExistingNumLods)
		{
			NewMesh->SetNumSourceModels(ExistingNumLods);
		}
		//Create only the LOD Group we need, extra LOD will be put back when calling RestoreExistingMeshData later in the re import process
		if (NewMesh->LODGroup != NAME_None)
		{
			ITargetPlatform* CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
			check(CurrentPlatform);
			const FStaticMeshLODGroup& LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NewMesh->LODGroup);
			ExistingNumLods = FMath::Min(ExistingNumLods, LODGroup.GetDefaultNumLODs());
		}

		for (int32 i = 0; i < ExistingNumLods; i++)
		{
			if (NewMesh->SourceModels.Num() <= i)
			{
				NewMesh->AddSourceModel();
			}
			FMeshDescription* LODMeshDescription = NewMesh->GetMeshDescription(i);
			bool bSwapFromGeneratedToImported = !ExistingMesh->ExistingLODData[i].ExistingMeshDescription.IsValid() && (LODMeshDescription && LODMeshDescription->Polygons().Num() > 0);
			bool bWasReduced = IsReductionActive(ExistingMesh->ExistingLODData[i].ExistingReductionSettings);

			if (!bSwapFromGeneratedToImported && bWasReduced)
			{
				NewMesh->SourceModels[i].ReductionSettings = ExistingMesh->ExistingLODData[i].ExistingReductionSettings;
			}
			NewMesh->SourceModels[i].BuildSettings = ExistingMesh->ExistingLODData[i].ExistingBuildSettings;
			NewMesh->SourceModels[i].ScreenSize = ExistingMesh->ExistingLODData[i].ExistingScreenSize;
			NewMesh->SourceModels[i].SourceImportFilename = ExistingMesh->ExistingLODData[i].ExistingSourceImportFilename;
		}
	}
	else
	{
		//Just set the old configuration for the desired LODIndex
		if(LODIndex >= 0 && LODIndex < CurrentNumLods && LODIndex < ExistingNumLods)
		{
			FMeshDescription* LODMeshDescription = NewMesh->GetMeshDescription(LODIndex);
			bool bSwapFromGeneratedToImported = !ExistingMesh->ExistingLODData[LODIndex].ExistingMeshDescription.IsValid() && (LODMeshDescription && LODMeshDescription->Polygons().Num() > 0);
			bool bWasReduced = IsReductionActive(ExistingMesh->ExistingLODData[LODIndex].ExistingReductionSettings);
			if (!bSwapFromGeneratedToImported && bWasReduced)
			{
				NewMesh->SourceModels[LODIndex].ReductionSettings = ExistingMesh->ExistingLODData[LODIndex].ExistingReductionSettings;
			}
			NewMesh->SourceModels[LODIndex].BuildSettings = ExistingMesh->ExistingLODData[LODIndex].ExistingBuildSettings;
			NewMesh->SourceModels[LODIndex].ScreenSize = ExistingMesh->ExistingLODData[LODIndex].ExistingScreenSize;
			NewMesh->SourceModels[LODIndex].SourceImportFilename = ExistingMesh->ExistingLODData[LODIndex].ExistingSourceImportFilename;
		}
	}

	//We need to fill the import version remap before building the mesh since the
	//static mesh component will be register at the end of the build.
	//We do the remap of the material override in the static mesh component in OnRegister()
	if(ExistingMesh->ImportVersion != EImportStaticMeshVersion::LastVersion)
	{
		uint32 MaterialMapKey = 0;
		TArray<int32> ImportRemapMaterial;
		MaterialMapKey = ((uint32)((ExistingMesh->ImportVersion & 0xffff) << 16) | (uint32)(EImportStaticMeshVersion::LastVersion & 0xffff));
		//Avoid matching a material more then once
		TArray<int32> MatchIndex;
		ImportRemapMaterial.AddZeroed(ExistingMesh->ExistingMaterials.Num());
		for (int32 ExistMaterialIndex = 0; ExistMaterialIndex < ExistingMesh->ExistingMaterials.Num(); ++ExistMaterialIndex)
		{
			ImportRemapMaterial[ExistMaterialIndex] = ExistMaterialIndex; //Set default value
			const FStaticMaterial &ExistMaterial = ExistingMesh->ExistingMaterials[ExistMaterialIndex];
			bool bFoundMatchingMaterial = false;
			for (int32 MaterialIndex = 0; MaterialIndex < NewMesh->StaticMaterials.Num(); ++MaterialIndex)
			{
				if (MatchIndex.Contains(MaterialIndex))
				{
					continue;
				}
				FStaticMaterial &Material = NewMesh->StaticMaterials[MaterialIndex];
				if (Material.ImportedMaterialSlotName == ExistMaterial.ImportedMaterialSlotName)
				{
					MatchIndex.Add(MaterialIndex);
					ImportRemapMaterial[ExistMaterialIndex] = MaterialIndex;
					bFoundMatchingMaterial = true;
					break;
				}
			}
			if (!bFoundMatchingMaterial)
			{
				for (int32 MaterialIndex = 0; MaterialIndex < NewMesh->StaticMaterials.Num(); ++MaterialIndex)
				{
					if (MatchIndex.Contains(MaterialIndex))
					{
						continue;
					}

					FStaticMaterial &Material = NewMesh->StaticMaterials[MaterialIndex];
					if (ExistMaterial.ImportedMaterialSlotName == NAME_None && Material.MaterialInterface == ExistMaterial.MaterialInterface)
					{
						MatchIndex.Add(MaterialIndex);
						ImportRemapMaterial[ExistMaterialIndex] = MaterialIndex;
						bFoundMatchingMaterial = true;
						break;
					}
				}
			}
			if (!bFoundMatchingMaterial)
			{
				ImportRemapMaterial[ExistMaterialIndex] = ExistMaterialIndex;
			}
		}
		NewMesh->MaterialRemapIndexPerImportVersion.Add(FMaterialRemapIndex(MaterialMapKey, ImportRemapMaterial));
	}
}

void UpdateSomeLodsImportMeshData(UStaticMesh* NewMesh, TArray<int32> *ReimportLodList)
{
	if (NewMesh == nullptr)
	{
		return;
	}
	UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(NewMesh->AssetImportData);
	//Update the LOD import data before restoring the data
	if (ReimportLodList != nullptr && ImportData != nullptr)
	{
		for (int32 LodLevelImport : (*ReimportLodList))
		{
			if (!ImportData->ImportMeshLodData.IsValidIndex(LodLevelImport))
			{
				ImportData->ImportMeshLodData.AddZeroed(LodLevelImport - ImportData->ImportMeshLodData.Num() + 1);
			}
			ImportData->ImportMeshLodData[LodLevelImport].SectionOriginalMaterialName.Empty();
			if (NewMesh->RenderData->LODResources.IsValidIndex(LodLevelImport))
			{
				FStaticMeshLODResources& LOD = NewMesh->RenderData->LODResources[LodLevelImport];
				int32 NumSections = LOD.Sections.Num();
				for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
				{
					int32 MaterialLodSectionIndex = LOD.Sections[SectionIndex].MaterialIndex;
					if (NewMesh->SectionInfoMap.IsValidSection(LodLevelImport, SectionIndex))
					{
						MaterialLodSectionIndex = NewMesh->SectionInfoMap.Get(LodLevelImport, SectionIndex).MaterialIndex;
					}

					if (NewMesh->StaticMaterials.IsValidIndex(MaterialLodSectionIndex))
					{
						bool bFoundMatch = false;
						FName OriginalImportName = NewMesh->StaticMaterials[MaterialLodSectionIndex].ImportedMaterialSlotName;
						//Find the material in the original import data
						int32 ImportMaterialIndex = 0;
						for (ImportMaterialIndex = 0; ImportMaterialIndex < ImportData->ImportMaterialOriginalNameData.Num(); ++ImportMaterialIndex)
						{
							if (ImportData->ImportMaterialOriginalNameData[ImportMaterialIndex] == OriginalImportName)
							{
								bFoundMatch = true;
								break;
							}
						}
						if (!bFoundMatch)
						{
							ImportMaterialIndex = ImportData->ImportMaterialOriginalNameData.Add(OriginalImportName);
						}
						ImportData->ImportMeshLodData[LodLevelImport].SectionOriginalMaterialName.Add(ImportData->ImportMaterialOriginalNameData[ImportMaterialIndex]);
					}
					else
					{
						ImportData->ImportMeshLodData[LodLevelImport].SectionOriginalMaterialName.Add(TEXT("InvalidMaterialIndex"));
					}
				}
			}
		}
	}
}

void RestoreExistingMeshData(ExistingStaticMeshData* ExistingMeshDataPtr, UStaticMesh* NewMesh, int32 LodLevel, bool bCanShowDialog)
{
	if (!ExistingMeshDataPtr || !NewMesh)
	{
		delete ExistingMeshDataPtr;
		return;
	}

	//Create a remap material Index use to find the matching section later
	TArray<int32> RemapMaterial;
	RemapMaterial.AddZeroed(NewMesh->StaticMaterials.Num());
	TArray<FName> RemapMaterialName;
	RemapMaterialName.AddZeroed(NewMesh->StaticMaterials.Num());

	//If user is attended, ask him to verify the match is good
	UnFbx::EFBXReimportDialogReturnOption ReturnOption;
	//Ask the user to match the materials conflict
	UnFbx::FFbxImporter::PrepareAndShowMaterialConflictDialog<FStaticMaterial>(ExistingMeshDataPtr->ExistingMaterials, NewMesh->StaticMaterials, RemapMaterial, RemapMaterialName, bCanShowDialog, false, ReturnOption);
	
	if (ReturnOption != UnFbx::EFBXReimportDialogReturnOption::FBXRDRO_ResetToFbx)
	{
		//Build a ordered material list that try to keep intact the existing material list
		TArray<FStaticMaterial> MaterialOrdered;
		TArray<bool> MatchedNewMaterial;
		MatchedNewMaterial.AddZeroed(NewMesh->StaticMaterials.Num());
		for (int32 ExistMaterialIndex = 0; ExistMaterialIndex < ExistingMeshDataPtr->ExistingMaterials.Num(); ++ExistMaterialIndex)
		{
			int32 MaterialIndexOrdered = MaterialOrdered.Add(ExistingMeshDataPtr->ExistingMaterials[ExistMaterialIndex]);
			FStaticMaterial& OrderedMaterial = MaterialOrdered[MaterialIndexOrdered];
			int32 NewMaterialIndex = INDEX_NONE;
			if (RemapMaterial.Find(ExistMaterialIndex, NewMaterialIndex))
			{
				MatchedNewMaterial[NewMaterialIndex] = true;
				RemapMaterial[NewMaterialIndex] = MaterialIndexOrdered;
				OrderedMaterial.ImportedMaterialSlotName = NewMesh->StaticMaterials[NewMaterialIndex].ImportedMaterialSlotName;
			}
			else
			{
				//Unmatched material must be conserve
			}
		}

		//Add the new material entries (the one that do not match with any existing material)
		for (int32 NewMaterialIndex = 0; NewMaterialIndex < MatchedNewMaterial.Num(); ++NewMaterialIndex)
		{
			if (MatchedNewMaterial[NewMaterialIndex] == false)
			{
				int32 NewMeshIndex = MaterialOrdered.Add(NewMesh->StaticMaterials[NewMaterialIndex]);
				RemapMaterial[NewMaterialIndex] = NewMeshIndex;
			}
		}

		//Set the RemapMaterialName array helper
		for (int32 MaterialIndex = 0; MaterialIndex < RemapMaterial.Num(); ++MaterialIndex)
		{
			int32 SourceMaterialMatch = RemapMaterial[MaterialIndex];
			if (ExistingMeshDataPtr->ExistingMaterials.IsValidIndex(SourceMaterialMatch))
			{
				RemapMaterialName[MaterialIndex] = ExistingMeshDataPtr->ExistingMaterials[SourceMaterialMatch].ImportedMaterialSlotName;
			}
		}

		//Copy the re ordered materials (this ensure the material array do not change when we re-import)
		NewMesh->StaticMaterials = MaterialOrdered;
	}
	int32 NumCommonLODs = FMath::Min<int32>(ExistingMeshDataPtr->ExistingLODData.Num(), NewMesh->SourceModels.Num());
	for(int32 i=0; i<NumCommonLODs; i++)
	{
		NewMesh->SourceModels[i].BuildSettings = ExistingMeshDataPtr->ExistingLODData[i].ExistingBuildSettings;
		FMeshDescription* LODMeshDescription = NewMesh->GetMeshDescription(i);
		//Restore the reduction settings only if the existing data was a using reduction. Because we can set some value if we reimport from existing rawmesh to auto generated.
		bool bSwapFromGeneratedToImported = !ExistingMeshDataPtr->ExistingLODData[i].ExistingMeshDescription.IsValid() && (LODMeshDescription && LODMeshDescription->Polygons().Num() > 0);
		bool bWasReduced = IsReductionActive(ExistingMeshDataPtr->ExistingLODData[i].ExistingReductionSettings);
		if ( !bSwapFromGeneratedToImported && bWasReduced)
		{
			NewMesh->SourceModels[i].ReductionSettings = ExistingMeshDataPtr->ExistingLODData[i].ExistingReductionSettings;
		}
		NewMesh->SourceModels[i].ScreenSize = ExistingMeshDataPtr->ExistingLODData[i].ExistingScreenSize;
		NewMesh->SourceModels[i].SourceImportFilename = ExistingMeshDataPtr->ExistingLODData[i].ExistingSourceImportFilename;
	}

	for(int32 i=NumCommonLODs; i < ExistingMeshDataPtr->ExistingLODData.Num(); ++i)
	{
		FStaticMeshSourceModel& SrcModel = NewMesh->AddSourceModel();
		if (ExistingMeshDataPtr->ExistingLODData[i].ExistingMeshDescription.IsValid())
		{
			FMeshDescription* MeshDescription = NewMesh->CreateMeshDescription(i);
			*MeshDescription = MoveTemp(*ExistingMeshDataPtr->ExistingLODData[i].ExistingMeshDescription);
			ExistingMeshDataPtr->ExistingLODData[i].ExistingMeshDescription.Reset();
			NewMesh->CommitMeshDescription(i);
		}
		SrcModel.BuildSettings = ExistingMeshDataPtr->ExistingLODData[i].ExistingBuildSettings;
		SrcModel.ReductionSettings = ExistingMeshDataPtr->ExistingLODData[i].ExistingReductionSettings;
		SrcModel.ScreenSize = ExistingMeshDataPtr->ExistingLODData[i].ExistingScreenSize;
		SrcModel.SourceImportFilename = ExistingMeshDataPtr->ExistingLODData[i].ExistingSourceImportFilename;
	}

	// Restore the section info of the just imported LOD so is section info map is remap to fit the mesh material array
	if (ExistingMeshDataPtr->ExistingSectionInfoMap.Map.Num() > 0)
	{
		//Build the mesh we need the render data and the existing section info map build before restoring the data
		if (NewMesh->RenderData->LODResources.Num() < NewMesh->SourceModels.Num())
		{
			NewMesh->Build();
		}
		for (int32 i = 0; i < NewMesh->RenderData->LODResources.Num(); i++)
		{
			//If a LOD was specified, only touch the specified LOD
			if (LodLevel != INDEX_NONE && LodLevel != 0 && LodLevel != i)
			{
				continue;
			}

			//When re-importing the asset, do not touch the LOD that was imported from file, the material array is keep intact so the section should still be valid.
			bool NoRemapForThisLOD = LodLevel == INDEX_NONE && i != 0 && !IsReductionActive(NewMesh->SourceModels[i].ReductionSettings);

			FStaticMeshLODResources& LOD = NewMesh->RenderData->LODResources[i];
			
			int32 NumSections = LOD.Sections.Num();
			int32 OldSectionNumber = ExistingMeshDataPtr->ExistingSectionInfoMap.GetSectionNumber(i);
			for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
			{
				//If the SectionInfoMap is not set yet. Because we re-import LOD 0 but we have other LODs
				//Just put back the old section Info Map
				if (NewMesh->SectionInfoMap.GetSectionNumber(i) <= SectionIndex)
				{
					NewMesh->SectionInfoMap.Set(i, SectionIndex, ExistingMeshDataPtr->ExistingSectionInfoMap.Get(i, SectionIndex));
				}
				//We recreate the SectionInfoMap from the existing data and we do not remap it if LOD is not auto generate and was not imported
				if (NoRemapForThisLOD)
				{
					continue;
				}

				FMeshSectionInfo NewSectionInfo =  NewMesh->SectionInfoMap.Get(i, SectionIndex);
				bool bFoundOldMatch = false;
				bool bKeepOldSectionMaterialIndex = false;
				int32 OriginalSectionMaterialIndex = INDEX_NONE;
				if (RemapMaterial.IsValidIndex(NewSectionInfo.MaterialIndex) && NewMesh->StaticMaterials.IsValidIndex(NewSectionInfo.MaterialIndex))
				{
					//Find the matching old index
					for (int32 ExistSectionIndex = 0; ExistSectionIndex < OldSectionNumber; ++ExistSectionIndex)
					{
						bKeepOldSectionMaterialIndex = false;
						OriginalSectionMaterialIndex = INDEX_NONE;
						FMeshSectionInfo OldSectionInfo = ExistingMeshDataPtr->ExistingSectionInfoMap.Get(i, ExistSectionIndex);
						if (ExistingMeshDataPtr->UseMaterialNameSlotWorkflow)
						{
							if (ExistingMeshDataPtr->ExistingImportData.IsValid() &&
								ExistingMeshDataPtr->LastImportMeshLodSectionMaterialData.IsValidIndex(i) &&
								ExistingMeshDataPtr->LastImportMeshLodSectionMaterialData[i].IsValidIndex(ExistSectionIndex))
							{
								//Keep the old section material index only if the user has change the section mapping
								bKeepOldSectionMaterialIndex = ExistingMeshDataPtr->LastImportMeshLodSectionMaterialData[i][ExistSectionIndex] != ExistingMeshDataPtr->ExistingMaterials[OldSectionInfo.MaterialIndex].ImportedMaterialSlotName;
								for (int32 ExistMaterialIndex = 0; ExistMaterialIndex < ExistingMeshDataPtr->ExistingMaterials.Num(); ++ExistMaterialIndex)
								{
									if (ExistingMeshDataPtr->LastImportMeshLodSectionMaterialData[i][ExistSectionIndex] == ExistingMeshDataPtr->ExistingMaterials[ExistMaterialIndex].ImportedMaterialSlotName)
									{
										OriginalSectionMaterialIndex = ExistMaterialIndex;
										break;
									}
								}
							}
						}
						int32 OldSectionMatchIndex = OriginalSectionMaterialIndex != INDEX_NONE ? OriginalSectionMaterialIndex : OldSectionInfo.MaterialIndex;
						if (RemapMaterial[NewSectionInfo.MaterialIndex] == OldSectionMatchIndex)
						{
							NewMesh->SectionInfoMap.Set(i, SectionIndex, OldSectionInfo);
							bFoundOldMatch = true;
							break;
						}
					}
				}

				if (!bFoundOldMatch)
				{
					if (RemapMaterial.IsValidIndex(NewSectionInfo.MaterialIndex))
					{
						//Find the old section that was using the NewSectionInfo.MaterialIndex
						//This will allow copying the section information: Cast Shadow, enable collision
						for (int32 ExistSectionIndex = 0; ExistSectionIndex < OldSectionNumber; ++ExistSectionIndex)
						{
							FMeshSectionInfo OldSectionInfo = ExistingMeshDataPtr->ExistingSectionInfoMap.Get(i, ExistSectionIndex);
							if (NewSectionInfo.MaterialIndex == OldSectionInfo.MaterialIndex)
							{
								NewSectionInfo.bCastShadow = OldSectionInfo.bCastShadow;
								NewSectionInfo.bEnableCollision = OldSectionInfo.bEnableCollision;
								break;
							}
						}
						//If user has change the section info map, we want to keep the change
						NewSectionInfo.MaterialIndex = RemapMaterial[NewSectionInfo.MaterialIndex];
						NewMesh->SectionInfoMap.Set(i, SectionIndex, NewSectionInfo);
					}
				}
			}
		}
		//Store the just imported section info map
		NewMesh->OriginalSectionInfoMap.CopyFrom(NewMesh->SectionInfoMap);
	}

	// Assign sockets from old version of this StaticMesh.
	for(int32 i=0; i<ExistingMeshDataPtr->ExistingSockets.Num(); i++)
	{
		UStaticMeshSocket* ExistingSocket = ExistingMeshDataPtr->ExistingSockets[i];
		UStaticMeshSocket* Socket = NewMesh->FindSocket(ExistingSocket->SocketName);
		if (!Socket && !ExistingSocket->bSocketCreatedAtImport)
		{
			NewMesh->Sockets.Add(ExistingSocket);
		}
	}

	NewMesh->bCustomizedCollision = ExistingMeshDataPtr->ExistingCustomizedCollision;
	NewMesh->bAutoComputeLODScreenSize = ExistingMeshDataPtr->bAutoComputeLODScreenSize;

	NewMesh->LightMapResolution = ExistingMeshDataPtr->ExistingLightMapResolution;
	NewMesh->LightMapCoordinateIndex = ExistingMeshDataPtr->ExistingLightMapCoordinateIndex;

	if (ExistingMeshDataPtr->ExistingImportData.IsValid())
	{
		//RestoredLods
		UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(NewMesh->AssetImportData);
		TArray<FName> ImportMaterialOriginalNameData;
		TArray<FImportMeshLodSectionsData> ImportMeshLodData;
		if (ImportData != nullptr && ImportData->ImportMaterialOriginalNameData.Num() > 0 && ImportData->ImportMeshLodData.Num() > 0)
		{
			ImportMaterialOriginalNameData = ImportData->ImportMaterialOriginalNameData;
			ImportMeshLodData = ImportData->ImportMeshLodData;
		}

		NewMesh->AssetImportData = ExistingMeshDataPtr->ExistingImportData.Get();

		ImportData = Cast<UFbxStaticMeshImportData>(NewMesh->AssetImportData);
		if (ImportData != nullptr && ImportMaterialOriginalNameData.Num() > 0 && ImportMeshLodData.Num() > 0)
		{
			ImportData->ImportMaterialOriginalNameData = ImportMaterialOriginalNameData;
			ImportData->ImportMeshLodData = ImportMeshLodData;
		}
	}
		

	NewMesh->ThumbnailInfo = ExistingMeshDataPtr->ExistingThumbnailInfo.Get();

	// If we already had some collision info...
	if(ExistingMeshDataPtr->ExistingBodySetup)
	{
		// If we didn't import anything, always keep collision.
		bool bKeepCollision;
		if(!NewMesh->BodySetup || NewMesh->BodySetup->AggGeom.GetElementCount() == 0)
		{
			bKeepCollision = true;
		}

		else
		{
			bKeepCollision = false;
		}

		if(bKeepCollision)
		{
			NewMesh->BodySetup = ExistingMeshDataPtr->ExistingBodySetup;
		}
		else
		{
			// New collision geometry, but we still want the original settings
			NewMesh->BodySetup->CopyBodySetupProperty(ExistingMeshDataPtr->ExistingBodySetup);
		}
	}

	NewMesh->LpvBiasMultiplier = ExistingMeshDataPtr->LpvBiasMultiplier;
	NewMesh->bHasNavigationData = ExistingMeshDataPtr->bHasNavigationData;
	NewMesh->LODGroup = ExistingMeshDataPtr->LODGroup;

	NewMesh->bGenerateMeshDistanceField = ExistingMeshDataPtr->ExistingGenerateMeshDistanceField;
	NewMesh->LODForCollision = ExistingMeshDataPtr->ExistingLODForCollision;
	NewMesh->DistanceFieldSelfShadowBias = ExistingMeshDataPtr->ExistingDistanceFieldSelfShadowBias;
	NewMesh->bSupportUniformlyDistributedSampling = ExistingMeshDataPtr->ExistingSupportUniformlyDistributedSampling;
	NewMesh->bAllowCPUAccess = ExistingMeshDataPtr->ExistingAllowCpuAccess;
	NewMesh->PositiveBoundsExtension = ExistingMeshDataPtr->ExistingPositiveBoundsExtension;
	NewMesh->NegativeBoundsExtension = ExistingMeshDataPtr->ExistingNegativeBoundsExtension;

	delete ExistingMeshDataPtr;
}

#undef LOCTEXT_NAMESPACE
