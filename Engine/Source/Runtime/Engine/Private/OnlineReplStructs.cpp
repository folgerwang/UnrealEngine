// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OnlineReplStructs.cpp: Unreal networking serialization helpers
=============================================================================*/

#include "GameFramework/OnlineReplStructs.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "UObject/PropertyPortFlags.h"
#include "Dom/JsonValue.h"
#include "EngineLogs.h"
#include "Net/OnlineEngineInterface.h"

/** Flags relevant to network serialization of a unique id */
enum class EUniqueIdEncodingFlags : uint8
{
	/** Default, nothing encoded, use normal FString serialization */
	NotEncoded = 0,
	/** Data is optimized based on some assumptions (even number of [0-9][a-f][A-F] that can be packed into nibbles) */
	IsEncoded = (1 << 0),
	/** This unique id is empty or invalid, nothing further to serialize */
	IsEmpty = (1 << 1)
};
ENUM_CLASS_FLAGS(EUniqueIdEncodingFlags);

FArchive& operator<<( FArchive& Ar, FUniqueNetIdRepl& UniqueNetId)
{
	int32 Size = UniqueNetId.IsValid() ? UniqueNetId->GetSize() : 0;
	Ar << Size;

	if (Size > 0)
	{
		if (Ar.IsSaving())
		{
			check(UniqueNetId.IsValid());
			FString Contents = UniqueNetId->ToString();
			Ar << Contents;
		}
		else if (Ar.IsLoading())
		{
			FString Contents;
			Ar << Contents;	// that takes care about possible overflow

			UniqueNetId.UniqueIdFromString(Contents);
		}
	}
	else if (Ar.IsLoading())
	{
		// @note: replicated a nullptr unique id
		UniqueNetId.SetUniqueNetId(nullptr);
	}

	return Ar;
}

/**
 * Possibly encode the unique net id in a smaller form
 *
 * Empty:
 *    <uint8 flags> noted it is encoded and empty
 * NonEmpty:
 * - Encoded - <uint8 flags> <uint8 encoded size> <encoded bytes>
 * - Unencoded - <uint8 flags> <serialized FString>
 */
void FUniqueNetIdRepl::MakeReplicationData()
{
	//UE_LOG(LogNet, Warning, TEXT("MakeReplicationData %s"), *ToString());

	FString Contents;
	if (IsValid())
	{
		Contents = UniqueNetId->ToString();
	}

	const int32 Length = Contents.Len();
	if (Length > 0)
	{
		// For now don't allow odd chars (HexToBytes adds a 0)
		const bool bEvenChars = (Length % 2) == 0;
		const int32 EncodedSize32 = ((Length * sizeof(ANSICHAR)) + 1) / 2;
		EUniqueIdEncodingFlags EncodingFlags = (bEvenChars && (EncodedSize32 < UINT8_MAX)) ? EUniqueIdEncodingFlags::IsEncoded : EUniqueIdEncodingFlags::NotEncoded;
		if (EnumHasAllFlags(EncodingFlags, EUniqueIdEncodingFlags::IsEncoded))
		{
			const TCHAR* const ContentChar = *Contents;
			for (int32 i = 0; i < Length; ++i)
			{
				// Don't allow uppercase because HexToBytes loses case and we aren't encoding anything but all lowercase hex right now
				if (!FChar::IsHexDigit(ContentChar[i]) || FChar::IsUpper(ContentChar[i]))
				{
					EncodingFlags = EUniqueIdEncodingFlags::NotEncoded;
					break;
				}
			}
		}

		if (EnumHasAllFlags(EncodingFlags, EUniqueIdEncodingFlags::IsEncoded))
		{
			uint8 EncodedSize = static_cast<uint8>(EncodedSize32);
			const int32 TotalBytes = sizeof(EncodingFlags) + sizeof(EncodedSize) + EncodedSize;
			ReplicationBytes.Empty(TotalBytes);

			FMemoryWriter Writer(ReplicationBytes);
			Writer << EncodingFlags;
			Writer << EncodedSize;

			int32 HexStartOffset = Writer.Tell();
			ReplicationBytes.AddUninitialized(EncodedSize);
			int32 HexEncodeLength = HexToBytes(Contents, ReplicationBytes.GetData() + HexStartOffset);
			ensure(HexEncodeLength == EncodedSize32);
			//Writer.Seek(HexStartOffset + HexEncodeLength);
			//UE_LOG(LogNet, Warning, TEXT("HexEncoded UniqueId, serializing %d bytes"), ReplicationBytes.Num());
		}
		else
		{
			ReplicationBytes.Empty(Length);
			FMemoryWriter Writer(ReplicationBytes);
			Writer << EncodingFlags;
			Writer << Contents;
			//UE_LOG(LogNet, Warning, TEXT("Normal UniqueId, serializing %d bytes"), ReplicationBytes.Num());
		}
	}
	else
	{
		EUniqueIdEncodingFlags EncodingFlags = (EUniqueIdEncodingFlags::IsEncoded | EUniqueIdEncodingFlags::IsEmpty);

		ReplicationBytes.Empty();
		FMemoryWriter Writer(ReplicationBytes);
		Writer << EncodingFlags;
		//UE_LOG(LogNet, Warning, TEXT("Empty/Invalid UniqueId, serializing %d bytes"), ReplicationBytes.Num());
	}
}

void FUniqueNetIdRepl::UniqueIdFromString(const FString& Contents)
{
	// Don't need to distinguish OSS interfaces here with world because we just want the create function below
	TSharedPtr<const FUniqueNetId> UniqueNetIdPtr = UOnlineEngineInterface::Get()->CreateUniquePlayerId(Contents);
	SetUniqueNetId(UniqueNetIdPtr);
}

bool FUniqueNetIdRepl::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = false;

	if (Ar.IsSaving())
	{
		if (ReplicationBytes.Num() == 0)
		{
			MakeReplicationData();
		}

		Ar.Serialize(ReplicationBytes.GetData(), ReplicationBytes.Num());
		bOutSuccess = (ReplicationBytes.Num() > 0);
		//UE_LOG(LogNet, Warning, TEXT("UID Save: ByteSize: %d Success: %d"), ReplicationBytes.Num(), bOutSuccess);
	}
	else if (Ar.IsLoading())
	{
		// @note: start by assuming a replicated nullptr unique id
		UniqueNetId.Reset();

		EUniqueIdEncodingFlags EncodingFlags = EUniqueIdEncodingFlags::NotEncoded;
		Ar << EncodingFlags;
		if (!Ar.IsError())
		{
			if (EnumHasAllFlags(EncodingFlags, EUniqueIdEncodingFlags::IsEncoded))
			{
				if (!EnumHasAllFlags(EncodingFlags, EUniqueIdEncodingFlags::IsEmpty))
				{
					// Non empty and hex encoded
					uint8 EncodedSize = 0;
					Ar << EncodedSize;
					if (!Ar.IsError())
					{
						if (EncodedSize > 0)
						{
							uint8* TempBytes = (uint8*)FMemory_Alloca(EncodedSize);
							Ar.Serialize(TempBytes, EncodedSize);
							if (!Ar.IsError())
							{
								FString Contents = BytesToHex(TempBytes, EncodedSize);
								if (Contents.Len() > 0)
								{
									// BytesToHex loses case
									Contents.ToLowerInline();
									UniqueIdFromString(Contents);
								}
							}
							else
							{
								UE_LOG(LogNet, Warning, TEXT("Error with encoded unique id contents"));
							}
						}
						else
						{
							UE_LOG(LogNet, Warning, TEXT("Empty Encoding!"));
						}

						bOutSuccess = (EncodedSize == 0) || IsValid();
					}
					else
					{
						UE_LOG(LogNet, Warning, TEXT("Error with encoded unique id size"));
					}
				}
				else
				{
					// empty cleared out unique id
					bOutSuccess = true;
				}
			}
			else
			{
				// Original FString serialization goes here
				FString Contents;
				Ar << Contents;
				if (!Ar.IsError())
				{
					UniqueIdFromString(Contents);
					bOutSuccess = !Contents.IsEmpty();
				}
				else
				{
					UE_LOG(LogNet, Warning, TEXT("Error with unencoded unique id"));
				}
			}
		}
		else
		{
			UE_LOG(LogNet, Warning, TEXT("Error serializing unique id"));
		}
	}

	return true;
}

bool FUniqueNetIdRepl::Serialize(FArchive& Ar)
{
	Ar << *this;
	return true;
}

bool FUniqueNetIdRepl::ExportTextItem(FString& ValueStr, FUniqueNetIdRepl const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (0 != (PortFlags & EPropertyPortFlags::PPF_ExportCpp))
	{
		return false;
	}

	ValueStr += UniqueNetId.IsValid() ? UniqueNetId->ToString() : TEXT("INVALID");
	return true;
}

bool FUniqueNetIdRepl::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	SetUniqueNetId(nullptr);

	bool bShouldWarn = true;
	if (Buffer)
	{
		static FString InvalidString(TEXT("INVALID"));
		if (Buffer[0] == TEXT('\0') || Buffer == InvalidString)
		{
			// An empty string or the word invalid are just considered expected invalid FUniqueNetIdRepls. No need to warn about those.
			bShouldWarn = false;
		}
		else
		{
			checkf(UOnlineEngineInterface::Get() && UOnlineEngineInterface::Get()->IsLoaded(), TEXT("Attempted to ImportText to FUniqueNetIdRepl while OSS is not loaded. Parent:%s"), *GetPathNameSafe(Parent));
			FString Contents(Buffer);
			UniqueIdFromString(Contents);
		}
	}

	if (bShouldWarn && !IsValid())
	{
#if !NO_LOGGING
		ErrorText->CategorizedLogf(LogNet.GetCategoryName(), ELogVerbosity::Warning, TEXT("Failed to import text to FUniqueNetIdRepl Parent:%s"), *GetPathNameSafe(Parent));
#endif
	}

	return true;
}

TSharedRef<FJsonValue> FUniqueNetIdRepl::ToJson() const
{
	if (IsValid())
	{
		return MakeShareable(new FJsonValueString(ToString()));
	}
	else
	{
		return MakeShareable(new FJsonValueString(TEXT("INVALID")));
	}
}

void FUniqueNetIdRepl::FromJson(const FString& Json)
{
	SetUniqueNetId(nullptr);
	if (!Json.IsEmpty())
	{
		UniqueIdFromString(Json);
	}
}

void TestUniqueIdRepl(UWorld* InWorld)
{
#if !UE_BUILD_SHIPPING
	bool bSuccess = true;

	TSharedPtr<const FUniqueNetId> UserId = UOnlineEngineInterface::Get()->GetUniquePlayerId(InWorld, 0);

	FUniqueNetIdRepl EmptyIdIn;
	if (EmptyIdIn.IsValid())
	{
		UE_LOG(LogNet, Warning, TEXT("EmptyId is valid."), *EmptyIdIn->ToString());
		bSuccess = false;
	}

	FUniqueNetIdRepl ValidIdIn(UserId);
	if (!ValidIdIn.IsValid() || UserId != ValidIdIn.GetUniqueNetId() || *UserId != *ValidIdIn)
	{
		UE_LOG(LogNet, Warning, TEXT("UserId input %s != UserId output %s"), *UserId->ToString(), *ValidIdIn->ToString());
		bSuccess = false;
	}

	FUniqueNetIdRepl OddStringIdIn(UOnlineEngineInterface::Get()->CreateUniquePlayerId(TEXT("abcde")));
	FUniqueNetIdRepl NonHexStringIdIn(UOnlineEngineInterface::Get()->CreateUniquePlayerId(TEXT("thisisnothex")));
	FUniqueNetIdRepl UpperCaseStringIdIn(UOnlineEngineInterface::Get()->CreateUniquePlayerId(TEXT("abcDEF")));

#if 1
	#define WAYTOOLONG TEXT("deadbeefba5eba11deadbeefba5eba11 \
		deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11 \
deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11 \
		deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11 \
		deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11 \
		deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11deadbeefba5eba11")
#else
	#define WAYTOOLONG TEXT("deadbeef")
#endif

	FUniqueNetIdRepl WayTooLongForHexEncodingIdIn(UOnlineEngineInterface::Get()->CreateUniquePlayerId(WAYTOOLONG));
	if (bSuccess)
	{
		// Regular Serialization
		{
			TArray<uint8> Buffer;
			Buffer.Empty();

			// Serialize In
			{
				FMemoryWriter TestUniqueIdWriter(Buffer);

				TestUniqueIdWriter << EmptyIdIn;
				TestUniqueIdWriter << ValidIdIn;
			}	
			
			FUniqueNetIdRepl EmptyIdOut;
			FUniqueNetIdRepl ValidIdOut;
			// Serialize Out
			{
				FMemoryReader TestUniqueIdReader(Buffer);
				TestUniqueIdReader << EmptyIdOut;
				TestUniqueIdReader << ValidIdOut;
			}
			
			if (EmptyIdOut.IsValid())
			{
				UE_LOG(LogNet, Warning, TEXT("EmptyId %s should have been invalid"), *EmptyIdOut->ToString());
				bSuccess = false;
			}

			if (*UserId != *ValidIdOut.GetUniqueNetId())
			{
				UE_LOG(LogNet, Warning, TEXT("UserId input %s != UserId output %s"), *ValidIdIn->ToString(), *ValidIdOut->ToString());
				bSuccess = false;
			}
		}

		// Network serialization
		{
			bool bOutSuccess = false;

			TArray<uint8> Buffer;
			Buffer.Empty();
			
			// Serialize In
			uint8 EncodingFailures = 0;
			{
				FMemoryWriter TestUniqueIdWriter(Buffer);

				EmptyIdIn.NetSerialize(TestUniqueIdWriter, nullptr, bOutSuccess);
				EncodingFailures += bOutSuccess ? 0 : 1;
				ValidIdIn.NetSerialize(TestUniqueIdWriter, nullptr, bOutSuccess);
				EncodingFailures += bOutSuccess ? 0 : 1;
				OddStringIdIn.NetSerialize(TestUniqueIdWriter, nullptr, bOutSuccess);
				EncodingFailures += bOutSuccess ? 0 : 1;
				NonHexStringIdIn.NetSerialize(TestUniqueIdWriter, nullptr, bOutSuccess);
				EncodingFailures += bOutSuccess ? 0 : 1;
				UpperCaseStringIdIn.NetSerialize(TestUniqueIdWriter, nullptr, bOutSuccess);
				EncodingFailures += bOutSuccess ? 0 : 1;
				WayTooLongForHexEncodingIdIn.NetSerialize(TestUniqueIdWriter, nullptr, bOutSuccess);
				EncodingFailures += bOutSuccess ? 0 : 1;
			}

			FUniqueNetIdRepl EmptyIdOut;
			FUniqueNetIdRepl ValidIdOut;
			FUniqueNetIdRepl OddStringIdOut;
			FUniqueNetIdRepl NonHexStringIdOut;
			FUniqueNetIdRepl UpperCaseStringIdOut;
			FUniqueNetIdRepl WayTooLongForHexEncodingIdOut;
			// Serialize Out
			uint8 DecodingFailures = 0;
			{
				FMemoryReader TestUniqueIdReader(Buffer);

				EmptyIdOut.NetSerialize(TestUniqueIdReader, nullptr, bOutSuccess);
				DecodingFailures += bOutSuccess ? 0 : 1;
				ValidIdOut.NetSerialize(TestUniqueIdReader, nullptr, bOutSuccess);
				DecodingFailures += bOutSuccess ? 0 : 1;
				OddStringIdOut.NetSerialize(TestUniqueIdReader, nullptr, bOutSuccess);
				DecodingFailures += bOutSuccess ? 0 : 1;
				NonHexStringIdOut.NetSerialize(TestUniqueIdReader, nullptr, bOutSuccess);
				DecodingFailures += bOutSuccess ? 0 : 1;	
				UpperCaseStringIdOut.NetSerialize(TestUniqueIdReader, nullptr, bOutSuccess);
				DecodingFailures += bOutSuccess ? 0 : 1;
				WayTooLongForHexEncodingIdOut.NetSerialize(TestUniqueIdReader, nullptr, bOutSuccess);
				DecodingFailures += bOutSuccess ? 0 : 1;
			}

			if (EmptyIdOut.IsValid())
			{
				UE_LOG(LogNet, Warning, TEXT("EmptyId %s should have been invalid"), *EmptyIdOut->ToString());
				bSuccess = false;
			}

			if (*UserId != *ValidIdOut.GetUniqueNetId())
			{
				UE_LOG(LogNet, Warning, TEXT("UserId input %s != UserId output %s"), *ValidIdIn->ToString(), *ValidIdOut->ToString());
				bSuccess = false;
			}

			if (*OddStringIdIn != *OddStringIdOut)
			{
				UE_LOG(LogNet, Warning, TEXT("OddStringIdIn %s != OddStringIdOut %s"), *OddStringIdIn->ToString(), *OddStringIdOut->ToString());
				bSuccess = false;
			}

			if (*NonHexStringIdIn != *NonHexStringIdOut)
			{
				UE_LOG(LogNet, Warning, TEXT("NonHexStringIdIn %s != NonHexStringIdOut %s"), *NonHexStringIdIn->ToString(), *NonHexStringIdOut->ToString());
				bSuccess = false;
			}

			if (*UpperCaseStringIdIn != *UpperCaseStringIdOut)
			{
				UE_LOG(LogNet, Warning, TEXT("UpperCaseStringIdIn %s != UpperCaseStringIdOut %s"), *UpperCaseStringIdIn->ToString(), *UpperCaseStringIdOut->ToString());
				bSuccess = false;
			}

			if (*WayTooLongForHexEncodingIdIn != *WayTooLongForHexEncodingIdOut)
			{
				UE_LOG(LogNet, Warning, TEXT("WayTooLongForHexEncodingIdIn %s != WayTooLongForHexEncodingIdOut %s"), *WayTooLongForHexEncodingIdIn->ToString(), *WayTooLongForHexEncodingIdOut->ToString());
				bSuccess = false;
			}
		}
	}

	if (bSuccess)
	{
		FString OutString;
		TSharedRef<FJsonValue> JsonValue = ValidIdIn.ToJson();
		bSuccess = JsonValue->TryGetString(OutString);
		if (bSuccess)
		{
			FUniqueNetIdRepl NewIdOut;
			NewIdOut.FromJson(OutString);
			bSuccess = NewIdOut.IsValid();
		}
	}

	if (!bSuccess)
	{
		UE_LOG(LogNet, Warning, TEXT("TestUniqueIdRepl test failure!"));
	}
#endif
}




PRAGMA_ENABLE_OPTIMIZATION