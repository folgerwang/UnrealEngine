// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FDisplayClusterClusterEvent;


/**
 * Cluster events protocol
 */
class IPDisplayClusterClusterEventsProtocol
{
public:
	virtual void EmitClusterEvent(const FDisplayClusterClusterEvent& Event) = 0;
};
