// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterConfigParser.h"


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

