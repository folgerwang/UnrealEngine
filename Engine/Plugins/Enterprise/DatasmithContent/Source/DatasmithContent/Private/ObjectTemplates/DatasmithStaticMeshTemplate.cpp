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

	// The settings for RecomputeNormals and RecomputeTangents when True must be honored irrespective of the previous template settings
	// because their values are determined by ShouldRecomputeNormals/ShouldRecomputeTangents which determine if they are needed by the renderer
	Destination->bRecomputeNormals = PreviousTemplate ? Destination->bRecomputeNormals | bRecomputeNormals : bRecomputeNormals;

	Destination->bRecomputeTangents = PreviousTemplate ? Destination->bRecomputeTangents | bRecomputeTangents : bRecomputeTangents;

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

bool FDatasmithMeshBuildSettingsTemplate::Equals( const FDatasmithMeshBuildSettingsTemplate& Other ) const
{
	bool bEquals = bUseMikkTSpace == Other.bUseMikkTSpace;
	bEquals = bEquals && ( bRecomputeNormals == Other.bRecomputeNormals );
	bEquals = bEquals && ( bRecomputeTangents == Other.bRecomputeTangents );
	bEquals = bEquals && ( bRemoveDegenerates == Other.bRemoveDegenerates );
	bEquals = bEquals && ( bBuildAdjacencyBuffer == Other.bBuildAdjacencyBuffer );
	bEquals = bEquals && ( bUseHighPrecisionTangentBasis == Other.bUseHighPrecisionTangentBasis );
	bEquals = bEquals && ( bUseFullPrecisionUVs == Other.bUseFullPrecisionUVs );
	bEquals = bEquals && ( bGenerateLightmapUVs == Other.bGenerateLightmapUVs );
	bEquals = bEquals && ( MinLightmapResolution == Other.MinLightmapResolution );
	bEquals = bEquals && ( SrcLightmapIndex == Other.SrcLightmapIndex );
	bEquals = bEquals && ( DstLightmapIndex == Other.DstLightmapIndex );

	return bEquals;
}

FDatasmithStaticMaterialTemplate::FDatasmithStaticMaterialTemplate()
{
	Load( FStaticMaterial() ); // Initialize from default object
}

void FDatasmithStaticMaterialTemplate::Apply( FStaticMaterial* Destination, FDatasmithStaticMaterialTemplate* PreviousTemplate )
{
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( MaterialSlotName, Destination, PreviousTemplate );
	DATASMITHOBJECTTEMPLATE_CONDITIONALSET( MaterialInterface, Destination, PreviousTemplate );

	Destination->ImportedMaterialSlotName = MaterialSlotName; // Not editable by the user, so always set it
}

void FDatasmithStaticMaterialTemplate::Load( const FStaticMaterial& Source )
{
	MaterialSlotName = Source.MaterialSlotName;
	MaterialInterface = Source.MaterialInterface;
}

bool FDatasmithStaticMaterialTemplate::Equals( const FDatasmithStaticMaterialTemplate& Other ) const
{
	bool bEquals = MaterialSlotName == Other.MaterialSlotName;
	bEquals = bEquals && ( MaterialInterface == Other.MaterialInterface );

	return bEquals;
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

bool FDatasmithMeshSectionInfoTemplate::Equals( const FDatasmithMeshSectionInfoTemplate& Other ) const
{
	return MaterialIndex == Other.MaterialIndex;
}

void FDatasmithMeshSectionInfoMapTemplate::Apply( FMeshSectionInfoMap* Destination, FDatasmithMeshSectionInfoMapTemplate* PreviousTemplate )
{
	for ( auto It = Map.CreateIterator(); It; ++It )
	{
		It->Value.Apply( &Destination->Map.FindOrAdd( It->Key ), PreviousTemplate ? PreviousTemplate->Map.Find( It->Key ) : nullptr );
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

bool FDatasmithMeshSectionInfoMapTemplate::Equals( const FDatasmithMeshSectionInfoMapTemplate& Other ) const
{
	bool bEquals = ( Map.Num() == Other.Map.Num() );

	if ( bEquals )
	{
		for ( const auto& It : Map )
		{
			bEquals = bEquals && ( Other.Map.Contains( It.Key ) );

			if ( bEquals )
			{
				bEquals = bEquals && It.Value.Equals( *Other.Map.Find( It.Key ) );
			}

			if ( !bEquals )
			{
				break;
			}
		}
	}
	
	return bEquals;
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

bool UDatasmithStaticMeshTemplate::Equals( const UDatasmithObjectTemplate* Other ) const
{
	const UDatasmithStaticMeshTemplate* TypedOther = Cast< UDatasmithStaticMeshTemplate >( Other );

	if ( !TypedOther )
	{
		return false;
	}

	bool bEquals = LightMapCoordinateIndex == TypedOther->LightMapCoordinateIndex;
	bEquals = bEquals && ( LightMapResolution == TypedOther->LightMapResolution );

	bEquals = bEquals && SectionInfoMap.Equals( TypedOther->SectionInfoMap );

	// Build settings
	bEquals = bEquals && ( BuildSettings.Num() == TypedOther->BuildSettings.Num() );

	if ( bEquals )
	{
		for ( int32 BuildSettingIndex = 0; BuildSettingIndex < BuildSettings.Num(); ++BuildSettingIndex )
		{
			bEquals = bEquals && BuildSettings[ BuildSettingIndex ].Equals( TypedOther->BuildSettings[ BuildSettingIndex ] );

			if ( !bEquals )
			{
				return false;
			}
		}
	}

	// Materials
	bEquals = bEquals && ( StaticMaterials.Num() == TypedOther->StaticMaterials.Num() );

	if ( bEquals )
	{
		for ( int32 StaticMaterialIndex = 0; StaticMaterialIndex < StaticMaterials.Num(); ++StaticMaterialIndex )
		{
			bEquals = bEquals && StaticMaterials[ StaticMaterialIndex ].Equals( TypedOther->StaticMaterials[ StaticMaterialIndex ] );

			if ( !bEquals )
			{
				return false;
			}
		}
	}

	return bEquals;
}
