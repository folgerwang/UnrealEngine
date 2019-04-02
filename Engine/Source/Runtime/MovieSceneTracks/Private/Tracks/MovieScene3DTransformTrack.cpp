// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Compilation/MovieSceneTemplateInterrogation.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Algo/BinarySearch.h"

UMovieScene3DTransformTrack::UMovieScene3DTransformTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	static FName Transform("Transform");
	SetPropertyNameAndPath(Transform, Transform.ToString());

	SupportedBlendTypes = FMovieSceneBlendTypeField::All();

#if WITH_EDITORONLY_DATA
	TrackTint = FColor(65, 173, 164, 65);
#endif

	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
}

bool UMovieScene3DTransformTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieScene3DTransformSection::StaticClass();
}

UMovieSceneSection* UMovieScene3DTransformTrack::CreateNewSection()
{
	return NewObject<UMovieScene3DTransformSection>(this, NAME_None, RF_Transactional);
}


#if WITH_EDITOR

uint32 GetDistanceFromTo(FFrameNumber TestValue, FFrameNumber TargetValue)
{
	static const int32 MaxInt = TNumericLimits<int32>::Max();
	if (TestValue == TargetValue)
	{
		return 0;
	}
	else if (TestValue > TargetValue)
	{
		return (uint32(TestValue.Value + MaxInt) - uint32(TargetValue.Value + MaxInt));
	}
	else
	{
		return (uint32(TargetValue.Value + MaxInt) - uint32(TestValue.Value + MaxInt));
	}
}

TArray<FTrajectoryKey> UMovieScene3DTransformTrack::GetTrajectoryData(FFrameNumber Time, int32 MaxNumDataPoints) const
{
	struct FCurveKeyIterator
	{
		FCurveKeyIterator(UMovieScene3DTransformSection* InSection, FMovieSceneFloatChannel* InChannel, FName InChannelName, FFrameNumber StartTime)
			: Section(InSection), Channel(InChannel->GetData()), ChannelName(InChannelName), SectionRange(InSection->GetRange()), CurrentIndex(INDEX_NONE)
		{
			TArrayView<const FFrameNumber> Times = Channel.GetTimes();
			CurrentIndex = Algo::LowerBound(Times, StartTime);

			bIsLowerBound = false;
			bIsUpperBound = CurrentIndex == Times.Num() && SectionRange.GetUpperBound().IsClosed();
		}

		bool IsValid() const
		{
			return Channel.GetTimes().IsValidIndex(CurrentIndex);
		}

		FFrameNumber GetCurrentKeyTime() const
		{
			return Channel.GetTimes()[CurrentIndex];
		}

		FCurveKeyIterator& operator--()
		{
			TArrayView<const FFrameNumber> Times = Channel.GetTimes();

			if (bIsLowerBound)
			{
				bIsLowerBound = false;
				CurrentIndex = INDEX_NONE;
			}
			else
			{
				if (bIsUpperBound)
				{
					bIsUpperBound = false;
					CurrentIndex = Algo::LowerBound(Times, SectionRange.GetUpperBoundValue()) - 1;
				}
				else
				{
					--CurrentIndex;
				}

				bIsLowerBound = SectionRange.GetLowerBound().IsClosed() && (!Times.IsValidIndex(CurrentIndex) || !SectionRange.Contains(Channel.GetTimes()[CurrentIndex]) );
			}
			return *this;
		}

		FCurveKeyIterator& operator++()
		{
			TArrayView<const FFrameNumber> Times = Channel.GetTimes();

			if (bIsUpperBound)
			{
				bIsUpperBound = false;
				CurrentIndex = INDEX_NONE;
			}
			else
			{
				if (bIsLowerBound)
				{
					bIsLowerBound = false;
					CurrentIndex = Algo::UpperBound(Times, SectionRange.GetLowerBoundValue());
				}
				else 
				{
					++CurrentIndex;
				}

				bIsUpperBound = SectionRange.GetUpperBound().IsClosed() && (!Times.IsValidIndex(CurrentIndex) || !SectionRange.Contains(Times[CurrentIndex]) );
			}
			return *this;
		}

		explicit operator bool() const
		{
			TArrayView<const FFrameNumber> Times = Channel.GetTimes();
			return ( bIsLowerBound || bIsUpperBound ) || ( Times.IsValidIndex(CurrentIndex) && SectionRange.Contains(Times[CurrentIndex]) );
		}

		FFrameNumber GetTime() const
		{
			return bIsLowerBound ? SectionRange.GetLowerBoundValue() : bIsUpperBound ? SectionRange.GetUpperBoundValue() : Channel.GetTimes()[CurrentIndex];
		}

		ERichCurveInterpMode GetInterpMode() const
		{
			return ( bIsLowerBound || bIsUpperBound ) ? RCIM_None : Channel.GetValues()[CurrentIndex].InterpMode.GetValue();
		}

		UMovieScene3DTransformSection* GetSection() const
		{
			return Section;
		}

		FName GetChannelName() const
		{
			return ChannelName;
		}

		TOptional<FKeyHandle> GetKeyHandle()
		{
			return CurrentIndex == INDEX_NONE ? TOptional<FKeyHandle>() : Channel.GetHandle(CurrentIndex);
		}

	private:
		UMovieScene3DTransformSection* Section;
		TMovieSceneChannelData<FMovieSceneFloatValue> Channel;
		FName ChannelName;
		TRange<FFrameNumber> SectionRange;
		int32 CurrentIndex;
		bool bIsUpperBound, bIsLowerBound;
	};

	TArray<FCurveKeyIterator> ForwardIters;
	TArray<FCurveKeyIterator> BackwardIters;

	for (UMovieSceneSection* Section : Sections)
	{
		UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(Section);
		if (TransformSection)
		{
			TArrayView<FMovieSceneFloatChannel*>         FloatChannels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
			TArrayView<const FMovieSceneChannelMetaData> MetaData      = TransformSection->GetChannelProxy().GetMetaData<FMovieSceneFloatChannel>();

			EMovieSceneTransformChannel Mask = TransformSection->GetMask().GetChannels();
			if (EnumHasAnyFlags(Mask, EMovieSceneTransformChannel::TranslationX))
			{
				ForwardIters.Emplace(TransformSection, FloatChannels[0], MetaData[0].Name, Time);
				BackwardIters.Emplace(TransformSection, FloatChannels[0], MetaData[0].Name, Time);
			}
			if (EnumHasAnyFlags(Mask, EMovieSceneTransformChannel::TranslationY))
			{
				ForwardIters.Emplace(TransformSection, FloatChannels[1], MetaData[1].Name, Time);
				BackwardIters.Emplace(TransformSection, FloatChannels[1], MetaData[1].Name, Time);
			}
			if (EnumHasAnyFlags(Mask, EMovieSceneTransformChannel::TranslationZ))
			{
				ForwardIters.Emplace(TransformSection, FloatChannels[2], MetaData[2].Name, Time);
				BackwardIters.Emplace(TransformSection, FloatChannels[2], MetaData[2].Name, Time);
			}
			if (EnumHasAnyFlags(Mask, EMovieSceneTransformChannel::RotationX))
			{
				ForwardIters.Emplace(TransformSection, FloatChannels[3], MetaData[3].Name, Time);
				BackwardIters.Emplace(TransformSection, FloatChannels[3], MetaData[3].Name, Time);
			}
			if (EnumHasAnyFlags(Mask, EMovieSceneTransformChannel::RotationY))
			{
				ForwardIters.Emplace(TransformSection, FloatChannels[4], MetaData[4].Name, Time);
				BackwardIters.Emplace(TransformSection, FloatChannels[4], MetaData[4].Name, Time);
			}
			if (EnumHasAnyFlags(Mask, EMovieSceneTransformChannel::RotationZ))
			{
				ForwardIters.Emplace(TransformSection, FloatChannels[5], MetaData[5].Name, Time);
				BackwardIters.Emplace(TransformSection, FloatChannels[5], MetaData[5].Name, Time);
			}
		}
	}

	auto HasAnyValidIterators = [](const FCurveKeyIterator& It)
	{
		return bool(It);
	};

	TArray<FTrajectoryKey> Result;
	while (ForwardIters.ContainsByPredicate(HasAnyValidIterators) || BackwardIters.ContainsByPredicate(HasAnyValidIterators))
	{
		if (MaxNumDataPoints != 0 && Result.Num() >= MaxNumDataPoints)
		{
			break;
		}

		uint32 ClosestDistance = -1;
		TOptional<FFrameNumber> ClosestTime;

		// Find the closest key time
		for (const FCurveKeyIterator& Fwd : ForwardIters)
		{
			if (Fwd && ( !ClosestTime.IsSet() || GetDistanceFromTo(Fwd.GetTime(), Time) < ClosestDistance ) )
			{
				ClosestTime = Fwd.GetTime();
				ClosestDistance = GetDistanceFromTo(Fwd.GetTime(), Time);
			}
		}
		for (const FCurveKeyIterator& Bck : BackwardIters)
		{
			if (Bck && ( !ClosestTime.IsSet() || GetDistanceFromTo(Bck.GetTime(), Time) < ClosestDistance ) )
			{
				ClosestTime = Bck.GetTime();
				ClosestDistance = GetDistanceFromTo(Bck.GetTime(), Time);
			}
		}

		
		FTrajectoryKey& NewKey = Result[Result.Emplace(ClosestTime.GetValue())];
		for (FCurveKeyIterator& Fwd : ForwardIters)
		{
			if (Fwd && Fwd.GetTime() == NewKey.Time)
			{
				if (Fwd.IsValid())
				{
					// Add this key to the trajectory key
					NewKey.KeyData.Emplace(Fwd.GetSection(), Fwd.GetKeyHandle(), Fwd.GetInterpMode(), Fwd.GetChannelName());
				}
				// Move onto the next key in this curve
				++Fwd;
			}
		}
		
		for (FCurveKeyIterator& Bck : BackwardIters)
		{
			if (Bck && Bck.GetTime() == NewKey.Time)
			{
				if (Bck.IsValid())
				{
					// Add this key to the trajectory key
					NewKey.KeyData.Emplace(Bck.GetSection(), Bck.GetKeyHandle(), Bck.GetInterpMode(), Bck.GetChannelName());
				}
				// Move onto the next key in this curve
				--Bck;
			}
		}
	}

	Result.Sort(
		[](const FTrajectoryKey& A, const FTrajectoryKey& B)
		{
			return A.Time < B.Time;
		}
	);

	return Result;
}

#endif	// WITH_EDITOR
