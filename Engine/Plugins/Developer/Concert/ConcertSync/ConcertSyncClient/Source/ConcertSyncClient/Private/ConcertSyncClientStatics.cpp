// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertSyncClientStatics.h"
#include "Modules/ModuleManager.h"
#include "IConcertSyncClientModule.h"
#include "ConcertSettings.h"
#include "ConcertLogGlobal.h"
#include "IConcertModule.h"
#include "IConcertClient.h"
#include "IConcertSession.h"
#include "ConcertMessageData.h"
#include "ConcertClientPresenceManager.h"

class FConcertSyncClientModule;

#define LOCTEXT_NAMESPACE "ConcertSyncClientStatics"

UConcertSyncClientStatics::UConcertSyncClientStatics(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

void UConcertSyncClientStatics::SetPresenceEnabled(const bool IsEnabled)
{
	if (IConcertSyncClientModule::IsAvailable())
	{
		IConcertSyncClientModule& ConcertSyncClientModule = IConcertSyncClientModule::Get();
		ConcertSyncClientModule.SetPresenceEnabled(IsEnabled);
	}
}

void UConcertSyncClientStatics::SetPresenceVisibility(FString Name, bool Visibility, bool PropagateToAll)
{
	if (IConcertSyncClientModule::IsAvailable())
	{
		IConcertSyncClientModule& ConcertSyncClientModule = IConcertSyncClientModule::Get();
		ConcertSyncClientModule.SetPresenceVisibility(Name, Visibility, PropagateToAll);
	}
}

void UConcertSyncClientStatics::UpdateWorkspaceModifiedPackages()
{
	PersistSessionChanges();
}

void UConcertSyncClientStatics::PersistSessionChanges()
{
	if (IConcertSyncClientModule::IsAvailable())
	{
		IConcertSyncClientModule::Get().PersistSessionChanges();
	}
}

FConcertClientInfo UConcertSyncClientStatics::GetLocalConcertClientInfo()
{
	return IConcertModule::Get().GetClientInstance()->GetClientInfo(); 
}

bool UConcertSyncClientStatics::GetConcertClientInfoByName(const FString ClientName, FConcertClientInfo& ClientInfo)
{
	// We return the first match by name that we find. We expect the user to avoid name collisions in the user names. 
	// @todo: We can change this behavior once Concert has unique client IDs that persist across sessions.
	const IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
	const FConcertClientInfo& LocalClientInfo = ConcertClient->GetClientInfo();		

	if (ClientName == LocalClientInfo.DisplayName)
	{
		ClientInfo = LocalClientInfo;
		return true;
	}

	const TSharedPtr<IConcertClientSession> ClientSession = ConcertClient->GetCurrentSession();
	if (ClientSession.IsValid())
	{
		const TArray<FConcertSessionClientInfo> SessionClients = ClientSession->GetSessionClients();
		for (const FConcertSessionClientInfo SessionClient : SessionClients)
		{
			if (SessionClient.ClientInfo.DisplayName == ClientName)
			{
				ClientInfo = SessionClient.ClientInfo;
				return true;
			}
		}
	}
		
	UE_LOG(LogConcert, Warning, TEXT("UConcertSyncClientStatics::GetConcertClientInfoByName - Failed to get ClientSession"));	
	return false;
}

bool UConcertSyncClientStatics::GetRemoteConcertClientInfos(TArray<FConcertClientInfo>& ClientInfos)
{
	const TSharedPtr<IConcertClientSession> ClientSession = IConcertModule::Get().GetClientInstance()->GetCurrentSession();
	if (ClientSession.IsValid())
	{
		const TArray<FConcertSessionClientInfo> SessionClients = ClientSession->GetSessionClients();
		for (const FConcertSessionClientInfo SessionClient : SessionClients)
		{
			ClientInfos.Add(SessionClient.ClientInfo);
		}

		return ClientInfos.Num() > 0;
	}

	UE_LOG(LogConcert, Warning, TEXT("UConcertSyncClientStatics::GetAllConcertClientInfos - Failed to get ClientSession"));
	return false;
}

bool UConcertSyncClientStatics::GetConcertConnectionStatus()
{
	const TSharedPtr<IConcertClientSession> ClientSession = IConcertModule::Get().GetClientInstance()->GetCurrentSession();
	if (ClientSession.IsValid())
	{
		return ClientSession->GetConnectionStatus() == EConcertConnectionStatus::Connected ? true : false;
	}
	
	UE_LOG(LogConcert, Warning, TEXT("UConcertSyncClientStatics::GetConcertConnectionStatus - Failed to get ClientSession"));
	return false;
}

void UConcertSyncClientStatics::ConcertJumpToPresence(const FString OtherUserName)
{
	if (IConcertSyncClientModule::IsAvailable())
	{
		const IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
		const TSharedPtr<IConcertClientSession> ClientSession = ConcertClient->GetCurrentSession();
		FGuid OtherClientId;

		if (ClientSession.IsValid())
		{
			const TArray<FConcertSessionClientInfo> SessionClients = ClientSession->GetSessionClients();
			for (const FConcertSessionClientInfo SessionClient : SessionClients)
			{
				if (SessionClient.ClientInfo.DisplayName == OtherUserName)
				{
					OtherClientId = SessionClient.ClientEndpointId;
				}
			}
		}

		if (OtherClientId.IsValid())
		{
			IConcertSyncClientModule& ConcertSyncClientModule = IConcertSyncClientModule::Get();
			ConcertSyncClientModule.JumpToPresence(OtherClientId);
		}
	}
}

#endif

#undef LOCTEXT_NAMESPACE

