// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SCurveEditorPanel.h"
#include "Templates/Tuple.h"
#include "Rendering/DrawElements.h"
#include "CurveDrawInfo.h"
#include "CurveEditorSettings.h"
#include "CurveEditorContextMenu.h"
#include "DragOperations/CurveEditorDragOperation_Tangent.h"
#include "DragOperations/CurveEditorDragOperation_MoveKeys.h"
#include "DragOperations/CurveEditorDragOperation_Pan.h"
#include "DragOperations/CurveEditorDragOperation_Zoom.h"
#include "DragOperations/CurveEditorDragOperation_Marquee.h"
#include "CurveEditorCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/SToolTip.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "Misc/Attribute.h"
#include "Algo/Sort.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "CurveEditorEditObjectContainer.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "SCurveEditorPanel"

namespace
{
	static float HoverProximityThresholdPx = 5.f;
	static float HoveredCurveThickness = 5.f;
	static float UnHoveredCurveThickness = 1.f;
	static float LabelOffsetPx = 2.f;
	static bool  bAntiAliasCurves = true;

	FReply HandledReply(const FGeometry&, const FPointerEvent&)
	{
		return FReply::Handled();
	}
}


TUniquePtr<ICurveEditorKeyDragOperation> CreateKeyDrag(ECurvePointType KeyType)
{
	switch (KeyType)
	{
	case ECurvePointType::ArriveTangent:
	case ECurvePointType::LeaveTangent:
		return MakeUnique<FCurveEditorDragOperation_Tangent>();

	default:
		return MakeUnique<FCurveEditorDragOperation_MoveKeys>();
	}
}

class SDynamicToolTip : public SToolTip
{
public:
	TAttribute<bool> bIsEnabled;
	virtual bool IsEmpty() const override { return !bIsEnabled.Get(); }
};

SCurveEditorPanel::SCurveEditorPanel()
{
	EditObjects = MakeUnique<FCurveEditorEditObjectContainer>();
	bSelectionSupportsWeightedTangents = false;
}

SCurveEditorPanel::~SCurveEditorPanel()
{
}

void SCurveEditorPanel::Construct(const FArguments& InArgs, TSharedRef<FCurveEditor> InCurveEditor)
{
	GridLineTintAttribute = InArgs._GridLineTint;

	CachedSelectionSerialNumber = 0;
	CurveEditor = InCurveEditor;

	ReduceTolerance = 0.1f;

	CurveEditor->BindCommands();

	CommandList = MakeShared<FUICommandList>();
	CommandList->Append(InCurveEditor->GetCommands().ToSharedRef());

	BindCommands();
	SetClipping(EWidgetClipping::ClipToBounds);

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

		KeyDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	}

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		.VAlign(VAlign_Top)
		[
			SNew(SSplitter)
			.Visibility(this, &SCurveEditorPanel::GetSplitterVisibility)

			+ SSplitter::Slot()
			.Value(0.25f)
			[
				SNew(SBorder)
				.Padding(FMargin(0))
				.BorderImage(FEditorStyle::GetBrush("NoBorder"))
				.OnMouseButtonDown_Static(HandledReply)
				.OnMouseMove_Static(HandledReply)
				.OnMouseButtonUp_Static(HandledReply)
				[
					KeyDetailsView.ToSharedRef()
				]
			]

			+ SSplitter::Slot()
			.Value(0.75f)
			[
				SNullWidget::NullWidget
			]
		]
	];

	UpdateEditBox();

	TSharedRef<SDynamicToolTip> ToolTipWidget =
		SNew(SDynamicToolTip)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolTip.BrightBackground"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(this, &SCurveEditorPanel::GetToolTipCurveName)
				.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
				.ColorAndOpacity(FLinearColor::Black)
			]
			+ SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(this, &SCurveEditorPanel::GetToolTipTimeText)
				.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
				.ColorAndOpacity(FLinearColor::Black)
			]
			+ SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(this, &SCurveEditorPanel::GetToolTipValueText)
				.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
				.ColorAndOpacity(FLinearColor::Black)
			]
		];

	ToolTipWidget->bIsEnabled = MakeAttributeSP(this, &SCurveEditorPanel::IsToolTipEnabled);
	SetToolTip(ToolTipWidget);
}

void SCurveEditorPanel::BindCommands()
{
	// Interpolation and tangents
	{
		FExecuteAction SetConstant   = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetKeyAttributes, FKeyAttributes().SetInterpMode(RCIM_Constant).SetTangentMode(RCTM_Auto), LOCTEXT("SetInterpConstant", "Set Interp Constant"));
		FExecuteAction SetLinear     = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetKeyAttributes, FKeyAttributes().SetInterpMode(RCIM_Linear).SetTangentMode(RCTM_Auto),   LOCTEXT("SetInterpLinear",   "Set Interp Linear"));
		FExecuteAction SetCubicAuto  = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetKeyAttributes, FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_Auto),    LOCTEXT("SetInterpCubic",    "Set Interp Auto"));
		FExecuteAction SetCubicUser  = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetKeyAttributes, FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_User),    LOCTEXT("SetInterpUser",     "Set Interp User"));
		FExecuteAction SetCubicBreak = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetKeyAttributes, FKeyAttributes().SetInterpMode(RCIM_Cubic).SetTangentMode(RCTM_Break),   LOCTEXT("SetInterpBreak",    "Set Interp Break"));

		FExecuteAction    ToggleWeighted    = FExecuteAction::CreateSP(this, &SCurveEditorPanel::ToggleWeightedTangents);
		FCanExecuteAction CanToggleWeighted = FCanExecuteAction::CreateSP(this, &SCurveEditorPanel::CanToggleWeightedTangents);

		FIsActionChecked IsConstantCommon   = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonInterpolationMode, RCIM_Constant);
		FIsActionChecked IsLinearCommon     = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonInterpolationMode, RCIM_Linear);
		FIsActionChecked IsCubicAutoCommon  = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonTangentMode, RCIM_Cubic, RCTM_Auto);
		FIsActionChecked IsCubicUserCommon  = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonTangentMode, RCIM_Cubic, RCTM_User);
		FIsActionChecked IsCubicBreakCommon = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonTangentMode, RCIM_Cubic, RCTM_Break);
		FIsActionChecked IsCubicWeightCommon = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonTangentWeightMode, RCIM_Cubic, RCTWM_WeightedBoth);

		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationConstant, SetConstant, FCanExecuteAction(), IsConstantCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationLinear, SetLinear, FCanExecuteAction(), IsLinearCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationCubicAuto, SetCubicAuto, FCanExecuteAction(), IsCubicAutoCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationCubicUser, SetCubicUser, FCanExecuteAction(), IsCubicUserCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationCubicBreak, SetCubicBreak, FCanExecuteAction(), IsCubicBreakCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().InterpolationToggleWeighted, ToggleWeighted, CanToggleWeighted, IsCubicWeightCommon);

	}

	// Pre Extrapolation Modes
	{
		FExecuteAction SetCycle           = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetCurveAttributes, FCurveAttributes().SetPreExtrapolation(RCCE_Cycle),           LOCTEXT("SetPreExtrapCycle",           "Set Pre Extrapolation (Cycle)"));
		FExecuteAction SetCycleWithOffset = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetCurveAttributes, FCurveAttributes().SetPreExtrapolation(RCCE_CycleWithOffset), LOCTEXT("SetPreExtrapCycleWithOffset", "Set Pre Extrapolation (Cycle With Offset)"));
		FExecuteAction SetOscillate       = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetCurveAttributes, FCurveAttributes().SetPreExtrapolation(RCCE_Oscillate),       LOCTEXT("SetPreExtrapOscillate",       "Set Pre Extrapolation (Oscillate)"));
		FExecuteAction SetLinear          = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetCurveAttributes, FCurveAttributes().SetPreExtrapolation(RCCE_Linear),          LOCTEXT("SetPreExtrapLinear",          "Set Pre Extrapolation (Linear)"));
		FExecuteAction SetConstant        = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetCurveAttributes, FCurveAttributes().SetPreExtrapolation(RCCE_Constant),        LOCTEXT("SetPreExtrapConstant",        "Set Pre Extrapolation (Constant)"));

		FIsActionChecked IsCycleCommon           = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonPreExtrapolationMode, RCCE_Cycle);
		FIsActionChecked IsCycleWithOffsetCommon = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonPreExtrapolationMode, RCCE_CycleWithOffset);
		FIsActionChecked IsOscillateCommon       = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonPreExtrapolationMode, RCCE_Oscillate);
		FIsActionChecked IsLinearCommon          = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonPreExtrapolationMode, RCCE_Linear);
		FIsActionChecked IsConstantCommon        = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonPreExtrapolationMode, RCCE_Constant);

		CommandList->MapAction(FCurveEditorCommands::Get().SetPreInfinityExtrapCycle, SetCycle, FCanExecuteAction(), IsCycleCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().SetPreInfinityExtrapCycleWithOffset, SetCycleWithOffset, FCanExecuteAction(), IsCycleWithOffsetCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().SetPreInfinityExtrapOscillate, SetOscillate, FCanExecuteAction(), IsOscillateCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().SetPreInfinityExtrapLinear, SetLinear, FCanExecuteAction(), IsLinearCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().SetPreInfinityExtrapConstant, SetConstant, FCanExecuteAction(), IsConstantCommon);
	}

	// Post Extrapolation Modes
	{
		FExecuteAction SetCycle           = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetCurveAttributes, FCurveAttributes().SetPostExtrapolation(RCCE_Cycle),           LOCTEXT("SetPostExtrapCycle",           "Set Post Extrapolation (Cycle)"));
		FExecuteAction SetCycleWithOffset = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetCurveAttributes, FCurveAttributes().SetPostExtrapolation(RCCE_CycleWithOffset), LOCTEXT("SetPostExtrapCycleWithOffset", "Set Post Extrapolation (Cycle With Offset)"));
		FExecuteAction SetOscillate       = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetCurveAttributes, FCurveAttributes().SetPostExtrapolation(RCCE_Oscillate),       LOCTEXT("SetPostExtrapOscillate",       "Set Post Extrapolation (Oscillate)"));
		FExecuteAction SetLinear          = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetCurveAttributes, FCurveAttributes().SetPostExtrapolation(RCCE_Linear),          LOCTEXT("SetPostExtrapLinear",          "Set Post Extrapolation (Linear)"));
		FExecuteAction SetConstant        = FExecuteAction::CreateSP(this, &SCurveEditorPanel::SetCurveAttributes, FCurveAttributes().SetPostExtrapolation(RCCE_Constant),        LOCTEXT("SetPostExtrapConstant",        "Set Post Extrapolation (Constant)"));

		FIsActionChecked IsCycleCommon           = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonPostExtrapolationMode, RCCE_Cycle);
		FIsActionChecked IsCycleWithOffsetCommon = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonPostExtrapolationMode, RCCE_CycleWithOffset);
		FIsActionChecked IsOscillateCommon       = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonPostExtrapolationMode, RCCE_Oscillate);
		FIsActionChecked IsLinearCommon          = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonPostExtrapolationMode, RCCE_Linear);
		FIsActionChecked IsConstantCommon        = FIsActionChecked::CreateSP(this, &SCurveEditorPanel::CompareCommonPostExtrapolationMode, RCCE_Constant);

		CommandList->MapAction(FCurveEditorCommands::Get().SetPostInfinityExtrapCycle, SetCycle, FCanExecuteAction(), IsCycleCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().SetPostInfinityExtrapCycleWithOffset, SetCycleWithOffset, FCanExecuteAction(), IsCycleWithOffsetCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().SetPostInfinityExtrapOscillate, SetOscillate, FCanExecuteAction(), IsOscillateCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().SetPostInfinityExtrapLinear, SetLinear, FCanExecuteAction(), IsLinearCommon);
		CommandList->MapAction(FCurveEditorCommands::Get().SetPostInfinityExtrapConstant, SetConstant, FCanExecuteAction(), IsConstantCommon);
	}
	//override for key reduction so we show popup
	{
		CommandList->MapAction(FCurveEditorCommands::Get().ReduceCurve, FExecuteAction::CreateSP(this, &SCurveEditorPanel::OnSimplifySelection));
	}
}

void SCurveEditorPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UpdateCommonCurveInfo();
	UpdateEditBox();

	CurveEditor->UpdateGeometry(AllottedGeometry);

	CachedDrawParams.Reset();
	CurveEditor->GetCurveDrawParams(CachedDrawParams);

	CachedSelectionSerialNumber = CurveEditor->Selection.GetSerialNumber();
}

void SCurveEditorPanel::UpdateCommonCurveInfo()
{
	// Gather up common extended curve info for the current set of curves
	TOptional<FCurveAttributes> AccumulatedCurveAttributes;
	for (const TTuple<FCurveModelID, TUniquePtr<FCurveModel>>& Pair : CurveEditor->GetCurves())
	{
		FCurveAttributes Attributes;
		Pair.Value->GetCurveAttributes(Attributes);

		if (!AccumulatedCurveAttributes.IsSet())
		{
			AccumulatedCurveAttributes = Attributes;
		}
		else
		{
			AccumulatedCurveAttributes = FCurveAttributes::MaskCommon(AccumulatedCurveAttributes.GetValue(), Attributes);
		}
	}

	// Reset the common curve and key info
	bSelectionSupportsWeightedTangents = false;
	CachedCommonCurveAttributes = AccumulatedCurveAttributes.Get(FCurveAttributes());

	TOptional<FKeyAttributes> AccumulatedKeyAttributes;
	TArray<FKeyAttributes> AllKeyAttributes;

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveEditor->Selection.GetAll())
	{
		FCurveModel* Curve = CurveEditor->FindCurve(Pair.Key);
		if (Curve)
		{
			AllKeyAttributes.Reset();
			AllKeyAttributes.SetNum(Pair.Value.Num());

			Curve->GetKeyAttributes(Pair.Value.AsArray(), AllKeyAttributes);
			for (const FKeyAttributes& Attributes : AllKeyAttributes)
			{
				if (Attributes.HasTangentWeightMode())
				{
					bSelectionSupportsWeightedTangents = true;
				}

				if (!AccumulatedKeyAttributes.IsSet())
				{
					AccumulatedKeyAttributes = Attributes;
				}
				else
				{
					AccumulatedKeyAttributes = FKeyAttributes::MaskCommon(AccumulatedKeyAttributes.GetValue(), Attributes);
				}
			}
		}
	}

	// Reset the common curve and key info
	CachedCommonKeyAttributes = AccumulatedKeyAttributes.Get(FKeyAttributes());
}

void SCurveEditorPanel::RebindContextualActions(FVector2D MousePosition)
{
	CommandList->UnmapAction(FCurveEditorCommands::Get().AddKeyHovered);
	CommandList->UnmapAction(FCurveEditorCommands::Get().AddKeyToAllCurves);
	CommandList->UnmapAction(FCurveEditorCommands::Get().AddKeyToAllCurvesHere);

	TOptional<FCurveModelID> HoveredCurve = GetHoveredCurve();
	if (HoveredCurve.IsSet())
	{
		CommandList->MapAction(FCurveEditorCommands::Get().AddKeyHovered, FExecuteAction::CreateSP(this, &SCurveEditorPanel::OnAddKey, HoveredCurve.GetValue(), MousePosition));
	}
	CommandList->MapAction(FCurveEditorCommands::Get().AddKeyToAllCurves, FExecuteAction::CreateSP(this, &SCurveEditorPanel::OnAddKey, MousePosition));
	CommandList->MapAction(FCurveEditorCommands::Get().AddKeyToAllCurvesHere, FExecuteAction::CreateSP(this, &SCurveEditorPanel::OnAddKeyHere, MousePosition));
}

void SCurveEditorPanel::UpdateEditBox()
{
	const FCurveEditorSelection& Selection = CurveEditor->Selection;
	for (TTuple<FCurveModelID, TMap<FKeyHandle, UObject*>>& OuterPair : EditObjects->CurveIDToKeyProxies)
	{
		const FKeyHandleSet* SelectedKeys = Selection.FindForCurve(OuterPair.Key);

		for (TTuple<FKeyHandle, UObject*>& InnerPair : OuterPair.Value)
		{
			if (ICurveEditorKeyProxy* Proxy = Cast<ICurveEditorKeyProxy>(InnerPair.Value))
			{
				Proxy->UpdateValuesFromRawData();
			}
		}
	}

	if (CachedSelectionSerialNumber == Selection.GetSerialNumber())
	{
		return;
	}

	TArray<FKeyHandle> KeyHandleScratch;
	TArray<UObject*>   NewProxiesScratch;

	TArray<UObject*> AllEditObjects;
	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
	{
		FCurveModel* Curve = CurveEditor->FindCurve(Pair.Key);
		if (!Curve)
		{
			continue;
		}

		KeyHandleScratch.Reset();
		NewProxiesScratch.Reset();

		TMap<FKeyHandle, UObject*>& KeyHandleToEditObject = EditObjects->CurveIDToKeyProxies.FindOrAdd(Pair.Key);
		for (FKeyHandle Handle : Pair.Value.AsArray())
		{
			if (UObject* Existing = KeyHandleToEditObject.FindRef(Handle))
			{
				AllEditObjects.Add(Existing);
			}
			else
			{
				KeyHandleScratch.Add(Handle);
			}
		}

		if (KeyHandleScratch.Num() > 0)
		{
			NewProxiesScratch.SetNum(KeyHandleScratch.Num());
			Curve->CreateKeyProxies(KeyHandleScratch, NewProxiesScratch);

			for (int32 Index = 0; Index < KeyHandleScratch.Num(); ++Index)
			{
				if (UObject* NewObject = NewProxiesScratch[Index])
				{
					KeyHandleToEditObject.Add(KeyHandleScratch[Index], NewObject);
					AllEditObjects.Add(NewObject);
				}
			}
		}
	}

	KeyDetailsView->SetObjects(AllEditObjects);
}

void SCurveEditorPanel::UpdateCurveProximities(FVector2D MousePixel)
{
	CachedToolTipData.Reset();

	// Update curve proximities
	CurveProximities.Reset();

	FCurveEditorScreenSpace ScreenSpace = CurveEditor->GetScreenSpace();

	double MinMouseTime    = ScreenSpace.ScreenToSeconds(MousePixel.X - HoverProximityThresholdPx);
	double MaxMouseTime    = ScreenSpace.ScreenToSeconds(MousePixel.X + HoverProximityThresholdPx);
	double MouseValue      = ScreenSpace.ScreenToValue(MousePixel.Y);
	float  PixelsPerOutput = ScreenSpace.PixelsPerOutput();

	FVector2D MinPos(MousePixel.X - HoverProximityThresholdPx, 0.0f);
	FVector2D MaxPos(MousePixel.X + HoverProximityThresholdPx, 0.0f);

	for (const TTuple<FCurveModelID, TUniquePtr<FCurveModel>>& Pair : CurveEditor->GetCurves())
	{
		double InputOffset = Pair.Value->GetInputDisplayOffset();
		double MinEvalTime = MinMouseTime - InputOffset;
		double MaxEvalTime = MaxMouseTime - InputOffset;

		double MinValue = 0.0, MaxValue = 0.0;
		if (Pair.Value->Evaluate(MinEvalTime, MinValue) && Pair.Value->Evaluate(MaxEvalTime, MaxValue))
		{
			MinPos.Y = ScreenSpace.ValueToScreen(MinValue);
			MaxPos.Y = ScreenSpace.ValueToScreen(MaxValue);

			float Distance = (FMath::ClosestPointOnSegment2D(MousePixel, MinPos, MaxPos) - MousePixel).Size();
			CurveProximities.Add(MakeTuple(Pair.Key, Distance));
		}
	}

	Algo::SortBy(CurveProximities, [](TTuple<FCurveModelID, float> In) { return In.Get<1>(); });

	if (CurveProximities.Num() > 0 && CurveProximities[0].Get<1>() < HoverProximityThresholdPx)
	{
		const FCurveModel* HoveredCurve = CurveEditor->FindCurve(CurveProximities[0].Get<0>());

		if (HoveredCurve)
		{
			double MouseTime = ScreenSpace.ScreenToSeconds(MousePixel.X) - HoveredCurve->GetInputDisplayOffset();
			double EvaluatedTime = CurveEditor->GetSnapMetrics().SnapInputSeconds(MouseTime);

			double EvaluatedValue = 0.0;
			HoveredCurve->Evaluate(EvaluatedTime, EvaluatedValue);

			FCachedToolTipData ToolTipData;
			ToolTipData.Text           = HoveredCurve->GetDisplayName();
			ToolTipData.EvaluatedTime  = FText::Format(NSLOCTEXT("CurveEditor", "CurveEditorTime", "{0}"), EvaluatedTime);
			ToolTipData.EvaluatedValue = FText::Format(NSLOCTEXT("CurveEditor", "CurveEditorValue", "{0}"), EvaluatedValue);

			CachedToolTipData = ToolTipData;
		}
	}
}

EVisibility SCurveEditorPanel::GetSplitterVisibility() const
{
	return EVisibility::Visible;
}

bool SCurveEditorPanel::IsToolTipEnabled() const
{
	return (CachedToolTipData.IsSet() && CurveEditor->GetSettings()->GetShowCurveEditorCurveToolTips());
}

FText SCurveEditorPanel::GetToolTipCurveName() const
{
	return CachedToolTipData.IsSet() ? CachedToolTipData->Text : FText();
}

FText SCurveEditorPanel::GetToolTipTimeText() const
{
	return CachedToolTipData.IsSet() ? CachedToolTipData->EvaluatedTime : FText();
}

FText SCurveEditorPanel::GetToolTipValueText() const
{
	return CachedToolTipData.IsSet() ? CachedToolTipData->EvaluatedValue : FText();
}

TOptional<FCurveModelID> SCurveEditorPanel::GetHoveredCurve() const
{
	if (CurveProximities.Num() > 0 && CurveProximities[0].Get<1>() < HoverProximityThresholdPx)
	{
		return CurveProximities[0].Get<0>();
	}

	return TOptional<FCurveModelID>();
}

void SCurveEditorPanel::SetKeyAttributes(FKeyAttributes KeyAttributes, FText Description)
{
	FScopedTransaction Transaction(Description);

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveEditor->Selection.GetAll())
	{
		if (FCurveModel* Curve = CurveEditor->FindCurve(Pair.Key))
		{
			Curve->Modify();
			Curve->SetKeyAttributes(Pair.Value.AsArray(), KeyAttributes);
		}
	}
}

void SCurveEditorPanel::SetCurveAttributes(FCurveAttributes CurveAttributes, FText Description)
{
	FScopedTransaction Transaction(Description);

	for (const TTuple<FCurveModelID, TUniquePtr<FCurveModel>>& Pair : CurveEditor->GetCurves())
	{
		Pair.Value->Modify();
		Pair.Value->SetCurveAttributes(CurveAttributes);
	}
}

void SCurveEditorPanel::ToggleWeightedTangents()
{
	FScopedTransaction Transaction(LOCTEXT("ToggleWeightedTangents_Transaction", "Toggle Weighted Tangents"));

	TMap<FCurveModelID, TArray<FKeyAttributes>> KeyAttributesPerCurve;

	const TMap<FCurveModelID, FKeyHandleSet>& Selection = CurveEditor->Selection.GetAll();

	// Disable weights unless we find something that doesn't have weights, then add them
	FKeyAttributes KeyAttributesToAssign = FKeyAttributes().SetTangentWeightMode(RCTWM_WeightedNone);

	// Gather current key attributes
	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection)
	{
		FCurveModel* Curve = CurveEditor->FindCurve(Pair.Key);
		if (Curve)
		{
			TArray<FKeyAttributes>& KeyAttributes = KeyAttributesPerCurve.Add(Pair.Key);
			KeyAttributes.SetNum(Pair.Value.Num());
			Curve->GetKeyAttributes(Pair.Value.AsArray(), KeyAttributes);

			// Check all the key attributes if they support tangent weights, but don't have any. If we find any such keys, we'll enable weights on all.
			if (KeyAttributesToAssign.GetTangentWeightMode() == RCTWM_WeightedNone)
			{
				for (const FKeyAttributes& Attributes : KeyAttributes)
				{
					if (Attributes.HasTangentWeightMode() && !(Attributes.HasArriveTangentWeight() || Attributes.HasLeaveTangentWeight()))
					{
						KeyAttributesToAssign.SetTangentWeightMode(RCTWM_WeightedBoth);
						break;
					}
				}
			}
		}
	}

	// Assign the new key attributes to all the selected curves
	for (TTuple<FCurveModelID, TArray<FKeyAttributes>>& Pair : KeyAttributesPerCurve)
	{
		FCurveModel* Curve = CurveEditor->FindCurve(Pair.Key);
		if (Curve)
		{
			for (FKeyAttributes& Attributes : Pair.Value)
			{
				Attributes = KeyAttributesToAssign;
			}

			TArrayView<const FKeyHandle> KeyHandles = Selection.FindChecked(Pair.Key).AsArray();
			Curve->Modify();
			Curve->SetKeyAttributes(KeyHandles, Pair.Value);
		}
	}
}

bool SCurveEditorPanel::CanToggleWeightedTangents() const
{
	return bSelectionSupportsWeightedTangents;
}

void SCurveEditorPanel::OnAddKey(FVector2D MousePixel)
{
	FScopedTransaction Transcation(LOCTEXT("OnAddKey", "Add Key"));

	bool bAddedKey = false;
	FKeyAttributes DefaultAttributes = CurveEditor->DefaultKeyAttributes.Get();

	double MouseTime = CurveEditor->GetScreenSpace().ScreenToSeconds(MousePixel.X);

	CurveEditor->Selection.Clear();
	for (const TTuple<FCurveModelID, TUniquePtr<FCurveModel>>& Pair : CurveEditor->GetCurves())
	{
		double EvalTime = MouseTime - Pair.Value->GetInputDisplayOffset();

		double CurveValue = 0.0;
		if (Pair.Value->Evaluate(EvalTime, CurveValue))
		{
			Pair.Value->Modify();

			// Add a key on this curve
			TOptional<FKeyHandle> NewKey = Pair.Value->AddKey(FKeyPosition(EvalTime, CurveValue), DefaultAttributes);
			if (NewKey.IsSet())
			{
				bAddedKey = true;
				CurveEditor->Selection.Add(FCurvePointHandle(Pair.Key, ECurvePointType::Key, NewKey.GetValue()));
			}
		}
	}

	if (!bAddedKey)
	{
		Transcation.Cancel();
	}
}

void SCurveEditorPanel::OnAddKeyHere(FVector2D MousePixel)
{
	FScopedTransaction Transcation(LOCTEXT("OnAddKeyHere", "Add Key"));

	bool bAddedKey = false;

	FKeyAttributes DefaultAttributes = CurveEditor->DefaultKeyAttributes.Get();

	double MouseTime = CurveEditor->GetScreenSpace().ScreenToSeconds(MousePixel.X);
	double NewValue  = CurveEditor->GetScreenSpace().ScreenToValue(MousePixel.Y);

	CurveEditor->Selection.Clear();
	for (const TTuple<FCurveModelID, TUniquePtr<FCurveModel>>& Pair : CurveEditor->GetCurves())
	{
		Pair.Value->Modify();

		double KeyTime = MouseTime - Pair.Value->GetInputDisplayOffset();
		TOptional<FKeyHandle> NewKey = Pair.Value->AddKey(FKeyPosition(KeyTime, NewValue), DefaultAttributes);
		if (NewKey.IsSet())
		{
			bAddedKey = true;
			CurveEditor->Selection.Add(FCurvePointHandle(Pair.Key, ECurvePointType::Key, NewKey.GetValue()));
		}
	}

	if (!bAddedKey)
	{
		Transcation.Cancel();
	}
}

void SCurveEditorPanel::OnAddKey(FCurveModelID CurveToAdd, FVector2D MousePixel)
{
	FCurveModel* Curve = CurveEditor->FindCurve(CurveToAdd);
	if (!Curve)
	{
		return;
	}

	FScopedTransaction Transcation(FText::Format(LOCTEXT("OnKeyToCurveFormat", "Add Key to Curve '{0}'"), Curve->GetDisplayName()));

	FKeyAttributes DefaultAttributes = CurveEditor->DefaultKeyAttributes.Get();

	double MouseTime = CurveEditor->GetScreenSpace().ScreenToSeconds(MousePixel.X);
	double NewValue  = CurveEditor->GetScreenSpace().ScreenToValue(MousePixel.Y);

	CurveEditor->Selection.Clear();

	Curve->Modify();

	double KeyTime = MouseTime - Curve->GetInputDisplayOffset();
	TOptional<FKeyHandle> NewKey = Curve->AddKey(FKeyPosition(KeyTime, NewValue), DefaultAttributes);
	if (NewKey.IsSet())
	{
		CurveEditor->Selection.Add(FCurvePointHandle(CurveToAdd, ECurvePointType::Key, NewKey.GetValue()));
	}
	else
	{
		Transcation.Cancel();
	}
}

TOptional<FCurvePointHandle> SCurveEditorPanel::HitPoint(FVector2D MousePixel) const
{
	TOptional<FCurvePointHandle> HitPoint;
	TOptional<float> ClosestDistance;

	// Find all keys within the current hittest time
	for (const FCurveDrawParams& Params : CachedDrawParams)
	{
		for (const FCurvePointInfo& Point : Params.Points)
		{
			const FKeyDrawInfo& PointDrawInfo = Params.GetKeyDrawInfo(Point.Type);

			FSlateRect KeyRect = FSlateRect::FromPointAndExtent(Point.ScreenPosition - PointDrawInfo.ScreenSize/2, PointDrawInfo.ScreenSize);
			if (KeyRect.ContainsPoint(MousePixel))
			{
				float DistanceSquared = (KeyRect.GetCenter() - MousePixel).SizeSquared();
				if (DistanceSquared <= ClosestDistance.Get(DistanceSquared))
				{
					ClosestDistance = DistanceSquared;
					HitPoint = FCurvePointHandle(Params.GetID(), Point.Type, Point.KeyHandle);
				}
			}
		}
	}

	return HitPoint;
}

FReply SCurveEditorPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	CachedToolTipData.Reset();

	FCurveEditorScreenSpace ScreenSpace = CurveEditor->GetScreenSpace();

	FVector2D MousePixel = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	// Mouse interaction that does not require a hit test
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		if (MouseEvent.IsAltDown())
		{
			DragOperation = FCurveEditorDelayedDrag(MousePixel, MouseEvent.GetEffectingButton());
			DragOperation->DragImpl = MakeUnique<FCurveEditorDragOperation_Pan>(CurveEditor.Get());
			return FReply::Handled().CaptureMouse(AsShared());
		}
		else
		{
			TOptional<FCurvePointHandle> NewPoint;

			// Add a key to the closest curve to the mouse

			if (TOptional<FCurveModelID> HoveredCurve = GetHoveredCurve())
			{
				FCurveModel* CurveToAddTo = CurveEditor->FindCurve(HoveredCurve.GetValue());
				if (CurveToAddTo)
				{
					FScopedTransaction Transcation(LOCTEXT("InsertKey", "Insert Key"));

					FKeyAttributes DefaultAttributes = CurveEditor->DefaultKeyAttributes.Get();

					double MouseTime  = ScreenSpace.ScreenToSeconds(MousePixel.X);
					double MouseValue = ScreenSpace.ScreenToValue(MousePixel.Y);

					CurveToAddTo->Modify();

					// Add a key on this curve
					TOptional<FKeyHandle> NewKey = CurveToAddTo->AddKey(FKeyPosition(MouseTime, MouseValue), DefaultAttributes);
					if (NewKey.IsSet())
					{
						NewPoint = FCurvePointHandle(HoveredCurve.GetValue(), ECurvePointType::Key, NewKey.GetValue());

						CurveEditor->Selection.Clear();
						CurveEditor->Selection.Add(NewPoint.GetValue());
					}
					else
					{
						Transcation.Cancel();
					}
				}
			}

			TUniquePtr<ICurveEditorKeyDragOperation> KeyDrag = CreateKeyDrag(CurveEditor->Selection.GetSelectionType());

			bool bAllowSnapping = NewPoint.IsSet();

			KeyDrag->Initialize(CurveEditor.Get(), NewPoint);
			KeyDrag->SnapMetrics.bSnapInputValues = bAllowSnapping;
			KeyDrag->SnapMetrics.bSnapOutputValues = bAllowSnapping;

			DragOperation = FCurveEditorDelayedDrag(MousePixel, MouseEvent.GetEffectingButton());
			DragOperation->DragImpl = MoveTemp(KeyDrag);

			return FReply::Handled().CaptureMouse(AsShared());
		}
	}
	else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (MouseEvent.IsAltDown())
		{
			DragOperation = FCurveEditorDelayedDrag(MousePixel, MouseEvent.GetEffectingButton());
			DragOperation->DragImpl = MakeUnique<FCurveEditorDragOperation_Zoom>(CurveEditor.Get());
			return FReply::Handled().CaptureMouse(AsShared());
		}
	}

	bool bShiftPressed = MouseEvent.IsShiftDown();
	bool bCtrlPressed  = MouseEvent.IsControlDown();

	TOptional<FCurvePointHandle> MouseDownPoint = HitPoint(MousePixel);
	if (MouseDownPoint.IsSet())
	{
		if (bShiftPressed)
		{
			CurveEditor->Selection.Add(MouseDownPoint.GetValue());
		}
		else if (bCtrlPressed)
		{
			CurveEditor->Selection.Toggle(MouseDownPoint.GetValue());
		}
		else
		{
			if (CurveEditor->Selection.Contains(MouseDownPoint->CurveID, MouseDownPoint->KeyHandle))
			{
				CurveEditor->Selection.ChangeSelectionPointType(MouseDownPoint->PointType);
			}
			else
			{
				CurveEditor->Selection.Clear();
				CurveEditor->Selection.Add(MouseDownPoint.GetValue());
			}
		}

		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			TUniquePtr<ICurveEditorKeyDragOperation> KeyDrag = CreateKeyDrag(MouseDownPoint->PointType);

			KeyDrag->Initialize(CurveEditor.Get(), MouseDownPoint);

			DragOperation = FCurveEditorDelayedDrag(MousePixel, MouseEvent.GetEffectingButton());
			DragOperation->DragImpl = MoveTemp(KeyDrag);

			return FReply::Handled().CaptureMouse(AsShared());
		}
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		DragOperation = FCurveEditorDelayedDrag(MousePixel, MouseEvent.GetEffectingButton());
		DragOperation->DragImpl = MakeUnique<FCurveEditorDragOperation_Marquee>(CurveEditor.Get(), this);
		return FReply::Handled().CaptureMouse(AsShared());
	}
	else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		DragOperation = FCurveEditorDelayedDrag(MousePixel, MouseEvent.GetEffectingButton());
		DragOperation->DragImpl = MakeUnique<FCurveEditorDragOperation_Pan>(CurveEditor.Get());

		return FReply::Handled().CaptureMouse(AsShared());
	}

	return FReply::Handled();
}

FReply SCurveEditorPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FVector2D MousePixel = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	if (DragOperation.IsSet())
	{
		FVector2D InitialPosition = DragOperation->GetInitialPosition();

		if (!DragOperation->IsDragging() && DragOperation->AttemptDragStart(MouseEvent))
		{
			DragOperation->DragImpl->BeginDrag(InitialPosition, MousePixel, MouseEvent);
		}
		else if (DragOperation->IsDragging())
		{
			DragOperation->DragImpl->Drag(InitialPosition, MousePixel, MouseEvent);
		}
	}
	else
	{
		UpdateCurveProximities(MousePixel);
	}

	return FReply::Handled();
}

FReply SCurveEditorPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FVector2D MousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	if (DragOperation.IsSet() && DragOperation->IsDragging())
	{
		FVector2D InitialPosition = DragOperation->GetInitialPosition();
		DragOperation->DragImpl->EndDrag(InitialPosition, MousePosition, MouseEvent);
	}
	else
	{
		TOptional<FCurvePointHandle> MouseUpPoint = HitPoint(MousePosition);

		RebindContextualActions(MousePosition);

		if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			if (!MouseUpPoint.IsSet())
			{
				CurveEditor->Selection.Clear();
			}

			const bool bCloseAfterSelection = true;
			FMenuBuilder MenuBuilder(bCloseAfterSelection, CommandList);

			FCurveEditorContextMenu::BuildMenu(MenuBuilder, CurveEditor, MouseUpPoint, GetHoveredCurve());

			// Push the context menu
			FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuBuilder.MakeWidget(), FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}
		else if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !MouseUpPoint.IsSet())
		{
			if (!CommandList->ProcessCommandBindings(MouseEvent))
			{
				CurveEditor->Selection.Clear();
			}
		}
	}

	UpdateCurveProximities(MousePosition);

	DragOperation.Reset();
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SCurveEditorPanel::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FCurveEditorScreenSpace ScreenSpace = CurveEditor->GetScreenSpace();

	FVector2D MousePixel   = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	double    CurrentTime  = ScreenSpace.ScreenToSeconds(MousePixel.X);
	double    CurrentValue = ScreenSpace.ScreenToValue(MousePixel.Y);

	float ZoomDelta = 1.f - FMath::Clamp(0.1f * MouseEvent.GetWheelDelta(), -0.9f, 0.9f);
	CurveEditor->ZoomAround(ZoomDelta, CurrentTime, CurrentValue);

	return FReply::Handled();
}

void SCurveEditorPanel::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	CurveProximities.Reset();

	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
}

void SCurveEditorPanel::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	CurveProximities.Reset();

	SCompoundWidget::OnMouseLeave(MouseEvent);
}

void SCurveEditorPanel::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	if (DragOperation.IsSet())
	{
		DragOperation->DragImpl->CancelDrag();
		DragOperation.Reset();
	}

	SCompoundWidget::OnFocusLost(InFocusEvent);
}

FReply SCurveEditorPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (DragOperation.IsSet())
		{
			DragOperation->DragImpl->CancelDrag();
			DragOperation.Reset();
		}
		else
		{
			CurveEditor->Selection.Clear();
		}
		return FReply::Handled();
	}
	else if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

int32 SCurveEditorPanel::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const ESlateDrawEffect DrawEffects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	FSlateDrawElement::MakeBox(OutDrawElements,	LayerId, AllottedGeometry.ToPaintGeometry(),
		FEditorStyle::GetBrush("ToolPanel.GroupBorder"), DrawEffects);

	LayerId = DrawGridLines(AllottedGeometry, OutDrawElements, LayerId, DrawEffects);
	LayerId = DrawCurves(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, DrawEffects);

	if (DragOperation.IsSet() && DragOperation->IsDragging())
	{
		LayerId = DragOperation->DragImpl->Paint(AllottedGeometry, OutDrawElements, LayerId);
	}
	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId+2000, InWidgetStyle, bParentEnabled)-2000;
}

int32 SCurveEditorPanel::DrawGridLines(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, ESlateDrawEffect DrawEffects) const
{
	// Rendering info
	const float          Width          = AllottedGeometry.GetLocalSize().X;
	const float          Height         = AllottedGeometry.GetLocalSize().Y;
	const FLinearColor   MajorGridColor = GridLineTintAttribute.Get();
	const FLinearColor   MinorGridColor = MajorGridColor.CopyWithNewOpacity(MajorGridColor.A * .25f);
	const FPaintGeometry PaintGeometry  = AllottedGeometry.ToPaintGeometry();
	const FLinearColor	 LabelColor = FLinearColor::White.CopyWithNewOpacity(0.65f);
	const FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont");

	TArray<float> MajorGridLines, MinorGridLines;
	TArray<FText> MajorGridLabels;
	CurveEditor->GetGridLinesX(MajorGridLines, MinorGridLines, MajorGridLabels);
	ensureMsgf(MajorGridLines.Num() == MajorGridLabels.Num(), TEXT("A grid label should be specified for every major grid line, even if it is just an empty FText."));


	TArray<FVector2D> LinePoints;
	LinePoints.Add(FVector2D(0.f, 0.f));
	LinePoints.Add(FVector2D(0.f, Height));

	// Draw major grid lines
	for(int32 i = 0; i < MajorGridLines.Num(); i++)
	{
		LinePoints[0].X = LinePoints[1].X = MajorGridLines[i];

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			PaintGeometry,
			LinePoints,
			DrawEffects,
			MajorGridColor,
			false
		);

		FPaintGeometry LabelGeometry = AllottedGeometry.ToPaintGeometry( FSlateLayoutTransform(FVector2D(LinePoints[0].X + LabelOffsetPx, LabelOffsetPx)) );

		// Draw the axis labels above the curves (which get drawn next) now so that we don't
		// have to cache off our labels and draw after. Done as +2 as the curves will draw on
		// +1 after this function returns.
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId+2,
			LabelGeometry,
			MajorGridLabels[i],
			FontInfo,
			DrawEffects,
			LabelColor
		);
	}

	for (float PosX : MinorGridLines)
	{
		LinePoints[0].X = LinePoints[1].X = PosX;

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			PaintGeometry,
			LinePoints,
			DrawEffects,
			MinorGridColor,
			false
		);
	}

	MajorGridLines.Reset();
	MinorGridLines.Reset();
	MajorGridLabels.Reset();
	CurveEditor->GetGridLinesY(MajorGridLines, MinorGridLines, MajorGridLabels);
	ensureMsgf(MajorGridLines.Num() == MajorGridLabels.Num(), TEXT("A grid label should be specified for every major grid line, even if it is just an empty FText."));

	LinePoints[0].X = 0.f;
	LinePoints[1].X = Width;
	for (int32 i = 0; i < MajorGridLines.Num(); i++)
	{
		LinePoints[0].Y = LinePoints[1].Y = MajorGridLines[i];

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			PaintGeometry,
			LinePoints,
			DrawEffects,
			MajorGridColor,
			false
		);

		FPaintGeometry LabelGeometry = AllottedGeometry.ToPaintGeometry(FSlateLayoutTransform(FVector2D(LabelOffsetPx, LinePoints[0].Y+ LabelOffsetPx)));

		// Draw the axis labels above the curves (which get drawn next) now so that we don't
		// have to cache off our labels and draw after. Done as +2 as the curves will draw on
		// +1 after this function returns.
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId+2,
			LabelGeometry,
			MajorGridLabels[i],
			FontInfo,
			DrawEffects,
			LabelColor
		);
	}

	for (float PosX : MinorGridLines)
	{
		LinePoints[0].X = LinePoints[1].X = PosX;

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			PaintGeometry,
			LinePoints,
			DrawEffects,
			MinorGridColor,
			false
		);
	}

	return LayerId + 1;
}

int32 SCurveEditorPanel::DrawCurves(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, ESlateDrawEffect DrawEffects) const
{
	FCurveEditorScreenSpace ScreenSpace = CurveEditor->GetScreenSpace();

	static const FName SelectionColorName("SelectionColor");
	FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SelectionColorName).GetColor(InWidgetStyle);

	const FVector2D      VisibleSize   = AllottedGeometry.GetLocalSize();
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();

	TOptional<FCurveModelID> HoveredCurve = GetHoveredCurve();
	for (const FCurveDrawParams& Params : CachedDrawParams)
	{
		float Thickness = ( HoveredCurve.IsSet() && HoveredCurve.GetValue() == Params.GetID() ) ? HoveredCurveThickness : UnHoveredCurveThickness;
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			PaintGeometry,
			Params.InterpolatingPoints,
			DrawEffects,
			Params.Color,
			bAntiAliasCurves,
			Thickness
		);

		TArray<FVector2D> LinePoints;
		LinePoints.SetNum(2);

		// Draw tangents
		for (const FCurvePointInfo& Point : Params.Points)
		{
			const FKeyDrawInfo& PointDrawInfo = Params.GetKeyDrawInfo(Point.Type);
			const bool          bSelected     = CurveEditor->Selection.IsSelected(FCurvePointHandle(Params.GetID(), Point.Type, Point.KeyHandle));
			const FLinearColor  PointTint     = bSelected ? SelectionColor : PointDrawInfo.Tint;

			if (Point.LineDelta.X != 0.f || Point.LineDelta.Y != 0.f)
			{
				LinePoints[0] = Point.ScreenPosition;
				LinePoints[1] = Point.ScreenPosition + Point.LineDelta;

				// Draw the connecting line
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PaintGeometry, LinePoints, DrawEffects, PointTint, true);
			}

			FPaintGeometry PointGeometry = AllottedGeometry.ToPaintGeometry(
				Point.ScreenPosition - (PointDrawInfo.ScreenSize * 0.5f),
				PointDrawInfo.ScreenSize
			);

			FSlateDrawElement::MakeBox(OutDrawElements, LayerId+Point.LayerBias, PointGeometry, PointDrawInfo.Brush, DrawEffects, PointTint);
		}
	}

	return LayerId + 1;
}


void SCurveEditorPanel::OnSimplifySelection()
{
	// Display dialog and let user enter tolerance.
	GenericTextEntryModeless(
		NSLOCTEXT("CurveEditor.Popups", "ReduceCurveTolerance", "Tolerance"),
		FText::AsNumber(ReduceTolerance),
		FOnTextCommitted::CreateSP(this, &SCurveEditorPanel::OnSimplifySelectionCommited)
	);
}

void SCurveEditorPanel::OnSimplifySelectionCommited(const FText& InText, ETextCommit::Type CommitInfo)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		double NewTolerance = FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if (!bIsNumber)
			return;
		CurveEditor->SimplifySelection((float)NewTolerance);
	}
}



void SCurveEditorPanel::GenericTextEntryModeless(const FText& DialogText, const FText& DefaultText, FOnTextCommitted OnTextComitted)
{
	TSharedRef<STextEntryPopup> TextEntryPopup =
		SNew(STextEntryPopup)
		.Label(DialogText)
		.DefaultText(DefaultText)
		.OnTextCommitted(OnTextComitted)
		.ClearKeyboardFocusOnCommit(false)
		.SelectAllTextWhenFocused(true)
		.MaxWidth(1024.0f);

	EntryPopupMenu = FSlateApplication::Get().PushMenu(
		SharedThis(this),
		FWidgetPath(),
		TextEntryPopup,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
	);
}


void SCurveEditorPanel::CloseEntryPopupMenu()
{
	if (EntryPopupMenu.IsValid())
	{
		EntryPopupMenu.Pin()->Dismiss();
	}
}

#undef LOCTEXT_NAMESPACE