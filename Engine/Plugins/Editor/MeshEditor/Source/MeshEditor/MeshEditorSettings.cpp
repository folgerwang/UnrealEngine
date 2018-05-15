// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MeshEditorSettings.h"
#include "UnrealEdMisc.h"


void UMeshEditorSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty( PropertyChangedEvent );

	if ( !FUnrealEdMisc::Get().IsDeletePreferences() )
	{
		SaveConfig();
	}
}
