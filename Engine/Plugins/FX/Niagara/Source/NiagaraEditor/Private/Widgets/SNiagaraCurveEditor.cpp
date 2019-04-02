// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraCurveEditor.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "SCurveEditor.h"
#include "Widgets/SOverlay.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "CurveEditorCommands.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "SCurveEditor.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "NiagaraCurveEditor"

void SNiagaraCurveEditor::Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel)
{
	SystemViewModel = InSystemViewModel;
	SystemViewModel->OnCurveOwnerChanged().AddRaw(this, &SNiagaraCurveEditor::OnCurveOwnerChanged);
	InputSnap = .1f;
	OutputSnap = .1f;

	SAssignNew(CurveEditor, SCurveEditor)
		.ShowCurveSelector(true)
		.InputSnap(this, &SNiagaraCurveEditor::GetInputSnap)
		.OutputSnap(this, &SNiagaraCurveEditor::GetOutputSnap);
	CurveEditor->SetCurveOwner(&SystemViewModel->GetCurveOwner());

	TSharedPtr<SOverlay> OverlayWidget;
	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ConstructToolBar(CurveEditor->GetCommands())
		]
		+ SVerticalBox::Slot()
		[
			CurveEditor.ToSharedRef()
		]
	];
}

SNiagaraCurveEditor::~SNiagaraCurveEditor()
{
	SystemViewModel->OnCurveOwnerChanged().RemoveAll(this);
}

TSharedRef<SWidget> SNiagaraCurveEditor::ConstructToolBar(TSharedPtr<FUICommandList> CurveEditorCommandList)
{
	FToolBarBuilder ToolBarBuilder(CurveEditorCommandList, FMultiBoxCustomization::None, TSharedPtr<FExtender>(), Orient_Horizontal, true);

	// TODO: Move this to a shared location since it's 99% the same as the sequencer curve toolbar.
	ToolBarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP(this, &SNiagaraCurveEditor::MakeCurveEditorViewOptionsMenu, CurveEditorCommandList),
		LOCTEXT("CurveEditorViewOptions", "View Options"),
		LOCTEXT("CurveEditorViewOptionsToolTip", "View Options"),
		TAttribute<FSlateIcon>(),
		true);

	TArray<SNumericDropDown<float>::FNamedValue> SnapValues;
	SnapValues.Add(SNumericDropDown<float>::FNamedValue(0.001f, LOCTEXT("Snap_OneThousandth", "0.001"), LOCTEXT("SnapDescription_OneThousandth", "Set snap to 1/1000th")));
	SnapValues.Add(SNumericDropDown<float>::FNamedValue(0.01f, LOCTEXT("Snap_OneHundredth", "0.01"), LOCTEXT("SnapDescription_OneHundredth", "Set snap to 1/100th")));
	SnapValues.Add(SNumericDropDown<float>::FNamedValue(0.1f, LOCTEXT("Snap_OneTenth", "0.1"), LOCTEXT("SnapDescription_OneTenth", "Set snap to 1/10th")));
	SnapValues.Add(SNumericDropDown<float>::FNamedValue(1.0f, LOCTEXT("Snap_One", "1"), LOCTEXT("SnapDescription_One", "Set snap to 1")));
	SnapValues.Add(SNumericDropDown<float>::FNamedValue(10.0f, LOCTEXT("Snap_Ten", "10"), LOCTEXT("SnapDescription_Ten", "Set snap to 10")));
	SnapValues.Add(SNumericDropDown<float>::FNamedValue(100.0f, LOCTEXT("Snap_OneHundred", "100"), LOCTEXT("SnapDescription_OneHundred", "Set snap to 100")));

	TSharedRef<SWidget> InputSnapWidget =
		SNew(SNumericDropDown<float>)
		.DropDownValues(SnapValues)
		.LabelText(LOCTEXT("InputSnapLabel", "Input Snap"))
		.Value(this, &SNiagaraCurveEditor::GetInputSnap)
		.OnValueChanged(this, &SNiagaraCurveEditor::SetInputSnap);

	TSharedRef<SWidget> OutputSnapWidget =
		SNew(SNumericDropDown<float>)
		.DropDownValues(SnapValues)
		.LabelText(LOCTEXT("OutputSnapLabel", "Output Snap"))
		.Value(this, &SNiagaraCurveEditor::GetOutputSnap)
		.OnValueChanged(this, &SNiagaraCurveEditor::SetOutputSnap);

	ToolBarBuilder.BeginSection("Snap");
	{
		ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().ToggleInputSnapping);
		ToolBarBuilder.AddWidget(InputSnapWidget);
		ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().ToggleOutputSnapping);
		ToolBarBuilder.AddWidget(OutputSnapWidget);
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection("Curve");
	{
		ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().ZoomToFitHorizontal);
		ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().ZoomToFitVertical);
		ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().ZoomToFit);
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection("Interpolation");
	{
		ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().InterpolationCubicAuto);
		ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().InterpolationCubicUser);
		ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().InterpolationCubicBreak);
		ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().InterpolationLinear);
		ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().InterpolationConstant);
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection("Tangents");
	{
		ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().FlattenTangents);
		ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().StraightenTangents);
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP(this, &SNiagaraCurveEditor::MakeCurveEditorCurveOptionsMenu, CurveEditorCommandList),
		LOCTEXT("CurveEditorCurveOptions", "Curves Options"),
		LOCTEXT("CurveEditorCurveOptionsToolTip", "Curve Options"),
		TAttribute<FSlateIcon>(),
		true);

	return ToolBarBuilder.MakeWidget();
}

TSharedRef<SWidget> SNiagaraCurveEditor::MakeCurveEditorViewOptionsMenu(TSharedPtr<FUICommandList> CurveEditorCommandList)
{
	FMenuBuilder MenuBuilder(true, CurveEditorCommandList);

	MenuBuilder.BeginSection("TangentVisibility", LOCTEXT("CurveEditorMenuTangentVisibilityHeader", "Tangent Visibility"));
	{
		MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetAllTangentsVisibility);
		MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetSelectedKeysTangentVisibility);
		MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetNoTangentsVisibility);
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().ToggleAutoFrameCurveEditor);
	MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().ToggleShowCurveEditorCurveToolTips);

	return MenuBuilder.MakeWidget();
}
TSharedRef<SWidget> SNiagaraCurveEditor::MakeCurveEditorCurveOptionsMenu(TSharedPtr<FUICommandList> CurveEditorCommandList)
{
	struct FExtrapolationMenus
	{
		static void MakePreInfinityExtrapSubMenu(FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection("Pre-Infinity Extrapolation", LOCTEXT("CurveEditorMenuPreInfinityExtrapHeader", "Extrapolation"));
			{
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetPreInfinityExtrapCycle);
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetPreInfinityExtrapCycleWithOffset);
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetPreInfinityExtrapOscillate);
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetPreInfinityExtrapLinear);
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetPreInfinityExtrapConstant);
			}
			MenuBuilder.EndSection();
		}

		static void MakePostInfinityExtrapSubMenu(FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection("Post-Infinity Extrapolation", LOCTEXT("CurveEditorMenuPostInfinityExtrapHeader", "Extrapolation"));
			{
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetPostInfinityExtrapCycle);
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetPostInfinityExtrapCycleWithOffset);
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetPostInfinityExtrapOscillate);
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetPostInfinityExtrapLinear);
				MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().SetPostInfinityExtrapConstant);
			}
			MenuBuilder.EndSection();
		}
	};

	FMenuBuilder MenuBuilder(true, CurveEditorCommandList);

	MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().BakeCurve);
	MenuBuilder.AddMenuEntry(FCurveEditorCommands::Get().ReduceCurve);

	MenuBuilder.AddSubMenu(
		LOCTEXT("PreInfinitySubMenu", "Pre-Infinity"),
		LOCTEXT("PreInfinitySubMenuToolTip", "Pre-Infinity Extrapolation"),
		FNewMenuDelegate::CreateStatic(&FExtrapolationMenus::MakePreInfinityExtrapSubMenu));

	MenuBuilder.AddSubMenu(
		LOCTEXT("PostInfinitySubMenu", "Post-Infinity"),
		LOCTEXT("PostInfinitySubMenuToolTip", "Post-Infinity Extrapolation"),
		FNewMenuDelegate::CreateStatic(&FExtrapolationMenus::MakePostInfinityExtrapSubMenu));

	return MenuBuilder.MakeWidget();
}


float SNiagaraCurveEditor::GetInputSnap() const
{
	return InputSnap;
}

void SNiagaraCurveEditor::SetInputSnap(float Value)
{
	InputSnap = Value;
}

float SNiagaraCurveEditor::GetOutputSnap() const
{
	return OutputSnap;
}

void SNiagaraCurveEditor::SetOutputSnap(float Value)
{
	OutputSnap = Value;
}

void SNiagaraCurveEditor::OnCurveOwnerChanged()
{
	CurveEditor->SetCurveOwner(&SystemViewModel->GetCurveOwner());
}

#undef LOCTEXT_NAMESPACE
