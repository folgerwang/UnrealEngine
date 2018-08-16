// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkTypes.h"
#include "Misc/Timecode.h"
#include "Misc/FrameRate.h"
#include "ILiveLinkSource.h"
#include "LiveLinkBlueprintStructs.generated.h"

USTRUCT(BlueprintType)
struct FSubjectMetadata
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LiveLink")
	TMap<FName, FString> StringMetadata;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LiveLink")
	FTimecode SceneTimecode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LiveLink")
	FFrameRate SceneFramerate;
};

USTRUCT()
struct FCachedSubjectFrame
{
	GENERATED_USTRUCT_BODY()

	FCachedSubjectFrame();

	FCachedSubjectFrame(const FLiveLinkSubjectFrame* InSourceFrame);

	virtual ~FCachedSubjectFrame() = default;

	void SetCurvesFromCache(TMap<FName, float>& OutCurves);

	void GetSubjectMetadata(FSubjectMetadata& SubjectMetadata);

	int GetNumberOfTransforms();

	void GetTransformNames(TArray<FName>& TransformNames);

	void GetTransformName(const int TransformIndex, FName& Name);

	int GetTransformIndexFromName(FName TransformName);

	int GetParentTransformIndex(const int TransformIndex);

	void GetChildTransformIndices(const int TransformIndex, TArray<int>& ChildIndices);

	void GetTransformParentSpace(const int TransformIndex, FTransform& Transform);

	void GetTransformRootSpace(const int TransformIndex, FTransform& Transform);

	int GetRootIndex();

private:
	FLiveLinkSubjectFrame SourceFrame;
	TArray<TPair<bool, FTransform>> RootSpaceTransforms;
	TArray<TPair<bool, TArray<int>>> ChildTransformIndices;
	TMap<FName, float> CachedCurves;

	bool bHaveCachedCurves;

	void CacheCurves();

	bool IsValidTransformIndex(int TransformIndex);
};

USTRUCT(BlueprintType)
struct FLiveLinkTransform
{
	GENERATED_USTRUCT_BODY()

	FLiveLinkTransform();

	virtual ~FLiveLinkTransform() = default;

	void GetName(FName& Name);

	void GetTransformParentSpace(FTransform& Transform);

	void GetTransformRootSpace(FTransform& Transform);

	bool HasParent();

	void GetParent(FLiveLinkTransform& ParentTransform);

	int GetChildCount();

	void GetChildren(TArray<FLiveLinkTransform>& ChildTransforms);

	void SetCachedFrame(TSharedPtr<FCachedSubjectFrame> InCachedFrame);

	void SetTransformIndex(const int InTransformIndex);

	int GetTransformIndex() const;

private:
	TSharedPtr<FCachedSubjectFrame> CachedFrame;
	int TransformIndex;
};

USTRUCT(BlueprintType)
struct FSubjectFrameHandle
{
	GENERATED_USTRUCT_BODY()

	FSubjectFrameHandle() = default;

	virtual ~FSubjectFrameHandle() = default;

	void GetCurves(TMap<FName, float>& Curves);

	void GetSubjectMetadata(FSubjectMetadata& Metadata);

	int GetNumberOfTransforms();

	void GetTransformNames(TArray<FName>& TransformNames);

	void GetRootTransform(FLiveLinkTransform& LiveLinkTransform);

	void GetTransformByIndex(int TransformIndex, FLiveLinkTransform& LiveLinkTransform);

	void GetTransformByName(FName TransformName, FLiveLinkTransform& LiveLinkTransform);

	void SetCachedFrame(TSharedPtr<FCachedSubjectFrame> InCachedFrame);
private:
	TSharedPtr<FCachedSubjectFrame> CachedFrame;
};

