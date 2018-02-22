// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshUtilitiesLibrary.h"
#include "RawMesh.h"
#include "PhysicsEngine/BodySetup.h"

DEFINE_LOG_CATEGORY_STATIC(LogStaticMeshUtilitiesLibrary, Verbose, All);

void UStaticMeshUtilitiesLibrary::EnableSectionCollision(UStaticMesh* StaticMesh, bool bCollisionEnabled, int32 LODIndex, int32 SectionIndex)
{
	if(StaticMesh == nullptr)
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("NULL static mesh passed to EnableSectionCollision"));
		return;
	}

	if(LODIndex >= StaticMesh->GetNumLODs())
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("Invalid LOD index %d (of %d) passed to EnableSectionCollision"), LODIndex, StaticMesh->GetNumLODs());
		return;
	}

	if(SectionIndex >= StaticMesh->GetNumSections(LODIndex))
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("Invalid section index %d (of %d) passed to EnableSectionCollision"), SectionIndex, StaticMesh->GetNumSections(LODIndex));
		return;
	}

	StaticMesh->Modify();

	FMeshSectionInfo SectionInfo = StaticMesh->SectionInfoMap.Get(LODIndex, SectionIndex);

	SectionInfo.bEnableCollision = bCollisionEnabled;

	StaticMesh->SectionInfoMap.Set(LODIndex, SectionIndex, SectionInfo);

	StaticMesh->PostEditChange();
}

bool UStaticMeshUtilitiesLibrary::IsSectionCollisionEnabled(UStaticMesh* StaticMesh, int32 LODIndex, int32 SectionIndex)
{
	if(StaticMesh == nullptr)
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("NULL static mesh passed to IsSectionCollisionEnabled"));
		return false;
	}

	if(LODIndex >= StaticMesh->GetNumLODs())
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("Invalid LOD index %d (of %d) passed to IsSectionCollisionEnabled"), LODIndex, StaticMesh->GetNumLODs());
		return false;
	}

	if(SectionIndex >= StaticMesh->GetNumSections(LODIndex))
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("Invalid section index %d (of %d) passed to IsSectionCollisionEnabled"), SectionIndex, StaticMesh->GetNumSections(LODIndex));
		return false;
	}

	FMeshSectionInfo SectionInfo = StaticMesh->SectionInfoMap.Get(LODIndex, SectionIndex);
	return SectionInfo.bEnableCollision;
}

void UStaticMeshUtilitiesLibrary::EnableSectionCastShadow(UStaticMesh* StaticMesh, bool bCastShadow, int32 LODIndex, int32 SectionIndex)
{
	if(StaticMesh == nullptr)
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("NULL static mesh passed to EnableSectionCastShadow"));
		return;
	}

	if(LODIndex >= StaticMesh->GetNumLODs())
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("Invalid LOD index %d (of %d) passed to EnableSectionCastShadow"), LODIndex, StaticMesh->GetNumLODs());
		return;
	}

	if(SectionIndex >= StaticMesh->GetNumSections(LODIndex))
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("Invalid section index %d (of %d) passed to EnableSectionCastShadow"), SectionIndex, StaticMesh->GetNumSections(LODIndex));
		return;
	}

	StaticMesh->Modify();

	FMeshSectionInfo SectionInfo = StaticMesh->SectionInfoMap.Get(LODIndex, SectionIndex);

	SectionInfo.bCastShadow = bCastShadow;

	StaticMesh->SectionInfoMap.Set(LODIndex, SectionIndex, SectionInfo);

	StaticMesh->PostEditChange();
}

bool UStaticMeshUtilitiesLibrary::HasVertexColors(UStaticMesh* StaticMesh)
{
	if(StaticMesh == nullptr)
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("NULL static mesh passed to HasVertexColors"));
		return false;
	}

	for (FStaticMeshSourceModel& SourceModel : StaticMesh->SourceModels)
	{
		if (SourceModel.RawMeshBulkData && !SourceModel.RawMeshBulkData->IsEmpty())
		{
			FRawMesh RawMesh;
			SourceModel.RawMeshBulkData->LoadRawMesh(RawMesh);
			if (RawMesh.WedgeColors.Num() > 0)
			{
				return true;
			}
		}
	}

	return false;
}

bool UStaticMeshUtilitiesLibrary::HasInstanceVertexColors(UStaticMeshComponent* StaticMeshComponent)
{
	if(StaticMeshComponent == nullptr)
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("NULL static mesh component passed to HasVertexColors"));
		return false;
	}

	for (const FStaticMeshComponentLODInfo& CurrentLODInfo : StaticMeshComponent->LODData)
	{
		if(CurrentLODInfo.OverrideVertexColors != nullptr || CurrentLODInfo.PaintedVertices.Num() > 0)
		{
			return true;
		}
	}

	return false;
}

bool UStaticMeshUtilitiesLibrary::SetGenerateLightmapUVs(UStaticMesh* StaticMesh, bool bGenerateLightmapUVs)
{
	if (StaticMesh == nullptr)
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("NULL static mesh passed to SetGenerateLightmapUVs"));
		return false;
	}

	bool AnySettingsToChange = false;
	for (FStaticMeshSourceModel& SourceModel : StaticMesh->SourceModels)
	{
		//Make sure LOD is not a reduction before considering its BuildSettings
		if (SourceModel.RawMeshBulkData && !SourceModel.RawMeshBulkData->IsEmpty())
		{
			AnySettingsToChange = (SourceModel.BuildSettings.bGenerateLightmapUVs != bGenerateLightmapUVs);

			if (AnySettingsToChange)
			{
				break;
			}
		}
	}

	if (AnySettingsToChange)
	{
		StaticMesh->Modify();
		for (FStaticMeshSourceModel& SourceModel : StaticMesh->SourceModels)
		{
			SourceModel.BuildSettings.bGenerateLightmapUVs = bGenerateLightmapUVs;

		}

		StaticMesh->Build();
		StaticMesh->PostEditChange();
		return true;
	}

	return false;

}

TEnumAsByte<ECollisionTraceFlag> UStaticMeshUtilitiesLibrary::GetCollisionComplexity(UStaticMesh* StaticMesh)
{
	if(StaticMesh == nullptr)
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("NULL static mesh passed to GetCollisionComplexity"));
		return ECollisionTraceFlag::CTF_UseDefault;
	}

	if(StaticMesh->BodySetup)
	{
		return StaticMesh->BodySetup->CollisionTraceFlag;
	}

	return ECollisionTraceFlag::CTF_UseDefault;
}

int32 UStaticMeshUtilitiesLibrary::GetNumberVerts(UStaticMesh* StaticMesh, int32 LODIndex)
{
	if (StaticMesh == nullptr)
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("NULL static mesh passed to GetNumVerts"));
		return 0;
	}

	return StaticMesh->GetNumVertices(LODIndex);

}

TArray<float> UStaticMeshUtilitiesLibrary::GetLODScreenSizes(UStaticMesh* StaticMesh)
{
	TArray<float> ScreenSizes;

	if (StaticMesh == nullptr)
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("NULL static mesh passed to GetLODScreenSizes"));
		return ScreenSizes;
	}

	for (int i = 0; i < StaticMesh->GetNumLODs(); i++)
	{
		float CurScreenSize = StaticMesh->RenderData->ScreenSize[i].Default;
		ScreenSizes.Add(CurScreenSize);
	}

	return ScreenSizes;

}

void UStaticMeshUtilitiesLibrary::SetAllowCPUAccess(UStaticMesh* StaticMesh, bool bAllowCPUAccess)
{
	if (StaticMesh == nullptr)
	{
		UE_LOG(LogStaticMeshUtilitiesLibrary, Warning, TEXT("NULL static mesh passed to SetAllowCPUAccess"));
		return;
	}

	StaticMesh->Modify();
	StaticMesh->bAllowCPUAccess = bAllowCPUAccess;
	StaticMesh->PostEditChange();
}