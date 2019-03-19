// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"

struct RSA_API FRSA
{
	struct FKey;
	static const ESPMode KeyThreadMode = ESPMode::ThreadSafe;
	typedef TSharedPtr<FKey, KeyThreadMode> TKeyPtr;

	/**
	 * Enumerate which key to use when performing encrypt/decrpt operations
	 */
	enum class EKeyType
	{
		Public,
		Private,
	};

	/**
	 * Create a new RSA public/private key from the supplied exponents and modulus binary data. Each of these arrays should contain a single little endian
	 * large integer value
	 */
	static TKeyPtr CreateKey(const TArray<uint8>& InPublicExponent, const TArray<uint8>& InPrivateExponent, const TArray<uint8>& InModulus);

	/**
	 * Returns the size in bits of the supplied key
	 */
	static int32 GetKeySizeInBits(TKeyPtr InKey);

	/**
	 * Returns the maximum number of bytes that can be encrypted in a single payload
	 */
	static int32 GetMaxDataSizeInBytes(TKeyPtr InKey);

	/**
	 * Encrypt the supplied byte data using the given key
	 */
	static bool Encrypt(EKeyType InKeyType, const uint8* InSource, int32 InSourceSizeInBytes, TArray<uint8>& OutDestination, TKeyPtr InKey);
	static bool Encrypt(EKeyType InKeyType, const TArray<uint8>& InSource, TArray<uint8>& OutDestination, TKeyPtr InKey);

	/**
	 * Decrypt the supplied byte data using the given key
	 */
	static bool Decrypt(EKeyType InKeyType, const TArray<uint8>& InSource, uint8* OutDestination, int32 OutDestinationSizeInBytes, TKeyPtr InKey);
	static bool Decrypt(EKeyType InKeyType, const TArray<uint8>& InSource, TArray<uint8>& OutDestination, TKeyPtr InKey);
};