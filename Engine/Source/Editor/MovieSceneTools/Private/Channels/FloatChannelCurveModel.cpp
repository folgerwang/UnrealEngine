// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "FloatChannelCurveModel.h"
#include "Math/Vector2D.h"
#include "HAL/PlatformMath.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneSection.h"
#include "MovieScene.h"
#include "CurveDrawInfo.h"
#include "CurveDataAbstraction.h"
#include "CurveEditor.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditorSnapMetrics.h"
#include "EditorStyleSet.h"
#include "BuiltInChannelEditors.h"
#include "SKeyEditInterface.h"
#include "SequencerChannelTraits.h"
#include "ISequencer.h"

FFloatChannelCurveModel::FFloatChannelCurveModel(TMovieSceneChannelHandle<FMovieSceneFloatChannel> InChannel, UMovieSceneSection* OwningSection, TWeakPtr<ISequencer> InWeakSequencer)
{
	ChannelHandle = InChannel;
	WeakSection = OwningSection;
	WeakSequencer = InWeakSequencer;
}

const void* FFloatChannelCurveModel::GetCurve() const
{
	return ChannelHandle.Get();
}

void FFloatChannelCurveModel::Modify()
{
	if (UMovieSceneSection* Section = WeakSection.Get())
	{
		Section->Modify();
	}
}

double FFloatChannelCurveModel::GetInputDisplayOffset() const
{
	UMovieSceneSection* Section = WeakSection.Get();
	return Section ? ( FFrameTime(0, 0.5f) / Section->GetTypedOuter<UMovieScene>()->GetFrameResolution() ) : 0.0;
}

void FFloatChannelCurveModel::AddKeys(TArrayView<const FKeyPosition> InKeyPositions, TArrayView<const FKeyAttributes> InKeyAttributes, TArrayView<TOptional<FKeyHandle>>* OutKeyHandles)
{
	check(InKeyPositions.Num() == InKeyAttributes.Num() && (!OutKeyHandles || OutKeyHandles->Num() == InKeyPositions.Num()));

	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();
	if (Channel && Section)
	{
		Section->Modify();

		TMovieSceneChannel<FMovieSceneFloatValue> ChannelInterface = Channel->GetInterface();
		FFrameRate FrameResolution = Section->GetTypedOuter<UMovieScene>()->GetFrameResolution();

		for (int32 Index = 0; Index < InKeyPositions.Num(); ++Index)
		{
			FKeyPosition   Position   = InKeyPositions[Index];
			FKeyAttributes Attributes = InKeyAttributes[Index];

			FFrameNumber Time = (Position.InputValue * FrameResolution).RoundToFrame();

			FMovieSceneFloatValue Value(Position.OutputValue);

			if (Attributes.HasInterpMode())    { Value.InterpMode            = Attributes.GetInterpMode();    }
			if (Attributes.HasTangentMode())   { Value.TangentMode           = Attributes.GetTangentMode();   }
			if (Attributes.HasArriveTangent()) { Value.Tangent.ArriveTangent = Attributes.GetArriveTangent(); }
			if (Attributes.HasLeaveTangent())  { Value.Tangent.LeaveTangent  = Attributes.GetLeaveTangent();  }

			int32 KeyIndex = ChannelInterface.AddKey(Time, Value);
			if (OutKeyHandles)
			{
				(*OutKeyHandles)[Index] = ChannelInterface.GetHandle(KeyIndex);
			}
		}

		Channel->AutoSetTangents();
	}
}

bool FFloatChannelCurveModel::Evaluate(double Time, double& OutValue) const
{
	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();

	if (Channel && Section)
	{
		FFrameRate FrameResolution = Section->GetTypedOuter<UMovieScene>()->GetFrameResolution();

		float ThisValue = 0.f;
		if (Channel->Evaluate(Time * FrameResolution, ThisValue))
		{
			OutValue = ThisValue;
			return true;
		}
	}

	return false;
}

void FFloatChannelCurveModel::RemoveKeys(TArrayView<const FKeyHandle> InKeys)
{
	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();
	if (Channel && Section)
	{
		Section->Modify();

		TMovieSceneChannel<FMovieSceneFloatValue> ChannelInterface = Channel->GetInterface();

		for (FKeyHandle Handle : InKeys)
		{
			int32 KeyIndex = ChannelInterface.GetIndex(Handle);
			if (KeyIndex != INDEX_NONE)
			{
				ChannelInterface.RemoveKey(KeyIndex);
			}
		}
	}
}

void FFloatChannelCurveModel::DrawCurve(const FCurveEditor& CurveEditor, TArray<TTuple<double, double>>& InterpolatingPoints) const
{
	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();

	if (Channel && Section)
	{
		FCurveEditorScreenSpace ScreenSpace = CurveEditor.GetScreenSpace();

		FFrameRate   FrameResolution  = Section->GetTypedOuter<UMovieScene>()->GetFrameResolution();

		const double DisplayOffset    = GetInputDisplayOffset();
		const double StartTimeSeconds = ScreenSpace.GetInputMin() - DisplayOffset;
		const double EndTimeSeconds   = ScreenSpace.GetInputMax() - DisplayOffset;
		const double TimeThreshold    = FMath::Max(0.0001, 1.0 / ScreenSpace.PixelsPerInput());
		const double ValueThreshold   = FMath::Max(0.0001, 1.0 / ScreenSpace.PixelsPerOutput());

		Channel->PopulateCurvePoints(StartTimeSeconds, EndTimeSeconds, TimeThreshold, ValueThreshold, FrameResolution, InterpolatingPoints);
	}
}

void FFloatChannelCurveModel::GetKeys(const FCurveEditor& CurveEditor, double MinTime, double MaxTime, double MinValue, double MaxValue, TArray<FKeyHandle>& OutKeyHandles) const
{
	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();

	if (Channel && Section)
	{
		FFrameRate FrameResolution = Section->GetTypedOuter<UMovieScene>()->GetFrameResolution();

		TMovieSceneChannel<FMovieSceneFloatValue> ChannelInterface = Channel->GetInterface();
		TArrayView<const FFrameNumber>            Times  = ChannelInterface.GetTimes();
		TArrayView<const FMovieSceneFloatValue>   Values = ChannelInterface.GetValues();

		const FFrameNumber StartFrame = MinTime <= MIN_int32 ? MIN_int32 : (MinTime * FrameResolution).CeilToFrame();
		const FFrameNumber EndFrame   = MaxTime >= MAX_int32 ? MAX_int32 : (MaxTime * FrameResolution).FloorToFrame();

		const int32 StartingIndex = Algo::LowerBound(Times, StartFrame);
		const int32 EndingIndex   = Algo::UpperBound(Times, EndFrame);

		for (int32 KeyIndex = StartingIndex; KeyIndex < EndingIndex; ++KeyIndex)
		{
			if (Values[KeyIndex].Value >= MinValue && Values[KeyIndex].Value <= MaxValue)
			{
				OutKeyHandles.Add(ChannelInterface.GetHandle(KeyIndex));
			}
		}
	}
}

void FFloatChannelCurveModel::GetKeyDrawInfo(ECurvePointType PointType, FKeyDrawInfo& OutDrawInfo) const
{
	switch (PointType)
	{
	case ECurvePointType::ArriveTangent:
	case ECurvePointType::LeaveTangent:
		OutDrawInfo.Brush = FEditorStyle::GetBrush("Sequencer.TangentHandle");
		OutDrawInfo.ScreenSize = FVector2D(7, 7);
		break;
	default:
		OutDrawInfo.Brush = FEditorStyle::GetBrush("CurveEd.CurveKey");
		OutDrawInfo.ScreenSize = FVector2D(11, 11);
		break;
	}
}

void FFloatChannelCurveModel::GetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<FKeyPosition> OutKeyPositions) const
{
	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();

	if (Channel && Section)
	{
		FFrameRate FrameResolution = Section->GetTypedOuter<UMovieScene>()->GetFrameResolution();

		TMovieSceneChannel<FMovieSceneFloatValue> ChannelInterface = Channel->GetInterface();
		TArrayView<const FFrameNumber>            Times            = ChannelInterface.GetTimes();
		TArrayView<const FMovieSceneFloatValue>   Values           = ChannelInterface.GetValues();

		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			int32 KeyIndex = ChannelInterface.GetIndex(InKeys[Index]);
			if (KeyIndex != INDEX_NONE)
			{
				OutKeyPositions[Index].InputValue  = Times[KeyIndex] / FrameResolution;
				OutKeyPositions[Index].OutputValue = Values[KeyIndex].Value;
			}
		}
	}
}

void FFloatChannelCurveModel::SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions)
{
	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();

	if (Channel && Section)
	{
		Section->MarkAsChanged();

		FFrameRate FrameResolution = Section->GetTypedOuter<UMovieScene>()->GetFrameResolution();

		TMovieSceneChannel<FMovieSceneFloatValue> ChannelInterface = Channel->GetInterface();
		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			int32 KeyIndex = ChannelInterface.GetIndex(InKeys[Index]);
			if (KeyIndex != INDEX_NONE)
			{
				FFrameTime NewTime = InKeyPositions[Index].InputValue * FrameResolution;

				KeyIndex = ChannelInterface.MoveKey(KeyIndex, NewTime.FloorToFrame());
				ChannelInterface.GetValues()[KeyIndex].Value = InKeyPositions[Index].OutputValue;
			}
		}

		Channel->AutoSetTangents();
	}
}

void FFloatChannelCurveModel::GetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<FKeyAttributes> OutAttributes) const
{
	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();
	if (Channel && Section)
	{
		TMovieSceneChannel<FMovieSceneFloatValue> ChannelInterface = Channel->GetInterface();
		TArrayView<const FFrameNumber>    Times  = ChannelInterface.GetTimes();
		TArrayView<FMovieSceneFloatValue> Values = ChannelInterface.GetValues();

		float TimeInterval = Section->GetTypedOuter<UMovieScene>()->GetFrameResolution().AsInterval();

		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			const int32 KeyIndex = ChannelInterface.GetIndex(InKeys[Index]);
			if (KeyIndex != INDEX_NONE)
			{
				const FMovieSceneFloatValue& KeyValue    = Values[KeyIndex];
				FKeyAttributes&              Attributes  = OutAttributes[Index];

				Attributes.SetInterpMode(KeyValue.InterpMode);

				if (KeyValue.InterpMode != RCIM_Constant && KeyValue.InterpMode != RCIM_Linear)
				{
					Attributes.SetTangentMode(KeyValue.TangentMode);
					if (KeyIndex != 0)
					{
						Attributes.SetArriveTangent(KeyValue.Tangent.ArriveTangent / TimeInterval);
					}

					if (KeyIndex != Times.Num()-1)
					{
						Attributes.SetLeaveTangent(KeyValue.Tangent.LeaveTangent / TimeInterval);
					}
				}
			}
		}
	}
}

void FFloatChannelCurveModel::SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes)
{
	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();
	if (Channel && Section)
	{
		bool bAutoSetTangents = false;
		Section->MarkAsChanged();

		TMovieSceneChannel<FMovieSceneFloatValue> ChannelInterface = Channel->GetInterface();
		TArrayView<FMovieSceneFloatValue> Values = ChannelInterface.GetValues();

		float TimeInterval = Section->GetTypedOuter<UMovieScene>()->GetFrameResolution().AsInterval();

		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			const int32 KeyIndex = ChannelInterface.GetIndex(InKeys[Index]);
			if (KeyIndex != INDEX_NONE)
			{
				const FKeyAttributes&  Attributes = InAttributes[Index];
				FMovieSceneFloatValue& KeyValue   = Values[KeyIndex];

				if (Attributes.HasInterpMode())    { KeyValue.InterpMode  = Attributes.GetInterpMode();  bAutoSetTangents = true; }
				if (Attributes.HasTangentMode())   { KeyValue.TangentMode = Attributes.GetTangentMode(); bAutoSetTangents = true; }

				if (Attributes.HasArriveTangent())
				{
					if (KeyValue.TangentMode == RCTM_Auto)
					{
						KeyValue.TangentMode = RCTM_User;
					}

					KeyValue.Tangent.ArriveTangent = Attributes.GetArriveTangent() * TimeInterval;
					if (KeyValue.InterpMode == RCIM_Cubic && KeyValue.TangentMode != RCTM_Break)
					{
						KeyValue.Tangent.LeaveTangent = KeyValue.Tangent.ArriveTangent;
					}
				}

				if (Attributes.HasLeaveTangent())
				{
					if (KeyValue.TangentMode == RCTM_Auto)
					{
						KeyValue.TangentMode = RCTM_User;
					}

					KeyValue.Tangent.LeaveTangent = Attributes.GetLeaveTangent() * TimeInterval;
					if (KeyValue.InterpMode == RCIM_Cubic && KeyValue.TangentMode != RCTM_Break)
					{
						KeyValue.Tangent.ArriveTangent = KeyValue.Tangent.LeaveTangent;
					}
				}
			}
		}

		if (bAutoSetTangents)
		{
			Channel->AutoSetTangents();
		}
	}
}

void FFloatChannelCurveModel::GetCurveAttributes(FCurveAttributes& OutCurveAttributes) const
{
	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	if (Channel)
	{
		OutCurveAttributes.SetPreExtrapolation(Channel->PreInfinityExtrap);
		OutCurveAttributes.SetPostExtrapolation(Channel->PostInfinityExtrap);
	}
}

void FFloatChannelCurveModel::SetCurveAttributes(const FCurveAttributes& InCurveAttributes)
{
	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();
	if (Channel && Section)
	{
		Section->MarkAsChanged();

		if (InCurveAttributes.HasPreExtrapolation())
		{
			Channel->PreInfinityExtrap = InCurveAttributes.GetPreExtrapolation();
		}

		if (InCurveAttributes.HasPostExtrapolation())
		{
			Channel->PostInfinityExtrap = InCurveAttributes.GetPostExtrapolation();
		}
	}
}

class SCurveEditorKeyEditInterface : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCurveEditorKeyEditInterface){}
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs,
		TSharedRef<ISequencer> InSequencer,
		FCurveModelID InCurveID,
		TWeakPtr<FCurveEditor> InWeakCurveEditor,
		TWeakObjectPtr<UMovieSceneSection> InWeakSection,
		TMovieSceneChannelHandle<FMovieSceneFloatChannel> InChannelHandle
		)
	{
		CachedSelectionSerialNumber = 0;

		CurveID         = InCurveID;
		WeakCurveEditor = InWeakCurveEditor;
		WeakSection     = InWeakSection;
		ChannelHandle   = InChannelHandle;

		ChildSlot
		[
			SAssignNew(Interface, SKeyEditInterface, InSequencer)
			.EditData(this, &SCurveEditorKeyEditInterface::UpdateAndRetrieveEditData)
		];
	}

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
	{
		TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
		if (CurveEditor.IsValid() && CurveEditor->Selection.GetSerialNumber() != CachedSelectionSerialNumber)
		{
			Interface->Initialize();
			CachedSelectionSerialNumber = CurveEditor->Selection.GetSerialNumber();
		}
	}

private:

	FKeyEditData UpdateAndRetrieveEditData() const
	{
		FKeyEditData EditData;

		TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
		if (CurveEditor.IsValid())
		{
			const FKeyHandleSet* SelectedKeys = CurveEditor->Selection.FindForCurve(CurveID.GetValue());
			if (SelectedKeys && SelectedKeys->Num() == 1)
			{
				using namespace Sequencer;
				EditData.KeyStruct = GetKeyStruct(ChannelHandle, SelectedKeys->AsArray()[0]);
				EditData.OwningSection = WeakSection;
			}
		}

		return EditData;
	}

	TOptional<FCurveModelID> CurveID;
	TWeakPtr<FCurveEditor> WeakCurveEditor;
	TWeakObjectPtr<UMovieSceneSection> WeakSection;
	TMovieSceneChannelHandle<FMovieSceneFloatChannel> ChannelHandle;
	uint32 CachedSelectionSerialNumber;
	TSharedPtr<SKeyEditInterface> Interface;
};

TSharedPtr<SWidget> FFloatChannelCurveModel::CreateEditUI(TSharedPtr<FCurveEditor> InCurveEditor, FCurveModelID ThisCurveID)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		return SNew(SCurveEditorKeyEditInterface, Sequencer.ToSharedRef(), ThisCurveID, TWeakPtr<FCurveEditor>(InCurveEditor), WeakSection, ChannelHandle);
	}

	return nullptr;
}