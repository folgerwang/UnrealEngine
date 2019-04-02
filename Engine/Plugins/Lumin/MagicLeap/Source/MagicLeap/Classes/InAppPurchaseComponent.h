// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "InAppPurchaseComponent.generated.h"

UENUM(BlueprintType)
enum class PurchaseType : uint8
{
	/*!
	\brief This represents an item that can be bought multiple times. The
	application is responsible for managing the consumption of this item.
	*/
	Consumable,
	/*!
	\brief This represents an item that can only be bought once. This will
	be enforced by the services.
	*/
	Nonconsumable,

	Undefined,
};

UENUM(BlueprintType)
enum class CloudStatus : uint8
{
	/*! The current request is still in-progress. */
	CloudStatus_NotDone = 0,
	/*! The current request is complete. It may have succeeded or failed. */
	CloudStatus_Done,
};

USTRUCT(BlueprintType)
struct FPurchaseItemDetails
{
	GENERATED_BODY()

	/*! This is the id of the item. */
	FString IAPID;
	/*! This is the formatted price for the item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PurchaseItemDetails|MagicLeap")
	FString Price;
	/*! This is the name of the item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PurchaseItemDetails|MagicLeap")
	FString Name;
	/*! This is the type of purchase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PurchaseItemDetails|MagicLeap")
	PurchaseType Type;
	/*! This is the token to be used when submitting a purchase. */
	const char *Token;
};

USTRUCT(BlueprintType)
struct FPurchaseConfirmation
{
	GENERATED_BODY()
	/*! This is the unique order id for this purchase. */
	FString OrderID;
	/*! This is the name of the item from where this purchase originated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PurchaseConfirmation|MagicLeap")
	FString PackageName;
	/*! This is the time the product was purchased, in milliseconds since the epoch (Jan 1, 1970). */
	uint64 PurchaseTime;
	/*!
	\brief This is a string containing the signature of the purchase data that
	was signed with the private key of the developer.
	*/
	FString Signature;
	/*! This is the in app purchase ID of the item being purchased. */
	FString IAPID;
	/*! This is the type of purchase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PurchaseConfirmation|MagicLeap")
	PurchaseType Type;
};

/**
	The CameraCaptureComponent provides access to and maintains state for camera capture functionality.
	The connection to the device's camera is managed internally.  Users of this component
	are able to asynchronously capture camera images and footage to file.  Alternatively,
	a camera image can be captured directly to texture.  The user need only make the relevant
	asynchronous call and then register the appropriate success/fail event handlers for the
	operation's completion.
*/
UCLASS(ClassGroup = MagicLeap, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class MAGICLEAP_API UInAppPurchaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInAppPurchaseComponent();
	virtual ~UInAppPurchaseComponent();

	/** Intializes internal systems. */
	void BeginPlay() override;

	/** Polls for query results. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/**
		Attempts to retrieve details for the specified items.
		@brief	This call instigates an items details query which is handled on a separate thread.  The result of this asynchronous
				operation is reported back to the calling blueprint via the FGetItemsDetailsSuccess or FGetItemsDetailsFailure event handlers.
		@param	ItemIDs An array of FString objects specifying the names of the items whose details will be queried.
		@return	False if an items details query is already running, true otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "InAppPurchase|MagicLeap")
	bool TryGetItemsDetailsAsync(const TArray<FString>& ItemIDs);

	/**
		Attempts to purchase the specified item.
		@brief	This call instigates a purchase request which is handled on a separate thread.  The result of this asynchronous operation is
				reported back to the calling blueprint via the FPurchaseConfirmationSuccess or FPurchaseConfirmationFailure event handlers.
		@param	ItemDetails The details of the item to be purchased.
		@return	False if a purchase confirmation is already running, true otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "InAppPurchase|MagicLeap")
	bool TryPurchaseItemAsync(const FPurchaseItemDetails& ItemDetails);

	/**
		Attempts to retrieve the app's purchase history.
		@brief	This call instigates a purchase history request which is handled on a separate thread.  The result of this asynchronous
				operation is reported back to the calling blueprint via the FGetPurchaseHistorySuccess or FGetPurchaseHistoryFailure event handlers.
		@param	InNumPages Specifies the number of history pages to retrieve.
		@return	False if a purchase history query is already running or InNumPages is less than or equal to zero, true otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "InAppPurchase|MagicLeap")
	bool TryGetPurchaseHistoryAsync(int32 InNumPages);

public:
	/**
		Delegate used to report log messages.
		@note This is useful if the user wishes to have log messages in 3D space.
		@param LogMessage A string containing the log message.
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInAppPurchaseLogMessage, const FString&, LogMessage);

	/**
		Delegate used to report a successful retrieval of items details.
		@param ItemsDetails A list of items details.
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGetItemsDetailsSuccess, const TArray<FPurchaseItemDetails>&, ItemsDetails);

	/**
		Delegate used to report a failure to retrieve the requested items details.
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGetItemsDetailsFailure);

	/**
		Delegate used to report a successful item purchase confirmation.
		@param PurchaseConfirmations A list of item purchase confirmations.
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPurchaseConfirmationSuccess, const FPurchaseConfirmation&, PurchaseConfirmations);

	/**
		Delegate used to report a failure to retrieve an item purchase confirmation.
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPurchaseConfirmationFailure);

	/**
		Delegate used to report a successful item purchase history retrieval.
		@param PurchasesHistory The current purchases history of the app.
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGetPurchaseHistorySuccess, const TArray<FPurchaseConfirmation>&, PurchaseHistory);

	/**
		Delegate used to pass a purchase history request failure back to the instigating blueprint.
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGetPurchaseHistoryFailure);

	/** Activated when a log message is generated on the capture thread. */
	FInAppPurchaseLogMessage& OnInAppPurchaseLogMessage();

	/** Activated when retrieval of items details succeeds. */
	FGetItemsDetailsSuccess& OnGetItemsDetailsSuccess();

	/** Activated when retrieval of items details fails. */
	FGetItemsDetailsFailure& OnGetItemsDetailsFailure();

	/** Activated when a purchase confirmationn request succeeds. */
	FPurchaseConfirmationSuccess& OnPurchaseConfirmationSuccess();

	/** Activated when a purchase confirmation request fails. */
	FPurchaseConfirmationFailure& OnPurchaseConfirmationFailure();

	/** Activated when a purchase history request succeeds. */
	FGetPurchaseHistorySuccess& OnGetPurchaseHistorySuccess();

	/** Activated when a purchase history request fails. */
	FGetPurchaseHistoryFailure& OnGetPurchaseHistoryFailure();

	void Log(const FString& LogMessage, bool bError = true);

	UPROPERTY(BlueprintAssignable, Category = "InAppPurchase|MagicLeap", meta = (AllowPrivateAccess = true))
	FInAppPurchaseLogMessage InAppPurchaseLogMessage;

	UPROPERTY(BlueprintAssignable, Category = "InAppPurchase|MagicLeap", meta = (AllowPrivateAccess = true))
	FGetItemsDetailsSuccess GetItemsDetailsSuccess;

	UPROPERTY(BlueprintAssignable, Category = "InAppPurchase|MagicLeap", meta = (AllowPrivateAccess = true))
	FGetItemsDetailsFailure GetItemsDetailsFailure;

	UPROPERTY(BlueprintAssignable, Category = "InAppPurchase|MagicLeap", meta = (AllowPrivateAccess = true))
	FPurchaseConfirmationSuccess PurchaseConfirmationSuccess;

	UPROPERTY(BlueprintAssignable, Category = "InAppPurchase|MagicLeap", meta = (AllowPrivateAccess = true))
	FPurchaseConfirmationFailure PurchaseConfirmationFailure;

	UPROPERTY(BlueprintAssignable, Category = "InAppPurchase|MagicLeap", meta = (AllowPrivateAccess = true))
	FGetPurchaseHistorySuccess GetPurchaseHistorySuccess;

	UPROPERTY(BlueprintAssignable, Category = "InAppPurchase|MagicLeap", meta = (AllowPrivateAccess = true))
	FGetPurchaseHistoryFailure GetPurchaseHistoryFailure;

private:
	class FInAppPurchaseImpl* Impl;
};

DECLARE_LOG_CATEGORY_EXTERN(LogInAppPurchase, Verbose, All);