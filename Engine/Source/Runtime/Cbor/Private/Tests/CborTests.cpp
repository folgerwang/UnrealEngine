// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#include "CborReader.h"
#include "CborWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * FCborAutomationTest
 * Simple unit test that runs Cbor's in-built test cases
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCborAutomationTest, "System.Core.Serialization.CBOR", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter )


/** 
 * Execute the Cbor test cases
 *
 * @return	true if the test was successful, false otherwise
 */
bool FCborAutomationTest::RunTest(const FString& Parameters)
{
	// Create the Writer
	TArray<uint8> Bytes;
	TUniquePtr<FArchive> OutputStream = MakeUnique<FMemoryWriter>(Bytes);
	FCborWriter Writer(OutputStream.Get());

	// Create the Reader
	TUniquePtr<FArchive> InputStream = MakeUnique<FMemoryReader>(Bytes);
	FCborReader Reader(InputStream.Get());

	int64 TestInt = 0;
	FCborContext Context;

	// Positive Integer Item
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Uint);
	check(Context.AsUInt() == TestInt);
	check(Context.AsInt() == TestInt);
	
	TestInt = 1;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Uint);
	check(Context.AsUInt() == TestInt);
	check(Context.AsInt() == TestInt);

	TestInt = 10;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Uint);
	check(Context.AsUInt() == TestInt);
	check(Context.AsInt() == TestInt);

	TestInt = 23;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Uint);	
	check(Context.AsUInt() == TestInt);
	check(Context.AsInt() == TestInt);

	TestInt = 24;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Uint);
	check(Context.AdditionalValue() == ECborCode::Value_1Byte);
	check(Context.AsUInt() == TestInt);
	check(Context.AsInt() == TestInt);

	TestInt = 1000;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Uint);
	check(Context.AdditionalValue() == ECborCode::Value_2Bytes);

	check(Context.AsUInt() == TestInt);
	check(Context.AsInt() == TestInt);

	TestInt = 3000000000;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Uint);
	check(Context.AdditionalValue() == ECborCode::Value_4Bytes);

	check(Context.AsUInt() == TestInt);
	check(Context.AsInt() == TestInt);

	TestInt = 9223372036854775807;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Uint);
	check(Context.AdditionalValue() == ECborCode::Value_8Bytes);
	check(Context.AsUInt() == TestInt);
	check(Context.AsInt() == TestInt);

	// Negative numbers

	TestInt = -1;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Int);
	check(Context.AsInt() == TestInt);

	TestInt = -23;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Int);
	check(Context.AsInt() == TestInt);

	TestInt = -25;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Int);
	check(Context.AdditionalValue() == ECborCode::Value_1Byte);
	check(Context.AsInt() == TestInt);

	TestInt = -1000;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Int);
	check(Context.AdditionalValue() == ECborCode::Value_2Bytes);
	check(Context.AsInt() == TestInt);

	TestInt = -3000000000LL;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Int);
	check(Context.AdditionalValue() == ECborCode::Value_4Bytes);
	check(Context.AsInt() == TestInt);

	TestInt = -92233720368547758LL; //-9223372036854775807LL;
	Writer.WriteValue(TestInt);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Int);
	check(Context.AdditionalValue() == ECborCode::Value_8Bytes);
	check(Context.AsInt() == TestInt);

	// Bool

	bool TestBool = false;
	Writer.WriteValue(TestBool);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Prim);
	check(Context.AdditionalValue() == ECborCode::False);
	check(Context.AsBool() == TestBool);

	TestBool = true;
	Writer.WriteValue(TestBool);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Prim);
	check(Context.AdditionalValue() == ECborCode::True);
	check(Context.AsBool() == TestBool);

	// Float

	float TestFloat = 3.14159265f;
	Writer.WriteValue(TestFloat);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Prim);
	check(Context.AdditionalValue() == ECborCode::Value_4Bytes);
	check(Context.AsFloat() == TestFloat);


	// Double

	double TestDouble = 3.14159265; // 3.4028234663852886e+38;
	Writer.WriteValue(TestDouble);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Prim);
	check(Context.AdditionalValue() == ECborCode::Value_8Bytes);
	check(Context.AsDouble() == TestDouble);


	// String

	FString TestString(TEXT("ANSIString"));

	Writer.WriteValue(TestString);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::TextString);
	check(Context.AsString() == TestString);

	TestString = TEXT("\u3042\u308A\u304C\u3068\u3046");
	Writer.WriteValue(TestString);
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::TextString);
	check(Context.AsString() == TestString);

	// C String
	char TestCString[] = "Potato";

	Writer.WriteValue(TestCString, (sizeof(TestCString) / sizeof(char)) - 1); // do not count the null terminating character
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::ByteString);
	check(TCString<char>::Strcmp(Context.AsCString(), TestCString) == 0);

	// Array
	TArray<int64> IntArray { 0, 1, -1, 10, -1000, -3000000000LL, 240, -24 };
	Writer.WriteContainerStart(ECborCode::Array, IntArray.Num());
	for (int64 Val : IntArray)
	{
		Writer.WriteValue(Val);
	}
	// Array start & length
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Array);
	check(Context.AsLength() == IntArray.Num());

	for (int64 Val : IntArray)
	{
		check(Reader.ReadNext(Context) == true);
		check(Context.AsInt() == Val);
	}

	// Read array end, report length 0 on finite container
	// although the array wasn't written as indefinite,
	// the reader will emit a virtual break token to notify the container end
	check(Reader.ReadNext(Context) == true);
	check(Context.IsBreak());
	check(Context.AsLength() == 0);

	// Indefinite Array
	Writer.WriteContainerStart(ECborCode::Array, -1);
	for (int64 Val : IntArray)
	{
		Writer.WriteValue(Val);
	}
	Writer.WriteContainerEnd();

	// Array start & length
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Array);
	check(Context.IsIndefiniteContainer());
	check(Context.AsLength() == 0);

	for (int64 Val : IntArray)
	{
		check(Reader.ReadNext(Context) == true);
		check(Context.AsInt() == Val);
	}

	// Read array end, report length 
	// although the array wasn't written as indefinite,
	// the reader will emit a virtual break token to notify the container end
	check(Reader.ReadNext(Context) == true);
	check(Context.IsBreak());
	check(Context.AsLength() == IntArray.Num());

	// Map
	TMap<FString, FString> StringMap = { {TEXT("Apple"), TEXT("Orange")}, {TEXT("Potato"), TEXT("Tomato")}, {TEXT("Meat"), TEXT("Treat")} };
	Writer.WriteContainerStart(ECborCode::Map, StringMap.Num());

	for (const auto& Pair : StringMap)
	{
		Writer.WriteValue(Pair.Key);
		Writer.WriteValue(Pair.Value);
	}

	// Map start & length
	check(Reader.ReadNext(Context) == true);
	check(Context.MajorType() == ECborCode::Map);
	check(Context.AsLength() == StringMap.Num() * 2);

	for (const auto& Pair : StringMap)
	{
		check(Reader.ReadNext(Context) == true);
		check(Context.AsString() == Pair.Key);
		check(Reader.ReadNext(Context) == true);
		check(Context.AsString() == Pair.Value);
	}

	// Read map end 
	// although the array wasn't written as indefinite,
	// the reader will emit a virtual break token to notify the container end
	check(Reader.ReadNext(Context) == true);
	check(Context.IsBreak());

	check(Reader.ReadNext(Context) == false);
	check(Context.RawCode() == ECborCode::StreamEnd);
	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
