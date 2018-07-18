// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPDisplayClusterNodeController.h"

class FDisplayClusterClusterManager;


/**
 * Abstract node controller
 */
class FDisplayClusterNodeCtrlBase
	: public  IPDisplayClusterNodeController
{
	// This is needed to perform initialization from outside of constructor (polymorphic init)
	friend FDisplayClusterClusterManager;

public:
	FDisplayClusterNodeCtrlBase(const FString& ctrlName, const FString& nodeName);

	virtual ~FDisplayClusterNodeCtrlBase() = 0
	{ }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterNodeController
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Initialize() override final;
	virtual void Release() override final;

	virtual bool IsMaster() const override final
	{ return !IsSlave(); }
	
	virtual bool IsCluster() const override final
	{ return !IsStandalone(); }

	virtual FString GetNodeId() const override final
	{ return NodeName; }

	virtual FString GetControllerName() const override final
	{ return ControllerName; }

protected:
	virtual bool InitializeStereo()
	{ return true; }

	virtual bool InitializeServers()
	{ return true; }

	virtual bool StartServers()
	{ return true; }

	virtual void StopServers()
	{ return; }

	virtual bool InitializeClients()
	{ return true; }

	virtual bool StartClients()
	{ return true; }
	
	virtual void StopClients()
	{ return; }

private:
	const FString NodeName;
	const FString ControllerName;
};

