// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkBlueprintStructs.h"
#include "Misc/QualifiedFrameTime.h"

// FCachedSubjectFrame

FCachedSubjectFrame::FCachedSubjectFrame() 
	: bHaveCachedCurves(false)
{

};

FCachedSubjectFrame::FCachedSubjectFrame(const FLiveLinkSubjectFrame* InSourceFrame)
	: bHaveCachedCurves(false)
{
	SourceFrame = *InSourceFrame;
	int NumTransforms = SourceFrame.Transforms.Num();
	check(SourceFrame.RefSkeleton.GetBoneNames().Num() == NumTransforms);
	check(SourceFrame.RefSkeleton.GetBoneParents().Num() == NumTransforms);
	RootSpaceTransforms.SetNum(NumTransforms);
	ChildTransformIndices.SetNum(NumTransforms);
	for (int i = 0; i < NumTransforms; ++i)
	{
		RootSpaceTransforms[i].Key = false;
		ChildTransformIndices[i].Key = false;
	}
};

void FCachedSubjectFrame::SetCurvesFromCache(TMap<FName, float>& OutCurves)
{
	if (!bHaveCachedCurves)
	{
		CacheCurves();
	}
	OutCurves = CachedCurves;
};

void FCachedSubjectFrame::GetSubjectMetadata(FSubjectMetadata& SubjectMetadata)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SubjectMetadata.StringMetadata = SourceFrame.MetaData.StringMetaData;
	FQualifiedFrameTime QualifiedFrameTime = SourceFrame.MetaData.SceneTime;
	SubjectMetadata.SceneTimecode = FTimecode::FromFrameNumber(QualifiedFrameTime.Time.FrameNumber, QualifiedFrameTime.Rate, false);
	SubjectMetadata.SceneFramerate = QualifiedFrameTime.Rate;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
};

int FCachedSubjectFrame::GetNumberOfTransforms()
{
	return SourceFrame.Transforms.Num();
};

void FCachedSubjectFrame::GetTransformNames(TArray<FName>& TransformNames)
{
	TransformNames = SourceFrame.RefSkeleton.GetBoneNames();
};

void FCachedSubjectFrame::GetTransformName(const int TransformIndex, FName& Name)
{
	if (IsValidTransformIndex(TransformIndex))
	{
		Name = SourceFrame.RefSkeleton.GetBoneNames()[TransformIndex];
	}
	else
	{
		Name = TEXT("None");
	}
};

int FCachedSubjectFrame::GetTransformIndexFromName(FName TransformName)
{
	return SourceFrame.RefSkeleton.GetBoneNames().IndexOfByKey(TransformName);
};

int FCachedSubjectFrame::GetParentTransformIndex(const int TransformIndex)
{
	if (IsValidTransformIndex(TransformIndex))
	{
		return SourceFrame.RefSkeleton.GetBoneParents()[TransformIndex];
	}
	else
	{
		return -1;
	}
};

void FCachedSubjectFrame::GetChildTransformIndices(const int TransformIndex, TArray<int>& ChildIndices)
{
	ChildIndices.Reset();
	if (IsValidTransformIndex(TransformIndex))
	{
		TPair<bool, TArray<int>>& ChildIndicesCache = ChildTransformIndices[TransformIndex];
		bool bHasValidCache = ChildIndicesCache.Key;
		TArray<int>& CachedChildIndices = ChildIndicesCache.Value;
		if (!bHasValidCache)
		{
			// Build Cache
			const TArray<int32>& BoneParents = SourceFrame.RefSkeleton.GetBoneParents();
			for (int ChildIndex = 0; ChildIndex < BoneParents.Num(); ++ChildIndex)
			{
				if (BoneParents[ChildIndex] == TransformIndex)
				{
					CachedChildIndices.Emplace(ChildIndex);
				}
			}
			ChildIndicesCache.Key = true;
		}
		ChildIndices = CachedChildIndices;
	}
}

void FCachedSubjectFrame::GetTransformParentSpace(const int TransformIndex, FTransform& Transform)
{
	// Case: Root joint or invalid
	Transform = FTransform::Identity;
	// Case: Joint in SourceFrame
	if (IsValidTransformIndex(TransformIndex))
	{
		Transform = SourceFrame.Transforms[TransformIndex];
	}
};

void FCachedSubjectFrame::GetTransformRootSpace(const int TransformIndex, FTransform& Transform)
{
	// Case: Root joint or invalid
	Transform = FTransform::Identity;
	if (IsValidTransformIndex(TransformIndex))
	{
		TPair<bool, FTransform>& RootSpaceCache = RootSpaceTransforms[TransformIndex];
		bool bHasValidCache = RootSpaceCache.Key;
		// Case: Have Cached Value
		if (bHasValidCache)
		{
			Transform = RootSpaceCache.Value;
		}
		// Case: Need to generate Cache
		else
		{
			const TArray<int32>& BoneParents = SourceFrame.RefSkeleton.GetBoneParents();
			int32 ParentIndex = BoneParents[TransformIndex];

			FTransform& LocalSpaceTransform = SourceFrame.Transforms[TransformIndex];

			FTransform ParentRootSpaceTransform;
			GetTransformRootSpace(ParentIndex, ParentRootSpaceTransform);

			Transform = LocalSpaceTransform * ParentRootSpaceTransform;

			// Save cached results
			RootSpaceCache.Key = true;
			RootSpaceCache.Value = Transform;
		}
	}
};

int FCachedSubjectFrame::GetRootIndex()
{
	const TArray<int32>& BoneParents = SourceFrame.RefSkeleton.GetBoneParents();
	return BoneParents.IndexOfByPredicate([](const int32& ParentIndex) { return ParentIndex < 0; });
};

void FCachedSubjectFrame::CacheCurves()
{
	TArray<FName>& CurveNames = SourceFrame.CurveKeyData.CurveNames;
	int MapSize = FMath::Min(CurveNames.Num(), SourceFrame.Curves.Num());
	CachedCurves.Reserve(MapSize);
	for (int CurveIdx = 0; CurveIdx < MapSize; ++CurveIdx)
	{
		FOptionalCurveElement& CurveElement = SourceFrame.Curves[CurveIdx];
		CachedCurves.Add(CurveNames[CurveIdx], CurveElement.IsValid() ? CurveElement.Value : 0.0f);
	}

	bHaveCachedCurves = true;
};

bool FCachedSubjectFrame::IsValidTransformIndex(int TransformIndex)
{
	return (TransformIndex >= 0) && (TransformIndex < SourceFrame.Transforms.Num());
};

// FLiveLinkTransform

FLiveLinkTransform::FLiveLinkTransform()
// Initialise with invalid index to force transforms
// to evaluate as identity
	: TransformIndex(-1)
{
};

void FLiveLinkTransform::GetName(FName& Name)
{
	if (CachedFrame.IsValid())
	{
		CachedFrame->GetTransformName(TransformIndex, Name);
	}
};

void FLiveLinkTransform::GetTransformParentSpace(FTransform& Transform)
{
	if (CachedFrame.IsValid())
	{
		CachedFrame->GetTransformParentSpace(TransformIndex, Transform);
	}
};

void FLiveLinkTransform::GetTransformRootSpace(FTransform& Transform)
{
	if (CachedFrame.IsValid())
	{
		CachedFrame->GetTransformRootSpace(TransformIndex, Transform);
	}
};

bool FLiveLinkTransform::HasParent()
{
	if (CachedFrame.IsValid())
	{
		return CachedFrame->GetParentTransformIndex(TransformIndex) >= 0;
	}
	else
	{
		return false;
	}
};

void FLiveLinkTransform::GetParent(FLiveLinkTransform& ParentTransform)
{
	if (CachedFrame.IsValid())
	{
		int ParentIndex = CachedFrame->GetParentTransformIndex(TransformIndex);
		ParentTransform.SetCachedFrame(CachedFrame);
		ParentTransform.SetTransformIndex(ParentIndex);
	}
};

int FLiveLinkTransform::GetChildCount()
{
	if (CachedFrame.IsValid())
	{
		TArray<int> ChildIndices;
		CachedFrame->GetChildTransformIndices(TransformIndex, ChildIndices);
		return ChildIndices.Num();
	}
	else
	{
		return 0;
	}
};

void FLiveLinkTransform::GetChildren(TArray<FLiveLinkTransform>& ChildTransforms)
{
	ChildTransforms.Reset();
	if (CachedFrame.IsValid())
	{
		TArray<int> ChildIndices;
		CachedFrame->GetChildTransformIndices(TransformIndex, ChildIndices);
		for (const int ChildIndex : ChildIndices)
		{
			int32 NewIndex = ChildTransforms.AddDefaulted();
			ChildTransforms[NewIndex].SetCachedFrame(CachedFrame);
			ChildTransforms[NewIndex].SetTransformIndex(ChildIndex);
		}
	}
};

void FLiveLinkTransform::SetCachedFrame(TSharedPtr<FCachedSubjectFrame> InCachedFrame)
{
	CachedFrame = InCachedFrame;
};

void FLiveLinkTransform::SetTransformIndex(const int InTransformIndex)
{
	TransformIndex = InTransformIndex;
};

int FLiveLinkTransform::GetTransformIndex() const
{
	return TransformIndex;
};

// FSubjectFrameHandle

void FSubjectFrameHandle::GetCurves(TMap<FName, float>& Curves)
{
	if (CachedFrame.IsValid())
	{
		CachedFrame->SetCurvesFromCache(Curves);
	}
};

void FSubjectFrameHandle::GetSubjectMetadata(FSubjectMetadata& Metadata)
{
	if (CachedFrame.IsValid())
	{
		CachedFrame->GetSubjectMetadata(Metadata);
	}
};

int FSubjectFrameHandle::GetNumberOfTransforms()
{
	if (CachedFrame.IsValid())
	{
		return CachedFrame->GetNumberOfTransforms();
	}
	else
	{
		return 0;
	}
};

void FSubjectFrameHandle::GetTransformNames(TArray<FName>& TransformNames)
{
	if (CachedFrame.IsValid())
	{
		CachedFrame->GetTransformNames(TransformNames);
	}
};

void FSubjectFrameHandle::GetRootTransform(FLiveLinkTransform& LiveLinkTransform)
{
	LiveLinkTransform.SetCachedFrame(CachedFrame);
	if (CachedFrame.IsValid())
	{
		LiveLinkTransform.SetTransformIndex(CachedFrame->GetRootIndex());
	}
}

void FSubjectFrameHandle::GetTransformByIndex(int TransformIndex, FLiveLinkTransform& LiveLinkTransform)
{
	LiveLinkTransform.SetCachedFrame(CachedFrame);
	LiveLinkTransform.SetTransformIndex(TransformIndex);
};

void FSubjectFrameHandle::GetTransformByName(FName TransformName, FLiveLinkTransform& LiveLinkTransform)
{
	int TransformIndex = CachedFrame->GetTransformIndexFromName(TransformName);
	LiveLinkTransform.SetCachedFrame(CachedFrame);
	LiveLinkTransform.SetTransformIndex(TransformIndex);
};

void FSubjectFrameHandle::SetCachedFrame(TSharedPtr<FCachedSubjectFrame> InCachedFrame)
{
	CachedFrame = InCachedFrame;
};

void FLiveLinkSourceHandle::SetSourcePointer(TSharedPtr<ILiveLinkSource> InSourcePointer)
{
	SourcePointer = InSourcePointer;
};