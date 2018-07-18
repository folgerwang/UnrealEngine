// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MagicLeapIdentityTypes.generated.h"

/** Identifies an attribute in a user profile. */
UENUM(BlueprintType, meta=(ScriptName="MagicLeapIdentityAttributeType"))
enum class EMagicLeapIdentityAttribute : uint8
{
	UserID,
	GivenName,
	FamilyName,
	Email,
	Status,
	TermsAccepted,
	Birthday,
	Company,
	Industry,
	Location,
	Tagline,
	PhoneNumber,
	Avatar2D,
	Avatar3D,
	IsDeveloper,
	Unknown
};

/** List of possible errors when calling the Identity functions. */
UENUM(BlueprintType)
enum class EMagicLeapIdentityError : uint8
{
	Ok,
	FailedToConnectToLocalService,
	FailedToConnectToCloudService,
	CloudAuthentication,
	InvalidInformationFromCloud,
	InvalidArgument,
	AsyncOperationNotComplete,
	OtherError
};

/** Represents an attribute and its value of a user's profile. */
USTRUCT(BlueprintType)
struct FMagicLeapIdentityAttribute
{
	GENERATED_BODY()

public:
	FMagicLeapIdentityAttribute()
	{}

	FMagicLeapIdentityAttribute(EMagicLeapIdentityAttribute attribute, const FString& value)
		: Attribute(attribute)
		, Value(value)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity|MagicLeap")
	EMagicLeapIdentityAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity|MagicLeap")
	FString Value;
};
