// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: UGeometryCollection methods.
=============================================================================*/

#include "GeometryCollection.h"

#if WITH_EDITOR
#include "EditorFramework/AssetImportData.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(UGeometryCollectionLogging, NoLogging, All);
const FName UGeometryCollection::VerticesGroup = "Vertices";
const FName UGeometryCollection::GeometryGroup = "Geometry";


UGeometryCollection::UGeometryCollection(const FObjectInitializer& ObjectInitializer)
	: UTransformCollection(ObjectInitializer)
	, Vertex(new TManagedArray<FVector>())
	, UV(new TManagedArray<FVector2D>())
	, Color(new TManagedArray<FLinearColor>())
	, TangentU(new TManagedArray<FVector>())
	, TangentV(new TManagedArray<FVector>())
	, Normal(new TManagedArray<FVector>())
	, BoneMap(new TManagedArray<int32>())
	, Indices(new TManagedArray<FIntVector>())
	, Visible(new TManagedArray<bool>())
{
	check(ObjectInitializer.GetClass() == GetClass());
	if (UGeometryCollection * CollectionAsset = static_cast<UGeometryCollection*>(ObjectInitializer.GetObj()))
	{
		Vertex = CollectionAsset->Vertex;
		Normal = CollectionAsset->Normal;
		UV = CollectionAsset->UV;
		Color = CollectionAsset->Color;
		TangentU = CollectionAsset->TangentU;
		TangentV = CollectionAsset->TangentV;
		BoneMap = CollectionAsset->BoneMap;
		Indices = CollectionAsset->Indices;
		Visible = CollectionAsset->Visible;
	}

	// Vertices Group
	AddAttribute<FVector>("Vertex", UGeometryCollection::VerticesGroup, Vertex);
	AddAttribute<FVector>("Normal", UGeometryCollection::VerticesGroup, Normal);
	AddAttribute<FVector2D>("UV", UGeometryCollection::VerticesGroup, UV);
	AddAttribute<FLinearColor>("Color", UGeometryCollection::VerticesGroup, Color);
	AddAttribute<FVector>("TangentU", UGeometryCollection::VerticesGroup, TangentU);
	AddAttribute<FVector>("TangentV", UGeometryCollection::VerticesGroup, TangentV);
	AddAttribute<int32>("BoneMap", UGeometryCollection::VerticesGroup, BoneMap);

	// Geometry Group
	AddAttribute<FIntVector>("Indices", UGeometryCollection::GeometryGroup, Indices);
	AddAttribute<bool>("Visible", UGeometryCollection::GeometryGroup, Visible);
}

int32 UGeometryCollection::AppendGeometry(const UGeometryCollection & Element)
{
	check(Element.NumElements(UGeometryCollection::GeometryGroup) > 0);
	check(Element.NumElements(UGeometryCollection::VerticesGroup) > 0);

	int NumNewVertices = Element.NumElements(UGeometryCollection::VerticesGroup);
	const TManagedArray<FVector>& ElementVertices = *Element.Vertex;
	const TManagedArray<FVector>& ElementNormals = *Element.Normal;
	const TManagedArray<FVector2D>& ElementUVs = *Element.UV;
	const TManagedArray<FLinearColor>& ElementColors = *Element.Color;
	const TManagedArray<FVector>& ElementTangentUs = *Element.TangentU;
	const TManagedArray<FVector>& ElementTangentVs = *Element.TangentV;
	const TManagedArray<int32>& ElementBoneMap = *Element.BoneMap;
	const TManagedArray<FIntVector>& ElementIndices = *Element.Indices;
	const TManagedArray<bool>& ElementVisible = *Element.Visible;
	const TManagedArray<FTransform>& ElementTransform = *Element.Transform;
	const TManagedArray<FString>& ElementBoneName = *Element.BoneName;
	const TManagedArray<FGeometryCollectionBoneNode>& ElementBoneHierarchy = *Element.BoneHierarchy;


	int VerticesIndex = AddElements(NumNewVertices, UGeometryCollection::VerticesGroup);
	TManagedArray<FVector>& Vertices = *Vertex;
	TManagedArray<FVector>& Normals = *Normal;
	TManagedArray<FVector2D>& UVs = *UV;
	TManagedArray<FLinearColor>& Colors = *Color;
	TManagedArray<FVector>& TangentUs = *TangentU;
	TManagedArray<FVector>& TangentVs = *TangentV;
	TManagedArray<int32>& BoneMaps = *BoneMap;

	for (int vdx = 0; vdx < NumNewVertices; vdx++)
	{
		Vertices[VerticesIndex + vdx] = ElementVertices[vdx];
		Normals[VerticesIndex + vdx] = ElementNormals[vdx];
		UVs[VerticesIndex + vdx] = ElementUVs[vdx];
		Colors[VerticesIndex + vdx] = ElementColors[vdx];
		TangentUs[VerticesIndex + vdx] = ElementTangentUs[vdx];
		TangentVs[VerticesIndex + vdx] = ElementTangentVs[vdx];
		BoneMaps[VerticesIndex + vdx] = NumElements(UGeometryCollection::TransformGroup);
	}

	int NumNewIndices = ElementIndices.Num();
	int IndicesIndex = AddElements(NumNewIndices, UGeometryCollection::GeometryGroup);
	for (int32 tdx = 0; tdx < NumNewIndices; tdx++)
	{
		(*Indices)[IndicesIndex + tdx] = FIntVector(VerticesIndex, VerticesIndex, VerticesIndex) + ElementIndices[tdx];
		(*Visible)[IndicesIndex + tdx] = ElementVisible[tdx];
	}

	return Super::AppendTransform(Element);
}


bool UGeometryCollection::HasVisibleGeometry()
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

void  UGeometryCollection::Initialize(UManagedArrayCollection & CollectionIn)
{
	Super::Initialize(CollectionIn);
	BindSharedArrays();
}



void UGeometryCollection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		BindSharedArrays();
	}
}



void  UGeometryCollection::BindSharedArrays()
{
	Super::BindSharedArrays();

	Vertex = ShareAttribute<FVector>("Vertex", UGeometryCollection::VerticesGroup);
	Normal = ShareAttribute<FVector>("Normal", UGeometryCollection::VerticesGroup);
	UV = ShareAttribute<FVector2D>("UV", UGeometryCollection::VerticesGroup);
	Color = ShareAttribute<FLinearColor>("Color", UGeometryCollection::VerticesGroup);
	TangentU = ShareAttribute<FVector>("TangentU", UGeometryCollection::VerticesGroup);
	TangentV = ShareAttribute<FVector>("TangentV", UGeometryCollection::VerticesGroup);
	BoneMap = ShareAttribute<int32>("BoneMap", UGeometryCollection::VerticesGroup);
	Indices = ShareAttribute<FIntVector>("Indices", UGeometryCollection::GeometryGroup);
	Visible = ShareAttribute<bool>("Visible", UGeometryCollection::GeometryGroup);
}

