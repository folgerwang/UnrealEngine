// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VPUtilitiesEditorStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

namespace VPUtilitiesEditorStyle
{
	const FName NAME_StyleName(TEXT("VPUtilitiesStyle"));

	static TUniquePtr<FSlateStyleSet> StyleInstance;
}

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(VPUtilitiesEditorStyle::StyleInstance->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

void FVPUtilitiesEditorStyle::Register()
{
	const FVector2D Icon16x16(16.0f, 16.0f);

	VPUtilitiesEditorStyle::StyleInstance = MakeUnique<FSlateStyleSet>(VPUtilitiesEditorStyle::NAME_StyleName);
	VPUtilitiesEditorStyle::StyleInstance->SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Experimental/VirtualProductionUtilities/Content/Editor/Icons/"));

	VPUtilitiesEditorStyle::StyleInstance->Set("TabIcons.Genlock.Small", new IMAGE_BRUSH("Icon_GenlockTab_16x", Icon16x16));


	FSlateStyleRegistry::RegisterSlateStyle(*VPUtilitiesEditorStyle::StyleInstance.Get());
}

void FVPUtilitiesEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*VPUtilitiesEditorStyle::StyleInstance.Get());
	VPUtilitiesEditorStyle::StyleInstance.Reset();
}

FName FVPUtilitiesEditorStyle::GetStyleSetName()
{
	return VPUtilitiesEditorStyle::NAME_StyleName;
}

const ISlateStyle& FVPUtilitiesEditorStyle::Get()
{
	check(VPUtilitiesEditorStyle::StyleInstance.IsValid());
	return *VPUtilitiesEditorStyle::StyleInstance.Get();
}

#undef IMAGE_BRUSH
