// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "FrameNumberDisplayFormat.h"
#include "SequencerSettings.generated.h"

struct FPropertyChangedEvent;
enum class EAutoChangeMode : uint8;
enum class EAllowEditsMode : uint8;
enum class EKeyGroupMode : uint8;
enum class EMovieSceneKeyInterpolation : uint8;



/** Defines visibility states for the curves in the curve editor. */
UENUM()
enum class ECurveEditorCurveVisibility : uint8
{
	/** All curves should be visible. */
	AllCurves,
	/** Only curves from selected nodes should be visible. */
	SelectedCurves,
	/** Only curves which have keyframes should be visible. */
	AnimatedCurves
};

UENUM()
enum ESequencerSpawnPosition
{
	/** Origin. */
	SSP_Origin UMETA(DisplayName="Origin"),

	/** Place in Front of Camera. */
	SSP_PlaceInFrontOfCamera UMETA(DisplayName="Place in Front of Camera"),
};

UENUM()
enum ESequencerZoomPosition
{
	/** Current Time. */
	SZP_CurrentTime UMETA(DisplayName="Current Time"),

	/** Mouse Position. */
	SZP_MousePosition UMETA(DisplayName="Mouse Position"),
};

UENUM()
enum ESequencerLoopMode
{
	/** No Looping. */
	SLM_NoLoop UMETA(DisplayName="No Looping"),

	/** Loop Playback Range. */
	SLM_Loop UMETA(DisplayName="Loop"),

	/** Loop Selection Range. */
	SLM_LoopSelectionRange UMETA(DisplayName="Loop Selection Range"),
};

/** Empty class used to house multiple named USequencerSettings */
UCLASS()
class SEQUENCER_API USequencerSettingsContainer
	: public UObject
{
public:
	GENERATED_BODY()

	/** Get or create a settings object for the specified name */
	template<class T> 
	static T* GetOrCreate(const TCHAR* InName)
	{
		static const TCHAR* SettingsContainerName = TEXT("SequencerSettingsContainer");

		auto* Outer = FindObject<USequencerSettingsContainer>(GetTransientPackage(), SettingsContainerName);
		if (!Outer)
		{
			Outer = NewObject<USequencerSettingsContainer>(GetTransientPackage(), USequencerSettingsContainer::StaticClass(), SettingsContainerName);
			Outer->AddToRoot();
		}
	
		T* Inst = FindObject<T>( Outer, InName );
		if (!Inst)
		{
			Inst = NewObject<T>( Outer, T::StaticClass(), InName );
			Inst->LoadConfig();
		}

		return Inst;
	}
};


/** Serializable options for sequencer. */
UCLASS(config=EditorPerProjectUserSettings, PerObjectConfig)
class SEQUENCER_API USequencerSettings
	: public UObject
{
public:
	GENERATED_UCLASS_BODY()

	DECLARE_MULTICAST_DELEGATE( FOnEvaluateSubSequencesInIsolationChanged );
	DECLARE_MULTICAST_DELEGATE( FOnShowSelectedNodesOnlyChanged );
	DECLARE_MULTICAST_DELEGATE_OneParam( FOnAllowEditsModeChanged, EAllowEditsMode );
	DECLARE_MULTICAST_DELEGATE( FOnCurveEditorCurveVisibilityChanged );
	DECLARE_MULTICAST_DELEGATE(FOnLoopStateChanged);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Gets the current auto change mode. */
	EAutoChangeMode GetAutoChangeMode() const;
	/** Sets the current auto change mode. */
	void SetAutoChangeMode(EAutoChangeMode AutoChangeMode);

	/** Gets the current allow edits mode. */
	EAllowEditsMode GetAllowEditsMode() const;
	/** Sets the current auto-key mode. */
	void SetAllowEditsMode(EAllowEditsMode AllowEditsMode);
	/** Gets the multicast delegate which is run whenever the allow edits mode is changed. */
	FOnAllowEditsModeChanged& GetOnAllowEditsModeChanged() { return OnAllowEditsModeChangedEvent; }

	/** Returns what channels will get keyed when one channel changes */
	EKeyGroupMode GetKeyGroupMode() const;
	/** Sets which channels are keyed when a channel is keyed */
	void SetKeyGroupMode(EKeyGroupMode);

	/** Gets whether or not to key interp properties only. */
	bool GetKeyInterpPropertiesOnly() const;
	/** Sets whether or not to key interp properties only. */
	void SetKeyInterpPropertiesOnly(bool InbKeyInterpPropertiesOnly); 

	/** Gets default key interpolation. */
	EMovieSceneKeyInterpolation GetKeyInterpolation() const;
	/** Sets default key interpolation */
	void SetKeyInterpolation(EMovieSceneKeyInterpolation InKeyInterpolation);

	/** Get initial spawn position. */
	ESequencerSpawnPosition GetSpawnPosition() const;
	/** Set initial spawn position. */
	void SetSpawnPosition(ESequencerSpawnPosition InSpawnPosition);

	/** Get whether to create spawnable cameras. */
	bool GetCreateSpawnableCameras() const;
	/** Set whether to create spawnable cameras. */
	void SetCreateSpawnableCameras(bool bInCreateSpawnableCameras);

	/** Gets whether or not to show the time range slider. */
	bool GetShowRangeSlider() const;
	/** Sets whether or not to show frame numbers. */
	void SetShowRangeSlider(bool InbShowRangeSlider);

	/** Gets whether or not snapping is enabled. */
	bool GetIsSnapEnabled() const;
	/** Sets whether or not snapping is enabled. */
	void SetIsSnapEnabled(bool InbIsSnapEnabled);

	/** Gets whether or not to snap key times to the interval. */
	bool GetSnapKeyTimesToInterval() const;
	/** Sets whether or not to snap keys to the interval. */
	void SetSnapKeyTimesToInterval(bool InbSnapKeyTimesToInterval);

	/** Gets whether or not to snap keys to other keys. */
	bool GetSnapKeyTimesToKeys() const;
	/** Sets whether or not to snap keys to other keys. */
	void SetSnapKeyTimesToKeys(bool InbSnapKeyTimesToKeys);

	/** Gets whether or not to snap sections to the interval. */
	bool GetSnapSectionTimesToInterval() const;
	/** Sets whether or not to snap sections to the interval. */
	void SetSnapSectionTimesToInterval(bool InbSnapSectionTimesToInterval);

	/** Gets whether or not to snap sections to other sections. */
	bool GetSnapSectionTimesToSections() const;
	/** sets whether or not to snap sections to other sections. */
	void SetSnapSectionTimesToSections( bool InbSnapSectionTimesToSections );

	/** Gets whether or not to snap the play time to keys while scrubbing. */
	bool GetSnapPlayTimeToKeys() const;
	/** Sets whether or not to snap the play time to keys while scrubbing. */
	void SetSnapPlayTimeToKeys(bool InbSnapPlayTimeToKeys);

	/** Gets whether or not to snap the play time to the interval while scrubbing. */
	bool GetSnapPlayTimeToInterval() const;
	/** Sets whether or not to snap the play time to the interval while scrubbing. */
	void SetSnapPlayTimeToInterval(bool InbSnapPlayTimeToInterval);

	/** Gets whether or not to snap the play time to the pressed key. */
	bool GetSnapPlayTimeToPressedKey() const;
	/** Sets whether or not to snap the play time to the pressed key. */
	void SetSnapPlayTimeToPressedKey(bool InbSnapPlayTimeToPressedKey);

	/** Gets whether or not to snap the play time to the dragged key. */
	bool GetSnapPlayTimeToDraggedKey() const;
	/** Sets whether or not to snap the play time to the dragged key. */
	void SetSnapPlayTimeToDraggedKey(bool InbSnapPlayTimeToDraggedKey);

	/** Gets the snapping interval for curve values. */
	float GetCurveValueSnapInterval() const;
	/** Sets the snapping interval for curve values. */
	void SetCurveValueSnapInterval(float InCurveValueSnapInterval);

	/** Gets whether or not to snap curve values to the interval. */
	bool GetSnapCurveValueToInterval() const;
	/** Sets whether or not to snap curve values to the interval. */
	void SetSnapCurveValueToInterval(bool InbSnapCurveValueToInterval);

	/** Gets whether or not the label browser is visible. */
	bool GetLabelBrowserVisible() const;
	/** Sets whether or not the label browser is visible. */
	void SetLabelBrowserVisible(bool Visible);

	/** Gets whether or not to show selected nodes only. */
	bool GetShowSelectedNodesOnly() const;
	/** Sets whether or not to show selected nodes only. */
	void SetShowSelectedNodesOnly(bool Visible);
	FOnShowSelectedNodesOnlyChanged& GetOnShowSelectedNodesOnlyChanged() { return OnShowSelectedNodesOnlyChangedEvent; }

	/** Gets whether to jump to the start of the sequence when we start a recording or not. */
	bool ShouldRewindOnRecord() const;
	/** Sets whether to jump to the start of the sequence when we start a recording. */
	void SetRewindOnRecord(bool bInRewindOnRecord);

	/** Get zoom in/out position (mouse position or current time). */
	ESequencerZoomPosition GetZoomPosition() const;
	/** Set zoom in/out position (mouse position or current time). */
	void SetZoomPosition(ESequencerZoomPosition InZoomPosition);

	/** Gets whether or not auto-scroll is enabled. */
	bool GetAutoScrollEnabled() const;
	/** Sets whether or not auto-scroll is enabled. */
	void SetAutoScrollEnabled(bool bInAutoScrollEnabled);
	
	/** Gets whether or not to link the curve editor time range. */
	bool GetLinkCurveEditorTimeRange() const;
	/** Sets whether or not to link the curve editor time range. */
	void SetLinkCurveEditorTimeRange(bool InbLinkCurveEditorTimeRange);

	/** Gets the loop mode. */
	ESequencerLoopMode GetLoopMode() const;
	/** Sets the loop mode. */
	void SetLoopMode(ESequencerLoopMode InLoopMode);

	/** @return true if the cursor should be kept within the playback range while scrubbing in sequencer, false otherwise */
	bool ShouldKeepCursorInPlayRangeWhileScrubbing() const;
	/** Set whether or not the cursor should be kept within the playback range while scrubbing in sequencer */
	void SetKeepCursorInPlayRangeWhileScrubbing(bool bInKeepCursorInPlayRangeWhileScrubbing);

	/** @return true if the cursor should be kept within the playback range during playback in sequencer, false otherwise */
	bool ShouldKeepCursorInPlayRange() const;
	/** Set whether or not the cursor should be kept within the playback range during playback in sequencer */
	void SetKeepCursorInPlayRange(bool bInKeepCursorInPlayRange);

	/** @return true if the playback range should be synced to the section bounds, false otherwise */
	bool ShouldKeepPlayRangeInSectionBounds() const;
	/** Set whether or not the playback range should be synced to the section bounds */
	void SetKeepPlayRangeInSectionBounds(bool bInKeepPlayRangeInSectionBounds);

	/** Get the number of digits we should zero-pad to when showing frame numbers in sequencer */
	uint8 GetZeroPadFrames() const;
	/** Set the number of digits we should zero-pad to when showing frame numbers in sequencer */
	void SetZeroPadFrames(uint8 InZeroPadFrames);

	/** @return true if showing combined keyframes at the top node */
	bool GetShowCombinedKeyframes() const;
	/** Set whether to show combined keyframes at the top node */ 
	void SetShowCombinedKeyframes(bool bInShowCombinedKeyframes);

	/** @return true if key areas are infinite */
	bool GetInfiniteKeyAreas() const;
	/** Set whether to show channel colors */
	void SetInfiniteKeyAreas(bool bInInfiniteKeyAreas);

	/** @return true if showing channel colors */
	bool GetShowChannelColors() const;
	/** Set whether to show channel colors */
	void SetShowChannelColors(bool bInShowChannelColors);

	/** @return Whether to allow possession of PIE viewports */
	bool ShouldAllowPossessionOfPIEViewports() const;
	/** Toggle whether to allow possession of PIE viewports */
	void SetAllowPossessionOfPIEViewports(bool bInAllowPossessionOfPIEViewports);

	/** @return Whether to activate realtime viewports when in sequencer */
	bool ShouldActivateRealtimeViewports() const;
	/** Toggle whether to allow possession of PIE viewports */
	void SetActivateRealtimeViewports(bool bInActivateRealtimeViewports);

	/** Gets whether or not track defaults will be automatically set when modifying tracks. */
	bool GetAutoSetTrackDefaults() const;
	/** Sets whether or not track defaults will be automatically set when modifying tracks. */
	void SetAutoSetTrackDefaults(bool bInAutoSetTrackDefaults);

	/** @return Whether to show debug vis */
	bool ShouldShowDebugVisualization() const;
	/** Toggle whether to show debug vis */
	void SetShowDebugVisualization(bool bInShowDebugVisualization);

	/** @return Whether to evaluate sub sequences in isolation */
	bool ShouldEvaluateSubSequencesInIsolation() const;
	/** Set whether to evaluate sub sequences in isolation */
	void SetEvaluateSubSequencesInIsolation(bool bInEvaluateSubSequencesInIsolation);
	/** Gets the multicast delegate which is run whenever evaluate sub sequences in isolation is changed. */
	FOnEvaluateSubSequencesInIsolationChanged& GetOnEvaluateSubSequencesInIsolationChanged() { return OnEvaluateSubSequencesInIsolationChangedEvent; }

	/** @return Whether to rerun construction scripts on bound actors every frame */
	bool ShouldRerunConstructionScripts() const;
	/** Set whether to rerun construction scripts on bound actors every frame */
	void SetRerunConstructionScripts(bool bInRerunConstructionScripts);

	/** Snaps a time value in seconds to the currently selected interval. */
	float SnapTimeToInterval(float InTimeValue) const;

	/** Check whether to show pre and post roll in sequencer */
	bool ShouldShowPrePostRoll() const;
	/** Toggle whether to show pre and post roll in sequencer */
	void SetShouldShowPrePostRoll(bool bInVisualizePreAndPostRoll);

	/** Check whether whether to recompile the director blueprint when the sequence is evaluated (if one exists) */
	bool ShouldCompileDirectorOnEvaluate() const;
	/** Assign whether whether to recompile the director blueprint when the sequence is evaluated (if one exists) */
	void SetCompileDirectorOnEvaluate(bool bInCompileDirectorOnEvaluate);

	uint32 GetTrajectoryPathCap() const { return TrajectoryPathCap; }

	/** Gets the current curve visibility. */
	ECurveEditorCurveVisibility GetCurveVisibility() const;
	/** Sets the current curve visibility. */
	void SetCurveVisibility(ECurveEditorCurveVisibility InCurveVisibility);

	FOnCurveEditorCurveVisibilityChanged& GetOnCurveEditorCurveVisibilityChanged();

	FOnLoopStateChanged& GetOnLoopStateChanged();

	/** What format should we display the UI controls in when representing time in a sequence? */
	EFrameNumberDisplayFormats GetTimeDisplayFormat() const { return FrameNumberDisplayFormat; }
	/** Sets the time display format to the specified type. */
	void SetTimeDisplayFormat(EFrameNumberDisplayFormats InFormat);
protected:

	/** The auto change mode (auto-key, auto-track or none). */
	UPROPERTY( config, EditAnywhere, Category=Keyframing )
	EAutoChangeMode AutoChangeMode;

	/** Allow edits mode. */
	UPROPERTY( config, EditAnywhere, Category=Keyframing )
	EAllowEditsMode AllowEditsMode;

	/**Key group mode. */
	UPROPERTY(config, EditAnywhere, Category = Keyframing)
	EKeyGroupMode KeyGroupMode;

	/** Enable or disable only keyframing properties marked with the 'Interp' keyword. */
	UPROPERTY( config, EditAnywhere, Category=Keyframing )
	bool bKeyInterpPropertiesOnly;

	/** The interpolation type for newly created keyframes */
	UPROPERTY( config, EditAnywhere, Category=Keyframing )
	EMovieSceneKeyInterpolation KeyInterpolation;

	/** Whether or not track defaults will be automatically set when modifying tracks. */
	UPROPERTY( config, EditAnywhere, Category=Keyframing, meta = (ToolTip = "When setting keys on properties and transforms automatically update the track default values used when there are no keys."))
	bool bAutoSetTrackDefaults;

	/** The default location of a spawnable when it is first dragged into the viewport from the content browser. */
	UPROPERTY( config, EditAnywhere, Category=General )
	TEnumAsByte<ESequencerSpawnPosition> SpawnPosition;

	/** Enable or disable creating of spawnable cameras whenever cameras are created. */
	UPROPERTY( config, EditAnywhere, Category=General )
	bool bCreateSpawnableCameras;

	/** Show the in/out range in the timeline with respect to the start/end range. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bShowRangeSlider;

	/** Enable or disable snapping in the timeline. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bIsSnapEnabled;

	/** Enable or disable snapping keys to the time snapping interval. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapKeyTimesToInterval;

	/** Enable or disable snapping keys to other keys. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapKeyTimesToKeys;
	
	/** Enable or disable snapping sections to the time snapping interval. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapSectionTimesToInterval;

	/** Enable or disable snapping sections to other sections. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapSectionTimesToSections;

	/** Enable or disable snapping the current time to keys of the selected track while scrubbing. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapPlayTimeToKeys;

	/** Enable or disable snapping the current time to the time snapping interval while scrubbing. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapPlayTimeToInterval;

	/** Enable or disable snapping the current time to the pressed key. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapPlayTimeToPressedKey;

	/** Enable or disable snapping the current time to the dragged key. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapPlayTimeToDraggedKey;

	/** The curve value interval to snap to. */
	float CurveValueSnapInterval;

	/** Enable or disable snapping the curve value to the curve value interval. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapCurveValueToInterval;

	/** Enable or disable the label browser. */
	UPROPERTY( config, EditAnywhere, Category=General )
	bool bLabelBrowserVisible;

	/** Only show selected nodes in the tree view. */
	UPROPERTY( config, EditAnywhere, Category=General )
	bool bShowSelectedNodesOnly;

	/** Defines whether to jump back to the start of the sequence when a recording is started */
	UPROPERTY(config, EditAnywhere, Category=General)
	bool bRewindOnRecord;

	/** Whether to zoom in on the current position or the current time in the timeline. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	TEnumAsByte<ESequencerZoomPosition> ZoomPosition;

	/** Enable or disable auto scroll in the timeline. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bAutoScrollEnabled;

	/** Enable or disable linking the curve editor time range to the sequencer timeline's time range. */
	UPROPERTY( config, EditAnywhere, Category=CurveEditor )
	bool bLinkCurveEditorTimeRange;

	/** The loop mode of the playback in timeline. */
	UPROPERTY( config )
	TEnumAsByte<ESequencerLoopMode> LoopMode;

	/** Enable or disable keeping the cursor in the current playback range while scrubbing. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bKeepCursorInPlayRangeWhileScrubbing;

	/** Enable or disable keeping the cursor in the current playback range during playback. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bKeepCursorInPlayRange;

	/** Enable or disable keeping the playback range constrained to the section bounds. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bKeepPlayRangeInSectionBounds;

	/** The number of zeros to pad the frame numbers by. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	uint8 ZeroPadFrames;

	/** Enable or disable the combined keyframes at the top node level. Disabling can improve editor performance. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bShowCombinedKeyframes;

	/** Enable or disable setting key area sections as infinite by default. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bInfiniteKeyAreas;

	/** Enable or disable displaying channel bar colors for vector properties. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bShowChannelColors;

	/** When enabled, sequencer is able to possess viewports that represent PIE worlds */
	UPROPERTY(config, EditAnywhere, Category=General)
	bool bAllowPossessionOfPIEViewports;

	/** When enabled, sequencer will activate 'Realtime' in viewports */
	UPROPERTY(config, EditAnywhere, Category=General)
	bool bActivateRealtimeViewports;

	/** When enabled, entering a sub sequence will evaluate that sub sequence in isolation, rather than from the master sequence */
	UPROPERTY(config, EditAnywhere, Category=Playback)
	bool bEvaluateSubSequencesInIsolation;

	/** When enabled, construction scripts will be rerun on bound actors for every frame */
	UPROPERTY(config, EditAnywhere, Category=Playback)
	bool bRerunConstructionScripts;

	/** Enable or disable showing of debug visualization. */
	UPROPERTY( config, EditAnywhere, Category=General )
	bool bShowDebugVisualization;

	/** Enable or disable showing of pre and post roll visualization. */
	UPROPERTY( config, EditAnywhere, Category=General )
	bool bVisualizePreAndPostRoll;

	/** Whether to recompile the director blueprint when the sequence is evaluated (if one exists) */
	UPROPERTY(config, EditAnywhere, Category=General)
	bool bCompileDirectorOnEvaluate;

	/** Specifies the maximum number of keys to draw when rendering trajectories in viewports */
	UPROPERTY(config, EditAnywhere, Category=General)
	uint32 TrajectoryPathCap;

	/** What format do we display time in to the user? */	
	UPROPERTY(config, EditAnywhere, Category=General)
	EFrameNumberDisplayFormats FrameNumberDisplayFormat;

	/** Specifies which curves to show in the curve editor */
	UPROPERTY(config, EditAnywhere, Category=General)
	ECurveEditorCurveVisibility CurveVisibility;
	FOnCurveEditorCurveVisibilityChanged OnCurveEditorCurveVisibilityChanged;
	FOnEvaluateSubSequencesInIsolationChanged OnEvaluateSubSequencesInIsolationChangedEvent;
	FOnShowSelectedNodesOnlyChanged OnShowSelectedNodesOnlyChangedEvent;
	FOnAllowEditsModeChanged OnAllowEditsModeChangedEvent;
	FOnLoopStateChanged OnLoopStateChangedEvent;
};
