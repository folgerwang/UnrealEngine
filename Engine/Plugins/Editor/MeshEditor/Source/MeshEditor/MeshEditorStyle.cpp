// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MeshEditorStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateTypes.h"

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( StyleSet->RootToContentDir( RelativePath, TEXT( ".png" ) ), __VA_ARGS__ )
#define IMAGE_PLUGIN_BRUSH( RelativePath, ... ) FSlateImageBrush( FMeshEditorStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( StyleSet->RootToContentDir( RelativePath, TEXT( ".png" ) ), __VA_ARGS__ )
#define TTF_CORE_FONT( RelativePath, ... ) FSlateFontInfo( StyleSet->RootToCoreContentDir( RelativePath, TEXT( ".ttf" ) ), __VA_ARGS__ )

TSharedPtr<FSlateStyleSet> FMeshEditorStyle::StyleSet = nullptr;

FString FMeshEditorStyle::InContent( const FString& RelativePath, const ANSICHAR* Extension )
{
	static FString ContentDir = IPluginManager::Get().FindPlugin( TEXT( "MeshEditor" ) )->GetContentDir() / TEXT( "Slate" );
	return ( ContentDir / RelativePath ) + Extension;
}

void FMeshEditorStyle::Initialize()
{
	if( StyleSet.IsValid() )
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>( GetStyleSetName() );

	StyleSet->SetContentRoot( FPaths::EngineContentDir() / TEXT( "Editor/Slate") );
	StyleSet->SetCoreContentRoot( FPaths::EngineContentDir() / TEXT( "Slate" ) );

	const FVector2D Icon20x20( 20.0f, 20.0f );
	const FVector2D Icon40x40( 40.0f, 40.0f );
	const FVector2D Icon512x512(512.0f, 512.0f);
	const FLinearColor DimBackground = FLinearColor( FColor( 64, 64, 64 ) );
	const FLinearColor DimBackgroundHover = FLinearColor( FColor( 80, 80, 80 ) );
	const FLinearColor LightBackground = FLinearColor( FColor( 128, 128, 128 ) );
	const FLinearColor HighlightedBackground = FLinearColor( FColor( 255, 192, 0 ) );

	// Icons for the mode panel tabs
	StyleSet->Set( "LevelEditor.MeshEditorMode", new IMAGE_PLUGIN_BRUSH( "Icons/MeshEditorMode_40px", Icon40x40 ) );
	StyleSet->Set( "LevelEditor.MeshEditorMode.Small", new IMAGE_PLUGIN_BRUSH( "Icons/MeshEditorMode_40px", Icon20x20) );
	StyleSet->Set( "LevelEditor.MeshEditorMode.Selected", new IMAGE_PLUGIN_BRUSH( "Icons/MeshEditorMode_40px", Icon40x40 ) );
	StyleSet->Set( "LevelEditor.MeshEditorMode.Selected.Small", new IMAGE_PLUGIN_BRUSH( "Icons/MeshEditorMode_40px", Icon20x20) );

	StyleSet->Set( "EditingMode.GroupName.Text", FTextBlockStyle()
		.SetFont( TTF_CORE_FONT( "Fonts/Roboto-Bold", 12 ) )
		.SetColorAndOpacity( FLinearColor::White )
		.SetHighlightColor( FLinearColor( 0.02f, 0.3f, 0.0f ) )
		.SetHighlightShape( BOX_BRUSH( "Common/TextBlockHighlightShape", FMargin(3.f/8.f) ) ) );

	StyleSet->Set( "EditingMode.Entry", FCheckBoxStyle()
		.SetCheckBoxType( ESlateCheckBoxType::ToggleButton )
		.SetUncheckedImage( BOX_BRUSH( "Common/Selection", 8.0f / 32.0f, DimBackground ) )
		.SetUncheckedPressedImage( BOX_BRUSH( "Common/Selection", 8.0f / 32.0f, LightBackground  ) )
		.SetUncheckedHoveredImage( BOX_BRUSH( "Common/Selection", 8.0f / 32.0f, DimBackgroundHover ) )
		.SetCheckedImage( BOX_BRUSH( "Common/Selection", 8.0f / 32.0f, LightBackground ) )
		.SetCheckedHoveredImage( BOX_BRUSH( "Common/Selection", 8.0f / 32.0f, LightBackground ) )
		.SetCheckedPressedImage( BOX_BRUSH( "Common/Selection", 8.0f / 32.0f, LightBackground ) )
		.SetPadding( 0 ) );

	StyleSet->Set( "EditingMode.Entry.Text", FTextBlockStyle()
		.SetFont( TTF_CORE_FONT( "Fonts/Roboto-Bold", 10 ) )
		.SetColorAndOpacity( FLinearColor( 1.0f, 1.0f, 1.0f, 0.9f ) )
		.SetShadowOffset( FVector2D( 1, 1 ) )
		.SetShadowColorAndOpacity( FLinearColor( 0, 0, 0, 0.9f ) )
		.SetHighlightColor( FLinearColor( 0.02f, 0.3f, 0.0f ) )
		.SetHighlightShape( BOX_BRUSH( "Common/TextBlockHighlightShape", FMargin(3.f/8.f) ) ) );

	StyleSet->Set( "SelectionMode.Entry", FCheckBoxStyle()
		.SetCheckBoxType( ESlateCheckBoxType::ToggleButton )
		.SetUncheckedImage( BOX_BRUSH( "Common/Button", 8.0f / 32.0f, DimBackground ) )
		.SetUncheckedPressedImage( BOX_BRUSH( "Common/Button", 8.0f / 32.0f, LightBackground  ) )
		.SetUncheckedHoveredImage( BOX_BRUSH( "Common/Button", 8.0f / 32.0f, DimBackgroundHover ) )
		.SetCheckedImage( BOX_BRUSH( "Common/Button", 8.0f / 32.0f, HighlightedBackground ) )
		.SetCheckedHoveredImage( BOX_BRUSH( "Common/Button", 8.0f / 32.0f, HighlightedBackground ) )
		.SetCheckedPressedImage( BOX_BRUSH( "Common/Button", 8.0f / 32.0f, HighlightedBackground ) )
		.SetPadding( 6 ) );

	StyleSet->Set( "SelectionMode.Entry.Text", FTextBlockStyle()
		.SetFont( TTF_CORE_FONT( "Fonts/Roboto-Regular", 10 ) )
		.SetColorAndOpacity( FLinearColor( 1.0f, 1.0f, 1.0f, 0.9f ) )
		.SetHighlightColor( FLinearColor( 0.02f, 0.3f, 0.0f ) )
		.SetHighlightShape( BOX_BRUSH( "Common/TextBlockHighlightShape", FMargin(3.f/8.f) ) ) );

	StyleSet->Set("MeshEditorMode.AddSubdivision", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Sub_Add", Icon512x512));
	StyleSet->Set("MeshEditorMode.RemoveSubdivision", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Sub_Minus", Icon512x512));
	StyleSet->Set("MeshEditorMode.PropagateChanges", new IMAGE_PLUGIN_BRUSH("Icons/Z_Radial_Mesh_Instance", Icon512x512));
	StyleSet->Set("MeshEditorMode.EditInstance", new IMAGE_PLUGIN_BRUSH("Icons/Z_Radial_Mesh_Non_Instance", Icon512x512));

	StyleSet->Set("MeshEditorMode.PolyDelete", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Poly_Delete", Icon512x512));
	StyleSet->Set("MeshEditorMode.PolyExtrude", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Poly_Extrude", Icon512x512));
	StyleSet->Set("MeshEditorMode.PolyInset", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Poly_Inset", Icon512x512));
	StyleSet->Set("MeshEditorMode.PolyMove", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Poly_Move", Icon512x512));

	StyleSet->Set("MeshEditorMode.VertexExtend", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Vertex_Extend", Icon512x512));
	StyleSet->Set("MeshEditorMode.VertexMove", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Vertex_Move", Icon512x512));
	StyleSet->Set("MeshEditorMode.VertexWeld", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Vertex_Weld", Icon512x512));
	StyleSet->Set("MeshEditorMode.VertexRemove", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Vertex_Remove", Icon512x512));
	StyleSet->Set("MeshEditorMode.VertexDelete", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Vertex_Delete", Icon512x512));

	StyleSet->Set("MeshEditorMode.EdgeDelete", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Edge_Delete", Icon512x512));
	StyleSet->Set("MeshEditorMode.EdgeExtend", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Edge_Extend", Icon512x512));
	StyleSet->Set("MeshEditorMode.EdgeInsert", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Edge_Insert", Icon512x512));
	StyleSet->Set("MeshEditorMode.EdgeMove", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Edge_Move", Icon512x512));
	StyleSet->Set("MeshEditorMode.EdgeRemove", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Edge_Remove", Icon512x512));
	StyleSet->Set("MeshEditorMode.SelectLoop", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Edge_Select_Loop", Icon512x512));

	StyleSet->Set("MeshEditorMode.MeshEditMode", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Mesh_All", Icon512x512));
	StyleSet->Set("MeshEditorMode.PolygonEditMode", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Mesh_Poly", Icon512x512));
	StyleSet->Set("MeshEditorMode.EdgeEditMode", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Mesh_Edge", Icon512x512));
	StyleSet->Set("MeshEditorMode.VertexEditMode", new IMAGE_PLUGIN_BRUSH("Icons/T_Radial_Mesh_Vertex", Icon512x512));

	StyleSet->Set( "MeshEditorAnyElement.DeleteMeshElement", new IMAGE_PLUGIN_BRUSH( "Icons/DeleteMeshElement", Icon40x40 ) );
	StyleSet->Set( "MeshEditorAnyElement.DeleteMeshElement.Small", new IMAGE_PLUGIN_BRUSH( "Icons/DeleteMeshElement", Icon20x20) );
	StyleSet->Set( "MeshEditorAnyElement.DeleteMeshElement.Selected", new IMAGE_PLUGIN_BRUSH( "Icons/DeleteMeshElement", Icon40x40 ) );
	StyleSet->Set( "MeshEditorAnyElement.DeleteMeshElement.Selected.Small", new IMAGE_PLUGIN_BRUSH( "Icons/DeleteMeshElement", Icon20x20) );

	StyleSet->Set( "MeshEditorPolygon.FlipPolygon", new IMAGE_PLUGIN_BRUSH( "Icons/FlipPolygon", Icon40x40 ) );
	StyleSet->Set( "MeshEditorPolygon.FlipPolygon.Small", new IMAGE_PLUGIN_BRUSH( "Icons/FlipPolygon", Icon20x20) );
	StyleSet->Set( "MeshEditorPolygon.FlipPolygon.Selected", new IMAGE_PLUGIN_BRUSH( "Icons/FlipPolygon", Icon40x40 ) );
	StyleSet->Set( "MeshEditorPolygon.FlipPolygon.Selected.Small", new IMAGE_PLUGIN_BRUSH( "Icons/FlipPolygon", Icon20x20) );

	StyleSet->Set( "MeshEditorPolygon.AssignMaterial", new IMAGE_PLUGIN_BRUSH( "Icons/AssignMaterial", Icon40x40 ) );
	StyleSet->Set( "MeshEditorPolygon.AssignMaterial.Small", new IMAGE_PLUGIN_BRUSH( "Icons/AssignMaterial", Icon20x20) );
	StyleSet->Set( "MeshEditorPolygon.AssignMaterial.Selected", new IMAGE_PLUGIN_BRUSH( "Icons/AssignMaterial", Icon40x40 ) );
	StyleSet->Set( "MeshEditorPolygon.AssignMaterial.Selected.Small", new IMAGE_PLUGIN_BRUSH( "Icons/AssignMaterial", Icon20x20) );

	StyleSet->Set( "MeshEditorPolygon.UnifyNormals", new IMAGE_PLUGIN_BRUSH( "Icons/UnifyNormals", Icon40x40 ) );
	StyleSet->Set( "MeshEditorPolygon.UnifyNormals.Small", new IMAGE_PLUGIN_BRUSH( "Icons/UnifyNormals", Icon20x20) );
	StyleSet->Set( "MeshEditorPolygon.UnifyNormals.Selected", new IMAGE_PLUGIN_BRUSH( "Icons/UnifyNormals", Icon40x40 ) );
	StyleSet->Set( "MeshEditorPolygon.UnifyNormals.Selected.Small", new IMAGE_PLUGIN_BRUSH( "Icons/UnifyNormals", Icon20x20) );

	StyleSet->Set( "MeshEditorSelectionModifiers.PolygonsByGroup", new IMAGE_PLUGIN_BRUSH( "Icons/PolygonsByGroup", Icon40x40 ) );
	StyleSet->Set( "MeshEditorSelectionModifiers.PolygonsByGroup.Small", new IMAGE_PLUGIN_BRUSH( "Icons/PolygonsByGroup", Icon20x20) );
	StyleSet->Set( "MeshEditorSelectionModifiers.PolygonsByGroup.Selected", new IMAGE_PLUGIN_BRUSH( "Icons/PolygonsByGroup", Icon40x40 ) );
	StyleSet->Set( "MeshEditorSelectionModifiers.PolygonsByGroup.Selected.Small", new IMAGE_PLUGIN_BRUSH( "Icons/PolygonsByGroup", Icon20x20) );

	StyleSet->Set( "MeshEditorSelectionModifiers.SingleElement", new IMAGE_PLUGIN_BRUSH( "Icons/SingleElement", Icon40x40 ) );
	StyleSet->Set( "MeshEditorSelectionModifiers.SingleElement.Small", new IMAGE_PLUGIN_BRUSH( "Icons/SingleElement", Icon20x20) );
	StyleSet->Set( "MeshEditorSelectionModifiers.SingleElement.Selected", new IMAGE_PLUGIN_BRUSH( "Icons/SingleElement", Icon40x40 ) );
	StyleSet->Set( "MeshEditorSelectionModifiers.SingleElement.Selected.Small", new IMAGE_PLUGIN_BRUSH( "Icons/SingleElement", Icon20x20) );

	StyleSet->Set( "MeshEditorSelectionModifiers.PolygonsByConnectivity", new IMAGE_PLUGIN_BRUSH( "Icons/PolygonsByConnectivity", Icon40x40 ) );
	StyleSet->Set( "MeshEditorSelectionModifiers.PolygonsByConnectivity.Small", new IMAGE_PLUGIN_BRUSH( "Icons/PolygonsByConnectivity", Icon20x20) );
	StyleSet->Set( "MeshEditorSelectionModifiers.PolygonsByConnectivity.Selected", new IMAGE_PLUGIN_BRUSH( "Icons/PolygonsByConnectivity", Icon40x40 ) );
	StyleSet->Set( "MeshEditorSelectionModifiers.PolygonsByConnectivity.Selected.Small", new IMAGE_PLUGIN_BRUSH( "Icons/PolygonsByConnectivity", Icon20x20) );

	StyleSet->Set("MeshEditorSelectionModifiers.PolygonsBySmoothingGroup", new IMAGE_PLUGIN_BRUSH("Icons/PolygonsBySmoothingGroup", Icon40x40));
	StyleSet->Set("MeshEditorSelectionModifiers.PolygonsBySmoothingGroup.Small", new IMAGE_PLUGIN_BRUSH("Icons/PolygonsBySmoothingGroup", Icon20x20));
	StyleSet->Set("MeshEditorSelectionModifiers.PolygonsBySmoothingGroup.Selected", new IMAGE_PLUGIN_BRUSH("Icons/PolygonsBySmoothingGroup", Icon40x40));
	StyleSet->Set("MeshEditorSelectionModifiers.PolygonsBySmoothingGroup.Selected.Small", new IMAGE_PLUGIN_BRUSH("Icons/PolygonsBySmoothingGroup", Icon20x20));

	FSlateStyleRegistry::RegisterSlateStyle( *StyleSet.Get() );
}


void FMeshEditorStyle::Shutdown()
{
	if( StyleSet.IsValid() )
	{
		FSlateStyleRegistry::UnRegisterSlateStyle( *StyleSet.Get() );
		ensure( StyleSet.IsUnique() );
		StyleSet.Reset();
	}
}


FName FMeshEditorStyle::GetStyleSetName()
{
	static FName StyleName( "MeshEditorStyle" );
	return StyleName;
}
