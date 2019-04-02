// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Factories/FbxSkeletalMeshImportData.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/Package.h"

UFbxSkeletalMeshImportData::UFbxSkeletalMeshImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bImportMeshesInBoneHierarchy(true)
{
	bTransformVertexToAbsolute = true;
	bBakePivotInVertex = false;
	VertexColorImportOption = EVertexColorImportOption::Replace;
	LastImportContentType = EFBXImportContentType::FBXICT_All;
}

UFbxSkeletalMeshImportData* UFbxSkeletalMeshImportData::GetImportDataForSkeletalMesh(USkeletalMesh* SkeletalMesh, UFbxSkeletalMeshImportData* TemplateForCreation)
{
	check(SkeletalMesh);
	
	UFbxSkeletalMeshImportData* ImportData = Cast<UFbxSkeletalMeshImportData>(SkeletalMesh->AssetImportData);
	if ( !ImportData )
	{
		ImportData = NewObject<UFbxSkeletalMeshImportData>(SkeletalMesh, NAME_None, RF_NoFlags, TemplateForCreation);

		// Try to preserve the source file data if possible
		if ( SkeletalMesh->AssetImportData != NULL )
		{
			ImportData->SourceData = SkeletalMesh->AssetImportData->SourceData;
		}

		SkeletalMesh->AssetImportData = ImportData;
	}

	return ImportData;
}

bool UFbxSkeletalMeshImportData::CanEditChange(const UProperty* InProperty) const
{
	bool bMutable = Super::CanEditChange(InProperty);
	UObject* Outer = GetOuter();
	if(Outer && bMutable)
	{
		// Let the FbxImportUi object handle the editability of our properties
		bMutable = Outer->CanEditChange(InProperty);
	}
	return bMutable;
}

bool UFbxSkeletalMeshImportData::GetImportContentFilename(FString& OutFilename, FString& OutFilenameLabel) const
{
	if (SourceData.SourceFiles.Num() < 1)
	{
		OutFilename = FString();
		OutFilenameLabel = FString();
		return false;
	}
	int32 SourceIndex = ImportContentType == EFBXImportContentType::FBXICT_All ? 0 : ImportContentType == EFBXImportContentType::FBXICT_Geometry ? 1 : 2;
	if (SourceData.SourceFiles.Num() > SourceIndex)
	{
		OutFilename = ResolveImportFilename(SourceData.SourceFiles[SourceIndex].RelativeFilename);
		OutFilenameLabel = SourceData.SourceFiles[SourceIndex].DisplayLabelName;
		return true;
	}
	OutFilename = ResolveImportFilename(SourceData.SourceFiles[0].RelativeFilename);
	OutFilenameLabel = SourceData.SourceFiles[0].DisplayLabelName;
	return true;
}

void UFbxSkeletalMeshImportData::AppendAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags)
{
	auto EFBXImportContentTypeToString = [](const EFBXImportContentType value)-> FString
	{
		switch (value)
		{
		case EFBXImportContentType::FBXICT_All:
			return TEXT("FBXICT_All");
		case EFBXImportContentType::FBXICT_Geometry:
			return TEXT("FBXICT_Geometry");
		case EFBXImportContentType::FBXICT_SkinningWeights:
			return TEXT("FBXICT_SkinningWeights");
		case EFBXImportContentType::FBXICT_MAX:
			return TEXT("FBXICT_MAX");
		}
		return TEXT("FBXICT_All");
	};

	FString EnumString = EFBXImportContentTypeToString(LastImportContentType);
	OutTags.Add(FAssetRegistryTag("LastImportContentType", EnumString, FAssetRegistryTag::TT_Hidden));

	Super::AppendAssetRegistryTags(OutTags);
}