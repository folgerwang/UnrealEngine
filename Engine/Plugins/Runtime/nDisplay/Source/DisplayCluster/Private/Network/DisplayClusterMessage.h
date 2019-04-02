// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterSerializable.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#include "Misc/DisplayClusterTypesConverter.h"


/**
 * Abstract network message
 */
class FDisplayClusterMessage
	: IDisplayClusterSerializable
{
public:
	typedef TMap<FString, FString> DataType;

public:
	FDisplayClusterMessage();
	FDisplayClusterMessage(const FString& InName, const FString& InType, const FString& InProtocol);

	FDisplayClusterMessage(const FDisplayClusterMessage&) = default;
	FDisplayClusterMessage(FDisplayClusterMessage&&)      = default;
	
	FDisplayClusterMessage& operator= (const FDisplayClusterMessage&) = default;
	FDisplayClusterMessage& operator= (FDisplayClusterMessage&&)      = default;
	
	virtual ~FDisplayClusterMessage();

public:
	// Message head
	inline FString GetName()     const { return Name; }
	inline FString GetType()     const { return Type; }
	inline FString GetProtocol() const { return Protocol; }

	// Sets arguments to a message
	template <typename ValType>
	bool GetArg(const FString& ArgName, ValType& ArgVal) const
	{
		if (Arguments.Contains(ArgName))
		{
			FString StrVal = Arguments[ArgName];
			ArgVal = FDisplayClusterTypesConverter::FromString<ValType>(StrVal);
			return true;
		}
		return false;
	}

	// Get arguments from a message
	template <typename ValType>
	void SetArg(const FString& ArgName, const ValType& ArgVal)
	{
		Arguments.Add(ArgName, FDisplayClusterTypesConverter::ToString<ValType>(ArgVal));
	}

	// Get all arguments (be careful with the reference)
	const DataType& GetArgs() const
	{ return Arguments; }

	void SetArgs(const DataType& Data)
	{ Arguments = Data; }

	// Serialization
	virtual bool Serialize  (FMemoryWriter& Arch) override;
	virtual bool Deserialize(FMemoryReader& Arch) override;

	FString ToString() const;

private:
	//inline bool ExtractKeyVal(const FString& pair, FString& key, FString& val);
	FString ArgsToString() const;

private:
	FString Name;
	FString Type;
	FString Protocol;

	DataType Arguments;
};
