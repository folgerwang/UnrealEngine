// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Serialization/Formatters/BinaryArchiveFormatter.h"
#include "Serialization/Archive.h"

FBinaryArchiveFormatter::FBinaryArchiveFormatter(FArchive& InInner) 
	: Inner(InInner)
{
}

FBinaryArchiveFormatter::~FBinaryArchiveFormatter()
{
}

bool FBinaryArchiveFormatter::HasDocumentTree() const
{
	return false;
}

void FBinaryArchiveFormatter::EnterRecord_TextOnly(TArray<FString>& OutFieldNames)
{
	checkf(false, TEXT("FBinaryArchiveFormatter does not support functions with the _TextOnly() suffix."));
}

void FBinaryArchiveFormatter::EnterField_TextOnly(FArchiveFieldName Name, EArchiveValueType& OutType)
{
	checkf(false, TEXT("FBinaryArchiveFormatter does not support functions with the _TextOnly() suffix."));
}

void FBinaryArchiveFormatter::EnterArrayElement_TextOnly(EArchiveValueType& OutType)
{
	checkf(false, TEXT("FBinaryArchiveFormatter does not support functions with the _TextOnly() suffix."));
}

void FBinaryArchiveFormatter::EnterStream_TextOnly(int32& NumElements)
{
	checkf(false, TEXT("FBinaryArchiveFormatter does not support functions with the _TextOnly() suffix."));
}

void FBinaryArchiveFormatter::EnterStreamElement_TextOnly(EArchiveValueType& OutType)
{
	checkf(false, TEXT("FBinaryArchiveFormatter does not support functions with the _TextOnly() suffix."));
}

void FBinaryArchiveFormatter::EnterMapElement_TextOnly(FString& Name, EArchiveValueType& OutType)
{
	checkf(false, TEXT("FBinaryArchiveFormatter does not support functions with the _TextOnly() suffix."));
}
