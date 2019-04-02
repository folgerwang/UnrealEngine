// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/DisplayClusterMessage.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterMessage::FDisplayClusterMessage()
{
}

FDisplayClusterMessage::FDisplayClusterMessage(const FString& InName, const FString& InType, const FString& InProtocol) :
	Name(InName),
	Type(InType),
	Protocol(InProtocol)
{
}

FDisplayClusterMessage::~FDisplayClusterMessage()
{
}


bool FDisplayClusterMessage::Serialize(FMemoryWriter& Arch)
{
	// Header
	Arch << Name;
	Arch << Type;
	Arch << Protocol;

	TArray<FString> keys;
	Arguments.GenerateKeyArray(keys);

	// Arguments amount
	FString strArgAmount = FString::FromInt(Arguments.Num());
	Arch << strArgAmount;

	// Arguments
	for (int i = 0; i < keys.Num(); ++i)
	{
		Arch << keys[i];
		Arch << Arguments[keys[i]];
	}

	return true;
}

bool FDisplayClusterMessage::Deserialize(FMemoryReader& Arch)
{
	// Header
	Arch << Name;
	Arch << Type;
	Arch << Protocol;

	// Arguments amount
	FString StrArgsAmount;
	Arch << StrArgsAmount;
	const int32 Amount = FCString::Atoi(*StrArgsAmount);
	check(Amount >= 0);
	
	// Arguments
	for (int32 i = 0; i < Amount; ++i)
	{
		FString Key;
		FString Val;

		Arch << Key;
		Arch << Val;

		Arguments.Add(Key, Val);
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
	FString TmpStr;
	TmpStr.Reserve(512);
	
	for (auto it = Arguments.CreateConstIterator(); it; ++it)
	{
		TmpStr += FString::Printf(TEXT("%s=%s "), *it->Key, *it->Value);
	}

	return TmpStr;
}
