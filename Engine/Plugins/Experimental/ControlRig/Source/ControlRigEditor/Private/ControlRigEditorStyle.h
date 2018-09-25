// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define OTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".otf")), __VA_ARGS__)

class FControlRigEditorStyle
	: public FSlateStyleSet
{
public:
	FControlRigEditorStyle()
		: FSlateStyleSet("ControlRigEditorStyle")
	{
		const FVector2D Icon10x10(10.0f, 10.0f);
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon20x20(20.0f, 20.0f);
		const FVector2D Icon24x24(24.0f, 24.0f);
		const FVector2D Icon32x32(32.0f, 32.0f);
		const FVector2D Icon40x40(40.0f, 40.0f);
		SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Experimental/ControlRig/Content"));

		const FSlateColor DefaultForeground(FLinearColor(0.72f, 0.72f, 0.72f, 1.f));

		// Class Icons
		{
			Set("ClassIcon.ControlRigSequence", new IMAGE_BRUSH("ControlRigSequence_16x", Icon16x16));
			Set("ClassIcon.ControlRigBlueprint", new IMAGE_BRUSH("ControlRigBlueprint_16x", Icon16x16));
		}

		// Edit mode styles
		{
			Set("ControlRigEditMode", new IMAGE_BRUSH("ControlRigMode_40x", Icon40x40));
			Set("ControlRigEditMode.Small", new IMAGE_BRUSH("ControlRigMode_40x", Icon20x20));
		}

		// Sequencer styles
		{
			Set("ControlRig.ExportAnimSequence", new IMAGE_BRUSH("ExportAnimSequence_24x", Icon24x24));
			Set("ControlRig.ExportAnimSequence.Small", new IMAGE_BRUSH("ExportAnimSequence_24x", Icon24x24));
			Set("ControlRig.ReExportAnimSequence", new IMAGE_BRUSH("ExportAnimSequence_24x", Icon24x24));
			Set("ControlRig.ReExportAnimSequence.Small", new IMAGE_BRUSH("ExportAnimSequence_24x", Icon24x24));
			Set("ControlRig.ImportFromRigSequence", new IMAGE_BRUSH("ReImportRigSequence_16x", Icon16x16));
			Set("ControlRig.ImportFromRigSequence.Small", new IMAGE_BRUSH("ReImportRigSequence_16x", Icon16x16));
			Set("ControlRig.ReImportFromRigSequence", new IMAGE_BRUSH("ReImportRigSequence_16x", Icon16x16));
			Set("ControlRig.ReImportFromRigSequence.Small", new IMAGE_BRUSH("ReImportRigSequence_16x", Icon16x16));
		}

		// Control Rig Editor styles
		{
			Set("ControlRig.TabIcon", new IMAGE_BRUSH("ControlRigTab_16x", Icon16x16));
			Set("ControlRig.RigUnit", new IMAGE_BRUSH("ControlRigUnit_16x", Icon16x16));

			// icons for control units
			Set("ControlRig.ControlUnitOn", new IMAGE_BRUSH(TEXT("ControlUnit_On"), Icon32x32));
			Set("ControlRig.ControlUnitOff", new IMAGE_BRUSH(TEXT("ControlUnit_Off"), Icon32x32));

			Set("ControlRig.ExecuteGraph", new IMAGE_BRUSH("ExecuteGraph", Icon40x40));
			Set("ControlRig.ExecuteGraph.Small", new IMAGE_BRUSH("ExecuteGraph", Icon20x20));
		}

		// Graph styles
		{
			Set("ControlRig.Node.PinTree.Arrow_Collapsed_Left", new IMAGE_BRUSH("TreeArrow_Collapsed_Left", Icon10x10, DefaultForeground));
			Set("ControlRig.Node.PinTree.Arrow_Collapsed_Hovered_Left", new IMAGE_BRUSH("TreeArrow_Collapsed_Hovered_Left", Icon10x10, DefaultForeground));

			Set("ControlRig.Node.PinTree.Arrow_Expanded_Left", new IMAGE_BRUSH("TreeArrow_Expanded_Left", Icon10x10, DefaultForeground));
			Set("ControlRig.Node.PinTree.Arrow_Expanded_Hovered_Left", new IMAGE_BRUSH("TreeArrow_Expanded_Hovered_Left", Icon10x10, DefaultForeground));

			Set("ControlRig.Node.PinTree.Arrow_Collapsed_Right", new IMAGE_BRUSH("TreeArrow_Collapsed_Right", Icon10x10, DefaultForeground));
			Set("ControlRig.Node.PinTree.Arrow_Collapsed_Hovered_Right", new IMAGE_BRUSH("TreeArrow_Collapsed_Hovered_Right", Icon10x10, DefaultForeground));

			Set("ControlRig.Node.PinTree.Arrow_Expanded_Right", new IMAGE_BRUSH("TreeArrow_Expanded_Right", Icon10x10, DefaultForeground));
			Set("ControlRig.Node.PinTree.Arrow_Expanded_Hovered_Right", new IMAGE_BRUSH("TreeArrow_Expanded_Hovered_Right", Icon10x10, DefaultForeground));
		}

		// Font?
		{
			Set("ControlRig.Hierarchy.Menu", TTF_FONT("Fonts/Roboto-Regular", 12));
		}

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	static FControlRigEditorStyle& Get()
	{
		static FControlRigEditorStyle Inst;
		return Inst;
	}
	
	~FControlRigEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}
};

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT
