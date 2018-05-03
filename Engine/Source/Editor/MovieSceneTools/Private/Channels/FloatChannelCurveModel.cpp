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
#include "SequencerChannelTraits.h"
#include "ISequencer.h"
#include "Channels/FloatChannelKeyProxy.h"

FFloatChannelCurveModel::FFloatChannelCurveModel(TMovieSceneChannelHandle<FMovieSceneFloatChannel> InChannel, UMovieSceneSection* OwningSection, TWeakPtr<ISequencer> InWeakSequencer)
{
	ChannelHandle = InChannel;
	WeakSection = OwningSection;
	WeakSequencer = InWeakSequencer;
	if (UMovieSceneSection* Section = WeakSection.Get())
	{
		FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
		Channel->SetTickResolution(Section->GetTypedOuter<UMovieScene>()->GetTickResolution());
	}
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

void FFloatChannelCurveModel::AddKeys(TArrayView<const FKeyPosition> InKeyPositions, TArrayView<const FKeyAttributes> InKeyAttributes, TArrayView<TOptional<FKeyHandle>>* OutKeyHandles)
{
	check(InKeyPositions.Num() == InKeyAttributes.Num() && (!OutKeyHandles || OutKeyHandles->Num() == InKeyPositions.Num()));

	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();
	if (Channel && Section)
	{
		Section->Modify();

		TMovieSceneChannel<FMovieSceneFloatValue> ChannelInterface = Channel->GetInterface();
		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		for (int32 Index = 0; Index < InKeyPositions.Num(); ++Index)
		{
			FKeyPosition   Position   = InKeyPositions[Index];
			FKeyAttributes Attributes = InKeyAttributes[Index];

			FFrameNumber Time = (Position.InputValue * TickResolution).RoundToFrame();
			Section->ExpandToFrame(Time);

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
		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		float ThisValue = 0.f;
		if (Channel->Evaluate(Time * TickResolution, ThisValue))
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

		FFrameRate   TickResolution   = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		const double DisplayOffset    = GetInputDisplayOffset();
		const double StartTimeSeconds = ScreenSpace.GetInputMin() - DisplayOffset;
		const double EndTimeSeconds   = ScreenSpace.GetInputMax() - DisplayOffset;
		const double TimeThreshold    = FMath::Max(0.0001, 1.0 / ScreenSpace.PixelsPerInput());
		const double ValueThreshold   = FMath::Max(0.0001, 1.0 / ScreenSpace.PixelsPerOutput());

		Channel->PopulateCurvePoints(StartTimeSeconds, EndTimeSeconds, TimeThreshold, ValueThreshold, TickResolution, InterpolatingPoints);
	}
}

void FFloatChannelCurveModel::GetKeys(const FCurveEditor& CurveEditor, double MinTime, double MaxTime, double MinValue, double MaxValue, TArray<FKeyHandle>& OutKeyHandles) const
{
	FMovieSceneFloatChannel* Channel = ChannelHandle.Get();
	UMovieSceneSection*      Section = WeakSection.Get();

	if (Channel && Section)
	{
		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		TMovieSceneChannel<FMovieSceneFloatValue> ChannelInterface = Channel->GetInterface();
		TArrayView<const FFrameNumber>            Times  = ChannelInterface.GetTimes();
		TArrayView<const FMovieSceneFloatValue>   Values = ChannelInterface.GetValues();

		const FFrameNumber StartFrame = MinTime <= MIN_int32 ? MIN_int32 : (MinTime * TickResolution).CeilToFrame();
		const FFrameNumber EndFrame   = MaxTime >= MAX_int32 ? MAX_int32 : (MaxTime * TickResolution).FloorToFrame();

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
		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		TMovieSceneChannel<FMovieSceneFloatValue> ChannelInterface = Channel->GetInterface();
		TArrayView<const FFrameNumber>            Times            = ChannelInterface.GetTimes();
		TArrayView<const FMovieSceneFloatValue>   Values           = ChannelInterface.GetValues();

		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			int32 KeyIndex = ChannelInterface.GetIndex(InKeys[Index]);
			if (KeyIndex != INDEX_NONE)
			{
				OutKeyPositions[Index].InputValue  = Times[KeyIndex] / TickResolution;
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

		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();

		TMovieSceneChannel<FMovieSceneFloatValue> ChannelInterface = Channel->GetInterface();
		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			int32 KeyIndex = ChannelInterface.GetIndex(InKeys[Index]);
			if (KeyIndex != INDEX_NONE)
			{
				FFrameNumber NewTime = (InKeyPositions[Index].InputValue * TickResolution).FloorToFrame();

				KeyIndex = ChannelInterface.MoveKey(KeyIndex, NewTime);
				ChannelInterface.GetValues()[KeyIndex].Value = InKeyPositions[Index].OutputValue;

				Section->ExpandToFrame(NewTime);
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

		float TimeInterval = Section->GetTypedOuter<UMovieScene>()->GetTickResolution().AsInterval();

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
					if (KeyValue.InterpMode == RCIM_Cubic)
					{
						Attributes.SetTangentWeightMode(KeyValue.Tangent.TangentWeightMode);
						if (KeyValue.Tangent.TangentWeightMode != RCTWM_WeightedNone)
						{
							Attributes.SetArriveTangentWeight(KeyValue.Tangent.ArriveTangentWeight);
							Attributes.SetLeaveTangentWeight(KeyValue.Tangent.LeaveTangentWeight);
						}
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

		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
		float TimeInterval = TickResolution.AsInterval();

		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			const int32 KeyIndex = ChannelInterface.GetIndex(InKeys[Index]);
			if (KeyIndex != INDEX_NONE)
			{
				const FKeyAttributes&  Attributes = InAttributes[Index];
				FMovieSceneFloatValue& KeyValue   = Values[KeyIndex];
				if (Attributes.HasInterpMode())    { KeyValue.InterpMode  = Attributes.GetInterpMode();  bAutoSetTangents = true; }
				if (Attributes.HasTangentMode())
				{
					KeyValue.TangentMode = Attributes.GetTangentMode();
					if (KeyValue.TangentMode == RCTM_Auto)
					{
						KeyValue.Tangent.TangentWeightMode = RCTWM_WeightedNone;
					}
					bAutoSetTangents = true;
				}
				if (Attributes.HasTangentWeightMode()) 
				{ 
					if (KeyValue.Tangent.TangentWeightMode == RCTWM_WeightedNone) //set tangent weights to default use
					{
						TArrayView<const FFrameNumber> Times = Channel->GetTimes();
						
						//calculate a tangent weight based upon tangent and time difference
						//calculate arrive tangent weight
						if (KeyIndex > 0 )
						{
							const float X = TickResolution.AsSeconds(Times[KeyIndex].Value - Times[KeyIndex - 1].Value);
							const float ArriveTangentNormal = KeyValue.Tangent.ArriveTangent / (TimeInterval);
							const float Y = ArriveTangentNormal * X;
							KeyValue.Tangent.ArriveTangentWeight = FMath::Sqrt(X*X + Y*Y) * 0.3f;
						}
						//calculate leave weight
						if(KeyIndex < ( Times.Num() - 1))
						{
							const float X = TickResolution.AsSeconds(Times[KeyIndex].Value - Times[KeyIndex + 1].Value);
							const float LeaveTangentNormal = KeyValue.Tangent.LeaveTangent / (TimeInterval);
							const float Y = LeaveTangentNormal * X;
							KeyValue.Tangent.LeaveTangentWeight = FMath::Sqrt(X*X + Y*Y) * 0.3f;
						}
					}
					KeyValue.Tangent.TangentWeightMode = Attributes.GetTangentWeightMode();

					if( KeyValue.Tangent.TangentWeightMode != RCTWM_WeightedNone )
					{
						if (KeyValue.TangentMode != RCTM_User && KeyValue.TangentMode != RCTM_Break)
						{
							KeyValue.TangentMode = RCTM_User;
						}
					}
				}

				if (Attributes.HasArriveTangent())
				{
					if (KeyValue.TangentMode == RCTM_Auto)
					{
						KeyValue.TangentMode = RCTM_User;
						KeyValue.Tangent.TangentWeightMode = RCTWM_WeightedNone;
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
						KeyValue.Tangent.TangentWeightMode = RCTWM_WeightedNone;
					}

					KeyValue.Tangent.LeaveTangent = Attributes.GetLeaveTangent() * TimeInterval;
					if (KeyValue.InterpMode == RCIM_Cubic && KeyValue.TangentMode != RCTM_Break)
					{
						KeyValue.Tangent.ArriveTangent = KeyValue.Tangent.LeaveTangent;
					}
				}

				if (Attributes.HasArriveTangentWeight())
				{
					if (KeyValue.TangentMode == RCTM_Auto)
					{
						KeyValue.TangentMode = RCTM_User;
						KeyValue.Tangent.TangentWeightMode = RCTWM_WeightedNone;
					}

					KeyValue.Tangent.ArriveTangentWeight = Attributes.GetArriveTangentWeight(); 
					if (KeyValue.InterpMode == RCIM_Cubic && KeyValue.TangentMode != RCTM_Break)
					{
						KeyValue.Tangent.LeaveTangentWeight = KeyValue.Tangent.ArriveTangentWeight;
					}
				}

				if (Attributes.HasLeaveTangentWeight())
				{
				
					if (KeyValue.TangentMode == RCTM_Auto)
					{
						KeyValue.TangentMode = RCTM_User;
						KeyValue.Tangent.TangentWeightMode = RCTWM_WeightedNone;
					}

					KeyValue.Tangent.LeaveTangentWeight = Attributes.GetLeaveTangentWeight();
					if (KeyValue.InterpMode == RCIM_Cubic && KeyValue.TangentMode != RCTM_Break)
					{
						KeyValue.Tangent.ArriveTangentWeight = KeyValue.Tangent.LeaveTangentWeight;
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

void FFloatChannelCurveModel::CreateKeyProxies(TArrayView<const FKeyHandle> InKeyHandles, TArrayView<UObject*> OutObjects)
{
	for (int32 Index = 0; Index < InKeyHandles.Num(); ++Index)
	{
		UFloatChannelKeyProxy* NewProxy = NewObject<UFloatChannelKeyProxy>(GetTransientPackage(), NAME_None);

		NewProxy->Initialize(InKeyHandles[Index], ChannelHandle, WeakSection);
		OutObjects[Index] = NewProxy;
	}
}