// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameTime.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneFwd.h"
#include "KeyParams.h"
#include "MovieScene.h"
#include "MovieSceneSignedObject.h"
#include "Evaluation/Blending/MovieSceneBlendType.h"
#include "Generators/MovieSceneEasingFunction.h"
#include "MovieSceneFrameMigration.h"
#include "Misc/QualifiedFrameTime.h"
#include "MovieSceneSection.generated.h"

class FStructOnScope;

struct FKeyHandle;
struct FMovieSceneChannelProxy;
struct FMovieSceneEvalTemplatePtr;

template<typename> class TArrayView;

/** Enumeration specifying how to handle state when this section is no longer evaluated */
UENUM()
enum class EMovieSceneCompletionMode : uint8
{
	KeepState,

	RestoreState,

	ProjectDefault,
};


USTRUCT()
struct FMovieSceneSectionEvalOptions
{
	GENERATED_BODY()
	
	FMovieSceneSectionEvalOptions()
		: bCanEditCompletionMode(false)
		, CompletionMode(EMovieSceneCompletionMode::KeepState)
	{}

	void EnableAndSetCompletionMode(EMovieSceneCompletionMode NewCompletionMode)
	{
		bCanEditCompletionMode = true;
		CompletionMode = NewCompletionMode;
	}

	UPROPERTY()
	bool bCanEditCompletionMode;

	/** When set to "RestoreState", this section will restore any animation back to its previous state  */
	UPROPERTY(EditAnywhere, DisplayName="When Finished", Category="Section")
	EMovieSceneCompletionMode CompletionMode;
};

USTRUCT()
struct FMovieSceneEasingSettings
{
	GENERATED_BODY()

	FMovieSceneEasingSettings()
		: AutoEaseInDuration(0), AutoEaseOutDuration(0)
		, EaseIn(nullptr), bManualEaseIn(false), ManualEaseInDuration(0)
		, EaseOut(nullptr), bManualEaseOut(false), ManualEaseOutDuration(0)
#if WITH_EDITORONLY_DATA
		, AutoEaseInTime_DEPRECATED(0.f), AutoEaseOutTime_DEPRECATED(0.f), ManualEaseInTime_DEPRECATED(0.f), ManualEaseOutTime_DEPRECATED(0.f)
#endif
	{}

public:

	int32 GetEaseInDuration() const
	{
		return bManualEaseIn ? ManualEaseInDuration : AutoEaseInDuration;
	}

	int32 GetEaseOutDuration() const
	{
		return bManualEaseOut ? ManualEaseOutDuration : AutoEaseOutDuration;
	}

public:

	/** Automatically applied ease in duration in frames */
	UPROPERTY()
	int32 AutoEaseInDuration;

	/** Automatically applied ease out time */
	UPROPERTY()
	int32 AutoEaseOutDuration;

	UPROPERTY()
	TScriptInterface<IMovieSceneEasingFunction> EaseIn;

	/** Whether to manually override this section's ease in time */
	UPROPERTY()
	bool bManualEaseIn;

	/** Manually override this section's ease in duration in frames */
	UPROPERTY()
	int32 ManualEaseInDuration;

	UPROPERTY()
	TScriptInterface<IMovieSceneEasingFunction> EaseOut;

	/** Whether to manually override this section's ease out time */
	UPROPERTY()
	bool bManualEaseOut;

	/** Manually override this section's ease-out duration in frames */
	UPROPERTY()
	int32 ManualEaseOutDuration;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	float AutoEaseInTime_DEPRECATED;
	UPROPERTY()
	float AutoEaseOutTime_DEPRECATED;
	UPROPERTY()
	float ManualEaseInTime_DEPRECATED;
	UPROPERTY()
	float ManualEaseOutTime_DEPRECATED;
#endif
};

/**
 * Base class for movie scene sections
 */
UCLASS(abstract, DefaultToInstanced, MinimalAPI, BlueprintType)
class UMovieSceneSection
	: public UMovieSceneSignedObject
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, Category="Section", meta=(ShowOnlyInnerProperties))
	FMovieSceneSectionEvalOptions EvalOptions;

public:

	/**
	 * Calls Modify if this section can be modified, i.e. can't be modified if it's locked
	 *
	 * @return Returns whether this section is locked or not
	 */
	MOVIESCENE_API bool TryModify(bool bAlwaysMarkDirty=true);

	/*
	 * A section is read only if it or its outer movie are read only
	 * 
	 * @return Returns whether this section is read only
	 */
	MOVIESCENE_API bool IsReadOnly() const;

	/**
	 * @return The range of times of the section
	 */
	TRange<FFrameNumber> GetRange() const
	{
		return SectionRange.Value;
	}

	/**
	 * A true representation of this section's range with an inclusive start frame and an exclusive end frame.
	 * The resulting range defines that the section lies between { lower <= time < upper }
	 */
	TRange<FFrameNumber> GetTrueRange() const
	{
		TRangeBound<FFrameNumber> SectionLower = SectionRange.Value.GetLowerBound();
		TRangeBound<FFrameNumber> SectionUpper = SectionRange.Value.GetUpperBound();

		// Make exclusive lower bounds inclusive on the next frame
		if (SectionLower.IsExclusive())
		{
			SectionLower = TRangeBound<FFrameNumber>::Inclusive(SectionLower.GetValue() + 1);
		}
		// Make inclusive upper bounds exclusive on the next frame
		if (SectionUpper.IsInclusive())
		{
			SectionUpper = TRangeBound<FFrameNumber>::Exclusive(SectionUpper.GetValue() + 1);
		}
		return TRange<FFrameNumber>(SectionLower, SectionUpper);
	}

	/**
	 * Expands this section's range to include the specified time
	 */
	void ExpandToFrame(FFrameNumber InFrame)
	{
		SetRange(TRange<FFrameNumber>::Hull(GetRange(), TRange<FFrameNumber>::Inclusive(InFrame, InFrame)));
	}

	/**
	 * Sets a new range of times for this section
	 * 
	 * @param NewRange	The new range of times
	 */
	void SetRange(const TRange<FFrameNumber>& NewRange)
	{
		// Do not modify for objects that still need initialization (i.e. we're in the object's constructor)
		bool bCanSetRange = HasAnyFlags(RF_NeedInitialization) || TryModify();
		if (bCanSetRange)
		{
			check(NewRange.GetLowerBound().IsOpen() || NewRange.GetUpperBound().IsOpen() || NewRange.GetLowerBoundValue() <= NewRange.GetUpperBoundValue());
			SectionRange.Value = NewRange;
		}
	}

	/**
	 * Check whether this section has a start frame (else infinite)
	 * @return true if this section has an inclusive or exclusive start frame, false if it's open (infinite)
	 */
	bool HasStartFrame() const
	{
		return !SectionRange.Value.GetLowerBound().IsOpen();
	}

	/**
	 * Check whether this section has an end frame (else infinite)
	 * @return true if this section has an inclusive or exclusive end frame, false if it's open (infinite)
	 */
	bool HasEndFrame() const
	{
		return !SectionRange.Value.GetUpperBound().IsOpen();
	}

	/**
	 * Gets the frame number at which this section starts
	 *
	 * @note Assumes a non-infinite start time. Check HasStartFrame first.
	 * @return The frame number at which this section starts.
	 */
	FFrameNumber GetInclusiveStartFrame() const
	{
		TRangeBound<FFrameNumber> LowerBound = SectionRange.GetLowerBound();
		return LowerBound.IsInclusive() ? LowerBound.GetValue() : LowerBound.GetValue() + 1;
	}

	/**
	 * Gets the first frame number after the end of this section
	 *
	 * @note Assumes a non-infinite end time. Check HasEndFrame first.
	 * @return The first frame after this section ends
	 */
	FFrameNumber GetExclusiveEndFrame() const
	{
		TRangeBound<FFrameNumber> UpperBound = SectionRange.GetUpperBound();
		return UpperBound.IsInclusive() ? UpperBound.GetValue() + 1 : UpperBound.GetValue();
	}

	/**
	 * Set this section's start frame in sequence resolution space.
	 * @note: Will be clamped to the current end frame if necessary
	 */
	MOVIESCENE_API void SetStartFrame(TRangeBound<FFrameNumber> NewStartFrame);

	/**
	 * Set this section's end frame in sequence resolution space
	 * @note: Will be clamped to the current start frame if necessary
	 */
	MOVIESCENE_API void SetEndFrame(TRangeBound<FFrameNumber> NewEndFrame);

	/**
	 * Returns whether or not a provided position in time is within the timespan of the section 
	 *
	 * @param Position	The position to check
	 * @return true if the position is within the timespan, false otherwise
	 */
	bool IsTimeWithinSection(FFrameNumber Position) const 
	{
		return SectionRange.Value.Contains(Position);
	}

	/*
	 * Returns the range to auto size this section to, if there is one. This defaults to the 
	 * range of all the keys.
	 *
	 * @return the range of this section to auto size to
	 */
	MOVIESCENE_API virtual TOptional<TRange<FFrameNumber> > GetAutoSizeRange() const;

	/**
	 * Gets this section's blend type
	 */
	FOptionalMovieSceneBlendType GetBlendType() const
	{
		return BlendType;
	}

	/**
	 * Sets this section's blend type
	 */
	MOVIESCENE_API virtual void SetBlendType(EMovieSceneBlendType InBlendType)
	{
		if (GetSupportedBlendTypes().Contains(InBlendType))
		{
			BlendType = InBlendType;
		}
	}

	/**
	 * Gets what kind of blending is supported by this section
	 */
	MOVIESCENE_API FMovieSceneBlendTypeField GetSupportedBlendTypes() const;

	/**
	 * Moves the section by a specific amount of time
	 *
	 * @param DeltaTime	The distance in time to move the curve
	 */
	MOVIESCENE_API void MoveSection(FFrameNumber DeltaTime);

	/**
	 * Return the range within which this section is effective. Used for automatic calculation of sequence bounds.
	 *
	 * @return the range within which this section is effective
	 */
	MOVIESCENE_API TRange<FFrameNumber> ComputeEffectiveRange() const;

	/**
	 * Split a section in two at the split time
	 *
	 * @param SplitTime The time at which to split
	 * @return The newly created split section
	 */
	MOVIESCENE_API virtual UMovieSceneSection* SplitSection(FQualifiedFrameTime SplitTime);

	/**
	 * Trim a section at the trim time
	 *
	 * @param TrimTime The time at which to trim
	 * @param bTrimLeft Whether to trim left or right
	 */
	MOVIESCENE_API virtual void TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft);

	/**
	 * Get the data structure representing the specified keys.
	 *
	 * @param KeyHandles The handles of the keys.
	 * @return The keys' data structure representation, or nullptr if key not found or no structure available.
	 */
	MOVIESCENE_API virtual TSharedPtr<FStructOnScope> GetKeyStruct(TArrayView<const FKeyHandle> KeyHandles);

	/**
	 * Generate an evaluation template for this section
	 * @return a valid evaluation template ptr, or nullptr
	 */
	MOVIESCENE_API virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const;

	/**
	 * Gets all snap times for this section
	 *
	 * @param OutSnapTimes The array of times we will to output
	 * @param bGetSectionBorders Gets the section borders in addition to any custom snap times
	 */
	virtual void GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const
	{
		if (bGetSectionBorders)
		{
			if (SectionRange.Value.GetLowerBound().IsClosed())
			{
				OutSnapTimes.Add(SectionRange.Value.GetLowerBoundValue());
			}

			if (SectionRange.Value.GetUpperBound().IsClosed())
			{
				OutSnapTimes.Add(SectionRange.Value.GetUpperBoundValue());
			}
		}
	}

	/** Sets this section's new row index */
	UFUNCTION(BlueprintCallable, Category = "Movie Scene Section")
	void SetRowIndex(int32 NewRowIndex) {RowIndex = NewRowIndex;}

	/** Gets the row index for this section */
	UFUNCTION(BlueprintPure, Category = "Movie Scene Section")
	int32 GetRowIndex() const { return RowIndex; }
	
	/** Sets this section's priority over overlapping sections (higher wins) */
	UFUNCTION(BlueprintCallable, Category = "Movie Scene Section")
	void SetOverlapPriority(int32 NewPriority)
	{
		OverlapPriority = NewPriority;
	}

	/** Gets this section's priority over overlapping sections (higher wins) */
	UFUNCTION(BlueprintPure, Category = "Movie Scene Section")
	int32 GetOverlapPriority() const
	{
		return OverlapPriority;
	}

	/**
	 * Checks to see if this section overlaps with an array of other sections
	 * given an optional time and track delta.
	 *
	 * @param Sections Section array to check against.
	 * @param TrackDelta Optional offset to this section's track index.
	 * @param TimeDelta Optional offset to this section's time delta.
	 * @return The first section that overlaps, or null if there is no overlap.
	 */
	virtual MOVIESCENE_API const UMovieSceneSection* OverlapsWithSections(const TArray<UMovieSceneSection*>& Sections, int32 TrackDelta = 0, int32 TimeDelta = 0) const;
	
	/**
	 * Places this section at the first valid row at the specified time. Good for placement upon creation.
	 *
	 * @param Sections Sections that we can not overlap with.
	 * @param InStartTime The new start time.
	 * @param InDuration The duration.
	 * @param bAllowMultipleRows If false, it will move the section in the time direction to make it fit, rather than the row direction.
	 */
	virtual MOVIESCENE_API void InitialPlacement(const TArray<UMovieSceneSection*>& Sections, FFrameNumber InStartTime, int32 InDuration, bool bAllowMultipleRows);

	/**
	 * Places this section at the specified row at the specified time. Overlapping sections will be moved down a row. Good for placement upon creation.
	 *
	 * @param Sections Sections that we can not overlap with.
	 * @param InStartTime The new start time.
	 * @param InDuration The duration.
	 * @param InRowIndex The row index to place this section on.
	 */
	virtual MOVIESCENE_API void InitialPlacementOnRow(const TArray<UMovieSceneSection*>& Sections, FFrameNumber InStartTime, int32 InDuration, int32 InRowIndex);

	/** Whether or not this section is active. */
	UFUNCTION(BlueprintCallable, Category = "Movie Scene Section")
	void SetIsActive(bool bInIsActive) { bIsActive = bInIsActive; }
	UFUNCTION(BlueprintPure, Category = "Movie Scene Section")
	bool IsActive() const { return bIsActive; }

	/** Whether or not this section is locked. */
	UFUNCTION(BlueprintCallable, Category = "Movie Scene Section")
	void SetIsLocked(bool bInIsLocked) { bIsLocked = bInIsLocked; }
	UFUNCTION(BlueprintPure, Category = "Movie Scene Section")
	bool IsLocked() const { return bIsLocked; }

	/** Gets the number of frames to prepare this section for evaluation before it actually starts. */
	UFUNCTION(BlueprintCallable, Category = "Movie Scene Section")
	void SetPreRollFrames(int32 InPreRollFrames) { if (TryModify()) { PreRollFrames = InPreRollFrames; } }
	UFUNCTION(BlueprintPure, Category = "Movie Scene Section")
	int32 GetPreRollFrames() const { return PreRollFrames.Value; }

	/** Gets/sets the number of frames to continue 'postrolling' this section for after evaluation has ended. */
	UFUNCTION(BlueprintCallable, Category = "Movie Scene Section")
	void SetPostRollFrames(int32 InPostRollFrames) { if (TryModify()) { PostRollFrames = InPostRollFrames; } }
	UFUNCTION(BlueprintPure, Category = "Movie Scene Section")
	int32 GetPostRollFrames() const { return PostRollFrames.Value; }

	/** The optional offset time of this section */
	virtual TOptional<FFrameTime> GetOffsetTime() const { return TOptional<FFrameTime>(); }

	/**
	 * When guid bindings are updated to allow this section to fix-up any internal bindings
	 *
	 */
	virtual void OnBindingsUpdated(const TMap<FGuid, FGuid>& OldGuidToNewGuidMap) { }

	/** Get the referenced bindings for this section */
	MOVIESCENE_API virtual void GetReferencedBindings(TArray<FGuid>& OutBindings) {}

	/**
	 * Gets a list of all overlapping sections
	 */
	MOVIESCENE_API void GetOverlappingSections(TArray<UMovieSceneSection*>& OutSections, bool bSameRow, bool bIncludeThis);

	/**
	 * Evaluate this sections's easing functions based on the specified time
	 */
	MOVIESCENE_API float EvaluateEasing(FFrameTime InTime) const;

	/**
	 * Evaluate this sections's easing functions based on the specified time
	 */
	MOVIESCENE_API void EvaluateEasing(FFrameTime InTime, TOptional<float>& OutEaseInValue, TOptional<float>& OutEaseOutValue, float* OutEaseInInterp, float* OutEaseOutInterp) const;

	MOVIESCENE_API TRange<FFrameNumber> GetEaseInRange() const;

	MOVIESCENE_API TRange<FFrameNumber> GetEaseOutRange() const;

	/**
	 * Access this section's channel proxy, containing pointers to all existing data channels in this section
	 * @note: Proxy can be reallocated at any time; this accessor is only for immediate use.
	 *
	 * @return A reference to this section's channel proxy.
	 */
	MOVIESCENE_API FMovieSceneChannelProxy& GetChannelProxy() const;

	/** Does this movie section support infinite ranges for evaluation */
	MOVIESCENE_API bool GetSupportsInfiniteRange() const { return bSupportsInfiniteRange; }

	/**
	*  Whether or not we draw a curve for a particular channel owned by this section.
	*  Defaults to true.
	*/
	MOVIESCENE_API virtual bool ShowCurveForChannel(const void *Channel) const  { return true; }

	/** 
	*  Get The Total Weight Value for this Section
	*  For Most Sections it's just the Ease Value, but for some Sections also have an extra Weight Curve
	*/
	MOVIESCENE_API virtual float GetTotalWeightValue(FFrameTime InTime) const { return EvaluateEasing(InTime); }

protected:

	//~ UObject interface
	MOVIESCENE_API virtual void PostInitProperties() override;
	MOVIESCENE_API virtual bool IsPostLoadThreadSafe() const override;
	MOVIESCENE_API virtual void Serialize(FArchive& Ar) override;

	virtual void OnMoved(int32 DeltaTime) {}
	virtual void OnDilated(float DilationFactor, FFrameNumber Origin) {}

public:

	UPROPERTY(EditAnywhere, Category="Easing", meta=(ShowOnlyInnerProperties))
	FMovieSceneEasingSettings Easing;

	/** The range in which this section is active */
	UPROPERTY(EditAnywhere, Category="Section")
	FMovieSceneFrameRange SectionRange;

#if WITH_EDITORONLY_DATA
	/** The timecode at which this movie scene section is based (ie. when it was recorded) */
	UPROPERTY(EditAnywhere, Category="Section")
	FMovieSceneTimecodeSource TimecodeSource;
#endif

private:

	/** The amount of time to prepare this section for evaluation before it actually starts. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Section", meta=(UIMin=0))
	FFrameNumber PreRollFrames;

	/** The amount of time to continue 'postrolling' this section for after evaluation has ended. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Section", meta=(UIMin=0))
	FFrameNumber PostRollFrames;

	/** The row index that this section sits on */
	UPROPERTY()
	int32 RowIndex;

	/** This section's priority over overlapping sections */
	UPROPERTY()
	int32 OverlapPriority;

	/** Toggle whether this section is active/inactive */
	UPROPERTY(EditAnywhere, Category="Section")
	uint32 bIsActive : 1;

	/** Toggle whether this section is locked/unlocked */
	UPROPERTY(EditAnywhere, Category="Section")
	uint32 bIsLocked : 1;

protected:

	/** The start time of the section */
	UPROPERTY()
	float StartTime_DEPRECATED;

	/** The end time of the section */
	UPROPERTY()
	float EndTime_DEPRECATED;

	/** The amount of time to prepare this section for evaluation before it actually starts. */
	UPROPERTY()
	float PreRollTime_DEPRECATED;

	/** The amount of time to continue 'postrolling' this section for after evaluation has ended. */
	UPROPERTY()
	float PostRollTime_DEPRECATED;

	/** Toggle to set this section to be infinite */
	UPROPERTY()
	uint32 bIsInfinite_DEPRECATED : 1;

protected:
	/** Does this section support infinite ranges in the track editor? */
	UPROPERTY()
	bool bSupportsInfiniteRange;

	UPROPERTY()
	FOptionalMovieSceneBlendType BlendType;

	/**
	 * Channel proxy that contains all the channels in this section - must be populated and invalidated by derived types.
	 * Must be re-allocated any time any channel pointer in derived types is reallocated (such as channel data stored in arrays)
	 * to ensure that any weak handles to channels are invalidated correctly. Allocation is via MakeShared<FMovieSceneChannelProxy>().
	 */
	TSharedPtr<FMovieSceneChannelProxy> ChannelProxy;
};
