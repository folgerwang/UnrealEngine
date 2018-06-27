// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Tickable.h"
#include "MagicLeapIdentityTypes.h"
#include "MagicLeapIdentity.generated.h"

/**
 *  Class which provides functions to read and update the user's Magic Leap profile.
 */
UCLASS(ClassGroup = MagicLeap, BlueprintType)
class MAGICLEAPIDENTITY_API UMagicLeapIdentity : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UMagicLeapIdentity();
	virtual ~UMagicLeapIdentity();

	/**
	  Delegate for the result of available attributes for the user's Magic Leap profile.
	  @param ResultCode Error code when getting the available attributes.
	  @param AvailableAttributes List of attributes available for the user's Magic Leap profile.
	*/
	DECLARE_DYNAMIC_DELEGATE_TwoParams(FAvailableIdentityAttributesDelegate, EMagicLeapIdentityError, ResultCode, const TArray<EMagicLeapIdentityKey>&, AvailableAttributes);

	/**
	  Delegate for the result of attribute values for the user's Magic Leap profile.
	  @param ResultCode Error code when getting the attribute values.
	  @param AttributeValue List of attribute values for the user's Magic Leap profile.
	*/
	DECLARE_DYNAMIC_DELEGATE_TwoParams(FRequestIdentityAttributeValueDelegate, EMagicLeapIdentityError, ResultCode, const TArray<FMagicLeapIdentityAttribute>&, AttributeValue);

	/**
	  Delegate for the result of modifying the attribute values of a user's Magic Leap profile.
	  @param ResultCode Error code when modifying the attribute values.
	  @param AttributesUpdatedSuccessfully List of attributes whose values were successfully modified.
	*/
	DECLARE_DYNAMIC_DELEGATE_TwoParams(FModifyIdentityAttributeValueDelegate, EMagicLeapIdentityError, ResultCode, const TArray<EMagicLeapIdentityKey>&, AttributesUpdatedSuccessfully);

	/**
	  Get the attributes available for the user's Magic Leap profile. Note that this does not request the values for these attribtues.
	  This function makes a blocking call to the cloud. You can alternatively use GetAllAvailableAttributesAsync() to request the attributes asynchronously.
	  @param AvailableAttributes Output parameter populated with the list of attributes available for the user's Magic Leap profile.
	  @return Error code when getting the list of available attributes.
	*/
	UFUNCTION(BlueprintCallable, Category = "Identity|MagicLeap")
	EMagicLeapIdentityError GetAllAvailableAttributes(TArray<EMagicLeapIdentityKey>& AvailableAttributes);

	/**
	  Asynchronous call to get the attributes available for the user's Magic Leap profile. Note that this does not request the values for these attribtues.
	  @param ResultDelegate Callback which reports the list of available attributes.
	  @return Error code when getting the list of available attributes.
	*/
	UFUNCTION(BlueprintCallable, Category = "Identity|MagicLeap")
	void GetAllAvailableAttributesAsync(const FAvailableIdentityAttributesDelegate& ResultDelegate);

	/**
	  Get the values for the attributes of the user's Magic Leap profile.
	  This function makes a blocking call to the cloud. You can alternatively use RequestAttributeValueAsync() to request the attribute values asynchronously.
	  @param RequestedAttributeList List of attributes to request the value for.
	  @param RequestedAttributeValues Output parameter populated with the list of attributes and their values.
	  @return Error code when getting the attribute values.
	*/
	UFUNCTION(BlueprintCallable, Category = "Identity|MagicLeap")
	EMagicLeapIdentityError RequestAttributeValue(const TArray<EMagicLeapIdentityKey>& RequestedAttributeList, TArray<FMagicLeapIdentityAttribute>& RequestedAttributeValues);

	/**
	  Asynchronous call to get the values for the attributes of the user's Magic Leap profile.
	  @param RequestedAttributeList List of attributes to request the value for.
	  @param ResultDelegate Callback which reports the list of attributes and their values.
	  @return Error code when getting the attribute values.
	*/
	UFUNCTION(BlueprintCallable, Category = "Identity|MagicLeap")
	EMagicLeapIdentityError RequestAttributeValueAsync(const TArray<EMagicLeapIdentityKey>& RequestedAttributeList, const FRequestIdentityAttributeValueDelegate& ResultDelegate);

	/** UObjectBase interface */
	virtual UWorld* GetWorld() const override;

	/** FTickableObjectBase interface */
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	/** FTickableGameObject interface */
	virtual UWorld* GetTickableGameObjectWorld() const override;

private:
	class FIdentityImpl *Impl;
};
