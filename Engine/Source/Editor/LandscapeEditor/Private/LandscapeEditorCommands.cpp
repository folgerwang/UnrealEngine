// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "LandscapeEditorCommands.h"

#define LOCTEXT_NAMESPACE "LandscapeEditorCommands"

FName FLandscapeEditorCommands::LandscapeContext = TEXT("LandscapeEditor");

FLandscapeEditorCommands::FLandscapeEditorCommands()
	: TCommands<FLandscapeEditorCommands>
(
	FLandscapeEditorCommands::LandscapeContext, // Context name for fast lookup
	NSLOCTEXT("Contexts", "LandscapeEditor", "Landscape Editor"), // Localized context name for displaying
	NAME_None, //"LevelEditor" // Parent
	FEditorStyle::GetStyleSetName() // Icon Style Set
	)
{
}

void FLandscapeEditorCommands::RegisterCommands()
{
	UI_COMMAND(ManageMode, "Mode - Manage", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("ToolMode_Manage", ManageMode);
	UI_COMMAND(SculptMode, "Mode - Sculpt", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("ToolMode_Sculpt", SculptMode);
	UI_COMMAND(PaintMode, "Mode - Paint", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("ToolMode_Paint", PaintMode);

	UI_COMMAND(NewLandscape, "Tool - New Landscape", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_NewLandscape", NewLandscape);

	UI_COMMAND(ResizeLandscape, "Tool - Change Component Size", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_ResizeLandscape", ResizeLandscape);

	UI_COMMAND(SculptTool, "Tool - Sculpt", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Sculpt", SculptTool);
	UI_COMMAND(PaintTool, "Tool - Paint", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Paint", PaintTool);
	UI_COMMAND(SmoothTool, "Tool - Smooth", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Smooth", SmoothTool);
	UI_COMMAND(FlattenTool, "Tool - Flatten", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Flatten", FlattenTool);
	UI_COMMAND(RampTool, "Tool - Ramp", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Ramp", RampTool);
	UI_COMMAND(ErosionTool, "Tool - Erosion", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Erosion", ErosionTool);
	UI_COMMAND(HydroErosionTool, "Tool - Hydraulic Erosion", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_HydraErosion", HydroErosionTool);
	UI_COMMAND(NoiseTool, "Tool - Noise", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Noise", NoiseTool);
	UI_COMMAND(RetopologizeTool, "Tool - Retopologize", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Retopologize", RetopologizeTool);
	UI_COMMAND(VisibilityTool, "Tool - Visibility", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Visibility", VisibilityTool);

	UI_COMMAND(SelectComponentTool, "Tool - Component Selection", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Select", SelectComponentTool);
	UI_COMMAND(AddComponentTool, "Tool - Add Components", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_AddComponent", AddComponentTool);
	UI_COMMAND(DeleteComponentTool, "Tool - Delete Components", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_DeleteComponent", DeleteComponentTool);
	UI_COMMAND(MoveToLevelTool, "Tool - Move to Level", "Moves the selected landscape components to the current streaming level", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_MoveToLevel", MoveToLevelTool);

	UI_COMMAND(RegionSelectTool, "Tool - Region Selection", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Mask", RegionSelectTool);
	UI_COMMAND(RegionCopyPasteTool, "Tool - Copy/Paste", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_CopyPaste", RegionCopyPasteTool);
	UI_COMMAND(MirrorTool, "Tool - Mirror", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Mirror", MirrorTool);

	UI_COMMAND(SplineTool, "Tool - Edit Splines", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Tool_Splines", SplineTool);

	UI_COMMAND(CircleBrush, "Brush - Circle", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("BrushSet_Circle", CircleBrush);
	UI_COMMAND(AlphaBrush, "Brush - Alpha", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("BrushSet_Alpha", AlphaBrush);
	UI_COMMAND(AlphaBrush_Pattern, "Brush - Pattern", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("BrushSet_Pattern", AlphaBrush_Pattern);
	UI_COMMAND(ComponentBrush, "Brush - Component", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("BrushSet_Component", ComponentBrush);
	UI_COMMAND(GizmoBrush, "Brush - Gizmo", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("BrushSet_Gizmo", GizmoBrush);
	NameToCommandMap.Add("BrushSet_Splines", SplineTool);

	UI_COMMAND(CircleBrush_Smooth, "Circle Brush - Smooth Falloff", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Circle_Smooth", CircleBrush_Smooth);
	UI_COMMAND(CircleBrush_Linear, "Circle Brush - Linear Falloff", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Circle_Linear", CircleBrush_Linear);
	UI_COMMAND(CircleBrush_Spherical, "Circle Brush - Spherical Falloff", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Circle_Spherical", CircleBrush_Spherical);
	UI_COMMAND(CircleBrush_Tip, "Circle Brush - Tip Falloff", "", EUserInterfaceActionType::RadioButton, FInputChord());
	NameToCommandMap.Add("Circle_Tip", CircleBrush_Tip);

	UI_COMMAND(ViewModeNormal, "Normal", "", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ViewModeLOD, "LOD", "", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ViewModeLayerUsage, "Layer Usage", "", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ViewModeLayerDensity, "Layer Density", "", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ViewModeLayerDebug, "Layer Debug", "", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(ViewModeWireframeOnTop, "Wireframe on Top", "", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(IncreaseBrushSize, "Increase Brush Size", "Press this key to increase brush size by a fixed increment.", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::RightBracket));
	UI_COMMAND(DecreaseBrushSize, "Decrease Brush Size", "Press this key to decrease brush size by a fixed increment.", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::LeftBracket));
	UI_COMMAND(IncreaseBrushFalloff, "Increase Brush Falloff", "Press this key to increase brush falloff by a fixed increment.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(DecreaseBrushFalloff, "Decrease Brush Falloff", "Press this key to decrease brush falloff by a fixed increment.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(IncreaseBrushStrength, "Increase Brush Strength", "Press this key to increase brush strength by a fixed increment.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(DecreaseBrushStrength, "Decrease Brush Strength", "Press this key to decrease brush strength by a fixed increment.", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(DragBrushSize, "Change Brush Size", "Hold this key and then click and drag to increase or decrease brush size.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(DragBrushFalloff, "Change Brush Falloff", "Hold this key and then click and drag to increase or decrease brush falloff.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(DragBrushStrength, "Change Brush Strength", "Hold this key and then click and drag to increase or decrease brush strength.", EUserInterfaceActionType::RadioButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE
