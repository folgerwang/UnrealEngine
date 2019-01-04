// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

class FFieldSystemEditorStyle : public FSlateStyleSet
{
public:
	FFieldSystemEditorStyle() : FSlateStyleSet("FieldSystemEditorStyle")
	{
		const FVector2D Icon16x16(16.f, 16.f);
		const FVector2D Icon64x64(64.f, 64.f);

		FString PluginBasePath = FPaths::GetPath(FModuleManager::Get().GetModuleFilename("FieldSystemEditor"));
		SetContentRoot(PluginBasePath / TEXT("../../Resources"));

		Set("ClassIcon.FieldSystem", new FSlateImageBrush(RootToContentDir(TEXT("FieldSystem_16x.png")), Icon16x16));
		Set("ClassThumbnail.FieldSystem", new FSlateImageBrush(RootToContentDir(TEXT("FieldSystem_64x.png")), Icon64x64));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	~FFieldSystemEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}

public:

	static FFieldSystemEditorStyle& Get()
	{
		if (!Singleton.IsSet())
		{
			Singleton.Emplace();
		}
		return Singleton.GetValue();
	}

	static void Destroy()
	{
		Singleton.Reset();
	}

private:
	static TOptional<FFieldSystemEditorStyle> Singleton;
};

TOptional<FFieldSystemEditorStyle> FFieldSystemEditorStyle::Singleton;