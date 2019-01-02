// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Views/STreeView.h"
#include "Framework/SlateDelegates.h"
#include "IDetailCustomization.h"
#include "IDetailRootObjectCustomization.h"
#include "Widgets/Input/SSlider.h"
#include "MeshFractureSettings.h"

class IDetailsView;
class FMeshEditorModeToolkit;
struct FTreeItem;

// #note: cutout fracturing isn't currently working
//#define CUTOUT_ENABLED

class SCustomSlider : public SSlider
{
public:
	SLATE_BEGIN_ARGS(SCustomSlider)
		: _IndentHandle(true)
		, _Locked(false)
		, _Orientation(EOrientation::Orient_Horizontal)
		, _SliderBarColor(FLinearColor::White)
		, _SliderHandleColor(FLinearColor::White)
		, _Style(&FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider"))
		, _StepSize(0.01f)
		, _Value(0.2f)
		, _IsFocusable(true)
		, _OnMouseCaptureBegin()
		, _OnMouseCaptureEnd()
		, _OnValueChanged()
		, _OnAnalogCapture()
	{ }

	/** Whether the slidable area should be indented to fit the handle. */
	SLATE_ATTRIBUTE(bool, IndentHandle)

	/** Whether the handle is interactive or fixed. */
	SLATE_ATTRIBUTE(bool, Locked)

	/** The slider's orientation. */
	SLATE_ARGUMENT(EOrientation, Orientation)

	/** The color to draw the slider bar in. */
	SLATE_ATTRIBUTE(FSlateColor, SliderBarColor)

	/** The color to draw the slider handle in. */
	SLATE_ATTRIBUTE(FSlateColor, SliderHandleColor)

	/** The style used to draw the slider. */
	SLATE_STYLE_ARGUMENT(FSliderStyle, Style)

	/** The input mode while using the controller. */
	SLATE_ATTRIBUTE(float, StepSize)

	/** A value that drives where the slider handle appears. Value is normalized between 0 and 1. */
	SLATE_ATTRIBUTE(float, Value)

	/** Sometimes a slider should only be mouse-clickable and never keyboard focusable. */
	SLATE_ARGUMENT(bool, IsFocusable)

	/** Invoked when the mouse is pressed and a capture begins. */
	SLATE_EVENT(FSimpleDelegate, OnMouseCaptureBegin)

	/** Invoked when the mouse is released and a capture ends. */
	SLATE_EVENT(FSimpleDelegate, OnMouseCaptureEnd)

	/** Invoked when the Controller is pressed and capture begins. */
	SLATE_EVENT(FSimpleDelegate, OnControllerCaptureBegin)

	/** Invoked when the controller capture is released.  */
	SLATE_EVENT(FSimpleDelegate, OnControllerCaptureEnd)

	/** Called when the value is changed by the slider. */
	SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)

	/** Invoked when the mouse is pressed and a capture begins. */
	SLATE_EVENT(FOnFloatValueChanged, OnAnalogCapture)

	SLATE_END_ARGS()

	/** Construct Custom Slider. */
	void Construct(const SCustomSlider::FArguments& InDeclaration);

	/** Capture slider analog value when slider in use. */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Slider use begine when mouse button down. */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Slider use ends when button released. */
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	/** Holds a delegate that is executed when the mouse is let up and a capture ends. */
	FOnFloatValueChanged OnAnalogCapture;

	/** Slider control in use */
	bool IsSliderControlMoving;
};


class SFractureSettingsWidget : public SCompoundWidget
{
public:

	typedef TSlateDelegates< TSharedPtr<int32> >::FOnSelectionChanged FOnSelectionChanged;

	SLATE_BEGIN_ARGS(SFractureSettingsWidget) {}
	SLATE_ARGUMENT(UMeshFractureSettings*, FractureSettings)

	SLATE_END_ARGS()

	/** Construct the widget */
	void Construct(const FArguments& InArgs, class IMeshEditorModeEditingContract& MeshEditorMode);
	
	/** Callback for changes in the exploded view expansion slider. */
	void HandleExplodedViewSliderChanged(float NewValue);

	/** Callback for getting the exploded view expansion slider's value. */
	float HandleExplodedViewSliderValue() const;

	/** Callback for instant analog control of exploded view expansion slider's. */
	void HandleExplodedViewSliderAnalog(float NewValue);

protected:

	void CreateDetailsView();

	void HandleExplodedViewSliderChangedInternal(float NewValue);

	/** Delegate for when the common properties have changed.*/
	void OnDetailsPanelFinishedChangingProperties(const FPropertyChangedEvent& InEvent);

	// Details views for authoring fracturing settings
	TSharedPtr<IDetailsView> CommonDetailsView;
	TSharedPtr<IDetailsView> UniformDetailsView;
	TSharedPtr<IDetailsView> ClusterDetailsView;
	TSharedPtr<IDetailsView> RadialDetailsView;
	TSharedPtr<IDetailsView> SlicingDetailsView;
	TSharedPtr<IDetailsView> PlaneCutDetailsView;
#ifdef CUTOUT_ENABLED
	TSharedPtr<IDetailsView> CutoutDetailsView;
	TSharedPtr<IDetailsView> BrickDetailsView;
#endif

	/** Fracture Configuration Settings */
	UMeshFractureSettings* MeshFractureSettings;

	/** Switcher for the different fracture types */
	TSharedPtr<class SWidgetSwitcher> WidgetSwitcher;

	/** Previously selected fracture level */
	EMeshFractureLevel PreviousViewMode;

	/** Previous show bone colors mode */
	bool PrevShowBoneColors;

};
