// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "Slate/SlateGameResources.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr< FSlateStyleSet > FNiagaraEditorStyle::NiagaraEditorStyleInstance = NULL;

void FNiagaraEditorStyle::Initialize()
{
	if (!NiagaraEditorStyleInstance.IsValid())
	{
		NiagaraEditorStyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*NiagaraEditorStyleInstance);
	}
}

void FNiagaraEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*NiagaraEditorStyleInstance);
	ensure(NiagaraEditorStyleInstance.IsUnique());
	NiagaraEditorStyleInstance.Reset();
}

FName FNiagaraEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("NiagaraEditorStyle"));
	return StyleSetName;
}

NIAGARAEDITOR_API FString RelativePathToPluginPath(const FString& RelativePath, const ANSICHAR* Extension)
{
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("Niagara"))->GetContentDir();
	return (ContentDir / RelativePath) + Extension;
}

#define IMAGE_PLUGIN_BRUSH( RelativePath, ... ) FSlateImageBrush( RelativePathToPluginPath( RelativePath, ".png" ), __VA_ARGS__ )
#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_CORE_BRUSH( RelativePath, ... ) FSlateBoxBrush( FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".png"), __VA_ARGS__ )
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);
const FVector2D Icon64x64(64.0f, 64.0f);

TSharedRef< FSlateStyleSet > FNiagaraEditorStyle::Create()
{
	const FTextBlockStyle NormalText = FEditorStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");
	const FEditableTextBoxStyle NormalEditableTextBox = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	const FSpinBoxStyle NormalSpinBox = FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("SpinBox");

	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("NiagaraEditorStyle"));
	Style->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate/Niagara"));

	// Stats
	FTextBlockStyle CategoryText = FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Regular", 10))
		.SetShadowOffset(FVector2D(0, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));

	Style->Set("NiagaraEditor.StatsText", CategoryText);

	// Asset picker
	FTextBlockStyle AssetPickerAssetNameText = FTextBlockStyle(NormalText)
		.SetColorAndOpacity(FLinearColor::White)
		.SetFont(DEFAULT_FONT("Bold", 14))
		.SetShadowOffset(FVector2D(0, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));

	Style->Set("NiagaraEditor.AssetPickerAssetNameText", AssetPickerAssetNameText);

	FTextBlockStyle AssetPickerAssetCategoryText = FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Regular", 10));

	Style->Set("NiagaraEditor.AssetPickerAssetCategoryText", AssetPickerAssetCategoryText);

	// New Asset Dialog
	FTextBlockStyle NewAssetDialogOptionText = FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Bold", 11));

	Style->Set("NiagaraEditor.NewAssetDialog.OptionText", NewAssetDialogOptionText);

	FTextBlockStyle NewAssetDialogHeaderText = FTextBlockStyle(NormalText)
		.SetColorAndOpacity(FLinearColor::White)
		.SetFont(DEFAULT_FONT("Bold", 10))
		.SetShadowOffset(FVector2D(0, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));

	Style->Set("NiagaraEditor.NewAssetDialog.HeaderText", NewAssetDialogHeaderText);

	FTextBlockStyle NewAssetDialogSubHeaderText = FTextBlockStyle(NormalText)
		.SetColorAndOpacity(FLinearColor::White)
		.SetFont(DEFAULT_FONT("Bold", 10))
		.SetShadowOffset(FVector2D(0, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));

	Style->Set("NiagaraEditor.NewAssetDialog.SubHeaderText", NewAssetDialogSubHeaderText);

	Style->Set("NiagaraEditor.NewAssetDialog.AddButton", FButtonStyle()
		.SetNormal(BOX_CORE_BRUSH("Common/FlatButton", 2.0f / 8.0f, FLinearColor(0, 0, 0, .25f)))
		.SetHovered(BOX_CORE_BRUSH("Common/FlatButton", 2.0f / 8.0f, FEditorStyle::GetSlateColor("SelectionColor")))
		.SetPressed(BOX_CORE_BRUSH("Common/FlatButton", 2.0f / 8.0f, FEditorStyle::GetSlateColor("SelectionColor_Pressed")))
	);

	Style->Set("NiagaraEditor.NewAssetDialog.SubBorderColor", FLinearColor(FColor(48, 48, 48)));
	Style->Set("NiagaraEditor.NewAssetDialog.ActiveOptionBorderColor", FLinearColor(FColor(96, 96, 96)));

	Style->Set("NiagaraEditor.NewAssetDialog.SubBorder", new BOX_CORE_BRUSH("Common/GroupBorderLight", FMargin(4.0f / 16.0f)));

	// Emitter Header
	FTextBlockStyle HeadingText = FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Regular", 14))
		.SetShadowOffset(FVector2D(0, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));

	FEditableTextBoxStyle HeadingEditableTextBox = FEditableTextBoxStyle(NormalEditableTextBox)
		.SetFont(DEFAULT_FONT("Regular", 14));

	Style->Set("NiagaraEditor.HeadingEditableTextBox", HeadingEditableTextBox);

	Style->Set("NiagaraEditor.HeadingInlineEditableText", FInlineEditableTextBlockStyle()
		.SetTextStyle(HeadingText)
		.SetEditableTextBoxStyle(HeadingEditableTextBox));

	FTextBlockStyle TabText = FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Regular", 12))
		.SetShadowOffset(FVector2D(0, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
	
	Style->Set("NiagaraEditor.AttributeSpreadsheetTabText", TabText);

	FTextBlockStyle SubduedHeadingText = FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Regular", 14))
		.SetShadowOffset(FVector2D(0, 1))
		.SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
	
	Style->Set("NiagaraEditor.SubduedHeadingTextBox", SubduedHeadingText);

	// Parameters
	FSlateFontInfo ParameterFont = DEFAULT_FONT("Regular", 8);

	Style->Set("NiagaraEditor.ParameterFont", ParameterFont);

	FTextBlockStyle ParameterText = FTextBlockStyle(NormalText)
		.SetFont(ParameterFont);

	Style->Set("NiagaraEditor.ParameterText", ParameterText);

	FEditableTextBoxStyle ParameterEditableTextBox = FEditableTextBoxStyle(NormalEditableTextBox)
		.SetFont(ParameterFont);

	Style->Set("NiagaraEditor.ParameterEditableTextBox", ParameterEditableTextBox);

	Style->Set("NiagaraEditor.ParameterInlineEditableText", FInlineEditableTextBlockStyle()
		.SetTextStyle(ParameterText)
		.SetEditableTextBoxStyle(ParameterEditableTextBox));

	FSpinBoxStyle ParameterSpinBox = FSpinBoxStyle(NormalSpinBox)
		.SetTextPadding(1);

	Style->Set("NiagaraEditor.ParameterSpinbox", ParameterSpinBox);

	// Code View
	{
		Style->Set("NiagaraEditor.CodeView.Checkbox.Text", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 12))
			.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.9f))
			.SetShadowOffset(FVector2D(1, 1))
			.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

		const int32 LogFontSize = 9;
		FSlateFontInfo LogFont = DEFAULT_FONT("Mono", LogFontSize);
		FTextBlockStyle NormalLogText = FTextBlockStyle(NormalText)
			.SetFont(LogFont)
			.SetColorAndOpacity(FLinearColor(FColor(0xffffffff)))
			.SetSelectedBackgroundColor(FLinearColor(FColor(0xff666666)));
		Style->Set("NiagaraEditor.CodeView.Hlsl.Normal", NormalLogText);
	}

	// Selected Emitter
	FSlateFontInfo SelectedEmitterUnsupportedSelectionFont = DEFAULT_FONT("Regular", 10);
	FTextBlockStyle SelectedEmitterUnsupportedSelectionText = FTextBlockStyle(NormalText)
		.SetFont(SelectedEmitterUnsupportedSelectionFont)
		.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		.SetShadowOffset(FVector2D(0, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
	Style->Set("NiagaraEditor.SelectedEmitter.UnsupportedSelectionText", SelectedEmitterUnsupportedSelectionText);

	// Toolbar Icons
	Style->Set("NiagaraEditor.Apply", new IMAGE_BRUSH("Icons/icon_Niagara_Apply_40x", Icon40x40));
	Style->Set("NiagaraEditor.Apply.Small", new IMAGE_BRUSH("Icons/icon_Niagara_Apply_40x", Icon20x20));
	Style->Set("NiagaraEditor.Compile", new IMAGE_BRUSH("Icons/icon_compile_40x", Icon40x40));
	Style->Set("NiagaraEditor.Compile.Small", new IMAGE_BRUSH("Icons/icon_compile_40x", Icon20x20));
	Style->Set("NiagaraEditor.AddEmitter", new IMAGE_BRUSH("Icons/icon_AddObject_40x", Icon40x40));
	Style->Set("NiagaraEditor.AddEmitter.Small", new IMAGE_BRUSH("Icons/icon_AddObject_40x", Icon20x20));
	Style->Set("NiagaraEditor.UnlockToChanges", new IMAGE_BRUSH("Icons/icon_levels_unlocked_40x", Icon40x40));
	Style->Set("NiagaraEditor.UnlockToChanges.Small", new IMAGE_BRUSH("Icons/icon_levels_unlocked_40x", Icon20x20));
	Style->Set("NiagaraEditor.LockToChanges", new IMAGE_BRUSH("Icons/icon_levels_LockedReadOnly_40x", Icon40x40));
	Style->Set("NiagaraEditor.LockToChanges.Small", new IMAGE_BRUSH("Icons/icon_levels_LockedReadOnly_40x", Icon20x20));
	Style->Set("NiagaraEditor.SimulationOptions", new IMAGE_PLUGIN_BRUSH("Icons/Commands/icon_simulationOptions_40x", Icon40x40));
	Style->Set("NiagaraEditor.SimulationOptions.Small", new IMAGE_PLUGIN_BRUSH("Icons/Commands/icon_simulationOptions_40x", Icon20x20));

	Style->Set("Niagara.CompileStatus.Unknown", new IMAGE_BRUSH("Icons/CompileStatus_Working", Icon40x40));
	Style->Set("Niagara.CompileStatus.Unknown.Small", new IMAGE_BRUSH("Icons/CompileStatus_Working", Icon20x20));
	Style->Set("Niagara.CompileStatus.Error",   new IMAGE_BRUSH("Icons/CompileStatus_Fail", Icon40x40));
	Style->Set("Niagara.CompileStatus.Error.Small", new IMAGE_BRUSH("Icons/CompileStatus_Fail", Icon20x20));
	Style->Set("Niagara.CompileStatus.Good",    new IMAGE_BRUSH("Icons/CompileStatus_Good", Icon40x40));
	Style->Set("Niagara.CompileStatus.Good.Small", new IMAGE_BRUSH("Icons/CompileStatus_Good", Icon20x20));
	Style->Set("Niagara.CompileStatus.Warning", new IMAGE_BRUSH("Icons/CompileStatus_Warning", Icon40x40));
	Style->Set("Niagara.CompileStatus.Warning.Small", new IMAGE_BRUSH("Icons/CompileStatus_Warning", Icon20x20));
	Style->Set("Niagara.Asset.ReimportAsset.Needed", new IMAGE_BRUSH("Icons/icon_Reimport_Needed_40x", Icon40x40));
	Style->Set("Niagara.Asset.ReimportAsset.Default", new IMAGE_BRUSH("Icons/icon_Reimport_40x", Icon40x40));

	// Icons
	Style->Set("NiagaraEditor.Isolate", new IMAGE_PLUGIN_BRUSH("Icons/Isolate", Icon16x16));
	
	// Emitter details customization
	Style->Set("NiagaraEditor.MaterialWarningBorder", new BOX_CORE_BRUSH("Common/GroupBorderLight", FMargin(4.0f / 16.0f)));

	// Asset colors
	Style->Set("NiagaraEditor.AssetColors.System", FLinearColor(1.0f, 0.0f, 0.0f));
	Style->Set("NiagaraEditor.AssetColors.Emitter", FLinearColor(1.0f, 0.3f, 0.0f));
	Style->Set("NiagaraEditor.AssetColors.Script", FLinearColor(1.0f, 1.0f, 0.0f));
	Style->Set("NiagaraEditor.AssetColors.ParameterCollection", FLinearColor(1.0f, 1.0f, 0.3f));
	Style->Set("NiagaraEditor.AssetColors.ParameterCollectionInstance", FLinearColor(1.0f, 1.0f, 0.7f));

	// Script factory thumbnails
	Style->Set("NiagaraEditor.Thumbnails.DynamicInputs", new IMAGE_BRUSH("Icons/NiagaraScriptDynamicInputs_64x", Icon64x64));
	Style->Set("NiagaraEditor.Thumbnails.Functions", new IMAGE_BRUSH("Icons/NiagaraScriptFunction_64x", Icon64x64));
	Style->Set("NiagaraEditor.Thumbnails.Modules", new IMAGE_BRUSH("Icons/NiagaraScriptModules_64x", Icon64x64));

	// Renderer class icons
	Style->Set("ClassIcon.NiagaraSpriteRendererProperties", new IMAGE_PLUGIN_BRUSH("Icons/Renderers/renderer_sprite", Icon16x16));
	Style->Set("ClassIcon.NiagaraMeshRendererProperties", new IMAGE_PLUGIN_BRUSH("Icons/Renderers/renderer_mesh", Icon16x16));
	Style->Set("ClassIcon.NiagaraRibbonRendererProperties", new IMAGE_PLUGIN_BRUSH("Icons/Renderers/renderer_ribbon", Icon16x16));
	Style->Set("ClassIcon.NiagaraLightRendererProperties", new IMAGE_PLUGIN_BRUSH("Icons/Renderers/renderer_light", Icon16x16));
	Style->Set("ClassIcon.NiagaraRendererProperties", new IMAGE_PLUGIN_BRUSH("Icons/Renderers/renderer_default", Icon16x16));

	// Niagara sequence
	Style->Set("NiagaraEditor.NiagaraSequence.DefaultTrackColor", FLinearColor(0, .25f, 0));

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef BOX_CORE_BRUSH
#undef DEFAULT_FONT

void FNiagaraEditorStyle::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FNiagaraEditorStyle::Get()
{
	return *NiagaraEditorStyleInstance;
}
