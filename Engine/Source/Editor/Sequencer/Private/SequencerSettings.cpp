// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SequencerSettings.h"
#include "KeyParams.h"
#include "ISequencer.h"

USequencerSettings::USequencerSettings( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	AutoChangeMode = EAutoChangeMode::None;
	AllowEditsMode = EAllowEditsMode::AllEdits;
	KeyGroupMode = EKeyGroupMode::KeyChanged;
	bKeyInterpPropertiesOnly = false;
	KeyInterpolation = EMovieSceneKeyInterpolation::Auto;
	bAutoSetTrackDefaults = false;
	SpawnPosition = SSP_Origin;
	bCreateSpawnableCameras = true;
	bShowRangeSlider = false;
	bIsSnapEnabled = true;
	bSnapKeyTimesToInterval = true;
	bSnapKeyTimesToKeys = true;
	bSnapSectionTimesToInterval = true;
	bSnapSectionTimesToSections = true;
	bSnapPlayTimeToKeys = false;
	bSnapPlayTimeToInterval = true;
	bSnapPlayTimeToPressedKey = true;
	bSnapPlayTimeToDraggedKey = true;
	CurveValueSnapInterval = 10.0f;
	bSnapCurveValueToInterval = true;
	bLabelBrowserVisible = false;
	bShowSelectedNodesOnly = false;
	bRewindOnRecord = true;
	ZoomPosition = ESequencerZoomPosition::SZP_CurrentTime;
	bAutoScrollEnabled = false;
	bLinkCurveEditorTimeRange = false;
	LoopMode = ESequencerLoopMode::SLM_NoLoop;
	bKeepCursorInPlayRangeWhileScrubbing = false;
	bKeepCursorInPlayRange = true;
	bKeepPlayRangeInSectionBounds = true;
	bCompileDirectorOnEvaluate = true;
	ZeroPadFrames = 0;
	bShowCombinedKeyframes = true;
	bInfiniteKeyAreas = false;
	bShowChannelColors = false;
	bAllowPossessionOfPIEViewports = false;
	bActivateRealtimeViewports = true;
	bEvaluateSubSequencesInIsolation = false;
	bRerunConstructionScripts = true;
	bVisualizePreAndPostRoll = true;
	TrajectoryPathCap = 250;
	CurveVisibility = ECurveEditorCurveVisibility::SelectedCurves;
	FrameNumberDisplayFormat = EFrameNumberDisplayFormats::Seconds;
}

void USequencerSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

EAutoChangeMode USequencerSettings::GetAutoChangeMode() const
{
	return AutoChangeMode;
}

void USequencerSettings::SetAutoChangeMode(EAutoChangeMode InAutoChangeMode)
{
	if ( AutoChangeMode != InAutoChangeMode )
	{
		AutoChangeMode = InAutoChangeMode;
		SaveConfig();
	}
}

EAllowEditsMode USequencerSettings::GetAllowEditsMode() const
{
	return AllowEditsMode;
}

void USequencerSettings::SetAllowEditsMode(EAllowEditsMode InAllowEditsMode)
{
	if ( AllowEditsMode != InAllowEditsMode )
	{
		AllowEditsMode = InAllowEditsMode;
		SaveConfig();

		OnAllowEditsModeChangedEvent.Broadcast(InAllowEditsMode);
	}
}

EKeyGroupMode USequencerSettings::GetKeyGroupMode() const
{
	return KeyGroupMode;
}

void USequencerSettings::SetKeyGroupMode(EKeyGroupMode InKeyGroupMode)
{
	if (KeyGroupMode != InKeyGroupMode)
	{
		KeyGroupMode = InKeyGroupMode;
		SaveConfig();
	}
}

bool USequencerSettings::GetKeyInterpPropertiesOnly() const
{
	return bKeyInterpPropertiesOnly;
}

void USequencerSettings::SetKeyInterpPropertiesOnly(bool InbKeyInterpPropertiesOnly)
{
	if ( bKeyInterpPropertiesOnly != InbKeyInterpPropertiesOnly )
	{
		bKeyInterpPropertiesOnly = InbKeyInterpPropertiesOnly;
		SaveConfig();
	}
}

EMovieSceneKeyInterpolation USequencerSettings::GetKeyInterpolation() const
{
	return KeyInterpolation;
}

void USequencerSettings::SetKeyInterpolation(EMovieSceneKeyInterpolation InKeyInterpolation)
{
	if ( KeyInterpolation != InKeyInterpolation)
	{
		KeyInterpolation = InKeyInterpolation;
		SaveConfig();
	}
}

ESequencerSpawnPosition USequencerSettings::GetSpawnPosition() const
{
	return SpawnPosition;
}

void USequencerSettings::SetSpawnPosition(ESequencerSpawnPosition InSpawnPosition)
{
	if ( SpawnPosition != InSpawnPosition)
	{
		SpawnPosition = InSpawnPosition;
		SaveConfig();
	}
}

bool USequencerSettings::GetCreateSpawnableCameras() const
{
	return bCreateSpawnableCameras;
}

void USequencerSettings::SetCreateSpawnableCameras(bool bInCreateSpawnableCameras)
{
	if ( bCreateSpawnableCameras != bInCreateSpawnableCameras)
	{
		bCreateSpawnableCameras = bInCreateSpawnableCameras;
		SaveConfig();
	}
}

bool USequencerSettings::GetShowRangeSlider() const
{
	return bShowRangeSlider;
}

void USequencerSettings::SetShowRangeSlider(bool InbShowRangeSlider)
{
	if ( bShowRangeSlider != InbShowRangeSlider )
	{
		bShowRangeSlider = InbShowRangeSlider;
		SaveConfig();
	}
}

bool USequencerSettings::GetIsSnapEnabled() const
{
	return bIsSnapEnabled;
}

void USequencerSettings::SetIsSnapEnabled(bool InbIsSnapEnabled)
{
	if ( bIsSnapEnabled != InbIsSnapEnabled )
	{
		bIsSnapEnabled = InbIsSnapEnabled;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapKeyTimesToInterval() const
{
	return bSnapKeyTimesToInterval;
}

void USequencerSettings::SetSnapKeyTimesToInterval(bool InbSnapKeyTimesToInterval)
{
	if ( bSnapKeyTimesToInterval != InbSnapKeyTimesToInterval )
	{
		bSnapKeyTimesToInterval = InbSnapKeyTimesToInterval;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapKeyTimesToKeys() const
{
	return bSnapKeyTimesToKeys;
}

void USequencerSettings::SetSnapKeyTimesToKeys(bool InbSnapKeyTimesToKeys)
{
	if ( bSnapKeyTimesToKeys != InbSnapKeyTimesToKeys )
	{
		bSnapKeyTimesToKeys = InbSnapKeyTimesToKeys;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapSectionTimesToInterval() const
{
	return bSnapSectionTimesToInterval;
}

void USequencerSettings::SetSnapSectionTimesToInterval(bool InbSnapSectionTimesToInterval)
{
	if ( bSnapSectionTimesToInterval != InbSnapSectionTimesToInterval )
	{
		bSnapSectionTimesToInterval = InbSnapSectionTimesToInterval;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapSectionTimesToSections() const
{
	return bSnapSectionTimesToSections;
}

void USequencerSettings::SetSnapSectionTimesToSections( bool InbSnapSectionTimesToSections )
{
	if ( bSnapSectionTimesToSections != InbSnapSectionTimesToSections )
	{
		bSnapSectionTimesToSections = InbSnapSectionTimesToSections;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapPlayTimeToKeys() const
{
	return bSnapPlayTimeToKeys;
}

void USequencerSettings::SetSnapPlayTimeToKeys(bool InbSnapPlayTimeToKeys)
{
	if ( bSnapPlayTimeToKeys != InbSnapPlayTimeToKeys )
	{
		bSnapPlayTimeToKeys = InbSnapPlayTimeToKeys;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapPlayTimeToInterval() const
{
	return bSnapPlayTimeToInterval;
}

void USequencerSettings::SetSnapPlayTimeToInterval(bool InbSnapPlayTimeToInterval)
{
	if ( bSnapPlayTimeToInterval != InbSnapPlayTimeToInterval )
	{
		bSnapPlayTimeToInterval = InbSnapPlayTimeToInterval;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapPlayTimeToPressedKey() const
{
	return bSnapPlayTimeToPressedKey;
}

void USequencerSettings::SetSnapPlayTimeToPressedKey(bool InbSnapPlayTimeToPressedKey)
{
	if ( bSnapPlayTimeToPressedKey != InbSnapPlayTimeToPressedKey )
	{
		bSnapPlayTimeToPressedKey = InbSnapPlayTimeToPressedKey;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapPlayTimeToDraggedKey() const
{
	return bSnapPlayTimeToDraggedKey;
}

void USequencerSettings::SetSnapPlayTimeToDraggedKey(bool InbSnapPlayTimeToDraggedKey)
{
	if ( bSnapPlayTimeToDraggedKey != InbSnapPlayTimeToDraggedKey )
	{
		bSnapPlayTimeToDraggedKey = InbSnapPlayTimeToDraggedKey;
		SaveConfig();
	}
}

float USequencerSettings::GetCurveValueSnapInterval() const
{
	return CurveValueSnapInterval;
}

void USequencerSettings::SetCurveValueSnapInterval( float InCurveValueSnapInterval )
{
	if ( CurveValueSnapInterval != InCurveValueSnapInterval )
	{
		CurveValueSnapInterval = InCurveValueSnapInterval;
		SaveConfig();
	}
}

bool USequencerSettings::GetSnapCurveValueToInterval() const
{
	return bSnapCurveValueToInterval;
}

void USequencerSettings::SetSnapCurveValueToInterval( bool InbSnapCurveValueToInterval )
{
	if ( bSnapCurveValueToInterval != InbSnapCurveValueToInterval )
	{
		bSnapCurveValueToInterval = InbSnapCurveValueToInterval;
		SaveConfig();
	}
}

bool USequencerSettings::GetLabelBrowserVisible() const
{
	return bLabelBrowserVisible;
}

void USequencerSettings::SetLabelBrowserVisible(bool Visible)
{
	if (bLabelBrowserVisible != Visible)
	{
		bLabelBrowserVisible = Visible;
		SaveConfig();
	}
}

bool USequencerSettings::GetShowSelectedNodesOnly() const
{
	return bShowSelectedNodesOnly;
}

void USequencerSettings::SetShowSelectedNodesOnly(bool Visible)
{
	if (bShowSelectedNodesOnly != Visible)
	{
		bShowSelectedNodesOnly = Visible;
		SaveConfig();

		OnShowSelectedNodesOnlyChangedEvent.Broadcast();
	}
}

bool USequencerSettings::ShouldRewindOnRecord() const
{
	return bRewindOnRecord;
}

void USequencerSettings::SetRewindOnRecord(bool bInRewindOnRecord)
{
	if (bInRewindOnRecord != bRewindOnRecord)
	{
		bRewindOnRecord = bInRewindOnRecord;
		SaveConfig();
	}
}

ESequencerZoomPosition USequencerSettings::GetZoomPosition() const
{
	return ZoomPosition;
}

void USequencerSettings::SetZoomPosition(ESequencerZoomPosition InZoomPosition)
{
	if ( ZoomPosition != InZoomPosition)
	{
		ZoomPosition = InZoomPosition;
		SaveConfig();
	}
}

bool USequencerSettings::GetAutoScrollEnabled() const
{
	return bAutoScrollEnabled;
}

void USequencerSettings::SetAutoScrollEnabled(bool bInAutoScrollEnabled)
{
	if (bAutoScrollEnabled != bInAutoScrollEnabled)
	{
		bAutoScrollEnabled = bInAutoScrollEnabled;
		SaveConfig();
	}
}

ESequencerLoopMode USequencerSettings::GetLoopMode() const
{
	return LoopMode;
}

void USequencerSettings::SetLoopMode(ESequencerLoopMode InLoopMode)
{
	if (LoopMode != InLoopMode)
	{
		LoopMode = InLoopMode;
		OnLoopStateChangedEvent.Broadcast();
		SaveConfig();
	}
}

bool USequencerSettings::ShouldKeepCursorInPlayRangeWhileScrubbing() const
{
	return bKeepCursorInPlayRangeWhileScrubbing;
}

void USequencerSettings::SetKeepCursorInPlayRangeWhileScrubbing(bool bInKeepCursorInPlayRangeWhileScrubbing)
{
	if (bKeepCursorInPlayRangeWhileScrubbing != bInKeepCursorInPlayRangeWhileScrubbing)
	{
		bKeepCursorInPlayRangeWhileScrubbing = bInKeepCursorInPlayRangeWhileScrubbing;
		SaveConfig();
	}
}

bool USequencerSettings::ShouldKeepCursorInPlayRange() const
{
	return bKeepCursorInPlayRange;
}

void USequencerSettings::SetKeepCursorInPlayRange(bool bInKeepCursorInPlayRange)
{
	if (bKeepCursorInPlayRange != bInKeepCursorInPlayRange)
	{
		bKeepCursorInPlayRange = bInKeepCursorInPlayRange;
		SaveConfig();
	}
}

bool USequencerSettings::ShouldKeepPlayRangeInSectionBounds() const
{
	return bKeepPlayRangeInSectionBounds;
}

void USequencerSettings::SetKeepPlayRangeInSectionBounds(bool bInKeepPlayRangeInSectionBounds)
{
	if (bKeepPlayRangeInSectionBounds != bInKeepPlayRangeInSectionBounds)
	{
		bKeepPlayRangeInSectionBounds = bInKeepPlayRangeInSectionBounds;
		SaveConfig();
	}
}


bool USequencerSettings::GetLinkCurveEditorTimeRange() const
{
	return bLinkCurveEditorTimeRange;
}

void USequencerSettings::SetLinkCurveEditorTimeRange(bool InbLinkCurveEditorTimeRange)
{
	if (bLinkCurveEditorTimeRange != InbLinkCurveEditorTimeRange)
	{
		bLinkCurveEditorTimeRange = InbLinkCurveEditorTimeRange;
		SaveConfig();
	}
}


uint8 USequencerSettings::GetZeroPadFrames() const
{
	return ZeroPadFrames;
}

void USequencerSettings::SetZeroPadFrames(uint8 InZeroPadFrames)
{
	if (ZeroPadFrames != InZeroPadFrames)
	{
		ZeroPadFrames = InZeroPadFrames;
		SaveConfig();
	}
}

bool USequencerSettings::GetShowCombinedKeyframes() const
{
	return bShowCombinedKeyframes;
}

void USequencerSettings::SetShowCombinedKeyframes(bool InbShowCombinedKeyframes)
{
	if (bShowCombinedKeyframes != InbShowCombinedKeyframes)
	{
		bShowCombinedKeyframes = InbShowCombinedKeyframes;
		SaveConfig();
	}
}


bool USequencerSettings::GetInfiniteKeyAreas() const
{
	return bInfiniteKeyAreas;
}

void USequencerSettings::SetInfiniteKeyAreas(bool InbInfiniteKeyAreas)
{
	if (bInfiniteKeyAreas != InbInfiniteKeyAreas)
	{
		bInfiniteKeyAreas = InbInfiniteKeyAreas;
		SaveConfig();
	}
}


bool USequencerSettings::GetShowChannelColors() const
{
	return bShowChannelColors;
}

void USequencerSettings::SetShowChannelColors(bool InbShowChannelColors)
{
	if (bShowChannelColors != InbShowChannelColors)
	{
		bShowChannelColors = InbShowChannelColors;
		SaveConfig();
	}
}

bool USequencerSettings::ShouldAllowPossessionOfPIEViewports() const
{
	return bAllowPossessionOfPIEViewports;
}

void USequencerSettings::SetAllowPossessionOfPIEViewports(bool bInAllowPossessionOfPIEViewports)
{
	if (bInAllowPossessionOfPIEViewports != bAllowPossessionOfPIEViewports)
	{
		bAllowPossessionOfPIEViewports = bInAllowPossessionOfPIEViewports;
		SaveConfig();
	}
}

bool USequencerSettings::ShouldActivateRealtimeViewports() const
{
	return bActivateRealtimeViewports;
}

void USequencerSettings::SetActivateRealtimeViewports(bool bInActivateRealtimeViewports)
{
	if (bInActivateRealtimeViewports != bActivateRealtimeViewports)
	{
		bActivateRealtimeViewports = bInActivateRealtimeViewports;
		SaveConfig();
	}
}

bool USequencerSettings::GetAutoSetTrackDefaults() const
{
	return bAutoSetTrackDefaults;
}

void USequencerSettings::SetAutoSetTrackDefaults(bool bInAutoSetTrackDefaults)
{
	if (bInAutoSetTrackDefaults != bAutoSetTrackDefaults)
	{
		bAutoSetTrackDefaults = bInAutoSetTrackDefaults;
		SaveConfig();
	}
}

bool USequencerSettings::ShouldShowDebugVisualization() const
{
	return bShowDebugVisualization;
}

void USequencerSettings::SetShowDebugVisualization(bool bInShowDebugVisualization)
{
	if (bShowDebugVisualization != bInShowDebugVisualization)
	{
		bShowDebugVisualization = bInShowDebugVisualization;
		SaveConfig();
	}
}

bool USequencerSettings::ShouldEvaluateSubSequencesInIsolation() const
{
	return bEvaluateSubSequencesInIsolation;
}

void USequencerSettings::SetEvaluateSubSequencesInIsolation(bool bInEvaluateSubSequencesInIsolation)
{
	if (bEvaluateSubSequencesInIsolation != bInEvaluateSubSequencesInIsolation)
	{
		bEvaluateSubSequencesInIsolation = bInEvaluateSubSequencesInIsolation;
		SaveConfig();

		OnEvaluateSubSequencesInIsolationChangedEvent.Broadcast();
	}
}

bool USequencerSettings::ShouldRerunConstructionScripts() const
{
	return bRerunConstructionScripts;
}

void USequencerSettings::SetRerunConstructionScripts(bool bInRerunConstructionScripts)
{
	if (bRerunConstructionScripts != bInRerunConstructionScripts)
	{
		bRerunConstructionScripts = bInRerunConstructionScripts;
		SaveConfig();
	}
}

bool USequencerSettings::ShouldShowPrePostRoll() const
{
	return bVisualizePreAndPostRoll;
}

void USequencerSettings::SetShouldShowPrePostRoll(bool bInVisualizePreAndPostRoll)
{
	if (bInVisualizePreAndPostRoll != bVisualizePreAndPostRoll)
	{
		bVisualizePreAndPostRoll = bInVisualizePreAndPostRoll;
		SaveConfig();
	}
}


bool USequencerSettings::ShouldCompileDirectorOnEvaluate() const
{
	return bCompileDirectorOnEvaluate;
}

void USequencerSettings::SetCompileDirectorOnEvaluate(bool bInCompileDirectorOnEvaluate)
{
	if (bInCompileDirectorOnEvaluate != bCompileDirectorOnEvaluate)
	{
		bCompileDirectorOnEvaluate = bInCompileDirectorOnEvaluate;
		SaveConfig();
	}
}

ECurveEditorCurveVisibility USequencerSettings::GetCurveVisibility() const
{
	return CurveVisibility;
}

void USequencerSettings::SetCurveVisibility(ECurveEditorCurveVisibility InCurveVisibility)
{
	if (CurveVisibility != InCurveVisibility)
	{
		CurveVisibility = InCurveVisibility;
		OnCurveEditorCurveVisibilityChanged.Broadcast();
		SaveConfig();
	}
}

USequencerSettings::FOnCurveEditorCurveVisibilityChanged& USequencerSettings::GetOnCurveEditorCurveVisibilityChanged()
{
	return OnCurveEditorCurveVisibilityChanged;
}

USequencerSettings::FOnLoopStateChanged& USequencerSettings::GetOnLoopStateChanged()
{
	return OnLoopStateChangedEvent;
}


void USequencerSettings::SetTimeDisplayFormat(EFrameNumberDisplayFormats InFormat)
{
	if (InFormat != FrameNumberDisplayFormat)
	{
		FrameNumberDisplayFormat = InFormat;
		SaveConfig();
	}
}
