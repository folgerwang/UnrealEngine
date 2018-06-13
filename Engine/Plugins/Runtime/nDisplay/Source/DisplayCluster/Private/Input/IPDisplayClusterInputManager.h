// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/IDisplayClusterInputManager.h"
#include "IPDisplayClusterManager.h"

#include "Network/DisplayClusterMessage.h"


/**
 * Input manager private interface
 */
struct IPDisplayClusterInputManager
	: public IDisplayClusterInputManager
	, public IPDisplayClusterManager
{
	virtual ~IPDisplayClusterInputManager()
	{ }

	virtual void Update() = 0;

	virtual void ExportInputData(FDisplayClusterMessage::DataType& data) const = 0;
	virtual void ImportInputData(const FDisplayClusterMessage::DataType& data) = 0;
};
