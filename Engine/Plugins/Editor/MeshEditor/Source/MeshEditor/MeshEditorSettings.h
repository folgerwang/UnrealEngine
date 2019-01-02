// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MeshEditorSettings.generated.h"

/**
 * Implements the Mesh Editor's settings.
 */
UCLASS(config=EditorPerProjectUserSettings)
class MESHEDITOR_API UMeshEditorSettings
	: public UObject
{
	GENERATED_BODY()

public:

	UMeshEditorSettings()
		: bSeparateSelectionSetPerMode( false )
		, bOnlySelectVisibleMeshes( true )
		, bOnlySelectVisibleElements( true )
		, bAllowGrabberSphere( false )
		, bAutoQuadrangulate( false )
	{}

public:

	/** If set, each element selection mode remembers its own selection set. Otherwise, changing selection mode adapts the current selection as appropriate. */
	UPROPERTY(EditAnywhere, config, Category=MeshEditor)
	bool bSeparateSelectionSetPerMode;

	/** Whether only unoccluded meshes will be selected by marquee select, or whether all meshes within the selection box will be selected, regardless of whether they are behind another. */
	UPROPERTY(EditAnywhere, config, Category=MeshEditor)
	bool bOnlySelectVisibleMeshes;

	/** Whether only front-facing vertices, edges or polygons will be selected by marquee select. */
	UPROPERTY(EditAnywhere, config, Category=MeshEditor)
	bool bOnlySelectVisibleElements;

	/** When enabled, the grabber sphere will be used to select and move mesh elements near the interactor's origin */
	UPROPERTY(EditAnywhere, config, Category=MeshEditor)
	bool bAllowGrabberSphere;

	/** When enabled, triangulated static meshes will be auto-quadrangulated when converted to editable meshes */
	UPROPERTY(EditAnywhere, config, Category=MeshEditor)
	bool bAutoQuadrangulate;

protected:

	// UObject overrides
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;

};
