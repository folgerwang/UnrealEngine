// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ALandscape;
struct FLandscapeFileResolution;
struct FLandscapeImportLayerInfo;
class ULandscapeEditorObject;

class LANDSCAPEEDITOR_API FNewLandscapeUtils
{
public:
	static void ChooseBestComponentSizeForImport( ULandscapeEditorObject* UISettings );
	static void ImportLandscapeData( ULandscapeEditorObject* UISettings, TArray< FLandscapeFileResolution >& ImportResolutions );
	static TOptional< TArray< FLandscapeImportLayerInfo > > CreateImportLayersInfo( ULandscapeEditorObject* UISettings, int32 NewLandscapePreviewMode );
	static TArray< uint16 > ComputeHeightData( ULandscapeEditorObject* UISettings, TArray< FLandscapeImportLayerInfo >& ImportLayers, int32 NewLandscapePreviewMode );

	static const int32 SectionSizes[6];
	static const int32 NumSections[2];
};
