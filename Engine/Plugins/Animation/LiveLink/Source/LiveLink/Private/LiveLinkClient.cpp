// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkClient.h"
#include "Misc/ScopeLock.h"
#include "UObject/UObjectHash.h"
#include "LiveLinkSourceFactory.h"
#include "Misc/Guid.h"
#include "Misc/App.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "TimeSynchronizationSource.h"

DEFINE_LOG_CATEGORY(LogLiveLink);

const double VALIDATE_SOURCES_TIME = 3.0; //How long should we wait between validation checks
const int32 MIN_FRAMES_TO_REMOVE = 5;

FLiveLinkCurveIntegrationData FLiveLinkCurveKey::UpdateCurveKey(const TArray<FLiveLinkCurveElement>& CurveElements)
{
	FLiveLinkCurveIntegrationData IntegrationData;

	int32 CurrentSize = CurveNames.Num();

	IntegrationData.CurveValues.AddDefaulted(CurrentSize);

	for(const FLiveLinkCurveElement& Elem : CurveElements)
	{
		int32 CurveIndex = CurveNames.IndexOfByKey(Elem.CurveName);
		if (CurveIndex == INDEX_NONE)
		{
			CurveIndex = CurveNames.Add(Elem.CurveName);
			IntegrationData.CurveValues.AddDefaulted();
		}
		IntegrationData.CurveValues[CurveIndex].SetValue(Elem.CurveValue);
	}
	IntegrationData.NumNewCurves = CurveNames.Num() - CurrentSize;

	return IntegrationData;
}

void BlendItem(const FTransform& A, const FTransform& B, FTransform& Output, float BlendWeight)
{
	const ScalarRegister ABlendWeight(1.0f - BlendWeight);
	const ScalarRegister BBlendWeight(BlendWeight);

	Output = A * ABlendWeight;
	Output.AccumulateWithShortestRotation(B, BBlendWeight);
	Output.NormalizeRotation();
}

void BlendItem(const FOptionalCurveElement& A, const FOptionalCurveElement& B, FOptionalCurveElement& Output, float BlendWeight)
{
	Output.Value = (A.Value * (1.0f - BlendWeight)) + (B.Value * BlendWeight);
	Output.bValid = A.bValid || B.bValid;
}

template<class Type>
void Blend(const TArray<Type>& A, const TArray<Type>& B, TArray<Type>& Output, float BlendWeight)
{
	check(A.Num() == B.Num());
	Output.SetNum(A.Num(), false);

	for (int32 BlendIndex = 0; BlendIndex < A.Num(); ++BlendIndex)
	{
		BlendItem(A[BlendIndex], B[BlendIndex], Output[BlendIndex], BlendWeight);
	}
}

void FLiveLinkSubject::AddFrame(const FLiveLinkFrameData& FrameData, FGuid FrameSource, bool bSaveFrame)
{
	LastModifier = FrameSource;

	int32 FrameIndex = INDEX_NONE;
	switch (CachedSettings.SourceMode)
	{
	case ELiveLinkSourceMode::TimeSynchronized:
		if (TimeSyncData.IsSet())
		{
			FrameIndex = AddFrame_TimeSynchronized(FrameData.MetaData.SceneTime.Time, bSaveFrame);
		}
		else
		{
			FrameIndex = AddFrame_Default(FrameData.WorldTime, bSaveFrame);
		}
		break;

	case ELiveLinkSourceMode::Interpolated:
		FrameIndex = AddFrame_Interpolated(FrameData.WorldTime, bSaveFrame);
		break;

	default:
		FrameIndex = AddFrame_Default(FrameData.WorldTime, bSaveFrame);
		break;
	}

	FLiveLinkFrame& NewFrame = Frames.EmplaceAt_GetRef(FrameIndex);
	FLiveLinkCurveIntegrationData IntegrationData = CurveKeyData.UpdateCurveKey(FrameData.CurveElements);

	NewFrame.Transforms = FrameData.Transforms;
	NewFrame.Curves = MoveTemp(IntegrationData.CurveValues);
	NewFrame.MetaData = FrameData.MetaData;
	NewFrame.WorldTime = FrameData.WorldTime;

	// update existing curves
	if (IntegrationData.NumNewCurves > 0)
	{
		for (FLiveLinkFrame& Frame : Frames)
		{
			Frame.ExtendCurveData(IntegrationData.NumNewCurves);
		}
	}
}

int32 FLiveLinkSubject::AddFrame_Default(const FLiveLinkWorldTime& WorldTime, bool bSaveFrame)
{
	if (!bSaveFrame && WorldTime.Time < LastReadTime)
	{
		//Gone back in time
		Frames.Reset();
		LastReadTime = 0;
		SubjectTimeOffset = WorldTime.Offset;
	}

	int32 FrameIndex = 0;
	if (Frames.Num() == 0)
	{
		LastReadFrame = 0;
	}
	else
	{
		if (!bSaveFrame && (LastReadFrame > MIN_FRAMES_TO_REMOVE))
		{
			check(Frames.Num() > LastReadFrame);
			Frames.RemoveAt(0, LastReadFrame, false);
			LastReadFrame = 0;
		}

		for (FrameIndex = Frames.Num() - 1; FrameIndex >= 0; --FrameIndex)
		{
			if (Frames[FrameIndex].WorldTime.Time <= WorldTime.Time)
			{
				break;
			}
		}

		FrameIndex += 1;
	}

	return FrameIndex;
}

int32 FLiveLinkSubject::AddFrame_Interpolated(const FLiveLinkWorldTime& WorldTime, bool bSaveFrame)
{
	return AddFrame_Default(WorldTime, bSaveFrame);
}

int32 FLiveLinkSubject::AddFrame_TimeSynchronized(const FFrameTime& FrameTime, bool bSaveFrame)
{
	int32 FrameIndex = 0;

	const FLiveLinkTimeSynchronizationData& TimeSyncDataLocal = TimeSyncData.GetValue();

	// If we're not actively synchronizing, we don't need to do anything special.
	if (Frames.Num() == 0)
	{
		LastReadTime = 0;
		LastReadFrame = 0;
	}
	else if (TimeSyncData->RolloverModulus.IsSet())
	{
		const FFrameTime UseFrameTime = UTimeSynchronizationSource::AddOffsetWithRolloverModulus(FrameTime, TimeSyncDataLocal.Offset, TimeSyncDataLocal.RolloverModulus.GetValue());
		FrameIndex = AddFrame_TimeSynchronized</*bWithRollover=*/true>(UseFrameTime, (!TimeSyncDataLocal.bHasEstablishedSync) || bSaveFrame);
	}
	else
	{
		FrameIndex = AddFrame_TimeSynchronized</*bWithRollover=*/false>(FrameTime + TimeSyncDataLocal.Offset, (!TimeSyncDataLocal.bHasEstablishedSync) || bSaveFrame);
	}

	return FrameIndex;
}

template<bool bWithRollover>
int32 FLiveLinkSubject::AddFrame_TimeSynchronized(const FFrameTime& FrameTime, bool bSaveFrame)
{
	if (!bSaveFrame && (LastReadFrame > MIN_FRAMES_TO_REMOVE))
	{
		check(Frames.Num() > LastReadFrame);

		if (bWithRollover)
		{
			int32& RolloverFrame = TimeSyncData->RolloverFrame;

			// If we had previously detected that a roll over had occurred in the range of frames we have,
			// then we need to adjust that as well.
			if (RolloverFrame > 0)
			{
				RolloverFrame = RolloverFrame - LastReadFrame;
				if (RolloverFrame <= 0)
				{
					RolloverFrame = INDEX_NONE;
				}
			}
		}

		Frames.RemoveAt(0, LastReadFrame, false);
		LastReadFrame = 0;
	}

	return FindFrameIndex_TimeSynchronized</*bForInsert=*/true, bWithRollover>(FrameTime);
}

void FLiveLinkSubject::CopyFrameData(const FLiveLinkFrame& InFrame, FLiveLinkSubjectFrame& OutFrame)
{
	OutFrame.Transforms = InFrame.Transforms;
	OutFrame.Curves = InFrame.Curves;
	OutFrame.MetaData = InFrame.MetaData;
}

void FLiveLinkSubject::CopyFrameDataBlended(const FLiveLinkFrame& PreFrame, const FLiveLinkFrame& PostFrame, float BlendWeight, FLiveLinkSubjectFrame& OutFrame)
{
	Blend(PreFrame.Transforms, PostFrame.Transforms, OutFrame.Transforms, BlendWeight);
	Blend(PreFrame.Curves, PostFrame.Curves, OutFrame.Curves, BlendWeight);
}

void FLiveLinkSubject::ResetFrame(FLiveLinkSubjectFrame& OutFrame) const
{
	OutFrame.RefSkeleton = RefSkeleton;
	OutFrame.RefSkeletonGuid = RefSkeletonGuid;
	OutFrame.CurveKeyData = CurveKeyData;

	OutFrame.Transforms.Reset();
	OutFrame.Curves.Reset();
	OutFrame.MetaData.StringMetaData.Reset();
}

void FLiveLinkSubject::GetFrameAtWorldTime(const double InSeconds, FLiveLinkSubjectFrame& OutFrame)
{
	ResetFrame(OutFrame);

	switch (CachedSettings.SourceMode)
	{
	case ELiveLinkSourceMode::TimeSynchronized:
		ensureMsgf(false, TEXT("Attempting to use WorldTime for a TimeSynchronized source! Source = %s"), *Name.ToString());
		GetFrameAtWorldTime_Default(InSeconds, OutFrame);
		break;

	case ELiveLinkSourceMode::Interpolated:
		GetFrameAtWorldTime_Interpolated(InSeconds, OutFrame);
		break;

	default:
		GetFrameAtWorldTime_Default(InSeconds, OutFrame);
		break;
	}
}

void FLiveLinkSubject::GetFrameAtSceneTime(const FQualifiedFrameTime& InSceneTime, FLiveLinkSubjectFrame& OutFrame)
{
	ResetFrame(OutFrame);

	switch (CachedSettings.SourceMode)
	{
	case ELiveLinkSourceMode::TimeSynchronized:

		if (TimeSyncData.IsSet())
		{
			const FFrameTime FrameTime = InSceneTime.ConvertTo(CachedSettings.TimeSynchronizationSettings->FrameRate);
			if (TimeSyncData->RolloverModulus.IsSet())
			{
				GetFrameAtSceneTime_TimeSynchronized</*bWithRollover=*/true>(FrameTime, OutFrame);
			}
			else
			{
				GetFrameAtSceneTime_TimeSynchronized</*bWithRollover=*/false>(FrameTime, OutFrame);
			}
		}
		else
		{
			GetFrameAtWorldTime_Default(InSceneTime.AsSeconds(), OutFrame);
		}
		break;

	default:
		ensureMsgf(false, TEXT("Attempting to use SceneTime for a non TimeSynchronized source! Source = %s Mode = %d"), *Name.ToString(), static_cast<int32>(CachedSettings.SourceMode));
		GetFrameAtWorldTime_Default(InSceneTime.AsSeconds(), OutFrame);
		break;
	}
}

void FLiveLinkSubject::GetFrameAtWorldTime_Default(const double InSeconds, FLiveLinkSubjectFrame& OutFrame)
{
	CopyFrameData(Frames.Last(), OutFrame);
	LastReadTime = Frames.Last().WorldTime.Time;
	LastReadFrame = Frames.Num() - 1;
}

void FLiveLinkSubject::GetFrameAtWorldTime_Interpolated(const double InSeconds, FLiveLinkSubjectFrame& OutFrame)
{
	LastReadTime = (InSeconds - SubjectTimeOffset) - CachedSettings.InterpolationSettings->InterpolationOffset;

	bool bBuiltFrame = false;

	for (int32 FrameIndex = Frames.Num() - 1; FrameIndex >= 0; --FrameIndex)
	{
		if (Frames[FrameIndex].WorldTime.Time < LastReadTime)
		{
			//Found Start frame

			if (FrameIndex == Frames.Num() - 1)
			{
				LastReadFrame = FrameIndex;
				CopyFrameData(Frames[FrameIndex], OutFrame);
				bBuiltFrame = true;
				break;
			}
			else
			{
				LastReadFrame = FrameIndex;
				const FLiveLinkFrame& PreFrame = Frames[FrameIndex];
				const FLiveLinkFrame& PostFrame = Frames[FrameIndex + 1];

				// Calc blend weight (Amount through frame gap / frame gap) 
				const float BlendWeight = (LastReadTime - PreFrame.WorldTime.Time) / (PostFrame.WorldTime.Time - PreFrame.WorldTime.Time);

				CopyFrameDataBlended(PreFrame, PostFrame, BlendWeight, OutFrame);

				bBuiltFrame = true;
				break;
			}
		}
	}

	if (!bBuiltFrame)
	{
		LastReadFrame = 0;
		// Failed to find an interp point so just take earliest frame
		CopyFrameData(Frames[0], OutFrame);
	}
}

template<bool bWithRollover>
void FLiveLinkSubject::GetFrameAtSceneTime_TimeSynchronized(const FFrameTime& InTime, FLiveLinkSubjectFrame& OutFrame)
{
	const int32 UseFrame = FindFrameIndex_TimeSynchronized</*bForInsert=*/false, bWithRollover>(InTime);
	CopyFrameData(Frames[UseFrame], OutFrame);
	LastReadTime = Frames[UseFrame].WorldTime.Time;
	LastReadFrame = UseFrame;
}

template<bool bForInsert, bool bWithRollover>
int32 FLiveLinkSubject::FindFrameIndex_TimeSynchronized(const FFrameTime& FrameTime)
{
	if (Frames.Num() == 0)
	{
		return 0;
	}

	FLiveLinkTimeSynchronizationData& TimeSyncDataLocal = TimeSyncData.GetValue();

	// Preroll / Synchronization should handle the case where there are any time skips by simply clearing out the buffered data.
	// Therefore, there are only 2 cases where time would go backwards:
	// 1. We've received frames out of order. In this case, we want to push it backwards.
	// 2. We've rolled over. In that case, value have wrapped around zero (and appear "smaller") but should be treated as newer.

	// Further, when we're not inserting a value, we're guaranteed that the frame time should always go up
	// (or stay the same). So, in that case we only need to search between our LastReadFrameTime and the Newest Frame.
	// That assumption will break if external code tries to grab anything other than the frame of data we build internally.

	// Finally, we only update the RolloverFrame value when inserting values. This is because we may query for a rollover frame
	// before we receive a rollover frame (in the case of missing or unordered frames).
	// We generally don't want to modify state if we're just reading data.

	int32 HighFrame = Frames.Num() - 1;
	int32 LowFrame = bForInsert ? 0 : LastReadFrame;
	int32 FrameIndex = HighFrame;

	if (bWithRollover)
	{
		bool bDidRollover = false;
		int32& RolloverFrame = TimeSyncDataLocal.RolloverFrame;
		const FFrameTime& CompareFrameTime = ((RolloverFrame == INDEX_NONE) ? Frames.Last() : Frames[RolloverFrame - 1]).MetaData.SceneTime.Time;
		UTimeSynchronizationSource::FindDistanceBetweenFramesWithRolloverModulus(CompareFrameTime, FrameTime, TimeSyncDataLocal.RolloverModulus.GetValue(), bDidRollover);

		if (RolloverFrame == INDEX_NONE)
		{
			if (bDidRollover)
			{
				if (bForInsert)
				{
					RolloverFrame = HighFrame;
					FrameIndex = Frames.Num();
				}
				else
				{
					FrameIndex = HighFrame;
				}

				return FrameIndex;
			}
		}
		else
		{
			if (bDidRollover)
			{
				LowFrame = RolloverFrame;
			}
			else
			{
				HighFrame = RolloverFrame - 1;
				if (bForInsert)
				{
					++RolloverFrame;
				}
			}
		}
	}

	if (bForInsert)
	{
		for (; LowFrame <= FrameIndex && Frames[FrameIndex].MetaData.SceneTime.Time > FrameTime; --FrameIndex);
		FrameIndex += 1;
	}
	else
	{
		for (; LowFrame < FrameIndex && Frames[FrameIndex].MetaData.SceneTime.Time > FrameTime; --FrameIndex);
	}

	return FrameIndex;
}

void FLiveLinkSubject::ClearFrames()
{
	LastReadFrame = INDEX_NONE;
	LastReadTime = 0;
	Frames.Reset();
}

void FLiveLinkSubject::CacheSourceSettings(const ULiveLinkSourceSettings* Settings)
{
	check(IsInGameThread());

	const bool bSourceModeChanged = Settings->Mode != CachedSettings.SourceMode;
	if (bSourceModeChanged)
	{
		ClearFrames();
		CachedSettings.TimeSynchronizationSettings.Reset();
		CachedSettings.InterpolationSettings.Reset();

		switch (CachedSettings.SourceMode)
		{
		case ELiveLinkSourceMode::TimeSynchronized:
			TimeSyncData.Reset();
			break;

		default:
			break;
		}
	}

	CachedSettings.SourceMode = Settings->Mode;

	// Even if the mode didn't change, settings may have updated.
	// Handle those changes now.
	switch (CachedSettings.SourceMode)
	{
	case ELiveLinkSourceMode::TimeSynchronized:
		CachedSettings.TimeSynchronizationSettings = Settings->TimeSynchronizationSettings;
		break;

	case ELiveLinkSourceMode::Interpolated:
		CachedSettings.InterpolationSettings = Settings->InterpolationSettings;
		break;

	default:
		break;
	}
}

FLiveLinkClient::~FLiveLinkClient()
{
	TArray<int> ToRemove;
	ToRemove.Reserve(Sources.Num());

	while (Sources.Num() > 0)
	{
		ToRemove.Reset();

		for (int32 Idx = 0; Idx < Sources.Num(); ++Idx)
		{
			if (Sources[Idx]->RequestSourceShutdown())
			{
				ToRemove.Add(Idx);
			}
		}

		for (int32 Idx = ToRemove.Num() - 1; Idx >= 0; --Idx)
		{
			Sources.RemoveAtSwap(ToRemove[Idx], 1, false);
		}
	}
}

void FLiveLinkClient::Tick(float DeltaTime)
{
	if (LastValidationCheck < FPlatformTime::Seconds() - VALIDATE_SOURCES_TIME)
	{
		ValidateSources();
	}

	BuildThisTicksSubjectSnapshot();
}

void FLiveLinkClient::AddReferencedObjects(FReferenceCollector & Collector)
{
	for (const ULiveLinkSourceSettings* Settings : SourceSettings)
	{
		Collector.AddReferencedObject(Settings);
	}
}

void FLiveLinkClient::ValidateSources()
{
	bool bSourcesChanged = false;
	for (int32 SourceIdx = Sources.Num() - 1; SourceIdx >= 0; --SourceIdx)
	{
		if (!Sources[SourceIdx]->IsSourceStillValid())
		{
			RemoveSourceInternal(SourceIdx);

			bSourcesChanged = true;
		}
	}

	for (int32 SourceIdx = SourcesToRemove.Num() - 1; SourceIdx >= 0; --SourceIdx)
	{
		if (SourcesToRemove[SourceIdx]->RequestSourceShutdown())
		{
			SourcesToRemove.RemoveAtSwap(SourceIdx, 1, false);
		}
	}

	LastValidationCheck = FPlatformTime::Seconds();

	if (bSourcesChanged)
	{
		OnLiveLinkSourcesChanged.Broadcast();
	}
}

void FLiveLinkClient::BuildThisTicksSubjectSnapshot()
{
	const int32 PreviousSize = ActiveSubjectSnapshots.Num();

	TArray<FName> OldSubjectSnapshotNames;
	ActiveSubjectSnapshots.GenerateKeyArray(OldSubjectSnapshotNames);

	const double CurrentInterpTime = FPlatformTime::Seconds();	// Set this up once, every subject
																// uses the same time

	const FFrameRate FrameRate = FApp::GetTimecodeFrameRate();
	const FTimecode Timecode = FApp::GetTimecode();
	const FQualifiedFrameTime CurrentSyncTime(Timecode.ToFrameNumber(FrameRate), FrameRate);

	{
		FScopeLock Lock(&SubjectDataAccessCriticalSection);

		for (TPair<FName, FLiveLinkSubject>& SubjectPair : LiveSubjectData)
		{
			const FName SubjectName = SubjectPair.Key;
			OldSubjectSnapshotNames.RemoveSingleSwap(SubjectName, false);

			FLiveLinkSubject& SourceSubject = SubjectPair.Value;

			if (const ULiveLinkSourceSettings* Settings = GetSourceSettingsForEntry(SourceSubject.LastModifier))
			{
				SourceSubject.CacheSourceSettings(Settings);
			}

			if (SourceSubject.Frames.Num() > 0)
			{
				FLiveLinkSubjectFrame* SnapshotSubject = ActiveSubjectSnapshots.Find(SubjectName);
				if (!SnapshotSubject)
				{
					ActiveSubjectNames.Add(SubjectName);
					SnapshotSubject = &ActiveSubjectSnapshots.Add(SubjectName);
				}

				if (SourceSubject.GetMode() == ELiveLinkSourceMode::TimeSynchronized)
				{
					SourceSubject.GetFrameAtSceneTime(CurrentSyncTime, *SnapshotSubject);
				}
				else
				{
					SourceSubject.GetFrameAtWorldTime(CurrentInterpTime, *SnapshotSubject);
				}
			}
		}
	}

	//Now that ActiveSubjectSnapshots is up to date we now need to build the virtual subject data
	for (TPair<FName, FLiveLinkVirtualSubject>& SubjectPair : VirtualSubjects)
	{
		if (SubjectPair.Value.GetSubjects().Num() > 0)
		{
			const FName SubjectName = SubjectPair.Key;
			OldSubjectSnapshotNames.RemoveSingleSwap(SubjectName, false);

			FLiveLinkSubjectFrame& SnapshotSubject = ActiveSubjectSnapshots.FindOrAdd(SubjectName);

			BuildVirtualSubjectFrame(SubjectPair.Value, SnapshotSubject);
		}
	}

	if (PreviousSize != ActiveSubjectSnapshots.Num() || OldSubjectSnapshotNames.Num() > 0)
	{
		//Have either added or removed a subject, must signal update
		OnLiveLinkSubjectsChanged.Broadcast();
	}

	for (FName SubjectName : OldSubjectSnapshotNames)
	{
		ActiveSubjectSnapshots.Remove(SubjectName);
		ActiveSubjectNames.RemoveSingleSwap(SubjectName, false);
	}
}

void FLiveLinkClient::BuildVirtualSubjectFrame(FLiveLinkVirtualSubject& VirtualSubject, FLiveLinkSubjectFrame& SnapshotSubject)
{
	VirtualSubject.BuildRefSkeletonForVirtualSubject(ActiveSubjectSnapshots, ActiveSubjectNames);

	SnapshotSubject.RefSkeleton = VirtualSubject.GetRefSkeleton();
	SnapshotSubject.CurveKeyData = VirtualSubject.CurveKeyData;

	SnapshotSubject.Transforms.Reset(SnapshotSubject.RefSkeleton.GetBoneNames().Num());
	SnapshotSubject.Transforms.Add(FTransform::Identity);
	SnapshotSubject.MetaData.StringMetaData.Empty();
	for (FName SubjectName : VirtualSubject.Subjects)
	{
		FLiveLinkSubjectFrame& SubjectFrame = ActiveSubjectSnapshots.FindChecked(SubjectName);
		SnapshotSubject.Transforms.Append(SubjectFrame.Transforms);
		for (const auto& MetaDatum : SubjectFrame.MetaData.StringMetaData)
		{
			FName QualifiedKey = FName(*(SubjectName.ToString() + MetaDatum.Key.ToString()));
			SnapshotSubject.MetaData.StringMetaData.Emplace(SubjectName, MetaDatum.Value);
		}
	}
}

void FLiveLinkClient::AddVirtualSubject(FName NewVirtualSubjectName)
{
	VirtualSubjects.Add(NewVirtualSubjectName);
}

void FLiveLinkClient::AddSource(TSharedPtr<ILiveLinkSource> InSource)
{
	Sources.Add(InSource);
	SourceGuids.Add(FGuid::NewGuid());

	UClass* CustomSettingsClass = InSource->GetCustomSettingsClass();

	if (CustomSettingsClass && !CustomSettingsClass->IsChildOf<ULiveLinkSourceSettings>())
	{
		UE_LOG(LogLiveLink, Warning, TEXT("Custom Setting Failure: Source '%s' settings class '%s' does not derive from ULiveLinkSourceSettings"), *InSource->GetSourceType().ToString(), *CustomSettingsClass->GetName());
		CustomSettingsClass = nullptr;
	}

	UClass* SettingsClass = CustomSettingsClass ? CustomSettingsClass : ULiveLinkSourceSettings::StaticClass();
	ULiveLinkSourceSettings* NewSettings = NewObject<ULiveLinkSourceSettings>(GetTransientPackage(), SettingsClass);

	SourceSettings.Add(NewSettings);

	InSource->ReceiveClient(this, SourceGuids.Last());
	InSource->InitializeSettings(NewSettings);

	OnLiveLinkSourcesChanged.Broadcast();
}

void FLiveLinkClient::AddVirtualSubjectSource()
{
	SourceGuids.Add(VirtualSubjectGuid);
	Sources.Add(MakeShared<FLiveLinkVirtualSubjectSource>());

	ULiveLinkSourceSettings* NewSettings = NewObject<ULiveLinkSourceSettings>(GetTransientPackage());
	SourceSettings.Add(NewSettings);
}

void FLiveLinkClient::RemoveSourceInternal(int32 SourceIdx)
{
	Sources.RemoveAtSwap(SourceIdx, 1, false);
	SourceGuids.RemoveAtSwap(SourceIdx, 1, false);
	SourceSettings.RemoveAtSwap(SourceIdx, 1, false);
}

void FLiveLinkClient::RemoveSource(FGuid InEntryGuid)
{
	LastValidationCheck = 0.0; //Force validation check next frame
	int32 SourceIdx = GetSourceIndexForGUID(InEntryGuid);
	if (SourceIdx != INDEX_NONE)
	{
		SourcesToRemove.Add(Sources[SourceIdx]);
		RemoveSourceInternal(SourceIdx);
		OnLiveLinkSourcesChanged.Broadcast();
	}
}

void FLiveLinkClient::RemoveSource(TSharedPtr<ILiveLinkSource> InSource)
{
	LastValidationCheck = 0.0; //Force validation check next frame
	int32 SourceIdx = GetSourceIndexForPointer(InSource);
	if (SourceIdx != INDEX_NONE)
	{
		SourcesToRemove.Add(Sources[SourceIdx]);
		RemoveSourceInternal(SourceIdx);
		OnLiveLinkSourcesChanged.Broadcast();
	}
}

void FLiveLinkClient::RemoveAllSources()
{
	LastValidationCheck = 0.0; //Force validation check next frame
	SourcesToRemove = Sources;
	Sources.Reset();
	SourceGuids.Reset();
	SourceSettings.Reset();

	AddVirtualSubjectSource();
	OnLiveLinkSourcesChanged.Broadcast();
}

void FLiveLinkClient::PushSubjectSkeleton(FGuid SourceGuid, FName SubjectName, const FLiveLinkRefSkeleton& RefSkeleton)
{
	FScopeLock Lock(&SubjectDataAccessCriticalSection);

	if (FLiveLinkSubject* Subject = LiveSubjectData.Find(SubjectName))
	{
		Subject->Frames.Reset();
		Subject->SetRefSkeleton(RefSkeleton);
		Subject->LastModifier = SourceGuid;
	}
	else
	{
		LiveSubjectData.Emplace(SubjectName, FLiveLinkSubject(RefSkeleton, SubjectName)).LastModifier = SourceGuid;
	}
}

void FLiveLinkClient::ClearSubject(FName SubjectName)
{
	FScopeLock Lock(&SubjectDataAccessCriticalSection);

	LiveSubjectData.Remove(SubjectName);
}

void FLiveLinkClient::ClearSubjectsFrames(FName SubjectName)
{
	FScopeLock Lock(&SubjectDataAccessCriticalSection);
	if (FLiveLinkSubject* Subject = LiveSubjectData.Find(SubjectName))
	{
		Subject->ClearFrames();
	}
}

void FLiveLinkClient::ClearAllSubjectsFrames()
{
	FScopeLock Lock(&SubjectDataAccessCriticalSection);
	for (TPair<FName, FLiveLinkSubject>& Subject : LiveSubjectData)
	{
		Subject.Value.ClearFrames();
	}

}
void FLiveLinkClient::PushSubjectData(FGuid SourceGuid, FName SubjectName, const FLiveLinkFrameData& FrameData)
{
	FScopeLock Lock(&SubjectDataAccessCriticalSection);

	if (FLiveLinkSubject* Subject = LiveSubjectData.Find(SubjectName))
	{
		Subject->AddFrame(FrameData, SourceGuid, bSaveFrames);
	}
}

const FLiveLinkSubjectFrame* FLiveLinkClient::GetSubjectData(FName SubjectName)
{
	if (FLiveLinkSubjectFrame* Subject = ActiveSubjectSnapshots.Find(SubjectName))
	{
		return Subject;
	}
	return nullptr;
}

const FLiveLinkSubjectFrame* FLiveLinkClient::GetSubjectDataAtWorldTime(FName SubjectName, double WorldTime)
{
	FLiveLinkSubjectFrame* OutFrame = nullptr;

	FLiveLinkSubject* Subject;
	FScopeLock Lock(&SubjectDataAccessCriticalSection);

	Subject = LiveSubjectData.Find(SubjectName);

	if (Subject != nullptr)
	{
		OutFrame = new FLiveLinkSubjectFrame();
		Subject->GetFrameAtWorldTime(WorldTime, *OutFrame);
	}
	else
	{
		// Try Virtual Subjects
		// TODO: Currently only works on real subjects
	}

	return OutFrame;
}

const FLiveLinkSubjectFrame* FLiveLinkClient::GetSubjectDataAtSceneTime(FName SubjectName, const FTimecode& Timecode)
{
	FLiveLinkSubjectFrame* OutFrame = nullptr;

	FLiveLinkSubject* Subject;
	FScopeLock Lock(&SubjectDataAccessCriticalSection);

	Subject = LiveSubjectData.Find(SubjectName);

	if (Subject != nullptr)
	{
		const FFrameRate FrameRate = FApp::GetTimecodeFrameRate();
		const FQualifiedFrameTime UseTime(Timecode.ToFrameNumber(FrameRate), FrameRate);

		OutFrame = new FLiveLinkSubjectFrame();
		Subject->GetFrameAtSceneTime(UseTime, *OutFrame);
	}
	else
	{
		// Try Virtual Subjects
		// TODO: Currently only works on real subjects
	}

	return OutFrame;
}

const TArray<FLiveLinkFrame>*	FLiveLinkClient::GetSubjectRawFrames(FName SubjectName)
{
	FLiveLinkSubject* Subject;
	FScopeLock Lock(&SubjectDataAccessCriticalSection);

	Subject = LiveSubjectData.Find(SubjectName);
	TArray<FLiveLinkFrame>*  Frames = nullptr;
	if (Subject != nullptr)
	{
		Frames = &Subject->Frames;
	}
	return Frames;
}

TArray<FLiveLinkSubjectKey> FLiveLinkClient::GetSubjects()
{
	TArray<FLiveLinkSubjectKey> SubjectEntries;
	{
		FScopeLock Lock(&SubjectDataAccessCriticalSection);

		SubjectEntries.Reserve(LiveSubjectData.Num() + VirtualSubjects.Num());

		for (const TPair<FName, FLiveLinkSubject>& LiveSubject : LiveSubjectData)
		{
			SubjectEntries.Emplace(LiveSubject.Key, LiveSubject.Value.LastModifier);
		}
	}

	for (TPair<FName, FLiveLinkVirtualSubject>& VirtualSubject : VirtualSubjects)
	{
		const int32 NewItem = SubjectEntries.Emplace(VirtualSubject.Key, VirtualSubjectGuid);
	}

	return SubjectEntries;
}

FLiveLinkSubjectTimeSyncData FLiveLinkClient::GetTimeSyncData(FName SubjectName)
{
	FScopeLock Lock(&SubjectDataAccessCriticalSection);

	FLiveLinkSubjectTimeSyncData SyncData;
	if (FLiveLinkSubject* Subject = LiveSubjectData.Find(SubjectName))
	{
		SyncData = Subject->GetTimeSyncData();
	}

	return SyncData;
}

FLiveLinkSubjectTimeSyncData FLiveLinkSubject::GetTimeSyncData()
{
	FLiveLinkSubjectTimeSyncData SyncData;
	SyncData.bIsValid = Frames.Num() > 0;
	SyncData.Settings = CachedSettings.TimeSynchronizationSettings.Get(FLiveLinkTimeSynchronizationSettings());

	if (SyncData.bIsValid)
	{
		SyncData.NewestSampleTime = Frames.Last().MetaData.SceneTime.Time;
		SyncData.OldestSampleTime = Frames[0].MetaData.SceneTime.Time;
		SyncData.SkeletonGuid = RefSkeletonGuid;
	}

	return SyncData;
}

void FLiveLinkClient::GetSubjectNames(TArray<FName>& SubjectNames)
{
	SubjectNames.Reset();
	{
		FScopeLock Lock(&SubjectDataAccessCriticalSection);

		SubjectNames.Reserve(LiveSubjectData.Num() + VirtualSubjects.Num());

		for (const TPair<FName, FLiveLinkSubject>& LiveSubject : LiveSubjectData)
		{
			SubjectNames.Emplace(LiveSubject.Key);
		}
	}

	for (TPair<FName, FLiveLinkVirtualSubject>& VirtualSubject : VirtualSubjects)
	{
		SubjectNames.Emplace(VirtualSubject.Key);
	}
}

bool FLiveLinkClient::GetSaveFrames() const
{
	return bSaveFrames;
}

bool FLiveLinkClient::SetSaveFrames(bool InSave)
{
	bool Prev = bSaveFrames;
	if (bSaveFrames != InSave)
	{
		bSaveFrames = InSave;
	}
	return Prev;
}


int32 FLiveLinkClient::GetSourceIndexForPointer(TSharedPtr<ILiveLinkSource> InSource) const
{
	return Sources.IndexOfByKey(InSource);
}

int32 FLiveLinkClient::GetSourceIndexForGUID(FGuid InEntryGuid) const
{
	return SourceGuids.IndexOfByKey(InEntryGuid);
}

TSharedPtr<ILiveLinkSource> FLiveLinkClient::GetSourceForGUID(FGuid InEntryGuid) const
{
	int32 Idx = GetSourceIndexForGUID(InEntryGuid);
	return Idx != INDEX_NONE ? Sources[Idx] : nullptr;
}

FText FLiveLinkClient::GetSourceTypeForEntry(FGuid InEntryGuid) const
{
	TSharedPtr<ILiveLinkSource> Source = GetSourceForGUID(InEntryGuid);
	if (Source.IsValid())
	{
		return Source->GetSourceType();
	}
	return FText(NSLOCTEXT("TempLocTextLiveLink", "InvalidSourceType", "Invalid Source Type"));
}

FText FLiveLinkClient::GetMachineNameForEntry(FGuid InEntryGuid) const
{
	TSharedPtr<ILiveLinkSource> Source = GetSourceForGUID(InEntryGuid);
	if (Source.IsValid())
	{
		return Source->GetSourceMachineName();
	}
	return FText(NSLOCTEXT("TempLocTextLiveLink", "InvalidSourceMachineName", "Invalid Source Machine Name"));
}

bool FLiveLinkClient::ShowSourceInUI(FGuid InEntryGuid) const
{
	TSharedPtr<ILiveLinkSource> Source = GetSourceForGUID(InEntryGuid);
	if (Source.IsValid())
	{
		return Source->CanBeDisplayedInUI();
	}
	return false;
}

bool FLiveLinkClient::IsVirtualSubject(const FLiveLinkSubjectKey& Subject) const
{
	return Subject.Source == VirtualSubjectGuid && VirtualSubjects.Contains(Subject.SubjectName);
}

FText FLiveLinkClient::GetEntryStatusForEntry(FGuid InEntryGuid) const
{
	TSharedPtr<ILiveLinkSource> Source = GetSourceForGUID(InEntryGuid);
	if (Source.IsValid())
	{
		return Source->GetSourceStatus();
	}
	return FText(NSLOCTEXT("TempLocTextLiveLink", "InvalidSourceStatus", "Invalid Source Status"));
}

FLiveLinkInterpolationSettings* FLiveLinkClient::GetInterpolationSettingsForEntry(FGuid InEntryGuid)
{
	const int32 SourceIndex = GetSourceIndexForGUID(InEntryGuid);
	return (SourceIndex != INDEX_NONE) ? &SourceSettings[SourceIndex]->InterpolationSettings : nullptr;
}

ULiveLinkSourceSettings* FLiveLinkClient::GetSourceSettingsForEntry(FGuid InEntryGuid)
{
	const int32 SourceIndex = GetSourceIndexForGUID(InEntryGuid);
	return (SourceIndex != INDEX_NONE) ? SourceSettings[SourceIndex] : nullptr;
}

void FLiveLinkClient::UpdateVirtualSubjectProperties(const FLiveLinkSubjectKey& Subject, const FLiveLinkVirtualSubject& VirtualSubject)
{
	if (Subject.Source == VirtualSubjectGuid)
	{
		FLiveLinkVirtualSubject& ExistingVirtualSubject = VirtualSubjects.FindOrAdd(Subject.SubjectName);
		ExistingVirtualSubject = VirtualSubject;
		ExistingVirtualSubject.InvalidateSubjectGuids();
	}
}

FLiveLinkVirtualSubject FLiveLinkClient::GetVirtualSubjectProperties(const FLiveLinkSubjectKey& SubjectKey) const
{
	check(SubjectKey.Source == VirtualSubjectGuid);

	return VirtualSubjects.FindChecked(SubjectKey.SubjectName);
}

void FLiveLinkClient::OnPropertyChanged(FGuid InEntryGuid, const FPropertyChangedEvent& PropertyChangedEvent)
{
	const int32 SourceIndex = GetSourceIndexForGUID(InEntryGuid);
	if (SourceIndex != INDEX_NONE)
	{
		Sources[SourceIndex]->OnSettingsChanged(SourceSettings[SourceIndex], PropertyChangedEvent);
	}
}

FDelegateHandle FLiveLinkClient::RegisterSourcesChangedHandle(const FSimpleMulticastDelegate::FDelegate& SourcesChanged)
{
	return OnLiveLinkSourcesChanged.Add(SourcesChanged);
}

void FLiveLinkClient::UnregisterSourcesChangedHandle(FDelegateHandle Handle)
{
	OnLiveLinkSourcesChanged.Remove(Handle);
}

FDelegateHandle FLiveLinkClient::RegisterSubjectsChangedHandle(const FSimpleMulticastDelegate::FDelegate& SubjectsChanged)
{
	return OnLiveLinkSubjectsChanged.Add(SubjectsChanged);
}

void FLiveLinkClient::UnregisterSubjectsChangedHandle(FDelegateHandle Handle)
{
	OnLiveLinkSubjectsChanged.Remove(Handle);
}

FText FLiveLinkVirtualSubjectSource::GetSourceType() const
{
	return NSLOCTEXT("TempLocTextLiveLink", "LiveLinkVirtualSubjectName", "Virtual Subjects");
}

void FLiveLinkClient::OnStartSynchronization(FName SubjectName, const struct FTimeSynchronizationOpenData& OpenData, const int32 FrameOffset)
{
	FScopeLock Lock(&SubjectDataAccessCriticalSection);
	if (FLiveLinkSubject* Subject = LiveSubjectData.Find(SubjectName))
	{
		Subject->OnStartSynchronization(OpenData, FrameOffset);
	}
}

void FLiveLinkClient::OnSynchronizationEstablished(FName SubjectName, const struct FTimeSynchronizationStartData& StartData)
{
	FScopeLock Lock(&SubjectDataAccessCriticalSection);
	if (FLiveLinkSubject* Subject = LiveSubjectData.Find(SubjectName))
	{
		Subject->OnSynchronizationEstablished(StartData);
	}
}

void FLiveLinkClient::OnStopSynchronization(FName SubjectName)
{
	FScopeLock Lock(&SubjectDataAccessCriticalSection);
	if (FLiveLinkSubject* Subject = LiveSubjectData.Find(SubjectName))
	{
		Subject->OnStopSynchronization();
	}
}

void FLiveLinkSubject::OnStartSynchronization(const FTimeSynchronizationOpenData& OpenData, const int32 FrameOffset)
{
	if (ensure(CachedSettings.SourceMode == ELiveLinkSourceMode::TimeSynchronized))
	{
		ensure(!TimeSyncData.IsSet());
		TimeSyncData = FLiveLinkTimeSynchronizationData();
		TimeSyncData->RolloverModulus = OpenData.RolloverFrame;
		TimeSyncData->SyncFrameRate = OpenData.SynchronizationFrameRate;
		TimeSyncData->Offset = FrameOffset;

		// Still need to check this, because OpenData.RolloverFrame is a TOptional which may be unset.
		if (TimeSyncData->RolloverModulus.IsSet())
		{
			TimeSyncData->RolloverModulus = FFrameRate::TransformTime(TimeSyncData->RolloverModulus.GetValue(), OpenData.SynchronizationFrameRate, CachedSettings.TimeSynchronizationSettings->FrameRate);
		}

		ClearFrames();
	}
	else
	{
		TimeSyncData.Reset();
	}
}

void FLiveLinkSubject::OnSynchronizationEstablished(const struct FTimeSynchronizationStartData& StartData)
{
	if (ensure(CachedSettings.SourceMode == ELiveLinkSourceMode::TimeSynchronized))
	{
		TimeSyncData->SyncStartTime = StartData.StartFrame;
		TimeSyncData->bHasEstablishedSync = true;

		// Prevent buffers from being deleted if new data is pushed before we build snapshots.
		LastReadTime = 0.f;
		LastReadFrame = 0.f;
	}
}

void FLiveLinkSubject::OnStopSynchronization()
{
	if (ensure(CachedSettings.SourceMode == ELiveLinkSourceMode::TimeSynchronized))
	{
		TimeSyncData.Reset();
	}
}