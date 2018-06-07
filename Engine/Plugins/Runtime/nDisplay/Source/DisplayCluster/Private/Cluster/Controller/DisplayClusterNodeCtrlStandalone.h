// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterNodeCtrlBase.h"

class FDisplayClusterMessage;


/**
 * Standalone node controller (no cluster)
 */
class FDisplayClusterNodeCtrlStandalone
	: public FDisplayClusterNodeCtrlBase
{
public:
	FDisplayClusterNodeCtrlStandalone(const FString& ctrlName, const FString& nodeName);
	virtual ~FDisplayClusterNodeCtrlStandalone();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterNodeController
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool IsSlave() const override final
	{ return false; }

	virtual bool IsStandalone() const override final
	{ return true; }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterClusterSyncProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void WaitForGameStart() override;
	virtual void WaitForFrameStart() override;
	virtual void WaitForFrameEnd() override;
	virtual void WaitForTickEnd() override;
	virtual void GetDeltaTime(float& deltaTime) override;
	virtual void GetSyncData(FDisplayClusterMessage::DataType& data)  override;
	virtual void GetInputData(FDisplayClusterMessage::DataType& data) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterSwapSyncProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void WaitForSwapSync(double* pThreadWaitTime, double* pBarrierWaitTime) override;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FDisplayClusterNodeCtrlBase
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool InitializeStereo() override;
};
