// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Config/Parser/DisplayClusterConfigParser.h"


/**
 * Auxiliary config parser. It generates hard-coded config.
 */
class FDisplayClusterConfigParserDebugAuto
	: public FDisplayClusterConfigParser
{
public:
	FDisplayClusterConfigParserDebugAuto(IDisplayClusterConfigParserListener* pListener);

protected:
	// Entry point for file parsing
	virtual bool ParseFile(const FString& path) override;
};

