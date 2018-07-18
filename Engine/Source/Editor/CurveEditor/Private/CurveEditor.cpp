// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CurveEditor.h"
#include "Layout/Geometry.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditorSnapMetrics.h"
#include "CurveEditorCommands.h"
#include "CurveEditorSettings.h"
#include "CurveDrawInfo.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Editor.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "CurveEditor"

FCurveModelID FCurveModelID::Unique()
{
	static uint32 CurrentID = 1;

	FCurveModelID ID;
	ID.ID = CurrentID++;
	return ID;
}

FCurveEditor::FCurveEditor()
	: Bounds(new FStaticCurveEditorBounds)
	, CachedPhysicalSize(0.f, 0.f)
{
	Settings = GetMutableDefault<UCurveEditorSettings>();
}

FCurveModel* FCurveEditor::FindCurve(FCurveModelID CurveID) const
{
	const TUniquePtr<FCurveModel>* Ptr = CurveData.Find(CurveID);
	return Ptr ? Ptr->Get() : nullptr;
}

const TMap<FCurveModelID, TUniquePtr<FCurveModel>>& FCurveEditor::GetCurves() const
{
	return CurveData;
}

FCurveModelID FCurveEditor::AddCurve(TUniquePtr<FCurveModel>&& InCurve)
{
	FCurveModelID NewID = FCurveModelID::Unique();
	CurveData.Add(NewID, MoveTemp(InCurve));
	return NewID;
}

void FCurveEditor::RemoveCurve(FCurveModelID InCurveID)
{
	CurveData.Remove(InCurveID);
	Selection.Remove(InCurveID);
}

void FCurveEditor::SetBounds(TUniquePtr<ICurveEditorBounds>&& InBounds)
{
	check(InBounds.IsValid());
	Bounds = MoveTemp(InBounds);
}

bool FCurveEditor::ShouldAutoFrame() const
{
	return Settings->GetAutoFrameCurveEditor();
}

void FCurveEditor::BindCommands()
{
	UCurveEditorSettings* CurveSettings = Settings;

	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction(FGenericCommands::Get().Undo,   FExecuteAction::CreateLambda([]{ GEditor->UndoTransaction(); }));
	CommandList->MapAction(FGenericCommands::Get().Redo,   FExecuteAction::CreateLambda([]{ GEditor->RedoTransaction(); }));
	CommandList->MapAction(FGenericCommands::Get().Delete, FExecuteAction::CreateSP(this, &FCurveEditor::DeleteSelection));

	CommandList->MapAction(FCurveEditorCommands::Get().ZoomToFitAll,        FExecuteAction::CreateSP(this, &FCurveEditor::ZoomToFit, EAxisList::All));
	CommandList->MapAction(FCurveEditorCommands::Get().ZoomToFit,           FExecuteAction::CreateSP(this, &FCurveEditor::ZoomToFitSelection, EAxisList::All));
	CommandList->MapAction(FCurveEditorCommands::Get().ZoomToFitHorizontal, FExecuteAction::CreateSP(this, &FCurveEditor::ZoomToFitSelection, EAxisList::X));
	CommandList->MapAction(FCurveEditorCommands::Get().ZoomToFitVertical,   FExecuteAction::CreateSP(this, &FCurveEditor::ZoomToFitSelection, EAxisList::Y));

	{
		FExecuteAction   ToggleInputSnapping     = FExecuteAction::CreateSP(this,   &FCurveEditor::ToggleInputSnapping);
		FIsActionChecked IsInputSnappingEnabled  = FIsActionChecked::CreateSP(this, &FCurveEditor::IsInputSnappingEnabled);
		FExecuteAction   ToggleOutputSnapping    = FExecuteAction::CreateSP(this,   &FCurveEditor::ToggleOutputSnapping);
		FIsActionChecked IsOutputSnappingEnabled = FIsActionChecked::CreateSP(this, &FCurveEditor::IsOutputSnappingEnabled);

		CommandList->MapAction(FCurveEditorCommands::Get().ToggleInputSnapping, ToggleInputSnapping, FCanExecuteAction(), IsInputSnappingEnabled);
		CommandList->MapAction(FCurveEditorCommands::Get().ToggleOutputSnapping, ToggleOutputSnapping, FCanExecuteAction(), IsOutputSnappingEnabled);
	}

	CommandList->MapAction(FCurveEditorCommands::Get().FlattenTangents,    FExecuteAction::CreateSP(this, &FCurveEditor::FlattenSelection));
	CommandList->MapAction(FCurveEditorCommands::Get().StraightenTangents, FExecuteAction::CreateSP(this, &FCurveEditor::StraightenSelection));
	CommandList->MapAction(FCurveEditorCommands::Get().BakeCurve,          FExecuteAction::CreateSP(this, &FCurveEditor::BakeSelection));
	CommandList->MapAction(FCurveEditorCommands::Get().ReduceCurve,        FExecuteAction::CreateSP(this, &FCurveEditor::SimplifySelection, 0.1f));

	// Tangent Visibility
	{
		FExecuteAction SetAllTangents          = FExecuteAction::CreateUObject(Settings, &UCurveEditorSettings::SetTangentVisibility, ECurveEditorTangentVisibility::AllTangents);
		FExecuteAction SetSelectedKeyTangents  = FExecuteAction::CreateUObject(Settings, &UCurveEditorSettings::SetTangentVisibility, ECurveEditorTangentVisibility::SelectedKeys);
		FExecuteAction SetNoTangents           = FExecuteAction::CreateUObject(Settings, &UCurveEditorSettings::SetTangentVisibility, ECurveEditorTangentVisibility::NoTangents);

		FIsActionChecked IsAllTangents         = FIsActionChecked::CreateLambda( [CurveSettings]{ return CurveSettings->GetTangentVisibility() == ECurveEditorTangentVisibility::AllTangents; } );
		FIsActionChecked IsSelectedKeyTangents = FIsActionChecked::CreateLambda( [CurveSettings]{ return CurveSettings->GetTangentVisibility() == ECurveEditorTangentVisibility::SelectedKeys; } );
		FIsActionChecked IsNoTangents          = FIsActionChecked::CreateLambda( [CurveSettings]{ return CurveSettings->GetTangentVisibility() == ECurveEditorTangentVisibility::NoTangents; } );

		CommandList->MapAction(FCurveEditorCommands::Get().SetAllTangentsVisibility, SetAllTangents, FCanExecuteAction(), IsAllTangents);
		CommandList->MapAction(FCurveEditorCommands::Get().SetSelectedKeysTangentVisibility, SetSelectedKeyTangents, FCanExecuteAction(), IsSelectedKeyTangents);
		CommandList->MapAction(FCurveEditorCommands::Get().SetNoTangentsVisibility, SetNoTangents, FCanExecuteAction(), IsNoTangents);
	}

	CommandList->MapAction(FCurveEditorCommands::Get().ToggleAutoFrameCurveEditor,
		FExecuteAction::CreateLambda( [CurveSettings]{ CurveSettings->SetAutoFrameCurveEditor( !CurveSettings->GetAutoFrameCurveEditor() ); } ),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda( [CurveSettings]{ return CurveSettings->GetAutoFrameCurveEditor(); } )
	);

	CommandList->MapAction(FCurveEditorCommands::Get().ToggleShowCurveEditorCurveToolTips,
		FExecuteAction::CreateLambda( [CurveSettings]{ CurveSettings->SetShowCurveEditorCurveToolTips( !CurveSettings->GetShowCurveEditorCurveToolTips() ); } ),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda( [CurveSettings]{ return CurveSettings->GetShowCurveEditorCurveToolTips(); } ) );
}

FCurveEditorScreenSpace FCurveEditor::GetScreenSpace() const
{
	// Retrieve the current bounds and cache them
	double InputMin = 0, InputMax = 1;
	Bounds->GetInputBounds(InputMin, InputMax);

	double OutputMin = 0, OutputMax = 1;
	Bounds->GetOutputBounds(OutputMin, OutputMax);

	return FCurveEditorScreenSpace(CachedPhysicalSize, InputMin, InputMax, OutputMin, OutputMax);
}

FCurveEditorSnapMetrics FCurveEditor::GetSnapMetrics() const
{
	FCurveEditorSnapMetrics Metrics;

	Metrics.bSnapOutputValues  = OutputSnapEnabledAttribute.Get();
	Metrics.OutputSnapInterval = OutputSnapIntervalAttribute.Get();
	Metrics.bSnapInputValues   = InputSnapEnabledAttribute.Get();
	Metrics.InputSnapRate      = InputSnapRateAttribute.Get();

	return Metrics;
}

void FCurveEditor::UpdateGeometry(const FGeometry& NewGeometry)
{
	if (CachedPhysicalSize.X != 0 && CachedPhysicalSize.Y != 0)
	{
		// Retrieve the current bounds and cache them
		double InputMin = 0, InputMax = 1;
		Bounds->GetInputBounds(InputMin, InputMax);

		double OutputMin = 0, OutputMax = 1;
		Bounds->GetOutputBounds(OutputMin, OutputMax);

		// Increase the visible input/output ranges based on the new size of the panel
		const float OldWidth = CachedPhysicalSize.X;
		if (NewGeometry.GetLocalSize().X != OldWidth)
		{
			const double PixelToInputRatio = (InputMax - InputMin) / OldWidth;
			InputMax += PixelToInputRatio * (NewGeometry.GetLocalSize().X - OldWidth);

			Bounds->SetInputBounds(InputMin, InputMax);
		}

		const float OldHeight = CachedPhysicalSize.Y;
		if (NewGeometry.GetLocalSize().Y != OldHeight)
		{
			const double PixelToOutputRatio = (OutputMax - OutputMin) / OldHeight;
			OutputMin -= PixelToOutputRatio * (NewGeometry.GetLocalSize().Y - OldHeight);

			Bounds->SetOutputBounds(OutputMin, OutputMax);
		}
	}

	CachedPhysicalSize = NewGeometry.GetLocalSize();
}

void FCurveEditor::Zoom(float Amount)
{
	FCurveEditorScreenSpace ScreenSpace = GetScreenSpace();

	const double InputOrigin  = (ScreenSpace.GetInputMax()  - ScreenSpace.GetInputMin())  * 0.5;
	const double OutputOrigin = (ScreenSpace.GetOutputMax() - ScreenSpace.GetOutputMin()) * 0.5;

	ZoomAround(Amount, InputOrigin, OutputOrigin);
}

void FCurveEditor::ZoomAround(float Amount, double InputOrigin, double OutputOrigin)
{
	FCurveEditorScreenSpace ScreenSpace = GetScreenSpace();

	double InputMin  = ScreenSpace.GetInputMin();
	double InputMax  = ScreenSpace.GetInputMax();
	double OutputMin = ScreenSpace.GetOutputMin();
	double OutputMax = ScreenSpace.GetOutputMax();

	InputMin = InputOrigin - (InputOrigin - InputMin) * Amount;
	InputMax = InputOrigin + (InputMax - InputOrigin) * Amount;

	OutputMin = OutputOrigin - (OutputOrigin - OutputMin) * Amount;
	OutputMax = OutputOrigin + (OutputMax - OutputOrigin) * Amount;

	Bounds->SetInputBounds(InputMin, InputMax);
	Bounds->SetOutputBounds(OutputMin, OutputMax);
}

void FCurveEditor::ZoomToFit(EAxisList::Type Axes)
{
	TArray<FCurveModelID> CurveModelIDs;
	for (const TTuple<FCurveModelID, TUniquePtr<FCurveModel>>& Pair : CurveData)
	{
		CurveModelIDs.Add(Pair.Key);
	}

	ZoomToFitCurves(MakeArrayView(CurveModelIDs.GetData(), CurveModelIDs.Num()), Axes);
}

void FCurveEditor::ZoomToFitCurves(TArrayView<const FCurveModelID> CurveModelIDs, EAxisList::Type Axes)
{
	if (CurveData.Num() && CurveModelIDs.Num())
	{
		TArrayView<const FKeyHandle> Empty;

		double InputMin = TNumericLimits<double>::Max(), InputMax = TNumericLimits<double>::Lowest(), OutputMin = TNumericLimits<double>::Max(), OutputMax = TNumericLimits<double>::Lowest();

		TArray<FKeyHandle> KeyHandlesScratch;
		TArray<FKeyPosition> KeyPositionsScratch;

		for (const FCurveModelID CurveModelID : CurveModelIDs)
		{
			if (const FCurveModel* Curve = FindCurve(CurveModelID))
			{
				double LocalMin, LocalMax;
				Curve->GetTimeRange(LocalMin, LocalMax);
				InputMin = FMath::Min(InputMin, LocalMin);
				InputMax = FMath::Max(InputMax, LocalMax);
				Curve->GetValueRange(LocalMin, LocalMax);
				OutputMin = FMath::Min(OutputMin, LocalMin);
				OutputMax = FMath::Max(OutputMax, LocalMax);
			}
		}

		if (InputMin != TNumericLimits<double>::Max() && InputMax != TNumericLimits<double>::Lowest() && 
			OutputMin != TNumericLimits<double>::Max() && OutputMax != TNumericLimits<double>::Lowest())
		{
			ZoomToFitInternal(Axes, InputMin, InputMax, OutputMin, OutputMax);
		}
	}
}

void FCurveEditor::ZoomToFitSelection(EAxisList::Type Axes)
{
	if (Selection.Count() <= 1)
	{
		ZoomToFit(Axes);
	}
	else
	{
		TArray<FKeyPosition> KeyPositionsScratch;

		double InputMin = TNumericLimits<double>::Max(), InputMax = TNumericLimits<double>::Lowest(), OutputMin = TNumericLimits<double>::Max(), OutputMax = TNumericLimits<double>::Lowest();

		for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
		{
			if (const FCurveModel* Curve = FindCurve(Pair.Key))
			{
				KeyPositionsScratch.SetNum(Pair.Value.AsArray().Num());

				Curve->GetKeyPositions(Pair.Value.AsArray(), KeyPositionsScratch);
				for (FKeyPosition Key : KeyPositionsScratch)
				{
					InputMin  = FMath::Min(InputMin, Key.InputValue);
					InputMax  = FMath::Max(InputMax, Key.InputValue);
					OutputMin = FMath::Min(OutputMin, Key.OutputValue);
					OutputMax = FMath::Max(OutputMax, Key.OutputValue);
				}
			}
		}

		if (InputMin != TNumericLimits<double>::Max() && InputMax != TNumericLimits<double>::Lowest() && 
			OutputMin != TNumericLimits<double>::Max() && OutputMax != TNumericLimits<double>::Lowest())
		{
			ZoomToFitInternal(Axes, InputMin, InputMax, OutputMin, OutputMax);
		}
	}
}

void FCurveEditor::ZoomToFitInternal(EAxisList::Type Axes, double InputMin, double InputMax, double OutputMin, double OutputMax)
{
	FCurveEditorSnapMetrics SnapMetrics = GetSnapMetrics();
	double MinInputZoom = SnapMetrics.bSnapInputValues ? SnapMetrics.InputSnapRate.AsInterval() : 0.00001;
	double MinOutputZoom = SnapMetrics.bSnapOutputValues ? SnapMetrics.OutputSnapInterval : 0.00001;

	InputMax = FMath::Max(InputMin + MinInputZoom, InputMax);
	OutputMax = FMath::Max(OutputMin + MinOutputZoom, OutputMax);

	double InputPadding  = (InputMax - InputMin) * 0.1;
	double OutputPadding = (OutputMax - OutputMin) * 0.05;

	InputMin -= InputPadding;
	InputMax += InputPadding;
	if (Axes & EAxisList::X)
	{
		Bounds->SetInputBounds(InputMin - InputPadding, InputMax + InputPadding);
	}

	OutputMin -= OutputPadding;
	OutputMax += OutputPadding;
	if (Axes & EAxisList::Y)
	{
		Bounds->SetOutputBounds(OutputMin - OutputPadding, OutputMax + OutputPadding);
	}
}

bool FCurveEditor::IsInputSnappingEnabled() const
{
	return InputSnapEnabledAttribute.Get();
}

void FCurveEditor::ToggleInputSnapping()
{
	bool NewValue = !InputSnapEnabledAttribute.Get();

	if (!InputSnapEnabledAttribute.IsBound())
	{
		InputSnapEnabledAttribute = NewValue;
	}
	else
	{
		OnInputSnapEnabledChanged.ExecuteIfBound(NewValue);
	}
}

bool FCurveEditor::IsOutputSnappingEnabled() const
{
	return OutputSnapEnabledAttribute.Get();
}

void FCurveEditor::ToggleOutputSnapping()
{
	bool NewValue = !OutputSnapEnabledAttribute.Get();

	if (!OutputSnapEnabledAttribute.IsBound())
	{
		OutputSnapEnabledAttribute = NewValue;
	}
	else
	{
		OnOutputSnapEnabledChanged.ExecuteIfBound(NewValue);
	}
}

FVector2D FCurveEditor::GetVectorFromSlopeAndLength(float Slope, float Length)
{
	float x = Length / FMath::Sqrt(Slope*Slope + 1.f);
	float y = Slope * x;
	return FVector2D(x, y);
}

FVector2D  FCurveEditor::GetTangentPositionInScreenSpace(const FVector2D &StartPos, float Tangent, float Weight) const
{
	FCurveEditorScreenSpace ScreenSpace = GetScreenSpace();
	const float Angle = FMath::Atan(-Tangent);
	float X, Y;
	FMath::SinCos(&Y, &X, Angle);
	X *= Weight;
	Y *= Weight;

	X *= ScreenSpace.PixelsPerInput();
	Y *= ScreenSpace.PixelsPerOutput();
	return FVector2D(StartPos.X + X, StartPos.Y +Y);
}

void FCurveEditor::GetTangentAndWeightFromScreenPosition(const FVector2D &StartPos, const  FVector2D &TangentPos, float &Tangent, float &Weight) const
{
	FCurveEditorScreenSpace ScreenSpace = GetScreenSpace();
	float X = ScreenSpace.ScreenToSeconds(TangentPos.X) - ScreenSpace.ScreenToSeconds(StartPos.X);
	float Y = ScreenSpace.ScreenToValue(TangentPos.Y) - ScreenSpace.ScreenToValue(StartPos.Y);

	Tangent = Y / X;
	Weight = FMath::Sqrt(X*X + Y * Y);
}

void FCurveEditor::GetCurveDrawParams(TArray<FCurveDrawParams>& OutDrawParams) const
{
	FCurveEditorScreenSpace ScreenSpace = GetScreenSpace();
	const float DisplayRatio = (ScreenSpace.PixelsPerOutput() / ScreenSpace.PixelsPerInput());

	double InputMin = 0, InputMax = 1;
	Bounds->GetInputBounds(InputMin, InputMax);

	double OutputMin = 0, OutputMax = 1;
	Bounds->GetOutputBounds(OutputMin, OutputMax);

	ECurveEditorTangentVisibility::Type TangentVisibility = Settings->GetTangentVisibility();

	OutDrawParams.Reserve(CurveData.Num());
	for (const TTuple<FCurveModelID, TUniquePtr<FCurveModel>>& Pair : CurveData)
	{
		const FKeyHandleSet* SelectedKeys = Selection.GetAll().Find(Pair.Key);

		FCurveDrawParams Params(Pair.Key);
		Params.Color = Pair.Value->GetColor();

		// Gather the display metrics to use for each key type
		Pair.Value->GetKeyDrawInfo(ECurvePointType::Key, Params.KeyDrawInfo);
		Pair.Value->GetKeyDrawInfo(ECurvePointType::ArriveTangent, Params.ArriveTangentDrawInfo);
		Pair.Value->GetKeyDrawInfo(ECurvePointType::LeaveTangent, Params.LeaveTangentDrawInfo);

		// Gather the interpolating points in input/output space
		TArray<TTuple<double, double>> InterpolatingPoints;

		Pair.Value->DrawCurve(*this, InterpolatingPoints);
		Params.InterpolatingPoints.Reserve(InterpolatingPoints.Num());

		double InputOffset = Pair.Value->GetInputDisplayOffset();

		// Convert the interpolating points to screen space
		for (TTuple<double, double> Point : InterpolatingPoints)
		{
			Params.InterpolatingPoints.Add(
				FVector2D(
					ScreenSpace.SecondsToScreen(Point.Get<0>() + InputOffset),
					ScreenSpace.ValueToScreen(Point.Get<1>())
				)
			);
		}

		TArray<FKeyHandle> VisibleKeys;
		Pair.Value->GetKeys(*this, InputMin, InputMax, OutputMin, OutputMax, VisibleKeys);

		if (VisibleKeys.Num())
		{
			TArray<FKeyPosition> AllKeyPositions;
			TArray<FKeyAttributes> AllKeyAttributes;

			AllKeyPositions.SetNum(VisibleKeys.Num());
			AllKeyAttributes.SetNum(VisibleKeys.Num());

			Pair.Value->GetKeyPositions(VisibleKeys, AllKeyPositions);
			Pair.Value->GetKeyAttributes(VisibleKeys, AllKeyAttributes);

			for (int32 Index = 0; Index < VisibleKeys.Num(); ++Index)
			{
				const FKeyHandle      KeyHandle   = VisibleKeys[Index];
				const FKeyPosition&   KeyPosition = AllKeyPositions[Index];
				const FKeyAttributes& Attributes  = AllKeyAttributes[Index];

				bool bShowTangents = TangentVisibility == ECurveEditorTangentVisibility::AllTangents || ( TangentVisibility == ECurveEditorTangentVisibility::SelectedKeys && SelectedKeys && SelectedKeys->Contains(VisibleKeys[Index]) );

				// Add this key
				FCurvePointInfo Key(KeyHandle);
				Key.ScreenPosition = FVector2D(ScreenSpace.SecondsToScreen(KeyPosition.InputValue + InputOffset), ScreenSpace.ValueToScreen(KeyPosition.OutputValue));
				Key.LayerBias = 2;

				Params.Points.Add(Key);

				if (bShowTangents && Attributes.HasArriveTangent())
				{
					float ArriveTangent = Attributes.GetArriveTangent();

					FCurvePointInfo ArriveTangentPoint(KeyHandle);
					ArriveTangentPoint.Type = ECurvePointType::ArriveTangent;


					if (Attributes.HasTangentWeightMode() && Attributes.HasArriveTangentWeight() &&
						(Attributes.GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes.GetTangentWeightMode() == RCTWM_WeightedArrive))
					{
						ArriveTangentPoint.ScreenPosition = GetTangentPositionInScreenSpace(Key.ScreenPosition, ArriveTangent, -Attributes.GetArriveTangentWeight());
					}
					else
					{
						float PixelLength = 60.0f; 
						ArriveTangentPoint.ScreenPosition = Key.ScreenPosition + FCurveEditor::GetVectorFromSlopeAndLength(ArriveTangent * -DisplayRatio, -PixelLength);
					}
					ArriveTangentPoint.LineDelta = Key.ScreenPosition - ArriveTangentPoint.ScreenPosition;
					ArriveTangentPoint.LayerBias = 1;

					Params.Points.Add(ArriveTangentPoint);
				}

				if (bShowTangents && Attributes.HasLeaveTangent())
				{
					float LeaveTangent = Attributes.GetLeaveTangent();

					FCurvePointInfo LeaveTangentPoint(KeyHandle);
					LeaveTangentPoint.Type = ECurvePointType::LeaveTangent;

					if (Attributes.HasTangentWeightMode() && Attributes.HasLeaveTangentWeight() &&
						(Attributes.GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes.GetTangentWeightMode() == RCTWM_WeightedLeave))
					{
						LeaveTangentPoint.ScreenPosition = GetTangentPositionInScreenSpace(Key.ScreenPosition, LeaveTangent, Attributes.GetLeaveTangentWeight());
					}
					else
					{
						float PixelLength = 60.0f; 
						LeaveTangentPoint.ScreenPosition = Key.ScreenPosition + FCurveEditor::GetVectorFromSlopeAndLength(LeaveTangent * -DisplayRatio, PixelLength);

					}

					LeaveTangentPoint.LineDelta = Key.ScreenPosition - LeaveTangentPoint.ScreenPosition;
					LeaveTangentPoint.LayerBias = 1;

					Params.Points.Add(LeaveTangentPoint);
				}
			}
		}

		OutDrawParams.Add(MoveTemp(Params));
	}
}

void FCurveEditor::ConstructXGridLines(TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>& MajorGridLabels) const
{
	FCurveEditorScreenSpace ScreenSpace = GetScreenSpace();

	double MajorGridStep  = 0.0;
	int32  MinorDivisions = 0;
	if (GetSnapMetrics().InputSnapRate.ComputeGridSpacing(ScreenSpace.PixelsPerInput(), MajorGridStep, MinorDivisions))
	{
		const double FirstMajorLine = FMath::FloorToDouble(ScreenSpace.GetInputMin() / MajorGridStep) * MajorGridStep;
		const double LastMajorLine  = FMath::CeilToDouble(ScreenSpace.GetInputMax() / MajorGridStep) * MajorGridStep;

		for (double CurrentMajorLine = FirstMajorLine; CurrentMajorLine < LastMajorLine; CurrentMajorLine += MajorGridStep)
		{
			MajorGridLines.Add( ScreenSpace.SecondsToScreen(CurrentMajorLine) );
			MajorGridLabels.Add( FText::Format(LOCTEXT("GridXLabelFormat", "{0}s"), FText::AsNumber(CurrentMajorLine)) );

			for (int32 Step = 1; Step < MinorDivisions; ++Step)
			{
				MinorGridLines.Add( ScreenSpace.SecondsToScreen(CurrentMajorLine + Step*MajorGridStep/MinorDivisions) );
			}
		}
	}
}

void FCurveEditor::ConstructYGridLines(TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>&MajorGridLabels, uint8 MinorDivisions) const
{
	FCurveEditorSnapMetrics SnapMetrics = GetSnapMetrics();
	FCurveEditorScreenSpace ScreenSpace = GetScreenSpace();

	if (ScreenSpace.GetOutputMin() == ScreenSpace.GetOutputMax() || ScreenSpace.PixelsPerOutput() <= 0)
	{
		return;
	}

	const float GridPixelSpacing = FMath::Min(ScreenSpace.GetPhysicalHeight()/1.5f, 150.0f);

	static float Base = 10;
	float MaxTimeStep = GridPixelSpacing / ScreenSpace.PixelsPerOutput();
	float MajorGridStep = FMath::Pow(Base, FMath::FloorToFloat(FMath::LogX(Base, MaxTimeStep)));

	const double FirstMajorLine = FMath::FloorToDouble(ScreenSpace.GetOutputMin() / MajorGridStep) * MajorGridStep;
	const double LastMajorLine  = FMath::CeilToDouble(ScreenSpace.GetOutputMax() / MajorGridStep) * MajorGridStep;

	for (double CurrentMajorLine = FirstMajorLine; CurrentMajorLine < LastMajorLine; CurrentMajorLine += MajorGridStep)
	{
		MajorGridLines.Add( ScreenSpace.ValueToScreen(CurrentMajorLine) );
		MajorGridLabels.Add(FText::Format(LOCTEXT("GridYLabelFormat", "{0}"), FText::AsNumber(CurrentMajorLine)));

		for (int32 Step = 1; Step < MinorDivisions; ++Step)
		{
			MinorGridLines.Add( ScreenSpace.ValueToScreen(CurrentMajorLine + Step*MajorGridStep/MinorDivisions) );
		}
	}
}

void FCurveEditor::DeleteSelection()
{
	FScopedTransaction Transaction(LOCTEXT("DeleteKeys", "Delete Keys"));

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
	{
		if (FCurveModel* Curve = FindCurve(Pair.Key))
		{
			Curve->Modify();
			Curve->RemoveKeys(Pair.Value.AsArray());
		}
	}

	Selection.Clear();
}

void FCurveEditor::FlattenSelection()
{
	FScopedTransaction Transaction(LOCTEXT("FlattenTangents", "Flatten Tangents"));
	bool bFoundAnyTangents = false;

	TArray<FKeyHandle> KeyHandles;
	TArray<FKeyAttributes> AllKeyPositions;

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
	{
		if (FCurveModel* Curve = FindCurve(Pair.Key))
		{
			KeyHandles.Reset(Pair.Value.Num());
			KeyHandles.Append(Pair.Value.AsArray().GetData(), Pair.Value.Num());

			AllKeyPositions.SetNum(KeyHandles.Num());
			Curve->GetKeyAttributes(KeyHandles, AllKeyPositions);

			// Straighten tangents, ignoring any keys that we can't set tangents on
			for (int32 Index = AllKeyPositions.Num()-1 ; Index >= 0; --Index)
			{
				FKeyAttributes& Attributes = AllKeyPositions[Index];
				if (Attributes.HasTangentMode() && (Attributes.HasArriveTangent() || Attributes.HasLeaveTangent()))
				{
					Attributes.SetArriveTangent(0.f).SetLeaveTangent(0.f);
					if (Attributes.GetTangentMode() == RCTM_Auto)
					{
						Attributes.SetTangentMode(RCTM_User);
					}
				}
				else
				{
					AllKeyPositions.RemoveAtSwap(Index, 1, false);
					KeyHandles.RemoveAtSwap(Index, 1, false);
				}
			}

			if (AllKeyPositions.Num() > 0)
			{
				Curve->Modify();
				Curve->SetKeyAttributes(KeyHandles, AllKeyPositions);
				bFoundAnyTangents = true;
			}
		}
	}

	if (!bFoundAnyTangents)
	{
		Transaction.Cancel();
	}
}

void FCurveEditor::StraightenSelection()
{
	FScopedTransaction Transaction(LOCTEXT("StraightenTangents", "Straighten Tangents"));
	bool bFoundAnyTangents = false;

	TArray<FKeyHandle> KeyHandles;
	TArray<FKeyAttributes> AllKeyPositions;

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
	{
		if (FCurveModel* Curve = FindCurve(Pair.Key))
		{
			KeyHandles.Reset(Pair.Value.Num());
			KeyHandles.Append(Pair.Value.AsArray().GetData(), Pair.Value.Num());

			AllKeyPositions.SetNum(KeyHandles.Num());
			Curve->GetKeyAttributes(KeyHandles, AllKeyPositions);

			// Straighten tangents, ignoring any keys that we can't set tangents on
			for (int32 Index = AllKeyPositions.Num()-1 ; Index >= 0; --Index)
			{
				FKeyAttributes& Attributes = AllKeyPositions[Index];
				if (Attributes.HasTangentMode() && Attributes.HasArriveTangent() && Attributes.HasLeaveTangent())
				{
					float NewTangent = (Attributes.GetLeaveTangent() + Attributes.GetArriveTangent()) * 0.5f;
					Attributes.SetArriveTangent(NewTangent).SetLeaveTangent(NewTangent);
					if (Attributes.GetTangentMode() == RCTM_Auto)
					{
						Attributes.SetTangentMode(RCTM_User);
					}
				}
				else
				{
					AllKeyPositions.RemoveAtSwap(Index, 1, false);
					KeyHandles.RemoveAtSwap(Index, 1, false);
				}
			}

			if (AllKeyPositions.Num() > 0)
			{
				Curve->Modify();
				Curve->SetKeyAttributes(KeyHandles, AllKeyPositions);
				bFoundAnyTangents = true;
			}
		}
	}

	if (!bFoundAnyTangents)
	{
		Transaction.Cancel();
	}
}

void FCurveEditor::BakeSelection()
{
	FText TransactionText = FText::Format(LOCTEXT("BakeCurves", "Bake {0}|plural(one=Curve, other=Curves)"), Selection.GetAll().Num());
	FScopedTransaction Transaction(TransactionText);

	TArray<FKeyHandle> KeyHandles;
	TArray<FKeyPosition> SelectedKeyPositions;

	TArray<FKeyPosition> NewKeyPositions;
	TArray<FKeyAttributes> NewKeyAttributes;

	FFrameRate BakeRate = GetSnapMetrics().InputSnapRate;

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
	{
		if (FCurveModel* Curve = FindCurve(Pair.Key))
		{
			KeyHandles.Reset(Pair.Value.Num());
			KeyHandles.Append(Pair.Value.AsArray().GetData(), Pair.Value.Num());

			// Get all the selected keys
			SelectedKeyPositions.SetNum(KeyHandles.Num());
			Curve->GetKeyPositions(KeyHandles, SelectedKeyPositions);

			// Find the hull of the range of the selected keys
			double MinKey = TNumericLimits<double>::Max(), MaxKey = TNumericLimits<double>::Lowest();
			for (FKeyPosition Key : SelectedKeyPositions)
			{
				MinKey = FMath::Min(Key.InputValue, MinKey);
				MaxKey = FMath::Max(Key.InputValue, MaxKey);
			}

			// Get all keys that exist between the time range
			KeyHandles.Reset();
			Curve->GetKeys(*this, MinKey, MaxKey, -MAX_dbl, MAX_dbl, KeyHandles);

			if (KeyHandles.Num() > 1)
			{
				// Determine new times for new keys
				double Interval = BakeRate.AsInterval();
				int32 NumKeysToAdd = FMath::FloorToInt((MaxKey - MinKey) / Interval);

				NewKeyPositions.Reset(NumKeysToAdd);
				NewKeyAttributes.Reset(NumKeysToAdd);

				for (int32 KeyIndex = 0; KeyIndex < NumKeysToAdd; ++KeyIndex)
				{
					FKeyPosition CurrentKey(MinKey + KeyIndex*Interval, 0.0);
					if (Curve->Evaluate(CurrentKey.InputValue, CurrentKey.OutputValue))
					{
						NewKeyPositions.Add(CurrentKey);
						NewKeyAttributes.Add(FKeyAttributes().SetInterpMode(RCIM_Linear));
					}
				}

				Curve->Modify();

				// Remove all the old keys and add the new ones
				Curve->RemoveKeys(KeyHandles);
				Curve->AddKeys(NewKeyPositions, NewKeyAttributes);
			}
		}
	}
}

/**
  The following key reduction is the same as that found in FRichCurve.
  It would be nice if there was just one implementation of the reduction (and) baking algorithms.
*/
/** Util to find float value on bezier defined by 4 control points */
static float BezierInterp(float P0, float P1, float P2, float P3, float Alpha)
{
	const float P01 = FMath::Lerp(P0, P1, Alpha);
	const float P12 = FMath::Lerp(P1, P2, Alpha);
	const float P23 = FMath::Lerp(P2, P3, Alpha);
	const float P012 = FMath::Lerp(P01, P12, Alpha);
	const float P123 = FMath::Lerp(P12, P23, Alpha);
	const float P0123 = FMath::Lerp(P012, P123, Alpha);

	return P0123;
}

static float EvalForTwoKeys (const FKeyPosition& Key1Pos, const FKeyAttributes& Key1Attrib,
							  const FKeyPosition& Key2Pos, const FKeyAttributes& Key2Attrib,
							  const float InTime)
{
	const float Diff = Key2Pos.InputValue - Key1Pos.InputValue;

	if (Diff > 0.f && Key1Attrib.GetInterpMode() != RCIM_Constant)
	{
		const float Alpha = (InTime - Key1Pos.InputValue) / Diff;
		const float P0 = Key1Pos.OutputValue;
		const float P3 = Key2Pos.OutputValue;

		if (Key1Attrib.GetInterpMode() == RCIM_Linear)
		{
			return FMath::Lerp(P0, P3, Alpha);
		}
		else
		{
			const float OneThird = 1.0f / 3.0f;
			const float P1 = P0 + (Key1Attrib.GetLeaveTangent() * Diff*OneThird);
			const float P2 = P3 - (Key2Attrib.GetArriveTangent() * Diff*OneThird);

			return BezierInterp(P0, P1, P2, P3, Alpha);
		}
	}
	else
	{
		return Key1Pos.OutputValue;
	}
}


void FCurveEditor::SimplifySelection(float Tolerance)
{
	FText TransactionText = FText::Format(LOCTEXT("SimplifyCurves", "Simplify {0}|plural(one=Curve, other=Curves)"), Selection.GetAll().Num());
	FScopedTransaction Transaction(TransactionText);
	TArray<FKeyHandle> KeyHandles;
	TArray<FKeyPosition> SelectedKeyPositions;
	TArray<FKeyAttributes> SelectedKeyAttributes;

	FFrameRate BakeRate = GetSnapMetrics().InputSnapRate;

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : Selection.GetAll())
	{
		if (FCurveModel* Curve = FindCurve(Pair.Key))
		{
			KeyHandles.Reset(Pair.Value.Num());
			KeyHandles.Append(Pair.Value.AsArray().GetData(), Pair.Value.Num());

			// Get all the selected keys
			SelectedKeyPositions.SetNum(KeyHandles.Num());
			Curve->GetKeyPositions(KeyHandles, SelectedKeyPositions);

			// Find the hull of the range of the selected keys
			double MinKey = TNumericLimits<double>::Max(), MaxKey = TNumericLimits<double>::Lowest();
			for (FKeyPosition Key : SelectedKeyPositions)
			{
				MinKey = FMath::Min(Key.InputValue, MinKey);
				MaxKey = FMath::Max(Key.InputValue, MaxKey);
			}

			// Get all keys that exist between the time range
			KeyHandles.Reset();
			Curve->GetKeys(*this, MinKey, MaxKey, -MAX_dbl, MAX_dbl, KeyHandles);
			if (KeyHandles.Num() > 2) //need at least 3 keys to reduce
			{
				SelectedKeyPositions.SetNum(KeyHandles.Num());
				Curve->GetKeyPositions(KeyHandles, SelectedKeyPositions);
				SelectedKeyAttributes.SetNum(KeyHandles.Num());
				Curve->GetKeyAttributes(KeyHandles, SelectedKeyAttributes);
			
				int32 MostRecentKeepKeyIndex = 0;
				TArray<FKeyHandle> KeysToRemove;
				for (int32 TestIndex = 1; TestIndex < KeyHandles.Num()-1; ++TestIndex)
				{
					
					const float KeyValue = SelectedKeyPositions[TestIndex].OutputValue;
					const float ValueWithoutKey = EvalForTwoKeys(SelectedKeyPositions[MostRecentKeepKeyIndex], SelectedKeyAttributes[MostRecentKeepKeyIndex],
						SelectedKeyPositions[TestIndex + 1], SelectedKeyAttributes[TestIndex + 1],
						SelectedKeyPositions[TestIndex].InputValue);
					if (FMath::Abs(ValueWithoutKey - KeyValue) > Tolerance) // Is this key needed
					{
						MostRecentKeepKeyIndex = TestIndex;
					}
					else
					{
						KeysToRemove.Add(KeyHandles[TestIndex]);
					}
					}
				Curve->Modify();
				Curve->RemoveKeys(KeysToRemove);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
