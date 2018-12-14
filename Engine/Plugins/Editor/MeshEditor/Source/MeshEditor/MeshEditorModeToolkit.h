// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Toolkits/BaseToolkit.h"
#include "EditorModeManager.h"


/** Mesh Editor Mode widget for controls */
class SMeshEditorModeControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SMeshEditorModeControls ) {}
	SLATE_END_ARGS()

public:

	/** SCompoundWidget functions */
	void Construct( const FArguments& InArgs, class IMeshEditorModeUIContract& MeshEditorMode );

protected:

};


/**
 * Mode Toolkit for the Mesh Editor MOde
 */
class FMeshEditorModeToolkit : public FModeToolkit
{
public:

	FMeshEditorModeToolkit( class IMeshEditorModeUIContract& InMeshEditorMode )
		: MeshEditorMode( InMeshEditorMode )
	{}

	// FModeToolkit overrides
	virtual void Init( const TSharedPtr<IToolkitHost>& InitToolkitHost ) override;

	// FBaseToolkit overrides
	virtual void RegisterTabSpawners( const TSharedRef<FTabManager>& TabManager ) override;
	virtual void UnregisterTabSpawners( const TSharedRef<FTabManager>& TabManager ) override;

	// IToolkit overrides
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual class FEdMode* GetEditorMode() const override;
	virtual TSharedPtr<SWidget> GetInlineContent() const override;

private:
	/** Geometry tools widget */
	TSharedPtr<SMeshEditorModeControls> ToolkitWidget;

	/** Reference to the owning FMeshEditorMode */
	IMeshEditorModeUIContract& MeshEditorMode;
};
