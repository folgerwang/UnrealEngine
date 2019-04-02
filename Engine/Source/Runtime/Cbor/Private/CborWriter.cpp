// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CborWriter.h"

FCborWriter::FCborWriter(FArchive* InStream)
	: Stream(InStream)
{
	check(Stream != nullptr && Stream->IsSaving());
	ContextStack.Emplace();
}

FCborWriter::~FCborWriter()
{
	check(ContextStack.Num() == 1 && ContextStack.Top().RawCode() == ECborCode::Dummy);
}

const FArchive* FCborWriter::GetArchive() const
{
	return Stream;
}

void FCborWriter::WriteContainerStart(ECborCode ContainerType, int64 NbItem)
{
	check(ContainerType == ECborCode::Array || ContainerType == ECborCode::Map);
	CheckContext(ContainerType);

	FCborHeader Header;

	// if NbItem is negative consider the map indefinite
	if (NbItem < 0)
	{
		Header.Set(ContainerType | ECborCode::Indefinite);
		*Stream << Header;
	}
	else
	{
		Header = WriteUIntValue(ContainerType, *Stream, (uint64) NbItem);
	}
	FCborContext Context;
	Context.Header = Header;
	// Length in context for indefinite container is marked as 0 and count up.
	// Map length in context are marked as twice their number of pairs in finite container and counted down.
	// @see CheckContext
	Context.Length = NbItem < 0 ? 0 : (ContainerType == ECborCode::Map ? NbItem * 2 : NbItem);
	ContextStack.Add(MoveTemp(Context));
}

void FCborWriter::WriteContainerEnd()
{
	check(ContextStack.Top().IsIndefiniteContainer());
	FCborHeader Header(ECborCode::Break);
	*Stream << Header;
	ContextStack.Pop();
}

void FCborWriter::WriteNull()
{
	CheckContext(ECborCode::Prim);
	FCborHeader Header(ECborCode::Prim | ECborCode::Null);
	*Stream << Header;
}

void FCborWriter::WriteValue(uint64 Value)
{
	CheckContext(ECborCode::Uint);
	WriteUIntValue(ECborCode::Uint, *Stream, Value);
}

void FCborWriter::WriteValue(int64 Value)
{
	if (Value < 0)
	{
		CheckContext(ECborCode::Int);
		WriteUIntValue(ECborCode::Int, *Stream, ~Value);
	}
	else
	{
		CheckContext(ECborCode::Uint);
		WriteUIntValue(ECborCode::Uint, *Stream, Value);
	}
}

void FCborWriter::WriteValue(bool Value)
{
	CheckContext(ECborCode::Prim);
	FCborHeader Header(ECborCode::Prim | (Value ? ECborCode::True : ECborCode::False));
	*Stream << Header;
}

void FCborWriter::WriteValue(float Value)
{
	CheckContext(ECborCode::Prim);
	FCborHeader Header(ECborCode::Prim | ECborCode::Value_4Bytes);
	*Stream << Header;
	*Stream << Value;
}

void FCborWriter::WriteValue(double Value)
{
	CheckContext(ECborCode::Prim);
	FCborHeader Header(ECborCode::Prim | ECborCode::Value_8Bytes);
	*Stream << Header;
	*Stream << Value;
}

void FCborWriter::WriteValue(const FString& Value)
{
	CheckContext(ECborCode::TextString);
	FTCHARToUTF8 UTF8String(*Value);
	// Write string header
	WriteUIntValue(ECborCode::TextString, *Stream, (uint64)UTF8String.Length());
	// Write string
	check(sizeof(decltype(*UTF8String.Get())) == 1);
	Stream->Serialize(const_cast<char*>(UTF8String.Get()), UTF8String.Length());
}

void FCborWriter::WriteValue(const char* CString, uint64 Length)
{
	CheckContext(ECborCode::ByteString);
	// Write c string header
	WriteUIntValue(ECborCode::ByteString, *Stream, Length);
	Stream->Serialize(const_cast<char*>(CString), Length);
}

FCborHeader FCborWriter::WriteUIntValue(FCborHeader Header, FArchive& Ar, uint64 Value)
{
	if (Value < 24)
	{
		Header.Set(Header.MajorType() | (ECborCode)Value);
		Ar << Header;
	}
	else if (Value < 256)
	{
		Header.Set(Header.MajorType() | ECborCode::Value_1Byte);
		Ar << Header;
		uint8 Temp = Value;
		Ar << Temp;
	}
	else if (Value < 65536)
	{
		Header.Set((uint8)(Header.MajorType() | ECborCode::Value_2Bytes));
		Ar << Header;
		uint16 Temp = Value;
		Ar << Temp;
	}
	else if (Value < 0x100000000L)
	{
		Header.Set((uint8)(Header.MajorType() | ECborCode::Value_4Bytes));
		Ar << Header;
		uint32 Temp = Value;
		Ar << Temp;
	}
	else
	{
		Header.Set((uint8)(Header.MajorType() | ECborCode::Value_8Bytes));
		Ar << Header;
		uint64 Temp = Value;
		Ar << Temp;
	}
	return Header;
}

void FCborWriter::CheckContext(ECborCode MajorType)
{
	FCborContext& Context = ContextStack.Top();
	if (Context.IsIndefiniteContainer())
	{
		++Context.Length;
		check(!Context.IsString() || MajorType != Context.MajorType());
	}
	else if (Context.IsFiniteContainer())
	{
		if (--Context.Length == 0)
		{
			ContextStack.Pop();
		}
	}
}
