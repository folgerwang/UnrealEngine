// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/StructuredArchiveFormatter.h"

#if WITH_TEXT_ARCHIVE_SUPPORT

class FTaggedBinaryArchiveOutputFormatter : public FStructuredArchiveFormatter
{
public:
	FTaggedBinaryArchiveOutputFormatter(FArchive& InInner, TFunction<void(FArchive&, UObject*&)> InSerializeObject);
	virtual ~FTaggedBinaryArchiveOutputFormatter();

	virtual FArchive& GetUnderlyingArchive() override;

	virtual bool HasDocumentTree() const;

	virtual void EnterRecord() override;
	virtual void EnterRecord_TextOnly(TArray<FString>& OutFieldNames) override;
	virtual void LeaveRecord() override;
	virtual void EnterField(FArchiveFieldName Name) override;
	virtual void EnterField_TextOnly(FArchiveFieldName Name, EArchiveValueType& OutType) override;
	virtual void LeaveField() override;
	virtual bool TryEnterField(FArchiveFieldName Name, bool bEnterWhenSaving) override;

	virtual void EnterArray(int32& NumElements) override;
	virtual void LeaveArray() override;
	virtual void EnterArrayElement() override;
	virtual void EnterArrayElement_TextOnly(EArchiveValueType& OutType) override;
	virtual void LeaveArrayElement() override;

	virtual void EnterStream() override;
	virtual void EnterStream_TextOnly(int32& NumElements) override;
	virtual void LeaveStream() override;
	virtual void EnterStreamElement() override;
	virtual void EnterStreamElement_TextOnly(EArchiveValueType& OutType) override;
	virtual void LeaveStreamElement() override;

	virtual void EnterMap(int32& NumElements) override;
	virtual void LeaveMap() override;
	virtual void EnterMapElement(FString& Name) override;
	virtual void EnterMapElement_TextOnly(FString& Name, EArchiveValueType& OutType) override;
	virtual void LeaveMapElement() override;

	virtual void Serialize(uint8& Value) override;
	virtual void Serialize(uint16& Value) override;
	virtual void Serialize(uint32& Value) override;
	virtual void Serialize(uint64& Value) override;
	virtual void Serialize(int8& Value) override;
	virtual void Serialize(int16& Value) override;
	virtual void Serialize(int32& Value) override;
	virtual void Serialize(int64& Value) override;
	virtual void Serialize(float& Value) override;
	virtual void Serialize(double& Value) override;
	virtual void Serialize(bool& Value) override;
	virtual void Serialize(FString& Value) override;
	virtual void Serialize(FName& Value) override;
	virtual void Serialize(UObject*& Value) override;
	virtual void Serialize(FText& Value) override;
	virtual void Serialize(FWeakObjectPtr& Value) override;
	virtual void Serialize(FSoftObjectPtr& Value) override;
	virtual void Serialize(FSoftObjectPath& Value) override;
	virtual void Serialize(FLazyObjectPtr& Value) override;
	virtual void Serialize(TArray<uint8>& Value) override;
	virtual void Serialize(void* Data, uint64 DataSize) override;

private:
	struct FField
	{
		int32 NameIdx;
		int64 Offset;
		int64 Size;
	};

	struct FRecord
	{
		TArray<FField> Fields;
		int64 StartOffset;
		int64 EndOffset;
	};

	struct FStream
	{
		int64 StartOffset;
		int32 NumItems;
	};

	FArchive& Inner;
	TFunction<void(FArchive&, UObject*&)> SerializeObject;

	TArray<FString> Names;
	TMap<FString, int32> NameToIndex;

	int64 StartOffset;
	int32 NextRecordIdx;
	TArray<FRecord> Records;
	TArray<int32> RecordStack;
	TArray<FStream> Streams;

	void WriteSize(uint64 Size);
	void WriteType(EArchiveValueType Type);

	int32 FindOrAddName(FString Name);
};

#endif
