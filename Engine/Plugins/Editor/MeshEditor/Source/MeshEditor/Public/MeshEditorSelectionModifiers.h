// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshElement.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/Commands.h"

#include "MeshEditorSelectionModifiers.generated.h"

class IMeshEditorModeUIContract;
class UEditableMesh;
class UMeshEditorSelectionModifier;

namespace MeshEditorSelectionModifiers
{
	MESHEDITOR_API const TArray< UMeshEditorSelectionModifier* >& Get();
}

class MESHEDITOR_API FMeshEditorSelectionModifiers : public TCommands< FMeshEditorSelectionModifiers >
{
public:
	FMeshEditorSelectionModifiers();

	virtual void RegisterCommands() override;
};

UCLASS( abstract )
class MESHEDITOR_API UMeshEditorSelectionModifier : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UMeshEditorSelectionModifier() = default;

	/** Which mesh element does this command apply to? */
	virtual EEditableMeshElementType GetElementType() const PURE_VIRTUAL(, return EEditableMeshElementType::Invalid; );

	/** Registers the UI command for this mesh editor command */
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) PURE_VIRTUAL(,);

	virtual bool ModifySelection( TMap< UEditableMesh*, TArray< FMeshElement > >& InOutSelection ) PURE_VIRTUAL(, return false; );

	/** Gets the UI command info for this command */
	const TSharedPtr<FUICommandInfo>& GetUICommandInfo() const { return UICommandInfo; }

	/** Gets the name of this selection modifier.  This is not to display to a user, but instead used to uniquely identify this selection modifier */
	FName GetSelectionModifierName() const
	{
		return UICommandInfo->GetCommandName();
	}

protected:

	/** Our UI command for this action */
	TSharedPtr< FUICommandInfo > UICommandInfo;
};

/**
 *  Dummy selection modifier that doesn't actually modifies the selection.
 */
UCLASS()
class MESHEDITOR_API USelectSingleMeshElement : public UMeshEditorSelectionModifier
{
	GENERATED_BODY()

public:
	virtual EEditableMeshElementType GetElementType() const override { return EEditableMeshElementType::Any; }

	virtual bool ModifySelection( TMap< UEditableMesh*, TArray< FMeshElement > >& InOutSelection ) override { return true; }

	virtual void RegisterUICommand( FBindingContext* BindingContext ) override;
};

/**
 * Selects all the polygons that are part of the selection polygons group ids.
 */
UCLASS()
class MESHEDITOR_API USelectPolygonsByGroup : public UMeshEditorSelectionModifier
{
	GENERATED_BODY()
public:
	virtual EEditableMeshElementType GetElementType() const override { return EEditableMeshElementType::Polygon; }

	virtual bool ModifySelection( TMap< UEditableMesh*, TArray< FMeshElement > >& InOutSelection ) override;

	virtual void RegisterUICommand( FBindingContext* BindingContext ) override;
};

/**
 * Selects all the polygons that are connected to the selection polygons.
 */
UCLASS()
class MESHEDITOR_API USelectPolygonsByConnectivity : public UMeshEditorSelectionModifier
{
	GENERATED_BODY()
public:
	virtual EEditableMeshElementType GetElementType() const override { return EEditableMeshElementType::Polygon; }

	virtual bool ModifySelection( TMap< UEditableMesh*, TArray< FMeshElement > >& InOutSelection ) override;

	virtual void RegisterUICommand( FBindingContext* BindingContext ) override;
};

/**
 * Selects all the polygons that have the same smoothing group as the selection polygons.
 */
UCLASS()
class MESHEDITOR_API USelectPolygonsBySmoothingGroup : public UMeshEditorSelectionModifier
{
	GENERATED_BODY()
public:
	virtual EEditableMeshElementType GetElementType() const override { return EEditableMeshElementType::Polygon; }

	virtual bool ModifySelection(TMap< UEditableMesh*, TArray< FMeshElement > >& InOutSelection) override;

	virtual void RegisterUICommand(FBindingContext* BindingContext) override;
};

UCLASS()
class MESHEDITOR_API UMeshEditorSelectionModifiersList : public UObject
{
	GENERATED_BODY()

public:
	void HarvestSelectionModifiers();

	/** All of the selection modifiers that were registered at startup */
	UPROPERTY()
	TArray< UMeshEditorSelectionModifier* > SelectionModifiers;
};
