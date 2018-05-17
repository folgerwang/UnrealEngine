// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CopyVertexColorToClothParams.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UClothingAsset;
struct FClothParameterMask_PhysMesh;

/** Widget used for copying vertex colors from sim mesh to a selected mask. */
class SCopyVertexColorSettingsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCopyVertexColorSettingsPanel)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UClothingAsset* InAsset, int32 InLOD, FClothParameterMask_PhysMesh* InMask);

private:

	// Params struct, displayed using details panel
	FCopyVertexColorToClothParams CopyParams;

	// Handle 'Copy' button being clicked
	FReply OnCopyClicked();

	// Pointer to currently selected ClothingAsset
	TWeakObjectPtr<UClothingAsset> SelectedAssetPtr;
	// Pointer to selected mask
	FClothParameterMask_PhysMesh* SelectedMask;
	// Currently selected LOD
	int32 SelectedLOD;
};