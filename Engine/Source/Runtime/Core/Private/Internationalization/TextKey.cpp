// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextKey.h"
#include "Containers/Map.h"
#include "Containers/StringConv.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Misc/Crc.h"
#include "Misc/ByteSwap.h"
#include "Misc/ScopeLock.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextKey, Log, All);

class FTextKeyState
{
public:
	void FindOrAdd(const TCHAR* InStr, const int32 InStrLen, const TCHAR*& OutStrPtr, uint32& OutStrHash)
	{
		check(*InStr != 0);

		FScopeLock ScopeLock(&SynchronizationObject);

		const FKeyData SrcKey(InStr, InStrLen);
		const FString* StrPtr = KeysTable.Find(SrcKey);
		if (!StrPtr)
		{
			FString StrCopy(InStrLen, InStr); // Need to copy the string here so we can reference its internal allocation as the key
			const FKeyData DestKey(*StrCopy, StrCopy.Len(), SrcKey.StrHash);
			StrPtr = &KeysTable.Add(DestKey, MoveTemp(StrCopy));
			check(DestKey.Str == **StrPtr); // The move must have moved the allocation we referenced in the key
		}

		OutStrPtr = **StrPtr;
		OutStrHash = SrcKey.StrHash;
	}

	void FindOrAdd(const TCHAR* InStr, const int32 InStrLen, const uint32 InStrHash, const TCHAR*& OutStrPtr)
	{
		check(*InStr != 0);

		FScopeLock ScopeLock(&SynchronizationObject);

		const FKeyData SrcKey(InStr, InStrLen, InStrHash);
		const FString* StrPtr = KeysTable.Find(SrcKey);
		if (!StrPtr)
		{
			FString StrCopy(InStrLen, InStr); // Need to copy the string here so we can reference its internal allocation as the key
			const FKeyData DestKey(*StrCopy, StrCopy.Len(), SrcKey.StrHash);
			StrPtr = &KeysTable.Add(DestKey, MoveTemp(StrCopy));
			check(DestKey.Str == **StrPtr); // The move must have moved the allocation we referenced in the key
		}

		OutStrPtr = **StrPtr;
	}

	void FindOrAdd(const FString& InStr, const TCHAR*& OutStrPtr, uint32& OutStrHash)
	{
		check(!InStr.IsEmpty());

		FScopeLock ScopeLock(&SynchronizationObject);

		const FKeyData SrcKey(*InStr, InStr.Len());
		const FString* StrPtr = KeysTable.Find(SrcKey);
		if (!StrPtr)
		{
			FString StrCopy = InStr; // Need to copy the string here so we can reference its internal allocation as the key
			const FKeyData DestKey(*StrCopy, StrCopy.Len(), SrcKey.StrHash);
			StrPtr = &KeysTable.Add(DestKey, MoveTemp(StrCopy));
			check(DestKey.Str == **StrPtr); // The move must have moved the allocation we referenced in the key
		}
		
		OutStrPtr = **StrPtr;
		OutStrHash = SrcKey.StrHash;
	}

	void FindOrAdd(FString&& InStr, const TCHAR*& OutStrPtr, uint32& OutStrHash)
	{
		check(!InStr.IsEmpty());

		FScopeLock ScopeLock(&SynchronizationObject);

		const FKeyData SrcKey(*InStr, InStr.Len());
		const FString* StrPtr = KeysTable.Find(SrcKey);
		if (!StrPtr)
		{
			const FKeyData DestKey(*InStr, InStr.Len(), SrcKey.StrHash);
			StrPtr = &KeysTable.Add(DestKey, MoveTemp(InStr));
			check(DestKey.Str == **StrPtr); // The move must have moved the allocation we referenced in the key
		}

		OutStrPtr = **StrPtr;
		OutStrHash = SrcKey.StrHash;
	}

	void Shrink()
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		KeysTable.Shrink();
	}

	static FTextKeyState& GetState()
	{
		static FTextKeyState State;
		return State;
	}

private:
	struct FKeyData
	{
		FKeyData(const TCHAR* InStr, const int32 InStrLen)
			: Str(InStr)
			, StrLen(InStrLen)
			, StrHash(FCrc::StrCrc32(Str)) // Note: This hash gets serialized so *DO NOT* change it
		{
		}

		FKeyData(const TCHAR* InStr, const int32 InStrLen, const uint32 InStrHash)
			: Str(InStr)
			, StrLen(InStrLen)
			, StrHash(InStrHash)
		{
		}

		friend FORCEINLINE bool operator==(const FKeyData& A, const FKeyData& B)
		{
			// We can use Memcmp here as we know we're comparing two blocks of the same size and don't care about lexical ordering
			return A.StrLen == B.StrLen && FMemory::Memcmp(A.Str, B.Str, A.StrLen * sizeof(TCHAR)) == 0;
		}

		friend FORCEINLINE bool operator!=(const FKeyData& A, const FKeyData& B)
		{
			// We can use Memcmp here as we know we're comparing two blocks of the same size and don't care about lexical ordering
			return A.StrLen != B.StrLen || FMemory::Memcmp(A.Str, B.Str, A.StrLen * sizeof(TCHAR)) != 0;
		}

		friend FORCEINLINE uint32 GetTypeHash(const FKeyData& A)
		{
			return A.StrHash;
		}

		const TCHAR* Str;
		int32 StrLen;
		uint32 StrHash;
	};

	FCriticalSection SynchronizationObject;
	TMap<FKeyData, FString> KeysTable;
};

namespace TextKeyUtil
{

static const int32 InlineStringSize = 128;
typedef TArray<TCHAR, TInlineAllocator<InlineStringSize>> FInlineStringBuffer;

bool SaveKeyString(FArchive& Ar, const TCHAR* InStrPtr)
{
	// Note: This serialization should be compatible with the FString serialization, but avoids creating an FString if the FTextKey is already cached
	// > 0 for ANSICHAR, < 0 for UCS2CHAR serialization
	check(!Ar.IsLoading());

	const bool SaveUCS2Char = Ar.IsForcingUnicode() || !FCString::IsPureAnsi(InStrPtr);
	const int32 Num = FCString::Strlen(InStrPtr) + 1; // include the null terminator

	int32 SaveNum = SaveUCS2Char ? -Num : Num;
	Ar << SaveNum;

	if (SaveNum)
	{
		if (SaveUCS2Char)
		{
			const TCHAR* LittleEndianStrPtr = InStrPtr;

			// TODO - This is creating a temporary in order to byte-swap.  Need to think about how to make this not necessary.
#if !PLATFORM_LITTLE_ENDIAN
			FString LittleEndianStr = FString(Num - 1, LittleEndianStrPtr);
			INTEL_ORDER_TCHARARRAY(LittleEndianStr.GetData());
			LittleEndianStrPtr = *LittleEndianStr;
#endif

			Ar.Serialize((void*)StringCast<UCS2CHAR>(LittleEndianStrPtr, Num).Get(), sizeof(UCS2CHAR)* Num);
		}
		else
		{
			Ar.Serialize((void*)StringCast<ANSICHAR>(InStrPtr, Num).Get(), sizeof(ANSICHAR)* Num);
		}
	}

	return true;
}

bool LoadKeyString(FArchive& Ar, FInlineStringBuffer& OutStrBuffer)
{
	// Note: This serialization should be compatible with the FString serialization, but avoids creating an FString if the FTextKey is already cached
	// > 0 for ANSICHAR, < 0 for UCS2CHAR serialization
	check(Ar.IsLoading());

	int32 SaveNum = 0;
	Ar << SaveNum;

	const bool LoadUCS2Char = SaveNum < 0;
	if (LoadUCS2Char)
	{
		SaveNum = -SaveNum;
	}

	// If SaveNum is still less than 0, they must have passed in MIN_INT. Archive is corrupted.
	if (SaveNum < 0)
	{
		Ar.ArIsError = 1;
		Ar.ArIsCriticalError = 1;
		return false;
	}

	// Protect against network packets allocating too much memory
	const int64 MaxSerializeSize = Ar.GetMaxSerializeSize();
	if ((MaxSerializeSize > 0) && (SaveNum > MaxSerializeSize))
	{
		Ar.ArIsError = 1;
		Ar.ArIsCriticalError = 1;
		return false;
	}

	// Create a buffer of the correct size
	OutStrBuffer.AddUninitialized(SaveNum);

	if (SaveNum)
	{
		if (LoadUCS2Char)
		{
			// Read in the Unicode string and byte-swap it
			auto Passthru = StringMemoryPassthru<UCS2CHAR, TCHAR, InlineStringSize>(OutStrBuffer.GetData(), SaveNum, SaveNum);
			Ar.Serialize(Passthru.Get(), SaveNum * sizeof(UCS2CHAR));
			Passthru.Get()[SaveNum - 1] = 0; // Ensure the string has a null terminator
			Passthru.Apply();

			INTEL_ORDER_TCHARARRAY(StrBuffer.GetData())
		}
		else
		{
			// Read in the ANSI string
			auto Passthru = StringMemoryPassthru<ANSICHAR, TCHAR, InlineStringSize>(OutStrBuffer.GetData(), SaveNum, SaveNum);
			Ar.Serialize(Passthru.Get(), SaveNum * sizeof(ANSICHAR));
			Passthru.Get()[SaveNum - 1] = 0; // Ensure the string has a null terminator
			Passthru.Apply();
		}

		UE_CLOG(SaveNum > InlineStringSize, LogTextKey, VeryVerbose, TEXT("Key string '%s' was larger (%d) than the inline size (%d) and caused an allocation!"), OutStrBuffer.GetData(), SaveNum, InlineStringSize);
	}

	return true;
}

}

FTextKey::FTextKey()
{
	Reset();
}

FTextKey::FTextKey(const TCHAR* InStr)
{
	if (*InStr == 0)
	{
		Reset();
	}
	else
	{
		FTextKeyState::GetState().FindOrAdd(InStr, FCString::Strlen(InStr), StrPtr, StrHash);
	}
}

FTextKey::FTextKey(const FString& InStr)
{
	if (InStr.IsEmpty())
	{
		Reset();
	}
	else
	{
		FTextKeyState::GetState().FindOrAdd(InStr, StrPtr, StrHash);
	}
}

FTextKey::FTextKey(FString&& InStr)
{
	if (InStr.IsEmpty())
	{
		Reset();
	}
	else
	{
		FTextKeyState::GetState().FindOrAdd(MoveTemp(InStr), StrPtr, StrHash);
	}
}

void FTextKey::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		Ar << StrHash;

		TextKeyUtil::FInlineStringBuffer StrBuffer;
		TextKeyUtil::LoadKeyString(Ar, StrBuffer);

		if (StrBuffer.Num() <= 1)
		{
			Reset();
		}
		else
		{
			FTextKeyState::GetState().FindOrAdd(StrBuffer.GetData(), StrBuffer.Num() - 1, StrHash, StrPtr);
		}
	}
	else
	{
		Ar << StrHash;

		TextKeyUtil::SaveKeyString(Ar, StrPtr);
	}
}

void FTextKey::SerializeAsString(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		TextKeyUtil::FInlineStringBuffer StrBuffer;
		TextKeyUtil::LoadKeyString(Ar, StrBuffer);

		if (StrBuffer.Num() <= 1)
		{
			Reset();
		}
		else
		{
			FTextKeyState::GetState().FindOrAdd(StrBuffer.GetData(), StrBuffer.Num() - 1, StrPtr, StrHash);
		}
	}
	else
	{
		TextKeyUtil::SaveKeyString(Ar, StrPtr);
	}
}

void FTextKey::Reset()
{
	StrPtr = TEXT("");
	StrHash = 0;
}

void FTextKey::CompactDataStructures()
{
	FTextKeyState::GetState().Shrink();
}
