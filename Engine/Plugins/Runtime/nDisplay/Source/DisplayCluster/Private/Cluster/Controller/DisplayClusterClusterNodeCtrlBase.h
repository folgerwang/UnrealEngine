// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Cluster/Controller/DisplayClusterNodeCtrlBase.h"


/**
 * Abstract cluster node controller (cluster mode).
 */
class FDisplayClusterClusterNodeCtrlBase
	: public FDisplayClusterNodeCtrlBase
{
public:
	FDisplayClusterClusterNodeCtrlBase(const FString& ctrlName, const FString& nodeName);
	virtual ~FDisplayClusterClusterNodeCtrlBase() = 0;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterNodeController
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool IsStandalone() const override final
	{ return false; }

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FDisplayClusterNodeCtrlBase
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool InitializeStereo() override;
};

