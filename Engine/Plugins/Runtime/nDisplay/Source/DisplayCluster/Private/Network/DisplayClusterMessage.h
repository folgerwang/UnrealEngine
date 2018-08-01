// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
	typedef TSharedPtr<FDisplayClusterMessage> Ptr;
	typedef TMap<FString, FString> DataType;

public:
	FDisplayClusterMessage();
	FDisplayClusterMessage(const FString& name, const FString& type, const FString& protocol);

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
	bool GetArg(const FString& argName, ValType& argVal) const
	{
		if (Arguments.Contains(argName))
		{
			FString strVal = Arguments[argName];
			argVal = FDisplayClusterTypesConverter::FromString<ValType>(strVal);
			return true;
		}
		return false;
	}

	// Get arguments from a message
	template <typename ValType>
	void SetArg(const FString& argName, const ValType& argVal)
	{
		Arguments.Add(argName, FDisplayClusterTypesConverter::ToString<ValType>(argVal));
	}

	// Get all arguments (be careful with the reference)
	const DataType& GetArgs() const
	{ return Arguments; }

	void SetArgs(const DataType& data)
	{ Arguments = data; }

	// Serialization
	virtual bool Serialize  (FMemoryWriter& ar) override;
	virtual bool Deserialize(FMemoryReader& ar) override;

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
