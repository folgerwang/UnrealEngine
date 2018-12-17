// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MagicLeapIdentityTypes.generated.h"

/** Identifies an attribute in a user profile. */
UENUM(BlueprintType, meta=(ScriptName="MagicLeapIdentityAttributeType"))
enum class EMagicLeapIdentityKey : uint8
{
	GivenName,
	FamilyName,
	Email,
	Bio,
	PhoneNumber,
	Avatar2D,
	Avatar3D,
	Unknown,
};

/** List of possible errors when calling the Identity functions. */
UENUM(BlueprintType)
enum class EMagicLeapIdentityError : uint8
{
	Ok,
	InvalidParam,
	AllocFailed,
	PrivilegeDenied,
	FailedToConnectToLocalService,
	FailedToConnectToCloudService,
	CloudAuthentication,
	InvalidInformationFromCloud,
	NotLoggedIn,
	ExpiredCredentials,
	FailedToGetUserProfile,
	Unauthorized,
	CertificateError,
	RejectedByCloud,
	AlreadyLoggedIn,
	ModifyIsNotSupported,
	NetworkError,
	UnspecifiedFailure,
};

/** Represents an attribute and its value of a user's profile. */
USTRUCT(BlueprintType)
struct FMagicLeapIdentityAttribute
{
	GENERATED_BODY()

public:
	FMagicLeapIdentityAttribute()
		: Attribute(EMagicLeapIdentityKey::GivenName)
	{}

	FMagicLeapIdentityAttribute(EMagicLeapIdentityKey attribute, const FString& value)
		: Attribute(attribute)
		, Value(value)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity|MagicLeap")
	EMagicLeapIdentityKey Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity|MagicLeap")
	FString Value;
};
