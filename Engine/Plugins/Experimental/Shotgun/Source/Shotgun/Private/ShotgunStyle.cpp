// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ShotgunStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

TUniquePtr< FSlateStyleSet > FShotgunStyle::ShotgunStyleInstance = nullptr;

void FShotgunStyle::Initialize()
{
	if (!ShotgunStyleInstance.IsValid())
	{
		ShotgunStyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*ShotgunStyleInstance);
	}
}

void FShotgunStyle::Shutdown()
{
	if (ShotgunStyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*ShotgunStyleInstance);
		ShotgunStyleInstance.Reset();
	}
}

FName FShotgunStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("ShotgunStyle"));
	return StyleSetName;
}

FName FShotgunStyle::GetContextName()
{
	static FName ContextName(TEXT("Shotgun"));
	return ContextName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )

const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

TUniquePtr< FSlateStyleSet > FShotgunStyle::Create()
{
	TUniquePtr< FSlateStyleSet > Style = MakeUnique<FSlateStyleSet>(GetStyleSetName());
	Style->SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Experimental/Shotgun/Resources"));

	return Style;
}

void FShotgunStyle::SetIcon(const FString& StyleName, const FString& ResourcePath)
{
	FSlateStyleSet* Style = ShotgunStyleInstance.Get();

	FString Name(GetContextName().ToString());
	Name = Name + "." + StyleName;
	Style->Set(*Name, new IMAGE_BRUSH(ResourcePath, Icon40x40));

	Name += ".Small";
	Style->Set(*Name, new IMAGE_BRUSH(ResourcePath, Icon20x20));

	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

#undef IMAGE_BRUSH

const ISlateStyle& FShotgunStyle::Get()
{
	check(ShotgunStyleInstance);
	return *ShotgunStyleInstance;
}
