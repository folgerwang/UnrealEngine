// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorWidgetsStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "Slate/SlateGameResources.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr< FSlateStyleSet > FNiagaraEditorWidgetsStyle::NiagaraEditorWidgetsStyleInstance = NULL;
 
void FNiagaraEditorWidgetsStyle::Initialize()
{
	if (!NiagaraEditorWidgetsStyleInstance.IsValid())
	{
		NiagaraEditorWidgetsStyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*NiagaraEditorWidgetsStyleInstance);
	}
}

void FNiagaraEditorWidgetsStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*NiagaraEditorWidgetsStyleInstance);
	ensure(NiagaraEditorWidgetsStyleInstance.IsUnique());
	NiagaraEditorWidgetsStyleInstance.Reset();
}

FName FNiagaraEditorWidgetsStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("NiagaraEditorWidgetsStyle"));
	return StyleSetName;
}

NIAGARAEDITOR_API FString RelativePathToPluginPath(const FString& RelativePath, const ANSICHAR* Extension);

#define IMAGE_PLUGIN_BRUSH( RelativePath, ... ) FSlateImageBrush( RelativePathToPluginPath( RelativePath, ".png" ), __VA_ARGS__ )
#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_PLUGIN_BRUSH( RelativePath, ... ) FSlateBoxBrush( RelativePathToPluginPath( RelativePath, ".png"), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)
#define IMAGE_CORE_BRUSH( RelativePath, ... ) FSlateImageBrush( FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".png") , __VA_ARGS__ )
#define BOX_CORE_BRUSH( RelativePath, ... ) FSlateBoxBrush( FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".png"), __VA_ARGS__ )

const FVector2D Icon8x8(8.0f, 8.0f);
const FVector2D Icon8x16(8.0f, 16.0f);
const FVector2D Icon12x12(12.0f, 12.0f);
const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon30x30(30.0f, 30.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

TSharedRef< FSlateStyleSet > FNiagaraEditorWidgetsStyle::Create()
{
	const FTextBlockStyle NormalText = FEditorStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");
	const FEditableTextBoxStyle NormalEditableTextBox = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	const FSpinBoxStyle NormalSpinBox = FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("SpinBox");

	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("NiagaraEditorWidgetsStyle"));
	Style->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate/Niagara"));

	// Stack
	FSlateFontInfo StackGroupFont = DEFAULT_FONT("Bold", 10);
	FTextBlockStyle StackGroupText = FTextBlockStyle(NormalText)
		.SetFont(StackGroupFont)
		.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		.SetShadowOffset(FVector2D(0, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
	Style->Set("NiagaraEditor.Stack.GroupText", StackGroupText);

	FSlateFontInfo StackDefaultFont = DEFAULT_FONT("Regular", 10);
	FTextBlockStyle StackDefaultText = FTextBlockStyle(NormalText)
		.SetFont(StackDefaultFont);
	Style->Set("NiagaraEditor.Stack.DefaultText", StackDefaultText);

	FSlateFontInfo StackCategoryFont = DEFAULT_FONT("Bold", 10);
	FTextBlockStyle StackCategoryText = FTextBlockStyle(NormalText)
		.SetFont(StackCategoryFont)
		.SetShadowOffset(FVector2D(1, 1));
	Style->Set("NiagaraEditor.Stack.CategoryText", StackCategoryText);

	FSlateFontInfo ParameterFont = DEFAULT_FONT("Regular", 8);
	FTextBlockStyle ParameterText = FTextBlockStyle(NormalText)
		.SetFont(ParameterFont);
	Style->Set("NiagaraEditor.Stack.ParameterText", ParameterText);

	FSlateFontInfo ParameterCollectionFont = DEFAULT_FONT("Regular", 9);
	FTextBlockStyle ParameterCollectionText = FTextBlockStyle(NormalText)
		.SetFont(ParameterCollectionFont);
	Style->Set("NiagaraEditor.Stack.ParameterCollectionText", ParameterCollectionText);

	FSlateFontInfo StackItemFont = DEFAULT_FONT("Regular", 11);
	FTextBlockStyle StackItemText = FTextBlockStyle(NormalText)
		.SetFont(StackItemFont);
	Style->Set("NiagaraEditor.Stack.ItemText", StackItemText);

	Style->Set("NiagaraEditor.Stack.Group.BackgroundColor", FLinearColor(FColor(96, 96, 96)));
	Style->Set("NiagaraEditor.Stack.Item.HeaderBackgroundColor", FLinearColor(FColor(48, 48, 48)));
	Style->Set("NiagaraEditor.Stack.Item.ContentBackgroundColor", FLinearColor(FColor(62, 62, 62)));
	Style->Set("NiagaraEditor.Stack.Item.ContentAdvancedBackgroundColor", FLinearColor(FColor(53, 53, 53)));
	Style->Set("NiagaraEditor.Stack.Item.FooterBackgroundColor", FLinearColor(FColor(71, 71, 71)));
	Style->Set("NiagaraEditor.Stack.Item.IssueBackgroundColor", FLinearColor(FColor(120, 120, 62)));
	Style->Set("NiagaraEditor.Stack.UnknownColor", FLinearColor(1, 0, 1));

	Style->Set("NiagaraEditor.Stack.ItemHeaderFooter.BackgroundBrush", new FSlateColorBrush(FLinearColor(FColor(20, 20, 20))));

	Style->Set("NiagaraEditor.Stack.ForegroundColor", FLinearColor(FColor(220, 220, 220)));
	Style->Set("NiagaraEditor.Stack.GroupForegroundColor", FLinearColor(FColor(220, 220, 220)));
	Style->Set("NiagaraEditor.Stack.FlatButtonColor", FLinearColor(FColor(191, 191, 191)));

	Style->Set("NiagaraEditor.Stack.AccentColor.System", FLinearColor(FColor(67, 105, 124)));
	Style->Set("NiagaraEditor.Stack.AccentColor.Emitter", FLinearColor(FColor(126, 87, 67)));
	Style->Set("NiagaraEditor.Stack.AccentColor.Particle", FLinearColor(FColor(87, 107, 61)));
	Style->Set("NiagaraEditor.Stack.AccentColor.Render", FLinearColor(FColor(134, 80, 80)));
	Style->Set("NiagaraEditor.Stack.AccentColor.None", FLinearColor::Transparent);

	Style->Set("NiagaraEditor.Stack.IconColor.System", FLinearColor(FColor(1, 202, 252)));
	Style->Set("NiagaraEditor.Stack.IconColor.Emitter", FLinearColor(FColor(241, 99, 6)));
	Style->Set("NiagaraEditor.Stack.IconColor.Particle", FLinearColor(FColor(131, 228, 9)));
	Style->Set("NiagaraEditor.Stack.IconColor.Render", FLinearColor(FColor(230, 102, 102)));

 	Style->Set("NiagaraEditor.Stack.DropTarget.BackgroundColor", FLinearColor(1.0f, 1.0f, 1.0f, 0.25f));
 	Style->Set("NiagaraEditor.Stack.DropTarget.BackgroundColorHover", FLinearColor(1.0f, 1.0f, 1.0f, 0.1f));
	Style->Set("NiagaraEditor.Stack.DropTarget.BorderVertical", new IMAGE_PLUGIN_BRUSH("Icons/StackDropTargetBorder_Vertical", FVector2D(2, 8), FLinearColor::White, ESlateBrushTileType::Vertical));
	Style->Set("NiagaraEditor.Stack.DropTarget.BorderHorizontal", new IMAGE_PLUGIN_BRUSH("Icons/StackDropTargetBorder_Horizontal", FVector2D(8, 2), FLinearColor::White, ESlateBrushTileType::Horizontal));

	Style->Set("NiagaraEditor.Stack.GoToSourceIcon", new IMAGE_CORE_BRUSH("Common/GoToSource", Icon30x30, FLinearColor::White));
	Style->Set("NiagaraEditor.Stack.ParametersIcon", new IMAGE_PLUGIN_BRUSH("Icons/SystemParams", Icon12x12, FLinearColor::White));
	Style->Set("NiagaraEditor.Stack.SpawnIcon", new IMAGE_PLUGIN_BRUSH("Icons/Spawn", Icon12x12, FLinearColor::White));
	Style->Set("NiagaraEditor.Stack.UpdateIcon", new IMAGE_PLUGIN_BRUSH("Icons/Update", Icon12x12, FLinearColor::White));
	Style->Set("NiagaraEditor.Stack.EventIcon", new IMAGE_PLUGIN_BRUSH("Icons/Event", Icon12x12, FLinearColor::White));

	Style->Set("NiagaraEditor.Stack.ParametersIconHighlighted", new IMAGE_PLUGIN_BRUSH("Icons/SystemParams", Icon16x16, FLinearColor::White));
	Style->Set("NiagaraEditor.Stack.SpawnIconHighlighted", new IMAGE_PLUGIN_BRUSH("Icons/Spawn", Icon16x16, FLinearColor::White));
	Style->Set("NiagaraEditor.Stack.UpdateIconHighlighted", new IMAGE_PLUGIN_BRUSH("Icons/Update", Icon16x16, FLinearColor::White));
	Style->Set("NiagaraEditor.Stack.EventIconHighlighted", new IMAGE_PLUGIN_BRUSH("Icons/Event", Icon16x16, FLinearColor::White));

	Style->Set("NiagaraEditor.Stack.IconHighlightedSize", 16.0f);

	Style->Set("NiagaraEditor.Stack.Splitter", FSplitterStyle()
		.SetHandleNormalBrush(IMAGE_CORE_BRUSH("Common/SplitterHandleHighlight", Icon8x8, FLinearColor(.1f, .1f, .1f, 1.0f)))
		.SetHandleHighlightBrush(IMAGE_CORE_BRUSH("Common/SplitterHandleHighlight", Icon8x8, FLinearColor::White))
	);

	Style->Set("NiagaraEditor.Stack.SearchHighlightColor", FLinearColor(FColor::Orange));
	Style->Set("NiagaraEditor.Stack.SearchResult", new BOX_PLUGIN_BRUSH("Icons/SearchResultBorder", FMargin(1.f/8.f)));

	Style->Set("NiagaraEditor.Stack.AddButton", FButtonStyle()
		.SetNormal(BOX_CORE_BRUSH("Common/FlatButton", 2.0f / 8.0f, FLinearColor(0, 0, 0, .25f)))
		.SetHovered(BOX_CORE_BRUSH("Common/FlatButton", 2.0f / 8.0f, FEditorStyle::GetSlateColor("SelectionColor")))
		.SetPressed(BOX_CORE_BRUSH("Common/FlatButton", 2.0f / 8.0f, FEditorStyle::GetSlateColor("SelectionColor_Pressed")))
	);

	Style->Set("NiagaraEditor.ShowInCurveEditorIcon", new IMAGE_PLUGIN_BRUSH("Icons/ShowInCurveEditor", Icon16x16, FLinearColor::White));

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef DEFAULT_FONT
#undef BOX_CORE_BRUSH
#undef IMAGE_CORE_BRUSH

void FNiagaraEditorWidgetsStyle::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FNiagaraEditorWidgetsStyle::Get()
{
	return *NiagaraEditorWidgetsStyleInstance;
}
