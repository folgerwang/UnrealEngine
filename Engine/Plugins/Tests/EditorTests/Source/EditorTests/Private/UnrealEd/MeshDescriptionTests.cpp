// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "MeshDescriptionOperations.h"
#include "MeshBuilder.h"
#include "MeshAttributes.h"
#include "RawMesh.h"
#include "MeshUtilities.h"

//////////////////////////////////////////////////////////////////////////

/**
* FMeshDescriptionAutomationTest
* Test that verify the MeshDescription functionalities. (Creation, modification, conversion to/from FRawMesh, render build)
* The tests will create some transient geometry using the mesh description API
* Cannot be run in a commandlet as it executes code that routes through Slate UI.
*/
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMeshDescriptionAutomationTest, "Editor.Meshes.MeshDescription", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::EngineFilter))

#define ConversionTestData 1
#define NTB_TestData 2

class FMeshDescriptionTest
{
public:
	FMeshDescriptionTest(const FString& InBeautifiedNames, const FString& InTestData)
		: BeautifiedNames(InBeautifiedNames)
		, TestData(InTestData)
	{}

	FString BeautifiedNames;
	FString TestData;
	bool Execute(FAutomationTestExecutionInfo& ExecutionInfo);
	bool ConversionTest(FAutomationTestExecutionInfo& ExecutionInfo);
	bool NTBTest(FAutomationTestExecutionInfo& ExecutionInfo);
private:
	bool CompareRawMesh(const FString& AssetName, FAutomationTestExecutionInfo& ExecutionInfo, const FRawMesh& ReferenceRawMesh, const FRawMesh& ResultRawMesh) const;
	bool CompareMeshDescription(const FString& AssetName, FAutomationTestExecutionInfo& ExecutionInfo, const FMeshDescription& ReferenceMeshDescription, const FMeshDescription& MeshDescription) const;
};

class FMeshDescriptionTests
{
public:
	static FMeshDescriptionTests& GetInstance()
	{
		static FMeshDescriptionTests Instance;
		return Instance;
	}

	void ClearTests()
	{
		AllTests.Empty();
	}

	bool ExecTest(int32 TestKey, FAutomationTestExecutionInfo& ExecutionInfo)
	{
		check(AllTests.Contains(TestKey));
		FMeshDescriptionTest& Test = AllTests[TestKey];
		return Test.Execute(ExecutionInfo);
	}

	bool AddTest(const FMeshDescriptionTest& MeshDescriptionTest)
	{
		if (!MeshDescriptionTest.TestData.IsNumeric())
		{
			return false;
		}
		int32 TestID = FPlatformString::Atoi(*(MeshDescriptionTest.TestData));
		check(!AllTests.Contains(TestID));
		AllTests.Add(TestID, MeshDescriptionTest);
		return true;
	}

private:
	TMap<int32, FMeshDescriptionTest> AllTests;
	FMeshDescriptionTests() {}
};

/**
* Requests a enumeration of all sample assets to import
*/
void FMeshDescriptionAutomationTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	FMeshDescriptionTests::GetInstance().ClearTests();
	//Create conversion test
	FMeshDescriptionTest ConversionTest(TEXT("Conversion test data"), FString::FromInt(ConversionTestData));
	FMeshDescriptionTests::GetInstance().AddTest(ConversionTest);
	OutBeautifiedNames.Add(ConversionTest.BeautifiedNames);
	OutTestCommands.Add(ConversionTest.TestData);
	//Create NormalTangentBinormal test
/*	FMeshDescriptionTest NTB_Test(TEXT("Normals Tangents and Binormals test"), FString::FromInt(NTB_TestData));
	FMeshDescriptionTests::GetInstance().AddTest(NTB_Test);
	OutBeautifiedNames.Add(NTB_Test.BeautifiedNames);
	OutTestCommands.Add(NTB_Test.TestData);
*/
}

/**
* Execute the generic import test
*
* @param Parameters - Should specify the asset to import
* @return	TRUE if the test was successful, FALSE otherwise
*/
bool FMeshDescriptionAutomationTest::RunTest(const FString& Parameters)
{
	if (!Parameters.IsNumeric())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("Wrong parameter for mesh description test parameter should be a number: [%s]"), *Parameters)));
		return false;
	}
	int32 TestID = FPlatformString::Atoi(*Parameters);
	return FMeshDescriptionTests::GetInstance().ExecTest(TestID, ExecutionInfo);
}

bool FMeshDescriptionTest::Execute(FAutomationTestExecutionInfo& ExecutionInfo)
{
	if (!TestData.IsNumeric())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("Wrong parameter for mesh description test parameter should be a number: [%s]"), *TestData)));
		return false;
	}
	int32 TestID = FPlatformString::Atoi(*TestData);
	bool bSuccess = false;
	switch (TestID)
	{
	case ConversionTestData:
		bSuccess = ConversionTest(ExecutionInfo);
		break;
	case NTB_TestData:
		bSuccess = NTBTest(ExecutionInfo);
		break;
	}
	return bSuccess;
}

template<typename T>
void StructureArrayCompareFullPrecision(const FString& ConversionName, const FString& AssetName, FAutomationTestExecutionInfo& ExecutionInfo, bool& bIsSame, const FString& VectorArrayName, const TArray<T>& ReferenceArray, const TArray<T>& ResultArray)
{
	if (ReferenceArray.Num() != ResultArray.Num())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion %s is not lossless, %s count is different. %s count expected [%d] result [%d]"),
			*AssetName,
			*ConversionName,
			*VectorArrayName,
			*VectorArrayName,
			ReferenceArray.Num(),
			ResultArray.Num())));
		bIsSame = false;
	}
	else
	{
		for (int32 VertexIndex = 0; VertexIndex < ReferenceArray.Num(); ++VertexIndex)
		{
			if (ReferenceArray[VertexIndex] != ResultArray[VertexIndex])
			{
				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion %s is not lossless, %s array is different. Array index [%d] expected %s [%s] result [%s]"),
					*AssetName,
					*ConversionName,
					*VectorArrayName,
					VertexIndex,
					*VectorArrayName,
					*ReferenceArray[VertexIndex].ToString(),
					*ResultArray[VertexIndex].ToString())));
				bIsSame = false;
				break;
			}
		}
	}
}

template<typename T>
void StructureArrayCompare(const FString& ConversionName, const FString& AssetName, FAutomationTestExecutionInfo& ExecutionInfo, bool& bIsSame, const FString& VectorArrayName, const TArray<T>& ReferenceArray, const TArray<T>& ResultArray)
{
	if (ReferenceArray.Num() != ResultArray.Num())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion %s is not lossless, %s count is different. %s count expected [%d] result [%d]"),
			*AssetName,
			*ConversionName,
			*VectorArrayName,
			*VectorArrayName,
			ReferenceArray.Num(),
			ResultArray.Num())));
		bIsSame = false;
	}
	else
	{
		for (int32 VertexIndex = 0; VertexIndex < ReferenceArray.Num(); ++VertexIndex)
		{
			if (!ReferenceArray[VertexIndex].Equals(ResultArray[VertexIndex]))
			{
				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion %s is not lossless, %s array is different. Array index [%d] expected %s [%s] result [%s]"),
					*AssetName,
					*ConversionName,
					*VectorArrayName,
					VertexIndex,
					*VectorArrayName,
					*ReferenceArray[VertexIndex].ToString(),
					*ResultArray[VertexIndex].ToString())));
				bIsSame = false;
				break;
			}
		}
	}
}

template<typename T>
void NumberArrayCompare(const FString& ConversionName, const FString& AssetName, FAutomationTestExecutionInfo& ExecutionInfo, bool& bIsSame, const FString& VectorArrayName, const TArray<T>& ReferenceArray, const TArray<T>& ResultArray)
{
	if (ReferenceArray.Num() != ResultArray.Num())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion %s is not lossless, %s count is different. %s count expected [%d] result [%d]"),
			*AssetName,
			*ConversionName,
			*VectorArrayName,
			*VectorArrayName,
			ReferenceArray.Num(),
			ResultArray.Num())));
		bIsSame = false;
	}
	else
	{
		for (int32 VertexIndex = 0; VertexIndex < ReferenceArray.Num(); ++VertexIndex)
		{
			if (ReferenceArray[VertexIndex] != ResultArray[VertexIndex])
			{
				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion %s is not lossless, %s array is different. Array index [%d] expected %s [%d] result [%d]"),
					*AssetName,
					*ConversionName,
					*VectorArrayName,
					VertexIndex,
					*VectorArrayName,
					ReferenceArray[VertexIndex],
					ResultArray[VertexIndex])));
				bIsSame = false;
				break;
			}
		}
	}
}

bool FMeshDescriptionTest::CompareRawMesh(const FString& AssetName, FAutomationTestExecutionInfo& ExecutionInfo, const FRawMesh& ReferenceRawMesh, const FRawMesh& ResultRawMesh) const
{
	//////////////////////////////////////////////////////////////////////////
	// Do the comparison
	bool bAllSame = true;

	FString ConversionName = TEXT("RawMesh to MeshDescription to RawMesh");

	//Positions
	StructureArrayCompare<FVector>(ConversionName, AssetName, ExecutionInfo, bAllSame, TEXT("vertex positions"), ReferenceRawMesh.VertexPositions, ResultRawMesh.VertexPositions);

	//Normals
	StructureArrayCompare<FVector>(ConversionName, AssetName, ExecutionInfo, bAllSame, TEXT("vertex instance normals"), ReferenceRawMesh.WedgeTangentZ, ResultRawMesh.WedgeTangentZ);

	//Tangents
	StructureArrayCompare<FVector>(ConversionName, AssetName, ExecutionInfo, bAllSame, TEXT("vertex instance tangents"), ReferenceRawMesh.WedgeTangentX, ResultRawMesh.WedgeTangentX);

	//BiNormal
	StructureArrayCompare<FVector>(ConversionName, AssetName, ExecutionInfo, bAllSame, TEXT("vertex instance binormals"), ReferenceRawMesh.WedgeTangentY, ResultRawMesh.WedgeTangentY);

	//Colors --- FColor do not have Equals, so let use the full precision (FColor use integer anyway)
	StructureArrayCompareFullPrecision<FColor>(ConversionName, AssetName, ExecutionInfo, bAllSame, TEXT("vertex instance colors"), ReferenceRawMesh.WedgeColors, ResultRawMesh.WedgeColors);

	//Uvs
	for (int32 UVIndex = 0; UVIndex < MAX_MESH_TEXTURE_COORDS; ++UVIndex)
	{
		FString UVIndexName = FString::Printf(TEXT("vertex instance UVs(%d)"), UVIndex);
		StructureArrayCompare<FVector2D>(ConversionName, AssetName, ExecutionInfo, bAllSame, UVIndexName, ReferenceRawMesh.WedgeTexCoords[UVIndex], ResultRawMesh.WedgeTexCoords[UVIndex]);
	}
	
	//Indices
	NumberArrayCompare<uint32>(ConversionName, AssetName, ExecutionInfo, bAllSame, TEXT("vertex positions"), ReferenceRawMesh.WedgeIndices, ResultRawMesh.WedgeIndices);

	//Face
	NumberArrayCompare<int32>(ConversionName, AssetName, ExecutionInfo, bAllSame, TEXT("face material"), ReferenceRawMesh.FaceMaterialIndices, ResultRawMesh.FaceMaterialIndices);

	//Smoothing Mask
	NumberArrayCompare<uint32>(ConversionName, AssetName, ExecutionInfo, bAllSame, TEXT("smoothing mask"), ReferenceRawMesh.FaceSmoothingMasks, ResultRawMesh.FaceSmoothingMasks);

	return bAllSame;
}


template <typename T> FString AttributeValueAsString(const T& Value) { return LexToString(Value); }
FString AttributeValueAsString(const FVector& Value) { return Value.ToString(); }
FString AttributeValueAsString(const FVector2D& Value) { return Value.ToString(); }
FString AttributeValueAsString(const FVector4& Value) { return Value.ToString(); }

template<typename T, typename U>
void MeshDescriptionAttributeArrayCompare(const FString& ConversionName, const FString& AssetName, FAutomationTestExecutionInfo& ExecutionInfo, bool& bIsSame, const U& ElementIterator, const FString& ArrayName, const T ReferenceArray, const T ResultArray)
{
	if (ReferenceArray.GetNumElements() != ResultArray.GetNumElements())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion %s is not lossless, %s count is different. %s count expected [%d] result [%d]"),
			*AssetName,
			*ConversionName,
			*ArrayName,
			*ArrayName,
			ReferenceArray.GetNumElements(),
			ResultArray.GetNumElements())));
		bIsSame = false;
	}
	else if (ReferenceArray.GetNumIndices() != ResultArray.GetNumIndices())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion %s is not lossless, %s channel count is different. %s channel count expected [%d] result [%d]"),
			*AssetName,
			*ConversionName,
			*ArrayName,
			*ArrayName,
			ReferenceArray.GetNumIndices(),
			ResultArray.GetNumIndices())));
		bIsSame = false;
	}
	else
	{
		int32 NumDifferent = 0;
		for (int32 Index = 0; Index < ReferenceArray.GetNumIndices(); ++Index)
		{
			for (auto ElementID : ElementIterator.GetElementIDs())
			{
				if (ReferenceArray.Get(ElementID, Index) != ResultArray.Get(ElementID, Index))
				{
					NumDifferent++;
					if (NumDifferent < 5)
					{
						ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion %s is not lossless, %s array is different. Element [%d] of attribute index [%d] expected %s [%s] result [%s]"),
							*AssetName,
							*ConversionName,
							*ArrayName,
							ElementID.GetValue(),
							Index,
							*ArrayName,
							*AttributeValueAsString(ReferenceArray.Get(ElementID, Index)),
							*AttributeValueAsString(ResultArray.Get(ElementID, Index)))));
					}
					else
					{
						ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, TEXT("More than 5 unequal elements - silencing")));
						break;
					}
					bIsSame = false;
				}
			}
		}
	}
}

bool FMeshDescriptionTest::CompareMeshDescription(const FString& AssetName, FAutomationTestExecutionInfo& ExecutionInfo, const FMeshDescription& ReferenceMeshDescription, const FMeshDescription& MeshDescription) const
{
	//////////////////////////////////////////////////////////////////////////
	//Gather the reference data
	TVertexAttributesConstRef<FVector> ReferenceVertexPositions = ReferenceMeshDescription.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TVertexInstanceAttributesConstRef<FVector> ReferenceVertexInstanceNormals = ReferenceMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesConstRef<FVector> ReferenceVertexInstanceTangents = ReferenceMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesConstRef<float> ReferenceVertexInstanceBinormalSigns = ReferenceMeshDescription.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesConstRef<FVector4> ReferenceVertexInstanceColors = ReferenceMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesConstRef<FVector2D> ReferenceVertexInstanceUVs = ReferenceMeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
	TEdgeAttributesConstRef<bool> ReferenceEdgeHardnesses = ReferenceMeshDescription.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	TPolygonGroupAttributesConstRef<FName> ReferencePolygonGroupMaterialName = ReferenceMeshDescription.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);


	//////////////////////////////////////////////////////////////////////////
	//Gather the result data
	TVertexAttributesConstRef<FVector> ResultVertexPositions = MeshDescription.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TVertexInstanceAttributesConstRef<FVector> ResultVertexInstanceNormals = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesConstRef<FVector> ResultVertexInstanceTangents = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesConstRef<float> ResultVertexInstanceBinormalSigns = MeshDescription.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesConstRef<FVector4> ResultVertexInstanceColors = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesConstRef<FVector2D> ResultVertexInstanceUVs = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
	TEdgeAttributesConstRef<bool> ResultEdgeHardnesses = MeshDescription.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	TPolygonGroupAttributesConstRef<FName> ResultPolygonGroupMaterialName = MeshDescription.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

	
	//////////////////////////////////////////////////////////////////////////
	// Do the comparison
	bool bAllSame = true;

	FString ConversionName = TEXT("MeshDescription to RawMesh to MeshDescription");

	//Positions
	MeshDescriptionAttributeArrayCompare(ConversionName, AssetName, ExecutionInfo, bAllSame, ReferenceMeshDescription.Vertices(), TEXT("vertex positions"), ReferenceVertexPositions, ResultVertexPositions);

	//Normals
	MeshDescriptionAttributeArrayCompare(ConversionName, AssetName, ExecutionInfo, bAllSame, ReferenceMeshDescription.VertexInstances(), TEXT("vertex instance normals"), ReferenceVertexInstanceNormals, ResultVertexInstanceNormals);

	//Tangents
	MeshDescriptionAttributeArrayCompare(ConversionName, AssetName, ExecutionInfo, bAllSame, ReferenceMeshDescription.VertexInstances(), TEXT("vertex instance tangents"), ReferenceVertexInstanceTangents, ResultVertexInstanceTangents);

	//BiNormal signs
	MeshDescriptionAttributeArrayCompare(ConversionName, AssetName, ExecutionInfo, bAllSame, ReferenceMeshDescription.VertexInstances(), TEXT("vertex instance binormals"), ReferenceVertexInstanceBinormalSigns, ResultVertexInstanceBinormalSigns);

	//Colors
	MeshDescriptionAttributeArrayCompare(ConversionName, AssetName, ExecutionInfo, bAllSame, ReferenceMeshDescription.VertexInstances(), TEXT("vertex instance colors"), ReferenceVertexInstanceColors, ResultVertexInstanceColors);

	//Uvs
	MeshDescriptionAttributeArrayCompare(ConversionName, AssetName, ExecutionInfo, bAllSame, ReferenceMeshDescription.VertexInstances(), TEXT("vertex instance UVs"), ReferenceVertexInstanceUVs, ResultVertexInstanceUVs);

	//Edges
	//We do not use a template since we need to check the connected polygon count to validate a false comparison
	if (ReferenceEdgeHardnesses.GetNumElements() != ResultEdgeHardnesses.GetNumElements())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion MeshDescription to RawMesh to MeshDescription is not lossless, Edge count is different. Edges count expected [%d] result [%d]"),
			*AssetName,
			ReferenceEdgeHardnesses.GetNumElements(),
			ResultEdgeHardnesses.GetNumElements())));
		bAllSame = false;
	}
	else
	{
		for (const FEdgeID& EdgeID : MeshDescription.Edges().GetElementIDs())
		{
			if (ReferenceEdgeHardnesses[EdgeID] != ResultEdgeHardnesses[EdgeID])
			{
				//Make sure it is not an external edge (only one polygon connected) since it is impossible to retain this information in a smoothing group.
				//External edge hardnesses has no impact on the normal calculation. It is useful only when editing meshes.
				const TArray<FPolygonID>& EdgeConnectedPolygons = MeshDescription.GetEdgeConnectedPolygons(EdgeID);
				if (EdgeConnectedPolygons.Num() > 1)
				{
					ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, Edge hardnesses array is different. EdgeID [%d] expected hardnesse [%s] result [%s]"),
						*AssetName,
						EdgeID.GetValue(),
						(ReferenceEdgeHardnesses[EdgeID] ? TEXT("true") : TEXT("false")),
						(ResultEdgeHardnesses[EdgeID] ? TEXT("true") : TEXT("false")))));
					bAllSame = false;
				}
				break;
			}
		}
	}

	//Polygon group ID
	MeshDescriptionAttributeArrayCompare(ConversionName, AssetName, ExecutionInfo, bAllSame, ReferenceMeshDescription.PolygonGroups(), TEXT("PolygonGroup Material Name"), ReferencePolygonGroupMaterialName, ResultPolygonGroupMaterialName);

	return bAllSame;
}

bool FMeshDescriptionTest::ConversionTest(FAutomationTestExecutionInfo& ExecutionInfo)
{
	TArray<FString> AssetNames;
	AssetNames.Add(TEXT("Cone_1"));
	AssetNames.Add(TEXT("Cone_2"));
	AssetNames.Add(TEXT("Cube"));
	AssetNames.Add(TEXT("Patch_1"));
	AssetNames.Add(TEXT("Patch_2"));
	AssetNames.Add(TEXT("Patch_3"));
	AssetNames.Add(TEXT("Patch_4"));
	AssetNames.Add(TEXT("Patch_5"));
	AssetNames.Add(TEXT("Pentagone"));
	AssetNames.Add(TEXT("Sphere_1"));
	AssetNames.Add(TEXT("Sphere_2"));
	AssetNames.Add(TEXT("Sphere_3"));
	AssetNames.Add(TEXT("Torus_1"));
	AssetNames.Add(TEXT("Torus_2"));

	bool bAllSame = true;
	for (const FString& AssetName : AssetNames)
	{
		FString FullAssetName = TEXT("/Game/Tests/MeshDescription/") + AssetName + TEXT(".") + AssetName;
		UStaticMesh* AssetMesh = LoadObject<UStaticMesh>(nullptr, *FullAssetName, nullptr, LOAD_None, nullptr);

		if (AssetMesh != nullptr)
		{
			AssetMesh->BuildCacheAutomationTestGuid = FGuid::NewGuid();
			TMap<FName, int32> MaterialMap;
			TMap<int32, FName> MaterialMapInverse;
			for (int32 MaterialIndex = 0; MaterialIndex < AssetMesh->StaticMaterials.Num(); ++MaterialIndex)
			{
				MaterialMap.Add(AssetMesh->StaticMaterials[MaterialIndex].ImportedMaterialSlotName, MaterialIndex);
				MaterialMapInverse.Add(MaterialIndex, AssetMesh->StaticMaterials[MaterialIndex].ImportedMaterialSlotName);
			}

			//MeshDescription to RawMesh to MeshDescription
			for(int32 LodIndex = 0; LodIndex < AssetMesh->SourceModels.Num(); ++LodIndex)
			{
				const FMeshDescription* ReferenceAssetMesh = AssetMesh->GetOriginalMeshDescription(LodIndex);
				if (ReferenceAssetMesh == nullptr)
				{
					check(LodIndex != 0);
					continue;
				}
				//Create a temporary Mesh Description
				FMeshDescription ResultAssetMesh(*ReferenceAssetMesh);
				//Convert MeshDescription to FRawMesh
				FRawMesh RawMesh;
				FMeshDescriptionOperations::ConvertToRawMesh(ResultAssetMesh, RawMesh, MaterialMap);
				//Convert back the FRawmesh
				FMeshDescriptionOperations::ConvertFromRawMesh(RawMesh, ResultAssetMesh, MaterialMapInverse);
				if (!CompareMeshDescription(AssetName, ExecutionInfo, *ReferenceAssetMesh, ResultAssetMesh))
				{
					bAllSame = false;
				}
			}

			//RawMesh to MeshDescription to RawMesh
			for (int32 LodIndex = 0; LodIndex < AssetMesh->SourceModels.Num(); ++LodIndex)
			{
				if (AssetMesh->SourceModels[LodIndex].RawMeshBulkData->IsEmpty())
				{
					check(LodIndex != 0);
					continue;
				}
				FRawMesh ReferenceRawMesh;
				AssetMesh->SourceModels[LodIndex].LoadRawMesh(ReferenceRawMesh);
				FRawMesh ResultRawMesh;
				AssetMesh->SourceModels[LodIndex].LoadRawMesh(ResultRawMesh);
				//Create a temporary Mesh Description
				FMeshDescription MeshDescription;
				UStaticMesh::RegisterMeshAttributes(MeshDescription);
				FMeshDescriptionOperations::ConvertFromRawMesh(ResultRawMesh, MeshDescription, MaterialMapInverse);
				//Convert back the FRawmesh
				FMeshDescriptionOperations::ConvertToRawMesh(MeshDescription, ResultRawMesh, MaterialMap);
				if (!CompareRawMesh(AssetName, ExecutionInfo, ReferenceRawMesh, ResultRawMesh))
				{
					bAllSame = false;
				}
			}
		}
	}
	return bAllSame;
}

bool FMeshDescriptionTest::NTBTest(FAutomationTestExecutionInfo& ExecutionInfo)
{
	TArray<FString> AssetNames;
	AssetNames.Add(TEXT("Cone_1"));
	AssetNames.Add(TEXT("Cone_2"));
	AssetNames.Add(TEXT("Cube"));
	AssetNames.Add(TEXT("Patch_1"));
	AssetNames.Add(TEXT("Patch_2"));
	AssetNames.Add(TEXT("Patch_3"));
	AssetNames.Add(TEXT("Patch_4"));
	AssetNames.Add(TEXT("Patch_5"));
	AssetNames.Add(TEXT("Pentagone"));
	AssetNames.Add(TEXT("Sphere_1"));
	AssetNames.Add(TEXT("Sphere_2"));
	AssetNames.Add(TEXT("Sphere_3"));
	AssetNames.Add(TEXT("Torus_1"));
	AssetNames.Add(TEXT("Torus_2"));

	bool bAllSame = true;
	for (const FString& AssetName : AssetNames)
	{
		FString FullAssetName = TEXT("/Game/Tests/MeshDescription/") + AssetName + TEXT(".") + AssetName;
		UStaticMesh* AssetMesh = LoadObject<UStaticMesh>(nullptr, *FullAssetName, nullptr, LOAD_None, nullptr);

		if (AssetMesh == nullptr)
		{
			continue;
		}
		//Dirty the build
		AssetMesh->BuildCacheAutomationTestGuid = FGuid::NewGuid();
		FMeshDescription* OriginalMeshDescription = AssetMesh->GetOriginalMeshDescription(0);
		check(OriginalMeshDescription);

		// Take a copy, so changes made by the test don't make permanent changes to the mesh description
		// @todo Alexis: is this OK?
		FMeshDescription MeshDescription(*OriginalMeshDescription);

		FRawMesh RawMesh;
		AssetMesh->SourceModels[0].LoadRawMesh(RawMesh);

		//const TVertexAttributeArray<FVector>& VertexPositions = MeshDescription.VertexAttributes().GetAttributes<FVector>(MeshAttribute::Vertex::Position);
		const TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
		const TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
		const TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = MeshDescription.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
		const TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
		const TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);
		int32 ExistingUVCount = VertexInstanceUVs.GetNumIndices();
		//Build the normals and tangent and compare the result
		MeshDescription.ComputePolygonTangentsAndNormals(SMALL_NUMBER);
		EComputeNTBsOptions ComputeNTBsOptions = EComputeNTBsOptions::Normals | EComputeNTBsOptions::Tangents;
		MeshDescription.ComputeTangentsAndNormals(ComputeNTBsOptions);
		//FMeshDescriptionOperations::CreatePolygonNTB(MeshDescription, SMALL_NUMBER);
		//FMeshDescriptionOperations::CreateNormals(MeshDescription, FMeshDescriptionOperations::ETangentOptions::BlendOverlappingNormals, true);

		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities"));
		FMeshBuildSettings MeshBuildSettings;
		MeshBuildSettings.bRemoveDegenerates = true;
		MeshBuildSettings.bUseMikkTSpace = false;
		MeshUtilities.RecomputeTangentsAndNormalsForRawMesh(true, true, MeshBuildSettings, RawMesh);

		//The normals and tangents of both the meshdescription and the RawMesh should be equal to not break old data
		if(RawMesh.WedgeIndices.Num() != MeshDescription.VertexInstances().Num())
		{
			ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("Test: [Normals Tangents and Binormals test]    Asset: [%s]    Error: The number of vertex instances is not equal between FRawMesh [%d] and UMeshDescription [%d]."),
				*AssetName,
				RawMesh.WedgeIndices.Num(),
				MeshDescription.VertexInstances().Num())));
			continue;
		}
		int32 VertexInstanceIndex = 0;
		int32 TriangleIndex = 0;

		auto OutputError=[&ExecutionInfo, &AssetName](FString ErrorMessage)
		{
			ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("Test: [Normals Tangents and Binormals test]    Asset: [%s]    Error: %s."),
				*AssetName,
				*ErrorMessage)));
		};
		bool bError = false;
		for (const FPolygonID& PolygonID : MeshDescription.Polygons().GetElementIDs())
		{
			if (bError)
			{
				break;
			}
			const FPolygonGroupID& PolygonGroupID = MeshDescription.GetPolygonPolygonGroup(PolygonID);
			int32 PolygonIDValue = PolygonID.GetValue();
			const TArray<FMeshTriangle>& Triangles = MeshDescription.GetPolygonTriangles(PolygonID);
			for (const FMeshTriangle& MeshTriangle : Triangles)
			{
				if (bError)
				{
					break;
				}
				for (int32 Corner = 0; Corner < 3 && bError == false; ++Corner)
				{
					uint32 WedgeIndex = TriangleIndex * 3 + Corner;
					const FVertexInstanceID VertexInstanceID = MeshTriangle.GetVertexInstanceID(Corner);
					const int32 VertexInstanceIDValue = VertexInstanceID.GetValue();
					if (RawMesh.WedgeColors[WedgeIndex] != FLinearColor(VertexInstanceColors[VertexInstanceID]).ToFColor(true))
					{
						FString MeshDescriptionColor = FLinearColor(VertexInstanceColors[VertexInstanceID]).ToFColor(true).ToString();
						OutputError(FString::Printf(TEXT("Vertex color is different between MeshDescription [%s] and FRawMesh [%s].   Indice[%d]")
							, *MeshDescriptionColor
							, *(RawMesh.WedgeColors[WedgeIndex].ToString())
							, VertexInstanceIDValue));
						bError = true;
					}
					if (RawMesh.WedgeIndices[WedgeIndex] != MeshDescription.GetVertexInstanceVertex(VertexInstanceID).GetValue())
					{
						OutputError(FString::Printf(TEXT("Vertex index is different between MeshDescription [%d] and FRawMesh [%d].   Indice[%d]")
							, MeshDescription.GetVertexInstanceVertex(VertexInstanceID).GetValue()
							, RawMesh.WedgeIndices[WedgeIndex]
							, VertexInstanceIDValue));
						bError = true;
					}
					if (!RawMesh.WedgeTangentX[WedgeIndex].Equals(VertexInstanceTangents[VertexInstanceID], THRESH_NORMALS_ARE_SAME))
					{
						OutputError(FString::Printf(TEXT("Vertex tangent is different between MeshDescription [%s] and FRawMesh [%s].   Indice[%d]")
							, *(VertexInstanceTangents[VertexInstanceID].ToString())
							, *(RawMesh.WedgeTangentX[WedgeIndex].ToString())
							, VertexInstanceIDValue));
						bError = true;
					}
					if (!RawMesh.WedgeTangentY[WedgeIndex].Equals(FVector::CrossProduct(VertexInstanceNormals[VertexInstanceID], VertexInstanceTangents[VertexInstanceID]).GetSafeNormal() * VertexInstanceBinormalSigns[VertexInstanceID], THRESH_NORMALS_ARE_SAME))
					{
						FVector MeshDescriptionBinormalResult = FVector::CrossProduct(VertexInstanceNormals[VertexInstanceID], VertexInstanceTangents[VertexInstanceID]).GetSafeNormal() * VertexInstanceBinormalSigns[VertexInstanceID];
						OutputError(FString::Printf(TEXT("Vertex binormal is different between MeshDescription [%s] and FRawMesh [%s].   Indice[%d]")
							, *(MeshDescriptionBinormalResult.ToString())
							, *(RawMesh.WedgeTangentY[WedgeIndex].ToString())
							, VertexInstanceIDValue));
						bError = true;
					}
					if (!RawMesh.WedgeTangentZ[WedgeIndex].Equals(VertexInstanceNormals[VertexInstanceID], THRESH_NORMALS_ARE_SAME))
					{
						OutputError(FString::Printf(TEXT("Vertex normal is different between MeshDescription [%s] and FRawMesh [%s].   Indice[%d]")
							, *(VertexInstanceNormals[VertexInstanceID].ToString())
							, *(RawMesh.WedgeTangentZ[WedgeIndex].ToString())
							, VertexInstanceIDValue));
						bError = true;
					}

					for (int32 UVIndex = 0; UVIndex < ExistingUVCount; ++UVIndex)
					{
						if (!RawMesh.WedgeTexCoords[UVIndex][WedgeIndex].Equals(VertexInstanceUVs.Get(VertexInstanceID, UVIndex), THRESH_UVS_ARE_SAME))
						{
							OutputError(FString::Printf(TEXT("Vertex Texture coordinnate is different between MeshDescription [%s] and FRawMesh [%s].   UVIndex[%d]  Indice[%d]")
								, *(VertexInstanceUVs.Get(VertexInstanceID, UVIndex).ToString())
								, *(RawMesh.WedgeTexCoords[UVIndex][WedgeIndex].ToString())
								, UVIndex
								, VertexInstanceIDValue));
							bError = true;
						}
					}
				}
				++TriangleIndex;
			}
		}
	}

	return bAllSame;
}