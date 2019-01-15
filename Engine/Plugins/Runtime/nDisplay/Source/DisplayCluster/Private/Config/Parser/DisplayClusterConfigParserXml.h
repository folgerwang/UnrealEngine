// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Config/Parser/DisplayClusterConfigParser.h"


/**
 * Config parser for XML based config files
 */
class FDisplayClusterConfigParserXml
	: public FDisplayClusterConfigParser
{
public:
	FDisplayClusterConfigParserXml(IDisplayClusterConfigParserListener* pListener);

public:
	// Entry point for file parsing
	virtual bool ParseFile(const FString& path) override
	{
		// Not implemented yet
		return false;
	}

protected:
	//virtual bool ReadConfigFile(const FString& path);
};

