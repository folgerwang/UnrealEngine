// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SFractureSettingsWidget.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Layout/Visibility.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SButton.h"

#include "DetailLayoutBuilder.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "EditorSupportDelegates.h"

#include "IMeshEditorModeEditingContract.h"
#include "EditableMesh.h"
#include "EditorSupportDelegates.h"
#include "FractureToolDelegates.h"

#define LOCTEXT_NAMESPACE "FractureSettingsWidget"

void SCustomSlider::Construct(const SCustomSlider::FArguments& InDeclaration)
{
	SSlider::Construct(SSlider::FArguments()
		.Style(InDeclaration._Style)
		.IsFocusable(InDeclaration._IsFocusable)
		.OnMouseCaptureBegin(InDeclaration._OnMouseCaptureBegin)
		.OnMouseCaptureEnd(InDeclaration._OnMouseCaptureEnd)
		.OnControllerCaptureBegin(InDeclaration._OnControllerCaptureBegin)
		.OnControllerCaptureEnd(InDeclaration._OnControllerCaptureEnd)
		.OnValueChanged(InDeclaration._OnValueChanged));

	OnAnalogCapture = InDeclaration._OnAnalogCapture;
	IsSliderControlMoving = false;
	SetValue(0.0f);
}

void SCustomSlider::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (IsSliderControlMoving)
	{
		OnAnalogCapture.ExecuteIfBound(GetValue());
	}
}

FReply SCustomSlider::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	IsSliderControlMoving = true;
	FFractureToolDelegates::Get().OnFractureExpansionBegin.Broadcast();
	return SSlider::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SCustomSlider::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	IsSliderControlMoving = false;
	FFractureToolDelegates::Get().OnFractureExpansionUpdate.Broadcast();
	return SSlider::OnMouseButtonUp(MyGeometry, MouseEvent);
}

void SFractureSettingsWidget::Construct(const FArguments& InArgs, IMeshEditorModeEditingContract& MeshEditorModeIn)
{
	PreviousViewMode = EMeshFractureLevel::AllLevels;
	PrevShowBoneColors = true;
	MeshFractureSettings = InArgs._FractureSettings;
	check(MeshFractureSettings);
	check(MeshFractureSettings->CommonSettings);

	CreateDetailsView();

	const EMeshFractureMode& FractureMode = MeshFractureSettings->CommonSettings->FractureMode;

	// Uses the widget switcher widget so only the widget in the slot which corresponds to the selected mesh element type will be shown
	WidgetSwitcher = SNew(SWidgetSwitcher)
		.WidgetIndex_Lambda([&FractureMode]() -> int32
	{
		return static_cast<int32>(FractureMode);
	}
	);
	
	WidgetSwitcher->AddSlot(static_cast<int32>(EMeshFractureMode::Uniform))
	[
			SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			UniformDetailsView->AsShared()
		]
	];

	WidgetSwitcher->AddSlot(static_cast<int32>(EMeshFractureMode::Clustered))
	[
			SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			ClusterDetailsView->AsShared()
		]
	];

	WidgetSwitcher->AddSlot(static_cast<int32>(EMeshFractureMode::Radial))
	[
			SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			RadialDetailsView->AsShared()
		]
	];

	WidgetSwitcher->AddSlot(static_cast<int32>(EMeshFractureMode::Slicing))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SlicingDetailsView->AsShared()
		]
	];

	WidgetSwitcher->AddSlot(static_cast<int32>(EMeshFractureMode::PlaneCut))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			PlaneCutDetailsView->AsShared()
		]
	];

#ifdef CUTOUT_ENABLED
	WidgetSwitcher->AddSlot(static_cast<int32>(EMeshFractureMode::Cutout))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			CutoutDetailsView->AsShared()
		]
	];

	WidgetSwitcher->AddSlot(static_cast<int32>(EMeshFractureMode::Brick))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		[
			BrickDetailsView->AsShared()
		]
		];

#endif // CUTOUT_ENABLED

	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0)
			[
				CommonDetailsView->AsShared()
			]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0)
				[
					SNew(SCustomSlider)
					.OnValueChanged(this, &SFractureSettingsWidget::HandleExplodedViewSliderChanged)
					.OnAnalogCapture(this, &SFractureSettingsWidget::HandleExplodedViewSliderAnalog)
					.Value(this, &SFractureSettingsWidget::HandleExplodedViewSliderValue)
					.ToolTipText(LOCTEXT("ExplodedViewToolTip", "Show fractured pieces as an exploded view."))
				]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.Padding(0.0f)
				[
					WidgetSwitcher.ToSharedRef()
				]
			]

			// Separator
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 1)
				[
					SNew(SSeparator)
					.Visibility(EVisibility::Visible)
				]
		];

}

void SFractureSettingsWidget::HandleExplodedViewSliderChanged(float NewValue)
{
	HandleExplodedViewSliderChangedInternal(NewValue);
}

void SFractureSettingsWidget::HandleExplodedViewSliderChangedInternal(float NewValue)
{
	UMeshFractureSettings::ExplodedViewExpansion = NewValue;
	FFractureToolDelegates::Get().OnUpdateExplodedView.Broadcast(static_cast<uint8>(EViewResetType::RESET_TRANSFORMS), static_cast<uint8>(MeshFractureSettings->CommonSettings->ViewMode));
}

float SFractureSettingsWidget::HandleExplodedViewSliderValue() const
{
	return UMeshFractureSettings::ExplodedViewExpansion;
}

void SFractureSettingsWidget::HandleExplodedViewSliderAnalog(float NewValue)
{
	HandleExplodedViewSliderChangedInternal(NewValue);
}

void SFractureSettingsWidget::CreateDetailsView()
{
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs(
		/*bUpdateFromSelection=*/ false,
		/*bLockable=*/ false,
		/*bAllowSearch=*/ false,
		FDetailsViewArgs::HideNameArea,
		/*bHideSelectionTip=*/ true,
		/*InNotifyHook=*/ nullptr,
		/*InSearchInitialKeyFocus=*/ false,
		/*InViewIdentifier=*/ NAME_None);
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;

	CommonDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	CommonDetailsView->SetObject(MeshFractureSettings->CommonSettings, true);
	CommonDetailsView->OnFinishedChangingProperties().AddRaw(this, &SFractureSettingsWidget::OnDetailsPanelFinishedChangingProperties);

	UniformDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	UniformDetailsView->SetObject(MeshFractureSettings->UniformSettings, true);

	ClusterDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	ClusterDetailsView->SetObject(MeshFractureSettings->ClusterSettings, true);

	RadialDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	RadialDetailsView->SetObject(MeshFractureSettings->RadialSettings, true);

	SlicingDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	SlicingDetailsView->SetObject(MeshFractureSettings->SlicingSettings, true);

	PlaneCutDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	PlaneCutDetailsView->SetObject(MeshFractureSettings->PlaneCutSettings, true);

#ifdef CUTOUT_ENABLED
	CutoutDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	CutoutDetailsView->SetObject(MeshFractureSettings->CutoutSettings, true);

	BrickDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	BrickDetailsView->SetObject(MeshFractureSettings->BrickSettings, true);
#endif
}

void SFractureSettingsWidget::OnDetailsPanelFinishedChangingProperties(const FPropertyChangedEvent& InEvent)
{
	check(MeshFractureSettings);
	UCommonFractureSettings* CommonSettings = MeshFractureSettings->CommonSettings;
	check(CommonSettings);

	const EMeshFractureMode& FractureMode = CommonSettings->FractureMode;
	if (WidgetSwitcher.IsValid())
	{
		WidgetSwitcher->SetActiveWidgetIndex(static_cast<int32>(FractureMode));
	}

	if (PreviousViewMode != CommonSettings->ViewMode)
	{
		PreviousViewMode = CommonSettings->ViewMode;
		// show/hide bones based on their level in the hierarchy
		FFractureToolDelegates::Get().OnUpdateFractureLevelView.Broadcast(static_cast<uint8>(CommonSettings->ViewMode));
	}

	if ( PrevShowBoneColors != CommonSettings->ShowBoneColors)
	{
		PrevShowBoneColors = CommonSettings->ShowBoneColors;
		FFractureToolDelegates::Get().OnVisualizationSettingsChanged.Broadcast(CommonSettings->ShowBoneColors);

	}

}

#undef LOCTEXT_NAMESPACE

