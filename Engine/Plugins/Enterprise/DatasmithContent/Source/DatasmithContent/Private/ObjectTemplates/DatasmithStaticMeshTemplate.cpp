// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ObjectTemplates/DatasmithStaticMeshTemplate.h"

#include "DatasmithAssetUserData.h"

#include "CoreTypes.h"
#include "Engine/StaticMesh.h"
#include "Templates/Casts.h"

FDatasmithMeshBuildSettingsTemplate::FDatasmithMeshBuildSettingsTemplate()
{
	Load ( FMeshBuildSettings() ); // Initialize from default object
}

void FDatasmithMeshBuildSettingsTemplate::Apply( FMeshBuildSettings* Destination, FDatasmithMeshBuildSettingsTemplate* PreviousTemplate )
{
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bUseMikkTSpace, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bRecomputeNormals, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bRecomputeTangents, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bRemoveDegenerates, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bBuildAdjacencyBuffer, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bUseHighPrecisionTangentBasis, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bUseFullPrecisionUVs, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( bGenerateLightmapUVs, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( MinLightmapResolution, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( SrcLightmapIndex, Destination, PreviousTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( DstLightmapIndex, Destination, PreviousTemplate );
}

void FDatasmithMeshBuildSettingsTemplate::Load( const FMeshBuildSettings& Source )
{
	bUseMikkTSpace = Source.bUseMikkTSpace;
	bRecomputeNormals = Source.bRecomputeNormals;
	bRecomputeTangents = Source.bRecomputeTangents;
	bRemoveDegenerates = Source.bRemoveDegenerates;
	bBuildAdjacencyBuffer = Source.bBuildAdjacencyBuffer;
	bUseHighPrecisionTangentBasis = Source.bUseHighPrecisionTangentBasis;
	bUseFullPrecisionUVs = Source.bUseFullPrecisionUVs;
	bGenerateLightmapUVs = Source.bGenerateLightmapUVs;
	MinLightmapResolution = Source.MinLightmapResolution;
	SrcLightmapIndex = Source.SrcLightmapIndex;
	DstLightmapIndex = Source.DstLightmapIndex;
}

FDatasmithStaticMaterialTemplate::FDatasmithStaticMaterialTemplate()
{
	Load( FStaticMaterial() ); // Initialize from default object
}

void FDatasmithStaticMaterialTemplate::Apply( FStaticMaterial* Destination, FDatasmithStaticMaterialTemplate* PreviousTemplate )
{
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( MaterialSlotName, Destination, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( MaterialInterface, Destination, PreviousTemplate );
}

void FDatasmithStaticMaterialTemplate::Load( const FStaticMaterial& Source )
{
	MaterialSlotName = Source.MaterialSlotName;
	MaterialInterface = Source.MaterialInterface;
}

FDatasmithMeshSectionInfoTemplate::FDatasmithMeshSectionInfoTemplate()
{
	Load( FMeshSectionInfo() ); // Initialize from default object
}

void FDatasmithMeshSectionInfoTemplate::Apply( FMeshSectionInfo* Destination, FDatasmithMeshSectionInfoTemplate* PreviousTemplate )
{
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( MaterialIndex, Destination, PreviousTemplate );
}

void FDatasmithMeshSectionInfoTemplate::Load( const FMeshSectionInfo& Source )
{
	MaterialIndex = Source.MaterialIndex;
}

void FDatasmithMeshSectionInfoMapTemplate::Apply( FMeshSectionInfoMap* Destination, FDatasmithMeshSectionInfoMapTemplate* PreviousTemplate )
{
	for ( auto It = Map.CreateIterator(); It; ++It )
	{
		It->Value.Apply( &Destination->Map.Add( It->Key ), PreviousTemplate ? PreviousTemplate->Map.Find( It->Key ) : nullptr );
	}
}

void FDatasmithMeshSectionInfoMapTemplate::Load( const FMeshSectionInfoMap& Source )
{
	Map.Empty();

	for ( auto It = Source.Map.CreateConstIterator(); It; ++It )
	{
		FDatasmithMeshSectionInfoTemplate MeshSectionInfoTemplate;
		MeshSectionInfoTemplate.Load( It->Value );

		Map.Add( It->Key, MoveTemp( MeshSectionInfoTemplate ) );
	}
}

void UDatasmithStaticMeshTemplate::Apply( UObject* Destination, bool bForce )
{
#if WITH_EDITORONLY_DATA
	UStaticMesh* StaticMesh = Cast< UStaticMesh >( Destination );

	if ( !StaticMesh )
	{
		return;
	}

	UDatasmithStaticMeshTemplate* PreviousStaticMeshTemplate = !bForce ? FDatasmithObjectTemplateUtils::GetObjectTemplate< UDatasmithStaticMeshTemplate >( StaticMesh ) : nullptr;

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( LightMapCoordinateIndex, StaticMesh, PreviousStaticMeshTemplate );

	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( LightMapResolution, StaticMesh, PreviousStaticMeshTemplate );

	// Section info map
	SectionInfoMap.Apply( &StaticMesh->SectionInfoMap, PreviousStaticMeshTemplate ? &PreviousStaticMeshTemplate->SectionInfoMap : nullptr );

	// Build settings
	for ( int32 SourceModelIndex = 0; SourceModelIndex < BuildSettings.Num(); ++SourceModelIndex )
	{
		if ( !StaticMesh->SourceModels.IsValidIndex( SourceModelIndex ) )
		{
			continue;
		}

		FStaticMeshSourceModel& SourceModel = StaticMesh->SourceModels[ SourceModelIndex ];
		
		FDatasmithMeshBuildSettingsTemplate* PreviousBuildSettingsTemplate = nullptr;
		
		if ( PreviousStaticMeshTemplate && PreviousStaticMeshTemplate->BuildSettings.IsValidIndex( SourceModelIndex ) )
		{
			PreviousBuildSettingsTemplate = &PreviousStaticMeshTemplate->BuildSettings[ SourceModelIndex ];
		}

		BuildSettings[ SourceModelIndex ].Apply( &SourceModel.BuildSettings, PreviousBuildSettingsTemplate );
	}

	// Materials
	for ( int32 StaticMaterialIndex = 0; StaticMaterialIndex < StaticMaterials.Num(); ++StaticMaterialIndex )
	{
		if ( !StaticMesh->StaticMaterials.IsValidIndex( StaticMaterialIndex ) )
		{
			StaticMesh->StaticMaterials.AddDefaulted();
		}

		FStaticMaterial& StaticMaterial = StaticMesh->StaticMaterials[ StaticMaterialIndex ];

		FDatasmithStaticMaterialTemplate* PreviousStaticMaterialTemplate = nullptr;

		if ( PreviousStaticMeshTemplate && PreviousStaticMeshTemplate->StaticMaterials.IsValidIndex( StaticMaterialIndex ) )
		{
			PreviousStaticMaterialTemplate = &PreviousStaticMeshTemplate->StaticMaterials[ StaticMaterialIndex ];
		}

		StaticMaterials[ StaticMaterialIndex ].Apply( &StaticMaterial, PreviousStaticMaterialTemplate );
	}

	if ( PreviousStaticMeshTemplate )
	{
		// Remove materials that aren't in the template anymore
		for ( int32 MaterialIndexToRemove = PreviousStaticMeshTemplate->StaticMaterials.Num() - 1; MaterialIndexToRemove >= StaticMaterials.Num(); --MaterialIndexToRemove )
		{
			if ( StaticMesh->StaticMaterials.IsValidIndex( MaterialIndexToRemove ) )
			{
				StaticMesh->StaticMaterials.RemoveAt( MaterialIndexToRemove );
			}
		}
	}

	FDatasmithObjectTemplateUtils::SetObjectTemplate( Destination, this );
#endif // #if WITH_EDITORONLY_DATA
}

void UDatasmithStaticMeshTemplate::Load( const UObject* Source )
{
#if WITH_EDITORONLY_DATA
	const UStaticMesh* SourceStaticMesh = Cast< UStaticMesh >( Source );

	if ( !SourceStaticMesh )
	{
		return;
	}

	LightMapCoordinateIndex = SourceStaticMesh->LightMapCoordinateIndex;
	LightMapResolution = SourceStaticMesh->LightMapResolution;

	// Section info map
	SectionInfoMap.Load( SourceStaticMesh->SectionInfoMap );

	// Build settings
	BuildSettings.Empty( SourceStaticMesh->SourceModels.Num() );

	for ( const FStaticMeshSourceModel& SourceModel : SourceStaticMesh->SourceModels )
	{
		FDatasmithMeshBuildSettingsTemplate BuildSettingsTemplate;
		BuildSettingsTemplate.Load( SourceModel.BuildSettings );

		BuildSettings.Add( MoveTemp( BuildSettingsTemplate ) );
	}

	// Materials
	StaticMaterials.Empty( SourceStaticMesh->StaticMaterials.Num() );

	for ( const FStaticMaterial& StaticMaterial : SourceStaticMesh->StaticMaterials )
	{
		FDatasmithStaticMaterialTemplate StaticMaterialTemplate;
		StaticMaterialTemplate.Load( StaticMaterial );

		StaticMaterials.Add( MoveTemp( StaticMaterialTemplate ) );
	}
#endif // #if WITH_EDITORONLY_DATA
}