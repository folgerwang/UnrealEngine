// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BackChannel/Protocol/OSC/BackChannelOSCPacket.h"

/**
 *	Representation of an OSC message. Data can be read/written using the explicit
 *	Read/Write functions, or the Serialize function / << operator where the behaviour
 * is overloaded based on whether the message was created for reading or writing.
 *
 *	Any failed Reads() will result in the default value of the type (e.g. 0, 0.0, false, "")
 *	being returned.
 */
class BACKCHANNEL_API FBackChannelOSCMessage : public FBackChannelOSCPacket
{
public:

	FBackChannelOSCMessage(OSCPacketMode InMode);

	FBackChannelOSCMessage(const TCHAR* Address);

	virtual ~FBackChannelOSCMessage();

	/* Move constructor */
	FBackChannelOSCMessage(FBackChannelOSCMessage&& RHS);

	/* Move operator */
	FBackChannelOSCMessage& operator=(FBackChannelOSCMessage&& RHS);

	/* Return our type */
	virtual OSCPacketType GetType() const override { return OSCPacketType::Message; }

	/* Return our size (plus any necessary padding) */
	virtual int32 GetSize() const override;

	/* Returns a buffer with the contents of this message. Data is padded per OSC requirements */
	virtual TArray<uint8> WriteToBuffer() const override;
	
	/* Writes this message into the provided buffer at an offset of Buffer.Num() */
	virtual void WriteToBuffer(TArray<uint8>& Buffer) const override;

	/* Helper to check our read/write status */
	bool IsWriting() const { return Mode == OSCPacketMode::Write; }

	/* Helper to check our read/write status */
	bool IsReading() const { return Mode == OSCPacketMode::Read; }

	/* Returns the address of this packet */
	const FString& GetAddress() const
	{
		return Address;
	}

	/* Return our argument tags */
	const FString& GetTags() const
	{
		return TagString;
	}
	
	/* Returns the number of arguments in this message */
	int32 GetArgumentCount() const
	{
		return TagString.Len();
	}
	
	/* Returns the type of our next argument */
	TCHAR GetNextArgumentType() const
	{
		return TagString[TagIndex];
	}

	/* Return the size (plus padding) of all our arguments) */
	const int32 GetArgumentSize() const
	{
		return Buffer.Num();
	}

	/* Set our destination address */
	void	SetAddress(const TCHAR* Address);

	/* Reset us for reading. The next argument read will be our first argument */
	void	ResetRead();

	//! Int32 read/write

	/* Write an int32 into our arguments */
	void Write(const int32 Value)
	{
		check(IsWriting());
		SerializeWrite(TEXT('i'), &Value, sizeof(Value));
	}

	/* Read an int32 from our arguments */
	void Read(int32& Value)
	{
		check(IsReading());
		SerializeRead(TEXT('i'), &Value, sizeof(Value));
	}

	//! Float read/write

	/* Write a float to our arguments */
	void Write(const float Value)
	{
		check(IsWriting());
		SerializeWrite(TEXT('f'), &Value, sizeof(Value));
	}

	/* Read a foat from our arguments */
	void Read(float& OutValue)
	{
		check(IsReading());
		SerializeRead(TEXT('f'), &OutValue, sizeof(OutValue));
	}

	//! String read/write (multiple forms of write for TCHAR*'s

	/* Write a string to our arguments */
	void Write(const FString& Value)
	{
		Write(*Value);
	}

	/* Write a string to our arguments */
	void Write(const TCHAR* Value)
	{
		SerializeWrite(TEXT('s'), TCHAR_TO_ANSI(Value), FCString::Strlen(Value) + 1);
	}

	/* Read a string from our arguments.  */
	void Read(FString& OutValue);

	//! Raw data blobs

	/* Write a blob of data to our arguments */
	void Write(const void* InBlob, int32 BlobSize)
	{
		check(IsWriting());
		SerializeWrite(TEXT('b'), InBlob, BlobSize);
	}

	/* Read a blob of data from our arguments */
	void Read(void* InBlob, int32 BlobSize)
	{
		check(IsReading());
		SerializeRead(TEXT('b'), InBlob, BlobSize);
	}

	/*
	*	Write a TArray of type T to our arguments. This is a helper that write an int
	*	for the size, then a blob of sizeof(t) * NumItems
	*/
	template<typename T>
	void Write(const TArray<T>& Value)
	{
		Write(Value.Num());
		Write(Value.GetData(), Value.Num() * sizeof(T));
	}

	/*
	 *	Read a TArray of type T from our arguments. This is a helper that reads an int
	*	for the size, then allocated and reads a blob of sizeof(t) * NumItems
	 */
	template<typename T>
	void Read(TArray<T>& Value)
	{
		int32 ArraySize(0);
		Read(ArraySize);

		Value.Empty();
		Value.AddUninitialized(ArraySize);
		Read(Value.GetData(), Value.Num() * sizeof(T));
	}
	

	/* Serialize helper that will read/write based on the open mode of this message */
	template<typename T>
	void Serialize(T& Value)
	{
		if (IsWriting())
		{
			Write(Value);
		}
		else
		{
			Read(Value);
		}
	}

	/* Serialize helper that will read/write based on the open mode of this message */
	void Serialize(void* InBlob, int32 BlobSize)
	{
		if (IsReading())
		{
			Read(InBlob, BlobSize);
		}
		else
		{
			Write(InBlob, BlobSize);
		}
	}

	static int32 RoundedArgumentSize(int32 ArgSize)
	{
		return ((ArgSize + 3) / 4) * 4;
	}

	static TSharedPtr<FBackChannelOSCMessage> CreateFromBuffer(const void* Data, int32 DataLength);

protected:

	void Serialize(const TCHAR Code, void* InData, int32 InSize);

	void SerializeRead(const TCHAR Code, void* InData, int32 InSize);
	void SerializeWrite(const TCHAR Code, const void* InData, int32 InSize);

protected:

	OSCPacketMode		Mode;
	FString				Address;
	FString				TagString;
	int					TagIndex;
	int					BufferIndex;
	TArray<uint8>		Buffer;
};

BACKCHANNEL_API FBackChannelOSCMessage& operator << (FBackChannelOSCMessage& Msg, int32& Value);

BACKCHANNEL_API FBackChannelOSCMessage& operator << (FBackChannelOSCMessage& Msg, float& Value);

BACKCHANNEL_API FBackChannelOSCMessage& operator << (FBackChannelOSCMessage& Msg, bool& Value);

BACKCHANNEL_API FBackChannelOSCMessage& operator << (FBackChannelOSCMessage& Msg, TCHAR& Value);

BACKCHANNEL_API FBackChannelOSCMessage& operator << (FBackChannelOSCMessage& Msg, FString& Value);

template <typename T>
FBackChannelOSCMessage& operator << (FBackChannelOSCMessage& Msg, TArray<T>& Value)
{
	if (Msg.IsWriting())
	{
		Msg.Write(Value);
	}
	else
	{
		Msg.Read(Value);
	}

	return Msg;
}

template <typename T>
FBackChannelOSCMessage& SerializeOut(FBackChannelOSCMessage& Msg, const T& Value)
{
	T Tmp = Value;
	Msg << Tmp;
	return Msg;
}
