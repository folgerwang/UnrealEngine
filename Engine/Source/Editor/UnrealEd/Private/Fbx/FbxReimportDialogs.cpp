// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Static mesh creation from FBX data.
	Largely based on StaticMeshEdit.cpp
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "UObject/Object.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Editor.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "FbxImporter.h"
#include "Misc/FbxErrors.h"
#include "HAL/FileManager.h"
#include "Factories/FbxSceneImportFactory.h"
#include "Toolkits/AssetEditorManager.h"
#include "AssetRegistryModule.h"

//Windows dialog popup
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "FbxCompareWindow.h"
#include "FbxMaterialConflictWindow.h"

//Meshes includes
#include "MeshUtilities.h"
#include "RawMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "GeomFitUtils.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"

//Static mesh includes
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "StaticMeshResources.h"
#include "Factories/FbxStaticMeshImportData.h"

//Skeletal mesh includes
#include "SkelImport.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AnimEncoding.h"
#include "ApexClothingUtils.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Assets/ClothingAsset.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Rendering/SkeletalMeshModel.h"



#define LOCTEXT_NAMESPACE "FbxPreviewReimport"

using namespace UnFbx;

struct FCreateCompFromFbxArg
{
	FString MeshName;
	bool IsStaticMesh;
	bool IsStaticHasLodGroup;
};

void CreateCompFromSkeletalMesh(USkeletalMesh* SkeletalMesh, FCompMesh &CurrentData)
{
	//Fill the material array
	CurrentData.CompMaterials.AddZeroed(SkeletalMesh->Materials.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < SkeletalMesh->Materials.Num(); ++MaterialIndex)
	{
		const FSkeletalMaterial &Material = SkeletalMesh->Materials[MaterialIndex];
		FCompMaterial CompMaterial(Material.MaterialSlotName, Material.ImportedMaterialSlotName);
		CurrentData.CompMaterials[MaterialIndex] = CompMaterial;
	}
		
	//Fill the section topology
	if (SkeletalMesh->GetImportedModel())
	{
		CurrentData.CompLods.AddZeroed(SkeletalMesh->GetImportedModel()->LODModels.Num());
		//Fill sections data
		for (int32 LodIndex = 0; LodIndex < SkeletalMesh->GetImportedModel()->LODModels.Num(); ++LodIndex)
		{
			//Find the LodMaterialMap, which must be use for all LOD except the base
			TArray<int32> LODMaterialMap;
			if(LodIndex > 0 && SkeletalMesh->IsValidLODIndex(LodIndex))
			{
				LODMaterialMap = SkeletalMesh->GetLODInfo(LodIndex)->LODMaterialMap;
			}

			const FSkeletalMeshLODModel &StaticLodModel = SkeletalMesh->GetImportedModel()->LODModels[LodIndex];
			CurrentData.CompLods[LodIndex].Sections.AddZeroed(StaticLodModel.Sections.Num());
			for (int32 SectionIndex = 0; SectionIndex < StaticLodModel.Sections.Num(); ++SectionIndex)
			{
				const FSkelMeshSection &SkelMeshSection = StaticLodModel.Sections[SectionIndex];
				int32 MaterialIndex = SkelMeshSection.MaterialIndex;
				if (LodIndex > 0 && LODMaterialMap.IsValidIndex(MaterialIndex))
				{
					MaterialIndex = LODMaterialMap[MaterialIndex];
				}
				CurrentData.CompLods[LodIndex].Sections[SectionIndex].MaterialIndex = MaterialIndex;
			}
		}
	}

	//Fill the skeleton joint
	CurrentData.CompSkeleton.Joints.AddZeroed(SkeletalMesh->RefSkeleton.GetNum());
	for (int JointIndex = 0; JointIndex < CurrentData.CompSkeleton.Joints.Num(); ++JointIndex)
	{
		CurrentData.CompSkeleton.Joints[JointIndex].Name = SkeletalMesh->RefSkeleton.GetBoneName(JointIndex);
		CurrentData.CompSkeleton.Joints[JointIndex].ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(JointIndex);
		int32 ParentIndex = CurrentData.CompSkeleton.Joints[JointIndex].ParentIndex;
		if (CurrentData.CompSkeleton.Joints.IsValidIndex(ParentIndex))
		{
			CurrentData.CompSkeleton.Joints[ParentIndex].ChildIndexes.Add(JointIndex);
		}
	}

	USkeleton* Skeleton = SkeletalMesh->Skeleton;
	if (Skeleton != nullptr && !Skeleton->MergeAllBonesToBoneTree(SkeletalMesh))
	{
		CurrentData.CompSkeleton.bSkeletonFitMesh = false;
	}
}

void CreateCompFromStaticMesh(UStaticMesh* StaticMesh, FCompMesh &CurrentData)
{
	//Fill the material array
	CurrentData.CompMaterials.AddZeroed(StaticMesh->StaticMaterials.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < StaticMesh->StaticMaterials.Num(); ++MaterialIndex)
	{
		const FStaticMaterial &Material = StaticMesh->StaticMaterials[MaterialIndex];
		FCompMaterial CompMaterial(Material.MaterialSlotName, Material.ImportedMaterialSlotName);
		CurrentData.CompMaterials[MaterialIndex] = CompMaterial;
	}

	//Fill the section topology
	if (StaticMesh->RenderData)
	{
		CurrentData.CompLods.AddZeroed(StaticMesh->RenderData->LODResources.Num());

		//Fill sections data
		for (int32 LodIndex = 0; LodIndex < StaticMesh->RenderData->LODResources.Num(); ++LodIndex)
		{
			//StaticMesh->SectionInfoMap.Get()

			const FStaticMeshLODResources &StaticLodRessources = StaticMesh->RenderData->LODResources[LodIndex];
			CurrentData.CompLods[LodIndex].Sections.AddZeroed(StaticLodRessources.Sections.Num());
			for (int32 SectionIndex = 0; SectionIndex < StaticLodRessources.Sections.Num(); ++SectionIndex)
			{
				const FStaticMeshSection &StaticMeshSection = StaticLodRessources.Sections[SectionIndex];
				int32 MaterialIndex = StaticMeshSection.MaterialIndex;
				if (StaticMesh->SectionInfoMap.IsValidSection(LodIndex, SectionIndex))
				{
					FMeshSectionInfo MeshSectionInfo = StaticMesh->SectionInfoMap.Get(LodIndex, SectionIndex);
					MaterialIndex = MeshSectionInfo.MaterialIndex;
				}
				CurrentData.CompLods[LodIndex].Sections[SectionIndex].MaterialIndex = MaterialIndex;
			}
		}
	}
}

void FFbxImporter::FillGeneralFbxFileInformation(void *GeneralInfoPtr)
{
	FGeneralFbxFileInfo &FbxGeneralInfo = *(FGeneralFbxFileInfo*)GeneralInfoPtr;
	FbxAxisSystem SourceSetup = Scene->GetGlobalSettings().GetAxisSystem();

	//Get the UE4 sdk version
	int32 SDKMajor, SDKMinor, SDKRevision;
	FbxManager::GetFileFormatVersion(SDKMajor, SDKMinor, SDKRevision);

	int32 FileMajor, FileMinor, FileRevision;
	Importer->GetFileVersion(FileMajor, FileMinor, FileRevision);

	FString DateVersion = FString(FbxManager::GetVersion(false));
	FbxGeneralInfo.UE4SdkVersion = TEXT("UE4 Sdk Version: ") + FString::FromInt(SDKMajor) + TEXT(".") + FString::FromInt(SDKMinor) + TEXT(".") + FString::FromInt(SDKRevision) + TEXT(" (") + DateVersion + TEXT(")");

	FbxIOFileHeaderInfo *FileHeaderInfo = Importer->GetFileHeaderInfo();
	if (FileHeaderInfo)
	{
		FbxGeneralInfo.ApplicationCreator = TEXT("Creator:    ") + FString(FileHeaderInfo->mCreator.Buffer());
		FbxGeneralInfo.FileVersion = TEXT("Fbx File Version:    ") + FString::FromInt(FileMajor) + TEXT(".") + FString::FromInt(FileMinor) + TEXT(".") + FString::FromInt(FileRevision) + TEXT(" (") + FString::FromInt(FileHeaderInfo->mFileVersion) + TEXT(")");
		FbxGeneralInfo.CreationDate = TEXT("Created Time:    ") + FString::FromInt(FileHeaderInfo->mCreationTimeStamp.mYear) + TEXT("-") + FString::FromInt(FileHeaderInfo->mCreationTimeStamp.mMonth) + TEXT("-") + FString::FromInt(FileHeaderInfo->mCreationTimeStamp.mDay) + TEXT(" (Y-M-D)");
	}
	int32 UpVectorSign = 1;
	FbxAxisSystem::EUpVector UpVector = FileAxisSystem.GetUpVector(UpVectorSign);

	int32 FrontVectorSign = 1;
	FbxAxisSystem::EFrontVector FrontVector = FileAxisSystem.GetFrontVector(FrontVectorSign);


	FbxAxisSystem::ECoordSystem CoordSystem = FileAxisSystem.GetCoorSystem();

	FbxGeneralInfo.AxisSystem = TEXT("File Axis System:    UP: ");
	if (UpVectorSign == -1)
	{
		FbxGeneralInfo.AxisSystem = TEXT("-");
	}
	FbxGeneralInfo.AxisSystem += (UpVector == FbxAxisSystem::EUpVector::eXAxis) ? TEXT("X, Front: ") : (UpVector == FbxAxisSystem::EUpVector::eYAxis) ? TEXT("Y, Front: ") : TEXT("Z, Front: ");
	if (FrontVectorSign == -1)
	{
		FbxGeneralInfo.AxisSystem = TEXT("-");
	}

	if (UpVector == FbxAxisSystem::EUpVector::eXAxis)
	{
		FbxGeneralInfo.AxisSystem += (FrontVector == FbxAxisSystem::EFrontVector::eParityEven) ? TEXT("Y") : TEXT("Z");
	}
	else if (UpVector == FbxAxisSystem::EUpVector::eYAxis)
	{
		FbxGeneralInfo.AxisSystem += (FrontVector == FbxAxisSystem::EFrontVector::eParityEven) ? TEXT("X") : TEXT("Z");
	}
	else if (UpVector == FbxAxisSystem::EUpVector::eZAxis)
	{
		FbxGeneralInfo.AxisSystem += (FrontVector == FbxAxisSystem::EFrontVector::eParityEven) ? TEXT("X") : TEXT("Y");
	}

	//Hand side
	FbxGeneralInfo.AxisSystem += (CoordSystem == FbxAxisSystem::ECoordSystem::eLeftHanded) ? TEXT(" Left Handed") : TEXT(" Right Handed");


	if (FileAxisSystem == FbxAxisSystem::MayaZUp)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (Maya ZUp)");
	}
	else if (FileAxisSystem == FbxAxisSystem::MayaYUp)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (Maya YUp)");
	}
	else if (FileAxisSystem == FbxAxisSystem::Max)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (Max)");
	}
	else if (FileAxisSystem == FbxAxisSystem::Motionbuilder)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (Motion Builder)");
	}
	else if (FileAxisSystem == FbxAxisSystem::OpenGL)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (OpenGL)");
	}
	else if (FileAxisSystem == FbxAxisSystem::DirectX)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (DirectX)");
	}
	else if (FileAxisSystem == FbxAxisSystem::Lightwave)
	{
		FbxGeneralInfo.AxisSystem += TEXT(" (Lightwave)");
	}

	FbxGeneralInfo.UnitSystem = TEXT("Units:    ");
	if (FileUnitSystem == FbxSystemUnit::mm)
	{
		FbxGeneralInfo.UnitSystem += TEXT("mm (millimeter)");
	}
	else if (FileUnitSystem == FbxSystemUnit::cm)
	{
		FbxGeneralInfo.UnitSystem += TEXT("cm (centimeter)");
	}
	else if (FileUnitSystem == FbxSystemUnit::dm)
	{
		FbxGeneralInfo.UnitSystem += TEXT("dm (decimeter)");
	}
	else if (FileUnitSystem == FbxSystemUnit::m)
	{
		FbxGeneralInfo.UnitSystem += TEXT("m (meter)");
	}
	else if (FileUnitSystem == FbxSystemUnit::km)
	{
		FbxGeneralInfo.UnitSystem += TEXT("km (kilometer)");
	}
	else if (FileUnitSystem == FbxSystemUnit::Inch)
	{
		FbxGeneralInfo.UnitSystem += TEXT("Inch");
	}
	else if (FileUnitSystem == FbxSystemUnit::Foot)
	{
		FbxGeneralInfo.UnitSystem += TEXT("Foot");
	}
	else if (FileUnitSystem == FbxSystemUnit::Yard)
	{
		FbxGeneralInfo.UnitSystem += TEXT("Yard");
	}
	else if (FileUnitSystem == FbxSystemUnit::Mile)
	{
		FbxGeneralInfo.UnitSystem += TEXT("Mile");
	}
}
void FFbxImporter::ShowFbxCompareWindow(UObject *SourceObj, UObject *ResultObj, bool &UserCancel)
{
	if (SourceObj == nullptr || ResultObj == nullptr)
	{
		return;
	}
	
	//Show a dialog if there is some conflict
	UStaticMesh *SourceStaticMesh = Cast<UStaticMesh>(SourceObj);
	UStaticMesh *ResultStaticMesh = Cast<UStaticMesh>(ResultObj);

	USkeletalMesh *SourceSkeletalMesh = Cast<USkeletalMesh>(SourceObj);
	USkeletalMesh *ResultSkeletalMesh = Cast<USkeletalMesh>(ResultObj);

	FCompMesh SourceData;
	FCompMesh ResultData;

	//Create the current data to compare from
	if (SourceStaticMesh && ResultStaticMesh)
	{
		CreateCompFromStaticMesh(SourceStaticMesh, SourceData);
		CreateCompFromStaticMesh(ResultStaticMesh, ResultData);
	}
	else if (SourceSkeletalMesh)
	{
		CreateCompFromSkeletalMesh(SourceSkeletalMesh, SourceData);
		CreateCompFromSkeletalMesh(ResultSkeletalMesh, ResultData);
	}
	//Query general information
	FGeneralFbxFileInfo FbxGeneralInfo;
	FillGeneralFbxFileInformation(&FbxGeneralInfo);
	
	TArray<TSharedPtr<FString>> AssetReferencingSkeleton;
	if (SourceSkeletalMesh != nullptr && SourceSkeletalMesh->Skeleton != nullptr && !ResultData.CompSkeleton.bSkeletonFitMesh)
	{
		UObject* SelectedObject = SourceSkeletalMesh->Skeleton;
		if (SelectedObject)
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			const FName SelectedPackageName = SelectedObject->GetOutermost()->GetFName();
			//Get the Hard dependencies
			TArray<FName> HardDependencies;
			AssetRegistryModule.Get().GetReferencers(SelectedPackageName, HardDependencies, EAssetRegistryDependencyType::Hard);
			//Get the Soft dependencies
			TArray<FName> SoftDependencies;
			AssetRegistryModule.Get().GetReferencers(SelectedPackageName, SoftDependencies, EAssetRegistryDependencyType::Soft);
			//Compose the All dependencies array
			TArray<FName> AllDependencies = HardDependencies;
			AllDependencies += SoftDependencies;
			if (AllDependencies.Num() > 0)
			{
				for (const FName AssetDependencyName : AllDependencies)
				{
					const FString PackageString = AssetDependencyName.ToString();
					const FString FullAssetPathName = FString::Printf(TEXT("%s.%s"), *PackageString, *FPackageName::GetLongPackageAssetName(PackageString));
					
					FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FName(*FullAssetPathName));
					if (AssetData.GetClass() != nullptr)
					{
						TSharedPtr<FString> AssetReferencing = MakeShareable(new FString(AssetData.AssetClass.ToString() + TEXT(" ") + FullAssetPathName));
						AssetReferencingSkeleton.Add(AssetReferencing);
					}
				}
			}
		}
	}

	//Create the modal dialog window to let the user see the result of the compare
	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(NSLOCTEXT("UnrealEd", "FbxCompareWindowTitle", "Reimport Reports"))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(700, 650))
		.MinWidth(700)
		.MinHeight(650);

	TSharedPtr<SFbxCompareWindow> FbxCompareWindow;
	Window->SetContent
		(
			SAssignNew(FbxCompareWindow, SFbxCompareWindow)
			.WidgetWindow(Window)
			.FbxGeneralInfo(FbxGeneralInfo)
			.AssetReferencingSkeleton(&AssetReferencingSkeleton)
			.SourceData(&SourceData)
			.ResultData(&ResultData)
			.SourceObject(SourceObj)
			.ResultObject(ResultObj)
			);

	if (FbxCompareWindow->HasConflict())
	{
		// @todo: we can make this slow as showing progress bar later
		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

		UserCancel = false;
	}
}

void FFbxImporter::ShowFbxMaterialConflictWindowSK(const TArray<FSkeletalMaterial>& InSourceMaterials, const TArray<FSkeletalMaterial>& InResultMaterials, TArray<int32>& RemapMaterials, TArray<bool>& AutoRemapMaterials, bool &UserCancel)
{
	TArray<FCompMaterial> SourceMaterials;
	TArray<FCompMaterial> ResultMaterials;

	SourceMaterials.Reserve(InSourceMaterials.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < InSourceMaterials.Num(); ++MaterialIndex)
	{
		SourceMaterials.Add(FCompMaterial(InSourceMaterials[MaterialIndex].MaterialSlotName, InSourceMaterials[MaterialIndex].ImportedMaterialSlotName));
	}

	ResultMaterials.Reserve(InResultMaterials.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < InResultMaterials.Num(); ++MaterialIndex)
	{
		ResultMaterials.Add(FCompMaterial(InResultMaterials[MaterialIndex].MaterialSlotName, InResultMaterials[MaterialIndex].ImportedMaterialSlotName));
	}

	//Create the modal dialog window to let the user see the result of the compare
	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(NSLOCTEXT("UnrealEd", "FbxMaterialConflictOpionsTitle", "Reimport Material Conflicts Resolution"))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(700, 370))
		.MinWidth(700)
		.MinHeight(370);

	TSharedPtr<SFbxMaterialConflictWindow> FbxMaterialConflictWindow;
	Window->SetContent
	(
		SAssignNew(FbxMaterialConflictWindow, SFbxMaterialConflictWindow)
		.WidgetWindow(Window)
		.SourceMaterials(&SourceMaterials)
		.ResultMaterials(&ResultMaterials)
		.RemapMaterials(&RemapMaterials)
		.AutoRemapMaterials(&AutoRemapMaterials)
	);

	// @todo: we can make this slow as showing progress bar later
	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	UserCancel = FbxMaterialConflictWindow->HasUserCancel();
}

void FFbxImporter::ShowFbxMaterialConflictWindowSM(const TArray<FStaticMaterial>& InSourceMaterials, const TArray<FStaticMaterial>& InResultMaterials, TArray<int32>& RemapMaterials, TArray<bool>& AutoRemapMaterials, bool &UserCancel)
{
	TArray<FCompMaterial> SourceMaterials;
	TArray<FCompMaterial> ResultMaterials;

	SourceMaterials.Reserve(InSourceMaterials.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < InSourceMaterials.Num(); ++MaterialIndex)
	{
		SourceMaterials.Add(FCompMaterial(InSourceMaterials[MaterialIndex].MaterialSlotName, InSourceMaterials[MaterialIndex].ImportedMaterialSlotName));
	}

	ResultMaterials.Reserve(InResultMaterials.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < InResultMaterials.Num(); ++MaterialIndex)
	{
		ResultMaterials.Add(FCompMaterial(InResultMaterials[MaterialIndex].MaterialSlotName, InResultMaterials[MaterialIndex].ImportedMaterialSlotName));
	}

	//Create the modal dialog window to let the user see the result of the compare
	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(NSLOCTEXT("UnrealEd", "FbxMaterialConflictOpionsTitle", "Reimport Material Conflicts Resolution"))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(700, 370))
		.MinWidth(700)
		.MinHeight(370);

	TSharedPtr<SFbxMaterialConflictWindow> FbxMaterialConflictWindow;
	Window->SetContent
	(
		SAssignNew(FbxMaterialConflictWindow, SFbxMaterialConflictWindow)
		.WidgetWindow(Window)
		.SourceMaterials(&SourceMaterials)
		.ResultMaterials(&ResultMaterials)
		.RemapMaterials(&RemapMaterials)
		.AutoRemapMaterials(&AutoRemapMaterials)
	);

	// @todo: we can make this slow as showing progress bar later
	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	UserCancel = FbxMaterialConflictWindow->HasUserCancel();
}


#undef LOCTEXT_NAMESPACE
