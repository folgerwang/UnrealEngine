// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkRefSkeleton.h"
#include "Features/IModularFeature.h"
#include "LiveLinkTypes.h"
#include "Misc/Guid.h"

class ILiveLinkSource;

class LIVELINKINTERFACE_API ILiveLinkClient : public IModularFeature
{
public:
	virtual ~ILiveLinkClient() {}

	static FName ModularFeatureName;

	// Add a new live link source to the client
	virtual void AddSource(TSharedPtr<ILiveLinkSource> InSource) = 0;

	// Remove the specified source from the live link client
	virtual void RemoveSource(TSharedPtr<ILiveLinkSource> InSource) = 0;

	virtual void PushSubjectSkeleton(FGuid SourceGuid, FName SubjectName, const FLiveLinkRefSkeleton& RefSkeleton) = 0;
	virtual void PushSubjectData(FGuid SourceGuid, FName SubjectName, const FLiveLinkFrameData& FrameData) = 0;
	virtual void ClearSubject(FName SubjectName) = 0;

	// Populates an array with in-use subject names
	virtual void GetSubjectNames(TArray<FName>& SubjectNames) = 0;

	virtual const FLiveLinkSubjectFrame* GetSubjectData(FName SubjectName) = 0;
};
