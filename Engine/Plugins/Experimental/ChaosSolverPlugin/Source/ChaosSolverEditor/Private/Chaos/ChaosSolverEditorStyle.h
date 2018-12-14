// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

class FChaosSolverEditorStyle : public FSlateStyleSet
{
public:
	FChaosSolverEditorStyle() : FSlateStyleSet("ChaosSolverEditorStyle")
	{
		const FVector2D Icon16x16(16.f, 16.f);
		const FVector2D Icon64x64(64.f, 64.f);

		FString PluginBasePath = FPaths::GetPath(FModuleManager::Get().GetModuleFilename("ChaosSolverEditor"));
		SetContentRoot(PluginBasePath / TEXT("../../Resources"));

		Set("ClassIcon.ChaosSolver", new FSlateImageBrush(RootToContentDir(TEXT("ChaosSolver_16x.png")), Icon16x16));
		Set("ClassThumbnail.ChaosSolver", new FSlateImageBrush(RootToContentDir(TEXT("ChaosSolver_64x.png")), Icon64x64));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	~FChaosSolverEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}

public:

	static FChaosSolverEditorStyle& Get()
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
	static TOptional<FChaosSolverEditorStyle> Singleton;
};

TOptional<FChaosSolverEditorStyle> FChaosSolverEditorStyle::Singleton;