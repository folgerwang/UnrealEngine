// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemImpl.h"
#include "Containers/Ticker.h"
#include "Misc/App.h"
#include "NamedInterfaces.h"
#include "OnlineError.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Interfaces/OnlinePurchaseInterface.h"

#if UE_BUILD_SHIPPING
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#endif

namespace OSSConsoleVariables
{
	// CVars
	TAutoConsoleVariable<int32> CVarVoiceLoopback(
		TEXT("OSS.VoiceLoopback"),
		0,
		TEXT("Enables voice loopback\n")
		TEXT("1 Enabled. 0 Disabled."),
		ECVF_Default);
}

const FName FOnlineSubsystemImpl::DefaultInstanceName(TEXT("DefaultInstance"));

FOnlineSubsystemImpl::FOnlineSubsystemImpl(FName InSubsystemName, FName InInstanceName) :
	SubsystemName(InSubsystemName),
	InstanceName(InInstanceName),
	bForceDedicated(false),
	NamedInterfaces(nullptr)
{
	StartTicker();
}

FOnlineSubsystemImpl::~FOnlineSubsystemImpl()
{	
	ensure(!TickHandle.IsValid());
}

void FOnlineSubsystemImpl::PreUnload()
{
}

bool FOnlineSubsystemImpl::Shutdown()
{
	OnNamedInterfaceCleanup();
	StopTicker();
	return true;
}

FString FOnlineSubsystemImpl::FilterResponseStr(const FString& ResponseStr, const TArray<FString>& RedactFields)
{
#if UE_BUILD_SHIPPING
	const FString Redacted = TEXT("[REDACTED]");
	if (RedactFields.Num() > 0)
	{
		TSharedPtr< FJsonObject > JsonObject;
		TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(ResponseStr);
		if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
		{
			for (const auto& RedactField : RedactFields)
			{
				// @todo support redacting other types - issues with GetHashKey using TMultiMap<EJson, FString>
				if (JsonObject->HasTypedField<EJson::String>(RedactField))
				{
					JsonObject->SetStringField(RedactField, Redacted);
				}
			}
			FString NewResponseStr;
			TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&NewResponseStr);
			if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
			{
				return NewResponseStr;
			}
		}
	}
	return Redacted;
#else
	return ResponseStr;
#endif
}

void FOnlineSubsystemImpl::ExecuteDelegateNextTick(const FNextTickDelegate& Callback)
{
	NextTickQueue.Enqueue(Callback);
}

void FOnlineSubsystemImpl::StartTicker()
{
	if (!TickHandle.IsValid())
	{
		// Register delegate for ticker callback
		FTickerDelegate TickDelegate = FTickerDelegate::CreateRaw(this, &FOnlineSubsystemImpl::Tick);
		TickHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, 0.0f);
	}
}

void FOnlineSubsystemImpl::StopTicker()
{
	// Unregister ticker delegate
	if (TickHandle.IsValid())
	{
		FTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
}

bool FOnlineSubsystemImpl::Tick(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FOnlineSubsystemImpl_Tick);
	if (!NextTickQueue.IsEmpty())
	{
		// unload the next-tick queue into our buffer. Any further executes (from within callbacks) will happen NEXT frame (as intended)
		FNextTickDelegate Temp;
		while (NextTickQueue.Dequeue(Temp))
		{
			CurrentTickBuffer.Add(Temp);
		}

		// execute any functions in the current tick array
		for (const auto& Callback : CurrentTickBuffer)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FOnlineSubsystemImpl_Tick_ExecuteCallback);
			Callback.ExecuteIfBound();
		}
		CurrentTickBuffer.SetNum(0, false); // keep the memory around
	}
	return true;
}

void FOnlineSubsystemImpl::InitNamedInterfaces()
{
	NamedInterfaces = NewObject<UNamedInterfaces>();
	if (NamedInterfaces)
	{
		NamedInterfaces->Initialize();
		NamedInterfaces->OnCleanup().AddRaw(this, &FOnlineSubsystemImpl::OnNamedInterfaceCleanup);
		NamedInterfaces->AddToRoot();
	}
}

void FOnlineSubsystemImpl::OnNamedInterfaceCleanup()
{
	if (NamedInterfaces)
	{
		UE_LOG_ONLINE(Display, TEXT("Removing %d named interfaces"), NamedInterfaces->GetNumInterfaces());
		NamedInterfaces->RemoveFromRoot();
		NamedInterfaces->OnCleanup().RemoveAll(this);
		NamedInterfaces = nullptr;
	}
}

UObject* FOnlineSubsystemImpl::GetNamedInterface(FName InterfaceName)
{
	if (!NamedInterfaces)
	{
		InitNamedInterfaces();
	}

	if (NamedInterfaces)
	{
		return NamedInterfaces->GetNamedInterface(InterfaceName);
	}

	return nullptr;
}

void FOnlineSubsystemImpl::SetNamedInterface(FName InterfaceName, UObject* NewInterface)
{
	if (!NamedInterfaces)
	{
		InitNamedInterfaces();
	}

	if (NamedInterfaces)
	{
		return NamedInterfaces->SetNamedInterface(InterfaceName, NewInterface);
	}
}

bool FOnlineSubsystemImpl::IsServer() const
{
#if WITH_EDITOR
	FName WorldContextHandle = (InstanceName != NAME_None && InstanceName != DefaultInstanceName) ? InstanceName : NAME_None;
	return IsServerForOnlineSubsystems(WorldContextHandle);
#else
	return IsServerForOnlineSubsystems(NAME_None);
#endif
}

bool FOnlineSubsystemImpl::IsLocalPlayer(const FUniqueNetId& UniqueId) const
{
	if (!IsDedicated())
	{
		IOnlineIdentityPtr IdentityInt = GetIdentityInterface();
		if (IdentityInt.IsValid())
		{
			for (int32 LocalUserNum = 0; LocalUserNum < MAX_LOCAL_PLAYERS; LocalUserNum++)
			{
				TSharedPtr<const FUniqueNetId> LocalUniqueId = IdentityInt->GetUniquePlayerId(LocalUserNum);
				if (LocalUniqueId.IsValid() && UniqueId == *LocalUniqueId)
				{
					return true;
				}
			}
		}
	}

	return false;
}

IMessageSanitizerPtr FOnlineSubsystemImpl::GetMessageSanitizer(int32 LocalUserNum, FString& OutAuthTypeToExclude) const
{
	IMessageSanitizerPtr MessageSanitizer;
	IOnlineSubsystem* SanitizerSubsystem = IOnlineSubsystem::GetByConfig(TEXT("SanitizerPlatformService"));
	if (!SanitizerSubsystem)
	{
		SanitizerSubsystem = IOnlineSubsystem::GetByPlatform();
	}

	if (SanitizerSubsystem && SanitizerSubsystem != static_cast<const IOnlineSubsystem*>(this))
	{
		MessageSanitizer = SanitizerSubsystem->GetMessageSanitizer(LocalUserNum, OutAuthTypeToExclude);
	}
	return MessageSanitizer;
}

bool FOnlineSubsystemImpl::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bWasHandled = false;

	if (FParse::Command(&Cmd, TEXT("FRIEND")))
	{
		bWasHandled = HandleFriendExecCommands(InWorld, Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("SESSION")))
	{
		bWasHandled = HandleSessionExecCommands(InWorld, Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("PRESENCE")))
	{
		bWasHandled = HandlePresenceExecCommands(InWorld, Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("PURCHASE")))
	{
		bWasHandled = HandlePurchaseExecCommands(InWorld, Cmd, Ar);
	}

	return bWasHandled;
}

bool FOnlineSubsystemImpl::IsEnabled() const
{
	return IOnlineSubsystem::IsEnabled(SubsystemName);
}

void FOnlineSubsystemImpl::DumpReceipts(const FUniqueNetId& UserId)
{
	IOnlinePurchasePtr PurchaseInt = GetPurchaseInterface();
	if (PurchaseInt.IsValid())
	{
		TArray<FPurchaseReceipt> Receipts;
		PurchaseInt->GetReceipts(UserId, Receipts);
		if (Receipts.Num() > 0)
		{
			for (const FPurchaseReceipt& Receipt : Receipts)
			{
				UE_LOG_ONLINE(Display, TEXT("Receipt: %s %d"),
							  *Receipt.TransactionId,
							  (int32)Receipt.TransactionState);

				UE_LOG_ONLINE(Display, TEXT("-Offers:"));
				for (const FPurchaseReceipt::FReceiptOfferEntry& ReceiptOffer : Receipt.ReceiptOffers)
				{
					UE_LOG_ONLINE(Display, TEXT(" -Namespace: %s Id: %s Quantity: %d"),
								  *ReceiptOffer.Namespace,
								  *ReceiptOffer.OfferId,
								  ReceiptOffer.Quantity);

					UE_LOG_ONLINE(Display, TEXT(" -LineItems:"));
					for (const FPurchaseReceipt::FLineItemInfo& LineItem : ReceiptOffer.LineItems)
					{
						UE_LOG_ONLINE(Display, TEXT("  -Name: %s Id: %s ValidationInfo: %d bytes"),
									  *LineItem.ItemName,
									  *LineItem.UniqueId,
									  LineItem.ValidationInfo.Len());
					}
				}
			}
		}
		else
		{
			UE_LOG_ONLINE(Display, TEXT("No receipts!"));
		}
	}
}

void FOnlineSubsystemImpl::FinalizeReceipts(const FUniqueNetId& UserId)
{
	IOnlinePurchasePtr PurchaseInt = GetPurchaseInterface();
	if (PurchaseInt.IsValid())
	{
		TArray<FPurchaseReceipt> Receipts;
		PurchaseInt->GetReceipts(UserId, Receipts);
		for (const FPurchaseReceipt& Receipt : Receipts)
		{
			UE_LOG_ONLINE(Display, TEXT("Receipt: %s %d"),
				*Receipt.TransactionId,
				(int32)Receipt.TransactionState);
			for (const FPurchaseReceipt::FReceiptOfferEntry& ReceiptOffer : Receipt.ReceiptOffers)
			{
				UE_LOG_ONLINE(Display, TEXT(" -Namespace: %s Id: %s Quantity: %d"),
					*ReceiptOffer.Namespace,
					*ReceiptOffer.OfferId,
					ReceiptOffer.Quantity);

				UE_LOG_ONLINE(Display, TEXT(" -LineItems:"));
				for (const FPurchaseReceipt::FLineItemInfo& LineItem : ReceiptOffer.LineItems)
				{
					UE_LOG_ONLINE(Display, TEXT("  -Name: %s Id: %s ValidationInfo: %d bytes"),
						*LineItem.ItemName,
						*LineItem.UniqueId,
						LineItem.ValidationInfo.Len());
					if (LineItem.IsRedeemable())
					{
						UE_LOG_ONLINE(Display, TEXT("Finalizing %s!"), *Receipt.TransactionId);
						PurchaseInt->FinalizePurchase(UserId, LineItem.UniqueId);
					}
					else
					{
						UE_LOG_ONLINE(Display, TEXT("Not redeemable"));
					}
				}
			}	
		}
	}
}

void FOnlineSubsystemImpl::OnQueryReceiptsComplete(const FOnlineError& Result, TSharedPtr<const FUniqueNetId> UserId)
{
	UE_LOG_ONLINE(Display, TEXT("OnQueryReceiptsComplete %s"), Result.ToLogString());
	DumpReceipts(*UserId);
}

bool FOnlineSubsystemImpl::HandlePurchaseExecCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bWasHandled = false;
	
	if (FParse::Command(&Cmd, TEXT("RECEIPTS")))
	{
		IOnlinePurchasePtr PurchaseInt = GetPurchaseInterface();
		IOnlineIdentityPtr IdentityInt = GetIdentityInterface();
		if (PurchaseInt.IsValid() && IdentityInt.IsValid())
		{
			FString CommandStr = FParse::Token(Cmd, false);
			if (CommandStr.IsEmpty())
			{
				UE_LOG_ONLINE(Warning, TEXT("usage: PURCHASE RECEIPTS <command> <userid>"));
			}
			else
			{
				FString UserIdStr = FParse::Token(Cmd, false);
				if (UserIdStr.IsEmpty())
				{
					UE_LOG_ONLINE(Warning, TEXT("usage: PURCHASE RECEIPTS <command> <userid>"));
				}
				else
				{
					TSharedPtr<const FUniqueNetId> UserId = IdentityInt->CreateUniquePlayerId(UserIdStr);
					if (UserId.IsValid())
					{
						if (CommandStr == TEXT("RESTORE"))
						{
							FOnQueryReceiptsComplete CompletionDelegate;
							CompletionDelegate.BindRaw(this, &FOnlineSubsystemImpl::OnQueryReceiptsComplete, UserId);
							PurchaseInt->QueryReceipts(*UserId, true, CompletionDelegate);
							
							bWasHandled = true;
						}
						else if (CommandStr == TEXT("DUMP"))
						{
							DumpReceipts(*UserId);
							bWasHandled = true;
						}
						else if (CommandStr == TEXT("FINALIZE"))
						{
							FinalizeReceipts(*UserId);
							bWasHandled = true;
						}
					}
				}
			}
		}
	}

	return bWasHandled;
}

bool FOnlineSubsystemImpl::HandleFriendExecCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bWasHandled = false;

	if (FParse::Command(&Cmd, TEXT("BLOCK")))
	{
		FString LocalNumStr = FParse::Token(Cmd, false);
		int32 LocalNum = FCString::Atoi(*LocalNumStr);

		FString UserId = FParse::Token(Cmd, false);
		if (UserId.IsEmpty() || LocalNum < 0 || LocalNum > MAX_LOCAL_PLAYERS)
		{
			UE_LOG_ONLINE(Warning, TEXT("usage: FRIEND BLOCK <localnum> <userid>"));
		}
		else
		{
			IOnlineIdentityPtr IdentityInt = GetIdentityInterface();
			if (IdentityInt.IsValid())
			{
				TSharedPtr<const FUniqueNetId> BlockUserId = IdentityInt->CreateUniquePlayerId(UserId);
				IOnlineFriendsPtr FriendsInt = GetFriendsInterface();
				if (FriendsInt.IsValid())
				{
					FriendsInt->BlockPlayer(0, *BlockUserId);
				}
			}
		}
	}
	else if (FParse::Command(&Cmd, TEXT("QUERYRECENT")))
	{
		IOnlineIdentityPtr IdentityInt = GetIdentityInterface();
		if (IdentityInt.IsValid())
		{
			int32 LocalUserNum = 0;
			FString LocalUserNumStr = FParse::Token(Cmd, false);
			if (!LocalUserNumStr.IsEmpty())
			{
				LocalUserNum = FCString::Atoi(*LocalUserNumStr);
			}
			FString Namespace = FParse::Token(Cmd, false);

			TSharedPtr<const FUniqueNetId> UserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
			IOnlineFriendsPtr FriendsInt = GetFriendsInterface();
			if (FriendsInt.IsValid())
			{
				FriendsInt->QueryRecentPlayers(*UserId, Namespace);
			}
		}

		bWasHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("DUMPRECENT")))
	{
		IOnlineFriendsPtr FriendsInt = GetFriendsInterface();
		if (FriendsInt.IsValid())
		{
			FriendsInt->DumpRecentPlayers();
		}
		bWasHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("DUMPBLOCKED")))
	{
		IOnlineFriendsPtr FriendsInt = GetFriendsInterface();
		if (FriendsInt.IsValid())
		{
			FriendsInt->DumpBlockedPlayers();
		}
		bWasHandled = true;
	}

	return bWasHandled;
}

bool FOnlineSubsystemImpl::HandlePresenceExecCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bWasHandled = false;
	if (FParse::Command(&Cmd, TEXT("DUMP")))
	{
		IOnlinePresencePtr PresenceInt = GetPresenceInterface();
		if (PresenceInt.IsValid())
		{
			IOnlinePresence::FOnPresenceTaskCompleteDelegate CompletionDelegate = IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateLambda([this](const FUniqueNetId& UserId, const bool bWasSuccessful)
			{
				UE_LOG_ONLINE(Display, TEXT("Presence [%s]"), *UserId.ToDebugString());
				if (bWasSuccessful)
				{
					IOnlinePresencePtr LambdaPresenceInt = GetPresenceInterface();
					if (LambdaPresenceInt.IsValid())
					{
						TSharedPtr<FOnlineUserPresence> UserPresence;
						if (LambdaPresenceInt->GetCachedPresence(UserId, UserPresence) == EOnlineCachedResult::Success &&
							UserPresence.IsValid())
						{
							UE_LOG_ONLINE(Display, TEXT("- %s"), *UserPresence->ToDebugString());
						}
						else
						{
							UE_LOG_ONLINE(Display, TEXT("Failed to get cached presence"));
						}
					}
				}
				else
				{
					UE_LOG_ONLINE(Display, TEXT("Failed to query presence"));
				}
			});

			TArray<TSharedRef<FOnlineFriend>> FriendsList;
			IOnlineFriendsPtr FriendsInt = GetFriendsInterface();
			if (FriendsInt.IsValid())
			{
				FriendsInt->GetFriendsList(0, EFriendsLists::ToString(EFriendsLists::Default), FriendsList);
			}

			// Query and dump friends presence
			for (const TSharedRef<FOnlineFriend>& Friend : FriendsList)
			{
				PresenceInt->QueryPresence(*Friend->GetUserId(), CompletionDelegate);
			}

			// Query own presence
			TSharedPtr<const FUniqueNetId> UserId = GetFirstSignedInUser(GetIdentityInterface());
			if (UserId.IsValid())
			{
				PresenceInt->QueryPresence(*UserId, CompletionDelegate);
			}
		}
		bWasHandled = true;
	}

	return bWasHandled;
}

bool FOnlineSubsystemImpl::HandleSessionExecCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bWasHandled = false;

	if (FParse::Command(&Cmd, TEXT("DUMP")))
	{
		IOnlineSessionPtr SessionsInt = GetSessionInterface();
		if (SessionsInt.IsValid())
		{
			SessionsInt->DumpSessionState();
		}
		bWasHandled = true;
	}

	return bWasHandled;
}
