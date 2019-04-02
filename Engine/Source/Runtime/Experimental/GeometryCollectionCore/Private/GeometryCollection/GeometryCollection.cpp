// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: FGeometryCollection methods.
=============================================================================*/

#include "GeometryCollection/GeometryCollection.h"

#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/GeometryCollectionProximityUtility.h"

#include <iostream>
#include <fstream>

DEFINE_LOG_CATEGORY_STATIC(FGeometryCollectionLogging, NoLogging, All);

// @todo: update names 
// const FName FGeometryCollection::FacesGroup = "Faces";
// const FName FGeometryCollection::GeometryGroup = "Geometry";
//
const FName FGeometryCollection::VerticesGroup = "Vertices";
const FName FGeometryCollection::FacesGroup = "Geometry";
const FName FGeometryCollection::GeometryGroup = "Structure";
const FName FGeometryCollection::BreakingGroup = "Breaking";
const FName FGeometryCollection::MaterialGroup = "Material";


FGeometryCollection::FGeometryCollection()
	: FTransformCollection()
	, Vertex(new TManagedArray<FVector>())
	, UV(new TManagedArray<FVector2D>())
	, Color(new TManagedArray<FLinearColor>())
	, TangentU(new TManagedArray<FVector>())
	, TangentV(new TManagedArray<FVector>())
	, Normal(new TManagedArray<FVector>())
	, BoneMap(new TManagedArray<int32>())
	, Indices(new TManagedArray<FIntVector>())
	, Visible(new TManagedArray<bool>())
	, MaterialIndex(new TManagedArray<int32>())
	, MaterialID(new TManagedArray<int32>())
	, TransformIndex(new TManagedArray<int32>())
	, BoundingBox( new TManagedArray<FBox>())
	, InnerRadius(new TManagedArray<float>())
	, OuterRadius(new TManagedArray<float>())
	, VertexStart(new TManagedArray<int32>())
	, VertexCount(new TManagedArray<int32>())
	, FaceStart(new TManagedArray<int32>())
	, FaceCount(new TManagedArray<int32>())
	, Proximity(new TManagedArray<TSet<int32>>())
	, BreakingFaceIndex(new TManagedArray<int32>())
	, BreakingSourceTransformIndex(new TManagedArray<int32>())
	, BreakingTargetTransformIndex(new TManagedArray<int32>())
	, BreakingRegionCentroid(new TManagedArray<FVector>())
	, BreakingRegionNormal(new TManagedArray<FVector>())
	, BreakingRegionRadius(new TManagedArray<float>())
	, Sections(new TManagedArray<FGeometryCollectionSection>())
{
	Construct();
}


FGeometryCollection::FGeometryCollection( FGeometryCollection& GeometryCollectionIn)
	: FTransformCollection(GeometryCollectionIn)
	, Vertex(GeometryCollectionIn.Vertex)
	, UV(GeometryCollectionIn.UV)
	, Color(GeometryCollectionIn.Color)
	, TangentU(GeometryCollectionIn.TangentU)
	, TangentV(GeometryCollectionIn.TangentV)
	, Normal(GeometryCollectionIn.Normal)
	, BoneMap(GeometryCollectionIn.BoneMap)
	, Indices(GeometryCollectionIn.Indices)
	, Visible(GeometryCollectionIn.Visible)
	, MaterialIndex(GeometryCollectionIn.MaterialIndex)
	, MaterialID(GeometryCollectionIn.MaterialID)
	, TransformIndex(GeometryCollectionIn.TransformIndex)
	, BoundingBox(GeometryCollectionIn.BoundingBox)
	, InnerRadius(GeometryCollectionIn.InnerRadius)
	, OuterRadius(GeometryCollectionIn.OuterRadius)
	, VertexStart(GeometryCollectionIn.VertexStart)
	, VertexCount(GeometryCollectionIn.VertexCount)
	, FaceStart(GeometryCollectionIn.FaceStart)
	, FaceCount(GeometryCollectionIn.FaceCount)
	, Proximity(GeometryCollectionIn.Proximity)
	, BreakingFaceIndex(GeometryCollectionIn.BreakingFaceIndex)
	, BreakingSourceTransformIndex(GeometryCollectionIn.BreakingSourceTransformIndex)
	, BreakingTargetTransformIndex(GeometryCollectionIn.BreakingTargetTransformIndex)
	, BreakingRegionCentroid(GeometryCollectionIn.BreakingRegionCentroid)
	, BreakingRegionNormal(GeometryCollectionIn.BreakingRegionNormal)
	, BreakingRegionRadius(GeometryCollectionIn.BreakingRegionRadius)
	, Sections(GeometryCollectionIn.Sections)
{
}


void FGeometryCollection::Construct()
{
	FManagedArrayCollection::FConstructionParameters TransformDependency(FTransformCollection::TransformGroup);
	FManagedArrayCollection::FConstructionParameters VerticesDependency(FGeometryCollection::VerticesGroup);
	FManagedArrayCollection::FConstructionParameters FacesDependency(FGeometryCollection::FacesGroup);
	FManagedArrayCollection::FConstructionParameters GeometryDependency(FGeometryCollection::GeometryGroup);

	// Vertices Group
	AddAttribute<FVector>("Vertex", FGeometryCollection::VerticesGroup, Vertex);
	AddAttribute<FVector>("Normal", FGeometryCollection::VerticesGroup, Normal);
	AddAttribute<FVector2D>("UV", FGeometryCollection::VerticesGroup, UV);
	AddAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup, Color);
	AddAttribute<FVector>("TangentU", FGeometryCollection::VerticesGroup, TangentU);
	AddAttribute<FVector>("TangentV", FGeometryCollection::VerticesGroup, TangentV);
	AddAttribute<int32>("BoneMap", FGeometryCollection::VerticesGroup, BoneMap, TransformDependency);

	// Faces Group
	AddAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup, Indices, VerticesDependency);
	AddAttribute<bool>("Visible", FGeometryCollection::FacesGroup, Visible);
	AddAttribute<int32>("MaterialIndex", FGeometryCollection::FacesGroup, MaterialIndex);
	AddAttribute<int32>("MaterialID", FGeometryCollection::FacesGroup, MaterialID);

	// Geometry Group
	AddAttribute<int32>("TransformIndex", FGeometryCollection::GeometryGroup, TransformIndex, TransformDependency);
	AddAttribute<FBox>("BoundingBox", FGeometryCollection::GeometryGroup, BoundingBox);
	AddAttribute<float>("InnerRadius", FGeometryCollection::GeometryGroup, InnerRadius);
	AddAttribute<float>("OuterRadius", FGeometryCollection::GeometryGroup, OuterRadius);
	AddAttribute<int32>("VertexStart", FGeometryCollection::GeometryGroup, VertexStart, VerticesDependency);
	AddAttribute<int32>("VertexCount", FGeometryCollection::GeometryGroup, VertexCount);
	AddAttribute<int32>("FaceStart", FGeometryCollection::GeometryGroup, FaceStart, FacesDependency);
	AddAttribute<int32>("FaceCount", FGeometryCollection::GeometryGroup, FaceCount);
	AddAttribute<TSet<int32>>("Proximity", FGeometryCollection::GeometryGroup, Proximity, GeometryDependency);

	// Breaking Group
	AddAttribute<int32>("BreakingFaceIndex", FGeometryCollection::BreakingGroup, BreakingFaceIndex);
	AddAttribute<int32>("BreakingSourceTransformIndex", FGeometryCollection::BreakingGroup, BreakingSourceTransformIndex);
	AddAttribute<int32>("BreakingTargetTransformIndex", FGeometryCollection::BreakingGroup, BreakingTargetTransformIndex);
	AddAttribute<FVector>("BreakingRegionCentroid", FGeometryCollection::BreakingGroup, BreakingRegionCentroid);
	AddAttribute<FVector>("BreakingRegionNormal", FGeometryCollection::BreakingGroup, BreakingRegionNormal);
	AddAttribute<float>("BreakingRegionRadius", FGeometryCollection::BreakingGroup, BreakingRegionRadius);

	// Material Group
	AddAttribute<FGeometryCollectionSection>("Sections", FGeometryCollection::MaterialGroup, Sections, FacesDependency);
}

int32 FGeometryCollection::AppendGeometry(const FGeometryCollection & Element)
{
	// until we support a transform hierarchy this is just one.
	check(Element.NumElements(FGeometryCollection::TransformGroup) == 1);

	// This calls AddElements(1, TransformGroup)
	int32 NewTransformIndex = Super::AppendTransform(Element);

	check(Element.NumElements(FGeometryCollection::FacesGroup) > 0);
	check(Element.NumElements(FGeometryCollection::VerticesGroup) > 0);


	int NumNewVertices = Element.NumElements(FGeometryCollection::VerticesGroup);
	const TManagedArray<FVector>& ElementVertices = *Element.Vertex;
	const TManagedArray<FVector>& ElementNormals = *Element.Normal;
	const TManagedArray<FVector2D>& ElementUVs = *Element.UV;
	const TManagedArray<FLinearColor>& ElementColors = *Element.Color;
	const TManagedArray<FVector>& ElementTangentUs = *Element.TangentU;
	const TManagedArray<FVector>& ElementTangentVs = *Element.TangentV;
	const TManagedArray<int32>& ElementBoneMap = *Element.BoneMap;

	const TManagedArray<FIntVector>& ElementIndices = *Element.Indices;
	const TManagedArray<bool>& ElementVisible = *Element.Visible;
	const TManagedArray<int32>& ElementMaterialIndex = *Element.MaterialIndex;
	const TManagedArray<int32>& ElementMaterialID = *Element.MaterialID;

	const TManagedArray<int32>& ElementTransformIndex = *Element.TransformIndex;
	const TManagedArray<FBox>& ElementBoundingBox = *Element.BoundingBox;
	const TManagedArray<float>& ElementInnerRadius = *Element.InnerRadius;
	const TManagedArray<float>& ElementOuterRadius = *Element.OuterRadius;
	const TManagedArray<int32>& ElementVertexStart = *Element.VertexStart;
	const TManagedArray<int32>& ElementVertexCount = *Element.VertexCount;
	const TManagedArray<int32>& ElementFaceStart = *Element.FaceStart;
	const TManagedArray<int32>& ElementFaceCount = *Element.FaceCount;

	const TManagedArray<FTransform>& ElementTransform = *Element.Transform;
	const TManagedArray<FString>& ElementBoneName = *Element.BoneName;
	const TManagedArray<FGeometryCollectionBoneNode>& ElementBoneHierarchy = *Element.BoneHierarchy;

	const TManagedArray<FGeometryCollectionSection>& ElementSections = *Element.Sections;

	// --- VERTICES GROUP ---

	int NumVertices = NumElements(FGeometryCollection::VerticesGroup);
	int VerticesIndex = AddElements(NumNewVertices, FGeometryCollection::VerticesGroup);
	TManagedArray<FVector>& Vertices = *Vertex;
	TManagedArray<FVector>& Normals = *Normal;
	TManagedArray<FVector2D>& UVs = *UV;
	TManagedArray<FLinearColor>& Colors = *Color;
	TManagedArray<FVector>& TangentUs = *TangentU;
	TManagedArray<FVector>& TangentVs = *TangentV;
	TManagedArray<int32>& BoneMaps = *BoneMap;
	TManagedArray<FIntVector>& FaceIndices = *Indices;

	for (int vdx = 0; vdx < NumNewVertices; vdx++)
	{
		Vertices[VerticesIndex + vdx] = ElementVertices[vdx];
		Normals[VerticesIndex + vdx] = ElementNormals[vdx];
		UVs[VerticesIndex + vdx] = ElementUVs[vdx];
		Colors[VerticesIndex + vdx] = ElementColors[vdx];
		TangentUs[VerticesIndex + vdx] = ElementTangentUs[vdx];
		TangentVs[VerticesIndex + vdx] = ElementTangentVs[vdx];
		BoneMaps[VerticesIndex + vdx] = NewTransformIndex;
	}

	// --- FACES GROUP ---

	TManagedArray<FIntVector>&	IndicesArray = *Indices;
	TManagedArray<bool>& VisibleArray = *Visible;
	TManagedArray<int32>& MaterialIndexArray = *MaterialIndex;
	TManagedArray<int32>& MaterialIDArray = *MaterialID;

	int NumIndices = NumElements(FGeometryCollection::FacesGroup);
	int NumNewIndices = ElementIndices.Num();
	int IndicesIndex = AddElements(NumNewIndices, FGeometryCollection::FacesGroup);
	for (int32 tdx = 0; tdx < NumNewIndices; tdx++)
	{
		IndicesArray[IndicesIndex + tdx] = FIntVector(VerticesIndex, VerticesIndex, VerticesIndex) + ElementIndices[tdx];
		VisibleArray[IndicesIndex + tdx] = ElementVisible[tdx];
		MaterialIndexArray[IndicesIndex + tdx] = ElementMaterialIndex[tdx];
		MaterialIDArray[IndicesIndex + tdx] = ElementMaterialID[tdx];		
	}

	// --- GEOMETRY GROUP ---

	TManagedArray<int32>& TransformIndexArray = *TransformIndex;
	TManagedArray<FBox>& BoundingBoxArray = *BoundingBox;
	TManagedArray<float>& InnerRadiusArray = *InnerRadius;
	TManagedArray<float>& OuterRadiusArray = *OuterRadius;	
	TManagedArray<int32>& VertexStartArray = *VertexStart;
	TManagedArray<int32>& VertexCountArray = *VertexCount;
	TManagedArray<int32>& FaceStartArray = *FaceStart;
	TManagedArray<int32>& FaceCountArray = *FaceCount;

	check(ElementTransformIndex.Num() <= 1); // until we support a transform hierarchy this is just one. 
	int GeometryIndex = AddElements(1, FGeometryCollection::GeometryGroup);
	if (ElementTransformIndex.Num() == 1)
	{
		TransformIndexArray[GeometryIndex] = BoneMaps[VerticesIndex];
		BoundingBoxArray[GeometryIndex] = ElementBoundingBox[0];
		InnerRadiusArray[GeometryIndex] = ElementInnerRadius[0];
		OuterRadiusArray[GeometryIndex] = ElementOuterRadius[0];
		FaceStartArray[GeometryIndex] = NumIndices + ElementFaceStart[0];
		FaceCountArray[GeometryIndex] = ElementFaceCount[0];
		VertexStartArray[GeometryIndex] = NumVertices + ElementVertexStart[0];
		VertexCountArray[GeometryIndex] = ElementVertexCount[0];
	}
	else // Element input failed to create a geometry group
	{
		// Compute BoundingBox 
		BoundingBoxArray[GeometryIndex] = FBox(ForceInitToZero);
		TransformIndexArray[GeometryIndex] = BoneMaps[VerticesIndex];
		VertexStartArray[GeometryIndex] = VerticesIndex;
		VertexCountArray[GeometryIndex] = NumNewVertices;
		FaceStartArray[GeometryIndex] = IndicesIndex;
		FaceCountArray[GeometryIndex] = NumNewIndices;

		// Bounding Box
		for (int vdx = VerticesIndex; vdx < VerticesIndex+NumNewVertices; vdx++)
		{
			BoundingBoxArray[GeometryIndex] += Vertices[vdx];
		}

		// Find average particle
		// @todo (CenterOfMass) : This need to be the center of mass instead
		FVector Center(0);
		for (int vdx = VerticesIndex; vdx <  VerticesIndex + NumNewVertices; vdx++)
		{
			Center += Vertices[vdx];
		}
		if (NumNewVertices) Center /= NumNewVertices;

		//
		//  Inner/Outer Radius
		//
		{
			TManagedArray<float>& InnerR = *InnerRadius;
			TManagedArray<float>& OuterR = *OuterRadius;

			// init the radius arrays
			InnerR[GeometryIndex] = FLT_MAX;
			OuterR[GeometryIndex] = -FLT_MAX;

			// Vertices
			for (int vdx = VerticesIndex; vdx < VerticesIndex + NumNewVertices; vdx++)
			{
				float Delta = (Center - Vertices[vdx]).Size();
				InnerR[GeometryIndex] = FMath::Min(InnerR[GeometryIndex], Delta);
				OuterR[GeometryIndex] = FMath::Max(OuterR[GeometryIndex], Delta);
			}


			// Inner/Outer centroid
			for (int fdx = IndicesIndex; fdx < IndicesIndex+NumNewIndices; fdx++)
			{
				FVector Centroid(0);
				for (int e = 0; e < 3; e++)
				{
					Centroid += Vertices[FaceIndices[fdx][e]];
				}
				Centroid /= 3;

				float Delta = (Center - Centroid).Size();
				InnerR[GeometryIndex] = FMath::Min(InnerR[GeometryIndex], Delta);
				OuterR[GeometryIndex] = FMath::Max(OuterR[GeometryIndex], Delta);
			}

			// Inner/Outer edges
			for (int fdx = IndicesIndex; fdx < IndicesIndex + NumNewIndices; fdx++)
			{
				for (int e = 0; e < 3; e++)
				{
					int i = e, j = (e + 1) % 3;
					FVector Edge = Vertices[FaceIndices[fdx][i]] + 0.5*(Vertices[FaceIndices[fdx][j]] - Vertices[FaceIndices[fdx][i]]);
					float Delta = (Center - Edge).Size();
					InnerR[GeometryIndex] = FMath::Min(InnerR[GeometryIndex], Delta);
					OuterR[GeometryIndex] = FMath::Max(OuterR[GeometryIndex], Delta);
				}
			}
		}
	}

	// --- MATERIAL GROUP ---

	ReindexMaterials();

	return NewTransformIndex;
}

void FGeometryCollection::ReindexMaterials()
{
	// Reset current sections
	for (int SectionElement = 0; SectionElement < NumElements(FGeometryCollection::MaterialGroup); SectionElement++)
	{
		(*Sections)[SectionElement].FirstIndex = -1;
		(*Sections)[SectionElement].NumTriangles = 0;
	}

	int NumFaces = NumElements(FGeometryCollection::FacesGroup);

	// count the number of triangles for each material section, adding a new section if the material ID is higher than the current number of sections
	for (int FaceElement = 0; FaceElement < NumElements(FGeometryCollection::FacesGroup); FaceElement++)
	{
		int32 Section = (*MaterialID)[FaceElement];

		while (Section + 1 > NumElements(FGeometryCollection::MaterialGroup))
		{
			// add a new material section
			int32 Element = AddElements(1, FGeometryCollection::MaterialGroup);
			check(Section == Element);
			(*Sections)[Element].MaterialID = Element;
			(*Sections)[Element].FirstIndex = -1;
			(*Sections)[Element].NumTriangles = 0;
			(*Sections)[Element].MinVertexIndex = 0;
			(*Sections)[Element].MaxVertexIndex = 0;
		}

		(*Sections)[Section].NumTriangles++;
	}

	TArray<int> DelSections;
	// fixup the section FirstIndex and MaxVertexIndex
	for (int SectionElement = 0; SectionElement < NumElements(FGeometryCollection::MaterialGroup); SectionElement++)
	{
		if (SectionElement == 0)
		{
			(*Sections)[SectionElement].FirstIndex = 0;
		}
		else
		{
			// Each subsequent section has an index that starts after the last one
			// note the NumTriangles*3 - this is because indices are sent to the renderer in a flat array
			(*Sections)[SectionElement].FirstIndex = (*Sections)[SectionElement - 1].FirstIndex + (*Sections)[SectionElement - 1].NumTriangles * 3;
		}

		(*Sections)[SectionElement].MaxVertexIndex = NumElements(FGeometryCollection::VerticesGroup) - 1;

		// if a material group no longer has any triangles in it then add material section for removal
		if ((*Sections)[SectionElement].NumTriangles == 0)
		{
			DelSections.Push(SectionElement);
		}
	}

	// remap indices so the materials appear to be grouped
	int Idx = 0;
	for (int Section=0; Section < NumElements(FGeometryCollection::MaterialGroup); Section++)
	{
		for (int FaceElement = 0; FaceElement < NumElements(FGeometryCollection::FacesGroup); FaceElement++)
		{
			int32 ID = (*MaterialID)[FaceElement];
	
			if (Section == ID)
			{
				(*MaterialIndex)[Idx++] = FaceElement;
			}
		}
	}

	// delete unused material sections
	if (DelSections.Num())
	{
		Super::RemoveElements(FGeometryCollection::MaterialGroup, DelSections);
	}
}

void FGeometryCollection::RemoveElements(const FName & Group, const TArray<int32>& SortedDeletionList)
{
	if (SortedDeletionList.Num())
	{
		if (Group == FTransformCollection::TransformGroup)
		{
			RemoveGeometryElements(SortedDeletionList);
		}

		Super::RemoveElements(Group, SortedDeletionList);

	}
}

void FGeometryCollection::RemoveGeometryElements(const TArray<int32>& SortedDeletionList)
{
	if (SortedDeletionList.Num())
	{

		TArray<bool> Mask;

		//
		// Delete Vertices
		//
		TManagedArray<int32> & Bones = *BoneMap;
		GeometryCollectionAlgo::BuildLookupMask(SortedDeletionList, NumElements(FGeometryCollection::TransformGroup), Mask);

		TArray<int32> DelVertices;
		for (int32 Index = 0; Index < Bones.Num(); Index++)
		{
			if (Bones[Index] != Invalid && Bones[Index] < Mask.Num() && Mask[Bones[Index]])
			{
				DelVertices.Add(Index);
			}
		}
		DelVertices.Sort();

		//
		// Geometry
		// 
		TManagedArray<int32> & GeoemtryTransformIndex = *TransformIndex;

		TArray<int32> DelGeometryEntries;

		for (int32 Index = 0; Index < GeoemtryTransformIndex.Num(); Index++)
		{
			if (GeoemtryTransformIndex[Index] != Invalid && GeoemtryTransformIndex[Index] < Mask.Num()
				&& Mask[GeoemtryTransformIndex[Index]])
			{
				DelGeometryEntries.Add(Index);
			}
		}
		DelGeometryEntries.Sort();

		//
		// Delete Faces
		//
		GeometryCollectionAlgo::BuildLookupMask(DelVertices, NumElements(FGeometryCollection::VerticesGroup), Mask);
		TManagedArray<FIntVector> & Tris = *Indices;

		TArray<int32> DelFaces;
		for (int32 Index = 0; Index < Tris.Num(); Index++)
		{
			const FIntVector & Face = Tris[Index];
			for (int i = 0; i < 3; i++)
			{
				ensure(Face[i] < Mask.Num());
				if (Mask[Face[i]])
				{
					DelFaces.Add(Index);
					break;
				}
			}
		}
		DelFaces.Sort();

		Super::RemoveElements(FGeometryCollection::GeometryGroup, DelGeometryEntries);
		Super::RemoveElements(FGeometryCollection::VerticesGroup, DelVertices);
		Super::RemoveElements(FGeometryCollection::FacesGroup, DelFaces);

		ReindexMaterials();
	}
}

bool FGeometryCollection::HasVisibleGeometry()
{
	if (!Visible) 
	{
		return false;
	}
	bool bHasVisibleGeometry = false;
	TManagedArray<bool>& VisibleIndices = *Visible;

	for (int32 fdx = 0; fdx < VisibleIndices.Num(); fdx++)
	{
		if (VisibleIndices[fdx])
		{
			bHasVisibleGeometry = true;
			break;
		}
	}
	return bHasVisibleGeometry;
}

void  FGeometryCollection::Initialize(FManagedArrayCollection & CollectionIn)
{
	Super::Initialize(CollectionIn);
	BindSharedArrays();

	// Versioning - correct assets that were saved before material sections were introduced
	if (NumElements(FGeometryCollection::MaterialGroup) == 0)
	{
		int SectionIndex = AddElements(1, FGeometryCollection::MaterialGroup);
		(*Sections)[SectionIndex].MaterialID = 0;
		(*Sections)[SectionIndex].FirstIndex = 0;
		(*Sections)[SectionIndex].NumTriangles = Indices->Num();
		(*Sections)[SectionIndex].MinVertexIndex = 0;
		(*Sections)[SectionIndex].MaxVertexIndex = Vertex->Num();
	}
}

void FGeometryCollection::UpdateBoundingBox()
{
	const TManagedArray<FVector>& VertexArray = *Vertex;
	const TManagedArray<int32>& BoneMapArray = *BoneMap;
	const TManagedArray<int32>& TransformIndexArray = *TransformIndex;
	TManagedArray<FBox>& BoundingBoxArray = *BoundingBox;

	if (BoundingBoxArray.Num())
	{
		// Initialize BoundingBox
		for (int32 Idx = 0; Idx < BoundingBoxArray.Num(); ++Idx)
		{
			BoundingBoxArray[Idx].Init();
		}

		// Build reverse map between TransformIdx and index in the GeometryGroup
		TMap<int32, int32> GeometryGroupIndexMap;
		for (int32 Idx = 0; Idx < NumElements(FGeometryCollection::GeometryGroup); ++Idx)
		{
			GeometryGroupIndexMap.Add(TransformIndexArray[Idx], Idx);
		}
		// Compute BoundingBox
		for (int32 Idx = 0; Idx < VertexArray.Num(); ++Idx)
		{
			int32 TransformIndexValue = BoneMapArray[Idx];
			BoundingBoxArray[GeometryGroupIndexMap[TransformIndexValue]] += VertexArray[Idx];
		}
	}
}

void FGeometryCollection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		BindSharedArrays();
		// @todo(BackwardsCompatibility) : remove these lines after chaos project wraps [brice]
		SetDependency("BoneMap", FGeometryCollection::VerticesGroup, FTransformCollection::TransformGroup);
		SetDependency("Indices", FGeometryCollection::FacesGroup, FGeometryCollection::VerticesGroup);
		SetDependency("BoneHierarchy", FTransformCollection::TransformGroup, FTransformCollection::TransformGroup);
		SetDependency("TransformIndex", FGeometryCollection::GeometryGroup, FTransformCollection::TransformGroup);
		SetDependency("Sections", FGeometryCollection::MaterialGroup, FGeometryCollection::FacesGroup);

		TSharedPtr< TArray<int32> > GeometryIndices = GeometryCollectionAlgo::ContiguousArray(this->NumElements(FGeometryCollection::GeometryGroup));
		this->RemoveDependencyFor(FGeometryCollection::GeometryGroup);
		this->RemoveElements(FGeometryCollection::GeometryGroup, *GeometryIndices);
		GeometryCollection::AddGeometryProperties(this);

		GeometryCollection::MakeMaterialsContiguous(this);
		// @end(BackwardsCompatibility)
	}
}

void  FGeometryCollection::BindSharedArrays()
{
	Super::BindSharedArrays();

	Vertex = ShareAttribute<FVector>("Vertex", FGeometryCollection::VerticesGroup);
	Normal = ShareAttribute<FVector>("Normal", FGeometryCollection::VerticesGroup);
	UV = ShareAttribute<FVector2D>("UV", FGeometryCollection::VerticesGroup);
	Color = ShareAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup);
	TangentU = ShareAttribute<FVector>("TangentU", FGeometryCollection::VerticesGroup);
	TangentV = ShareAttribute<FVector>("TangentV", FGeometryCollection::VerticesGroup);
	BoneMap = ShareAttribute<int32>("BoneMap", FGeometryCollection::VerticesGroup);
	Indices = ShareAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);
	Visible = ShareAttribute<bool>("Visible", FGeometryCollection::FacesGroup);
	MaterialID = ShareAttribute<int32>("MaterialID", FGeometryCollection::FacesGroup);
	MaterialIndex = ShareAttribute<int32>("MaterialIndex", FGeometryCollection::FacesGroup);

	TransformIndex = ShareAttribute<int32>("TransformIndex", FGeometryCollection::GeometryGroup);
	BoundingBox = ShareAttribute<FBox>("BoundingBox", FGeometryCollection::GeometryGroup);
	InnerRadius = ShareAttribute<float>("InnerRadius", FGeometryCollection::GeometryGroup);
	OuterRadius = ShareAttribute<float>("OuterRadius", FGeometryCollection::GeometryGroup);
	VertexStart = ShareAttribute<int32>("VertexStart", FGeometryCollection::GeometryGroup);
	VertexCount = ShareAttribute<int32>("VertexCount", FGeometryCollection::GeometryGroup);
	FaceStart = ShareAttribute<int32>("FaceStart", FGeometryCollection::GeometryGroup);
	FaceCount = ShareAttribute<int32>("FaceCount", FGeometryCollection::GeometryGroup);
	Proximity = ShareAttribute<TSet<int32>>("Proximity", FGeometryCollection::GeometryGroup);

	BreakingFaceIndex = ShareAttribute<int32>("BreakingFaceIndex", FGeometryCollection::BreakingGroup);
	BreakingSourceTransformIndex = ShareAttribute<int32>("BreakingSourceTransformIndex", FGeometryCollection::BreakingGroup);
	BreakingTargetTransformIndex = ShareAttribute<int32>("BreakingTargetTransformIndex", FGeometryCollection::BreakingGroup);
	BreakingRegionCentroid = ShareAttribute<FVector>("BreakingRegionCentroid", FGeometryCollection::BreakingGroup);
	BreakingRegionNormal = ShareAttribute<FVector>("BreakingRegionNormal", FGeometryCollection::BreakingGroup);
	BreakingRegionRadius = ShareAttribute<float>("BreakingRegionRadius", FGeometryCollection::BreakingGroup);

	Sections = ShareAttribute<FGeometryCollectionSection>("Sections", FGeometryCollection::MaterialGroup);
}

bool FGeometryCollection::HasContiguousVertices( ) const
{
	// geometry
	const TManagedArray<int32>& GeometryTransformIndex = *this->TransformIndex;
	const TManagedArray<int32>& LocalVertexCount = *this->VertexCount;
	const TManagedArray<int32>& LocalVertexStart = *this->VertexStart;

	// vertices
	const TManagedArray<int32> & LocalBoneMap = *this->BoneMap;

	int32 NumTransforms = NumElements(FGeometryCollection::TransformGroup);

	for (int32 GeometryIndex = 0; GeometryIndex < GeometryTransformIndex.Num(); GeometryIndex++)
	{ // for each known geometry...
		int32 TransformIDFromGeometry = GeometryTransformIndex[GeometryIndex];
		int32 StartIndex = LocalVertexStart[GeometryIndex];
		int32 NumVertices = LocalVertexCount[GeometryIndex];

		int32 Counter = NumVertices;
		for (int BoneIndex = 0; BoneIndex < LocalBoneMap.Num(); BoneIndex++)
		{ // for each mapping from the vertex to the transform hierarchy ... 
			if (StartIndex <= BoneIndex && BoneIndex < (StartIndex + NumVertices))
			{ // process just the specified range
				int32 TransformIDFromBoneMap = LocalBoneMap[BoneIndex];
				if (TransformIDFromBoneMap < 0 || NumTransforms <= TransformIDFromBoneMap)
				{ // not contiguous if index is out of range
					return false;
				}
				if (TransformIDFromGeometry != TransformIDFromBoneMap)
				{ // not contiguous if indexing into a different transform
					return false;
				}
				Counter--;
			}
		}

		if (Counter)
		{
			return false;
		}
	}
	return true;
}


bool FGeometryCollection::HasContiguousFaces() const
{
	int32 TotalNumTransforms = NumElements(FGeometryCollection::TransformGroup);

	// geometry
	const TManagedArray<int32>& GeometryTransformIndex = *this->TransformIndex;
	const TManagedArray<int32>& LocalFaceCount = *this->FaceCount;
	const TManagedArray<int32>& LocalFaceStart = *this->FaceStart;

	// faces
	const TManagedArray<FIntVector> & LocalIndices = *this->Indices;

	// vertices
	int32 TotalNumVertices = NumElements(FGeometryCollection::VerticesGroup);
	const TManagedArray<int32> & LocalBoneMap = *this->BoneMap;


	for (int32 GeometryIndex = 0; GeometryIndex < GeometryTransformIndex.Num(); GeometryIndex++)
	{ // for each known geometry...
		int32 TransformIDFromGeometry = GeometryTransformIndex[GeometryIndex];
		int32 StartIndex = LocalFaceStart[GeometryIndex];
		int32 NumFaces = LocalFaceCount[GeometryIndex];

		int32 Counter = NumFaces;
		for (int FaceIndex = 0; FaceIndex < LocalIndices.Num(); FaceIndex++)
		{ // for each mapping from the vertex to the transform hierarchy ... 
			if (StartIndex <= FaceIndex && FaceIndex < (StartIndex + NumFaces))
			{ // process just the specified range
				for (int32 i = 0; i < 3; i++)
				{
					int32 VertexIndex = LocalIndices[FaceIndex][i];
					if (VertexIndex < 0 || TotalNumVertices <= VertexIndex)
					{
						return false;
					}

					int32 TransformIDFromBoneMap = LocalBoneMap[VertexIndex];

					if (TransformIDFromBoneMap < 0 && TotalNumTransforms < TransformIDFromBoneMap)
					{ // not contiguous if index is out of range
						return false;
					}
					if (TransformIDFromGeometry != TransformIDFromBoneMap)
					{ // not contiguous if indexing into a different transform
						return false;
					}
				}
				Counter--;
			}
		}

		if (Counter)
		{
			return false;
		}
	}
	return true;
}

bool FGeometryCollection::HasContiguousRenderFaces() const
{
	// validate all remapped indexes have their materials ID's grouped an in increasing order
	int LastMaterialID = 0;
	for (int IndexIdx = 0; IndexIdx < NumElements(FGeometryCollection::FacesGroup); IndexIdx++)
	{
		if (LastMaterialID > (*MaterialID)[(*MaterialIndex)[IndexIdx]])
			return false;
		LastMaterialID = (*MaterialID)[(*MaterialIndex)[IndexIdx]];
	}

	// check sections ranges do all point to a single material
	for (int MaterialIdx = 0; MaterialIdx < NumElements(FGeometryCollection::MaterialGroup); MaterialIdx++)
	{
		int first = (*Sections)[MaterialIdx].FirstIndex / 3;
		int last = first + (*Sections)[MaterialIdx].NumTriangles;

		for (int IndexIdx = first; IndexIdx < last; IndexIdx++)
		{
			if ( ((*MaterialID)[(*MaterialIndex)[IndexIdx]]) != MaterialIdx )
				return false;
		}

	}

	return true;
}

FGeometryCollection* FGeometryCollection::NewGeometryCollection(const TArray<float>& RawVertexArray, const TArray<int32>& RawIndicesArray, bool ReverseVertexOrder)
{

	FGeometryCollection* RestCollection = new FGeometryCollection();

	int NumNewVertices = RawVertexArray.Num() / 3;
	int VerticesIndex = RestCollection->AddElements(NumNewVertices, FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FVector> > VerticesRef = RestCollection->GetAttribute<FVector>("Vertex", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FVector> > NormalsRef = RestCollection->GetAttribute<FVector>("Normal", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FVector> > TangentURef = RestCollection->GetAttribute<FVector>("TangentU", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FVector> > TangentVRef = RestCollection->GetAttribute<FVector>("TangentV", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FVector2D> > UVsRef = RestCollection->GetAttribute<FVector2D>("UV", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FLinearColor> > ColorsRef = RestCollection->GetAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup);

	int NumNewIndices = RawIndicesArray.Num() / 3;
	int IndicesIndex = RestCollection->AddElements(NumNewIndices, FGeometryCollection::FacesGroup);
	TSharedRef<TManagedArray<FIntVector> > IndicesRef = RestCollection->GetAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);
	TSharedRef<TManagedArray<bool> > VisibleRef = RestCollection->GetAttribute<bool>("Visible", FGeometryCollection::FacesGroup);
	TSharedRef<TManagedArray<int32> > MaterialIDRef = RestCollection->GetAttribute<int32>("MaterialID", FGeometryCollection::FacesGroup);
	TSharedRef<TManagedArray<int32> > MaterialIndexRef = RestCollection->GetAttribute<int32>("MaterialIndex", FGeometryCollection::FacesGroup);

	int NumNewParticles = 1; // 1 particle for this geometry structure
	int ParticlesIndex = RestCollection->AddElements(NumNewParticles, FGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FTransform> > TransformRef = RestCollection->GetAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup);

	TManagedArray<FVector>& Vertices = *VerticesRef;
	TManagedArray<FVector>&  Normals = *NormalsRef;
	TManagedArray<FVector>&  TangentU = *TangentURef;
	TManagedArray<FVector>&  TangentV = *TangentVRef;
	TManagedArray<FVector2D>&  UVs = *UVsRef;
	TManagedArray<FLinearColor>&  Colors = *ColorsRef;
	TManagedArray<FIntVector>&  Indices = *IndicesRef;
	TManagedArray<bool>&  Visible = *VisibleRef;
	TManagedArray<int32>&  MaterialID = *MaterialIDRef;
	TManagedArray<int32>&  MaterialIndex = *MaterialIndexRef;
	TManagedArray<FTransform>&  Transform = *TransformRef;

	// set the vertex information
	FVector TempVertices(0.f, 0.f, 0.f);
	for (int32 Idx = 0; Idx < NumNewVertices; ++Idx)
	{
		Vertices[Idx] = FVector(RawVertexArray[3 * Idx], RawVertexArray[3 * Idx + 1], RawVertexArray[3 * Idx + 2]);
		TempVertices += Vertices[Idx];

		UVs[Idx] = FVector2D(0, 0);
		Colors[Idx] = FLinearColor::White;
	}

	// set the particle information
	TempVertices /= (float)NumNewVertices;
	Transform[0] = FTransform(TempVertices);
	Transform[0].NormalizeRotation();

	// set the index information
	TArray<FVector> FaceNormals;
	FaceNormals.SetNum(NumNewIndices);
	for (int32 Idx = 0; Idx < NumNewIndices; ++Idx)
	{
		int32 VertexIdx1, VertexIdx2, VertexIdx3;
		if (!ReverseVertexOrder)
		{
			VertexIdx1 = RawIndicesArray[3 * Idx];
			VertexIdx2 = RawIndicesArray[3 * Idx + 1];
			VertexIdx3 = RawIndicesArray[3 * Idx + 2];
		}
		else
		{
			VertexIdx1 = RawIndicesArray[3 * Idx];
			VertexIdx2 = RawIndicesArray[3 * Idx + 2];
			VertexIdx3 = RawIndicesArray[3 * Idx + 1];
		}

		Indices[Idx] = FIntVector(VertexIdx1, VertexIdx2, VertexIdx3);
		Visible[Idx] = true;
		MaterialID[Idx] = 0;
		MaterialIndex[Idx] = Idx;

		const FVector Edge1 = Vertices[VertexIdx1] - Vertices[VertexIdx2];
		const FVector Edge2 = Vertices[VertexIdx1] - Vertices[VertexIdx3];
		FaceNormals[Idx] = (Edge2 ^ Edge1).GetSafeNormal();
	}

	// Compute vertexNormals
	TArray<FVector> VertexNormals;
	VertexNormals.SetNum(NumNewVertices);
	for (int32 Idx = 0; Idx < NumNewVertices; ++Idx)
	{
		VertexNormals[Idx] = FVector(0.f, 0.f, 0.f);
	}

	for (int32 Idx = 0; Idx < NumNewIndices; ++Idx)
	{
		VertexNormals[Indices[Idx][0]] += FaceNormals[Idx];
		VertexNormals[Indices[Idx][1]] += FaceNormals[Idx];
		VertexNormals[Indices[Idx][2]] += FaceNormals[Idx];
	}

	for (int32 Idx = 0; Idx < NumNewVertices; ++Idx)
	{
		Normals[Idx] = (VertexNormals[Idx] / 3.f).GetSafeNormal();
	}

	for (int IndexIdx = 0; IndexIdx < NumNewIndices; IndexIdx++)
	{
		FIntVector Tri = Indices[IndexIdx];
		for (int idx = 0; idx < 3; idx++)
		{
			const FVector Normal = Normals[Tri[idx]];
			const FVector Edge = (Vertices[Tri[(idx + 1) % 3]] - Vertices[Tri[idx]]);
			TangentU[Tri[idx]] = (Edge ^ Normal).GetSafeNormal();
			TangentV[Tri[idx]] = (Normal ^ TangentU[Tri[idx]]).GetSafeNormal();
		}
	}

	// Build the Geometry Group
	GeometryCollection::AddGeometryProperties(RestCollection);

	// add a material section
	TSharedRef<TManagedArray<FGeometryCollectionSection> > SectionsRef = RestCollection->GetAttribute<FGeometryCollectionSection>("Sections", FGeometryCollection::MaterialGroup);
	TManagedArray<FGeometryCollectionSection>&  Sections = *SectionsRef;
	int Element = RestCollection->AddElements(1, FGeometryCollection::MaterialGroup);
	Sections[Element].MaterialID = 0;
	Sections[Element].FirstIndex = 0;
	Sections[Element].NumTriangles = Indices.Num();
	Sections[Element].MinVertexIndex = 0;
	Sections[Element].MaxVertexIndex = Vertices.Num() - 1;

	return RestCollection;
}

void FGeometryCollection::WriteDataToHeaderFile(const FString &Name, const FString &Path)
{
	using namespace std;

	static const FString DataFilePath = "D:";
	FString FullPath = (Path.IsEmpty() || Path.Equals(TEXT("None"))) ? DataFilePath : Path;
	FullPath.RemoveFromEnd("\\");
	FullPath += "\\" + Name + ".h";

	ofstream DataFile;
	DataFile.open(string(TCHAR_TO_UTF8(*FullPath)));
	DataFile << "// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved." << endl << endl;
	DataFile << "#pragma once" << endl << endl;
	DataFile << "class " << TCHAR_TO_UTF8(*Name) << endl;
	DataFile << "{" << endl;
	DataFile << "public:" << endl;
	DataFile << "    " << TCHAR_TO_UTF8(*Name) << "();" << endl;
	DataFile << "    ~" << TCHAR_TO_UTF8(*Name) << "() {};" << endl << endl;
	DataFile << "    static const TArray<float>	RawVertexArray;" << endl;
	DataFile << "    static const TArray<int32>	RawIndicesArray;" << endl;
	DataFile << "    static const TArray<int32>	RawBoneMapArray;" << endl;
	DataFile << "    static const TArray<FTransform> RawTransformArray;" << endl;
	DataFile << "    static const TArray<FGeometryCollectionBoneNode> RawBoneHierarchyArray;" << endl;
	DataFile << "};" << endl << endl;
	DataFile << "const TArray<float> " << TCHAR_TO_UTF8(*Name) << "::RawVertexArray = {" << endl;

	int32 NumVertices = NumElements(FGeometryCollection::VerticesGroup);
	const TManagedArray<FVector>& VertexArray = *Vertex;
	for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
	{
		DataFile << "                                                    " <<
			VertexArray[IdxVertex].X << ", " <<
			VertexArray[IdxVertex].Y << ", " <<
			VertexArray[IdxVertex].Z << ", " << endl;
	}
	DataFile << "};" << endl << endl;
	DataFile << "const TArray<int32> " << TCHAR_TO_UTF8(*Name) << "::RawIndicesArray = {" << endl;

	int32 NumFaces = NumElements(FGeometryCollection::FacesGroup);
	const TManagedArray<FIntVector>& IndicesArray = *Indices;
	for (int32 IdxFace = 0; IdxFace < NumFaces; ++IdxFace)
	{
		DataFile << "                                                    " <<
			IndicesArray[IdxFace].X << ", " <<
			IndicesArray[IdxFace].Y << ", " <<
			IndicesArray[IdxFace].Z << ", " << endl;
	}

	DataFile << "};" << endl << endl;
	DataFile << "const TArray<int32> " << TCHAR_TO_UTF8(*Name) << "::RawBoneMapArray = {" << endl;

	const TManagedArray<int32>& BoneMapArray = *BoneMap;
	for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
	{
		DataFile << "                                                    " <<
			BoneMapArray[IdxVertex] << ", " << endl;
	}
	DataFile << "};" << endl << endl;

	DataFile << "const TArray<FTransform> " << TCHAR_TO_UTF8(*Name) << "::RawTransformArray = {" << endl;

	int32 NumTransforms = NumElements(FGeometryCollection::TransformGroup);
	const TManagedArray<FTransform>& TransformArray = *Transform;
	for (int32 IdxTransform = 0; IdxTransform < NumTransforms; ++IdxTransform)
	{
		FQuat Rotation = TransformArray[IdxTransform].GetRotation();
		FVector Translation = TransformArray[IdxTransform].GetTranslation();
		FVector Scale3D = TransformArray[IdxTransform].GetScale3D();

		DataFile << "   FTransform(FQuat(" <<
			Rotation.X << ", " <<
			Rotation.Y << ", " <<
			Rotation.Z << ", " <<
			Rotation.W << "), " <<
			"FVector(" <<
			Translation.X << ", " <<
			Translation.Y << ", " <<
			Translation.Z << "), " <<
			"FVector(" <<
			Scale3D.X << ", " <<
			Scale3D.Y << ", " <<
			Scale3D.Z << ")), " << endl;
	}
	DataFile << "};" << endl << endl;

	// Write BoneHierarchy array
	DataFile << "const TArray<FGeometryCollectionBoneNode> " << TCHAR_TO_UTF8(*Name) << "::RawBoneHierarchyArray = {" << endl;

	const TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyArray = *BoneHierarchy;
	for (int32 IdxTransform = 0; IdxTransform < NumTransforms; ++IdxTransform)
	{
		DataFile << "   FGeometryCollectionBoneNode(" <<
			BoneHierarchyArray[IdxTransform].Level << ", " <<
			BoneHierarchyArray[IdxTransform].Parent << ", " <<
			BoneHierarchyArray[IdxTransform].StatusFlags << "), " << endl;
	}
	DataFile << "};" << endl << endl;
	DataFile.close();
}

void FGeometryCollection::WriteDataToOBJFile(const FString &Name, const FString &Path, const bool WriteTopology, const bool WriteAuxStructures)
{
	using namespace std;

	static const FString DataFilePath = "D:";

	int32 NumVertices = NumElements(FGeometryCollection::VerticesGroup);
	int32 NumFaces = NumElements(FGeometryCollection::FacesGroup);

	TArray<FTransform> GlobalTransformArray;
	GeometryCollectionAlgo::GlobalMatrices(this, GlobalTransformArray);

	TArray<FVector> VertexInWorldArray;
	VertexInWorldArray.SetNum(NumVertices);

	const TManagedArray<int32>& BoneMapArray = *BoneMap;
	const TManagedArray<FVector>& VertexArray = *Vertex;
	const TManagedArray<FIntVector>& IndicesArray = *Indices;

	for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
	{
		FTransform LocalTransform = GlobalTransformArray[BoneMapArray[IdxVertex]];
		FVector VertexInWorld = LocalTransform.TransformPosition(VertexArray[IdxVertex]);

		VertexInWorldArray[IdxVertex] = VertexInWorld;
	}

	ofstream DataFile;
	if (WriteTopology)
	{
		FString FullPath = (Path.IsEmpty() || Path.Equals(TEXT("None"))) ? DataFilePath : Path;
		FullPath.RemoveFromEnd("\\");
		FullPath += "\\" + Name + ".obj";

		DataFile.open(string(TCHAR_TO_UTF8(*FullPath)));

		DataFile << "# File exported from UE4" << endl;
		DataFile << "# " << NumVertices << " points" << endl;
		DataFile << "# " << NumVertices * 3 << " vertices" << endl;
		DataFile << "# " << NumFaces << " primitives" << endl;
		DataFile << "g" << endl;
		for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
		{
			DataFile << "v " << VertexInWorldArray[IdxVertex].X << " " <<
				VertexInWorldArray[IdxVertex].Y << " " <<
				VertexInWorldArray[IdxVertex].Z << endl;
		}
		DataFile << "g" << endl;

		// FaceIndex in the OBJ format starts with 1
		for (int32 IdxFace = 0; IdxFace < NumFaces; ++IdxFace)
		{
			DataFile << "f " << IndicesArray[IdxFace].X + 1 << " " <<
				IndicesArray[IdxFace].Z + 1 << " " <<
				IndicesArray[IdxFace].Y + 1 << endl;
		}
		DataFile << endl;
		DataFile.close();
	}
	if(WriteAuxStructures && HasAttribute("VertexVisibility", FGeometryCollection::VerticesGroup))
	{
		FString FullPath = (Path.IsEmpty() || Path.Equals(TEXT("None"))) ? DataFilePath : Path;
		FullPath.RemoveFromEnd("\\");
		FullPath += "\\" + Name + "_VertexVisibility.obj";

		DataFile.open(string(TCHAR_TO_UTF8(*FullPath)));
		DataFile << "# Vertex Visibility - vertices whose visibility flag are true" << endl;

		TSharedRef<TManagedArray<bool> > VertexVisibility =
			GetAttribute<bool>("VertexVisibility", FGeometryCollection::VerticesGroup);
		int num = 0;
		for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
		{
			if ((*VertexVisibility)[IdxVertex])
				num++;
		}
		DataFile << "# " << num << " Vertices" << endl;

		DataFile << "g" << endl;
		for (int32 IdxVertex = 0; IdxVertex < NumVertices; ++IdxVertex)
		{
			if((*VertexVisibility)[IdxVertex])
			{
				DataFile << "v " 
					<< VertexInWorldArray[IdxVertex].X << " " 
					<< VertexInWorldArray[IdxVertex].Y << " " 
					<< VertexInWorldArray[IdxVertex].Z << endl;
			}
		}
		DataFile << endl;
		DataFile.close();
	}
}

FGeometryCollection* FGeometryCollection::NewGeometryCollection(const TArray<float>& RawVertexArray,
																const TArray<int32>& RawIndicesArray,
																const TArray<int32>& RawBoneMapArray,
																const TArray<FTransform>& RawTransformArray,
																const TManagedArray<FGeometryCollectionBoneNode>& RawBoneHierarchyArray)
{
	FGeometryCollection* RestCollection = new FGeometryCollection();

	int NumNewVertices = RawVertexArray.Num() / 3;
	int VerticesIndex = RestCollection->AddElements(NumNewVertices, FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FVector> > VerticesRef = RestCollection->GetAttribute<FVector>("Vertex", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FVector> > NormalsRef = RestCollection->GetAttribute<FVector>("Normal", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FVector> > TangentURef = RestCollection->GetAttribute<FVector>("TangentU", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FVector> > TangentVRef = RestCollection->GetAttribute<FVector>("TangentV", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FVector2D> > UVsRef = RestCollection->GetAttribute<FVector2D>("UV", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<FLinearColor> > ColorsRef = RestCollection->GetAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup);
	TSharedRef<TManagedArray<int32> > BoneMapRef = RestCollection->GetAttribute<int32>("BoneMap", FGeometryCollection::VerticesGroup);

	int NumNewIndices = RawIndicesArray.Num() / 3;
	int IndicesIndex = RestCollection->AddElements(NumNewIndices, FGeometryCollection::FacesGroup);
	TSharedRef<TManagedArray<FIntVector> > IndicesRef = RestCollection->GetAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);
	TSharedRef<TManagedArray<bool> > VisibleRef = RestCollection->GetAttribute<bool>("Visible", FGeometryCollection::FacesGroup);
	TSharedRef<TManagedArray<int32> > MaterialIDRef = RestCollection->GetAttribute<int32>("MaterialID", FGeometryCollection::FacesGroup);
	TSharedRef<TManagedArray<int32> > MaterialIndexRef = RestCollection->GetAttribute<int32>("MaterialIndex", FGeometryCollection::FacesGroup);

	TSharedRef<TManagedArray<FTransform> > TransformRef = RestCollection->GetAttribute<FTransform>("Transform", FGeometryCollection::TransformGroup);
	TSharedRef<TManagedArray<FGeometryCollectionBoneNode> > BoneHierarchyRef = RestCollection->GetAttribute<FGeometryCollectionBoneNode>("BoneHierarchy", FGeometryCollection::TransformGroup);

	TManagedArray<FVector>& Vertices = *VerticesRef;
	TManagedArray<FVector>&  Normals = *NormalsRef;
	TManagedArray<FVector>&  TangentU = *TangentURef;
	TManagedArray<FVector>&  TangentV = *TangentVRef;
	TManagedArray<FVector2D>&  UVs = *UVsRef;
	TManagedArray<FLinearColor>&  Colors = *ColorsRef;
	TManagedArray<int32>& BoneMap = *BoneMapRef;
	TManagedArray<FIntVector>&  Indices = *IndicesRef;
	TManagedArray<bool>&  Visible = *VisibleRef;
	TManagedArray<int32>&  MaterialID = *MaterialIDRef;
	TManagedArray<int32>&  MaterialIndex = *MaterialIndexRef;
	TManagedArray<FTransform>&  Transform = *TransformRef;
	TManagedArray<FGeometryCollectionBoneNode>&  BoneHierarchy = *BoneHierarchyRef;


	// set the vertex information
	for (int32 Idx = 0; Idx < NumNewVertices; ++Idx)
	{
		Vertices[Idx] = FVector(RawVertexArray[3 * Idx], RawVertexArray[3 * Idx + 1], RawVertexArray[3 * Idx + 2]);
		BoneMap[Idx] = RawBoneMapArray[Idx];

		UVs[Idx] = FVector2D(0, 0);
		Colors[Idx] = FLinearColor::White;
	}

	// Transforms
	int NumNewTransforms = RawTransformArray.Num(); // 1 particle for this geometry structure
	int TransformIndex = RestCollection->AddElements(NumNewTransforms, FGeometryCollection::TransformGroup);

	for (int32 Idx = 0; Idx < NumNewTransforms; ++Idx)
	{
		Transform[Idx] = RawTransformArray[Idx];
		Transform[Idx].NormalizeRotation();

		BoneHierarchy[Idx] = RawBoneHierarchyArray[Idx];
		for (int32 Idx1 = 0; Idx1 < NumNewTransforms; ++Idx1)
		{
			if (RawBoneHierarchyArray[Idx1].Parent == Idx)
			{
				BoneHierarchy[Idx].Children.Add(Idx1);
			}
		}

	}

	// set the index information
	TArray<FVector> FaceNormals;
	FaceNormals.SetNum(NumNewIndices);
	for (int32 Idx = 0; Idx < NumNewIndices; ++Idx)
	{
		int32 VertexIdx1, VertexIdx2, VertexIdx3;
		VertexIdx1 = RawIndicesArray[3 * Idx];
		VertexIdx2 = RawIndicesArray[3 * Idx + 1];
		VertexIdx3 = RawIndicesArray[3 * Idx + 2];

		Indices[Idx] = FIntVector(VertexIdx1, VertexIdx2, VertexIdx3);
		Visible[Idx] = true;
		MaterialID[Idx] = 0;
		MaterialIndex[Idx] = Idx;

		const FVector Edge1 = Vertices[VertexIdx1] - Vertices[VertexIdx2];
		const FVector Edge2 = Vertices[VertexIdx1] - Vertices[VertexIdx3];
		FaceNormals[Idx] = (Edge2 ^ Edge1).GetSafeNormal();
	}

	// Compute vertexNormals
	TArray<FVector> VertexNormals;
	VertexNormals.SetNum(NumNewVertices);
	for (int32 Idx = 0; Idx < NumNewVertices; ++Idx)
	{
		VertexNormals[Idx] = FVector(0.f, 0.f, 0.f);
	}

	for (int32 Idx = 0; Idx < NumNewIndices; ++Idx)
	{
		VertexNormals[Indices[Idx][0]] += FaceNormals[Idx];
		VertexNormals[Indices[Idx][1]] += FaceNormals[Idx];
		VertexNormals[Indices[Idx][2]] += FaceNormals[Idx];
	}

	for (int32 Idx = 0; Idx < NumNewVertices; ++Idx)
	{
		Normals[Idx] = (VertexNormals[Idx] / 3.f).GetSafeNormal();
	}

	for (int IndexIdx = 0; IndexIdx < NumNewIndices; IndexIdx++)
	{
		FIntVector Tri = Indices[IndexIdx];
		for (int idx = 0; idx < 3; idx++)
		{
			const FVector Normal = Normals[Tri[idx]];
			const FVector Edge = (Vertices[Tri[(idx + 1) % 3]] - Vertices[Tri[idx]]);
			TangentU[Tri[idx]] = (Edge ^ Normal).GetSafeNormal();
			TangentV[Tri[idx]] = (Normal ^ TangentU[Tri[idx]]).GetSafeNormal();
		}
	}

	// Build the Geometry Group
	GeometryCollection::AddGeometryProperties(RestCollection);

	FGeometryCollectionProximityUtility::UpdateProximity(RestCollection);

	// add a material section
	TSharedRef<TManagedArray<FGeometryCollectionSection> > SectionsRef = RestCollection->GetAttribute<FGeometryCollectionSection>("Sections", FGeometryCollection::MaterialGroup);
	TManagedArray<FGeometryCollectionSection>&  Sections = *SectionsRef;
	int Element = RestCollection->AddElements(1, FGeometryCollection::MaterialGroup);
	Sections[Element].MaterialID = 0;
	Sections[Element].FirstIndex = 0;
	Sections[Element].NumTriangles = Indices.Num();
	Sections[Element].MinVertexIndex = 0;
	Sections[Element].MaxVertexIndex = Vertices.Num() - 1;

	return RestCollection;
}



