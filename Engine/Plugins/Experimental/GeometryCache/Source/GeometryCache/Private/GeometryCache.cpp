// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCache.h"
#include "EditorFramework/AssetImportData.h"
#include "Materials/MaterialInterface.h"
#include "GeometryCacheTrackTransformAnimation.h"
#include "GeometryCacheTrackFlipbookAnimation.h"
#include "UObject/FrameworkObjectVersion.h"
#include "UObject/AnimPhysObjectVersion.h"
#include "Interfaces/ITargetPlatform.h"
#include "Logging/LogMacros.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Logging/LogCategory.h"
#include "Logging/LogVerbosity.h" 

DEFINE_LOG_CATEGORY(LogGeometryCache);

#define LOCTEXT_NAMESPACE "GeometryCache"

UGeometryCache::UGeometryCache(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	StartFrame = 0;
	EndFrame = 0;
}

void UGeometryCache::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
	Super::PostInitProperties();
}

void UGeometryCache::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FAnimPhysObjectVersion::GUID);
	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	auto ShowDeprecationNotification = [this]()
	{
		Tracks.Empty();
		Materials.Empty();

		const FText ErrorText = LOCTEXT("GeometryCacheEmptied", "Geometry Cache asset has been emptied as it does not support backwards compatibility");
		FNotificationInfo Info(ErrorText);
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);

		UE_LOG(LogGeometryCache, Warning, TEXT("(%s) %s"), *ErrorText.ToString(), *GetName());
	};
		
	if (Ar.CustomVer(FAnimPhysObjectVersion::GUID) >= FAnimPhysObjectVersion::GeometryCacheAssetDeprecation)
	{
		Super::Serialize(Ar);

		if (Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::GeometryCacheFastDecoder)
		{
			ShowDeprecationNotification();
		}
	}
	else
	{
#if WITH_EDITORONLY_DATA
		if (!Ar.IsCooking() || (Ar.CookingTarget() && Ar.CookingTarget()->HasEditorOnlyData()))
		{
			Ar << AssetImportData;
		}
#endif
		Ar << Tracks;

		uint32 NumVertexAnimationTracks;
		uint32 NumTransformAnimationTracks;
		Ar << NumVertexAnimationTracks;
		Ar << NumTransformAnimationTracks;

		if (Ar.CustomVer(FFrameworkObjectVersion::GUID) >= FFrameworkObjectVersion::GeometryCacheMissingMaterials)
		{
			Ar << Materials;
		}

		ShowDeprecationNotification();
	}
}

FString UGeometryCache::GetDesc()
{
	const int32 NumTracks = Tracks.Num();
	return FString("%d Tracks", NumTracks);
}

void UGeometryCache::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	// Information on number total of (per type) tracks
	const int32 NumTracks = Tracks.Num();	
	OutTags.Add(FAssetRegistryTag("Total Tracks", FString::FromInt(NumTracks), FAssetRegistryTag::TT_Numerical));

#if WITH_EDITORONLY_DATA
	if (AssetImportData)
	{
		OutTags.Add(FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
	}
#endif

	Super::GetAssetRegistryTags(OutTags);
}

void UGeometryCache::BeginDestroy()
{
	Super::BeginDestroy();
	ReleaseResourcesFence.BeginFence();
}

void UGeometryCache::ClearForReimporting()
{
	Tracks.Empty();

	// Flush the resource release commands to the rendering thread to ensure that the edit change doesn't occur while a resource is still allocated
	ReleaseResourcesFence.Wait();
}

bool UGeometryCache::IsReadyForFinishDestroy()
{
	return ReleaseResourcesFence.IsFenceComplete();
}

#if WITH_EDITOR
void UGeometryCache::PreEditChange(UProperty* PropertyAboutToChange)
{
	// Flush the resource release commands to the rendering thread to ensure that the edit change doesn't occur while a resource is still allocated
	ReleaseResourcesFence.Wait();
}
#endif

void UGeometryCache::AddTrack(UGeometryCacheTrack* Track)
{
	Tracks.Add(Track);
}

void UGeometryCache::SetFrameStartEnd(int32 InStartFrame, int32 InEndFrame)
{
	StartFrame = InStartFrame;
	EndFrame = InEndFrame;
}

int32 UGeometryCache::GetStartFrame() const
{
	return StartFrame;
}

int32 UGeometryCache::GetEndFrame() const
{
	return EndFrame;
}

float UGeometryCache::CalculateDuration() const
{
	int32 NumTracks = Tracks.Num();
	float Duration = 0.0f;
	// Create mesh sections for each GeometryCacheTrack
	for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		const float TrackMaxSampleTime = Tracks[TrackIndex]->GetMaxSampleTime();
		Duration = (Duration > TrackMaxSampleTime) ? Duration : TrackMaxSampleTime;
	}
	return Duration;
}

int32 UGeometryCache::GetFrameAtTime(const float Time) const
{
	const float Duration = CalculateDuration();
	const int32 NumberOfFrames = GetEndFrame() - GetStartFrame() + 1;;
	const float FrameTime = NumberOfFrames > 1 ? Duration / (float)(NumberOfFrames - 1) : 0.0f;
	const int32 NormalizedFrame = FMath::Clamp(FMath::RoundToInt(Time / FrameTime), 0, NumberOfFrames - 1);
	return StartFrame + NormalizedFrame; 

}
#undef LOCTEXT_NAMESPACE // "GeometryCache"