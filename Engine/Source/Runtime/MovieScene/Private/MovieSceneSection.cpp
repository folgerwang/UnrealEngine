// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneTimeHelpers.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Generators/MovieSceneEasingCurves.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Containers/ArrayView.h"
#include "Channels/MovieSceneChannel.h"
#include "UObject/SequencerObjectVersion.h"

UMovieSceneSection::UMovieSceneSection(const FObjectInitializer& ObjectInitializer)
	: Super( ObjectInitializer )
	, PreRollFrames(0)
	, PostRollFrames(0)
	, RowIndex(0)
	, OverlapPriority(0)
	, bIsActive(true)
	, bIsLocked(false)
	, StartTime_DEPRECATED(0.f)
	, EndTime_DEPRECATED(0.f)
	, PreRollTime_DEPRECATED(0.f)
	, PostRollTime_DEPRECATED(0.f)
	, bIsInfinite_DEPRECATED(0)
	, bSupportsInfiniteRange(false)
{
	SectionRange.Value = TRange<FFrameNumber>(0);

	UMovieSceneBuiltInEasingFunction* DefaultEaseIn = ObjectInitializer.CreateDefaultSubobject<UMovieSceneBuiltInEasingFunction>(this, "EaseInFunction");
	DefaultEaseIn->SetFlags(RF_Public); //@todo Need to be marked public. GLEO occurs when transform sections are added to actor sequence blueprints. Are these not being duplicated properly?
	DefaultEaseIn->Type = EMovieSceneBuiltInEasing::CubicInOut;
	Easing.EaseIn = DefaultEaseIn;

	UMovieSceneBuiltInEasingFunction* DefaultEaseOut = ObjectInitializer.CreateDefaultSubobject<UMovieSceneBuiltInEasingFunction>(this, "EaseOutFunction");
	DefaultEaseOut->SetFlags(RF_Public); //@todo Need to be marked public. GLEO occurs when transform sections are added to actor sequence blueprints. Are these not being duplicated properly?
	DefaultEaseOut->Type = EMovieSceneBuiltInEasing::CubicInOut;
	Easing.EaseOut = DefaultEaseOut;
}


void UMovieSceneSection::PostInitProperties()
{
	// Propagate sub object flags from our outer (track) to ourselves. This is required for sections that are stored on blueprints (archetypes) so that they can be referenced in worlds.
	if (GetOuter()->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject))
	{
		SetFlags(GetOuter()->GetMaskedFlags(RF_PropagateToSubObjects));
	}
	
	Super::PostInitProperties();

	// Set up a default channel proxy if this class hasn't done so already
	if (!HasAnyFlags(RF_ClassDefaultObject) && !ChannelProxy.IsValid())
	{
		ChannelProxy = MakeShared<FMovieSceneChannelProxy>();
	}
}

bool UMovieSceneSection::IsPostLoadThreadSafe() const
{
	return true;
}

void UMovieSceneSection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);

	if (Ar.CustomVer(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::FloatToIntConversion)
	{
		const FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();

		if (bIsInfinite_DEPRECATED && bSupportsInfiniteRange)
		{
			SectionRange = TRange<FFrameNumber>::All();
		}
		else
		{
			FFrameNumber StartFrame = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, StartTime_DEPRECATED);
			FFrameNumber LastFrame  = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, EndTime_DEPRECATED);

			// Exclusive upper bound so we want the upper bound to be exclusively the next frame after LastFrame
			SectionRange = TRange<FFrameNumber>(StartFrame, LastFrame + 1);
		}

		// All these times are offsets from the start/end time so it's highly unlikely that they'll be out-of bounds
		PreRollFrames                = LegacyFrameRate.AsFrameNumber(PreRollTime_DEPRECATED).Value;
		PostRollFrames               = LegacyFrameRate.AsFrameNumber(PostRollTime_DEPRECATED).Value;
#if WITH_EDITORONLY_DATA
		Easing.AutoEaseInDuration    = (Easing.AutoEaseInTime_DEPRECATED * LegacyFrameRate).RoundToFrame().Value;
		Easing.AutoEaseOutDuration   = (Easing.AutoEaseOutTime_DEPRECATED * LegacyFrameRate).RoundToFrame().Value;
		Easing.ManualEaseInDuration  = (Easing.ManualEaseInTime_DEPRECATED * LegacyFrameRate).RoundToFrame().Value;
		Easing.ManualEaseOutDuration = (Easing.ManualEaseOutTime_DEPRECATED * LegacyFrameRate).RoundToFrame().Value;
#endif
	}
}

void UMovieSceneSection::SetStartFrame(TRangeBound<FFrameNumber> NewStartFrame)
{
	if (TryModify())
	{
		bool bIsValidStartFrame = ensureMsgf(SectionRange.Value.GetUpperBound().IsOpen() || NewStartFrame.IsOpen() || SectionRange.Value.GetUpperBound().GetValue() >= NewStartFrame.GetValue(),
			TEXT("Invalid start frame specified; will be clamped to current end frame."));

		if (bIsValidStartFrame)
		{
			SectionRange.Value.SetLowerBound(NewStartFrame);
		}
		else
		{
			SectionRange.Value.SetLowerBound(TRangeBound<FFrameNumber>::FlipInclusion(SectionRange.Value.GetUpperBound()));
		}
	}
}

void UMovieSceneSection::SetEndFrame(TRangeBound<FFrameNumber> NewEndFrame)
{
	if (TryModify())
	{
		bool bIsValidEndFrame = ensureMsgf(SectionRange.Value.GetLowerBound().IsOpen() || NewEndFrame.IsOpen() || SectionRange.Value.GetLowerBound().GetValue() <= NewEndFrame.GetValue(),
			TEXT("Invalid end frame specified; will be clamped to current start frame."));

		if (bIsValidEndFrame)
		{
			SectionRange.Value.SetUpperBound(NewEndFrame);
		}
		else
		{
			SectionRange.Value.SetUpperBound(TRangeBound<FFrameNumber>::FlipInclusion(SectionRange.Value.GetLowerBound()));
		}
	}
}

FMovieSceneChannelProxy& UMovieSceneSection::GetChannelProxy() const
{
	FMovieSceneChannelProxy* Proxy = ChannelProxy.Get();
	check(Proxy);
	return *Proxy;
}

TSharedPtr<FStructOnScope> UMovieSceneSection::GetKeyStruct(TArrayView<const FKeyHandle> KeyHandles)
{
	return nullptr;
}

void UMovieSceneSection::MoveSection(FFrameNumber DeltaFrame)
{
	if (TryModify())
	{
		TRange<FFrameNumber> NewRange = SectionRange.Value;
		if (SectionRange.Value.GetLowerBound().IsClosed())
		{
			SectionRange.Value.SetLowerBoundValue(SectionRange.Value.GetLowerBoundValue() + DeltaFrame);
		}
		if (SectionRange.Value.GetUpperBound().IsClosed())
		{
			SectionRange.Value.SetUpperBoundValue(SectionRange.Value.GetUpperBoundValue() + DeltaFrame);
		}

		if (ChannelProxy.IsValid())
		{
			for (const FMovieSceneChannelEntry& Entry : ChannelProxy->GetAllEntries())
			{
				for (FMovieSceneChannel* Channel : Entry.GetChannels())
				{
					Channel->Offset(DeltaFrame);
				}
			}
		}
	}

#if WITH_EDITORONLY_DATA
	TimecodeSource.DeltaFrame += DeltaFrame;
#endif
}


TRange<FFrameNumber> UMovieSceneSection::ComputeEffectiveRange() const
{
	if (!SectionRange.Value.GetLowerBound().IsOpen() && !SectionRange.Value.GetUpperBound().IsOpen())
	{
		return GetRange();
	}

	TRange<FFrameNumber> EffectiveRange = TRange<FFrameNumber>::Empty();

	if (ChannelProxy.IsValid())
	{
		for (const FMovieSceneChannelEntry& Entry : ChannelProxy->GetAllEntries())
		{
			for (const FMovieSceneChannel* Channel : Entry.GetChannels())
			{
				EffectiveRange = TRange<FFrameNumber>::Hull(EffectiveRange, Channel->ComputeEffectiveRange());
			}
		}
	}

	return TRange<FFrameNumber>::Intersection(EffectiveRange, SectionRange.Value);
}


TOptional<TRange<FFrameNumber> > UMovieSceneSection::GetAutoSizeRange() const
{
	if (ChannelProxy.IsValid())
	{
		TRange<FFrameNumber> EffectiveRange = TRange<FFrameNumber>::Empty();
	
		for (const FMovieSceneChannelEntry& Entry : ChannelProxy->GetAllEntries())
		{
			for (const FMovieSceneChannel* Channel : Entry.GetChannels())
			{
				EffectiveRange = TRange<FFrameNumber>::Hull(EffectiveRange, Channel->ComputeEffectiveRange());
			}
		}

		if (!EffectiveRange.IsEmpty())
		{
			return EffectiveRange;
		}
	}

	return TOptional<TRange<FFrameNumber> >();
}

FMovieSceneBlendTypeField UMovieSceneSection::GetSupportedBlendTypes() const
{
	UMovieSceneTrack* Track = GetTypedOuter<UMovieSceneTrack>();
	return Track ? Track->GetSupportedBlendTypes() : FMovieSceneBlendTypeField::None();
}


bool UMovieSceneSection::TryModify(bool bAlwaysMarkDirty)
{
	if (IsReadOnly())
	{
		return false;
	}

	Modify(bAlwaysMarkDirty);

	return true;
}

bool UMovieSceneSection::IsReadOnly() const
{
	if (IsLocked())
	{
		return true;
	}

#if WITH_EDITORONLY_DATA
	if (UMovieScene* OuterScene = GetTypedOuter<UMovieScene>())
	{
		if (OuterScene->IsReadOnly())
		{
			return true;
		}
	}
#endif

	return false;
}

void UMovieSceneSection::GetOverlappingSections(TArray<UMovieSceneSection*>& OutSections, bool bSameRow, bool bIncludeThis)
{
	UMovieSceneTrack* Track = GetTypedOuter<UMovieSceneTrack>();
	if (!Track)
	{
		return;
	}

	TRange<FFrameNumber> ThisRange = GetRange();
	for (UMovieSceneSection* Section : Track->GetAllSections())
	{
		if (!Section || (!bIncludeThis && Section == this))
		{
			continue;
		}

		if (bSameRow && Section->GetRowIndex() != GetRowIndex())
		{
			continue;
		}

		if (Section->GetRange().Overlaps(ThisRange))
		{
			OutSections.Add(Section);
		}
	}
}


const UMovieSceneSection* UMovieSceneSection::OverlapsWithSections(const TArray<UMovieSceneSection*>& Sections, int32 TrackDelta, int32 TimeDelta) const
{
	// Check overlaps with exclusive ranges so that sections can butt up against each other
	int32 NewTrackIndex = RowIndex + TrackDelta;

	// @todo: sequencer-timecode: is this correct? it seems like we should just use the section's ranges directly rather than fiddling with the bounds
	// TRange<FFrameNumber> NewSectionRange;
	// if (SectionRange.GetLowerBound().IsClosed())
	// {
	// 	NewSectionRange = TRange<FFrameNumber>(
	// 		TRangeBound<FFrameNumber>::Exclusive(SectionRange.GetLowerBoundValue() + TimeDelta),
	// 		NewSectionRange.GetUpperBound()
	// 		);
	// }

	// if (SectionRange.GetUpperBound().IsClosed())
	// {
	// 	NewSectionRange = TRange<FFrameNumber>(
	// 		NewSectionRange.GetLowerBound(),
	// 		TRangeBound<FFrameNumber>::Exclusive(SectionRange.GetUpperBoundValue() + TimeDelta)
	// 		);
	// }

	TRange<FFrameNumber> ThisRange = SectionRange.Value;

	for (const auto Section : Sections)
	{
		check(Section);
		if ((this != Section) && (Section->GetRowIndex() == NewTrackIndex))
		{
			//TRange<float> ExclusiveSectionRange = TRange<float>(TRange<float>::BoundsType::Exclusive(Section->GetRange().GetLowerBoundValue()), TRange<float>::BoundsType::Exclusive(Section->GetRange().GetUpperBoundValue()));
			if (ThisRange.Overlaps(Section->GetRange()))
			{
				return Section;
			}
		}
	}

	return nullptr;
}


void UMovieSceneSection::InitialPlacement(const TArray<UMovieSceneSection*>& Sections, FFrameNumber InStartTime, int32 Duration, bool bAllowMultipleRows)
{
	check(Duration >= 0);

	// Inclusive lower, exclusive upper bounds
	SectionRange = TRange<FFrameNumber>(InStartTime, InStartTime + Duration);
	RowIndex = 0;

	for (UMovieSceneSection* OtherSection : Sections)
	{
		OverlapPriority = FMath::Max(OtherSection->GetOverlapPriority()+1, OverlapPriority);
	}

	if (bAllowMultipleRows)
	{
		while (OverlapsWithSections(Sections) != nullptr)
		{
			++RowIndex;
		}
	}
	else
	{
		for (;;)
		{
			const UMovieSceneSection* OverlappedSection = OverlapsWithSections(Sections);
			if (OverlappedSection == nullptr)
			{
				break;
			}

			TRange<FFrameNumber> OtherRange = OverlappedSection->GetRange();
			if (OtherRange.GetUpperBound().IsClosed())
			{
				MoveSection(OtherRange.GetUpperBoundValue() - InStartTime);
			}
			else
			{
				++OverlapPriority;
				break;
			}
		}
	}

	UMovieSceneTrack* Track = GetTypedOuter<UMovieSceneTrack>();
	if (Track)
	{
		Track->UpdateEasing();
	}
}

void UMovieSceneSection::InitialPlacementOnRow(const TArray<UMovieSceneSection*>& Sections, FFrameNumber InStartTime, int32 Duration, int32 InRowIndex)
{
	check(Duration >= 0);

	// Inclusive lower, exclusive upper bounds
	SectionRange = TRange<FFrameNumber>(InStartTime, InStartTime + Duration);
	RowIndex = InRowIndex;

	// If no given row index, put it on the next available row
	if (RowIndex == INDEX_NONE)
	{
		RowIndex = 0;
		while (OverlapsWithSections(Sections) != nullptr)
		{
			++RowIndex;
		}
	}

	for (UMovieSceneSection* OtherSection : Sections)
	{
		OverlapPriority = FMath::Max(OtherSection->GetOverlapPriority()+1, OverlapPriority);
	}

	// If this overlaps with any sections, move out all the sections that are beyond this row
	if (OverlapsWithSections(Sections))
	{
		for (UMovieSceneSection* OtherSection : Sections)
		{
			if (OtherSection != nullptr && OtherSection != this && OtherSection->GetRowIndex() >= RowIndex)
			{
				OtherSection->SetRowIndex(OtherSection->GetRowIndex()+1);
			}
		}
	}

	UMovieSceneTrack* Track = GetTypedOuter<UMovieSceneTrack>();
	if (Track)
	{
		Track->UpdateEasing();
	}
}

UMovieSceneSection* UMovieSceneSection::SplitSection(FQualifiedFrameTime SplitTime)
{
	if (!SectionRange.Value.Contains(SplitTime.Time.GetFrame()))
	{
		return nullptr;
	}

	SetFlags(RF_Transactional);

	if (TryModify())
	{
		TRange<FFrameNumber> StartingRange  = SectionRange.Value;
		TRange<FFrameNumber> LeftHandRange  = TRange<FFrameNumber>(StartingRange.GetLowerBound(), TRangeBound<FFrameNumber>::Exclusive(SplitTime.Time.GetFrame()));
		TRange<FFrameNumber> RightHandRange = TRange<FFrameNumber>(TRangeBound<FFrameNumber>::Inclusive(SplitTime.Time.GetFrame()), StartingRange.GetUpperBound());

		// Trim off the right
		SectionRange = LeftHandRange;

		// Create a new section
		UMovieSceneTrack* Track = CastChecked<UMovieSceneTrack>(GetOuter());
		Track->Modify();

		UMovieSceneSection* NewSection = DuplicateObject<UMovieSceneSection>(this, Track);
		check(NewSection);

		NewSection->SetRange(RightHandRange);
		Track->AddSection(*NewSection);

		return NewSection;
	}

	return nullptr;
}


void UMovieSceneSection::TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft)
{
	if (SectionRange.Value.Contains(TrimTime.Time.GetFrame()))
	{
		SetFlags(RF_Transactional);
		if (TryModify())
		{
			if (bTrimLeft)
			{
				SectionRange.Value.SetLowerBound(TRangeBound<FFrameNumber>::Inclusive(TrimTime.Time.GetFrame()));
			}
			else
			{
				SectionRange.Value.SetUpperBound(TRangeBound<FFrameNumber>::Exclusive(TrimTime.Time.GetFrame()));
			}
		}
	}
}


FMovieSceneEvalTemplatePtr UMovieSceneSection::GenerateTemplate() const
{
	return FMovieSceneEvalTemplatePtr();
}


float UMovieSceneSection::EvaluateEasing(FFrameTime InTime) const
{
	float EaseInValue = 1.f;
	float EaseOutValue = 1.f;

	if (HasStartFrame() && Easing.GetEaseInDuration() > 0 && Easing.EaseIn.GetObject())
	{
		const int32  EaseFrame    = (InTime.FrameNumber - GetInclusiveStartFrame()).Value;
		const double EaseInInterp = (double(EaseFrame) + InTime.GetSubFrame()) / Easing.GetEaseInDuration();

		if (EaseInInterp <= 0.0)
		{
			EaseInValue = 0.0;
		}
		else if (EaseInInterp >= 1.0)
		{
			EaseInValue = 1.0;
		}
		else
		{
			EaseInValue = IMovieSceneEasingFunction::EvaluateWith(Easing.EaseIn, EaseInInterp);
		}
	}


	if (HasEndFrame() && Easing.GetEaseOutDuration() > 0 && Easing.EaseOut.GetObject())
	{
		const int32  EaseFrame     = (InTime.FrameNumber - GetExclusiveEndFrame() + Easing.GetEaseOutDuration()).Value;
		const double EaseOutInterp = (double(EaseFrame) + InTime.GetSubFrame()) / Easing.GetEaseOutDuration();

		if (EaseOutInterp <= 0.0)
		{
			EaseOutValue = 1.0;
		}
		else if (EaseOutInterp >= 1.0)
		{
			EaseOutValue = 0.0;
		}
		else
		{
			EaseOutValue = 1.f - IMovieSceneEasingFunction::EvaluateWith(Easing.EaseOut, EaseOutInterp);
		}
	}

	return EaseInValue * EaseOutValue;
}


void UMovieSceneSection::EvaluateEasing(FFrameTime InTime, TOptional<float>& OutEaseInValue, TOptional<float>& OutEaseOutValue, float* OutEaseInInterp, float* OutEaseOutInterp) const
{
	if (HasStartFrame() && Easing.EaseIn.GetObject() && GetEaseInRange().Contains(InTime.FrameNumber))
	{
		const int32  EaseFrame    = (InTime.FrameNumber - GetInclusiveStartFrame()).Value;
		const double EaseInInterp = (double(EaseFrame) + InTime.GetSubFrame()) / Easing.GetEaseInDuration();

		OutEaseInValue = IMovieSceneEasingFunction::EvaluateWith(Easing.EaseIn, EaseInInterp);

		if (OutEaseInInterp)
		{
			*OutEaseInInterp = EaseInInterp;
		}
	}

	if (HasEndFrame() && Easing.EaseOut.GetObject() && GetEaseOutRange().Contains(InTime.FrameNumber))
	{
		const int32  EaseFrame     = (InTime.FrameNumber - GetExclusiveEndFrame() + Easing.GetEaseOutDuration()).Value;
		const double EaseOutInterp = (double(EaseFrame) + InTime.GetSubFrame()) / Easing.GetEaseOutDuration();

		OutEaseOutValue = 1.f - IMovieSceneEasingFunction::EvaluateWith(Easing.EaseOut, EaseOutInterp);

		if (OutEaseOutInterp)
		{
			*OutEaseOutInterp = EaseOutInterp;
		}
	}
}


TRange<FFrameNumber> UMovieSceneSection::GetEaseInRange() const
{
	if (HasStartFrame() && Easing.GetEaseInDuration() > 0)
	{
		TRangeBound<FFrameNumber> LowerBound = TRangeBound<FFrameNumber>::Inclusive(GetInclusiveStartFrame());
		TRangeBound<FFrameNumber> UpperBound = TRangeBound<FFrameNumber>::Exclusive(GetInclusiveStartFrame() + Easing.GetEaseInDuration());

		UpperBound = TRangeBound<FFrameNumber>::MinUpper(UpperBound, SectionRange.Value.GetUpperBound());
		return TRange<FFrameNumber>(LowerBound, UpperBound);
	}

	return TRange<FFrameNumber>::Empty();
}


TRange<FFrameNumber> UMovieSceneSection::GetEaseOutRange() const
{
	if (HasEndFrame() && Easing.GetEaseOutDuration() > 0)
	{
		TRangeBound<FFrameNumber> UpperBound = TRangeBound<FFrameNumber>::Exclusive(GetExclusiveEndFrame());
		TRangeBound<FFrameNumber> LowerBound = TRangeBound<FFrameNumber>::Inclusive(GetExclusiveEndFrame() - Easing.GetEaseOutDuration());

		LowerBound = TRangeBound<FFrameNumber>::MaxLower(LowerBound, SectionRange.Value.GetLowerBound());
		return TRange<FFrameNumber>(LowerBound, UpperBound);
	}

	return TRange<FFrameNumber>::Empty();
}