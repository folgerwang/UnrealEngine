// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "IPAddress.h"

#include "IPAddressRef.generated.h"

/**
 * Represents an FInternetAddr used as the key for a TMap (IMPORTANT: Must not be used outside of TMap keys - will crash!)
 *
 * NOTE: Implemented as a USTRUCT, so can be paired with UObject's in a UPROPERTY TMap (hacks are worth the safety benefit of UPROPERTY)
 * NOTE: Implements TSharedRef over TSharedPtr (along with default constructor hacks), as this is required for performance critical code
 */
USTRUCT()
struct FInternetAddrMapRef
{
	GENERATED_USTRUCT_BODY()

	/** The FInternetAddr value stored as the map key */
	TSharedRef<FInternetAddr> Element;


	/**
	 * Primary constructor - all usage of this type, must specify a valid FInternetAddr shared pointer
	 *
	 * @param InAddr	The shared address pointer to store
	 */
	FORCEINLINE FInternetAddrMapRef(const TSharedPtr<FInternetAddr>& InAddr)
		: Element(InAddr.ToSharedRef())
	{
	}

	/**
	 * Equality operator for TMap key/hash support
	 *
	 * @param Other		The value to compare against
	 * @return			Whether or nor the values match
	 */
	FORCEINLINE bool operator == (const FInternetAddrMapRef& Other) const
	{
		return *Element == *Other.Element;
	}


	// Hacks to obscure the default constructor - required, as this struct must NEVER be default-constructed - only use as a TMap key
	FInternetAddrMapRef(EForceInit)
		: Element((FInternetAddr*)nullptr)
	{
		check(false);
	}

	FInternetAddrMapRef() = delete;
};

/**
 * FInternetAddrMapRef default constructor hacks
 */
template<>
struct TStructOpsTypeTraits<FInternetAddrMapRef> : public TStructOpsTypeTraitsBase2<FInternetAddrMapRef>
{
	enum
	{
		WithZeroConstructor		= true,	// Triggers an early assert if there's an attempt to default-construct
		WithNoInitConstructor	= true	// Makes it harder to accidentally call the default constructor, by requiring an EForceInit type
	};
};

/**
 * TMap key hash function
 *
 * @param InAddrRef		The address to hash
 * @return				The hash value
 */
FORCEINLINE uint32 GetTypeHash(const FInternetAddrMapRef& InAddrRef)
{
	return InAddrRef.Element->GetTypeHash();
}

/**
 * TMap key hash function
 *
 * @param InAddrRef		The address to hash
 * @return				The hash value
 */
FORCEINLINE uint32 GetTypeHash(FInternetAddrMapRef& InAddrRef)
{
	return InAddrRef.Element->GetTypeHash();
}

