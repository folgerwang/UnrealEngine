// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Algo/Sort.h"

UMovieScenePropertyTrack::UMovieScenePropertyTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
}


void UMovieScenePropertyTrack::SetPropertyNameAndPath(FName InPropertyName, const FString& InPropertyPath)
{
	check((InPropertyName != NAME_None) && !InPropertyPath.IsEmpty());

	PropertyName = InPropertyName;
	PropertyPath = InPropertyPath;
	
#if WITH_EDITORONLY_DATA
	if (UniqueTrackName == NAME_None)
	{
		UniqueTrackName = *PropertyPath;
	}
#endif
}


const TArray<UMovieSceneSection*>& UMovieScenePropertyTrack::GetAllSections() const
{
	return Sections;
}


void UMovieScenePropertyTrack::PostLoad()
{
#if WITH_EDITORONLY_DATA
	if (UniqueTrackName.IsNone())
	{
		UniqueTrackName = *PropertyPath;
	}
#endif

	Super::PostLoad();
}

#if WITH_EDITORONLY_DATA
FText UMovieScenePropertyTrack::GetDefaultDisplayName() const
{
	return FText::FromName(PropertyName);
}

FName UMovieScenePropertyTrack::GetTrackName() const
{
	return UniqueTrackName;
}
#endif

void UMovieScenePropertyTrack::RemoveAllAnimationData()
{
	Sections.Empty();
	SectionToKey = nullptr;
}


bool UMovieScenePropertyTrack::HasSection(const UMovieSceneSection& Section) const 
{
	return Sections.Contains(&Section);
}


void UMovieScenePropertyTrack::AddSection(UMovieSceneSection& Section) 
{
	Sections.Add(&Section);
}


void UMovieScenePropertyTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
	if (SectionToKey == &Section)
	{
		if (Sections.Num() > 0)
		{
			SectionToKey = Sections[0];
		}
		else
		{
			SectionToKey = nullptr;
		}
	}
}


bool UMovieScenePropertyTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}


TArray<UMovieSceneSection*, TInlineAllocator<4>> UMovieScenePropertyTrack::FindAllSections(FFrameNumber Time)
{
	TArray<UMovieSceneSection*, TInlineAllocator<4>> OverlappingSections;

	for (UMovieSceneSection* Section : Sections)
	{
		if (Section->GetRange().Contains(Time))
		{
			OverlappingSections.Add(Section);
		}
	}

	Algo::Sort(OverlappingSections, MovieSceneHelpers::SortOverlappingSections);

	return OverlappingSections;
}


UMovieSceneSection* UMovieScenePropertyTrack::FindSection(FFrameNumber Time)
{
	TArray<UMovieSceneSection*, TInlineAllocator<4>> OverlappingSections = FindAllSections(Time);

	if (OverlappingSections.Num())
	{
		if (SectionToKey && OverlappingSections.Contains(SectionToKey))
		{
			return SectionToKey;
		}
		else
		{
			return OverlappingSections[0];
		}
	}

	return nullptr;
}


UMovieSceneSection* UMovieScenePropertyTrack::FindOrExtendSection(FFrameNumber Time, float& Weight)
{
	Weight = 1.0f;
	TArray<UMovieSceneSection*, TInlineAllocator<4>> OverlappingSections = FindAllSections(Time);
	if (SectionToKey)
	{
		bool bCalculateWeight = false;
		if (SectionToKey && !OverlappingSections.Contains(SectionToKey))
		{
			if (SectionToKey->HasEndFrame() && SectionToKey->GetExclusiveEndFrame() < Time)
			{
				SectionToKey->SetEndFrame(Time);
			}
			else
			{
				SectionToKey->SetStartFrame(Time);
			}
			if (OverlappingSections.Num() > 0)
			{
				bCalculateWeight = true;
			}
		}
		else
		{
			if (OverlappingSections.Num() > 1)
			{
				bCalculateWeight = true;
			}
		}
		//we need to calculate weight also possibly
		FOptionalMovieSceneBlendType BlendType = SectionToKey->GetBlendType();
		if (bCalculateWeight)
		{
			Weight = MovieSceneHelpers::CalculateWeightForBlending(SectionToKey, Time);
		}
		return SectionToKey;
	}
	else
	{
		if (OverlappingSections.Num() > 0)
		{
			return OverlappingSections[0];
		}
	}

	// Find a spot for the section so that they are sorted by start time
	for(int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		UMovieSceneSection* Section = Sections[SectionIndex];

		// Check if there are no more sections that would overlap the time 
		if(!Sections.IsValidIndex(SectionIndex+1) || (Sections[SectionIndex+1]->HasEndFrame() && Sections[SectionIndex+1]->GetExclusiveEndFrame() > Time))
		{
			// No sections overlap the time
		
			if(SectionIndex > 0)
			{
				// Append and grow the previous section
				UMovieSceneSection* PreviousSection = Sections[ SectionIndex ? SectionIndex-1 : 0 ];
		
				PreviousSection->SetEndFrame(Time);
				return PreviousSection;
			}
			else if(Sections.IsValidIndex(SectionIndex+1))
			{
				// Prepend and grow the next section because there are no sections before this one
				UMovieSceneSection* NextSection = Sections[SectionIndex+1];
				NextSection->SetStartFrame(Time);
				return NextSection;
			}	
			else
			{
				// SectionIndex == 0 
				UMovieSceneSection* PreviousSection = Sections[0];
				if(PreviousSection->HasEndFrame() && PreviousSection->GetExclusiveEndFrame() < Time)
				{
					// Append and grow the section
					PreviousSection->SetEndFrame(Time);
				}
				else
				{
					// Prepend and grow the section
					PreviousSection->SetStartFrame(Time);
				}
				return PreviousSection;
			}
		}
	}

	return nullptr;
}

UMovieSceneSection* UMovieScenePropertyTrack::FindOrAddSection(FFrameNumber Time, bool& bSectionAdded)
{
	bSectionAdded = false;

	UMovieSceneSection* FoundSection = FindSection(Time);
	if (FoundSection)
	{
		return FoundSection;
	}

	// Add a new section that starts and ends at the same time
	UMovieSceneSection* NewSection = CreateNewSection();
	ensureAlwaysMsgf(NewSection->HasAnyFlags(RF_Transactional), TEXT("CreateNewSection must return an instance with RF_Transactional set! (pass RF_Transactional to NewObject)"));
	NewSection->SetFlags(RF_Transactional);
	NewSection->SetRange(TRange<FFrameNumber>::Inclusive(Time, Time));

	Sections.Add(NewSection);
	
	bSectionAdded = true;

	return NewSection;
}

void UMovieScenePropertyTrack::SetSectionToKey(UMovieSceneSection* InSection)
{
	SectionToKey = InSection;
}

UMovieSceneSection* UMovieScenePropertyTrack::GetSectionToKey()
{
	return SectionToKey;
}

