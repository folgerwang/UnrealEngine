// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkMessageBusFinder.h"

#include "Engine/Engine.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "LiveLinkMessages.h"
#include "LiveLinkMessageBusSource.h"
#include "MessageEndpointBuilder.h"

ULiveLinkMessageBusFinder::ULiveLinkMessageBusFinder()
	: MessageEndpoint(nullptr)
{

};

void ULiveLinkMessageBusFinder::GetAvailableProviders(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, float Duration, TArray<FProviderPollResult>& AvailableProviders)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentManager = World->GetLatentActionManager();
		if (LatentManager.FindExistingAction<FLiveLinkMessageBusFinderAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			PollNetwork();

			FLiveLinkMessageBusFinderAction* NewAction = new FLiveLinkMessageBusFinderAction(LatentInfo, this, Duration, AvailableProviders);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GetAvailableProviders not executed. The previous action hasn't finished yet."));
		}
	}
};

void ULiveLinkMessageBusFinder::GetPollResults(TArray<FProviderPollResult>& AvailableProviders)
{
	FScopeLock ScopedLock(&PollDataCriticalSection);
	AvailableProviders = PollData;
};

void ULiveLinkMessageBusFinder::PollNetwork()
{
	if (!MessageEndpoint.IsValid())
	{
		MessageEndpoint = FMessageEndpoint::Builder(TEXT("LiveLinkMessageBusSource"))
			.Handling<FLiveLinkPongMessage>(this, &ULiveLinkMessageBusFinder::HandlePongMessage);
	}

	PollData.Reset();
	CurrentPollRequest = FGuid::NewGuid();
	MessageEndpoint->Publish(new FLiveLinkPingMessage(CurrentPollRequest));
};

void ULiveLinkMessageBusFinder::HandlePongMessage(const FLiveLinkPongMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (Message.PollRequest == CurrentPollRequest)
	{
		FScopeLock ScopedLock(&PollDataCriticalSection);
		PollData.Add(FProviderPollResult(Context->GetSender(), Message.ProviderName, Message.MachineName));
	}
};

void ULiveLinkMessageBusFinder::ConnectToProvider(UPARAM(ref) FProviderPollResult& Provider, FLiveLinkSourceHandle& SourceHandle)
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();

	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		TSharedPtr<FLiveLinkMessageBusSource> NewSource = MakeShared<FLiveLinkMessageBusSource>(FText::FromString(Provider.Name), FText::FromString(Provider.MachineName), Provider.Address);
		LiveLinkClient->AddSource(NewSource);
		SourceHandle.SetSourcePointer(NewSource);
	}
	else
	{
		SourceHandle.SetSourcePointer(nullptr);
	}
};

ULiveLinkMessageBusFinder* ULiveLinkMessageBusFinder::ConstructMessageBusFinder()
{
	return NewObject<ULiveLinkMessageBusFinder>();
}