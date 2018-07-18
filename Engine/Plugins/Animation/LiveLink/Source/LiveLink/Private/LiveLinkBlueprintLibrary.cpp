// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkBlueprintLibrary.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"

void ULiveLinkBlueprintLibrary::GetCurves(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle, TMap<FName, float>& Curves)
{
	SubjectFrameHandle.GetCurves(Curves);
};

int ULiveLinkBlueprintLibrary::NumberOfTransforms(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle)
{
	return SubjectFrameHandle.GetNumberOfTransforms();
};

void ULiveLinkBlueprintLibrary::TransformNames(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle, TArray<FName>& TransformNames)
{
	return SubjectFrameHandle.GetTransformNames(TransformNames);
}

void ULiveLinkBlueprintLibrary::GetRootTransform(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle, FLiveLinkTransform& LiveLinkTransform)
{
	SubjectFrameHandle.GetRootTransform(LiveLinkTransform);
};

void ULiveLinkBlueprintLibrary::GetTransformByIndex(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle, int TransformIndex, FLiveLinkTransform& LiveLinkTransform)
{
	SubjectFrameHandle.GetTransformByIndex(TransformIndex, LiveLinkTransform);
};

void ULiveLinkBlueprintLibrary::GetTransformByName(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle, FName TransformName, FLiveLinkTransform& LiveLinkTransform)
{
	SubjectFrameHandle.GetTransformByName(TransformName, LiveLinkTransform);
};

void ULiveLinkBlueprintLibrary::GetMetadata(UPARAM(ref) FSubjectFrameHandle& SubjectFrameHandle, FSubjectMetadata& Metadata)
{
	SubjectFrameHandle.GetSubjectMetadata(Metadata);
};

void ULiveLinkBlueprintLibrary::TransformName(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform, FName& Name)
{
	return LiveLinkTransform.GetName(Name);
};

void ULiveLinkBlueprintLibrary::ParentBoneSpaceTransform(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform, FTransform& Transform)
{
	LiveLinkTransform.GetTransformParentSpace(Transform);
};

void ULiveLinkBlueprintLibrary::ComponentSpaceTransform(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform, FTransform& Transform)
{
	LiveLinkTransform.GetTransformRootSpace(Transform);
};

bool ULiveLinkBlueprintLibrary::HasParent(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform)
{
	return LiveLinkTransform.HasParent();
};

void ULiveLinkBlueprintLibrary::GetParent(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform, FLiveLinkTransform& Parent)
{
	LiveLinkTransform.GetParent(Parent);
};

int ULiveLinkBlueprintLibrary::ChildCount(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform)
{
	return LiveLinkTransform.GetChildCount();
};

void ULiveLinkBlueprintLibrary::GetChildren(UPARAM(ref) FLiveLinkTransform& LiveLinkTransform, TArray<FLiveLinkTransform>& Children)
{
	LiveLinkTransform.GetChildren(Children);
};

bool ULiveLinkBlueprintLibrary::IsSourceStillValid(UPARAM(ref) FLiveLinkSourceHandle& SourceHandle)
{
	return SourceHandle.SourcePointer.IsValid() && SourceHandle.SourcePointer->IsSourceStillValid();
};

bool ULiveLinkBlueprintLibrary::RequestShutdown(UPARAM(ref) FLiveLinkSourceHandle& SourceHandle)
{
	bool bSourceShutdown = SourceHandle.SourcePointer.IsValid() && SourceHandle.SourcePointer->RequestSourceShutdown();
	if (bSourceShutdown)
	{
		IModularFeatures& ModularFeatures = IModularFeatures::Get();

		if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
			LiveLinkClient->RemoveSource(SourceHandle.SourcePointer);
		}
	}
	return bSourceShutdown;
}

FText ULiveLinkBlueprintLibrary::GetSourceStatus(UPARAM(ref) FLiveLinkSourceHandle& SourceHandle)
{
	if (SourceHandle.SourcePointer.IsValid())
	{
		return SourceHandle.SourcePointer->GetSourceStatus();
	}
	else
	{
		return FText::GetEmpty();
	}
}

FText ULiveLinkBlueprintLibrary::GetSourceType(UPARAM(ref) FLiveLinkSourceHandle& SourceHandle)
{
	if (SourceHandle.SourcePointer.IsValid())
	{
		return SourceHandle.SourcePointer->GetSourceType();
	}
	else
	{
		return FText::GetEmpty();
	}
}

FText ULiveLinkBlueprintLibrary::GetSourceMachineName(UPARAM(ref) FLiveLinkSourceHandle& SourceHandle)
{
	if (SourceHandle.SourcePointer.IsValid())
	{
		return SourceHandle.SourcePointer->GetSourceMachineName();
	}
	else
	{
		return FText::GetEmpty();
	}
}