// Fill out your copyright notice in the Description page of Project Settings.

#include "InAppPurchaseComponent.h"
#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Modules/ModuleManager.h"
#include "AppEventHandler.h"
#if PLATFORM_LUMIN
#include "Lumin/LuminAffinity.h"
#endif // PLATFORM_LUMIN

#if WITH_MLSDK
#include <ml_purchase.h>
#endif //WITH_MLSDK

DEFINE_LOG_CATEGORY(LogInAppPurchase);

class FInAppPurchaseImpl : public MagicLeap::IAppEventHandler
{
public:
	FInAppPurchaseImpl(UInAppPurchaseComponent* InOwner)
	: CurrentRequests(0)
#if WITH_MLSDK
	, ItemsDetailsHandle(ML_INVALID_HANDLE)
	, PurchaseHandle(ML_INVALID_HANDLE)
	, PurchaseHistoryHandle(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
	, Owner(InOwner)
	{
	}

	virtual ~FInAppPurchaseImpl()
	{
#if WITH_MLSDK
		if (MLHandleIsValid(ItemsDetailsHandle))
		{
			MLResult Result = MLPurchaseItemDetailsDestroy(ItemsDetailsHandle);
			if (Result != MLResult_Ok)
			{
				Owner->Log(FString::Printf(TEXT("MLPurchaseItemDetailsDestroy failed with error %d"), Result));
			}
		}

		if (MLHandleIsValid(PurchaseHandle))
		{
			MLResult Result = MLPurchaseDestroy(PurchaseHandle);
			if (Result != MLResult_Ok)
			{
				Owner->Log(FString::Printf(TEXT("MLPurchaseDestroy failed with error %d"), Result));
			}
		}

		if (MLHandleIsValid(PurchaseHistoryHandle))
		{
			MLResult Result = MLPurchaseHistoryQueryDestroy(PurchaseHistoryHandle);
			if (Result != MLResult_Ok)
			{
				Owner->Log(FString::Printf(TEXT("MLPurchaseHistoryQueryDestroy failed with error %d"), Result));
			}
		}
#endif //WITH_MLSDK
	}

	bool TryGetItemsDetails(const TArray<FString>& InItems)
	{
#if WITH_MLSDK
		if ((CurrentRequests & RequestType::ItemsDetails) == 0)
		{
			MLResult Result = MLPurchaseItemDetailsCreate(&ItemsDetailsHandle);
			if (Result != MLResult_Ok)
			{
				Owner->Log(FString::Printf(TEXT("MLItemDetailsCreate failed with error %d"), Result));
			}
			else
			{
				const char** ItemsIds = static_cast<const char**>(FMemory::Malloc(sizeof(char*) * InItems.Num()));
				TArray<FString> ItemNames;
				for (int32 Index = 0; Index < InItems.Num(); ++Index)
				{
					ItemsIds[Index] = static_cast<char*>(FMemory::Malloc(sizeof(char) * InItems[Index].Len() + 1));
					FMemory::Memcpy(const_cast<char*>(ItemsIds[Index]), TCHAR_TO_UTF8(*InItems[Index]), InItems[Index].Len()+1);
				}
				MLPurchaseItemDetailsQuery ItemsDetailsQuery{ ItemsIds, static_cast<uint32_t>(InItems.Num()) };
				Result = MLPurchaseItemDetailsGet(ItemsDetailsHandle, &ItemsDetailsQuery);
				for (int32 Index = 0; Index < InItems.Num(); ++Index)
				{
					FMemory::Free(const_cast<char*>(ItemsIds[Index]));
				}
				FMemory::Free(ItemsIds);
				if (Result != MLResult_Ok)
				{
					Owner->Log(FString::Printf(TEXT("MLPurchaseItemDetailsGet failed with error %d"), Result));
					Result = MLPurchaseItemDetailsDestroy(ItemsDetailsHandle);
					if (Result != MLResult_Ok)
					{
						Owner->Log(FString::Printf(TEXT("MLPurchaseItemDetailsDestroy failed with error %d"), Result));
					}
				}
				else
				{
					CurrentRequests |= RequestType::ItemsDetails;
					return true;
				}
			}
		}
#endif //WITH_MLSDK
		return false;
	}

	bool TryGetPurchaseConfirmation(const FPurchaseItemDetails& ItemDetails)
	{
#if WITH_MLSDK
		if ((CurrentRequests & RequestType::PurchaseConfirmation) == 0)
		{
			MLResult Result = MLPurchaseCreate(&PurchaseHandle);
			if (Result != MLResult_Ok)
			{
				Owner->Log(FString::Printf(TEXT("MLPurchaseCreate failed with error %d"), Result));
			}
			else
			{
				Result = MLPurchaseSubmit(PurchaseHandle, ItemDetails.Token);
				if (Result != MLResult_Ok)
				{
					Owner->Log(FString::Printf(TEXT("MLPurchaseSubmit failed with error %d"), Result));
					Result = MLPurchaseDestroy(PurchaseHandle);
					if (Result != MLResult_Ok)
					{
						Owner->Log(FString::Printf(TEXT("MLPurchaseDestroy failed with error %d"), Result));
					}
				}
				else
				{
					CurrentRequests |= RequestType::PurchaseConfirmation;
					return true;
				}
			}
		}
#endif //WITH_MLSDK
		return false;
	}

	bool TryGetPurchaseHistory(int32 InNumPages)
	{
#if WITH_MLSDK
		if ((CurrentRequests & RequestType::PurchaseHistory) == 0)
		{
			MLResult Result = MLPurchaseHistoryQueryCreate(&PurchaseHistoryHandle);
			if (Result != MLResult_Ok)
			{
				Owner->Log(FString::Printf(TEXT("MLPurchaseHistoryQueryCreate failed with error %d"), Result));
			}
			else
			{
				Result = MLPurchaseHistoryQueryGetPage(PurchaseHistoryHandle, InNumPages);
				if (Result != MLResult_Ok)
				{
					Owner->Log(FString::Printf(TEXT("MLPurchaseHistoryQueryGetPage failed with error %d"), Result));
					Result = MLPurchaseHistoryQueryDestroy(PurchaseHistoryHandle);
					if (Result != MLResult_Ok)
					{
						Owner->Log(FString::Printf(TEXT("MLPurchaseHistoryQueryDestroy failed with error %d"), Result));
					}
				}
				else
				{
					CurrentRequests |= RequestType::PurchaseHistory;
					return true;
				}
			}
		}
#endif //WITH_MLSDK
		return false;
	}

	void Tick()
	{
#if WITH_MLSDK
		MLCloudStatus CloudStatus;

		if (CurrentRequests & ItemsDetails)
		{
			MLPurchaseItemDetailsResults MLItemsDetails;
			MLResult Result = MLPurchaseItemDetailsGetResult(ItemsDetailsHandle, &MLItemsDetails, &CloudStatus);
			if (Result != MLResult_Ok)
			{
				CurrentRequests &= (~ItemsDetails);
				Owner->Log(FString::Printf(TEXT("MLPurchaseItemDetailsGetResult failed with error %d"), Result));
				Owner->GetItemsDetailsFailure.Broadcast();
			}
			else if (CloudStatus == MLCloudStatus_Done)
			{
				CurrentRequests &= (~ItemsDetails);
				TArray<FPurchaseItemDetails> UEItemsDetails;
				MLToUE(MLItemsDetails, UEItemsDetails);
				Owner->GetItemsDetailsSuccess.Broadcast(UEItemsDetails);
			}

			if (!(CurrentRequests & ItemsDetails))
			{
				Result = MLPurchaseItemDetailsDestroy(ItemsDetailsHandle);
				if (Result != MLResult_Ok)
				{
					Owner->Log(FString::Printf(TEXT("MLPurchaseItemDetailsDestroy failed with error %d"), Result));
				}
			}
		}

		if (CurrentRequests & PurchaseConfirmation)
		{
			MLPurchaseConfirmation MLConfirmation;
			MLResult Result = MLPurchaseGetResult(PurchaseHandle, &MLConfirmation, &CloudStatus);
			if (Result != MLResult_Ok)
			{
				CurrentRequests &= (~PurchaseConfirmation);
				Owner->Log(FString::Printf(TEXT("MLPurchaseGetResult failed with error %d"), Result));
				Owner->PurchaseConfirmationFailure.Broadcast();
			}
			else if (CloudStatus == MLCloudStatus_Done)
			{
				CurrentRequests &= (~PurchaseConfirmation);
				FPurchaseConfirmation UEConfirmation;
				MLToUE(MLConfirmation, UEConfirmation);
				Owner->PurchaseConfirmationSuccess.Broadcast(UEConfirmation);
			}

			if (!(CurrentRequests & PurchaseConfirmation))
			{
				Result = MLPurchaseDestroy(PurchaseHandle);
				if (Result != MLResult_Ok)
				{
					Owner->Log(FString::Printf(TEXT("MLPurchaseDestroy failed with error %d"), Result));
				}
			}
		}

		if (CurrentRequests & PurchaseHistory)
		{
			MLPurchaseHistoryResult MLPurchaseHistory;
			MLResult Result = MLPurchaseHistoryQueryGetPageResult(PurchaseHistoryHandle, &MLPurchaseHistory);
			if (Result != MLResult_Ok)
			{
				CurrentRequests &= (~PurchaseHistory);
				Owner->Log(FString::Printf(TEXT("MLPurchaseHistoryQueryGetPageResult failed with error %d"), Result));
				Owner->GetPurchaseHistoryFailure.Broadcast();
			}
			else if (MLPurchaseHistory.status == MLCloudStatus_Done)
			{
				MLToUE(MLPurchaseHistory);
				if (!MLPurchaseHistory.has_next_page)
				{
					CurrentRequests &= (~PurchaseHistory);
					Owner->GetPurchaseHistorySuccess.Broadcast(CachedPurchaseHistory);
					CachedPurchaseHistory.Empty();
				}
			}

			if (!(CurrentRequests & PurchaseHistory))
			{
				Result = MLPurchaseHistoryQueryDestroy(PurchaseHistoryHandle);
				if (Result != MLResult_Ok)
				{
					Owner->Log(FString::Printf(TEXT("MLPurchaseHistoryQueryDestroy failed with error %d"), Result));
				}
			}
		}
#endif //WITH_MLSDK
	}

	enum RequestType
	{
		None = 0,
		ItemsDetails = 1,
		PurchaseConfirmation = 2,
		PurchaseHistory = 4,
	};

	uint32 CurrentRequests;
#if WITH_MLSDK
	MLHandle ItemsDetailsHandle;
	MLHandle PurchaseHandle;
	MLHandle PurchaseHistoryHandle;
#endif //WITH_MLSDK
	TArray<FPurchaseConfirmation> CachedPurchaseHistory;
	UInAppPurchaseComponent* Owner;

private:
#if WITH_MLSDK
	PurchaseType MLToUE(MLPurchaseType InMLPurchaseType)
	{
		switch (InMLPurchaseType)
		{
		case MLPurchaseType_Consumable: return PurchaseType::Consumable;
		case MLPurchaseType_Nonconsumable: return PurchaseType::Nonconsumable;
		}

		return PurchaseType::Undefined;
	}

	void MLToUE(const MLPurchaseItemDetailsResults& MLItemsDetails, TArray<FPurchaseItemDetails>& UEItemsDetails)
	{
		UEItemsDetails.AddDefaulted(MLItemsDetails.count);
		for (uint32_t Index = 0; Index < MLItemsDetails.count; ++Index)
		{
			const MLPurchaseItemDetailsResult& MLItemDetails = MLItemsDetails.item_details[Index];
			FPurchaseItemDetails& UEItemDetails = UEItemsDetails[Index];
			UEItemDetails.IAPID = MLItemDetails.iap_id;
			UEItemDetails.Price = MLItemDetails.price;
			UEItemDetails.Name = MLItemDetails.name;
			UEItemDetails.Type = MLToUE(MLItemDetails.type);
			UEItemDetails.Token = MLItemDetails.token;
		}
	}

	void MLToUE(const MLPurchaseConfirmation& MLConfirmation, FPurchaseConfirmation& UEConfirmation)
	{
		UEConfirmation.OrderID = MLConfirmation.order_id;
		UEConfirmation.PackageName = MLConfirmation.package_name;
		UEConfirmation.PurchaseTime = MLConfirmation.purchase_time;
		UEConfirmation.Signature = MLConfirmation.signature;
		UEConfirmation.IAPID = MLConfirmation.iap_id;
		UEConfirmation.Type = MLToUE(MLConfirmation.type);
	}

	void MLToUE(const MLPurchaseHistoryResult& MLPurchaseHistory)
	{
		int32 StartWriteIndex = CachedPurchaseHistory.Num();
		CachedPurchaseHistory.AddDefaulted(MLPurchaseHistory.count);
		for (size_t Index = 0; Index < MLPurchaseHistory.count; ++Index)
		{
			FPurchaseConfirmation& Confirmation = CachedPurchaseHistory[StartWriteIndex + Index];
			MLToUE(MLPurchaseHistory.confirmations[Index], Confirmation);
		}
	}
#endif //WITH_MLSDK
};

UInAppPurchaseComponent::UInAppPurchaseComponent()
: Impl(nullptr)
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = true;
}

UInAppPurchaseComponent::~UInAppPurchaseComponent()
{
	delete Impl;
	Impl = nullptr;
}

void UInAppPurchaseComponent::BeginPlay()
{
	Super::BeginPlay();
	Impl = new FInAppPurchaseImpl(this);
}

void UInAppPurchaseComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Impl->Tick();
}

UInAppPurchaseComponent::FInAppPurchaseLogMessage& UInAppPurchaseComponent::OnInAppPurchaseLogMessage()
{
  return InAppPurchaseLogMessage;
}

UInAppPurchaseComponent::FGetItemsDetailsSuccess& UInAppPurchaseComponent::OnGetItemsDetailsSuccess()
{
  return GetItemsDetailsSuccess;
}

UInAppPurchaseComponent::FGetItemsDetailsFailure& UInAppPurchaseComponent::OnGetItemsDetailsFailure()
{
  return GetItemsDetailsFailure;
}

UInAppPurchaseComponent::FPurchaseConfirmationSuccess& UInAppPurchaseComponent::OnPurchaseConfirmationSuccess()
{
  return PurchaseConfirmationSuccess;
}

UInAppPurchaseComponent::FPurchaseConfirmationFailure& UInAppPurchaseComponent::OnPurchaseConfirmationFailure()
{
  return PurchaseConfirmationFailure;
}

UInAppPurchaseComponent::FGetPurchaseHistorySuccess& UInAppPurchaseComponent::OnGetPurchaseHistorySuccess()
{
  return GetPurchaseHistorySuccess;
}

UInAppPurchaseComponent::FGetPurchaseHistoryFailure& UInAppPurchaseComponent::OnGetPurchaseHistoryFailure()
{
  return GetPurchaseHistoryFailure;
}

bool UInAppPurchaseComponent::TryGetItemsDetailsAsync(const TArray<FString>& ItemIDs)
{
  if (Impl->TryGetItemsDetails(ItemIDs))
  {
    return true;
  }

  Log("Items details query already in progress!", false);
  return false;
}

bool UInAppPurchaseComponent::TryPurchaseItemAsync(const FPurchaseItemDetails& ItemDetails)
{
  if (Impl->TryGetPurchaseConfirmation(ItemDetails))
  {
    return true;
  }

  Log("Item purchase already in progress!", false);
  return false;
}

bool UInAppPurchaseComponent::TryGetPurchaseHistoryAsync(int32 InNumPages)
{
	if (InNumPages <= 0)
	{
		Log(FString::Printf(TEXT("TryGetPurchaseHistoryAsync failed due to invalid number of pages (%d)"), InNumPages));
		return false;
	}

	if (Impl->TryGetPurchaseHistory(InNumPages))
	{
		return true;
	}

	Log("Purchase history query already in progress!", false);
	return false;
}

void UInAppPurchaseComponent::Log(const FString& LogMessage, bool bError/* = true */)
{
	if (bError)
	{
		UE_LOG(LogInAppPurchase, Error, TEXT("%s"), *LogMessage);
	}
	else
	{
		UE_LOG(LogInAppPurchase, Display, TEXT("%s"), *LogMessage);
	}

	InAppPurchaseLogMessage.Broadcast(LogMessage);
}
