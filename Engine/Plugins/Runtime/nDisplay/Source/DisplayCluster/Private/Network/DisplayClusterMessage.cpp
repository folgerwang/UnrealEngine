// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterMessage.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterMessage::FDisplayClusterMessage()
{
}

FDisplayClusterMessage::FDisplayClusterMessage(const FString& name, const FString& type, const FString& protocol) :
	Name(name),
	Type(type),
	Protocol(protocol)
{
}

FDisplayClusterMessage::~FDisplayClusterMessage()
{
}


bool FDisplayClusterMessage::Serialize(FMemoryWriter& ar)
{
	// Header
	ar << Name;
	ar << Type;
	ar << Protocol;

	TArray<FString> keys;
	Arguments.GenerateKeyArray(keys);

	// Arguments amount
	FString strArgAmount = FString::FromInt(Arguments.Num());
	ar << strArgAmount;

	// Arguments
	for (int i = 0; i < keys.Num(); ++i)
	{
		ar << keys[i];
		ar << Arguments[keys[i]];
	}

	return true;
}

bool FDisplayClusterMessage::Deserialize(FMemoryReader& ar)
{
	// Header
	ar << Name;
	ar << Type;
	ar << Protocol;

	// Arguments amount
	FString strArgsAmount;
	ar << strArgsAmount;
	const int32 amount = FCString::Atoi(*strArgsAmount);
	check(amount >= 0);
	
	// Arguments
	for (int32 i = 0; i < amount; ++i)
	{
		FString key;
		FString val;

		ar << key;
		ar << val;

		Arguments.Add(key, val);
	}

	UE_LOG(LogDisplayClusterNetworkMsg, VeryVerbose, TEXT("Deserialized message: %s"), *ToString());

	return true;
}

FString FDisplayClusterMessage::ToString() const
{
	return FString::Printf(TEXT("<prot=%s type=%s name=%s args={%s}>"), *GetProtocol(), *GetType(), *GetName(), *ArgsToString());
}

FString FDisplayClusterMessage::ArgsToString() const
{
	FString str;
	str.Reserve(512);
	
	for (auto it = Arguments.CreateConstIterator(); it; ++it)
	{
		str += FString::Printf(TEXT("%s=%s "), *it->Key, *it->Value);
	}

	return str;
}
