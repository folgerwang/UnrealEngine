// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertSyncClientStatics.h"

#if WITH_CONCERT
#include "Modules/ModuleManager.h"
#include "IConcertSyncClientModule.h"
#include "ConcertSettings.h"
#include "ConcertLogGlobal.h"
#include "IConcertModule.h"
#include "IConcertClient.h"
#include "IConcertSession.h"
#include "ConcertMessageData.h"
#endif


#define LOCTEXT_NAMESPACE "ConcertSyncClientStatics"


#if WITH_CONCERT
namespace ConcertSyncClientStatics
{
	FConcertSyncClientInfo Convert(const FConcertClientInfo& ClientInfo)
	{
		FConcertSyncClientInfo Result;
		Result.DisplayName = ClientInfo.DisplayName;
		Result.AvatarColor = ClientInfo.AvatarColor;
		return Result;
	}
}
#endif


UConcertSyncClientStatics::UConcertSyncClientStatics(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UConcertSyncClientStatics::SetPresenceEnabled(const bool IsEnabled)
{
#if WITH_CONCERT // Do not exec in Shipping or Test
	if (IConcertSyncClientModule::IsAvailable())
	{
		IConcertSyncClientModule& ConcertSyncClientModule = IConcertSyncClientModule::Get();
		ConcertSyncClientModule.SetPresenceEnabled(IsEnabled);
	}
#endif
}

void UConcertSyncClientStatics::SetPresenceVisibility(FString Name, bool Visibility, bool PropagateToAll)
{
#if WITH_CONCERT // Do not exec in Shipping or Test
	if (IConcertSyncClientModule::IsAvailable())
	{
		IConcertSyncClientModule& ConcertSyncClientModule = IConcertSyncClientModule::Get();
		ConcertSyncClientModule.SetPresenceVisibility(Name, Visibility, PropagateToAll);
	}
#endif
}

void UConcertSyncClientStatics::UpdateWorkspaceModifiedPackages()
{
	PersistSessionChanges();
}

void UConcertSyncClientStatics::PersistSessionChanges()
{
#if WITH_CONCERT // Do not exec in Shipping or Test
	if (IConcertSyncClientModule::IsAvailable())
	{
		IConcertSyncClientModule::Get().PersistSessionChanges();
	}
#endif
}

FConcertSyncClientInfo UConcertSyncClientStatics::GetLocalConcertClientInfo()
{
	FConcertSyncClientInfo ClientInfo;
#if WITH_CONCERT // Do not exec in Shipping or Test
	ClientInfo = ConcertSyncClientStatics::Convert(IConcertModule::Get().GetClientInstance()->GetClientInfo());
#endif
	return ClientInfo;
}

bool UConcertSyncClientStatics::GetConcertClientInfoByName(const FString ClientName, FConcertSyncClientInfo& ClientInfo)
{
#if WITH_CONCERT // Do not exec in Shipping or Test
	// We return the first match by name that we find. We expect the user to avoid name collisions in the user names. 
	// @todo: We can change this behavior once Concert has unique client IDs that persist across sessions.
	const IConcertClientPtr ConcertClient = IConcertModule::Get().GetClientInstance();
	const FConcertClientInfo& LocalClientInfo = ConcertClient->GetClientInfo();		

	if (ClientName == LocalClientInfo.DisplayName)
	{
		ClientInfo = ConcertSyncClientStatics::Convert(LocalClientInfo);
		return true;
	}

	const TSharedPtr<IConcertClientSession> ClientSession = ConcertClient->GetCurrentSession();
	if (ClientSession.IsValid())
	{
		const TArray<FConcertSessionClientInfo> SessionClients = ClientSession->GetSessionClients();
		for (const FConcertSessionClientInfo& SessionClient : SessionClients)
		{
			if (SessionClient.ClientInfo.DisplayName == ClientName)
			{
				ClientInfo = ConcertSyncClientStatics::Convert(SessionClient.ClientInfo);
				return true;
			}
		}
	}
		
	UE_LOG(LogConcert, Warning, TEXT("UConcertSyncClientStatics::GetConcertClientInfoByName - Failed to get ClientSession"));	
#endif

	return false;
}

bool UConcertSyncClientStatics::GetRemoteConcertClientInfos(TArray<FConcertSyncClientInfo>& ClientInfos)
{
#if WITH_CONCERT // Do not exec in Shipping or Test
	const TSharedPtr<IConcertClientSession> ClientSession = IConcertModule::Get().GetClientInstance()->GetCurrentSession();
	if (ClientSession.IsValid())
	{
		const TArray<FConcertSessionClientInfo> SessionClients = ClientSession->GetSessionClients();
		for (const FConcertSessionClientInfo SessionClient : SessionClients)
		{
			ClientInfos.Add(ConcertSyncClientStatics::Convert(SessionClient.ClientInfo));
		}

		return ClientInfos.Num() > 0;
	}

	UE_LOG(LogConcert, Warning, TEXT("UConcertSyncClientStatics::GetAllConcertClientInfos - Failed to get ClientSession"));
#endif

	return false;
}

bool UConcertSyncClientStatics::GetConcertConnectionStatus()
{
#if WITH_CONCERT // Do not exec in Shipping or Test
	const TSharedPtr<IConcertClientSession> ClientSession = IConcertModule::Get().GetClientInstance()->GetCurrentSession();
	if (ClientSession.IsValid())
	{
		return ClientSession->GetConnectionStatus() == EConcertConnectionStatus::Connected ? true : false;
	}
	
	UE_LOG(LogConcert, Warning, TEXT("UConcertSyncClientStatics::GetConcertConnectionStatus - Failed to get ClientSession"));
#endif

	return false;
}

void UConcertSyncClientStatics::ConcertJumpToPresence(const FString OtherUserName)
{
#if WITH_CONCERT // Do not exec in Shipping or Test
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
#endif
}


#undef LOCTEXT_NAMESPACE

