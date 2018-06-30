// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UI/MediaBundleEditorStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

namespace MediaBundleStyle
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon64x64(64.0f, 64.0f);

	const FName NAME_StyleName(TEXT("MediaBundleStyle"));
	const FName NAME_ContextName(TEXT("MediaBundle"));

	static TUniquePtr<FSlateStyleSet> StyleInstance;
}

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(MediaBundleStyle::StyleInstance->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

void FMediaBundleEditorStyle::Register()
{
	MediaBundleStyle::StyleInstance = MakeUnique<FSlateStyleSet>(MediaBundleStyle::NAME_StyleName);
	MediaBundleStyle::StyleInstance->SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Media/MediaFrameworkUtilities/Content/Editor/Icons/"));

	MediaBundleStyle::StyleInstance->Set("ClassThumbnail.MediaBundle", new IMAGE_BRUSH("MediaBundle_64x", MediaBundleStyle::Icon64x64));
	MediaBundleStyle::StyleInstance->Set("ClassIcon.MediaBundle", new IMAGE_BRUSH("MediaBundle_20x", MediaBundleStyle::Icon20x20));

	MediaBundleStyle::StyleInstance->Set("CaptureCameraViewport_Capture", new IMAGE_BRUSH("CaptureCameraViewport_Capture_40x", MediaBundleStyle::Icon40x40));
	MediaBundleStyle::StyleInstance->Set("CaptureCameraViewport_Capture.Small", new IMAGE_BRUSH("CaptureCameraViewport_Capture_16x", MediaBundleStyle::Icon16x16));
	MediaBundleStyle::StyleInstance->Set("CaptureCameraViewport_Stop", new IMAGE_BRUSH("CaptureCameraViewport_Stop_40x", MediaBundleStyle::Icon40x40));

	//Use existing icon of MaterialInstanceConstant from Engine Editor content
	const FString EngineContentPath = FPaths::EngineContentDir() / TEXT("Editor/Slate/Icons/AssetIcons/MaterialInstanceConstant_64x.png");
	MediaBundleStyle::StyleInstance->Set("MaterialEditor", new FSlateImageBrush(EngineContentPath, MediaBundleStyle::Icon40x40));
	MediaBundleStyle::StyleInstance->Set("MaterialEditor.Small", new FSlateImageBrush(EngineContentPath, MediaBundleStyle::Icon20x20));


	FSlateStyleRegistry::RegisterSlateStyle(*MediaBundleStyle::StyleInstance.Get());
}

void FMediaBundleEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*MediaBundleStyle::StyleInstance.Get());
	MediaBundleStyle::StyleInstance.Reset();
}

FName FMediaBundleEditorStyle::GetStyleSetName()
{
	return MediaBundleStyle::NAME_StyleName;
}

const ISlateStyle& FMediaBundleEditorStyle::Get()
{
	check(MediaBundleStyle::StyleInstance.IsValid());
	return *MediaBundleStyle::StyleInstance.Get();
}

#undef IMAGE_BRUSH
