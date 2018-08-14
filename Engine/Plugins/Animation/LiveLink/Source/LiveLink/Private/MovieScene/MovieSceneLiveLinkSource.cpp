// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneLiveLinkSource.h"
#include "Features/IModularFeatures.h"

FMovieSceneLiveLinkSource::FMovieSceneLiveLinkSource():
	Client(nullptr)
	, LastFramePublished(0)
{
}

TSharedPtr<FMovieSceneLiveLinkSource> FMovieSceneLiveLinkSource::CreateLiveLinkSource()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();

	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		ILiveLinkClient* LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		TSharedPtr <FMovieSceneLiveLinkSource> Source = MakeShareable(new FMovieSceneLiveLinkSource());
		LiveLinkClient->AddSource(Source);
		return Source;
	}
	return TSharedPtr<FMovieSceneLiveLinkSource>();
}

void FMovieSceneLiveLinkSource::RemoveLiveLinkSource(TSharedPtr<FMovieSceneLiveLinkSource> Source)
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();

	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		ILiveLinkClient* LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		LiveLinkClient->RemoveSource(Source);
	}
}
void FMovieSceneLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;
}

bool FMovieSceneLiveLinkSource::IsSourceStillValid()
{
	return Client != nullptr;
}

bool FMovieSceneLiveLinkSource::RequestSourceShutdown()
{
	Client = nullptr;
	return true;
}

FText FMovieSceneLiveLinkSource::GetSourceMachineName() const
{
	return FText().FromString(FPlatformProcess::ComputerName());
}

FText FMovieSceneLiveLinkSource::GetSourceStatus() const
{
	return NSLOCTEXT( "MovieSceneLiveLinkSource", "MovieSceneLiveLinkSourceStatus", "Active" );
}

FText FMovieSceneLiveLinkSource::GetSourceType() const
{
	return FText::Format(NSLOCTEXT("FMovieSceneLiveLinkSource", "MovieSceneLiveLinkSourceType", "Sequencer Live Link ({0})"),FText::FromName(LastSubjectName));
}


void FMovieSceneLiveLinkSource::PublishLiveLinkFrameData(const FName& SubjectName, const TArray<FLiveLinkFrameData>& LiveLinkFrameDataArray, const FLiveLinkRefSkeleton& RefSkeleton)
{

	check(Client != nullptr);

	if (SubjectName != LastSubjectName)
	{
		// We need to publish a skeleton for this subject name even though we doesn't use one
		Client->PushSubjectSkeleton(SourceGuid, SubjectName, RefSkeleton);
	}
	LastSubjectName = SubjectName;
	for(FLiveLinkFrameData LiveLinkFrame: LiveLinkFrameDataArray)
	{
		// Share the data locally with the LiveLink client
		Client->PushSubjectData(SourceGuid, SubjectName, LiveLinkFrame);
	}
}



