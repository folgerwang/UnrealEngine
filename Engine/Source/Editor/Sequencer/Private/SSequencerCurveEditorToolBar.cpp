// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SSequencerCurveEditorToolBar.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "SequencerCommonHelpers.h"
#include "SequencerSettings.h"
#include "CurveEditorCommands.h"
#include "SequencerCommands.h"

#define LOCTEXT_NAMESPACE "CurveEditorToolBar"

void SSequencerCurveEditorToolBar::Construct( const FArguments& InArgs, TSharedRef<FSequencer> InSequencer, TSharedPtr<FUICommandList> CurveEditorCommandList )
{
	Sequencer = InSequencer;
	SequencerSettings = InSequencer->GetSequencerSettings();

	TArray<SNumericDropDown<float>::FNamedValue> SnapValues = {
		SNumericDropDown<float>::FNamedValue( 0.001f, LOCTEXT( "Snap_OneThousandth", "0.001" ), LOCTEXT( "SnapDescription_OneThousandth", "Set snap to 1/1000th" ) ),
		SNumericDropDown<float>::FNamedValue( 0.01f,  LOCTEXT( "Snap_OneHundredth",  "0.01" ),  LOCTEXT( "SnapDescription_OneHundredth",  "Set snap to 1/100th" ) ),
		SNumericDropDown<float>::FNamedValue( 0.1f,   LOCTEXT( "Snap_OneTenth",      "0.1" ),   LOCTEXT( "SnapDescription_OneTenth",      "Set snap to 1/10th" ) ),
		SNumericDropDown<float>::FNamedValue( 1.0f,   LOCTEXT( "Snap_One",           "1" ),     LOCTEXT( "SnapDescription_One",           "Set snap to 1" ) ),
		SNumericDropDown<float>::FNamedValue( 10.0f,  LOCTEXT( "Snap_Ten",           "10" ),    LOCTEXT( "SnapDescription_Ten",           "Set snap to 10" ) ),
		SNumericDropDown<float>::FNamedValue( 100.0f, LOCTEXT( "Snap_OneHundred",    "100" ),   LOCTEXT( "SnapDescription_OneHundred",    "Set snap to 100" ) ),
	};

	FToolBarBuilder ToolBarBuilder( CurveEditorCommandList, FMultiBoxCustomization::None, TSharedPtr<FExtender>(), Orient_Horizontal, true );

	ToolBarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP( this, &SSequencerCurveEditorToolBar::MakeCurveEditorViewOptionsMenu, CurveEditorCommandList ),
		LOCTEXT( "CurveEditorViewOptions", "View Options" ),
		LOCTEXT( "CurveEditorViewOptionsToolTip", "View Options" ),
		TAttribute<FSlateIcon>(),
		true );

	ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().ToggleOutputSnapping);

	ToolBarBuilder.AddWidget(
		SNew( SImage )
			.Image(FEditorStyle::GetBrush("Sequencer.Value.Small")) );

	ToolBarBuilder.AddWidget(
		SNew( SBox )
			.VAlign( VAlign_Center )
			[
				SNew( SNumericDropDown<float> )
					.DropDownValues( SnapValues )
					.ToolTipText( LOCTEXT( "ValueSnappingIntervalToolTip", "Curve value snapping interval" ) )
					.Value( this, &SSequencerCurveEditorToolBar::OnGetValueSnapInterval )
					.OnValueChanged( this, &SSequencerCurveEditorToolBar::OnValueSnapIntervalChanged )
			]);

	ToolBarBuilder.BeginSection( "Curve" );
	{
		ToolBarBuilder.AddToolBarButton( FCurveEditorCommands::Get().ZoomToFitHorizontal );
		ToolBarBuilder.AddToolBarButton( FCurveEditorCommands::Get().ZoomToFitVertical );
		ToolBarBuilder.AddToolBarButton( FCurveEditorCommands::Get().ZoomToFit );
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection( "Interpolation" );
	{
		ToolBarBuilder.AddToolBarButton( FCurveEditorCommands::Get().InterpolationCubicAuto );
		ToolBarBuilder.AddToolBarButton( FCurveEditorCommands::Get().InterpolationCubicUser );
		ToolBarBuilder.AddToolBarButton( FCurveEditorCommands::Get().InterpolationCubicBreak );
		ToolBarBuilder.AddToolBarButton( FCurveEditorCommands::Get().InterpolationLinear );
		ToolBarBuilder.AddToolBarButton( FCurveEditorCommands::Get().InterpolationConstant );
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection( "Tangents" );
	{
		ToolBarBuilder.AddToolBarButton( FCurveEditorCommands::Get().InterpolationToggleWeighted );
		ToolBarBuilder.AddToolBarButton( FCurveEditorCommands::Get().FlattenTangents );
		ToolBarBuilder.AddToolBarButton( FCurveEditorCommands::Get().StraightenTangents );
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP( this, &SSequencerCurveEditorToolBar::MakeCurveEditorCurveOptionsMenu, CurveEditorCommandList ),
		LOCTEXT( "CurveEditorCurveOptions", "Curves Options" ),
		LOCTEXT( "CurveEditorCurveOptionsToolTip", "Curve Options" ),
		TAttribute<FSlateIcon>(),
		true );

	ChildSlot
	[
		ToolBarBuilder.MakeWidget()
	];
}

TSharedRef<SWidget> SSequencerCurveEditorToolBar::MakeCurveEditorViewOptionsMenu(TSharedPtr<FUICommandList> CurveEditorCommandList)
{
	FMenuBuilder MenuBuilder( true, CurveEditorCommandList );

	MenuBuilder.BeginSection( "CurveVisibility", LOCTEXT( "CurveEditorMenuCurveVisibilityHeader", "Curve Visibility" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().SetAllCurveVisibility );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().SetSelectedCurveVisibility );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().SetAnimatedCurveVisibility );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "TangentVisibility", LOCTEXT( "CurveEditorMenuTangentVisibilityHeader", "Tangent Visibility" ) );
	{
		MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetAllTangentsVisibility );
		MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetSelectedKeysTangentVisibility );
		MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetNoTangentsVisibility );
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().ToggleAutoFrameCurveEditor);
	MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().ToggleShowCurveEditorCurveToolTips);

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencerCurveEditorToolBar::MakeCurveEditorCurveOptionsMenu(TSharedPtr<FUICommandList> CurveEditorCommandList)
{
	struct FExtrapolationMenus
	{
		static void MakePreInfinityExtrapSubMenu(FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection( "Pre-Infinity Extrapolation", LOCTEXT( "CurveEditorMenuPreInfinityExtrapHeader", "Extrapolation" ) );
			{
				MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetPreInfinityExtrapCycle);
				MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetPreInfinityExtrapCycleWithOffset);
				MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetPreInfinityExtrapOscillate);
				MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetPreInfinityExtrapLinear);
				MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetPreInfinityExtrapConstant);
			}
			MenuBuilder.EndSection();
		}

		static void MakePostInfinityExtrapSubMenu(FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection( "Post-Infinity Extrapolation", LOCTEXT( "CurveEditorMenuPostInfinityExtrapHeader", "Extrapolation" ) );
			{
				MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetPostInfinityExtrapCycle);
				MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetPostInfinityExtrapCycleWithOffset);
				MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetPostInfinityExtrapOscillate);
				MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetPostInfinityExtrapLinear);
				MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().SetPostInfinityExtrapConstant);
			}
			MenuBuilder.EndSection();
		}
	};

	FMenuBuilder MenuBuilder( true, CurveEditorCommandList );

	MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().BakeCurve);
	MenuBuilder.AddMenuEntry( FCurveEditorCommands::Get().ReduceCurve);

	MenuBuilder.AddSubMenu(
		LOCTEXT( "PreInfinitySubMenu", "Pre-Infinity" ),
		LOCTEXT( "PreInfinitySubMenuToolTip", "Pre-Infinity Extrapolation" ),
		FNewMenuDelegate::CreateStatic( &FExtrapolationMenus::MakePreInfinityExtrapSubMenu ) );

	MenuBuilder.AddSubMenu(
		LOCTEXT( "PostInfinitySubMenu", "Post-Infinity" ),
		LOCTEXT( "PostInfinitySubMenuToolTip", "Post-Infinity Extrapolation" ),
		FNewMenuDelegate::CreateStatic( &FExtrapolationMenus::MakePostInfinityExtrapSubMenu ) );
	
	return MenuBuilder.MakeWidget();
}

float SSequencerCurveEditorToolBar::OnGetValueSnapInterval() const
{
	return SequencerSettings->GetCurveValueSnapInterval();
}


void SSequencerCurveEditorToolBar::OnValueSnapIntervalChanged( float InInterval )
{
	SequencerSettings->SetCurveValueSnapInterval( InInterval );
}




#undef LOCTEXT_NAMESPACE
