// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DatasmithContentEditorStyle.h"

//#include "DatasmithContentEditorModule.h"
#include "DatasmithContentModule.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

#define IMAGE_PLUGIN_BRUSH( RelativePath, ... ) FSlateImageBrush( FDatasmithContentEditorStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )

TSharedPtr<FSlateStyleSet> FDatasmithContentEditorStyle::StyleSet;

void FDatasmithContentEditorStyle::Initialize()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>(GetStyleSetName());

	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	const FVector2D Icon20x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	StyleSet->Set("DatasmithDataPrepEditor.Importer", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithImporterIcon40", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.Importer.Small", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithImporterIcon40", Icon20x20));
	StyleSet->Set("DatasmithDataPrepEditor.Importer.Selected", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithImporterIcon40", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.Importer.Selected.Small", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithImporterIcon40", Icon20x20));

	StyleSet->Set("DatasmithDataPrepEditor.CADImporter", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithCADImporterIcon40", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.CADImporter.Small", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithCADImporterIcon40", Icon20x20));
	StyleSet->Set("DatasmithDataPrepEditor.CADImporter.Selected", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithCADImporterIcon40", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.CADImporter.Selected.Small", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithCADImporterIcon40", Icon20x20));

	StyleSet->Set("DatasmithDataPrepEditor.VREDImporter", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithVREDImporter40", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.VREDImporter.Small", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithVREDImporter40", Icon20x20));
	StyleSet->Set("DatasmithDataPrepEditor.VREDImporter.Selected", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithVREDImporter40", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.VREDImporter.Selected.Small", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithVREDImporter40", Icon20x20));

	StyleSet->Set("DatasmithDataPrepEditor.DeltaGenImporter", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithDeltaGenImporter40", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.DeltaGenImporter.Small", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithDeltaGenImporter40", Icon20x20));
	StyleSet->Set("DatasmithDataPrepEditor.DeltaGenImporter.Selected", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithDeltaGenImporter40", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.DeltaGenImporter.Selected.Small", new IMAGE_PLUGIN_BRUSH("Icons/DatasmithDeltaGenImporter40", Icon20x20));

	StyleSet->Set("DatasmithDataPrepEditor.SaveScene", new IMAGE_PLUGIN_BRUSH("Icons/SaveScene", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.SaveScene.Small", new IMAGE_PLUGIN_BRUSH("Icons/SaveScene", Icon20x20));
	StyleSet->Set("DatasmithDataPrepEditor.SaveScene.Selected", new IMAGE_PLUGIN_BRUSH("Icons/SaveScene", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.SaveScene.Selected.Small", new IMAGE_PLUGIN_BRUSH("Icons/SaveScene", Icon20x20));

	StyleSet->Set("DatasmithDataPrepEditor.ShowDatasmithSceneSettings", new IMAGE_PLUGIN_BRUSH("Icons/IconOptions", Icon40x40));

	StyleSet->Set("DatasmithDataPrepEditor.BuildWorld", new IMAGE_PLUGIN_BRUSH("Icons/BuildWorld", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.BuildWorld.Small", new IMAGE_PLUGIN_BRUSH("Icons/BuildWorld", Icon20x20));
	StyleSet->Set("DatasmithDataPrepEditor.BuildWorld.Selected", new IMAGE_PLUGIN_BRUSH("Icons/BuildWorld", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.BuildWorld.Selected.Small", new IMAGE_PLUGIN_BRUSH("Icons/BuildWorld", Icon20x20));

	StyleSet->Set("DatasmithDataPrepEditor.ExecutePipeline", new IMAGE_PLUGIN_BRUSH("Icons/ExecutePipeline", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.ExecutePipeline.Small", new IMAGE_PLUGIN_BRUSH("Icons/ExecutePipeline", Icon20x20));
	StyleSet->Set("DatasmithDataPrepEditor.ExecutePipeline.Selected", new IMAGE_PLUGIN_BRUSH("Icons/ExecutePipeline", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.ExecutePipeline.Selected.Small", new IMAGE_PLUGIN_BRUSH("Icons/ExecutePipeline", Icon20x20));

	StyleSet->Set("DatasmithDataPrepEditor.Jacketing", new IMAGE_PLUGIN_BRUSH("Icons/Jacketing", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.Jacketing.Small", new IMAGE_PLUGIN_BRUSH("Icons/Jacketing", Icon20x20));
	StyleSet->Set("DatasmithDataPrepEditor.Jacketing.Selected", new IMAGE_PLUGIN_BRUSH("Icons/Jacketing", Icon40x40));
	StyleSet->Set("DatasmithDataPrepEditor.Jacketing.Selected.Small", new IMAGE_PLUGIN_BRUSH("Icons/Jacketing", Icon20x20));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

void FDatasmithContentEditorStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

FName FDatasmithContentEditorStyle::GetStyleSetName()
{
	static FName StyleName("DatasmithContentEditorStyle");
	return StyleName;
}

FString FDatasmithContentEditorStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
	static FString BaseDir = IPluginManager::Get().FindPlugin(DATASMITHCONTENT_MODULE_NAME)->GetBaseDir() + TEXT("/Resources");
	return (BaseDir / RelativePath) + Extension;
}
