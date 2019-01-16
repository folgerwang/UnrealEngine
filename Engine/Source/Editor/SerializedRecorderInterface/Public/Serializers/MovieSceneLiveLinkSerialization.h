// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameRate.h"
#include "LiveLinkTypes.h"
#include "Serializers/MovieSceneSectionSerialization.h"


struct FLiveLinkManifestHeader
{

	static const int32 cVersion = 1;

	FLiveLinkManifestHeader() : Version(cVersion)
	{
	}


	FLiveLinkManifestHeader(const FName& InSerializedType)
		: Version(cVersion)
		, SerializedType(InSerializedType)
		, bIsManifest(true)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FLiveLinkManifestHeader& Header)
	{
		Ar << Header.Version;
		Ar << Header.SerializedType;
		Ar << Header.bIsManifest;
		Ar << Header.SubjectNames;

		return Ar;
	}
	int32 Version;
	FName SerializedType;
	bool bIsManifest;
	TArray<FName> SubjectNames;
};

using FLiveLinkManifestSerializer = TMovieSceneSerializer<FLiveLinkManifestHeader, FLiveLinkManifestHeader>;


//Header and data for individual subjects that contain the data.
struct FLiveLinkFileHeader
{
	FLiveLinkFileHeader() = default;

	FLiveLinkFileHeader(const FName& InSubjectName, double InSecondsDiff, const FLiveLinkRefSkeleton& InRefSkeleton, const TArray<FName>&  InCurveNames, 
		const FName& InSerializedType, const FGuid& InGuid)
		: bIsManifest(false)
		, SecondsDiff(InSecondsDiff)
		, SubjectName(InSubjectName)
		, CurveNames(InCurveNames) 
		, RefSkeleton(InRefSkeleton)
		, SerializedType(InSerializedType)
		, Guid(InGuid)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FLiveLinkFileHeader& Header)
	{
		Ar << Header.SerializedType;
		Ar << Header.bIsManifest;
		Ar << Header.Guid;
		Ar << Header.SecondsDiff;
		Ar << Header.SubjectName;
		Ar << Header.CurveNames;
		FLiveLinkRefSkeleton::StaticStruct()->SerializeItem(Ar, (void*)& Header.RefSkeleton, nullptr);

		return Ar;
	}

	bool bIsManifest;
	double SecondsDiff;
	FName SubjectName;
	TArray<FName> CurveNames;
	FLiveLinkRefSkeleton RefSkeleton;
	FName SerializedType;
	FGuid Guid;
};

using FLiveLinkSerializedFrame = TMovieSceneSerializedFrame<FLiveLinkFrame>;
using FLiveLinkSerializer = TMovieSceneSerializer<FLiveLinkFileHeader, FLiveLinkFrame>;
