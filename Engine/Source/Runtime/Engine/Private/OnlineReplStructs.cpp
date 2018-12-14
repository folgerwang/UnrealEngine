// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OnlineReplStructs.cpp: Unreal networking serialization helpers
=============================================================================*/

#include "GameFramework/OnlineReplStructs.h"
#include "UObject/CoreNet.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "UObject/PropertyPortFlags.h"
#include "Dom/JsonValue.h"
#include "EngineLogs.h"
#include "Net/OnlineEngineInterface.h"

namespace
{
	static const FString InvalidUniqueNetIdStr = TEXT("INVALID");
}

/** Flags relevant to network serialization of a unique id */
enum class EUniqueIdEncodingFlags : uint8
{
	/** Default, nothing encoded, use normal FString serialization */
	NotEncoded = 0,
	/** Data is optimized based on some assumptions (even number of [0-9][a-f][A-F] that can be packed into nibbles) */
	IsEncoded = (1 << 0),
	/** This unique id is empty or invalid, nothing further to serialize */
	IsEmpty = (1 << 1),
	/** Reserved for future use */
	Unused1 = (1 << 2),
	/** Remaining bits are used for encoding the type without requiring another byte */
	Reserved1 = (1 << 3),
	Reserved2 = (1 << 4),
	Reserved3 = (1 << 5),
	Reserved4 = (1 << 6),
	Reserved5 = (1 << 7),
	/** Helper masks */
	FlagsMask = (Reserved1 - 1),
	TypeMask = (MAX_uint8 ^ FlagsMask)
};
ENUM_CLASS_FLAGS(EUniqueIdEncodingFlags);

/** Use highest value for type for other (out of engine) oss type */
const uint8 TypeHash_Other = 31;

FArchive& operator<<(FArchive& Ar, FUniqueNetIdRepl& UniqueNetId)
{
	if (!Ar.IsPersistent() || Ar.IsNetArchive())
	{
		bool bOutSuccess = false;
		UniqueNetId.NetSerialize(Ar, nullptr, bOutSuccess);
	}
	else
	{
		int32 Size = UniqueNetId.IsValid() ? UniqueNetId->GetSize() : 0;
		Ar << Size;

		if (Size > 0)
		{
			if (Ar.IsSaving())
			{
				check(UniqueNetId.IsValid());

				FName Type = UniqueNetId.IsValid() ? UniqueNetId->GetType() : NAME_None;
				Ar << Type;

				FString Contents = UniqueNetId->ToString();
				Ar << Contents;
			}
			else if (Ar.IsLoading())
			{
				FName Type;
				Ar << Type;

				FString Contents;
				Ar << Contents;	// that takes care about possible overflow

				UniqueNetId.UniqueIdFromString(Type, Contents);
			}
		}
		else if (Ar.IsLoading())
		{
			// @note: replicated a nullptr unique id
			UniqueNetId.SetUniqueNetId(nullptr);
		}
	}
	return Ar;
}

inline uint8 GetTypeHashFromEncoding(EUniqueIdEncodingFlags inFlags)
{
	uint8 TypeHash = static_cast<uint8>(inFlags & EUniqueIdEncodingFlags::TypeMask) >> 3;
	return (TypeHash < 32) ? TypeHash : 0;
}

/**
 * Possibly encode the unique net id in a smaller form
 *
 * Empty:
 *    <uint8 flags> noted it is encoded and empty
 * NonEmpty:
 * - Encoded - <uint8 flags/type> <uint8 encoded size> <encoded bytes>
 * - Encoded (out of engine oss type) - <uint8 flags/type> <serialized FName> <uint8 encoded size> <encoded bytes>
 * - Unencoded - <uint8 flags/type> <serialized FString>
 * - Unencoded (out of engine oss type) - <uint8 flags/type> <serialized FName> <serialized FString>
 */
void FUniqueNetIdRepl::MakeReplicationData()
{
	//LOG_SCOPE_VERBOSITY_OVERRIDE(LogNet, ELogVerbosity::VeryVerbose);
	//UE_LOG(LogNet, VeryVerbose, TEXT("MakeReplicationData %s"), *ToString());

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
		const bool bIsNumeric = Contents.IsNumeric();

		//UE_LOG(LogNet, VeryVerbose, TEXT("bEvenChars: %d bIsNumeric: %d EncodedSize: %d"), bEvenChars, bIsNumeric, EncodedSize32);

		EUniqueIdEncodingFlags EncodingFlags = (bIsNumeric || (bEvenChars && (EncodedSize32 < UINT8_MAX))) ? EUniqueIdEncodingFlags::IsEncoded : EUniqueIdEncodingFlags::NotEncoded;
		if (EnumHasAllFlags(EncodingFlags, EUniqueIdEncodingFlags::IsEncoded) && !bIsNumeric)
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

		// Encode the unique id type
		FName Type = GetType();
		uint8 TypeHash = UOnlineEngineInterface::Get()->GetReplicationHashForSubsystem(Type);
		ensure(TypeHash < 32);
		if (TypeHash == 0 && Type != NAME_None)
		{
			TypeHash = TypeHash_Other;
		}
		EncodingFlags = static_cast<EUniqueIdEncodingFlags>((TypeHash << 3) | static_cast<uint8>(EncodingFlags));

		if (EnumHasAllFlags(EncodingFlags, EUniqueIdEncodingFlags::IsEncoded))
		{
			uint8 EncodedSize = static_cast<uint8>(EncodedSize32);
			const int32 TotalBytes = sizeof(EncodingFlags) + sizeof(EncodedSize) + EncodedSize;
			ReplicationBytes.Empty(TotalBytes);

			FMemoryWriter Writer(ReplicationBytes);
			Writer << EncodingFlags;
			if (TypeHash == TypeHash_Other)
			{
				FString TypeString = Type.ToString();
				Writer << TypeString;
			}
			Writer << EncodedSize;

			int32 HexStartOffset = Writer.Tell();
			ReplicationBytes.AddUninitialized(EncodedSize);
			int32 HexEncodeLength = HexToBytes(Contents, ReplicationBytes.GetData() + HexStartOffset);
			ensure(HexEncodeLength == EncodedSize32);
			//Writer.Seek(HexStartOffset + HexEncodeLength);
			//UE_LOG(LogNet, VeryVerbose, TEXT("HexEncoded UniqueId, serializing %d bytes"), ReplicationBytes.Num());
		}
		else
		{
			ReplicationBytes.Empty(Length);

			FMemoryWriter Writer(ReplicationBytes);
			Writer << EncodingFlags;
			if (TypeHash == TypeHash_Other)
			{
				FString TypeString = Type.ToString();
				Writer << TypeString;
			}
			Writer << Contents;
			//UE_LOG(LogNet, VeryVerbose, TEXT("Normal UniqueId, serializing %d bytes"), ReplicationBytes.Num());
		}
	}
	else
	{
		EUniqueIdEncodingFlags EncodingFlags = (EUniqueIdEncodingFlags::IsEncoded | EUniqueIdEncodingFlags::IsEmpty);

		ReplicationBytes.Empty();
		FMemoryWriter Writer(ReplicationBytes);
		Writer << EncodingFlags;
		//UE_LOG(LogNet, VeryVerbose, TEXT("Empty/Invalid UniqueId, serializing %d bytes"), ReplicationBytes.Num());
	}
}

void FUniqueNetIdRepl::UniqueIdFromString(FName Type, const FString& Contents)
{
	// Don't need to distinguish OSS interfaces here with world because we just want the create function below
	TSharedPtr<const FUniqueNetId> UniqueNetIdPtr = UOnlineEngineInterface::Get()->CreateUniquePlayerId(Contents, Type);
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
		//UE_LOG(LogNet, Warning, TEXT("UID Save: Bytes: %d Success: %d"), ReplicationBytes.Num(), bOutSuccess);
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
					uint8 TypeHash = GetTypeHashFromEncoding(EncodingFlags);
					if (TypeHash == 0)
					{
						// If no type was encoded, assume default
						TypeHash = UOnlineEngineInterface::Get()->GetReplicationHashForSubsystem(UOnlineEngineInterface::Get()->GetDefaultOnlineSubsystemName());
					}
					FName Type;
					bool bValidTypeHash = TypeHash != 0;
					if (TypeHash == TypeHash_Other)
					{
						FString TypeString;
						Ar << TypeString;
						Type = FName(*TypeString);
						if (Ar.IsError() || Type == NAME_None)
						{
							bValidTypeHash = false;
						}
					}
					else
					{
						Type = UOnlineEngineInterface::Get()->GetSubsystemFromReplicationHash(TypeHash);
					}

					if (bValidTypeHash)
					{
						// Get the size
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
										if (Type != NAME_None)
										{
											// BytesToHex loses case
											Contents.ToLowerInline();
											UniqueIdFromString(Type, Contents);
										}
										else
										{
											UE_LOG(LogNet, Warning, TEXT("Error with unique id type"));
										}
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
						UE_LOG(LogNet, Warning, TEXT("Error with encoded type hash"));
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
				uint8 TypeHash = GetTypeHashFromEncoding(EncodingFlags);
				if (TypeHash == 0)
				{
					// If no type was encoded, assume default
					TypeHash = UOnlineEngineInterface::Get()->GetReplicationHashForSubsystem(UOnlineEngineInterface::Get()->GetDefaultOnlineSubsystemName());
				}
				FName Type;
				bool bValidTypeHash = TypeHash != 0;
				if (TypeHash == TypeHash_Other)
				{
					FString TypeString;
					Ar << TypeString;
					Type = FName(*TypeString);
					if (Ar.IsError() || Type == NAME_None)
					{
						bValidTypeHash = false;
					}
				}
				else
				{
					Type = UOnlineEngineInterface::Get()->GetSubsystemFromReplicationHash(TypeHash);
				}

				if (bValidTypeHash)
				{
					FString Contents;
					Ar << Contents;
					if (!Ar.IsError())
					{
						if (Type != NAME_None)
						{
							UniqueIdFromString(Type, Contents);
							bOutSuccess = !Contents.IsEmpty();
						}
						else
						{
							UE_LOG(LogNet, Warning, TEXT("Error with unique id type"));
						}
					}
					else
					{
						UE_LOG(LogNet, Warning, TEXT("Error with unencoded unique id"));
					}
				}
				else
				{
					UE_LOG(LogNet, Warning, TEXT("Error with encoded type hash"));
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

	if (IsValid())
	{
		FName Type = UniqueNetId->GetType();
		if (Type == UOnlineEngineInterface::Get()->GetDefaultOnlineSubsystemName())
		{
			ValueStr += FString::Printf(TEXT("%s"), *UniqueNetId->ToString());
		}
		else
		{
			ValueStr += FString::Printf(TEXT("%s:%s"), *Type.ToString(), *UniqueNetId->ToString());
		}
	}
	else
	{
		ValueStr = InvalidUniqueNetIdStr;
	}
	
	return true;
}

bool FUniqueNetIdRepl::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	SetUniqueNetId(nullptr);

	bool bShouldWarn = true;
	if (Buffer)
	{
		if (Buffer[0] == TEXT('\0') || (Buffer == FString(TEXT("()"))) || (Buffer == InvalidUniqueNetIdStr))
		{
			// An empty string, BP empty "()", or the word invalid are just considered expected invalid FUniqueNetIdRepls. No need to warn about those.
			bShouldWarn = false;
		}
		else
		{
			checkf(UOnlineEngineInterface::Get() && UOnlineEngineInterface::Get()->IsLoaded(), TEXT("Attempted to ImportText to FUniqueNetIdRepl while OSS is not loaded. Parent:%s"), *GetPathNameSafe(Parent));
			FString Contents(Buffer);

			TArray<FString> Tokens;
			int32 NumTokens = Contents.ParseIntoArray(Tokens, TEXT(":"));
			if (NumTokens == 2)
			{
				UniqueIdFromString(FName(*Tokens[0]), Tokens[1]);
			}
			else if (NumTokens == 1)
			{
				UniqueIdFromString(NAME_None, Tokens[0]);
			}
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
		const FString JsonString = FString::Printf(TEXT("%s:%s"), *UniqueNetId->GetType().ToString(), *ToString());
		return MakeShareable(new FJsonValueString(JsonString));
	}
	else
	{
		return MakeShareable(new FJsonValueString(InvalidUniqueNetIdStr));
	}
}

void FUniqueNetIdRepl::FromJson(const FString& Json)
{
	SetUniqueNetId(nullptr);
	if (!Json.IsEmpty())
	{
		TArray<FString> Tokens;

		int32 NumTokens = Json.ParseIntoArray(Tokens, TEXT(":"));
		if (NumTokens == 2)
		{
			UniqueIdFromString(FName(*Tokens[0]), Tokens[1]);
		}
		else if (NumTokens == 1)
		{
			UniqueIdFromString(NAME_None, Tokens[0]);
		}
	}
}

void TestUniqueIdRepl(UWorld* InWorld)
{
#if !UE_BUILD_SHIPPING

#define CHECK_REPL_EQUALITY(IdOne, IdTwo, TheBool) \
	if (!IdOne.IsValid() || !IdTwo.IsValid() || (IdOne != IdTwo) || (*IdOne != *IdTwo.GetUniqueNetId())) \
	{ \
		UE_LOG(LogNet, Warning, TEXT(#IdOne) TEXT(" input %s != ") TEXT(#IdTwo) TEXT(" output %s"), *IdOne.ToString(), *IdTwo.ToString()); \
		TheBool = false; \
	} 

#define CHECK_REPL_VALIDITY(IdOne, TheBool) \
	if (!IdOne.IsValid()) \
	{ \
		UE_LOG(LogNet, Warning, TEXT(#IdOne) TEXT(" is not valid")); \
		TheBool = false; \
	} 

	bool bSetupSuccess = true;

	TSharedPtr<const FUniqueNetId> UserId = UOnlineEngineInterface::Get()->GetUniquePlayerId(InWorld, 0);

	FUniqueNetIdRepl EmptyIdIn;
	if (EmptyIdIn.IsValid())
	{
		UE_LOG(LogNet, Warning, TEXT("EmptyId is valid: %s"), *EmptyIdIn.ToString());
		bSetupSuccess = false;
	}

	FUniqueNetIdRepl ValidIdIn(UserId);
	if (!ValidIdIn.IsValid() || UserId != ValidIdIn.GetUniqueNetId() || *UserId != *ValidIdIn)
	{
		UE_LOG(LogNet, Warning, TEXT("UserId input %s != UserId output %s"), UserId.IsValid() ? *UserId->ToString() : *InvalidUniqueNetIdStr, *ValidIdIn.ToString());
		bSetupSuccess = false;
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

	CHECK_REPL_VALIDITY(OddStringIdIn, bSetupSuccess);
	CHECK_REPL_VALIDITY(NonHexStringIdIn, bSetupSuccess);
	CHECK_REPL_VALIDITY(UpperCaseStringIdIn, bSetupSuccess);
	CHECK_REPL_VALIDITY(WayTooLongForHexEncodingIdIn, bSetupSuccess);

	static FName NAME_CustomOSS(TEXT("MyCustomOSS"));
	FUniqueNetIdRepl CustomOSSIdIn(UOnlineEngineInterface::Get()->CreateUniquePlayerId(TEXT("a8d245fc-4b97-4150-a3cd-c2c91d8fc4b3"), NAME_CustomOSS));
	FUniqueNetIdRepl CustomOSSEncodedIdIn(UOnlineEngineInterface::Get()->CreateUniquePlayerId(TEXT("0123456789abcdef"), NAME_CustomOSS));

	CHECK_REPL_VALIDITY(CustomOSSIdIn, bSetupSuccess);
	CHECK_REPL_VALIDITY(CustomOSSEncodedIdIn, bSetupSuccess);

	bool bRegularSerializationSuccess = true;
	bool bNetworkSerializationSuccess = true;
	if (bSetupSuccess)
	{
		// Regular Serialization (persistent/disk based using FString)
		{
			TArray<uint8> Buffer;
			Buffer.Empty();

			// Serialize In
			{
				FMemoryWriter TestUniqueIdWriter(Buffer, true);

				TestUniqueIdWriter << EmptyIdIn;
				TestUniqueIdWriter << ValidIdIn;
				TestUniqueIdWriter << OddStringIdIn;
				TestUniqueIdWriter << NonHexStringIdIn;
				TestUniqueIdWriter << UpperCaseStringIdIn;
				TestUniqueIdWriter << WayTooLongForHexEncodingIdIn;
				TestUniqueIdWriter << CustomOSSIdIn;
				TestUniqueIdWriter << CustomOSSEncodedIdIn;
			}

			FUniqueNetIdRepl EmptyIdOut;
			FUniqueNetIdRepl ValidIdOut;
			FUniqueNetIdRepl OddStringIdOut;
			FUniqueNetIdRepl NonHexStringIdOut;
			FUniqueNetIdRepl UpperCaseStringIdOut;
			FUniqueNetIdRepl WayTooLongForHexEncodingIdOut;
			FUniqueNetIdRepl CustomOSSIdOut;
			FUniqueNetIdRepl CustomOSSEncodedIdOut;

			// Serialize Out
			{
				FMemoryReader TestUniqueIdReader(Buffer, true);
				TestUniqueIdReader << EmptyIdOut;
				TestUniqueIdReader << ValidIdOut;
				TestUniqueIdReader << OddStringIdOut;
				TestUniqueIdReader << NonHexStringIdOut;
				TestUniqueIdReader << UpperCaseStringIdOut;
				TestUniqueIdReader << WayTooLongForHexEncodingIdOut;
				TestUniqueIdReader << CustomOSSIdOut;
				TestUniqueIdReader << CustomOSSEncodedIdOut;
			}

			if (EmptyIdOut.IsValid())
			{
				UE_LOG(LogNet, Warning, TEXT("EmptyId %s should have been invalid"), *EmptyIdOut->ToDebugString());
				bRegularSerializationSuccess = false;
			}

			if (EmptyIdIn != EmptyIdOut)
			{
				UE_LOG(LogNet, Warning, TEXT("EmptyId In/Out mismatch"));
				bRegularSerializationSuccess = false;
			}

			CHECK_REPL_EQUALITY(ValidIdIn, ValidIdOut, bRegularSerializationSuccess);
			CHECK_REPL_EQUALITY(OddStringIdIn, OddStringIdOut, bRegularSerializationSuccess);
			CHECK_REPL_EQUALITY(NonHexStringIdIn, NonHexStringIdOut, bRegularSerializationSuccess);
			CHECK_REPL_EQUALITY(UpperCaseStringIdIn, UpperCaseStringIdOut, bRegularSerializationSuccess);
			CHECK_REPL_EQUALITY(WayTooLongForHexEncodingIdIn, WayTooLongForHexEncodingIdOut, bRegularSerializationSuccess);
			CHECK_REPL_EQUALITY(CustomOSSIdIn, CustomOSSIdOut, bRegularSerializationSuccess);
			CHECK_REPL_EQUALITY(CustomOSSEncodedIdIn, CustomOSSEncodedIdOut, bRegularSerializationSuccess);
		}

		// Network serialization (network/transient using MakeReplicationData)
		{
			bool bOutSuccess = false;

			// Serialize In
			FNetBitWriter TestUniqueIdWriter(16 * 1024);
			uint8 EncodingFailures = 0;
			{
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
				CustomOSSIdIn.NetSerialize(TestUniqueIdWriter, nullptr, bOutSuccess);
				EncodingFailures += bOutSuccess ? 0 : 1;
				CustomOSSEncodedIdIn.NetSerialize(TestUniqueIdWriter, nullptr, bOutSuccess);
				EncodingFailures += bOutSuccess ? 0 : 1;
			}

			if (EncodingFailures > 0)
			{
				UE_LOG(LogNet, Warning, TEXT("There were %d encoding failures"), EncodingFailures);
				bNetworkSerializationSuccess = false;
			}

			if (bNetworkSerializationSuccess)
			{
				FUniqueNetIdRepl EmptyIdOut;
				FUniqueNetIdRepl ValidIdOut;
				FUniqueNetIdRepl OddStringIdOut;
				FUniqueNetIdRepl NonHexStringIdOut;
				FUniqueNetIdRepl UpperCaseStringIdOut;
				FUniqueNetIdRepl WayTooLongForHexEncodingIdOut;
				FUniqueNetIdRepl CustomOSSIdOut;
				FUniqueNetIdRepl CustomOSSEncodedIdOut;

				// Serialize Out
				uint8 DecodingFailures = 0;
				{
					FNetBitReader TestUniqueIdReader(nullptr, TestUniqueIdWriter.GetData(), TestUniqueIdWriter.GetNumBits());

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
					CustomOSSIdOut.NetSerialize(TestUniqueIdReader, nullptr, bOutSuccess);
					DecodingFailures += bOutSuccess ? 0 : 1;
					CustomOSSEncodedIdOut.NetSerialize(TestUniqueIdReader, nullptr, bOutSuccess);
					DecodingFailures += bOutSuccess ? 0 : 1;
				}

				if (DecodingFailures > 0)
				{
					UE_LOG(LogNet, Warning, TEXT("There were %d decoding failures"), DecodingFailures);
					bNetworkSerializationSuccess = false;
				}

				if (EmptyIdOut.IsValid())
				{
					UE_LOG(LogNet, Warning, TEXT("EmptyId %s should have been invalid"), *EmptyIdOut->ToDebugString());
					bNetworkSerializationSuccess = false;
				}

				if (EmptyIdIn != EmptyIdOut)
				{
					UE_LOG(LogNet, Warning, TEXT("EmptyId In/Out mismatch"));
					bRegularSerializationSuccess = false;
				}

				CHECK_REPL_EQUALITY(ValidIdIn, ValidIdOut, bNetworkSerializationSuccess);
				CHECK_REPL_EQUALITY(OddStringIdIn, OddStringIdOut, bNetworkSerializationSuccess);
				CHECK_REPL_EQUALITY(NonHexStringIdIn, NonHexStringIdOut, bNetworkSerializationSuccess);
				CHECK_REPL_EQUALITY(UpperCaseStringIdIn, UpperCaseStringIdOut, bNetworkSerializationSuccess);
				CHECK_REPL_EQUALITY(WayTooLongForHexEncodingIdIn, WayTooLongForHexEncodingIdOut, bNetworkSerializationSuccess);
				CHECK_REPL_EQUALITY(CustomOSSIdIn, CustomOSSIdOut, bNetworkSerializationSuccess);
				CHECK_REPL_EQUALITY(CustomOSSEncodedIdIn, CustomOSSEncodedIdOut, bNetworkSerializationSuccess);
			}
		}
	}

	bool bPlatformSerializationSuccess = true;
#if PLATFORM_XBOXONE || PLATFORM_PS4
	if (bSetupSuccess)
	{
#if PLATFORM_XBOXONE
		TSharedPtr<const FUniqueNetId> PlatformUserId = UOnlineEngineInterface::Get()->GetUniquePlayerId(InWorld, 0, FName(TEXT("LIVE")));
#elif PLATFORM_PS4
		TSharedPtr<const FUniqueNetId> PlatformUserId = UOnlineEngineInterface::Get()->GetUniquePlayerId(InWorld, 0, FName(TEXT("PS4")));
#endif

		FUniqueNetIdRepl ValidPlatformIdIn(PlatformUserId);
		if (!ValidPlatformIdIn.IsValid() || PlatformUserId != ValidPlatformIdIn.GetUniqueNetId() || *PlatformUserId != *ValidPlatformIdIn)
		{
			UE_LOG(LogNet, Warning, TEXT("PlatformUserId input %s != PlatformUserId output %s"), PlatformUserId.IsValid() ? *PlatformUserId->ToString() : *InvalidUniqueNetIdStr, *ValidPlatformIdIn.ToString());
			bPlatformSerializationSuccess = false;
		}

		if (bPlatformSerializationSuccess)
		{
			bool bOutSuccess = false;

			TArray<uint8> Buffer;
			Buffer.Empty();

			FMemoryWriter TestUniqueIdWriter(Buffer);

			// Serialize In
			uint8 EncodingFailures = 0;
			{
				ValidPlatformIdIn.NetSerialize(TestUniqueIdWriter, nullptr, bOutSuccess);
				EncodingFailures += bOutSuccess ? 0 : 1;
			}

			if (EncodingFailures > 0)
			{
				UE_LOG(LogNet, Warning, TEXT("There were %d platform encoding failures"), EncodingFailures);
				bPlatformSerializationSuccess = false;
			}

			FUniqueNetIdRepl ValidPlatformIdOut;

			// Serialize Out
			uint8 DecodingFailures = 0;
			{
				FMemoryReader TestUniqueIdReader(Buffer);

				ValidPlatformIdOut.NetSerialize(TestUniqueIdReader, nullptr, bOutSuccess);
				DecodingFailures += bOutSuccess ? 0 : 1;
			}

			if (DecodingFailures > 0)
			{
				UE_LOG(LogNet, Warning, TEXT("There were %d platform decoding failures"), DecodingFailures);
				bPlatformSerializationSuccess = false;
			}

			CHECK_REPL_EQUALITY(ValidPlatformIdIn, ValidPlatformIdOut, bPlatformSerializationSuccess);
		}
	}
#endif

	bool bJSONSerializationSuccess = true;
	if (bSetupSuccess)
	{
		// JSON Serialization
		FString OutString;
		TSharedRef<FJsonValue> JsonValue = ValidIdIn.ToJson();
		bJSONSerializationSuccess = JsonValue->TryGetString(OutString);
		if (bJSONSerializationSuccess)
		{
			FUniqueNetIdRepl NewIdOut;
			NewIdOut.FromJson(OutString);
			bJSONSerializationSuccess = NewIdOut.IsValid() && (ValidIdIn == NewIdOut);
		}
	}

	UE_LOG(LogNet, Log, TEXT("TestUniqueIdRepl tests:"));
	UE_LOG(LogNet, Log, TEXT("	Setup: %s"), bSetupSuccess ? TEXT("PASS") : TEXT("FAIL"));
	UE_LOG(LogNet, Log, TEXT("	Normal: %s"), bRegularSerializationSuccess ? (bSetupSuccess ? TEXT("PASS") : TEXT("SKIPPED")) : TEXT("FAIL"));
	UE_LOG(LogNet, Log, TEXT("	Network: %s"), bNetworkSerializationSuccess ? (bSetupSuccess ? TEXT("PASS") : TEXT("SKIPPED")) : TEXT("FAIL"));
	UE_LOG(LogNet, Log, TEXT("	Platform: %s"), bPlatformSerializationSuccess ? (bSetupSuccess ? TEXT("PASS") : TEXT("SKIPPED")) : TEXT("FAIL"));
	UE_LOG(LogNet, Log, TEXT("	JSON: %s"), bJSONSerializationSuccess ? (bSetupSuccess ? TEXT("PASS") : TEXT("SKIPPED")) : TEXT("FAIL"));

#undef CHECK_REPL_VALIDITY
#undef CHECK_REPL_EQUALITY

#endif
}
