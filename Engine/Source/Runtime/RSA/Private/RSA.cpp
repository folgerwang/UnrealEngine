// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RSA.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Math/BigInt.h"

#ifndef RSA_USE_OPENSSL
#define RSA_USE_OPENSSL 0
#endif

#if RSA_USE_OPENSSL
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/err.h>

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

// Some platforms were upgraded to OpenSSL 1.1.1 while the others were left on a previous version. There are some minor differences we have to account for
// in the older version, so declare a handy define that we can use to gate the code
#if !defined(OPENSSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER < 0x10100000L
#define USE_LEGACY_OPENSSL 1
#else
#define USE_LEGACY_OPENSSL 0
#endif

struct FRSA::FKey
{
	FKey() 
	: KeySize(0)
	, KeySizeInBytes(0)
	, MaxDataSize(0)
	, RSAKey(RSA_new())
	{
	}

	~FKey()
	{
		RSA_free(RSAKey);
	}

	int32 GetKeySizeInBits() const
	{
		return KeySize;
	}

	int32 GetKeySizeInBytes() const
	{
		return KeySizeInBytes;
	}

	int32 GetMaxDataSize() const
	{
		return MaxDataSize;
	}

	int32 KeySize;
	int32 KeySizeInBytes;
	int32 MaxDataSize;
	RSA* RSAKey;
};

/**
 * Wrapper function for extracting the bytes of an OpenSSL big num in a consistent form
 */
void LoadBinaryIntoBigNum(const uint8* InData, int64 InDataSize, BIGNUM* InBigNum)
{
#if USE_LEGACY_OPENSSL
	TArray<uint8> Bytes(InData, InDataSize);
	Algo::Reverse(Bytes);
	BN_bin2bn(Bytes.GetData(), Bytes.Num(), InBigNum);
#else
	BN_lebin2bn(InData, InDataSize, InBigNum);
#endif
}

FRSA::TKeyPtr FRSA::CreateKey(const TArray<uint8>& InPublicExponent, const TArray<uint8>& InPrivateExponent, const TArray<uint8>& InModulus)
{
	FRSA::TKeyPtr Key = MakeShared<FRSA::FKey, FRSA::KeyThreadMode>();
	BIGNUM* PublicExponent = InPublicExponent.Num() > 0 ? BN_new() : nullptr;
	BIGNUM* PrivateExponent = InPrivateExponent.Num() > 0 ? BN_new() : nullptr;
	BIGNUM* Modulus = BN_new();
	
	if (InPublicExponent.Num())
	{
		LoadBinaryIntoBigNum(InPublicExponent.GetData(), InPublicExponent.Num(), PublicExponent);
	}
	
	if (InPrivateExponent.Num())
	{
		LoadBinaryIntoBigNum(InPrivateExponent.GetData(), InPrivateExponent.Num(), PrivateExponent);
	}

	LoadBinaryIntoBigNum(InModulus.GetData(), InModulus.Num(), Modulus);
#if USE_LEGACY_OPENSSL
	Key->RSAKey->n = Modulus;
	Key->RSAKey->e = PublicExponent;
	Key->RSAKey->d = PrivateExponent;
#else
	RSA_set0_key(Key->RSAKey, Modulus, PublicExponent, PrivateExponent);
#endif

	Key->KeySizeInBytes = RSA_size(Key->RSAKey);
	Key->KeySize = Key->KeySizeInBytes * 8;
	Key->MaxDataSize = Key->KeySizeInBytes - RSA_PKCS1_PADDING_SIZE;
	return Key;
}

bool FRSA::Encrypt(EKeyType InKeyType, const uint8* InSource, int32 InSourceSizeInBytes, TArray<uint8>& OutDestination, FRSA::TKeyPtr InKey)
{
	OutDestination.SetNum(InKey->KeySizeInBytes);

	int NumEncryptedBytes = 0;
	
	switch (InKeyType)
	{
	case EKeyType::Public:
	{
		NumEncryptedBytes = RSA_public_encrypt(InSourceSizeInBytes, InSource, OutDestination.GetData(), InKey->RSAKey, RSA_PKCS1_PADDING);
		break;
	}

	case EKeyType::Private:
	{
		NumEncryptedBytes = RSA_private_encrypt(InSourceSizeInBytes, InSource, OutDestination.GetData(), InKey->RSAKey, RSA_PKCS1_PADDING);
		break;
	}
	}
	
	if (NumEncryptedBytes == InKey->KeySizeInBytes)
	{
		return true;
	}
	else
	{
		OutDestination.Empty();
		return false;
	}
}

bool FRSA::Decrypt(EKeyType InKeyType, const TArray<uint8>& InSource, uint8* OutDestination, int32 OutDestinationSizeInBytes, FRSA::TKeyPtr InKey)
{
	if (InSource.Num() == InKey->GetKeySizeInBytes() && OutDestinationSizeInBytes <= InKey->GetKeySizeInBytes())
	{
		int NumDecryptedBytes = 0;

		switch (InKeyType)
		{
		case EKeyType::Public:
		{
			NumDecryptedBytes = RSA_public_decrypt(InSource.Num(), InSource.GetData(), OutDestination, InKey->RSAKey, RSA_PKCS1_PADDING);
			break;
		}

		case EKeyType::Private:
		{
			NumDecryptedBytes = RSA_private_decrypt(InSource.Num(), InSource.GetData(), OutDestination, InKey->RSAKey, RSA_PKCS1_PADDING);
			break;
		}
		}

		if (NumDecryptedBytes >= 0 && NumDecryptedBytes <= OutDestinationSizeInBytes)
		{
			return true;
		}
		else
		{
			long Error = ERR_get_error();
			const char* Message = ERR_error_string(Error, nullptr);
			return false;
		}
	}

	return false;
}

#else // !RSA_USE_OPENSSL

//#pragma message ("Using TBigInt for RSA")
#include "Math/RandomStream.h"

struct FRSA::FKey
{
	virtual int32 GetKeySizeInBits() const = 0;
	virtual int32 GetKeySizeInBytes() const = 0;
	virtual int32 GetMaxDataSize() const = 0;
	virtual bool Encrypt(FRSA::EKeyType InKeyType, const uint8* InSource, int32 InSourceSizeInBytes, TArray<uint8>& OutDestination) const = 0;
	virtual bool Decrypt(FRSA::EKeyType InKeyType, const TArray<uint8>& InSource, uint8* OutDestination, int32 OutDestinationSizeInBytes) const = 0;
};

template <int32 KEY_SIZE>
struct FFixedKey : public FRSA::FKey
{
	typedef TBigInt<KEY_SIZE * 2, false> TInternalBigInt;
	TInternalBigInt PublicExponent;
	TInternalBigInt PrivateExponent;
	TInternalBigInt Modulus;

	FFixedKey(const TArray<uint8>& InPublicExponent, const TArray<uint8>& InPrivateExponent, const TArray<uint8>& InModulus)
	{
		if (InPublicExponent.Num())
		{
			PublicExponent = TInternalBigInt(InPublicExponent.GetData(), InPublicExponent.Num());
		}

		if (InPrivateExponent.Num())
		{
			PrivateExponent = TInternalBigInt(InPrivateExponent.GetData(), InPrivateExponent.Num());
		}

		if (InModulus.Num())
		{
			Modulus = TInternalBigInt(InModulus.GetData(), InModulus.Num());
		}
	}

	int32 GetKeySizeInBits() const override
	{
		return KEY_SIZE;
	}

	int32 GetKeySizeInBytes() const override
	{
		return KEY_SIZE / 8;
	}

	/** 
		* Return the maximum amount of data that can be encrypted within the key, as you would if you were using a proper 
		* RSA padding scheme. Because this is a legacy system and should be deprecated soon, we're just going to use random byte
		* padding, but we'll reflect the correct RSA rules anyway
		*/
	int32 GetMaxDataSize() const override
	{
		return GetKeySizeInBytes() - 11;
	}

	bool Encrypt(FRSA::EKeyType InKeyType, const uint8* InSource, int32 InSourceSizeInBytes, TArray<uint8>& OutDestination) const override
	{
		if (InSourceSizeInBytes <= GetMaxDataSize())
		{
			TInternalBigInt Source(InSource, InSourceSizeInBytes);
			int32 PaddingBytesRequired = GetKeySizeInBytes() - InSourceSizeInBytes;
			uint8* PaddingStart = (uint8*)Source.GetBits() + InSourceSizeInBytes;
			// Not perfect, but an improvement on before, and soon to be defuncted anyway
			for (int32 PaddingIndex = 0; PaddingIndex < PaddingBytesRequired; ++PaddingIndex)
			{
				PaddingStart[PaddingIndex] = (uint8)FMath::RandRange(0, 255);
			}
			PaddingStart[PaddingBytesRequired - 1] &= 0x3f; // keep top two bits clear because this seems to cause the decryption to fail. TODO: Find out why... presumably some bug in TBigInt
			TInternalBigInt Result = FEncryption::ModularPow(Source, InKeyType == FRSA::EKeyType::Public ? PublicExponent : PrivateExponent, Modulus);
			TInternalBigInt Test = FEncryption::ModularPow(Result, InKeyType == FRSA::EKeyType::Public ? PrivateExponent : PublicExponent, Modulus);
			check(Source == Test);
			OutDestination.SetNum(GetKeySizeInBytes());
			FMemory::Memcpy(OutDestination.GetData(), Result.GetBits(), GetKeySizeInBytes());
		}
		else
		{
			OutDestination.Empty();
		}

		return true;
	}

	bool Decrypt(FRSA::EKeyType InKeyType, const TArray<uint8>& InSource, uint8* OutDestination, int32 OutDestinationSizeInBytes) const override
	{
		if (InSource.Num() == GetKeySizeInBytes() && OutDestinationSizeInBytes <= GetKeySizeInBytes())
		{
			TInternalBigInt Source(InSource.GetData(), InSource.Num());
			TInternalBigInt Result = FEncryption::ModularPow(Source, InKeyType == FRSA::EKeyType::Public ? PublicExponent : PrivateExponent, Modulus);
			FMemory::Memcpy(OutDestination, Result.GetBits(), OutDestinationSizeInBytes);
			return true;
		}
		else
		{
			FMemory::Memset(OutDestination, 0, OutDestinationSizeInBytes);
			return false;
		}
	}
};

int32 NumElementsIgnoringTrailingZeroes(const TArray<uint8>& InBytes)
{
	int32 Count = InBytes.Num();

	while (Count > 0 && InBytes[Count - 1] == 0)
	{
		Count--;
	}

	return Count;
}

FRSA::TKeyPtr FRSA::CreateKey(const TArray<uint8>& InPublicExponent, const TArray<uint8>& InPrivateExponent, const TArray<uint8>& InModulus)
{
	// The key data generated by openSSL is little endian, which is great for the latest version of OpenSSL and for our TBigInt. The older
	// version of OpenSSL still in use by Linux/Max doesn't have the same little-endian import functions so we have to swizzle the data 
	// in that case. OpenSSL should be upgraded on those platforms at some point, at which point we can remove this sub-optimal behavior
	int32 NumPublicExponentBytes = NumElementsIgnoringTrailingZeroes(InPublicExponent);
	int32 NumPrivateExponentBytes = NumElementsIgnoringTrailingZeroes(InPrivateExponent);
	int32 NumModulusBytes = NumElementsIgnoringTrailingZeroes(InModulus);

	int32 RequiredNumBytes = FMath::Max(NumModulusBytes, FMath::Max(NumPublicExponentBytes, NumPrivateExponentBytes));
	RequiredNumBytes = FMath::RoundUpToPowerOfTwo(RequiredNumBytes);
	int32 RequiredNumBits = RequiredNumBytes * 8;

	// With the legacy fixed key system, which is based on a templated large integer class, we can't be that
	// dynamic with the key lengths. Eventually, we'll move over to the totally dynamic version, but this is still an
	// expansion on the previous functionality
	FRSA::TKeyPtr Result;
	
	switch (RequiredNumBits)
	{
	case 4096: Result = MakeShared<FFixedKey<4096>, FRSA::KeyThreadMode>(InPublicExponent, InPrivateExponent, InModulus); break;
	case 2048: Result = MakeShared<FFixedKey<2048>, FRSA::KeyThreadMode>(InPublicExponent, InPrivateExponent, InModulus); break;
	case 1024: Result = MakeShared<FFixedKey<1024>, FRSA::KeyThreadMode>(InPublicExponent, InPrivateExponent, InModulus); break;
	case 512: Result = MakeShared<FFixedKey<512>, FRSA::KeyThreadMode>(InPublicExponent, InPrivateExponent, InModulus); break;
	case 256: Result = MakeShared<FFixedKey<256>, FRSA::KeyThreadMode>(InPublicExponent, InPrivateExponent, InModulus); break;
	default: break;
	}
		
	return Result;
}

bool FRSA::Encrypt(FRSA::EKeyType InKeyType, const uint8* InSource, int32 InSourceSizeInBytes, TArray<uint8>& OutDestination, FRSA::TKeyPtr InKey)
{
	return InKey->Encrypt(InKeyType, InSource, InSourceSizeInBytes, OutDestination);
}

bool FRSA::Decrypt(FRSA::EKeyType InKeyType, const TArray<uint8>& InSource, uint8* OutDestination, int32 OutDestinationSizeInBytes, FRSA::TKeyPtr InKey)
{
	return InKey->Decrypt(InKeyType, InSource, OutDestination, OutDestinationSizeInBytes);
}
#endif

int32 FRSA::GetKeySizeInBits(FRSA::TKeyPtr InKey)
{
	return InKey->GetKeySizeInBits();
}

int32 FRSA::GetMaxDataSizeInBytes(FRSA::TKeyPtr InKey)
{
	return InKey->GetMaxDataSize();
}

bool FRSA::Encrypt(EKeyType InKeyType, const TArray<uint8>& InSource, TArray<uint8>& OutDestination, FRSA::TKeyPtr InKey)
{
	return Encrypt(InKeyType, InSource.GetData(), InSource.Num(), OutDestination, InKey);
}

bool FRSA::Decrypt(EKeyType InKeyType, const TArray<uint8>& InSource, TArray<uint8>& OutDestination, FRSA::TKeyPtr InKey)
{
	OutDestination.SetNum(InKey->GetKeySizeInBytes());
	return Decrypt(InKeyType, InSource, OutDestination.GetData(), OutDestination.Num(), InKey);
}

#if RSA_USE_OPENSSL && USE_LEGACY_OPENSSL
/**
 * Module interface for RSA.
 */
class FRSAModule : public FDefaultModuleImpl
{
public:

	virtual void StartupModule() override
	{
		// Provide a locking callback so that an RSA key can be used from multiple threads safely.
		GetLocks().AddDefaulted(CRYPTO_num_locks());
		CRYPTO_set_locking_callback(LockingCallback);
	}

	virtual void ShutdownModule() override
	{
		CRYPTO_set_locking_callback(nullptr);

		// Just in case
		for (FCriticalSection& CriticalSection : GetLocks())
		{
			CriticalSection.Unlock();
		}
	}

	static TArray<FCriticalSection>& GetLocks()
	{
		static TArray<FCriticalSection> Locks;
		return Locks;
	}

	static void LockingCallback(int mode, int n, const char *file, int line)
	{
		TArray<FCriticalSection>& Locks = GetLocks();
		check(Locks.Num() > 0);

		if (mode & CRYPTO_LOCK)
			Locks[n].Lock();
		else
			Locks[n].Unlock();
	}
};

IMPLEMENT_MODULE(FRSAModule, RSA);
#else
// OpenSSL 1.1.1+ handles thread safety of keys internally, so there is no need to provide any custom handling and we can just use a default module implementation
IMPLEMENT_MODULE(FDefaultModuleImpl, RSA);
#endif