// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterConfigParser.h"
#include "Misc/DisplayClusterLog.h"


/**
 * Config parser for text based config files
 */
class FDisplayClusterConfigParserText
	: public FDisplayClusterConfigParser
{
public:
	FDisplayClusterConfigParserText(IDisplayClusterConfigParserListener* pListener);

protected:
	// Entry point for file parsing
	virtual bool ParseFile(const FString& path) override;

	// Entry point for line parsing
	void ParseLine(const FString& line);

protected:
	// Data type parsing
	template <typename T>
	inline T impl_parse(const FString& line)
	{
		static_assert(std::is_base_of<FDisplayClusterConfigBase, T>::value, "Only Display Cluster config types allowed");
		T tmp; bool result = static_cast<FDisplayClusterConfigBase&>(tmp).DeserializeFromString(line);
		UE_LOG(LogDisplayClusterConfig, Log, TEXT("Deserialization: %s"), result ? TEXT("ok") : TEXT("failed"));
		return tmp;
	}
};

