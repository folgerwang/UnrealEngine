// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Cluster/DisplayClusterClusterEvent.h"
#include "Misc/DisplayClusterLog.h"
#include "DisplayClusterStrings.h"


FString FDisplayClusterClusterEvent::SerializeToString() const
{
	return FString::Printf(TEXT("%s:%s:%s:%s"), *Name, *Type, *Category, *SerializeParametersToString());
}

bool FDisplayClusterClusterEvent::DeserializeFromString(const FString& Arch)
{
	const FString TokenSeparator(TEXT(":"));
	FString TempStr;

	if (Arch.Split(TokenSeparator, &Name, &TempStr, ESearchCase::IgnoreCase, ESearchDir::FromStart) == false ||
		TempStr.Split(TokenSeparator, &Type, &TempStr, ESearchCase::IgnoreCase, ESearchDir::FromStart) == false ||
		TempStr.Split(TokenSeparator, &Category, &TempStr, ESearchCase::IgnoreCase, ESearchDir::FromStart) == false)
	{
		return false;
	}

	if (DeserializeParametersFromString(TempStr) == false)
	{
		return false;
	}

	return true;
}

FString FDisplayClusterClusterEvent::SerializeParametersToString() const
{
	const FString ToketSeparator(TEXT(";"));
	FString Result;

	for (const auto& obj : Parameters)
	{
		Result += (obj.Key + FString(DisplayClusterStrings::strArrayValSeparator) + obj.Value + ToketSeparator);
	}

	return Result;
}

bool FDisplayClusterClusterEvent::DeserializeParametersFromString(const FString& Arch)
{
	Parameters.Empty(Parameters.Num());

	const FString ToketSeparator(TEXT(";"));

	FString TempStr = Arch;
	FString TempKeyValPair;

	while (TempStr.Split(ToketSeparator, &TempKeyValPair, &TempStr, ESearchCase::IgnoreCase, ESearchDir::FromStart))
	{
		FString l;
		FString r;

		if (TempKeyValPair.Split(FString(DisplayClusterStrings::strArrayValSeparator), &l, &r, ESearchCase::IgnoreCase, ESearchDir::FromStart) == false)
		{
			return false;
		}

		Parameters.Add(l, r);
	}

	return true;
}
