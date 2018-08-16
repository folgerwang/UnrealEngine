// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "LiveLinkTypes.h"
#include "MovieSceneLiveLinkSection.generated.h"

/**
* A movie scene section for animating FMovieSceneLiveLink Section properties.
*/
UCLASS()
class LIVELINK_API UMovieSceneLiveLinkSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	void SetSubjectName(const FName& InSubjectName);
	int32 CreateChannelProxy(const FLiveLinkFrame &LiveLinkFrame, const FLiveLinkRefSkeleton& InRefSkeleton,const FLiveLinkCurveKey&  CurveKey);
	TArray <FMovieSceneFloatChannel> &GetFloatChannels() { return PropertyFloatChannels; }

	UPROPERTY()
	FName SubjectName;
	UPROPERTY()
	FLiveLinkFrameData TemplateToPush; //we create this when we create the channels, and then use it to create the Frame Data Array when pushing.
	UPROPERTY()
	FLiveLinkRefSkeleton RefSkeleton;
	UPROPERTY()
	TArray <FMovieSceneFloatChannel> PropertyFloatChannels;

protected:

	virtual void Serialize(FArchive& Ar) override;
	void UpdateChannelProxy();

};
