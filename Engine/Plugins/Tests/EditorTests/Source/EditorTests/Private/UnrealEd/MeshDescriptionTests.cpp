// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "MeshBuilder.h"
#include "MeshAttributes.h"
#include "RawMesh.h"

//////////////////////////////////////////////////////////////////////////

/**
* FMeshDescriptionAutomationTest
* Test that verify the MeshDescription functionalities. (Creation, modification, conversion to/from FRawMesh, render build)
* The tests will create some transient geometry using the mesh description API
* Cannot be run in a commandlet as it executes code that routes through Slate UI.
*/
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMeshDescriptionAutomationTest, "Editor.Meshes.MeshDescription", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::EngineFilter))

#define ConversionTestData 1

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
private:
	bool CompareMeshDescription(const FString& AssetName, FAutomationTestExecutionInfo& ExecutionInfo, const UMeshDescription* ReferenceMeshDescription, const UMeshDescription* MeshDescription);
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
	//Create cube test
	FMeshDescriptionTest ConversionTest(TEXT("Conversion test data"), FString::FromInt(ConversionTestData));
	FMeshDescriptionTests::GetInstance().AddTest(ConversionTest);
	OutBeautifiedNames.Add(ConversionTest.BeautifiedNames);
	OutTestCommands.Add(ConversionTest.TestData);
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
	}
	return bSuccess;
}

bool FMeshDescriptionTest::CompareMeshDescription(const FString& AssetName, FAutomationTestExecutionInfo& ExecutionInfo, const UMeshDescription* ReferenceMeshDescription, const UMeshDescription* MeshDescription)
{
	//////////////////////////////////////////////////////////////////////////
	//Gather the reference data
	const TVertexAttributeArray<FVector>& ReferenceVertexPositions = ReferenceMeshDescription->VertexAttributes().GetAttributes<FVector>(MeshAttribute::Vertex::Position);

	const TVertexInstanceAttributeArray<FVector>& ReferenceVertexInstanceNormals = ReferenceMeshDescription->VertexInstanceAttributes().GetAttributes<FVector>(MeshAttribute::VertexInstance::Normal);
	const TVertexInstanceAttributeArray<FVector>& ReferenceVertexInstanceTangents = ReferenceMeshDescription->VertexInstanceAttributes().GetAttributes<FVector>(MeshAttribute::VertexInstance::Tangent);
	const TVertexInstanceAttributeArray<float>& ReferenceVertexInstanceBinormalSigns = ReferenceMeshDescription->VertexInstanceAttributes().GetAttributes<float>(MeshAttribute::VertexInstance::BinormalSign);
	const TVertexInstanceAttributeArray<FVector4>& ReferenceVertexInstanceColors = ReferenceMeshDescription->VertexInstanceAttributes().GetAttributes<FVector4>(MeshAttribute::VertexInstance::Color);
	const TVertexInstanceAttributeIndicesArray<FVector2D>& ReferenceVertexInstanceUVs = ReferenceMeshDescription->VertexInstanceAttributes().GetAttributesSet<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	const TEdgeAttributeArray<bool>& ReferenceEdgeHardnesses = ReferenceMeshDescription->EdgeAttributes().GetAttributes<bool>(MeshAttribute::Edge::IsHard);

	const TPolygonGroupAttributeArray<int>& ReferencePolygonGroupMaterialIndex = ReferenceMeshDescription->PolygonGroupAttributes().GetAttributes<int>(MeshAttribute::PolygonGroup::MaterialIndex);


	//////////////////////////////////////////////////////////////////////////
	//Gather the result data
	const TVertexAttributeArray<FVector>& ResultVertexPositions = MeshDescription->VertexAttributes().GetAttributes<FVector>(MeshAttribute::Vertex::Position);

	const TVertexInstanceAttributeArray<FVector>& ResultVertexInstanceNormals = MeshDescription->VertexInstanceAttributes().GetAttributes<FVector>(MeshAttribute::VertexInstance::Normal);
	const TVertexInstanceAttributeArray<FVector>& ResultVertexInstanceTangents = MeshDescription->VertexInstanceAttributes().GetAttributes<FVector>(MeshAttribute::VertexInstance::Tangent);
	const TVertexInstanceAttributeArray<float>& ResultVertexInstanceBinormalSigns = MeshDescription->VertexInstanceAttributes().GetAttributes<float>(MeshAttribute::VertexInstance::BinormalSign);
	const TVertexInstanceAttributeArray<FVector4>& ResultVertexInstanceColors = MeshDescription->VertexInstanceAttributes().GetAttributes<FVector4>(MeshAttribute::VertexInstance::Color);
	const TVertexInstanceAttributeIndicesArray<FVector2D>& ResultVertexInstanceUVs = MeshDescription->VertexInstanceAttributes().GetAttributesSet<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	const TEdgeAttributeArray<bool>& ResultEdgeHardnesses = MeshDescription->EdgeAttributes().GetAttributes<bool>(MeshAttribute::Edge::IsHard);

	const TPolygonGroupAttributeArray<int>& ResultPolygonGroupMaterialIndex = MeshDescription->PolygonGroupAttributes().GetAttributes<int>(MeshAttribute::PolygonGroup::MaterialIndex);

	
	//////////////////////////////////////////////////////////////////////////
	// Do the comparison
	bool bAllSame = true;
	//Positions
	if (ReferenceVertexPositions.Num() != ResultVertexPositions.Num())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex positions count is different. Vertex count expected [%d] result [%d]"),
			*AssetName,
			ReferenceVertexPositions.Num(),
			ResultVertexPositions.Num())));
		bAllSame = false;
	}
	else
	{
		for (FVertexID VertexID : ReferenceMeshDescription->Vertices().GetElementIDs())
		{
			if (ReferenceVertexPositions[VertexID] != ResultVertexPositions[VertexID])
			{
				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex position array is different. VertexID [%d] expected position [%s] result [%s]"),
					*AssetName,
					VertexID.GetValue(),
					*ReferenceVertexPositions[VertexID].ToString(),
					*ResultVertexPositions[VertexID].ToString())));
				bAllSame = false;
				break;
			}
		}
	}
	//Normals
	if (ReferenceVertexInstanceNormals.Num() != ResultVertexInstanceNormals.Num())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex instance normals count is different. Normals count expected [%d] result [%d]"),
			*AssetName,
			ReferenceVertexInstanceNormals.Num(),
			ResultVertexInstanceNormals.Num())));
		bAllSame = false;
	}
	else
	{
		for (FVertexInstanceID VertexInstanceID : ReferenceMeshDescription->VertexInstances().GetElementIDs())
		{
			if (ReferenceVertexInstanceNormals[VertexInstanceID] != ResultVertexInstanceNormals[VertexInstanceID])
			{
				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex instance normals array is different. VertexInstanceID [%d] expected normal [%s] result [%s]"),
					*AssetName,
					VertexInstanceID.GetValue(),
					*ReferenceVertexInstanceNormals[VertexInstanceID].ToString(),
					*ResultVertexInstanceNormals[VertexInstanceID].ToString())));
				bAllSame = false;
				break;
			}
		}
	}
	//Tangents
	if (ReferenceVertexInstanceTangents.Num() != ResultVertexInstanceTangents.Num())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex instance Tangents count is different. Tangents count expected [%d] result [%d]"),
			*AssetName,
			ReferenceVertexInstanceTangents.Num(),
			ResultVertexInstanceTangents.Num())));
		bAllSame = false;
	}
	else
	{
		for (FVertexInstanceID VertexInstanceID : ReferenceMeshDescription->VertexInstances().GetElementIDs())
		{
			if (ReferenceVertexInstanceTangents[VertexInstanceID] != ResultVertexInstanceTangents[VertexInstanceID])
			{
				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex instance Tangents array is different. VertexInstanceID [%d] expected Tangent [%s] result [%s]"),
					*AssetName,
					VertexInstanceID.GetValue(),
					*ReferenceVertexInstanceTangents[VertexInstanceID].ToString(),
					*ResultVertexInstanceTangents[VertexInstanceID].ToString())));
				bAllSame = false;
				break;
			}
		}
	}
	//BiNormal signs
	if (ReferenceVertexInstanceBinormalSigns.Num() != ResultVertexInstanceBinormalSigns.Num())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex instance BinormalSigns count is different. BinormalSigns count expected [%d] result [%d]"),
			*AssetName,
			ReferenceVertexInstanceBinormalSigns.Num(),
			ResultVertexInstanceBinormalSigns.Num())));
		bAllSame = false;
	}
	else
	{
		for (FVertexInstanceID VertexInstanceID : ReferenceMeshDescription->VertexInstances().GetElementIDs())
		{
			if (ReferenceVertexInstanceBinormalSigns[VertexInstanceID] != ResultVertexInstanceBinormalSigns[VertexInstanceID])
			{
				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex instance BinormalSigns array is different. VertexInstanceID [%d] expected binormal sign [%f] result [%f]"),
					*AssetName,
					VertexInstanceID.GetValue(),
					ReferenceVertexInstanceBinormalSigns[VertexInstanceID],
					ResultVertexInstanceBinormalSigns[VertexInstanceID])));
				bAllSame = false;
				break;
			}
		}
	}
	//Colors
	if (ReferenceVertexInstanceColors.Num() != ResultVertexInstanceColors.Num())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex instance Colors count is different. Colors count expected [%d] result [%d]"),
			*AssetName,
			ReferenceVertexInstanceColors.Num(),
			ResultVertexInstanceColors.Num())));
		bAllSame = false;
	}
	else
	{
		for (FVertexInstanceID VertexInstanceID : ReferenceMeshDescription->VertexInstances().GetElementIDs())
		{
			if (ReferenceVertexInstanceColors[VertexInstanceID] != ResultVertexInstanceColors[VertexInstanceID])
			{
				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex instance Colors array is different. VertexInstanceID [%d] expected Color [%s] result [%s]"),
					*AssetName, 
					VertexInstanceID.GetValue(),
					*ReferenceVertexInstanceColors[VertexInstanceID].ToString(),
					*ResultVertexInstanceColors[VertexInstanceID].ToString())));
				bAllSame = false;
				break;
			}
		}
	}
	//Uvs
	if (ReferenceVertexInstanceUVs.GetNumIndices() != ResultVertexInstanceUVs.GetNumIndices())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex instance UVs channel count is different. UVs channel count expected [%d] result [%d]"),
			*AssetName,
			ReferenceVertexInstanceUVs.GetNumIndices(),
			ResultVertexInstanceUVs.GetNumIndices())));
		bAllSame = false;
	}
	else
	{
		for (int32 UVChannelIndex = 0; UVChannelIndex < ReferenceVertexInstanceUVs.GetNumIndices(); ++UVChannelIndex)
		{
			const TMeshAttributeArray<FVector2D, FVertexInstanceID>& ReferenceUVs = ReferenceVertexInstanceUVs.GetArrayForIndex(UVChannelIndex);
			const TMeshAttributeArray<FVector2D, FVertexInstanceID>& ResultUVs = ResultVertexInstanceUVs.GetArrayForIndex(UVChannelIndex);
			if (ReferenceUVs.Num() != ResultUVs.Num())
			{
				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex instance UV channel[%d] UVs count is different. Uvs count expected [%d] result [%d]"),
					*AssetName,
					UVChannelIndex,
					ReferenceUVs.Num(),
					ResultUVs.Num())));
				bAllSame = false;
			}
			else
			{
				for (FVertexInstanceID VertexInstanceID : ReferenceMeshDescription->VertexInstances().GetElementIDs())
				{
					if (ReferenceUVs[VertexInstanceID] != ResultUVs[VertexInstanceID])
					{
						ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, vertex instance UV Channel[%d] UVs array is different. VertexInstanceID [%d] expected UV [%s] result [%s]"),
							*AssetName,
							UVChannelIndex,
							VertexInstanceID.GetValue(),
							*ReferenceUVs[VertexInstanceID].ToString(),
							*ResultUVs[VertexInstanceID].ToString())));
						bAllSame = false;
						break;
					}
				}
			}
		}
	}
	//Edges
	//We check if hard edges are kept correctly
	if (ReferenceEdgeHardnesses.Num() != ResultEdgeHardnesses.Num())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, Edge count is different. Edges count expected [%d] result [%d]"),
			*AssetName,
			ReferenceEdgeHardnesses.Num(),
			ResultEdgeHardnesses.Num())));
		bAllSame = false;
	}
	else
	{
		for (const FEdgeID& EdgeID : MeshDescription->Edges().GetElementIDs())
		{
			if (ReferenceEdgeHardnesses[EdgeID] != ResultEdgeHardnesses[EdgeID])
			{
				//Make sure it is not an external edge (only one polygon connected) since it is impossible to retain this information in a smoothing group.
				//External edge hardnesses has no impact on the normal calculation. It is useful only when editing meshes.
				const TArray<FPolygonID>& EdgeConnectedPolygons = MeshDescription->GetEdgeConnectedPolygons(EdgeID);
				if (EdgeConnectedPolygons.Num() > 1)
				{
					ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, Edge hardnesses array is different. EdgeID [%d] expected hardnesse [%s] result [%s]"),
						*AssetName,
						EdgeID.GetValue(),
						(ReferenceEdgeHardnesses[EdgeID] ? TEXT("true") : TEXT("false")),
						(ResultEdgeHardnesses[EdgeID] ? TEXT("true") : TEXT("false")))));
					bAllSame = false;
				}
				//break;
			}
		}
	}
	//Polygon group ID
	//We currently rely only on the PolygonGroupID. The duplicate material slot information (the info is store in the staticmesh material array) is not necessary and cannot be put in FRawMesh structure.
	if (ReferenceMeshDescription->PolygonGroups().Num() != MeshDescription->PolygonGroups().Num())
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, Polygon group count is different. Polygon group count expected [%d] result [%d]"),
			*AssetName,
			ReferenceMeshDescription->PolygonGroups().Num(),
			MeshDescription->PolygonGroups().Num())));
		bAllSame = false;
	}
	else
	{
		for (const FPolygonGroupID& PolygonGroupID : ReferenceMeshDescription->PolygonGroups().GetElementIDs())
		{
			if (ReferencePolygonGroupMaterialIndex[PolygonGroupID] != ResultPolygonGroupMaterialIndex[PolygonGroupID])
			{
				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, FString::Printf(TEXT("The %s conversion to RawMesh is not lossless, polygon group material index array is different. PolygonGroupID [%d] expected Material index [%d] result [%d]"),
					*AssetName,
					PolygonGroupID.GetValue(),
					ReferencePolygonGroupMaterialIndex[PolygonGroupID],
					ResultPolygonGroupMaterialIndex[PolygonGroupID])));
				bAllSame = false;
				break;
			}
		}
	}
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

#if UE_BUILD_DEBUG
		AssetMesh->BuildCacheAutomationTestGuid = FGuid::NewGuid();
#endif

		if (AssetMesh != nullptr)
		{
			const UMeshDescription* ReferenceAssetMesh = AssetMesh->GetOriginalMeshDescription(0);
			check(ReferenceAssetMesh != nullptr);
			//Create a temporary Mesh Description
			UMeshDescription* ResultAssetMesh = DuplicateObject<UMeshDescription>(ReferenceAssetMesh, GetTransientPackage(), NAME_None);
			//Convert MeshDescription to FRawMesh
			FRawMesh RawMesh;
			FMeshDescriptionOperations::ConverToRawMesh(ResultAssetMesh, RawMesh);
			//Convert back the FRawmesh
			FMeshDescriptionOperations::ConverFromRawMesh(RawMesh, ResultAssetMesh);
			if (!CompareMeshDescription(AssetName, ExecutionInfo, ReferenceAssetMesh, ResultAssetMesh))
			{
				bAllSame = false;
			}
			//Destroy temporary object
			ResultAssetMesh->MarkPendingKill();
		}
	}
	//Collect garbage pass to remove everything from memory
	TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	return bAllSame;
}
