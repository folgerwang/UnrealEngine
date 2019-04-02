// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "LiveLinkTypes.h"
#include "Containers/Array.h"
#include "MovieSceneLiveLinkSection.generated.h"


/**
* A movie scene section for animating FMovieSceneLiveLink Section properties.
*/
UCLASS()
class LIVELINK_API UMovieSceneLiveLinkSection
	: public UMovieSceneSection
{
	
	GENERATED_BODY()
public:
	UMovieSceneLiveLinkSection(const FObjectInitializer& ObjectInitializer);
	void SetSubjectName(const FName& InSubjectName);
	int32 CreateChannelProxy(const FLiveLinkRefSkeleton& InRefSkeleton,const TArray<FName>& InCurveNames);
	TArray <FMovieSceneFloatChannel> &GetFloatChannels() { return PropertyFloatChannels; }

	UPROPERTY()
	FName SubjectName;
	UPROPERTY()
	FLiveLinkFrameData TemplateToPush; //we create this when we create the channels, and then use it to create the Frame Data Array when pushing.
	UPROPERTY()
	FLiveLinkRefSkeleton RefSkeleton;
	UPROPERTY()
	TArray<FName> CurveNames;	
	UPROPERTY()
	TArray <FMovieSceneFloatChannel> PropertyFloatChannels;
	// Channels that we may not send to live link or they are sent but not priority (MattH to do).
	UPROPERTY()
	TArray<bool> ChannelMask; 


public:
	
	void SetMask(const TArray<bool>& InChannelMask);

protected:

	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditImport() override;
	void UpdateChannelProxy();

};
