// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ILiveLinkSource.h"
#include "ILiveLinkClient.h"



/** Publishes LiveLink From Sequencer*/
class FMovieSceneLiveLinkSource :
	public ILiveLinkSource
{
public:
	FMovieSceneLiveLinkSource();
	virtual ~FMovieSceneLiveLinkSource() {}
	static TSharedPtr<FMovieSceneLiveLinkSource> CreateLiveLinkSource();
	static void RemoveLiveLinkSource(TSharedPtr<FMovieSceneLiveLinkSource> Source);
	void PublishLiveLinkFrameData(const FName &SubjectName, const TArray<FLiveLinkFrameData>  &LiveLinkFrameDataArray, const FLiveLinkRefSkeleton& RefSkeleton);


	// ILiveLinkSource interface
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
	virtual bool IsSourceStillValid() override;
	virtual bool RequestSourceShutdown() override;
	virtual FText GetSourceMachineName() const override;
	virtual FText GetSourceStatus() const override;
	virtual FText GetSourceType() const override;
	// End ILiveLinkSource

public:	
	/** The local client to push data updates to */
	ILiveLinkClient* Client;

	/** Our identifier in LiveLink */
	FGuid SourceGuid;

	/** The last time we sent the data. Used to not send redundant data */
	uint32 LastFramePublished;  //mz maybe not needed todo

	/** Used to track names changes */
	FName LastSubjectName;
};
