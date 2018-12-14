// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BackChannel/Protocol/OSC/BackChannelOSCMessage.h"
#include "BackChannel/Private/BackChannelCommon.h"


FBackChannelOSCMessage::FBackChannelOSCMessage(OSCPacketMode InMode)
{
	Mode = InMode;
	TagIndex = 0;
	BufferIndex = 0;
}


FBackChannelOSCMessage::FBackChannelOSCMessage(const TCHAR* InAddress)
	: FBackChannelOSCMessage(OSCPacketMode::Write)
{
	SetAddress(InAddress);
}

FBackChannelOSCMessage::~FBackChannelOSCMessage()
{
}


FBackChannelOSCMessage::FBackChannelOSCMessage(FBackChannelOSCMessage&& RHS)
{
	*this = MoveTemp(RHS);
}

FBackChannelOSCMessage& FBackChannelOSCMessage::operator=(FBackChannelOSCMessage&& RHS)
{
	Mode = RHS.Mode;
	Address = MoveTemp(RHS.Address);
	TagString = MoveTemp(RHS.TagString);
	TagIndex = RHS.TagIndex;
	Buffer = MoveTemp(RHS.Buffer);
	BufferIndex = RHS.BufferIndex;

	return *this;
}


void FBackChannelOSCMessage::SetAddress(const TCHAR* InAddress)
{
	Address = InAddress;
}

void FBackChannelOSCMessage::ResetRead()
{
	check(IsReading());
	TagIndex = 0;
	BufferIndex = 0;
}

void FBackChannelOSCMessage::Read(FString& Value)
{
	check(IsReading());
	/*if (Mode == OSCPacketMode::Write)
	{
		// string should be null-terminated so allow space for that then
		// terminate
		int32 StartingIndex = BufferIndex;
		int32 StringLen = Value.Len();

		SerializeWrite(TEXT('s'), TCHAR_TO_ANSI(*Value), StringLen + 1);
		Buffer[StartingIndex + StringLen] = 0;
	}
	else*/
	{
		TCHAR CurrentTag = TagString[TagIndex];

		if (CurrentTag != 's')
		{
			UE_LOG(LogBackChannel, Error, TEXT("OSCMessage: Requested tag 's' but next tag was %c"), CurrentTag);
			return;
		}

		// get a pointer to the string (which is null-terminated) and read into the value
		ANSICHAR* pString = (ANSICHAR*)(Buffer.GetData() + BufferIndex);
		Value = ANSI_TO_TCHAR(pString);

		// manually adjust our buffer as if we read the string
		BufferIndex += RoundedArgumentSize(Value.Len() + 1);

		TagIndex++;
	}
}

void FBackChannelOSCMessage::Serialize(const TCHAR Code, void* InData, int32 InSize)
{
	if (Mode == OSCPacketMode::Read)
	{
		SerializeRead(Code, InData, InSize);
	}
	else
	{
		SerializeWrite(Code, InData, InSize);
	}
}

void FBackChannelOSCMessage::SerializeWrite(const TCHAR Code, const void* InData, int32 InSize)
{
	TagString += Code;

	// in OSC every write must be a multiple of 32-bits
	const int32 RoundedSize = RoundedArgumentSize(InSize);

	Buffer.AddUninitialized(RoundedSize);
	FMemory::Memcpy(Buffer.GetData() + BufferIndex, InData, InSize);

	BufferIndex += RoundedSize;
	TagIndex++;
}

void FBackChannelOSCMessage::SerializeRead(const TCHAR Code, void* InData, int32 InSize)
{
	if (TagIndex == TagString.Len())
	{
		UE_LOG(LogBackChannel, Error, TEXT("OSCMessage: Cannot read tag %c, no more tags!"), Code);
		return;
	}

	TCHAR CurrentTag = TagString[TagIndex];

	if (CurrentTag != Code)
	{
		UE_LOG(LogBackChannel, Error, TEXT("OSCMessage: Requested tag %c but next tag was %c"), Code, CurrentTag);
		return;
	}

	FMemory::Memcpy(InData, Buffer.GetData() + BufferIndex, InSize);

	// in OSC every write must be a multiple of 32-bits
	BufferIndex += RoundedArgumentSize(InSize);
	TagIndex++;
}

int32 FBackChannelOSCMessage::GetSize() const 
{
	const int32 kAddressLength = RoundedArgumentSize(GetAddress().Len()+1);

	const FString FinalTagString = FString::Printf(TEXT(",%s"), *GetTags());

	const int32 kTagLength = RoundedArgumentSize(FinalTagString.Len()+1);		// we don't store the , internally

	const int32 kArgumentSize = BufferIndex;

	return kAddressLength + kTagLength + kArgumentSize;
}


TArray<uint8> FBackChannelOSCMessage::WriteToBuffer() const
{
	TArray<uint8> OutBuffer;
	WriteToBuffer(OutBuffer);
	return OutBuffer;
}

void FBackChannelOSCMessage::WriteToBuffer(TArray<uint8>& OutBuffer) const
{
	const int kRequiredSize = GetSize();

	OutBuffer.AddUninitialized(kRequiredSize);

	ANSICHAR* pOutBuffer = (ANSICHAR*)OutBuffer.GetData();

	const int32 kAddressLength = RoundedArgumentSize(GetAddress().Len()+1);
	const FString FinalTagString = FString::Printf(TEXT(",%s"), *GetTags());
	const int32 kTagLength = RoundedArgumentSize(FinalTagString.Len() + 1);

	FCStringAnsi::Strncpy(pOutBuffer, TCHAR_TO_ANSI(*GetAddress()), kAddressLength);
	pOutBuffer[kAddressLength] = 0;
	pOutBuffer += RoundedArgumentSize(kAddressLength);

	FCStringAnsi::Strcpy(pOutBuffer, kRequiredSize, TCHAR_TO_ANSI(*FinalTagString));
	pOutBuffer[kTagLength] = 0;
	pOutBuffer += RoundedArgumentSize(kTagLength);

	FMemory::Memcpy(pOutBuffer, Buffer.GetData(), BufferIndex);
}

TSharedPtr<FBackChannelOSCMessage> FBackChannelOSCMessage::CreateFromBuffer(const void * Data, int32 DataLength)
{
	const uint8* pParsedData = (const uint8*)Data;

	TSharedPtr<FBackChannelOSCMessage> NewMessage = MakeShareable(new FBackChannelOSCMessage(OSCPacketMode::Read));

	// first argument is the address as a null-terminated char*
	NewMessage->Address = ANSI_TO_TCHAR((ANSICHAR*)pParsedData);
	pParsedData += RoundedArgumentSize(NewMessage->Address.Len() + 1);

	// second argument is the address as a null-terminated char*
	FString TagString = ANSI_TO_TCHAR((ANSICHAR*)pParsedData);

	// remove the leading ,
	NewMessage->TagString = TagString.RightChop(TagString.Find(TEXT(","))+1);

	pParsedData += RoundedArgumentSize(TagString.Len() + 1);

	// now pData points at the arguments.
	const int kArgLength = DataLength - (pParsedData - (const uint8*)Data);
	
	NewMessage->Buffer.AddUninitialized(kArgLength);
	FMemory::Memcpy(NewMessage->Buffer.GetData(), pParsedData, kArgLength);

	return NewMessage;
}


FBackChannelOSCMessage& operator << (FBackChannelOSCMessage& Msg, int32& Value)
{
	Msg.Serialize(Value);
	return Msg;
}

FBackChannelOSCMessage& operator << (FBackChannelOSCMessage& Msg, float& Value)
{
	Msg.Serialize(Value);
	return Msg;
}

FBackChannelOSCMessage& operator << (FBackChannelOSCMessage& Msg, bool& Value)
{
	if (Msg.IsWriting())
	{
		int32 IntValue = Value ? 1 : 0;
		Msg.Serialize(IntValue);
	}
	else
	{
		int32 IntValue(0);
		Msg.Serialize(IntValue);
		Value = IntValue == 0 ? false : true;
	}
	return Msg;
}

FBackChannelOSCMessage& operator << (FBackChannelOSCMessage& Msg, TCHAR& Value)
{
	if (Msg.IsWriting())
	{
		int32 IntValue = Value;
		Msg.Serialize(IntValue);
	}
	else
	{
		int32 IntValue(0);
		Msg.Serialize(IntValue);
		Value = (TCHAR)IntValue;
	}
	return Msg;
}

FBackChannelOSCMessage& operator << (FBackChannelOSCMessage& Msg, FString& Value)
{
	Msg.Serialize(Value);
	return Msg;
}

